"""search-answer — Search candidate answers with Bing/Baidu web crawling."""

from __future__ import annotations

import argparse
import html
import json
import re
import ssl
import urllib.error
import urllib.parse
import urllib.request

USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/120 Safari/537.36"


def fetch(url: str, timeout: int = 10) -> str:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
        },
    )
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
        return resp.read().decode("utf-8", errors="ignore")


def strip_tags(text: str) -> str:
    text = re.sub(r"<script[\s\S]*?</script>", " ", text, flags=re.I)
    text = re.sub(r"<style[\s\S]*?</style>", " ", text, flags=re.I)
    text = re.sub(r"<[^>]+>", " ", text)
    return re.sub(r"\s+", " ", html.unescape(text)).strip()


def clean_url(url: str) -> str:
    url = html.unescape(url)
    parsed = urllib.parse.urlparse(url)
    query = urllib.parse.parse_qs(parsed.query)
    for key in ("url", "u", "target", "wd"):
        if key in query and query[key]:
            return query[key][0]
    return url


def compact_text(text: str, limit: int = 300) -> str:
    text = strip_tags(text)
    return text[:limit] + ("..." if len(text) > limit else "")


def bing_search(query: str, limit: int = 5) -> tuple[list[dict], str | None]:
    url = "https://www.bing.com/search?" + urllib.parse.urlencode({"q": query})
    try:
        page = fetch(url)
    except Exception as exc:
        return [], f"bing fetch failed: {type(exc).__name__}: {exc}"

    results: list[dict] = []
    blocks = re.findall(r'<li class="b_algo"[\s\S]*?</li>', page, flags=re.I)
    for block in blocks:
        link = re.search(r'<a[^>]+href="(.*?)"[^>]*>([\s\S]*?)</a>', block, flags=re.I)
        if not link:
            continue
        snippet_match = re.search(r'<p[^>]*>([\s\S]*?)</p>', block, flags=re.I)
        title = strip_tags(link.group(2))
        snippet = strip_tags(snippet_match.group(1)) if snippet_match else compact_text(block)
        results.append({"engine": "bing", "url": clean_url(link.group(1)), "title": title, "snippet": snippet})
        if len(results) >= limit:
            break

    if not results:
        anchors = re.findall(r'<a[^>]+href="(https?://[^"]+)"[^>]*>([\s\S]*?)</a>', page, flags=re.I)
        for raw_url, title_html in anchors:
            title = strip_tags(title_html)
            if not title or "bing" in raw_url:
                continue
            results.append({"engine": "bing", "url": clean_url(raw_url), "title": title, "snippet": title})
            if len(results) >= limit:
                break

    return results, None if results else "bing returned no parseable results"


def baidu_search(query: str, limit: int = 5) -> tuple[list[dict], str | None]:
    url = "https://www.baidu.com/s?" + urllib.parse.urlencode({"wd": query})
    try:
        page = fetch(url)
    except Exception as exc:
        return [], f"baidu fetch failed: {type(exc).__name__}: {exc}"

    results: list[dict] = []
    blocks = re.findall(r'<div[^>]+(?:class|tpl)="[^"]*(?:result|c-container)[^"]*"[\s\S]*?</div>\s*</div>', page, flags=re.I)
    if not blocks:
        blocks = re.findall(r'<div[^>]+class="[^"]*result[^"]*"[\s\S]*?</div>', page, flags=re.I)

    for block in blocks:
        link = re.search(r'<a[^>]+href="(.*?)"[^>]*>([\s\S]*?)</a>', block, flags=re.I)
        if not link:
            continue
        snippet = compact_text(block)
        title = strip_tags(link.group(2))
        raw_url = clean_url(link.group(1))
        if not title and len(snippet) < 30:
            continue
        if any(skip in raw_url for skip in ("top.baidu.com", "chat.baidu.com/search")):
            continue
        if snippet in {"网页 图片 资讯 视频 笔记 地图 贴吧 文库 更多 搜索工具", "换一换"}:
            continue
        results.append({"engine": "baidu", "url": raw_url, "title": title, "snippet": snippet})
        if len(results) >= limit:
            break

    return results, None if results else "baidu returned no parseable results"


def parse_options(raw: str) -> list[dict]:
    options = []
    for item in raw.split(",") if raw else []:
        if ":" in item:
            letter, text = item.split(":", 1)
            options.append({"letter": letter.strip(), "text": text.strip()})
        else:
            options.append({"letter": "", "text": item.strip()})
    return options


def score_options(options: list[dict], documents: list[str]) -> list[dict]:
    corpus = " ".join(documents)
    compact_corpus = corpus.replace(" ", "")
    scored = []
    for opt in options:
        text = opt.get("text", "")
        compact = text.replace(" ", "")
        score = corpus.count(text) + compact_corpus.count(compact) if text else 0
        scored.append({"letter": opt.get("letter", ""), "text": text, "score": score})
    return sorted(scored, key=lambda x: x["score"], reverse=True)


def run_engine(engine: str, query: str, limit: int) -> tuple[list[dict], str | None]:
    if engine == "baidu":
        return baidu_search(query, limit)
    return bing_search(query, limit)


def search_answer(question: str, options: list[dict], limit: int = 5, engine: str = "auto") -> dict:
    query = question + (" " + " ".join(o.get("text", "") for o in options) if options else "")
    engines = ["bing", "baidu"] if engine == "auto" else [engine]
    diagnostics = []
    sources: list[dict] = []
    used_engine = None

    for name in engines:
        sources, error = run_engine(name, query, limit)
        if error:
            diagnostics.append(error)
        if sources:
            used_engine = name
            break

    documents = [f"{s.get('title', '')} {s.get('snippet', '')}" for s in sources]
    candidates = score_options(options, documents) if options else []
    return {
        "engine": used_engine or engine,
        "query": query,
        "best_guess": candidates[0] if candidates and candidates[0]["score"] > 0 else None,
        "candidates": candidates,
        "sources": sources,
        "diagnostics": diagnostics,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Search candidate answer from Bing/Baidu web pages")
    parser.add_argument("--question", required=True)
    parser.add_argument("--options", default="")
    parser.add_argument("--limit", type=int, default=5)
    parser.add_argument("--engine", default="auto", choices=["auto", "bing", "baidu"])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    data = search_answer(args.question, parse_options(args.options), args.limit, args.engine)
    if args.json:
        print(json.dumps(data, ensure_ascii=False, indent=2))
    else:
        print(f"engine: {data['engine']}")
        print(f"query: {data['query']}")
        print(f"best_guess: {data['best_guess']}")
        if data["diagnostics"]:
            print("diagnostics:")
            for item in data["diagnostics"]:
                print(f"- {item}")
        for src in data["sources"]:
            print(f"- {src.get('title', '')}: {src.get('snippet', '')} ({src.get('url', '')})")


if __name__ == "__main__":
    main()
