#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT_DIR/tsql"
TEST_FP="tsql_crud_test_$(date +%s)_$$"
TMP_DIR="$(mktemp -d /tmp/tsql_crud.XXXXXX)"

render_sql() {
  local src="$1"
  local dst="$TMP_DIR/$(basename "$src")"
  sed "s/__TEST_FP__/$TEST_FP/g" "$src" > "$dst"
  printf '%s\n' "$dst"
}

cleanup_db() {
  "$BIN" -c "delete from questions where fingerprint = '$TEST_FP'" >/dev/null 2>&1 || true
}

cleanup() {
  cleanup_db
  rm -rf "$TMP_DIR"
}

trap cleanup EXIT

if [[ ! -x "$BIN" ]]; then
  make -C "$ROOT_DIR" >/dev/null || exit 1
fi

printf '== tsql CRUD integration test ==\n'
printf 'test fingerprint: %s\n' "$TEST_FP"

printf '\n== pre-clean ==\n'
cleanup_db

printf '\n== create ==\n'
CREATE_SQL="$(render_sql "$ROOT_DIR/tests/01_create.sql")"
"$BIN" -f "$CREATE_SQL" || exit 1

printf '\n== read ==\n'
READ_SQL="$(render_sql "$ROOT_DIR/tests/02_read.sql")"
READ_OUT="$($BIN -f "$READ_SQL")" || exit 1
printf '%s\n' "$READ_OUT"
grep -F "$TEST_FP" <<<"$READ_OUT" >/dev/null || { printf 'read check failed\n' >&2; exit 1; }

printf '\n== update ==\n'
UPDATE_SQL="$(render_sql "$ROOT_DIR/tests/03_update.sql")"
UPDATE_OUT="$($BIN -f "$UPDATE_SQL")" || exit 1
printf '%s\n' "$UPDATE_OUT"
grep -F '"answer":"B"' <<<"$UPDATE_OUT" >/dev/null || { printf 'update answer check failed\n' >&2; exit 1; }
grep -F '"confidence":"medium"' <<<"$UPDATE_OUT" >/dev/null || { printf 'update confidence check failed\n' >&2; exit 1; }

printf '\n== delete ==\n'
DELETE_SQL="$(render_sql "$ROOT_DIR/tests/04_delete.sql")"
DELETE_OUT="$($BIN -f "$DELETE_SQL")" || exit 1
printf '%s\n' "$DELETE_OUT"
if grep -F "$TEST_FP" <<<"$DELETE_OUT" >/dev/null; then
  printf 'delete check failed: test row still visible; current API key/RLS likely does not allow DELETE\n' >&2
  exit 1
fi

printf '\n== CRUD integration test passed ==\n'
