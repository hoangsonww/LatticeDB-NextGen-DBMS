#!/usr/bin/env bash
set -euo pipefail
# Re-generate expected outputs from the current binary for deterministic tests.
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BIN="${ROOT}/build/latticedb"

if [[ ! -x "$BIN" ]]; then
  "${ROOT}/scripts/build.sh"
fi

pushd "${ROOT}/tests" >/dev/null
declare -a CASES=("temporal" "merge" "vector" "update_period" "persistence_load")

for c in "${CASES[@]}"; do
  echo "[regen] $c"
  out="$(mktemp)"
  "$BIN" < "cases/${c}.sql" > "$out"
  if [[ "$c" == "merge" ]]; then
    # Sort only the body rows (keep header and messages)
    awk '
      BEGIN{header_found=0}
      /^id \| name \| credits \| tags$/ { print; header_found=1; next }
      {
        if (header_found==1 && $0 !~ /^Bye\.$/) body[NR]=$0;
        else if (header_found==0) print;
        else if ($0 ~ /^Bye\.$/) bye=$0;
      }
      END{
        n=asorti(body, idx);
        for (i=1;i<=n;i++) print body[idx[i]];
        if (bye!="") print bye;
      }' "$out" > "expected/${c}.out"
  else
    cp "$out" "expected/${c}.out"
  fi
  rm -f "$out"
done
popd >/dev/null
echo "[regen] done."
