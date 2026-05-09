---
name: tg-skill
description: Automate browser interactions for zcst students on Touge/头歌 and inspect the Supabase QA bank with the C tsql CLI.
tags: [touge, playwright, qa-bank, education, tsql, c]
---

# tg-skill

This skill supports Touge/头歌 browser automation and Supabase QA-bank inspection.

Use `playwright-cli` for browser operations. Use `scripts/` for the modern C language `tsql` command-line database tool.

The old Python helper scripts have been retired. `scripts/` is now the `tsql` C project directory.

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
