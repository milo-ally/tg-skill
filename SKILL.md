---
name: tg-skills
description: Automate browser interactions for zcst students on touge/头歌 platform and use atomic answer helper scripts.
tags: [touge, playwright, qa-bank, education]
---

# tg-skills

This skill is part of `agent-skills` development. Install `playwright-cli`, move its generated skill into `./skills/playwright-cli`, then remove the temporary `.claude` directory (if you are claude ore codex, rename it or ignore it):

```bash
npm install -g @playwright/cli@latest
playwright-cli install --skills
mkdir -p ./skills
mv ./.claude/skills/playwright-cli ./skills/playwright-cli
rm -rf ./.claude
```
Use `playwright-cli` for Touge/头歌 browser operations. Use scripts in `scripts/` as atomic helpers for answer lookup, answer upload, and web search.


## Login

### Internal network

```bash
playwright-cli open http://172.16.36.150 --headed
playwright-cli click 'text="登录"' && \
playwright-cli fill 'getByPlaceholder("请输入有效的手机号/邮箱号/账号")' '<your-username>' && \
playwright-cli fill 'getByPlaceholder("密码")' '<your-password>' && \
playwright-cli click 'button:has-text("登录")'
```

### External network via VPN

The platform is accessible via `https://vpn.zcst.edu.cn`. The real resource domain is `tg.zcst.edu.cn`.

```bash
playwright-cli open https://vpn.zcst.edu.cn --headed
playwright-cli fill '请输入学工号' '<username>' && playwright-cli fill '请输入密码' '<password>'
playwright-cli click '登 录'
playwright-cli click '头歌教学平台'
playwright-cli tab-select 1
playwright-cli click '统一身份认证登录'
playwright-cli tab-select 2
playwright-cli fill '请输入学工号' '<username>' && playwright-cli fill '请输入密码' '<password>'
playwright-cli click '登 录'
```

## Atomic scripts

Run from this skill directory: `skills/tg-skills`.

Important agent behavior:

- Always trust and use the atomic scripts first.
- Do not hand-write `curl` Supabase REST commands when a script supports the task.
- For read-only QA bank inspection, use `get_answer.py --stats` or `get_answer.py --list`.
- Use `set_answer.py` only when the user explicitly asks to upload or update answers.

### get_answer.py

Query Supabase cloud QA bank by question fingerprint.

```bash
python scripts/get_answer.py --stats --json
python scripts/get_answer.py --list --limit 10 --json
python scripts/get_answer.py --title "Question title" --type single --options 'A:Option 1,B:Option 2'
python scripts/get_answer.py --title "Question title" --type single --options 'A:Option 1,B:Option 2' --multi-fp --json
```

### set_answer.py

Upload verified answers to Supabase cloud QA bank.

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

Question types: `single`, `multi`, `judge`, `fill`. Confidence: `verified`, `high`, `medium`, `low`.
