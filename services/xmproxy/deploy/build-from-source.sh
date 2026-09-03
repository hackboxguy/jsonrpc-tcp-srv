#!/bin/bash
# Build gloox (OpenSSL) and jsonrpc-tcp-srv from source and install both under
# one prefix. Works on Raspberry Pi OS Lite (bookworm), Debian, Ubuntu, Alpine
# (with the equivalent packages) and on a build host for a chroot.
#
#   build-from-source.sh [--prefix DIR] [--src DIR] [--jobs N] [--gloox-version V]
#
# Distribution gloox packages link GnuTLS and cannot authenticate against
# Snikket/Prosody with TLS 1.3 channel binding (see docs/dev-setup.md), so
# gloox is always built here with OpenSSL.
set -euo pipefail
PREFIX=/opt/xmproxy
SRC="$(cd "$(dirname "$0")/../../.." && pwd)"
JOBS="$(nproc 2>/dev/null || echo 2)"
GLOOX_VERSION=1.0.28
GLOOX_SHA256=591bd12c249ede0b50a1ef6b99ac0de8ef9c1ba4fd2e186f97a740215cc5966c # of gloox-1.0.28.tar.bz2; set GLOOX_SHA256=skip for another version
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) shift; PREFIX="$1" ;;
        --src) shift; SRC="$1" ;;
        --jobs) shift; JOBS="$1" ;;
        --gloox-version) shift; GLOOX_VERSION="$1" ;;
        *) echo "unknown option $1"; exit 2 ;;
    esac; shift
done
WORK="${TMPDIR:-/tmp}/xmproxy-build.$$"
mkdir -p "$WORK" "$PREFIX"
trap 'rm -rf "$WORK"' EXIT

echo "== gloox $GLOOX_VERSION with OpenSSL into $PREFIX"
if [ ! -f "$PREFIX/lib/pkgconfig/gloox.pc" ] || ! grep -q "Version: $GLOOX_VERSION" "$PREFIX/lib/pkgconfig/gloox.pc"; then
    cd "$WORK"
    TARBALL="gloox-$GLOOX_VERSION.tar.bz2"
    if [ -f "$SRC/3rd_party/$TARBALL" ]; then
        cp "$SRC/3rd_party/$TARBALL" .
    else
        wget -q "https://camaya.net/download/$TARBALL" || curl -sLO "https://camaya.net/download/$TARBALL"
    fi
    if [ "$GLOOX_SHA256" != skip ]; then
        echo "$GLOOX_SHA256  $TARBALL" | sha256sum -c - > /dev/null || { echo "gloox tarball digest mismatch"; exit 1; }
    fi
    tar xjf "$TARBALL"
    cd "gloox-$GLOOX_VERSION"
    CXXFLAGS="-Os -ffunction-sections -fdata-sections" LDFLAGS="-Wl,--gc-sections" \
        ./configure --prefix="$PREFIX" --with-openssl --without-gnutls \
                    --disable-static --enable-shared --without-examples --without-tests > configure.log
    make -j"$JOBS" > make.log
    make install > install.log
    # gloox's own pkg-config file is not always installed
    if [ ! -f "$PREFIX/lib/pkgconfig/gloox.pc" ]; then
        mkdir -p "$PREFIX/lib/pkgconfig"
        cat > "$PREFIX/lib/pkgconfig/gloox.pc" <<PC
prefix=$PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include
Name: gloox
Description: C++ library for XMPP clients
Version: $GLOOX_VERSION
Libs: -L\${libdir} -lgloox
Cflags: -I\${includedir}
PC
    fi
else
    echo "   already present"
fi

echo "== jsonrpc-tcp-srv from $SRC"
BUILD="$WORK/build"
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}" \
cmake -S "$SRC" -B "$BUILD" -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DCMAKE_INSTALL_RPATH="$PREFIX/lib" -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
      -DAUTO_GIT_VERSION=$( [ -d "$SRC/.git" ] && echo ON || echo OFF ) > "$WORK/cmake.log"
cmake --build "$BUILD" -- install -j"$JOBS" > "$WORK/build.log"
strip "$PREFIX"/bin/xmproxysrv "$PREFIX"/bin/sysmgr "$PREFIX"/bin/xmproxyclt "$PREFIX"/bin/sysmgrclt "$PREFIX"/bin/tcp-json-rpc-client 2>/dev/null || true
echo "== installed:"
ls "$PREFIX/bin"
