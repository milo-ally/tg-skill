# tg-skills

`tg-skills` is an agent skill for the Touge/头歌 platform. It uses `playwright-cli` for browser automation and provides atomic helper scripts for QA-bank operations.

## Layout

```text
tg-skills/
├── scripts/
│   ├── get_answer.py
│   ├── set_answer.py
│   └── search_answer.py
├── SKILL.md
├── requirements.txt
├── LICENCE
├── README.md
└── .gitignore
```

## Atomic scripts

Agent rules:

- Prefer scripts under `scripts/` before custom shell commands.
- Do not hand-write Supabase `curl` commands when an existing script supports the task.
- Use `get_answer.py --stats` or `get_answer.py --list` for read-only QA-bank inspection.
- Use `set_answer.py` only when the user explicitly asks to upload or update answers.