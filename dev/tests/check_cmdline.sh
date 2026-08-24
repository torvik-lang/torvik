#!/bin/sh
# check_cmdline.sh — refuse source that builds a command line from unvalidated data.
#
# WHY THIS EXISTS. Seven command-injection issues were found in Torvik between
# 1.5.1 and 1.5.4. Every one had the same shape: a value reached a command line
# for clang or the linker without being checked, and every fix closed the site
# that had been found while leaving the others open. The last one was in `shq`
# itself — the helper the earlier fixes had been relying on.
#
# A runtime check cannot end that pattern, because the mistake is made while
# WRITING the compiler, not while running it. This reads the compiler's own
# source and fails if a value reaches a command line without ever having been
# validated.
#
# HOW IT DECIDES. Validation happens where a flag is parsed, not where the
# command is assembled, so inspecting the assembly line alone gives false alarms.
# Instead: collect every name interpolated into a command string, then require
# that each one either appears inside a validator SOMEWHERE in the file, or is a
# compiler-constructed value on the allow-list below.
#
# Validators:
#   shq(x)              quotes AND refuses shell syntax   (paths)
#   run_arg_ok(x)       refuses shell syntax              (program arguments)
#   path_arg_ok(x)      refuses shell syntax              (output paths)
#   link_lib_ok(x)      restricted charset                (library names)
#   build_triple_ok(x)  restricted charset                (target triples)
#
# A noisy gate gets ignored, which is worse than no gate — so the allow-list is
# explicit and adding to it is a deliberate act that shows up in review.
#
# Exit 0 clean, 1 with findings.

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${1:-$HERE/../../src/torvc_main.tv}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

[ -f "$SRC" ] || { echo "check_cmdline: cannot find $SRC"; exit 1; }

# Values the COMPILER constructs, never taken from a user. Safe because of where
# they come from, not because of any check.
cat > "$WORK/allow" <<'ALLOW'
tgt
lld_ent
lld_ld
bare_rt
bare_extra
bare_entry_flag
bare_ld
bare_opt
libflags
extra_objs
opt_flags
final_flags
g_build_arch
g_build_entry
ALLOW

grep -nE "run\(|_cmd: str|_cmd =|cmd = " "$SRC" \
  | grep -v "^[0-9]*: *//" \
  | grep -v "df run\|df probe_cmd" > "$WORK/lines" 2>/dev/null || true

# Strip ASSIGNMENT TARGETS first: `fixed rc: i64 = run(cmd)` names `rc` on a
# matched line, but rc is the exit STATUS coming back, not an input going in.
# Without this the gate reports every result variable and reads as pure noise.
sed 's/^[0-9]*: *\(set\|fixed\) *[a-z_0-9]*\( *: *[a-z_0-9<>]*\)* *=/=/' "$WORK/lines" > "$WORK/lines2" && mv "$WORK/lines2" "$WORK/lines"

sed 's/shq([a-z_0-9]*)//g; s/run_arg_ok([a-z_0-9]*)//g; s/path_arg_ok([a-z_0-9]*)//g; s/link_lib_ok([a-z_0-9]*)//g; s/build_triple_ok([a-z_0-9]*)//g; s/"[^"]*"//g' "$WORK/lines" \
  | grep -oE "\b[a-z_][a-z_0-9]{2,}\b" \
  | sort -u \
  | grep -vE "^(str|i64|void|bool|str_concat|set|fixed|check|fallback|return|int_to_str|fmt|len|probe_cmd|line_eq|sys_os_name|torvik_path_join|torvik_build_dir|home_dir|temp_dir|cmd|run|sys_run|echo|exit|df|oc)$" \
  > "$WORK/names" 2>/dev/null || true

FOUND=0
while IFS= read -r n; do
    [ -z "$n" ] && continue
    # Assembled commands (checked on their own lines) and results coming BACK
    # from run() are not inputs. Naming them by suffix keeps the rule readable
    # and keeps the allow-list for genuine values only.
    case "$n" in
        *_cmd|*cmd|*_rc|rc|*_result|*_fresh|*_compile|have_*) continue ;;
    esac
    grep -qx "$n" "$WORK/allow" && continue
    # NOT "validated somewhere in the file" - that rule is too weak, and it let a
    # deliberately reintroduced bug through: `out` is wrapped in shq() at two
    # sites, so using it RAW at a third still looked validated. The wrapping has
    # to be at the site of use, which is what the sed above already checks by
    # deleting validated calls before the names are collected.
    :
    where=$(grep -n "\b$n\b" "$WORK/lines" | head -1 | cut -d: -f1)
    echo "  $n  (near line $where) reaches a command line with no validator"
    FOUND=$((FOUND+1))
done < "$WORK/names"

if [ "$FOUND" != "0" ]; then
    echo ""
    echo "check_cmdline: $FOUND value(s) reach a command line unvalidated."
    echo ""
    echo "  Wrap paths in shq(); use the validator matching the value's kind for"
    echo "  anything else. If a value is genuinely compiler-constructed, add it to"
    echo "  the allow-list in this script with a note saying why — do not silence"
    echo "  it by reformatting the code."
    exit 1
fi

echo "ok    cmdline: every value reaching a command line is validated"
exit 0
