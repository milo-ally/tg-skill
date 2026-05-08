"""set-answer — Write answers to the cloud QA bank."""

from __future__ import annotations

import argparse
import json
import re
import urllib.error
import urllib.request
from dataclasses import dataclass, field

CLOUD_URL = "https://qlfinntromtdvjxyvbyn.supabase.co"
CLOUD_KEY = "sb_publishable_jEf_67lB3bvHpFVGL8ov_Q_Xi3iV1il"


@dataclass
class Question:
    title: str = ""
    type: str = ""
    options: list[dict] = field(default_factory=list)


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
    raw = f"{q.type}_{title_key}_{option_key}"
    return f"{q.type}_{simple_hash(raw)}{simple_hash(raw + '_salt')}"


def request_upsert(body: dict, timeout: int = 10) -> bool:
    req = urllib.request.Request(
        f"{CLOUD_URL}/rest/v1/rpc/upsert_question",
        data=json.dumps(body).encode(),
        headers={"apikey": CLOUD_KEY, "Authorization": f"Bearer {CLOUD_KEY}", "Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return 200 <= resp.status < 300
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code}: {e.read().decode()[:200]}")
    except Exception as e:
        print(f"network error: {e}")
    return False


def set_answer(q: Question, answer: str, confidence: str = "verified", course: str = "") -> bool:
    fp = question_fingerprint(q)
    if not fp or not answer:
        return False
    title = re.sub(r"^\s*\d+\s*[、.．)）\s]+", "", q.title or "")[:500]
    return request_upsert({"p_fingerprint": fp, "p_title": title, "p_answer": answer, "p_type": q.type, "p_confidence": confidence, "p_course": course})


def parse_options(raw: str) -> list[dict]:
    options = []
    for item in raw.split(",") if raw else []:
        if ":" in item:
            letter, text = item.split(":", 1)
            options.append({"letter": letter.strip(), "text": text.strip()})
        else:
            options.append({"letter": "", "text": item.strip()})
    return options


def run_batch(path: str, confidence: str, course: str) -> tuple[int, int]:
    with open(path, encoding="utf-8") as f:
        items = json.load(f)
    uploaded = 0
    skipped = 0
    for item in items:
        q = Question(item.get("title", ""), item.get("type", "single"), item.get("options", []))
        if set_answer(q, item.get("answer", ""), confidence, item.get("course", course)):
            uploaded += 1
        else:
            skipped += 1
    return uploaded, skipped


def main() -> None:
    parser = argparse.ArgumentParser(description="Set answer into tg cloud QA bank")
    parser.add_argument("--title", default="")
    parser.add_argument("--type", choices=["single", "multi", "judge", "fill"], default="single")
    parser.add_argument("--options", default="")
    parser.add_argument("--answer", default="")
    parser.add_argument("--confidence", default="verified", choices=["verified", "high", "medium", "low"])
    parser.add_argument("--course", default="")
    parser.add_argument("--batch")
    args = parser.parse_args()

    if args.batch:
        uploaded, skipped = run_batch(args.batch, args.confidence, args.course)
        print(f"uploaded: {uploaded}\nskipped: {skipped}")
        return
    if not args.title or not args.answer:
        parser.error("single upload requires --title and --answer")
    q = Question(args.title, args.type, parse_options(args.options))
    print(f"uploaded: {set_answer(q, args.answer, args.confidence, args.course)}")
    print(f"fingerprint: {question_fingerprint(q)}")


if __name__ == "__main__":
    main()
