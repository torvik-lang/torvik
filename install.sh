#!/usr/bin/env sh
# install.sh - Torvik installer:
#   curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/install.sh | sh
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
        echo "  .tv icons already current (v$_ver) — skipping"; return 0
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
case "$OS" in linux|darwin) ;; mingw*|msys*|cygwin*) echo "error: Windows is not yet supported (planned for a later release)."; exit 1;; *) echo "error: unsupported OS $OS"; exit 1;; esac
command -v clang >/dev/null 2>&1 || { echo "error: clang required (Linux: sudo apt install clang | Solus: sudo eopkg install clang | macOS: xcode-select --install)"; exit 1; }
V="$(fetch "$RAW/VERSION" | grep -E '^[[:space:]]*torvik' | head -1 | sed 's/^[^=]*=[[:space:]]*//')"
[ -n "$V" ] || V="1.0.0"
REL="$ORG/releases/download/v$V"
echo "Installing Torvik v$V ($OS/$ARCH)..."
mkdir -p "$BIN_DIR" "$LIB_DIR" "$INSTALL_DIR/cache" "$INSTALL_DIR/runes"
dl "$REL/torvc-$OS-$ARCH" "$BIN_DIR/torvc"
dl "$REL/rune-$OS-$ARCH"  "$BIN_DIR/rune"
chmod +x "$BIN_DIR/torvc" "$BIN_DIR/rune"
for a in torvik_lexer.tv torvik_parser.tv torvik_codegen.tv diag.tv std.tv; do dl "$RAW/src/$a" "$LIB_DIR/$a"; done
dl "$RAW/runtime/torvik_runtime.c" "$LIB_DIR/torvik_runtime.c"
dl "$RAW/VERSION" "$LIB_DIR/VERSION"
mkdir -p "$LIB_DIR/std"
for a in math strings list; do dl "$RAW/src/std/$a.tv" "$LIB_DIR/std/$a.tv"; done

# --- icons + .tv file type (Linux only; macOS/Windows association: v1.1.0) ---
if [ "$OS" = "linux" ]; then
    echo "Registering the .tv file type and icons..."
    ICON_DIR="$INSTALL_DIR/icons"; mkdir -p "$ICON_DIR/png"
    dl "$RAW/assets/linux/torvik-mime.xml" "$ICON_DIR/torvik-mime.xml" 2>/dev/null || true
    for s in 16 32 48 64 128 256; do
        dl "$RAW/assets/png/torvik-file-$s.png" "$ICON_DIR/png/torvik-file-$s.png" 2>/dev/null || true
        dl "$RAW/assets/png/torvik-icon-$s.png" "$ICON_DIR/png/torvik-icon-$s.png" 2>/dev/null || true
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
