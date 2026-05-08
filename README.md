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

### get_answer.py

Query answers from the Supabase cloud QA bank.

```bash
python scripts/get_answer.py --stats --json
python scripts/get_answer.py --list --limit 10 --json
python scripts/get_answer.py --title "Question title" --type single --options 'A:Option 1,B:Option 2'
python scripts/get_answer.py --title "Question title" --type single --options 'A:Option 1,B:Option 2' --multi-fp --json
```

### set_answer.py

Upload verified answers to the Supabase cloud QA bank.

```bash
python scripts/set_answer.py --title "Question title" --type single --options 'A:Option 1,B:Option 2' --answer A --confidence verified --course "Course name"
python scripts/set_answer.py --batch questions.json --confidence verified --course "Course name"
```

### search_answer.py

Search candidate answers using Bing/Baidu web crawling. Default engine is `auto`, which tries Bing first and then Baidu.

```bash
python scripts/search_answer.py --question "Which layer does TCP belong to?" --options 'A:Network layer,B:Transport layer' --json
python scripts/search_answer.py --engine baidu --question "Which layer does TCP belong to?" --options 'A:Network layer,B:Transport layer' --json
```

## Install dependencies

```bash
pip install -r requirements.txt
```

## License

MIT
