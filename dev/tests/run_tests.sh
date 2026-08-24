#!/usr/bin/env sh
# run_tests.sh - Torvik v1.5.x end-to-end test suite (Linux).
# Usage: sh run_tests.sh [path-to-torvc]
#   Defaults to `torvc` on PATH.
#
# v1.5.0: rune ships from its own repository and is tested there
# (rune/dev/tests/run_tests.sh), so this suite needs only the compiler. That means
# a compiler regression can be caught without rune installed, and a rune regression
# without building the compiler.
# All work happens in ./tv-test-work (NOT /tmp - safe for hardened noexec /tmp).
# Exit code: 0 all pass, 1 any failure. Full log in ./tv-test-work/results.log

TORVC="${1:-torvc}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$HERE/tv-test-work"
LOG="$WORK/results.log"
rm -rf "$WORK"; mkdir -p "$WORK"
: > "$LOG"

PASS=0; FAIL=0; FAILED_NAMES=""

note() { echo "$1" | tee -a "$LOG"; }

command -v "$TORVC" >/dev/null 2>&1 || { echo "torvc not found ($TORVC)"; exit 1; }
note "== torvc: $($TORVC --version 2>&1) =="
note "== host:  $(uname -sr) =="

# ---------- positive cases: compile, run, diff stdout, check exit code ----------
note ""
note "== POSITIVE CASES =="
for tv in "$HERE"/cases/pos/*.tv; do
    base="$(basename "$tv" .tv)"
    case "$base" in helpers*) continue;; esac   # support modules, not test entries
    exp="$HERE/cases/pos/$base.out"
    expcode_f="$HERE/cases/pos/$base.code"
    stdin_f="$HERE/cases/pos/$base.in"
    d="$WORK/pos_$base"; mkdir -p "$d"
    cp "$tv" "$d/"
    # copy any helper module files (apply targets) into ./src (apply looks there)
    mkdir -p "$d/src"
    for h in "$HERE"/cases/pos/helpers*.tv; do [ -f "$h" ] && cp "$h" "$d/src/"; done
    (
      cd "$d" || exit 9
      if ! "$TORVC" "$base.tv" -o prog -q > compile.log 2>&1; then
          exit 7
      fi
      if [ -f "$stdin_f" ]; then ./prog < "$stdin_f" > actual.out 2>&1; else ./prog > actual.out 2>&1; fi
      echo "$?" > actual.code
    )
    rc=$?
    if [ "$rc" = "7" ]; then
        FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES pos/$base(compile)"
        note "FAIL  pos/$base  (did not compile)"; sed 's/^/      /' "$d/compile.log" >> "$LOG"
        continue
    fi
    want_code=1; [ -f "$expcode_f" ] && want_code="$(cat "$expcode_f")"
    got_code="$(cat "$d/actual.code")"
    ok=1
    if ! diff -u "$exp" "$d/actual.out" > "$d/diff.txt" 2>&1; then ok=0; fi
    if [ -f "$expcode_f" ]; then
        [ "$got_code" = "$want_code" ] || ok=0
    else
        [ "$got_code" = "0" ] || ok=0
    fi
    if [ "$ok" = "1" ]; then
        PASS=$((PASS+1)); note "ok    pos/$base"
    else
        FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES pos/$base"
        note "FAIL  pos/$base  (exit=$got_code)"
        sed 's/^/      /' "$d/diff.txt" | head -30 >> "$LOG"
    fi
done

# ---------- negative cases: must FAIL to compile (exit 1) with expected text ----------
note ""
note "== NEGATIVE CASES (expected clean compile errors) =="
for tv in "$HERE"/cases/neg/*.tv; do
    base="$(basename "$tv" .tv)"
    errf="$HERE/cases/neg/$base.err"
    d="$WORK/neg_$base"; mkdir -p "$d"; cp "$tv" "$d/"
    # Optional NAME.flags - needed when what is refused is a FLAG rather than the
    # source. A bad output path has no representation inside a .tv file.
    nflags_f="$HERE/cases/neg/$base.flags"
    nflags=""
    [ -f "$nflags_f" ] && nflags="$(cat "$nflags_f")"
    (
      cd "$d" || exit 9
      if [ -n "$nflags" ]; then
          # Unquoted on purpose: the flags file supplies its own words, and the
          # point of these cases is to pass shell syntax through as one argument.
          "$TORVC" "$base.tv" -q $nflags > compile.log 2>&1
      else
          "$TORVC" "$base.tv" -o prog -q > compile.log 2>&1
      fi
      echo "$?" > compile.code
    )
    code="$(cat "$d/compile.code")"
    if [ "$code" = "0" ]; then
        FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES neg/$base(compiled!)"
        note "FAIL  neg/$base  (COMPILED - expected a clean error)"
        continue
    fi
    if [ "$code" != "1" ]; then
        FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES neg/$base(exit=$code)"
        note "FAIL  neg/$base  (exit=$code, expected 1 - internal error?)"
        sed 's/^/      /' "$d/compile.log" | head -10 >> "$LOG"
        continue
    fi
    if grep -qi "$(cat "$errf")" "$d/compile.log"; then
        PASS=$((PASS+1)); note "ok    neg/$base"
    else
        FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES neg/$base(msg)"
        note "FAIL  neg/$base  (error text missing '$(cat "$errf")')"
        sed 's/^/      /' "$d/compile.log" | head -10 >> "$LOG"
    fi
done

# ---------- torvc flag cases ----------
note ""
note "== TORVC FLAG CASES =="
d="$WORK/flags"; mkdir -p "$d"; cd "$d"
printf 'df main() -> void {\n    echo!("flagtest");\n}\n' > flag.tv

if "$TORVC" flag.tv -o out1 -q > q.log 2>&1 && [ ! -s q.log ] && [ "$(./out1)" = "flagtest" ]; then
    PASS=$((PASS+1)); note "ok    flags/quiet"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/quiet"; note "FAIL  flags/quiet"; fi

if "$TORVC" flag.tv -o out2 > v.log 2>&1 && grep -qi "compiled successfully" v.log; then
    PASS=$((PASS+1)); note "ok    flags/success_message"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/success_message"; note "FAIL  flags/success_message"; fi

if "$TORVC" flag.tv -o out3 --final -q > f.log 2>&1 && [ "$(./out3)" = "flagtest" ]; then
    PASS=$((PASS+1)); note "ok    flags/final"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/final"; note "FAIL  flags/final"; fi

if "$TORVC" --version | grep -q "1\." && "$TORVC" -h > h.log 2>&1; then
    PASS=$((PASS+1)); note "ok    flags/version_help"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/version_help"; note "FAIL  flags/version_help"; fi

# v1.5.0: no -o => RUN mode (python3-style). Executes the file, forwards extra
# args to the program, and leaves no binary behind.
rm -f flag
if [ "$("$TORVC" flag.tv 2>/dev/null)" = "flagtest" ] && [ ! -e flag ]; then
    PASS=$((PASS+1)); note "ok    flags/run_mode"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/run_mode"; note "FAIL  flags/run_mode"; fi

printf 'df main() -> void {\n    fixed n: i64 = args();\n    check n >= 2 { echo!(args_get(1)); } fallback { echo!("noargs"); }\n}\n' > argfwd.tv
if [ "$("$TORVC" argfwd.tv HELLO 2>/dev/null)" = "HELLO" ]; then
    PASS=$((PASS+1)); note "ok    flags/run_args"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/run_args"; note "FAIL  flags/run_args"; fi

# missing source file: clean user error (exit 1, not 70)
"$TORVC" no_such_file.tv -o x -q > m.log 2>&1
rc=$?
if [ "$rc" = "1" ] && ! grep -q "TVC-" m.log; then
    PASS=$((PASS+1)); note "ok    flags/missing_source"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/missing_source"; note "FAIL  flags/missing_source (exit=$rc)"; fi

# ---------- warning cases ----------
note ""
note "== WARNING CASES =="
d="$WORK/warns"; mkdir -p "$d"; cd "$d"
cat > warny.tv <<'TVEOF'
df main() -> void {
    set unused: i64 = 1;
    echo!("ran");
    return;
    echo!("dead");
}
TVEOF

"$TORVC" warny.tv -o wy > w1.log 2>&1
if [ $? = 0 ] && grep -q "warning:" w1.log && grep -q "unused variable" w1.log && grep -q "unreachable code" w1.log && [ "$(./wy)" = "ran" ]; then
    PASS=$((PASS+1)); note "ok    warns/emitted_nonfatal"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/emitted_nonfatal"; note "FAIL  warns/emitted_nonfatal"; fi

"$TORVC" warny.tv -o wy2 --no-warn > w2.log 2>&1
if [ $? = 0 ] && ! grep -q "warning:" w2.log; then
    PASS=$((PASS+1)); note "ok    warns/no_warn_flag"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/no_warn_flag"; note "FAIL  warns/no_warn_flag"; fi

# -q suppresses the success banner but NOT warnings - they are diagnostics.
# (-q is how a project manager invokes torvc, so this is the path those users see.)
"$TORVC" warny.tv -o wy3 -q > w3.log 2>&1
if [ $? = 0 ] && grep -q "warning:" w3.log && ! grep -q "Compiled successfully" w3.log; then
    PASS=$((PASS+1)); note "ok    warns/quiet_keeps_warnings"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/quiet_keeps_warnings"; note "FAIL  warns/quiet_keeps_warnings"; fi

printf 'df main() -> void {\n    fixed _ignored: i64 = 5;\n    echo!("clean");\n}\n' > uscore.tv
"$TORVC" uscore.tv -o us > w4.log 2>&1
if [ $? = 0 ] && ! grep -q "warning:" w4.log; then
    PASS=$((PASS+1)); note "ok    warns/underscore_exempt"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/underscore_exempt"; note "FAIL  warns/underscore_exempt"; fi

# directive cases
cat > direc.tv <<'TVEOF'
!@ALLOW[unused_variable];
df main() -> void {
    set unused: i64 = 1;
    echo!("ran");
    return;
    echo!("dead");
}
TVEOF
"$TORVC" direc.tv -o dr > w5.log 2>&1
if [ $? = 0 ] && grep -q "unreachable code" w5.log && ! grep -q "unused variable" w5.log; then
    PASS=$((PASS+1)); note "ok    warns/allow_category"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/allow_category"; note "FAIL  warns/allow_category"; fi

sed 's/!@ALLOW\[unused_variable\];/!@NO_WARN;/' direc.tv > direc2.tv
"$TORVC" direc2.tv -o dr2 > w6.log 2>&1
if [ $? = 0 ] && ! grep -q "warning:" w6.log; then
    PASS=$((PASS+1)); note "ok    warns/no_warn_directive"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/no_warn_directive"; note "FAIL  warns/no_warn_directive"; fi

printf '!@NO_WRN;\ndf main() -> void { echo!("x"); }\n' > direc3.tv
"$TORVC" direc3.tv -o dr3 > w7.log 2>&1
if [ $? = 1 ] && grep -q "unknown warning directive" w7.log; then
    PASS=$((PASS+1)); note "ok    warns/typo_directive_errors"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/typo_directive_errors"; note "FAIL  warns/typo_directive_errors"; fi

# unused-result warning + apply line translation
cat > ur.tv <<'TVEOF'
df init(code: i64) -> i64 { return code; }
df main() -> void { init(1); echo!("ran"); }
TVEOF
"$TORVC" ur.tv -o ur > w8.log 2>&1
if [ $? = 0 ] && grep -q "unused_result\|result of 'init' is unused" w8.log; then
    PASS=$((PASS+1)); note "ok    warns/unused_result"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/unused_result"; note "FAIL  warns/unused_result"; fi

printf 'apply std;\ndf main() -> void {\n    nosuchfn();\n}\n' > lineoff.tv
"$TORVC" lineoff.tv -o lo > w9.log 2>&1
if [ $? = 1 ] && grep -q "lineoff.tv:3:" w9.log; then
    PASS=$((PASS+1)); note "ok    warns/apply_line_numbers"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/apply_line_numbers"; note "FAIL  warns/apply_line_numbers"; fi

# ---------- summary ----------
# v1.5.4: source-level gate against the command-injection family, and a fuzzer
# asserting the compiler never loses control on user input. See the scripts.
if sh "$HERE/check_cmdline.sh" > "$WORK/cmdline.log" 2>&1; then
    PASS=$((PASS+1)); note "ok    security/cmdline_validated"
else
    FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES security/cmdline_validated"
    note "FAIL  security/cmdline_validated"
    head -12 "$WORK/cmdline.log" | while IFS= read -r l; do note "      $l"; done
fi

if sh "$HERE/fuzz.sh" "$TORVC" 150 > "$WORK/fuzz.log" 2>&1; then
    PASS=$((PASS+1)); note "ok    fuzz/crash_freedom"
else
    FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES fuzz/crash_freedom"
    note "FAIL  fuzz/crash_freedom"
    head -12 "$WORK/fuzz.log" | while IFS= read -r l; do note "      $l"; done
fi

note ""
note "== SUMMARY: $PASS passed, $FAIL failed =="
[ -n "$FAILED_NAMES" ] && note "failed:$FAILED_NAMES"
note "(full log: $LOG)"
[ "$FAIL" = "0" ] && exit 0 || exit 1
