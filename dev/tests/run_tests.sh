#!/usr/bin/env sh
# run_tests.sh - Torvik v1.2.x end-to-end test suite (Linux).
# Usage: sh run_tests.sh [path-to-torvc] [path-to-rune]
#   Defaults to `torvc` / `rune` on PATH.
# All work happens in ./tv-test-work (NOT /tmp - safe for hardened noexec /tmp).
# Exit code: 0 all pass, 1 any failure. Full log in ./tv-test-work/results.log

TORVC="${1:-torvc}"
RUNE="${2:-rune}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$HERE/tv-test-work"
LOG="$WORK/results.log"
rm -rf "$WORK"; mkdir -p "$WORK"
: > "$LOG"

PASS=0; FAIL=0; FAILED_NAMES=""

note() { echo "$1" | tee -a "$LOG"; }

command -v "$TORVC" >/dev/null 2>&1 || { echo "torvc not found ($TORVC)"; exit 1; }
command -v "$RUNE"  >/dev/null 2>&1 || { echo "rune not found ($RUNE)";  exit 1; }
note "== torvc: $($TORVC --version 2>&1) =="
note "== rune:  $($RUNE --version 2>&1) =="
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
    (
      cd "$d" || exit 9
      "$TORVC" "$base.tv" -o prog -q > compile.log 2>&1
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

# ---------- rune project-tool cases ----------
note ""
note "== RUNE CASES =="
rune_case() { # $1 name, $2 expected(0/nonzero), rest: description; body via stdin executed in fresh dir
    name="$1"; d="$WORK/rune_$name"; mkdir -p "$d"
    ( cd "$d" && sh -s ) > "$WORK/rune_$name.log" 2>&1
    rc=$?
    if [ "$rc" = "0" ]; then PASS=$((PASS+1)); note "ok    rune/$name"
    else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES rune/$name"
        note "FAIL  rune/$name"; sed 's/^/      /' "$WORK/rune_$name.log" | tail -15 >> "$LOG"; fi
}

rune_case new_build_run <<EOF
set -e
"$RUNE" new myapp
[ -f myapp/torvik.rune ] && [ -f myapp/src/main.tv ]
cd myapp
"$RUNE" build
[ -x build/myapp ]
out=\$("$RUNE" run)
echo "\$out" | grep -qi "hello"
EOF

rune_case incremental <<EOF
set -e
"$RUNE" new capp && cd capp
"$RUNE" run > first.log 2>&1
"$RUNE" run > second.log 2>&1
grep -qi "cache\|up.to.date\|unchanged" second.log || ! grep -qi "compil" second.log
EOF

rune_case exit_propagation <<EOF
set -e
"$RUNE" new eapp && cd eapp
cat > src/main.tv <<'TV'
df main() -> void {
    exit(4);
}
TV
rc=0
"$RUNE" run || rc=\$?
[ "\$rc" = "4" ]
EOF

rune_case clean_list_version <<EOF
set -e
"$RUNE" new lapp && cd lapp
"$RUNE" build
"$RUNE" list | grep -qi "lapp"
"$RUNE" clean
[ ! -d build ]
"$RUNE" version | grep -q "1\."
EOF

rune_case min_version_gate <<EOF
set -e
"$RUNE" new vapp && cd vapp
# require an impossibly new toolchain; build must refuse and mention update
sed -i 's/^torvik *=.*/torvik = "99.0.0"/' torvik.rune || echo 'torvik = "99.0.0"' >> torvik.rune
if "$RUNE" build > b.log 2>&1; then exit 1; fi
grep -qi "update\|version" b.log
EOF

rune_case missing_entry <<EOF
set -e
mkdir p1 && cd p1
printf '[project]\nname = "p1"\n' > torvik.rune
if "$RUNE" build > b.log 2>&1; then exit 1; fi
grep -qi "main.tv" b.log
EOF

rune_case bad_name <<EOF
set -e
if "$RUNE" new "bad/name" > n.log 2>&1; then exit 1; fi
if "$RUNE" new "" > n2.log 2>&1; then exit 1; fi
exit 0
EOF

rune_case final_build <<EOF
set -e
"$RUNE" new fapp && cd fapp
"$RUNE" build --final
[ -x build/fapp ]
./build/fapp | grep -qi "hello"
EOF

# std version gate
d="$WORK/rune_std_gate"; mkdir -p "$d"; cd "$d"
"$RUNE" new gproj > /dev/null 2>&1
cd gproj
printf 'std         = "9.0.0"\n' >> torvik.rune
"$RUNE" build > g1.log 2>&1
r1=$?
sed 's/std         = "9.0.0"/std         = "0.1.0"/' torvik.rune > t.rune && mv t.rune torvik.rune
"$RUNE" build > g2.log 2>&1
r2=$?
if [ $r1 = 1 ] && grep -q "requires standard library" g1.log && [ $r2 = 0 ]; then
    PASS=$((PASS+1)); note "ok    rune/std_version_gate"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES rune/std_version_gate"; note "FAIL  rune/std_version_gate"; fi
cd "$WORK"

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

# no -o: name derived from source
rm -f flag
if "$TORVC" flag.tv -q > d.log 2>&1 && [ -x flag ] && [ "$(./flag)" = "flagtest" ]; then
    PASS=$((PASS+1)); note "ok    flags/derived_name"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES flags/derived_name"; note "FAIL  flags/derived_name"; fi

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

"$TORVC" warny.tv -o wy3 -q > w3.log 2>&1
if [ $? = 0 ] && ! grep -q "warning:" w3.log; then
    PASS=$((PASS+1)); note "ok    warns/quiet_suppresses"
else FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES warns/quiet_suppresses"; note "FAIL  warns/quiet_suppresses"; fi

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

# ---------- summary ----------
note ""
note "== SUMMARY: $PASS passed, $FAIL failed =="
[ -n "$FAILED_NAMES" ] && note "failed:$FAILED_NAMES"
note "(full log: $LOG)"
[ "$FAIL" = "0" ] && exit 0 || exit 1
