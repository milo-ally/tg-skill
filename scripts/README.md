# scripts/tsql

This `scripts/` directory contains `tsql`, a small C SQL-subset CLI for the Supabase REST API used by this skill.

It accepts a strict subset of common SQL statements and translates them to PostgREST requests. SQL keywords are case-insensitive. Unsupported SQL is rejected instead of being guessed.

## Build

```bash
make
```

Requires a C compiler, `make`, and the `curl` command-line program.

## Install to PATH

Install for the current user:

```bash
make install-user
```

If `tsql` is not found, add `~/.local/bin` to your shell PATH:

```bash
make path-user
source ~/.bashrc
```

Then run it from anywhere:

```bash
tsql
```

## Configuration

Defaults are built in for the current Supabase project. You can override them:

```bash
export TSQL_SUPABASE_URL='https://your-project.supabase.co'
export TSQL_SUPABASE_KEY='your-supabase-api-key'
```

## Usage

Interactive mode:

```bash
./tsql
```

```text
tsql interactive mode. Type exit, quit, or \q to quit.
tsql=> select * from questions limit 5
tsql=> \dt
tsql=> \d questions
tsql=> \q
```

```bash
tsql -c "select * from questions limit 5"
tsql -f script.sql
./tsql "select * from questions limit 5"
./tsql "SELECT id,title,answer FROM questions WHERE type = '3' ORDER BY id DESC LIMIT 10"
./tsql "select title,answer from questions where title ilike '%Amdahl%' limit 5"
./tsql "select title,answer from questions where title like '%计算机%' limit 5"
```

Run SQL scripts from stdin:

```bash
tsql < script.sql
cat script.sql | tsql
```

Script files support psql-like basics: semicolon-terminated multi-line SQL, `--` line comments, and slash commands at the start of a line.

```sql
-- script.sql
\dt

select title, answer
from questions
where title like '%计算机%'
limit 5;
```

Inspect generated REST request without executing:

```bash
./tsql --dry-run "select * from questions where type = '3' limit 5"
```

## CRUD examples

Write operations depend on Supabase RLS policies and API key permissions.

The test script checks `-c`, `-f`, stdin scripts, slash commands, `LIKE` fuzzy queries, invalid SQL rejection, and the installed `tsql` command if it is available on `PATH`.

Run the CRUD integration test suite:

```bash
make test-crud
```

It executes four SQL scripts in `tests/`:

```text
tests/01_create.sql
tests/02_read.sql
tests/03_update.sql
tests/04_delete.sql
```

The test row uses fingerprint `tsql_crud_test_001`, and the runner performs cleanup on exit.

## Slash commands

PostgreSQL-style slash commands are available in interactive mode and script input:

```text
\?              show slash command help
\h              show supported SQL syntax
\q              quit
\dt             list known tables
\d TABLE        describe a table by fetching one sample row
\conninfo       show the current Supabase REST endpoint
```

## Supported SQL subset

- `SELECT columns FROM table`
- `INSERT INTO table (columns) VALUES (values)`
- `UPDATE table SET column = value [, ...] WHERE ...`
- `DELETE FROM table WHERE ...`
- `WHERE` operators: `=`, `!=`, `<>`, `>`, `>=`, `<`, `<=`, `LIKE`, `ILIKE`
- `ORDER BY column ASC|DESC`
- `LIMIT n`
- `OFFSET n`

Unsupported syntax is rejected, including `OR`, parentheses in `WHERE`, joins, subqueries, CTEs, grouping, arbitrary expressions, and multiple-column ordering.

## Limitations

This is not a real PostgreSQL connection and not a complete SQL compiler. It cannot execute arbitrary SQL over the Supabase REST API.

Fuzzy queries use standard SQL `LIKE` and `ILIKE` operators:

```sql
where title LIKE '%Amdahl%'
where title ILIKE '%流水线%'
```

These become Supabase REST filters like `title=like.*keyword*` or `title=ilike.*keyword*`.

Unsupported or limited features include joins, subqueries, CTEs, transactions, DDL, functions, `GROUP BY`, `HAVING`, complex boolean expressions, and arbitrary PostgreSQL syntax.
