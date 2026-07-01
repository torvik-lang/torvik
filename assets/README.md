# Torvik brand & icon assets

Source art and exported icons for the Torvik toolchain and the `.tv` file type.

## Files

| File | Purpose |
|------|---------|
| `torvik-icon.svg` | App icon master — runestone with the carved red **T** on a dark backdrop |
| `torvik-mark.svg` | The mark on a transparent background (for the README, docs, the web) |
| `torvik-file-icon.svg` | `.tv` document/file icon master |
| `png/torvik-icon-<size>.png` | App icon, 16–1024 px |
| `png/torvik-file-<size>.png` | File icon, 16–1024 px |
| `torvik-mark.png` | Transparent mark, 512 px |
| `torvik.ico` / `torvik-file.ico` | Windows icons (multi-resolution: 16–256 px) |
| `torvik.icns` / `torvik-file.icns` | macOS icons |
| `linux/torvik-mime.xml` | freedesktop MIME definition for `*.tv` (`text/x-torvik`) |

## Design

A standing **runestone** slab in blue-grey granite, carved with a bold **T** painted in
traditional runestone red-ochre — with serifed bar ends and a flared base foot that nod to
**Tiwaz** (ᛏ), the Norse rune for "T". A notched border band frames the stone. The file icon
places the same red **T** on a document page with a `.tv` label.

## The `.tv` file type & icons

### Linux

You don't need to do anything by hand — **the installer registers the `.tv` file type and
icons for you**. The one-line install (`curl -fsSL <…>/install.sh | sh`) registers the
`text/x-torvik` MIME type for `*.tv`, installs the file and app icons into the per-user
`hicolor` icon theme, and refreshes the MIME and icon caches (no root needed). It's idempotent
and versioned via the `icons` field in `VERSION`, so upgrades refresh automatically. After
installing, file managers show the Torvik icon for `.tv` files instead of a generic source
icon.

To register the icons manually (for example into a packaging script of your own), the steps are:

```sh
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
# 1. MIME type
xdg-mime install --mode user assets/linux/torvik-mime.xml
# 2. icons (repeat per size: 16 32 48 64 128 256)
xdg-icon-resource install --mode user --context mimetypes --size 48 assets/png/torvik-file-48.png text-x-torvik
xdg-icon-resource install --mode user --context apps      --size 48 assets/png/torvik-icon-48.png  torvik
# 3. refresh caches
update-mime-database "$DATA/mime"
gtk-update-icon-cache -f "$DATA/icons/hicolor"
```

> **Known cosmetic note — Solus (Budgie + Nemo).** On some Solus setups the Nemo file manager
> does not repaint the `.tv` icon even though everything is installed and resolvable: `gio`
> types the file as `text/x-torvik` and GTK's own icon lookup returns the installed
> `text-x-torvik.png`. This is a Nemo/Budgie display-refresh quirk on Solus, **not** a
> packaging problem — the icons render correctly on GNOME Files, Dolphin (KDE), Thunar (XFCE),
> Caja (MATE), and Nautilus. No action is required; it's cosmetic and isolated to that
> file manager.

### macOS

`torvik.icns` and `torvik-file.icns` are ready to use. Full file-type association (declaring a
UTI for `.tv` and binding `torvik-file.icns`, e.g. via an app bundle's `Info.plist` or a tool
like `duti`) ships with the macOS packaging in **v1.1.0**.

### Windows

`torvik.ico` and `torvik-file.ico` are ready to use. Full file-type association (`HKCR\.tv`
plus a `DefaultIcon` entry pointing at `torvik-file.ico`) ships with the Windows installer in
**v1.1.0**.
