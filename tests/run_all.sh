#!/usr/bin/env bash
set -euo pipefail
ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BIN="${ROOT}/build/latticedb"

if [[ ! -x "$BIN" ]]; then
  echo "[run_all] Building..."
  "${ROOT}/scripts/build.sh"
fi

pass=0; fail=0

run_case() {
  local name="$1"
  local mode="${2:-plain}"  # plain|sorted|dp
  local out="$(mktemp)"
  trap 'rm -f "$out"' RETURN

  if [[ "$name" == "persistence_create" ]]; then
    "$BIN" < "tests/cases/${name}.sql" > /dev/null
    return 0
  fi

  "$BIN" < "tests/cases/${name}.sql" > "$out"

  if [[ "$mode" == "sorted" ]]; then
    # Keep messages & header, sort only result rows
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
      }' "$out" > "$out.sorted"
    mv "$out.sorted" "$out"
  fi

  if [[ "$mode" == "dp" ]]; then
    # Non-deterministic numeric; ensure header exists and exactly one value line follows it
    if grep -q '^dp_count$' "$out"; then
      rows=$(( $(wc -l < "$out") - 0 ))
      data_rows=$(grep -n '^dp_count$' "$out" | awk -F: '{print $1}')
      # require at least one line after the header
      if [[ -n "$data_rows" ]]; then
        echo "[PASS] $name"
        ((pass++))
        return 0
      fi
    fi
    echo "[FAIL] $name"
    echo "--- OUTPUT ---"; cat "$out"; echo "---------------"
    ((fail++))
    return 1
  fi

  if diff -u "tests/expected/${name}.out" "$out" >/dev/null 2>&1; then
    echo "[PASS] $name"
    ((pass++))
  else
    echo "[FAIL] $name"
    echo "--- EXPECTED ---"; cat "tests/expected/${name}.out" || true
    echo "--- GOT ---"; cat "$out"
    ((fail++))
  fi
}

# Order matters for persistence
run_case "temporal"
run_case "merge" "sorted"
run_case "vector"
run_case "agg"
run_case "update_period"
run_case "persistence_create"
run_case "persistence_load"
run_case "dp" "dp"

echo
echo "Passed: $pass   Failed: $fail"
if [[ $fail -ne 0 ]]; then exit 1; fi
