#!/bin/sh
# fuzz.sh — feed the compiler malformed input and assert it never loses control.
#
# WHAT THIS TESTS, and it is one property only:
#
#   For ANY input, torvc must either compile it or refuse it cleanly.
#
# "Cleanly" means exit 0 (compiled) or exit 1 (a located user error). Anything
# else is a bug in the compiler: exit 70 is an internal error asking the user to
# report a toolchain fault for their own typo, and a signal death (segfault,
# abort, exit >= 128) means the compiler crashed on text somebody typed.
#
# WHY IT MATTERS HERE. Torvik reports user errors through a located-error channel
# and reserves TVC-xxxx codes for faults it could not handle. That split is only
# meaningful if the second set is genuinely unreachable from user input - and the
# only way to find out is to try. Hand-written negative cases test the mistakes we
# thought of; this tests the ones we did not.
#
# The generator is deliberately dumb. Grammar-aware fuzzing finds deeper bugs and
# takes far longer to write; mutation of real programs finds the shallow ones fast
# and needs no model of the language, so it is what runs on every suite pass.
#
# Usage: fuzz.sh [torvc] [iterations]

HERE="$(cd "$(dirname "$0")" && pwd)"
TORVC="${1:-torvc}"
N="${2:-200}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

command -v "$TORVC" >/dev/null 2>&1 || [ -x "$TORVC" ] || {
    echo "fuzz: cannot find torvc ($TORVC)"; exit 1; }

# The corpus is the real test suite - programs that exercise the whole language.
# Mutating valid input reaches deep into the compiler; random bytes mostly bounce
# off the lexer.
CORPUS="$HERE/cases/pos"
[ -d "$CORPUS" ] || { echo "fuzz: no corpus at $CORPUS"; exit 1; }

# Fragments that have historically confused compilers: unbalanced delimiters,
# deep nesting, huge and boundary numbers, empty constructs, unterminated text.
cat > "$WORK/frags" <<'FRAGS'
{
}
(
)
[
]
"
'
;
::
->
~>
!@
df
set
fixed
check
whilst
return
unsafe
extern
skil(
shape
varda<
result<
list<
0x
1e
1e999
-9223372036854775808
18446744073709551615
340282366920938463463374607431768211455
\x
\
{{{{{{{{{{
((((((((((
""""""""""
FRAGS

FAILS=0
i=0
while [ "$i" -lt "$N" ]; do
    i=$((i+1))

    # Pick a victim and a mutation.
    victim=$(ls "$CORPUS"/*.tv | sed -n "$(( (i % 90) + 1 ))p")
    [ -f "$victim" ] || victim=$(ls "$CORPUS"/*.tv | head -1)
    cp "$victim" "$WORK/f.tv"

    nlines=$(wc -l < "$WORK/f.tv")
    [ "$nlines" -lt 2 ] && continue
    at=$(( (i * 7919) % nlines + 1 ))
    frag=$(sed -n "$(( (i * 31) % 34 + 1 ))p" "$WORK/frags")

    case $(( i % 4 )) in
        0) # insert a fragment
           sed "${at}i\\
$frag" "$WORK/f.tv" > "$WORK/g.tv" 2>/dev/null || cp "$WORK/f.tv" "$WORK/g.tv" ;;
        1) # delete a line
           sed "${at}d" "$WORK/f.tv" > "$WORK/g.tv" ;;
        2) # truncate mid-file, which leaves every construct unterminated
           head -n "$at" "$WORK/f.tv" > "$WORK/g.tv" ;;
        3) # replace a line with a fragment
           sed "${at}s/.*/$(echo "$frag" | sed 's/[&/\]/\\&/g')/" "$WORK/f.tv" > "$WORK/g.tv" 2>/dev/null || cp "$WORK/f.tv" "$WORK/g.tv" ;;
    esac

    ( cd "$WORK" && "$TORVC" g.tv -o out -q > log 2>&1 )
    code=$?

    # 0 and 1 are the two correct answers. Everything else is a finding.
    if [ "$code" != "0" ] && [ "$code" != "1" ]; then
        FAILS=$((FAILS+1))
        echo "FAIL  fuzz: exit $code on mutation $i (from $(basename "$victim"))"
        if [ "$code" -ge 128 ] 2>/dev/null; then
            echo "      the compiler died on a signal - it crashed, rather than refusing"
        elif [ "$code" = "70" ]; then
            echo "      TVC internal error - a user mistake reported as a toolchain bug"
        fi
        cp "$WORK/g.tv" "$HERE/fuzz-fail-$i.tv" 2>/dev/null || true
        echo "      saved: dev/tests/fuzz-fail-$i.tv"
        head -3 "$WORK/log" | sed 's/^/      /'
        [ "$FAILS" -ge 5 ] && break
    fi
done

if [ "$FAILS" != "0" ]; then
    echo "fuzz: $FAILS input(s) made the compiler lose control over $i mutations."
    exit 1
fi

echo "ok    fuzz: $i mutations, every one compiled or refused cleanly"
exit 0
