#!/usr/bin/env bash
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BIN="${ROOT}/build/latticedb"

if [[ ! -x "$BIN" ]]; then
  "${ROOT}/scripts/build.sh"
fi

cat > /tmp/latticedb_gen.sql <<'SQL'
CREATE TABLE gen (id TEXT PRIMARY KEY, v INT MERGE sum_bounded(0,1000000));
INSERT INTO gen (id,v) VALUES ('g1',1),('g2',2),('g3',3),('g4',4);
SAVE DATABASE 'gen.ldb';
EXIT;
SQL

"$BIN" < /tmp/latticedb_gen.sql
echo "Generated gen.ldb in current working directory."
