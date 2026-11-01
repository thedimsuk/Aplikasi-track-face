#pragma once

#include <string>

// ---------------------------------------------------------------------------
// Pengaturan aplikasi disimpan otomatis di direktori konfigurasi pengguna.
// ---------------------------------------------------------------------------
struct Settings {
  // Versi format berkas pengaturan (untuk migrasi ke depan).
  int versiPengaturan = 1;

  // Performa
  int batasFrame = 30;   // batas frame render avatar (fps), default 30
  int skalaUI    = 100;  // skala antarmuka: 100 / 125 / 150 (persen)
  int zoomAvatar = 100;
  int sensitivitasMikrofon = 35;

  // Koneksi kontrol lokal obs-websocket (password tidak pernah disimpan).
  std::string obsHost = "127.0.0.1";
  int obsPort = 4455;

  // Jalur model terakhir yang berhasil dimuat (auto-muat saat buka aplikasi)
  std::string modelTerakhir;

  // Geometri jendela avatar (untuk OBS)
  int avatarX = 60;
  int avatarY = 60;
  int avatarW = 540;
  int avatarH = 800;

  // Geometri jendela kontrol
  int kontrolX = 640;
  int kontrolY = 70;
  int kontrolW = 560;
  int kontrolH = 700;

  // Latar jendela avatar (bawaan: hijau untuk chroma key OBS)
  int latarR = 0;
  int latarG = 177;
  int latarB = 64;

  // Latar transparan (background tanpa warna, untuk OBS tanpa chroma key)
  bool latarTransparan = false;

  // Posisi offset model dalam piksel (untuk geser-geser model di jendela avatar)
  int modelOfsX = 0;
  int modelOfsY = 0;

  // Kamera tracker (optional — bisa dihidupkan/matikan dari GUI)
  bool kameraAktif = false;
  std::string kameraPerangkat = "/dev/video0";
  int kameraSensitivitas = 50;

  // Perangkat audio mikrofon (kosong = default sistem).
  std::string perangkatMikrofon;

  // Baca pengaturan dari berkas; bila gagal, pakai nilai bawaan.
  bool muat(const char* jalur);

  // Tulis pengaturan ke berkas (folder dibuat bila belum ada).
  bool simpan(const char* jalur) const;

  // Jalur bawaan mengikuti direktori konfigurasi pengguna pada tiap OS.
  static const char* jalurBawaan();
};
