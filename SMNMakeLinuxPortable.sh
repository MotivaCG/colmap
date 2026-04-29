
#!/usr/bin/env bash
set -Eeuo pipefail

# -----------------------------------------------------------------------------
# MakeCOLMAPPortableLinux.sh
#
# Creates a portable COLMAP bundle with Qt libraries/plugins and runtime deps.
#
# Default input binary:
#   /home/victor/Documentos/SMN/software/colmap/git/LinuxBuild/INSTALL/bin/colmap
#
# Default Qt:
#   /home/victor/Qt/6.8.3/gcc_64
#
# Default output:
#   /home/victor/Documentos/SMN/software/colmap/git/COLMAPPortable
#
# Usage:
#   ./MakeCOLMAPPortableLinux.sh
#
# Optional:
#   COLMAP_BIN=/path/to/colmap ./MakeCOLMAPPortableLinux.sh
#   QT_ROOT=/path/to/Qt/6.x.x/gcc_64 ./MakeCOLMAPPortableLinux.sh
#   BUNDLE=/path/to/COLMAPPortable ./MakeCOLMAPPortableLinux.sh
# -----------------------------------------------------------------------------

die() {
    echo ""
    echo "ERROR: $*" >&2
    echo ""
    exit 1
}

warn() {
    echo "WARNING: $*" >&2
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "Missing command: $1"
}

copy_file_deref() {
    local src="$1"
    local dst_dir="$2"

    [[ -f "$src" ]] || return 0
    mkdir -p "$dst_dir"

    # Dereference symlinks so the actual library file is present in the bundle.
    cp -u -L -v "$src" "$dst_dir/" >/dev/null || true
}

copy_glob() {
    local pattern="$1"
    local dst_dir="$2"

    mkdir -p "$dst_dir"

    shopt -s nullglob
    local files=( $pattern )
    shopt -u nullglob

    for f in "${files[@]}"; do
        if [[ -f "$f" || -L "$f" ]]; then
            cp -a -v "$f" "$dst_dir/" >/dev/null || true
        fi
    done
}

is_elf() {
    local f="$1"
    [[ -f "$f" ]] || return 1
    file "$f" | grep -q "ELF"
}

should_skip_library() {
    local lib="$1"
    local base
    base="$(basename "$lib")"

    case "$base" in
        linux-vdso*|ld-linux*|ld-musl*)
            return 0
            ;;
        libcuda.so*|libnvidia-*|libGLX_nvidia.so*|libEGL_nvidia.so*)
            # Do not bundle NVIDIA driver libraries. They must match the target machine driver.
            return 0
            ;;
    esac

    return 1
}

copy_ldd_deps_of() {
    local file="$1"

    is_elf "$file" || return 0

    echo "-- Scanning deps: $file"

    LD_LIBRARY_PATH="$BUNDLE/lib:$QT_ROOT/lib:$COLMAP_INSTALL_ROOT/lib:${LD_LIBRARY_PATH:-}" \
        ldd "$file" 2>/dev/null \
        | awk '
            /=> \// {print $3}
            /^[[:space:]]*\// {print $1}
        ' \
        | sort -u \
        | while read -r lib; do
            [[ -n "$lib" ]] || continue
            [[ -f "$lib" ]] || continue

            if should_skip_library "$lib"; then
                echo "-- Skipping driver/system loader lib: $lib"
                continue
            fi

            copy_file_deref "$lib" "$BUNDLE/lib"
        done
}

patch_rpath() {
    local file="$1"
    local rpath="$2"

    is_elf "$file" || return 0

    if patchelf --print-rpath "$file" >/dev/null 2>&1; then
        patchelf --set-rpath "$rpath" "$file" || true
    fi
}

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------
: "${COLMAP_BIN:=/home/victor/Documentos/SMN/software/colmap/git/LinuxBuild/INSTALL/bin/colmap}"
: "${QT_ROOT:=/home/victor/Qt/6.8.3/gcc_64}"
: "${BUNDLE:=/home/victor/Documentos/SMN/software/colmap/git/COLMAPPortable}"
: "${MAKE_ARCHIVE:=1}"

COLMAP_BIN="$(readlink -f "$COLMAP_BIN")"
COLMAP_INSTALL_ROOT="$(cd "$(dirname "$COLMAP_BIN")/.." && pwd)"

ARCHIVE="${BUNDLE}.tar.gz"

# -----------------------------------------------------------------------------
# Checks
# -----------------------------------------------------------------------------
need_cmd file
need_cmd ldd
need_cmd awk
need_cmd sort
need_cmd find
need_cmd cp
need_cmd mkdir
need_cmd tar
need_cmd patchelf

[[ -x "$COLMAP_BIN" ]] || die "COLMAP binary not found or not executable: $COLMAP_BIN"
[[ -d "$QT_ROOT" ]] || die "QT_ROOT does not exist: $QT_ROOT"
[[ -d "$QT_ROOT/lib" ]] || die "Qt lib directory not found: $QT_ROOT/lib"
[[ -d "$QT_ROOT/plugins" ]] || die "Qt plugins directory not found: $QT_ROOT/plugins"

echo "-- COLMAP_BIN: $COLMAP_BIN"
echo "-- COLMAP_INSTALL_ROOT: $COLMAP_INSTALL_ROOT"
echo "-- QT_ROOT: $QT_ROOT"
echo "-- BUNDLE: $BUNDLE"

# -----------------------------------------------------------------------------
# Create bundle layout
# -----------------------------------------------------------------------------
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/bin"
mkdir -p "$BUNDLE/lib"
mkdir -p "$BUNDLE/plugins"

cp -a -v "$COLMAP_BIN" "$BUNDLE/bin/colmap" >/dev/null
chmod +x "$BUNDLE/bin/colmap"

# Copy COLMAP install libs if present.
if [[ -d "$COLMAP_INSTALL_ROOT/lib" ]]; then
    echo "-- Copying COLMAP install libraries"
    copy_glob "$COLMAP_INSTALL_ROOT/lib/*.so*" "$BUNDLE/lib"
fi

# -----------------------------------------------------------------------------
# Copy Qt libraries
# -----------------------------------------------------------------------------
echo "-- Copying Qt libraries"

copy_glob "$QT_ROOT/lib/libQt6Core.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6Gui.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6Widgets.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6OpenGL.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6OpenGLWidgets.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6DBus.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6XcbQpa.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6WaylandClient.so*" "$BUNDLE/lib"
copy_glob "$QT_ROOT/lib/libQt6WaylandEglClientHwIntegration.so*" "$BUNDLE/lib"

# -----------------------------------------------------------------------------
# Copy Qt plugins
# -----------------------------------------------------------------------------
echo "-- Copying Qt plugins"

for plugin_dir in \
    platforms \
    xcbglintegrations \
    wayland-graphics-integration-client \
    imageformats \
    iconengines \
    platformthemes \
    tls \
    styles \
    generic \
    egldeviceintegrations
do
    if [[ -d "$QT_ROOT/plugins/$plugin_dir" ]]; then
        mkdir -p "$BUNDLE/plugins/$plugin_dir"
        cp -a -v "$QT_ROOT/plugins/$plugin_dir/"* "$BUNDLE/plugins/$plugin_dir/" 2>/dev/null || true
    fi
done

# -----------------------------------------------------------------------------
# qt.conf
# -----------------------------------------------------------------------------
cat > "$BUNDLE/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Libraries=lib
Plugins=plugins
EOF

# -----------------------------------------------------------------------------
# Copy runtime dependencies.
#
# Do several passes because copied libraries/plugins have their own dependencies.
# -----------------------------------------------------------------------------
echo "-- Copying runtime dependencies"

copy_ldd_deps_of "$BUNDLE/bin/colmap"

for pass in 1 2 3; do
    echo "-- Dependency pass $pass"

    while IFS= read -r elf; do
        copy_ldd_deps_of "$elf"
    done < <(
        find "$BUNDLE/lib" "$BUNDLE/plugins" -type f \
            \( -name "*.so" -o -name "*.so.*" \) \
            2>/dev/null
    )
done

# -----------------------------------------------------------------------------
# Patch RPATHs
# -----------------------------------------------------------------------------
echo "-- Patching RPATHs"

patch_rpath "$BUNDLE/bin/colmap" '$ORIGIN/../lib'

while IFS= read -r lib; do
    patch_rpath "$lib" '$ORIGIN'
done < <(
    find "$BUNDLE/lib" -type f \
        \( -name "*.so" -o -name "*.so.*" \) \
        2>/dev/null
)

while IFS= read -r plugin; do
    patch_rpath "$plugin" '$ORIGIN/../../lib'
done < <(
    find "$BUNDLE/plugins" -type f \
        \( -name "*.so" -o -name "*.so.*" \) \
        2>/dev/null
)

# -----------------------------------------------------------------------------
# Launcher
# -----------------------------------------------------------------------------
cat > "$BUNDLE/bin/run_colmap.sh" <<'EOF'
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

export LD_LIBRARY_PATH="$ROOT_DIR/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$ROOT_DIR/plugins:${QT_PLUGIN_PATH:-}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$ROOT_DIR/plugins/platforms"

exec "$SCRIPT_DIR/colmap" "$@"
EOF

chmod +x "$BUNDLE/bin/run_colmap.sh"

# -----------------------------------------------------------------------------
# Diagnostics
# -----------------------------------------------------------------------------
echo ""
echo "-- Checking unresolved dependencies"
if ldd "$BUNDLE/bin/colmap" | grep -q "not found"; then
    warn "Some dependencies are still missing:"
    ldd "$BUNDLE/bin/colmap" | grep "not found" || true
else
    echo "-- OK: no missing direct dependencies for colmap"
fi

if [[ -f "$BUNDLE/plugins/platforms/libqxcb.so" ]]; then
    if ldd "$BUNDLE/plugins/platforms/libqxcb.so" | grep -q "not found"; then
        warn "Some Qt xcb plugin dependencies are still missing:"
        ldd "$BUNDLE/plugins/platforms/libqxcb.so" | grep "not found" || true
    else
        echo "-- OK: no missing direct dependencies for Qt xcb plugin"
    fi
fi

# -----------------------------------------------------------------------------
# Archive
# -----------------------------------------------------------------------------
if [[ "$MAKE_ARCHIVE" == "1" ]]; then
    echo "-- Creating archive: $ARCHIVE"
    rm -f "$ARCHIVE"
    tar -C "$(dirname "$BUNDLE")" -czf "$ARCHIVE" "$(basename "$BUNDLE")"
fi

echo ""
echo "-- Portable COLMAP bundle created:"
echo "   $BUNDLE"
echo ""
echo "-- Run with:"
echo "   $BUNDLE/bin/run_colmap.sh --help"
echo "   $BUNDLE/bin/run_colmap.sh gui"
echo ""
if [[ "$MAKE_ARCHIVE" == "1" ]]; then
    echo "-- Archive:"
    echo "   $ARCHIVE"
    echo ""
fi

echo "-- Note:"
echo "   NVIDIA driver libraries are intentionally not bundled."
echo "   The target machine still needs a compatible NVIDIA driver if CUDA/OpenGL GPU features are used."