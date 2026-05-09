"""search-answer — Search candidate answers using microcode-style web search/fetch logic."""

from __future__ import annotations

import argparse
import json
import os
from typing import Any

import httpx

USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/120 Safari/537.36"


def web_fetch(url: str) -> dict[str, Any]:
    if not url:
        return {"error": "missing required field 'url'"}

    try:
        with httpx.Client(follow_redirects=True, timeout=30.0, verify=False) as client:
            response = client.get(url, headers={"User-Agent": USER_AGENT})
            response.raise_for_status()

        body = response.text
        content_type = response.headers.get("content-type", "")

        try:
            from readability import Document
            from bs4 import BeautifulSoup

            doc = Document(body)
            soup = BeautifulSoup(doc.summary(), "lxml")
            text = soup.get_text(separator="\n", strip=True)
        except ImportError:
            try:
                from bs4 import BeautifulSoup

                soup = BeautifulSoup(body, "lxml")
                text = soup.get_text(separator="\n", strip=True)
            except ImportError:
                text = body[:5000]

        if len(text) > 50_000:
            text = text[:50_000] + "\n... [content truncated]"

        return {"url": url, "content_type": content_type, "text": text, "status": "ok"}
    except httpx.HTTPError as exc:
        return {"error": f"HTTP error fetching {url}: {exc}"}
    except Exception as exc:
        return {"error": str(exc)}


def web_search(query: str, limit: int = 10) -> dict[str, Any]:
    if not query:
        return {"error": "missing required field 'query'"}

    searx_url = os.environ.get("SEARXNG_URL") or os.environ.get("SEARX_URL")
    errors = []
    if searx_url:
        result = search_searxng(searx_url, query, limit)
        if "error" not in result:
            return result
        errors.append(result["error"])

    for searcher in (search_duckduckgo, search_bing, search_baidu):
        result = searcher(query, limit)
        if "error" not in result and result.get("results"):
            result["errors"] = errors
            return result
        errors.append(result.get("error") or f"{result.get('engine', 'search')} returned no results")
    return {"error": "; ".join(errors)}


def search_searxng(base_url: str, query: str, limit: int) -> dict[str, Any]:
    try:
        with httpx.Client(follow_redirects=True, timeout=15.0, verify=False) as client:
            response = client.get(
                f"{base_url.rstrip('/')}/search",
                params={"q": query, "format": "json"},
                headers={"User-Agent": USER_AGENT},
            )
            response.raise_for_status()
            data = response.json()

        results = []
        for item in data.get("results", [])[:limit]:
            results.append({"title": item.get("title", ""), "url": item.get("url", ""), "snippet": item.get("content", "")})
        return {"query": query, "results": results, "total": len(results), "engine": "searxng"}
    except Exception as exc:
        return {"error": f"SearXNG search failed: {exc}"}


def search_duckduckgo(query: str, limit: int) -> dict[str, Any]:
    try:
        with httpx.Client(follow_redirects=True, timeout=15.0, verify=False) as client:
            response = client.get(
                "https://html.duckduckgo.com/html/",
                params={"q": query},
                headers={"User-Agent": USER_AGENT},
            )
            response.raise_for_status()

        from bs4 import BeautifulSoup

        soup = BeautifulSoup(response.text, "lxml")
        results = []
        for result_div in soup.select(".result"):
            title_el = result_div.select_one(".result__title a")
            snippet_el = result_div.select_one(".result__snippet")
            if title_el:
                results.append({
                    "title": title_el.get_text(strip=True),
                    "url": title_el.get("href", ""),
                    "snippet": snippet_el.get_text(strip=True) if snippet_el else "",
                })
            if len(results) >= limit:
                break
        return {"query": query, "results": results, "total": len(results), "engine": "duckduckgo"}
    except Exception as exc:
        return {"error": f"DuckDuckGo search failed: {exc}"}


def search_bing(query: str, limit: int) -> dict[str, Any]:
    try:
        with httpx.Client(follow_redirects=True, timeout=15.0, verify=False) as client:
            response = client.get(
                "https://www.bing.com/search",
                params={"q": query},
                headers={"User-Agent": USER_AGENT},
            )
            response.raise_for_status()

        from bs4 import BeautifulSoup

        soup = BeautifulSoup(response.text, "lxml")
        results = []
        for item in soup.select("li.b_algo"):
            title_el = item.select_one("h2 a") or item.select_one("a")
            snippet_el = item.select_one("p")
            if title_el:
                results.append({
                    "title": title_el.get_text(" ", strip=True),
                    "url": title_el.get("href", ""),
                    "snippet": snippet_el.get_text(" ", strip=True) if snippet_el else "",
                })
            if len(results) >= limit:
                break
        return {"query": query, "results": results, "total": len(results), "engine": "bing"}
    except Exception as exc:
        return {"error": f"Bing search failed: {exc}"}


def search_baidu(query: str, limit: int) -> dict[str, Any]:
    try:
        with httpx.Client(follow_redirects=True, timeout=15.0, verify=False) as client:
            response = client.get(
                "https://www.baidu.com/s",
                params={"wd": query},
                headers={"User-Agent": USER_AGENT},
            )
            response.raise_for_status()

        from bs4 import BeautifulSoup

        soup = BeautifulSoup(response.text, "lxml")
        results = []
        for item in soup.select(".result, .c-container"):
            title_el = item.select_one("h3 a") or item.select_one("a")
            if not title_el:
                continue
            snippet = item.get_text(" ", strip=True)
            url = title_el.get("href", "")
            if "top.baidu.com" in url or "chat.baidu.com/search" in url:
                continue
            results.append({"title": title_el.get_text(" ", strip=True), "url": url, "snippet": snippet[:300]})
            if len(results) >= limit:
                break
        return {"query": query, "results": results, "total": len(results), "engine": "baidu"}
    except Exception as exc:
        return {"error": f"Baidu search failed: {exc}"}


def parse_options(raw: str) -> list[dict[str, str]]:
    options = []
    for item in raw.split(",") if raw else []:
        if ":" in item:
            letter, text = item.split(":", 1)
            options.append({"letter": letter.strip(), "text": text.strip()})
        else:
            options.append({"letter": "", "text": item.strip()})
    return options


def score_options(options: list[dict[str, str]], documents: list[str]) -> list[dict[str, Any]]:
    corpus = " ".join(documents)
    compact_corpus = corpus.replace(" ", "")
    scored = []
    for opt in options:
        text = opt.get("text", "")
        compact = text.replace(" ", "")
        score = corpus.count(text) + compact_corpus.count(compact) if text else 0
        scored.append({"letter": opt.get("letter", ""), "text": text, "score": score})
    return sorted(scored, key=lambda item: item["score"], reverse=True)


def search_answer(question: str, options: list[dict[str, str]], limit: int = 5, fetch_pages: int = 0) -> dict[str, Any]:
    query = question + (" " + " ".join(option.get("text", "") for option in options) if options else "")
    search = web_search(query, limit)
    if "error" in search:
        return {"query": query, "best_guess": None, "candidates": score_options(options, []), "sources": [], "error": search["error"]}

    sources = search.get("results", [])
    fetched = []
    for source in sources[:max(0, fetch_pages)]:
        content = web_fetch(source.get("url", ""))
        if content.get("status") == "ok":
            fetched.append({"url": content.get("url", ""), "text": content.get("text", "")[:3000]})

    documents = [f"{item.get('title', '')} {item.get('snippet', '')}" for item in sources]
    documents.extend(item.get("text", "") for item in fetched)
    candidates = score_options(options, documents) if options else []
    return {
        "engine": search.get("engine", "unknown"),
        "query": query,
        "best_guess": candidates[0] if candidates and candidates[0]["score"] > 0 else None,
        "candidates": candidates,
        "sources": sources,
        "fetched": fetched,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Search candidate answers using microcode-style web_search/web_fetch logic")
    parser.add_argument("--question", required=True)
    parser.add_argument("--options", default="")
    parser.add_argument("--limit", type=int, default=5)
    parser.add_argument("--fetch-pages", type=int, default=0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    data = search_answer(args.question, parse_options(args.options), args.limit, args.fetch_pages)
    if args.json:
        print(json.dumps(data, ensure_ascii=False, indent=2))
        return

    print(f"engine: {data.get('engine')}")
    print(f"query: {data['query']}")
    print(f"best_guess: {data.get('best_guess')}")
    if data.get("error"):
        print(f"error: {data['error']}")
    for source in data.get("sources", []):
        print(f"- {source.get('title', '')}: {source.get('snippet', '')} ({source.get('url', '')})")


if __name__ == "__main__":
    main()
