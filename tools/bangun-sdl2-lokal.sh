#!/bin/sh
# ---------------------------------------------------------------------------
# bangun-sdl2-lokal.sh — bangun SDL2 resmi (statis) di DALAM proyek.
#
# Dipakai bila paket libsdl2-dev tidak bisa dipasang secara normal
# (mis. MASDA OS: libpulse0 versi nosystemd2 bentrok dengan libpulse-dev).
# Tidak menyentuh paket sistem, tidak mengubah linker global, tidak memakai
# direktori sementara. Hasil: third_party/sdl2-local (header + library statis
# + metadata CMake/pkg-config resmi dari hasil build SDL2 itu sendiri).
#
# Pemakaian:  sh tools/bangun-sdl2-lokal.sh
# Lalu:       cmake -S . -B build && cmake --build build
# ---------------------------------------------------------------------------
set -eu

VER="2.32.4"                                   # harus cocok dengan libsdl2-2.0-0 terpasang
SHA256="f15b478253e1ff6dac62257ded225ff4e7d0c5230204ac3450f1144ee806f934"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="$ROOT/third_party"
TARBALL="$TP/SDL2-$VER.tar.gz"
SRC="$TP/SDL2-$VER"
BUILD="$TP/sdl2-build"
PREFIX="$TP/sdl2-local"

mkdir -p "$TP"

# 1) Unduh sumber resmi bila arsip belum ada (libsdl.org, salinan GitHub cadangan).
if [ ! -f "$TARBALL" ]; then
  echo ">> Mengunduh SDL2 $VER dari situs resmi.libsdl.org ..."
  curl -fL -o "$TARBALL" "https://libsdl.org/release/SDL2-$VER.tar.gz" || {
    echo ">> Gagal dari .libsdl.org, mencoba GitHub resmi libsdl-org ..."
    curl -fL -o "$TARBALL" \
      "https://github.com/libsdl-org/SDL/releases/download/release-$VER/SDL2-$VER.tar.gz"
  }
fi

# 2) Verifikasi integritas (checksum dikunci agar reproducible).
echo "$SHA256  $TARBALL" | sha256sum -c -

# 3) Ekstrak bila belum.
if [ ! -d "$SRC" ]; then
  echo ">> Mengekstrak sumber SDL2 ..."
  tar -xzf "$TARBALL" -C "$TP"
fi

# 4) Bangun statis + instal ke prefix lokal (shared dimatikan agar aplikasi
#    tidak perlu rpath/LD_LIBRARY_PATH khusus).
if [ ! -f "$BUILD/CMakeCache.txt" ]; then
  echo ">> Konfigurasi SDL2 ..."
  cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED=OFF \
    -DSDL_STATIC=ON \
    -DSDL_TEST=OFF \
    -DSDL_TESTS=OFF
fi

echo ">> Kompilasi SDL2 ..."
cmake --build "$BUILD" -j"$(nproc)"
echo ">> Instal ke $PREFIX ..."
cmake --install "$BUILD" >/dev/null

echo ">> Selesai. SDL2 lokal siap di: $PREFIX"
echo ">> Konfigurasi proyek akan memakainya secara otomatis."
