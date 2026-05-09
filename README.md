# tg-skill

`tg-skill` is an agent skill for the Touge/头歌 platform. It uses `playwright-cli` for browser automation and the C language `tsql` CLI in `scripts/` for Supabase QA-bank inspection and CRUD checks.

The old Python helper scripts have been retired. `scripts/` is now the `tsql` C project directory.

## Layout

```text
tg-skill/
├── scripts/
│   ├── Makefile
│   ├── README.md
│   ├── src/tsql.c
│   ├── tests/
│   │   ├── 01_create.sql
│   │   ├── 02_read.sql
│   │   ├── 03_update.sql
│   │   ├── 04_delete.sql
│   │   └── test.sh
│   └── tsql
├── SKILL.md
├── LICENCE
├── README.md
└── .gitignore
```

## Browser automation

Use `playwright-cli` for Touge/头歌 browser operations. See `SKILL.md` for internal-network and VPN login flows.

## Database CLI

Build `tsql`:

```bash
cd scripts
make
```

Run interactively:

```bash
./tsql
```

Run one command:

```bash
./tsql -c "select id,title,answer from questions limit 5"
```

Run a SQL script:

```bash
./tsql -f tests/02_read.sql
```

Run CRUD integration checks:

```bash
make test-crud
```

## Notes

- `tsql` is a strict SQL subset translated to Supabase REST, not a full PostgreSQL engine.
- Use `LIKE '%keyword%'` or `ILIKE '%keyword%'` for fuzzy matching.
- Verify `DELETE` with a following `SELECT`, because Supabase RLS may deny deletion while the request itself succeeds.

## License

MIT
