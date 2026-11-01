#pragma once

#include <imgui.h>
#include <SDL.h>

#include "render/CameraTracker.hpp"

#include <string>
#include <vector>

struct Settings;
class App;

// ---------------------------------------------------------------------------
// Gui — seluruh antarmuka berbahasa Indonesia, memakai Dear ImGui.
// Dua konteks ImGui:
//   1. Kontrol : jendela "Pengaturan" berisi semua tombol dan pilihan.
//   2. Overlay : menu klik-kanan kecil di jendela avatar (buka pengaturan/keluar).
//                Hanya dirender saat menu terbuka → tidak membebani render.
// ---------------------------------------------------------------------------
class Gui {
public:
  // Siapkan ImGui untuk jendela kontrol. s = pengaturan (skala UI, dll).
  bool initKontrol(SDL_Window* jendela, SDL_GLContext konteks, Settings& s);

  // Siapkan ImGui untuk menu klik-kanan di jendela avatar.
  // skala = skala UI (1.0, 1.25, atau 1.5) agar gaya konsisten dengan kontrol.
  bool initOverlay(SDL_Window* jendela, SDL_GLContext konteks, float skala);

  // Teruskan semua event SDL ke kedua konteks ImGui (backend menyaring sendiri).
  void prosesEvent(const SDL_Event& e);

  // Minta menu klik-kanan avatar muncul di frame berikutnya.
  void mintaMenuAvatar();

  // true bila overlay harus digambar frame ini (menu terbuka/minta buka).
  bool overlayAktif() const;

  // Bangun + gambar satu frame jendela kontrol (konteks sudah aktif).
  void frameKontrol(App& app);

  // Bangun + gambar satu frame menu avatar (konteks sudah aktif).
  void frameOverlay(App& app);

  void shutdown();

private:
  ImGuiContext* ctxKontrol_ = nullptr;
  ImGuiContext* ctxOverlay_ = nullptr;
  SDL_Window* winKontrol_   = nullptr;
  SDL_Window* winOverlay_   = nullptr;
  ImFont* fontTebal_        = nullptr;
  bool permintaanMenu_ = false; // klik kanan barusan
  bool menuTerbuka_    = false; // popup sedang terbuka
  char obsHost_[256] = "127.0.0.1";
  std::string pesanInfo_; // kabar baik satu baris (mis. "Model dimuat: X.")
  bool mintaJalurManual_ = false; // tampilkan kolom ketik jalur manual
  char obsPassword_[256] = {};

  // Kamera tracker (optional)
  std::vector<PerangkatKamera> perangkatTersedia_;
  int perangkatTerpilih_ = 0;
  bool kameraAktif_ = false;
  bool perangkatSegar_ = false; // daftar sudah di-scan, jangan ulang tiap frame

  // Buka dialog pilih folder (zenity/kdialog bila ada). "" = batal/tak ada.
  std::string pilihFolderModel();
  // Buka dialog pilih berkas .model3.json (zenity/kdialog bila ada). "" = batal/tak ada.
  std::string pilihBerkasModel();
  // Cari model contoh Hiyori bawaan SDK. "" bila tidak ditemukan.
  std::string cariModelContoh();
};
