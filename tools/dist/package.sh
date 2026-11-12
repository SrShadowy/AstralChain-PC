#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="${1:-/workspace}"
OUT_DIR="${2:-/workspace/dist}"
PKG_NAME="AstralChain-PC"
APP_ID="io.github.srshadowy.AstralChainPC"
DEVELOPER_NAME="SrShadowy"
APP_VERSION="${AC_APP_VERSION:-$(tr -d '[:space:]' < "$SRC_ROOT/VERSION")}"
RELEASE_DATE="$(date -u +%Y-%m-%d)"
STAGE_ROOT="/tmp/ac-stage"
STAGE="$STAGE_ROOT/$PKG_NAME"
LAUNCHER_BUILD="/tmp/ac-launcher-build"

rm -rf "$STAGE_ROOT" "$LAUNCHER_BUILD"
mkdir -p "$STAGE/usr/bin" "$STAGE/usr/lib" "$STAGE/usr/include" "$STAGE/usr/plugins" "$STAGE/tools" "$OUT_DIR"

echo "== 1. Building Qt6 Launcher v${APP_VERSION} (generic x86-64, Ubuntu 22.04 baseline) =="
cmake -B "$LAUNCHER_BUILD" -S "$SRC_ROOT/src/launcher" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAC_PORTABLE_BUILD=ON \
    -DAC_APP_VERSION="$APP_VERSION"
cmake --build "$LAUNCHER_BUILD"
install -m 0755 "$LAUNCHER_BUILD/astral_chain_launcher" "$STAGE/usr/bin/astral_chain_launcher"

echo "== 2. Building romfs-build tool (generic x86-64) =="
gcc-12 -O2 -march=x86-64 -mtune=generic \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime" \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime/cpu" \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime/kernel" \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime/build" \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime/input" \
    -I "$SRC_ROOT/third_party/LASRecomp/runtime/hle" \
    "$SRC_ROOT/src/runtime/tools/romfs_tool.c" \
    "$SRC_ROOT/third_party/LASRecomp/runtime/build/romfs_build.c" \
    -o "$STAGE/tools/romfs-build"
chmod 0755 "$STAGE/tools/romfs-build"

echo "== 3. Copying clean source, scripts, and licenses (no configs/dumps/saves) =="
cp -a "$SRC_ROOT/src" "$STAGE/src"
find "$STAGE/src" -type d -name "__pycache__" -prune -exec rm -rf {} +
find "$STAGE/src" -type f -name "*.py[cod]" -delete

mkdir -p "$STAGE/third_party"
for tp in AstralChain-Dynamic-FPS LASRecomp Vulkan-Headers VulkanMemoryAllocator bc7enc imgui; do
    if [ -d "$SRC_ROOT/third_party/$tp" ]; then
        cp -a "$SRC_ROOT/third_party/$tp" "$STAGE/third_party/$tp"
    fi
done
find "$STAGE/third_party" -name ".git*" -prune -exec rm -rf {} +
find "$STAGE/third_party" -type d -name "__pycache__" -prune -exec rm -rf {} +
find "$STAGE/third_party" -type f -name "*.py[cod]" -delete

for f in VERSION play.sh play-diagnostics.sh setup.sh launcher.sh requirements.txt LICENSE THIRD_PARTY_NOTICES.md README.md; do
    cp -a "$SRC_ROOT/$f" "$STAGE/$f"
done
chmod 0755 "$STAGE/play.sh" "$STAGE/play-diagnostics.sh" "$STAGE/setup.sh" "$STAGE/launcher.sh"
chmod 0755 "$STAGE/src/astral_chain/tools/run_guarded.sh"

echo "== 4. Bundling portable SDL3, Python 3.10 + deps, and Qt6 runtime =="
cp -a /opt/sdl3/include/SDL3 "$STAGE/usr/include/SDL3"
cp -a /opt/sdl3/lib/libSDL3.so* "$STAGE/usr/lib/"

cp -a /usr/bin/python3.10 "$STAGE/usr/bin/python3"
chmod 0755 "$STAGE/usr/bin/python3"
cp -a /usr/lib/python3.10 "$STAGE/usr/lib/python3.10"
rm -rf "$STAGE/usr/lib/python3.10/test" "$STAGE/usr/lib/python3.10/idlelib" "$STAGE/usr/lib/python3.10/tkinter" "$STAGE/usr/lib/python3.10/ensurepip"
rm -f "$STAGE/usr/lib/python3.10/sitecustomize.py"
find "$STAGE/usr/lib/python3.10" -type d -name "__pycache__" -prune -exec rm -rf {} +
mkdir -p "$STAGE/usr/lib/python3.10/dist-packages"
cp -a /opt/pydeps/. "$STAGE/usr/lib/python3.10/dist-packages/"
find "$STAGE/usr/lib/python3.10/dist-packages" -type d -name "__pycache__" -prune -exec rm -rf {} +

QT_PLUGINS_SRC=""
for candidate in /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins; do
    if [ -d "$candidate" ]; then
        QT_PLUGINS_SRC="$candidate"
        break
    fi
done
if [ -z "$QT_PLUGINS_SRC" ]; then
    echo "ERROR: Qt6 plugins directory not found." >&2
    exit 1
fi

for group in platforms platformthemes imageformats iconengines xcbglintegrations wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client tls; do
    if [ -d "$QT_PLUGINS_SRC/$group" ]; then
        mkdir -p "$STAGE/usr/plugins/$group"
        cp -a "$QT_PLUGINS_SRC/$group/"*.so "$STAGE/usr/plugins/$group/" 2>/dev/null || true
    fi
done

is_excluded_lib() {
    case "$(basename "$1")" in
        linux-vdso.so.*|ld-linux-x86-64.so.*|\
        libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|libutil.so.*|libresolv.so.*|libnss_*|\
        libstdc++.so.*|libgcc_s.so.*|\
        libvulkan.so.*|libGL.so.*|libGLX.so.*|libOpenGL.so.*|libEGL.so.*|libGLdispatch.so.*|\
        libdrm.so.*|libgbm.so.*|libwayland-client.so.*|libwayland-cursor.so.*|libwayland-egl.so.*|libwayland-server.so.*|\
        libX11.so.*|libX11-xcb.so.*|libxcb.so.*|libXext.so.*|libXfixes.so.*|libXau.so.*|libXdmcp.so.*|\
        libglib-2.0.so.*|libgobject-2.0.so.*|libgthread-2.0.so.*|libgio-2.0.so.*|\
        libdbus-1.so.*|libsystemd.so.*|libudev.so.*|libz.so.*|libfontconfig.so.*|libfreetype.so.*)
            return 0 ;;
        *)
            return 1 ;;
    esac
}

collect_deps() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '/=> \// {print $3}' | while IFS= read -r lib; do
        [ -f "$lib" ] || continue
        is_excluded_lib "$lib" && continue
        local base
        base="$(basename "$lib")"
        if [ ! -e "$STAGE/usr/lib/$base" ]; then
            cp -L "$lib" "$STAGE/usr/lib/$base"
            chmod 0755 "$STAGE/usr/lib/$base"
        fi
    done
}

collect_deps "$STAGE/usr/bin/astral_chain_launcher"
collect_deps "$STAGE/usr/bin/python3"
collect_deps "$STAGE/tools/romfs-build"
for so in "$STAGE/usr/lib/"*.so* "$STAGE/usr/plugins/"*/*.so; do
    [ -f "$so" ] || continue
    collect_deps "$so"
done
while IFS= read -r so; do
    collect_deps "$so"
done < <(find "$STAGE/usr/lib/python3.10" -type f -name "*.so*")
for so in "$STAGE/usr/lib/"*.so*; do
    [ -f "$so" ] || continue
    collect_deps "$so"
done

patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN' "$STAGE/usr/bin/astral_chain_launcher"
patchelf --set-rpath '$ORIGIN/../lib:$ORIGIN' "$STAGE/usr/bin/python3"
for so in "$STAGE/usr/lib/"*.so*; do
    [ -f "$so" ] && [ ! -L "$so" ] || continue
    patchelf --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
done
for so in "$STAGE/usr/plugins/"*/*.so; do
    [ -f "$so" ] || continue
    patchelf --set-rpath '$ORIGIN/../../lib' "$so" 2>/dev/null || true
done
while IFS= read -r so; do
    patchelf --set-rpath '$ORIGIN/../..:$ORIGIN/../../..:$ORIGIN/../../../..' "$so" 2>/dev/null || true
done < <(find "$STAGE/usr/lib/python3.10" -type f -name "*.so*")

echo "== 5. Installing Desktop Entry, AppStream MetaInfo, and hicolor icons =="
mkdir -p "$STAGE/usr/share/applications" "$STAGE/usr/share/metainfo" "$STAGE/usr/share/pixmaps"
mkdir -p "$STAGE/usr/share/icons/hicolor/scalable/apps"
cp -a "$SRC_ROOT/tools/dist/astral-chain-pc.svg" "$STAGE/usr/share/icons/hicolor/scalable/apps/${APP_ID}.svg"

for size in 48 64 128 256 512; do
    ICON_DIR="$STAGE/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$ICON_DIR"
    rsvg-convert -w "$size" -h "$size" "$SRC_ROOT/tools/dist/astral-chain-pc.svg" -o "$ICON_DIR/${APP_ID}.png"
done
cp -a "$STAGE/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png" "$STAGE/usr/share/pixmaps/${APP_ID}.png"

render_template() {
    local src="$1" dst="$2"
    sed -e "s|@VERSION@|${APP_VERSION}|g" \
        -e "s|@RELEASE_DATE@|${RELEASE_DATE}|g" \
        -e "s|@DEVELOPER@|${DEVELOPER_NAME}|g" \
        "$src" > "$dst"
}

render_template "$SRC_ROOT/tools/dist/io.github.srshadowy.AstralChainPC.desktop.in" \
    "$STAGE/usr/share/applications/${APP_ID}.desktop"
render_template "$SRC_ROOT/tools/dist/io.github.srshadowy.AstralChainPC.metainfo.xml.in" \
    "$STAGE/usr/share/metainfo/${APP_ID}.metainfo.xml"
ln -sf "${APP_ID}.metainfo.xml" "$STAGE/usr/share/metainfo/${APP_ID}.appdata.xml"

desktop-file-validate "$STAGE/usr/share/applications/${APP_ID}.desktop"
appstreamcli validate --no-net "$STAGE/usr/share/metainfo/${APP_ID}.metainfo.xml"

echo "== 6. Creating clean versioned .tar.gz archive =="
rm -f "$OUT_DIR/${PKG_NAME}-linux-x86_64.tar.gz" "$OUT_DIR/${PKG_NAME}-linux-x86_64.AppImage"
TAR_OUT="$OUT_DIR/${PKG_NAME}-${APP_VERSION}-linux-x86_64.tar.gz"
tar -C "$STAGE_ROOT" -czf "$TAR_OUT" "$PKG_NAME"

echo "== 7. Creating versioned AppImage =="
cp -a "$SRC_ROOT/tools/dist/AppRun" "$STAGE/AppRun"
chmod 0755 "$STAGE/AppRun"
cp -a "$STAGE/usr/share/applications/${APP_ID}.desktop" "$STAGE/${APP_ID}.desktop"
cp -a "$STAGE/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png" "$STAGE/${APP_ID}.png"
ln -sf "${APP_ID}.png" "$STAGE/.DirIcon"

APPIMAGE_OUT="$OUT_DIR/${PKG_NAME}-${APP_VERSION}-linux-x86_64.AppImage"
ARCH=x86_64 VERSION="$APP_VERSION" /opt/appimagetool/AppRun "$STAGE" "$APPIMAGE_OUT"
chmod 0755 "$APPIMAGE_OUT"

echo "== Done =="
ls -lh "$TAR_OUT" "$APPIMAGE_OUT"
