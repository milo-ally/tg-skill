"""get-answer — Query answers from the cloud QA bank."""

from __future__ import annotations

import argparse
import difflib
import json
import re
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from typing import Optional

CLOUD_URL = "https://qlfinntromtdvjxyvbyn.supabase.co"
CLOUD_KEY = "sb_publishable_jEf_67lB3bvHpFVGL8ov_Q_Xi3iV1il"
TYPE_ALIASES = {"single": "1", "multi": "2", "judge": "3", "fill": "4"}


@dataclass
class Question:
    title: str = ""
    type: str = ""
    options: list[dict] = field(default_factory=list)


@dataclass
class AnswerResult:
    answer: str
    confidence: str = "high"
    confirm_count: int = 1
    fingerprint: str = ""


def simple_hash(s: str) -> str:
    h = 0x811C9DC5
    for ch in s:
        h ^= ord(ch)
        h = (h * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def question_fingerprint(q: Question) -> str:
    title_key = re.sub(r"\s+", "", q.title or "")[:80]
    title_key = re.sub(r"[【\[]答案[】\]][\s\S]*$", "", title_key)
    title_key = re.sub(r"^\s*\d+\s*[、.．)）\s]+", "", title_key)
    option_key = "|".join(re.sub(r"\s+", "", o.get("text", ""))[:18] for o in q.options)
    q_type = normalize_type(q.type)
    raw = f"{q_type}_{title_key}_{option_key}"
    return f"{q_type}_{simple_hash(raw)}{simple_hash(raw + '_salt')}"


def legacy_question_fingerprint(q: Question) -> str:
    return f"{normalize_type(q.type)}_{re.sub(r'\s+', '', q.title or '')[:30]}"


def sorted_question_fingerprint(q: Question) -> str:
    title_key = re.sub(r"[【\[]答案[】\]][\s\S]*$", "", q.title or "")
    title_key = re.sub(r"\s+", "", title_key)[:80]
    option_key = "|".join(sorted(re.sub(r"\s+", "", o.get("text", ""))[:18] for o in q.options))
    return f"sorted_{normalize_type(q.type)}_{title_key}_{option_key}"[:220]


def loose_question_fingerprint(q: Question) -> str:
    title_key = re.sub(r"[【\[]答案[】\]][\s\S]*$", "", q.title or "")
    title_key = re.sub(r"^\s*\d+\s*[、.．]\s*", "", title_key)
    title_key = re.sub(r"[\s!\"#$%&'()*+,\-./:;<=>?@\[\\\]^_`{|}~。，、；：？！【】《》（）…—·]", "", title_key)[:24]
    return f"loose_{normalize_type(q.type)}_{title_key}"


def all_fingerprints(q: Question) -> list[str]:
    return [question_fingerprint(q), legacy_question_fingerprint(q), sorted_question_fingerprint(q), loose_question_fingerprint(q)]


def normalize_type(type_name: str) -> str:
    return TYPE_ALIASES.get(type_name, type_name)


def normalize_text(s: str) -> str:
    s = re.sub(r"[【\[]答案[】\]][\s\S]*$", "", s or "")
    s = re.sub(r"^\s*\d+\s*[、.．)）\s]+", "", s)
    return re.sub(r"[\s!\"#$%&'()*+,\-./:;<=>?@\[\\\]^_`{|}~。，、；：？！【】《》（）…—·]", "", s)


def make_request(path: str, timeout: int = 10, prefer_count: bool = False) -> tuple[list | None, dict]:
    headers = {"apikey": CLOUD_KEY, "Authorization": f"Bearer {CLOUD_KEY}", "Content-Type": "application/json"}
    if prefer_count:
        headers["Prefer"] = "count=exact"
    req = urllib.request.Request(
        f"{CLOUD_URL}/rest/v1/{path}",
        headers=headers,
        method="GET",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read().decode()), dict(resp.headers)
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code}: {e.read().decode()[:200]}")
    except Exception as e:
        print(f"network error: {e}")
    return None, {}


def query_answer(q: Question, multi_fp: bool = False) -> Optional[AnswerResult]:
    for fp in all_fingerprints(q):
        path = f"questions?fingerprint=eq.{urllib.parse.quote(fp)}&select=answer,confidence,confirm_count&limit=1"
        rows, _ = make_request(path)
        if isinstance(rows, list) and rows and rows[0].get("answer"):
            return AnswerResult(rows[0]["answer"], rows[0].get("confidence", "high"), rows[0].get("confirm_count", 1), fp)
    return fuzzy_query_answer(q)


def fuzzy_query_answer(q: Question) -> Optional[AnswerResult]:
    title = normalize_text(q.title)
    if not title:
        return None
    keywords = [title[:24], title[:16], title[:10]]
    rows = []
    for keyword in keywords:
        if not keyword:
            continue
        path = (
            "questions?"
            f"type=eq.{urllib.parse.quote(normalize_type(q.type))}&"
            f"title=ilike.*{urllib.parse.quote(keyword)}*&"
            "select=fingerprint,title,answer,confidence,confirm_count&"
            "limit=20"
        )
        result, _ = make_request(path)
        if isinstance(result, list) and result:
            rows = result
            break
    if not rows:
        path = (
            "questions?"
            f"type=eq.{urllib.parse.quote(normalize_type(q.type))}&"
            "select=fingerprint,title,answer,confidence,confirm_count&"
            "limit=100"
        )
        result, _ = make_request(path)
        rows = result if isinstance(result, list) else []
    candidates = [row for row in rows if row.get("answer")]
    if not candidates:
        return None
    best = max(candidates, key=lambda row: difflib.SequenceMatcher(None, title, normalize_text(row.get("title", ""))).ratio())
    score = difflib.SequenceMatcher(None, title, normalize_text(best.get("title", ""))).ratio()
    if score < 0.18:
        return None
    confidence = best.get("confidence", "medium")
    if score < 0.3:
        confidence = "low"
    elif confidence == "high":
        confidence = "medium"
    return AnswerResult(best["answer"], confidence, best.get("confirm_count", 1), best.get("fingerprint", "fuzzy"))


def list_questions(limit: int = 10) -> list[dict]:
    limit = max(1, min(limit, 100))
    path = f"questions?select=fingerprint,title,answer,type,confidence,confirm_count,course&limit={limit}"
    rows, _ = make_request(path)
    return rows if isinstance(rows, list) else []


def question_stats() -> dict:
    rows, headers = make_request("questions?select=fingerprint&limit=1", prefer_count=True)
    content_range = headers.get("Content-Range") or headers.get("content-range") or ""
    total = None
    if "/" in content_range:
        total_text = content_range.rsplit("/", 1)[-1]
        total = int(total_text) if total_text.isdigit() else None
    sample_count = len(rows) if isinstance(rows, list) else 0
    return {"available": rows is not None, "total": total, "sample_count": sample_count}


def parse_options(raw: str) -> list[dict]:
    options = []
    for item in raw.split(",") if raw else []:
        if ":" in item:
            letter, text = item.split(":", 1)
            options.append({"letter": letter.strip(), "text": text.strip()})
        else:
            options.append({"letter": "", "text": item.strip()})
    return options


def main() -> None:
    parser = argparse.ArgumentParser(description="Query answer from tg cloud QA bank")
    parser.add_argument("--title", default="")
    parser.add_argument("--type", choices=["single", "multi", "judge", "fill"], default="single")
    parser.add_argument("--options", default="")
    parser.add_argument("--multi-fp", action="store_true")
    parser.add_argument("--list", action="store_true", help="list sample questions from cloud QA bank")
    parser.add_argument("--stats", action="store_true", help="show cloud QA bank availability and count")
    parser.add_argument("--limit", type=int, default=10)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if args.stats:
        data = question_stats()
        print(json.dumps(data, ensure_ascii=False) if args.json else f"available: {data['available']}\ntotal: {data['total']}")
        return
    if args.list:
        rows = list_questions(args.limit)
        print(json.dumps(rows, ensure_ascii=False, indent=2) if args.json else "\n".join(f"- {r}" for r in rows))
        return
    if not args.title:
        parser.error("query requires --title, or use --list/--stats")
    result = query_answer(Question(args.title, args.type, parse_options(args.options)), args.multi_fp)
    if args.json:
        print(json.dumps(result.__dict__ if result else None, ensure_ascii=False))
    elif result:
        print(f"answer: {result.answer}\nconfidence: {result.confidence}\nconfirm_count: {result.confirm_count}\nfingerprint: {result.fingerprint}")
    else:
        print("answer: NOT_FOUND")


if __name__ == "__main__":
    main()
