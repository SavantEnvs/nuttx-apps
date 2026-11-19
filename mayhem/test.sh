#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the suites mayhem/build.sh built (normal flags), exit 0 = pass:
#   * mayhem/out/nxinit_test — upstream's own cmocka suite for system/nxinit
#     (system/nxinit/test/*.c: argument splitting, RC sections, actions, services);
#   * mayhem/out/selftest    — known-answer tests for the other fuzzed modules
#     (codecs, netlib URL parsing, inifile, minibasic, xmlrpc). PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

OUT="$SRC/mayhem/out"
for b in nxinit_test selftest; do
  [ -x "$OUT/$b" ] || { echo "missing $OUT/$b — mayhem/build.sh did not build it" >&2; emit_ctrf "cmocka+selftest+bas+zmodem" 0 1 0; exit 1; }
done

passed=0 failed=0 skipped=0
cd "${TMPDIR:-/tmp}"

# cmocka: one "[       OK ]" / "[  FAILED  ]" / "[  SKIPPED ]" line per test, then a summary.
log="$("$OUT/nxinit_test" 2>&1)"; rc=$?
echo "$log"
ok=$(grep -c '^\[       OK \]' <<<"$log" || true)
bad=$(grep -c '^\[  FAILED  \] test_' <<<"$log" || true)
skip=$(grep -c '^\[  SKIPPED \] test_' <<<"$log" || true)
skip=$(( skip / 2 ))            # cmocka lists each skipped test twice (inline + summary)
grep -q 'test(s) run\.' <<<"$log" || bad=$(( bad + 1 ))   # suite did not run to the end
[ "$rc" -eq 0 ] || [ "$bad" -gt 0 ] || bad=1
passed=$(( passed + ok )); failed=$(( failed + bad )); skipped=$(( skipped + skip ))

log="$("$OUT/selftest" 2>&1)"; rc=$?
echo "$log"
ok=$(grep -c '^PASS ' <<<"$log" || true)
bad=$(grep -c '^FAIL ' <<<"$log" || true)
grep -q '^selftest: ' <<<"$log" || bad=$(( bad + 1 ))
[ "$rc" -eq 0 ] || [ "$bad" -gt 0 ] || bad=1
passed=$(( passed + ok )); failed=$(( failed + bad ))

# bas: known-answer BASIC programs, run by the interpreter's own main() and
# compared with their expected output (mayhem/tests/bas/<name>.out).
[ -x "$OUT/bas_test" ] || { echo "missing $OUT/bas_test — mayhem/build.sh did not build it" >&2; failed=$(( failed + 1 )); }
for prog in "$SRC"/mayhem/tests/bas/*.bas; do
  name="bas:$(basename "$prog" .bas)"
  if [ -x "$OUT/bas_test" ] && got="$("$OUT/bas_test" -r "$prog" </dev/null 2>&1)" \
     && [ "$got" = "$(cat "${prog%.bas}.out")" ]; then
    echo "PASS $name"; passed=$(( passed + 1 ))
  else
    echo "FAIL $name"; diff <(printf '%s\n' "${got:-}") "${prog%.bas}.out" | head -20
    failed=$(( failed + 1 ))
  fi
done

# zmodem: the receiver is sent two files (mayhem/zmodem/testsuite/*.zm, one
# with 16-bit, one with 32-bit CRCs); each must arrive byte for byte equal to
# mayhem/tests/zmodem/<name>.
[ -x "$OUT/zmodem_rz_test" ] || { echo "missing $OUT/zmodem_rz_test — mayhem/build.sh did not build it" >&2; failed=$(( failed + 1 )); }
for pair in hello16.zm:hello.txt hello32.zm:data.bin; do
  zm="${pair%%:*}" file="${pair#*:}" dir="$(mktemp -d)"
  [ -x "$OUT/zmodem_rz_test" ] && "$OUT/zmodem_rz_test" "$SRC/mayhem/zmodem/testsuite/$zm" "$dir" >/dev/null 2>&1
  if [ "$(ls "$dir")" = "$file" ] && cmp -s "$dir/$file" "$SRC/mayhem/tests/zmodem/$file"; then
    echo "PASS zmodem:$zm"; passed=$(( passed + 1 ))
  else
    echo "FAIL zmodem:$zm (received: $(ls "$dir" | tr '\n' ' '))"; failed=$(( failed + 1 ))
  fi
  rm -rf "$dir"
done

emit_ctrf "cmocka+selftest+bas+zmodem" "$passed" "$failed" "$skipped"
