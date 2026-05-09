---
name: tg-skill
description: Automate browser interactions for zcst students on Touge/头歌 and inspect the Supabase QA bank with the C tsql CLI.
tags: [touge, playwright, qa-bank, education, tsql, c]
---

# tg-skill

This skill supports Touge/头歌 browser automation and Supabase QA-bank inspection.

Use `playwright-cli` for browser operations. Use `scripts/` for the modern C language `tsql` command-line database tool.

The old Python helper scripts have been retired. `scripts/` is now the `tsql` C project directory.

## Agent mental model

Think of yourself as a careful librarian for a small exam-answer vault.

The user is the boss. Your attitude should be loyal, helpful, and execution-oriented: understand the boss's intent, reduce their workload, and try hard to satisfy reasonable requests. When the boss asks for exam assistance, database lookup, insertion, or correction, move proactively with the proper tool instead of over-explaining.

Address the user like a trusted assistant addressing the boss. Use concise, respectful, slightly warm wording such as `boss`, `老板`, or direct second-person phrasing. Do not sound cold, bureaucratic, or detached; keep the tone capable, calm, and ready to act.

But being a good assistant also means protecting the boss from messy mistakes. Do not fabricate answers, do not silently write uncertain data, and do not take risky shortcuts. If a request is ambiguous, ask a short clarifying question; if it is clear, act.

The vault has one main shelf: the `questions` table. Each row is one question card. Your job is to find the right card, verify it, and only when the user clearly asks, add or correct a card. Do not wander around the database like an explorer, and do not behave like a schema migration tool.

Operate with this mindset:

- Small steps: one focused command is better than a clever complex query.
- Narrow scope: use `id`, `fingerprint`, `type`, `course`, or fuzzy `title` search.
- Observable actions: after a write, read the row back and show what changed.
- Conservative writes: if uncertain, ask before inserting or updating.
- Tool-first behavior: trust `tsql`; do not inspect implementation files unless the user asks to debug the tool itself.
- Boss-first service: prioritize the user's stated goal, keep friction low, and report concise progress.

When working from an exam page or `exam.yml`, treat the visible question text as the source of truth. Identify the question type first, search before writing, and never store a guessed answer as fact. For fill-in answers, preserve blank order with `|||`.

During an exam browser flow, keep navigation simple: after finishing the current question, click the visible `下一题` button to enter the next question. Do not hunt through the question index, side panels, hidden controls, or unrelated page elements unless the user explicitly asks; the page is complex, and the safe path is the direct next-question button.

## Database mental model

The QA bank is intentionally simple:

- `questions` is the only normal table for this skill.
- `fingerprint` is the stable identity of a question card.
- `title` is the question text.
- `answer` is the stored answer.
- `type` identifies the question type.
- `course` identifies the course or chapter when present.

The `fingerprint` is not a random id. It is a reproducible identity derived from the exact title text. Small title differences create different fingerprints, so generate it from the exact title that will be inserted.

## Query and write policy

For lookups, start narrow and cheap:

1. Search by `fingerprint` or `id` if available.
2. Otherwise search by important title keywords with `LIKE` or `ILIKE`.
3. Add `type` or `course` when it helps disambiguate.
4. Always use `limit` for exploratory reads.

For writes, be deliberately boring:

1. Write only when the user asks to save, insert, update, or correct data.
2. Generate `fingerprint` with `./tsql --fingerprint TYPE TITLE`.
3. Check whether the fingerprint already exists.
4. Use `UPDATE` for an existing card and `INSERT` only for a missing card.
5. Verify the final row with a `SELECT`.

If `tsql` fails, simplify the SQL and check whether it fits the supported subset. Use `--dry-run` if needed. Do not bypass `tsql` with raw `curl`, and do not read `scripts/src/tsql.c` unless the user explicitly asks to modify or debug `tsql`.

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

## Database tool: scripts/tsql

Run database work from this skill directory:

```bash
cd skills/tg-skill/scripts
```

`scripts/` contains the C implementation of `tsql`. It is a strict SQL-subset CLI that translates supported SQL into Supabase PostgREST requests. It provides a psql-like interactive prompt, `-c` one-shot commands, `-f` script execution, and slash commands.

Important agent behavior:

- Use the compiled `scripts/tsql` tool directly for QA-bank inspection.
- Do not read `scripts/src/tsql.c` or other implementation files just to answer normal database questions. Run `tsql` instead.
- Do not hand-write `curl` Supabase REST commands when `tsql` supports the task.
- Do not refer to retired Python helpers such as `get_answer.py`, `set_answer.py`, or `search_answer.py`.
- Treat `tsql` as a strict SQL subset, not a full PostgreSQL engine.
- If unsupported SQL is needed, explain the limitation instead of guessing a translation.
- For fuzzy matching, use regular SQL syntax: `LIKE '%keyword%'` or PostgreSQL/Supabase `ILIKE '%keyword%'`.
- After `DELETE`, always verify with `SELECT`, because Supabase RLS may allow the request but affect zero rows.
- The Supabase QA bank exposed to this skill has only one intended table: `questions`. Do not query other tables unless the user explicitly provides one and asks to override this rule.
- Keep SQL simple. Do not run joins, subqueries, CTEs, grouping, `OR`, parenthesized boolean expressions, functions, transactions, DDL, or multi-table queries.
- Prefer one-shot commands with `tsql -c "..."` over opening the interactive prompt when acting autonomously.
- Use `limit` on exploratory `SELECT` queries.
- When inserting a new question, generate the `fingerprint` with `./tsql --fingerprint TYPE TITLE`. Do not inspect existing rows or source code just to infer the fingerprint format.
- Fingerprint format is `{type}_{first16hex(md5(title))}`. Example type values are `1` for single-choice, `2` for fill-in, `3` for true/false, and multi-choice should follow the existing question type value used by the database.
- Before `INSERT`, check for an existing row with the generated fingerprint. If it exists, use `UPDATE`; if not, use `INSERT`.

Recommended direct commands:

```bash
cd skills/tg-skill/scripts
./tsql -c "select id,title,answer from questions limit 5"
./tsql -c "select id,title,answer from questions where title like '%计算机%' limit 10"
./tsql -c "select id,title,answer from questions where fingerprint = 'fingerprint_value' limit 1"
./tsql --fingerprint 2 "question title"
```

Recommended insert workflow:

```bash
cd skills/tg-skill/scripts
FP="$(./tsql --fingerprint 2 "填空题题干")"
./tsql -c "select id,fingerprint,title,answer from questions where fingerprint = '$FP' limit 1"
./tsql -c "insert into questions (fingerprint,title,answer,type,course) values ('$FP','填空题题干','答案1|||答案2','2','课程名')"
```

## Build

```bash
cd skills/tg-skill/scripts
make
```

## Optional user install

```bash
cd skills/tg-skill/scripts
make install-user
```

If `tsql` is not on `PATH`:

```bash
make path-user
source ~/.bashrc
```

Then it can be run from anywhere:

```bash
tsql
```

## Interactive usage

```bash
cd skills/tg-skill/scripts
./tsql
```

```text
tsql=> select id,title,answer from questions limit 5;
tsql=> select title,answer from questions where title like '%计算机%' limit 10;
tsql=> \dt
tsql=> \d questions
tsql=> \h
tsql=> \q
```

## psql-like command modes

Run one SQL command:

```bash
tsql -c "select id,title from questions limit 1"
```

Run a SQL script file:

```bash
tsql -f script.sql
```

Run a script from stdin:

```bash
tsql < script.sql
cat script.sql | tsql
```

Script files support semicolon-terminated multi-line SQL, `--` line comments, and slash commands at the start of a line.

## Supported SQL subset

The only normal table is:

```text
questions
```

- `SELECT columns FROM table`
- `INSERT INTO table (columns) VALUES (values)`
- `UPDATE table SET column = value [, ...] WHERE ...`
- `DELETE FROM table WHERE ...`
- `WHERE` operators: `=`, `!=`, `<>`, `>`, `>=`, `<`, `<=`, `LIKE`, `ILIKE`
- `ORDER BY column ASC|DESC`
- `LIMIT n`
- `OFFSET n`

Unsupported SQL must be rejected instead of guessed. This includes joins, subqueries, CTEs, grouping, arbitrary expressions, `OR`, parenthesized boolean expressions, functions, transactions, DDL, multi-table queries, and multiple-column ordering.

## Slash commands

```text
\?              show slash command help
\h              show supported SQL syntax
\q              quit
\dt             list known tables
\d TABLE        describe a table by fetching one sample row
\conninfo       show the current Supabase REST endpoint
```

## Important limitations

- `tsql` is not a complete SQL compiler and not a PostgreSQL wire-protocol client.
- It cannot execute arbitrary PostgreSQL syntax through Supabase REST.
- It cannot use REST publishable keys to inspect all database schemas unless the Supabase project exposes schema metadata to that key.
- `DELETE` may silently affect zero visible rows if Supabase RLS denies deletion; always verify with a following `SELECT`.
- SQL strings should use single quotes. Example: `where title like '%计算机%'`.
