#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

if [[ ! -f "$ROOT/CMakeLists.txt" ]]; then
  printf 'Jalankan skrip dari salinan proyek Avatar Studio yang lengkap.\n' >&2
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1 ||
   ! command -v dpkg-deb >/dev/null 2>&1; then
  printf 'Skrip ini hanya mendukung Debian/Ubuntu dan turunannya (APT).\n' >&2
  exit 1
fi

if [[ $EUID -eq 0 ]]; then
  SUDO=()
else
  SUDO=(sudo)
fi

printf '%s\n' 'Memasang dependensi build untuk Debian/Ubuntu...'
"${SUDO[@]}" apt-get update
"${SUDO[@]}" apt-get install -y \
  build-essential \
  cmake \
  git \
  curl \
  libgl-dev \
  libx11-dev \
  pkg-config

if ! "${SUDO[@]}" apt-get install -y libsdl2-dev; then
  printf '%s\n' 'libsdl2-dev tidak dapat dipasang; menyiapkan SDL2 lokal sebagai fallback...'
  sh "$ROOT/tools/bangun-sdl2-lokal.sh"
fi

printf '%s\n' 'Mengonfigurasi dan membangun Avatar Studio...'
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

printf '%s\n' 'Membuat dan memasang paket aplikasi...'
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G DEB -B "$BUILD_DIR"
PACKAGE="$BUILD_DIR/avatar-studio_0.1.0_$(dpkg --print-architecture).deb"
if [[ ! -f "$PACKAGE" ]]; then
  printf 'Paket hasil build tidak ditemukan: %s\n' "$PACKAGE" >&2
  exit 1
fi
"${SUDO[@]}" apt-get install -y --reinstall "$PACKAGE"

printf '\nAvatar Studio berhasil dipasang. Jalankan dengan: avatar-studio\n'
