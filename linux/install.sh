#!/usr/bin/env sh
# install.sh - Torvik installer:
#   curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/linux/install.sh | sh
set -e
INSTALL_DIR="$HOME/.torvik"; BIN_DIR="$INSTALL_DIR/bin"; LIB_DIR="$INSTALL_DIR/lib"
ORG="https://github.com/torvik-lang/torvik"
RAW="https://raw.githubusercontent.com/torvik-lang/torvik/main"
fetch() { if command -v curl >/dev/null 2>&1; then curl -fsSL "$1"; else wget -qO- "$1"; fi; }
dl() { if command -v curl >/dev/null 2>&1; then curl -fsSL "$1" -o "$2"; else wget -qO "$2" "$1"; fi; }

# --- Torvik .tv file-type + icon registration (Linux, freedesktop, idempotent) ---
# $1 = assets dir (with torvik-mime.xml [optionally under linux/] and the icon PNGs
#      [optionally under png/]); $2 = version, used for the skip-if-current stamp.
# macOS/Windows file-type association ships with their installers in v1.1.0.
torvik_register_icons() {
    _adir="$1"; _ver="$2"
    [ "$(uname -s 2>/dev/null)" = "Linux" ] || return 0
    _vfile="$_adir/../VERSION"
    _iv="$(grep -E '^[[:space:]]*icons[[:space:]]*=' "$_vfile" 2>/dev/null | head -1 | sed 's/^[^=]*=[[:space:]]*//' | tr -d '[:space:]')"
    [ -n "${_iv:-}" ] && _ver="$_iv"
    _stamp="$HOME/.torvik/.icons-version"
    if [ "${TORVIK_FORCE_ICONS:-0}" != "1" ] && [ -f "$_stamp" ] && \
       [ "$(cat "$_stamp" 2>/dev/null)" = "$_ver" ]; then
        echo "  .tv icons already current (v$_ver) - skipping"; return 0
    fi
    _mime=""
    if   [ -f "$_adir/linux/torvik-mime.xml" ]; then _mime="$_adir/linux/torvik-mime.xml"
    elif [ -f "$_adir/torvik-mime.xml" ];       then _mime="$_adir/torvik-mime.xml"; fi
    _png=""
    if   [ -f "$_adir/png/torvik-file-48.png" ]; then _png="$_adir/png"
    elif [ -f "$_adir/torvik-file-48.png" ];     then _png="$_adir"; fi
    if [ -z "$_mime" ] || [ -z "$_png" ]; then
        echo "  (icon assets not found; skipping icon registration)"; return 0
    fi
    _data="${XDG_DATA_HOME:-$HOME/.local/share}"
    if command -v xdg-mime >/dev/null 2>&1; then
        xdg-mime install --mode user --novendor "$_mime" 2>/dev/null \
            || xdg-mime install --mode user "$_mime" 2>/dev/null || true
    else
        mkdir -p "$_data/mime/packages"; cp "$_mime" "$_data/mime/packages/torvik-mime.xml"
    fi
    _usexdg=0; command -v xdg-icon-resource >/dev/null 2>&1 && _usexdg=1
    for s in 16 32 48 64 128 256; do
        [ -f "$_png/torvik-file-${s}.png" ] || continue
        if [ "$_usexdg" -eq 1 ]; then
            xdg-icon-resource install --mode user --noupdate --context mimetypes --size "$s" \
                "$_png/torvik-file-${s}.png" text-x-torvik 2>/dev/null || true
            [ -f "$_png/torvik-icon-${s}.png" ] && xdg-icon-resource install --mode user --noupdate \
                --context apps --size "$s" "$_png/torvik-icon-${s}.png" torvik 2>/dev/null || true
        else
            mkdir -p "$_data/icons/hicolor/${s}x${s}/mimetypes" "$_data/icons/hicolor/${s}x${s}/apps"
            cp "$_png/torvik-file-${s}.png" "$_data/icons/hicolor/${s}x${s}/mimetypes/text-x-torvik.png"
            [ -f "$_png/torvik-icon-${s}.png" ] && cp "$_png/torvik-icon-${s}.png" "$_data/icons/hicolor/${s}x${s}/apps/torvik.png"
        fi
    done
    # GTK ignores a base icon dir with no index.theme, so give the user hicolor tree one
    # (copied from the system theme so the directory list stays complete), then build its cache.
    _hic="$_data/icons/hicolor"
    if [ ! -f "$_hic/index.theme" ]; then
        mkdir -p "$_hic"
        if [ -f /usr/share/icons/hicolor/index.theme ]; then
            cp /usr/share/icons/hicolor/index.theme "$_hic/index.theme" 2>/dev/null || true
        else
            {
                printf '[Icon Theme]\nName=Hicolor\nComment=Fallback theme\nDirectories=16x16/mimetypes,16x16/apps,32x32/mimetypes,32x32/apps,48x48/mimetypes,48x48/apps,64x64/mimetypes,64x64/apps,128x128/mimetypes,128x128/apps,256x256/mimetypes,256x256/apps\n\n'
                for s in 16 32 48 64 128 256; do
                    printf '[%sx%s/mimetypes]\nSize=%s\nContext=MimeTypes\nType=Threshold\n\n' "$s" "$s" "$s"
                    printf '[%sx%s/apps]\nSize=%s\nContext=Applications\nType=Threshold\n\n' "$s" "$s" "$s"
                done
            } > "$_hic/index.theme" 2>/dev/null || true
        fi
    fi
    [ "$_usexdg" -eq 1 ] && { xdg-icon-resource forceupdate --mode user 2>/dev/null || true; }
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f "$_hic" >/dev/null 2>&1 || true
    command -v update-mime-database  >/dev/null 2>&1 && update-mime-database "$_data/mime" >/dev/null 2>&1 || true
    mkdir -p "$HOME/.torvik"; printf '%s\n' "$_ver" > "$_stamp"
    echo "  registered the .tv file type & icons (v$_ver)"
}

OS=$(uname -s | tr '[:upper:]' '[:lower:]'); ARCH=$(uname -m)
case "$ARCH" in x86_64|amd64) ARCH=x86_64;; aarch64|arm64) ARCH=aarch64;; *) echo "error: unsupported arch $ARCH"; exit 1;; esac
case "$OS" in
  linux) ;;
  darwin)
    echo "Torvik doesn't have official macOS builds yet - macOS support is planned for v1.2.0."
    echo ""
    echo "There's no prebuilt torvc/rune for macOS to install right now. If you'd like to try"
    echo "it early, the toolchain is written to be macOS-compatible and can be built from source"
    echo "(you'll need clang, via 'xcode-select --install'), though this is untested and unsupported:"
    echo ""
    echo "  git clone https://github.com/torvik-lang/torvik"
    echo "  # then follow docs/GUIDE.md to bootstrap the compiler"
    echo ""
    echo "Otherwise, watch the repo for the v1.2.0 release, which brings tested macOS binaries and"
    echo "a proper installer. You can also run Torvik today on Linux (including in a Linux VM/container)."
    exit 1 ;;
  mingw*|msys*|cygwin*)
    echo "It looks like you're on Windows (running under $OS)."
    echo "Use the Windows installer instead - from PowerShell, run:"
    echo ""
    echo "  iwr -useb https://raw.githubusercontent.com/torvik-lang/torvik/main/windows/install.ps1 | iex"
    echo ""
    echo "(That installs torvc.exe and rune.exe under %USERPROFILE%\\.torvik.)"
    exit 1 ;;
  *) echo "error: unsupported OS $OS"; exit 1 ;;
esac
command -v clang >/dev/null 2>&1 || { echo "error: clang required (Linux: sudo apt install clang | Solus: sudo eopkg install clang)"; exit 1; }

# --- version selection --------------------------------------------------------
# Default: the latest version, read from the repo's VERSION on the main branch.
# Override: set TORVIK_VERSION to pin a release. It accepts a full tag (v1.0.1),
# or a partial version (v1, v1.0) which resolves to the newest matching release
# via the GitHub API. `rune update vX` sets this for you.
API="https://api.github.com/repos/torvik-lang/torvik/releases"

if [ -n "$TORVIK_VERSION" ]; then
    REQ="$TORVIK_VERSION"
    case "$REQ" in v*) : ;; *) REQ="v$REQ" ;; esac
    # Fetch the release list once so we can tell "version not found" apart from
    # "couldn't reach GitHub" (network down / API rate-limited).
    _releases="$(fetch "$API" 2>/dev/null)"
    if [ -z "$_releases" ]; then
        echo "error: could not reach the GitHub release list (network issue or API rate limit)."
        echo "  Check your connection and try again, or install the latest with a bare 'rune update'."
        exit 1
    fi
    TAG="$(printf '%s\n' "$_releases" \
      | grep -E '"tag_name"' \
      | sed -E 's/.*"tag_name"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/' \
      | awk -v r="$REQ" '$0==r || index($0, r".")==1' \
      | sed 's/^v//' | sort -t. -k1,1n -k2,2n -k3,3n | tail -1 | sed 's/^/v/')"
    if [ -z "$TAG" ]; then
        echo "error: no Torvik release matches '$TORVIK_VERSION'."
        echo "  See the available versions at https://github.com/torvik-lang/torvik/releases"
        exit 1
    fi
    V="${TAG#v}"
    echo "Installing the requested Torvik $TAG ($OS/$ARCH)..."
else
    V="$(fetch "$RAW/VERSION" | grep -E '^[[:space:]]*torvik' | head -1 | sed 's/^[^=]*=[[:space:]]*//')"
    [ -n "$V" ] || V="1.0.0"
    echo "Installing Torvik v$V ($OS/$ARCH)..."
fi
REL="$ORG/releases/download/v$V"
# Raw-content base for source/runtime/VERSION/stdlib/assets. When a specific
# version is pinned, pull these from that version's TAG so everything matches the
# binaries; a bare (latest) install tracks main.
if [ -n "$TORVIK_VERSION" ]; then
    RAW_REF="https://raw.githubusercontent.com/torvik-lang/torvik/v$V"
else
    RAW_REF="$RAW"
fi

# Non-blocking heads-up if replacing an older MAJOR install (see the note in
# install.ps1). The `rune update` command does the real confirm-before-major-bump;
# piping a script to sh consumes stdin, so we don't prompt here.
_existing_vf="$LIB_DIR/VERSION"
if [ -f "$_existing_vf" ]; then
    _exver="$(grep -E '^[[:space:]]*torvik' "$_existing_vf" 2>/dev/null | head -1 | sed 's/^[^=]*=[[:space:]]*//' | tr -d '[:space:]')"
    if [ -n "$_exver" ]; then
        _cur_major="${_exver%%.*}"
        _new_major="${V%%.*}"
        if [ "$_cur_major" != "$_new_major" ]; then
            echo ""
            echo "note: this replaces Torvik v$_exver with a new MAJOR version (v$V)."
            echo "      Major versions may include breaking changes - see the changelog:"
            echo "      https://github.com/torvik-lang/torvik/blob/main/CHANGELOG.md"
            echo "      To stay on v$_cur_major.x instead, install a pinned version (e.g. rune update v$_cur_major)."
            echo ""
        fi
    fi
fi
mkdir -p "$BIN_DIR" "$LIB_DIR" "$INSTALL_DIR/cache" "$INSTALL_DIR/runes"
# Verify the binaries exist for this release before overwriting anything.
if ! dl "$REL/torvc-$OS-$ARCH" "$BIN_DIR/torvc.new" 2>/dev/null; then
    echo "error: Torvik v$V has no $OS/$ARCH build (could not download torvc)."
    echo "  Pick another version from https://github.com/torvik-lang/torvik/releases"
    rm -f "$BIN_DIR/torvc.new"
    exit 1
fi
dl "$REL/rune-$OS-$ARCH" "$BIN_DIR/rune.new" || { echo "error: could not download rune for v$V."; rm -f "$BIN_DIR/torvc.new" "$BIN_DIR/rune.new"; exit 1; }
mv "$BIN_DIR/torvc.new" "$BIN_DIR/torvc"
mv "$BIN_DIR/rune.new"  "$BIN_DIR/rune"
chmod +x "$BIN_DIR/torvc" "$BIN_DIR/rune"
for a in torvik_lexer.tv torvik_parser.tv torvik_codegen.tv diag.tv std.tv; do dl "$RAW_REF/src/$a" "$LIB_DIR/$a"; done
dl "$RAW_REF/runtime/torvik_runtime.c" "$LIB_DIR/torvik_runtime.c"
dl "$RAW_REF/VERSION" "$LIB_DIR/VERSION"
mkdir -p "$LIB_DIR/std"
for a in math strings list; do dl "$RAW_REF/src/std/$a.tv" "$LIB_DIR/std/$a.tv"; done

# --- icons + .tv file type (Linux only; macOS/Windows association: v1.1.0) ---
if [ "$OS" = "linux" ]; then
    echo "Registering the .tv file type and icons..."
    ICON_DIR="$INSTALL_DIR/icons"; mkdir -p "$ICON_DIR/png"
    dl "$RAW_REF/assets/linux/torvik-mime.xml" "$ICON_DIR/torvik-mime.xml" 2>/dev/null || true
    for s in 16 32 48 64 128 256; do
        dl "$RAW_REF/assets/png/torvik-file-$s.png" "$ICON_DIR/png/torvik-file-$s.png" 2>/dev/null || true
        dl "$RAW_REF/assets/png/torvik-icon-$s.png" "$ICON_DIR/png/torvik-icon-$s.png" 2>/dev/null || true
    done
    torvik_register_icons "$ICON_DIR" "$V"
fi

[ -f "$INSTALL_DIR/config.rune" ] || cat > "$INSTALL_DIR/config.rune" <<'CONFIG'
# ~/.torvik/config.rune - Torvik user configuration
[user]
author = ""

[settings]
cache  = "true"
CONFIG
PATH_LINE='export PATH="$HOME/.torvik/bin:$PATH"'
add_to_rc() { [ -f "$1" ] || touch "$1"; grep -q ".torvik/bin" "$1" 2>/dev/null || printf '\n# Torvik\n%s\n' "$PATH_LINE" >> "$1"; }
case "${SHELL##*/}" in
  zsh)  add_to_rc "$HOME/.zshrc"; add_to_rc "$HOME/.profile" ;;
  fish) mkdir -p "$HOME/.config/fish"; grep -q ".torvik" "$HOME/.config/fish/config.fish" 2>/dev/null || printf '\n# Torvik\nset -gx PATH $HOME/.torvik/bin $PATH\n' >> "$HOME/.config/fish/config.fish" ;;
  *)    add_to_rc "$HOME/.bashrc"; add_to_rc "$HOME/.profile" ;;
esac
echo ""
echo "Torvik v$V installed."
echo ">>> Activate:  . ~/.bashrc   (or open a new terminal), then:  rune --version"

# One-shot installer: from here on Rune manages Torvik (upgrades and removal). If this script
# was run from a downloaded file, remove it so nothing lingers. When piped (curl ... | sh) the
# script never touches disk, so $0 isn't our file and this is simply skipped. Must stay last.
case "$0" in
  *install.sh)
    if [ -f "$0" ]; then rm -f -- "$0" 2>/dev/null || true; echo "(removed the one-time installer)"; fi
    ;;
esac
