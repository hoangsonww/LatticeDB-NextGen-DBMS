#!/usr/bin/env bash
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BIN="${ROOT}/build/latticedb"

if [[ ! -x "$BIN" ]]; then
  "${ROOT}/scripts/build.sh"
fi

SQL_FILE="$(mktemp)"
trap 'rm -f "$SQL_FILE"' EXIT

cat > "$SQL_FILE" <<'SQL'
CREATE TABLE bench (
  id TEXT PRIMARY KEY,
  v INT MERGE sum_bounded(0, 1000000000)
);
SQL

# Generate 10k inserts
for i in $(seq 1 10000); do
  echo "INSERT INTO bench (id, v) VALUES ('k$i', $i);" >> "$SQL_FILE"
done
echo "SELECT DP_COUNT(*) FROM bench WHERE v >= 5000;" >> "$SQL_FILE"
echo "EXIT;" >> "$SQL_FILE"

echo "[bench] Running..."
/usr/bin/time -f "\nTime %E  CPU %P  RSS %M KB" "$BIN" < "$SQL_FILE"
