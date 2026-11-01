#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>

#include "core/Settings.hpp"
#include "obs/ObsWebSocket.hpp"
#include "render/AudioMouthTracker.hpp"
#include "render/CameraTracker.hpp"

class AvatarRenderer;
class Gui;

// ---------------------------------------------------------------------------
// App — pemilik dua jendela (avatar + kontrol) dan loop utama.
//   * Jendela avatar : bersih, hanya model/latar, untuk ditangkap OBS.
//   * Jendela kontrol: berisi ImGui; ditutup = disembunyikan, avatar ganggu.
//   * Render dibatasi fps (default 30), tidak busy loop, jendela tersembunyi
//     tidak dirender sama sekali.
// ---------------------------------------------------------------------------
class App {
public:
  App() = default;
  ~App();

  // Siapkan SDL, jendela, GL, GUI. false = ada galat (pesan sudah ditampilkan).
  bool init();

  // Jalankan loop utama sampai pengguna keluar. Kembalikan kode keluar.
  int run();

  // Dipanggil dari GUI/menu avatar:
  void mintaBukaJendelaKontrol();
  void mintaKeluar();
  bool muatModel(const std::string& jalur);
  bool mulaiMikrofon();
  void berhentiMikrofon();
  void aturSensitivitasMikrofon(int sensitivitas);
  void aturPerangkatMikrofon(const std::string& namaPerangkat);
  std::string perangkatMikrofon() const { return audioMouth_.namaPerangkat(); }

  // --- Kamera tracker (optional) ------------------------------------------
  bool mulaiKamera(const std::string& perangkat);
  void berhentiKamera();
  bool kameraAktif() const { return kameraTracker_.aktif(); }
  std::string statusKamera() const { return kameraTracker_.status(); }
  DataKamera dataKamera() const { return kameraTracker_.dataTerbaru(); }

  void aturWarnaLatar(int r, int g, int b);
  void aturLatarTransparan(bool transparan);
  void aturPosisiModel(float x, float y);
  void aturZoomAvatar(int persen);
  bool sambungObs(const std::string& password);
  void putusObs();
  bool obsTerhubung() const { return obs_.aktif(); }
  std::string statusObs() const { return obs_.status(); }
  std::string sceneObs() const { return obs_.sceneAktif(); }
  bool mikrofonAktif() const { return audioMouth_.aktif(); }
  float tingkatSuara() const { return audioMouth_.tingkatSuara(); }
  float levelInputMikrofon() const { return audioMouth_.levelInput(); }
  const std::string& statusMikrofon() const { return audioMouth_.status(); }
  bool modelAktif() const;
  std::string namaModel() const;
  std::string statusModel() const;

  Settings& pengaturan() { return settings_; }
  double fpsTerkini() const { return fps_; }

private:
  void tanganiEvent();
  void renderAvatar();
  void renderKontrol();
  void simpanGeometriJendela();
  void bersihkan();

  static void tampilkanGalat(const char* judul, const char* isi,
                             SDL_Window* induk = nullptr);
  static bool jendelaTerlihat(SDL_Window* w);

  SDL_Window*   winAvatar_   = nullptr;
  SDL_Window*   winKontrol_  = nullptr;
  SDL_GLContext ctxAvatar_   = nullptr;
  SDL_GLContext ctxKontrol_  = nullptr;

  Settings        settings_;
  AudioMouthTracker audioMouth_;
  CameraTracker   kameraTracker_;
  ObsWebSocket    obs_;
  Gui*            gui_      = nullptr;
  AvatarRenderer* renderer_ = nullptr;
  std::string     statusModel_ = "Seret folder model Live2D ke jendela avatar.";

  bool berjalan_   = true;
  bool sudahBeres_ = false;

  // Pelacakan drag model (klik kiri + geser di jendela avatar)
  bool   dragAktif_     = false;
  int    dragMouseX_    = 0;
  int    dragMouseY_    = 0;

  // Pengukur fps (diperbarui tiap setengah detik)
  double      fps_ = 0.0;
  std::uint64_t fpsHitung_ = 0;
  std::uint64_t fpsMulai_  = 0;
  std::uint64_t frameSebelumnya_ = 0;
};
