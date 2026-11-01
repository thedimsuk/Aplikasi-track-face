#include "core/Settings.hpp"

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

// Baca satu baris "kunci=nilai" nilai, atau string kosong.
std::string nilaiDari(const std::string& baris) {
  const auto tanda = baris.find('=');
  if (tanda == std::string::npos) return {};
  return baris.substr(tanda + 1);
}

std::string kunciDari(const std::string& baris) {
  const auto tanda = baris.find('=');
  if (tanda == std::string::npos) return {};
  return baris.substr(0, tanda);
}

// Konversi aman: kalau bukan angka, biarkan nilai lama.
void bacaInt(const std::string& teks, int& tujuan) {
  try { tujuan = std::stoi(teks); } catch (...) { /* abaikan, pakai nilai lama */ }
}

// Jaga agar nilai tetap masuk akal walau berkas diubah tangan.
int batasi(int v, int min, int max) {
  return v < min ? min : (v > max ? max : v);
}

} // namespace

const char* Settings::jalurBawaan() {
  static std::string jalur;
  if (jalur.empty()) {
#ifdef _WIN32
    if (char* preferensi = SDL_GetPrefPath("Avatar Studio", "Avatar Studio")) {
      jalur = std::string(preferensi) + "pengaturan.ini";
      SDL_free(preferensi);
    } else {
      const char* appData = std::getenv("APPDATA");
      jalur = std::string(appData ? appData : ".") +
              "/Avatar Studio/pengaturan.ini";
    }
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    const char* rumah = std::getenv("HOME");
    if (xdgConfig && *xdgConfig)
      jalur = std::string(xdgConfig) + "/avatar-studio/pengaturan.ini";
    else
      jalur = std::string(rumah ? rumah : ".") +
              "/.config/avatar-vtuber/pengaturan.ini";
#endif
  }
  return jalur.c_str();

}
bool Settings::muat(const char* jalur) {
  std::ifstream berkas(fs::u8path(jalur));
  if (!berkas) return false;

  std::string baris;
  while (std::getline(berkas, baris)) {
    if (baris.empty() || baris[0] == '#') continue;
    const std::string k = kunciDari(baris);
    const std::string v = nilaiDari(baris);
    if (k == "batas_frame")            bacaInt(v, batasFrame);
    else if (k == "skala_ui")          bacaInt(v, skalaUI);
    else if (k == "zoom_avatar")       bacaInt(v, zoomAvatar);
    else if (k == "sensitivitas_mikrofon") bacaInt(v, sensitivitasMikrofon);
    else if (k == "obs_host")          obsHost = v;
    else if (k == "obs_port")          bacaInt(v, obsPort);
    else if (k == "model_terakhir")    modelTerakhir = v;
    else if (k == "avatar_x")          bacaInt(v, avatarX);
    else if (k == "avatar_y")          bacaInt(v, avatarY);
    else if (k == "avatar_w")          bacaInt(v, avatarW);
    else if (k == "avatar_h")          bacaInt(v, avatarH);
    else if (k == "kontrol_x")         bacaInt(v, kontrolX);
    else if (k == "kontrol_y")         bacaInt(v, kontrolY);
    else if (k == "kontrol_w")         bacaInt(v, kontrolW);
    else if (k == "kontrol_h")         bacaInt(v, kontrolH);
    else if (k == "latar_r")           bacaInt(v, latarR);
    else if (k == "latar_g")           bacaInt(v, latarG);
    else if (k == "latar_b")           bacaInt(v, latarB);
    else if (k == "latar_transparan") {
      int tmp = 0;
      bacaInt(v, tmp);
      latarTransparan = tmp != 0;
    }
    else if (k == "model_ofs_x")         bacaInt(v, modelOfsX);
    else if (k == "model_ofs_y")         bacaInt(v, modelOfsY);
    else if (k == "kamera_aktif") {
      int tmp = 0;
      bacaInt(v, tmp);
      kameraAktif = tmp != 0;
    }
    else if (k == "kamera_perangkat")    kameraPerangkat = v;
    else if (k == "kamera_sensitivitas") bacaInt(v, kameraSensitivitas);
    else if (k == "perangkat_mikrofon")  perangkatMikrofon = v;
    else if (k == "versi_pengaturan")    bacaInt(v, versiPengaturan);
  }

  // Migrasi versi pengaturan ke depan.
  if (versiPengaturan < 1) {
    // Versi 0 -> 1: inisialisasi nilai bawaan baru (bila ada di depan).
    versiPengaturan = 1;
  }
  versiPengaturan = 1; // selalu sinkronkan ke versi saat ini

  // Validasi ringan agar hasil edit manual tetap wajar.
  batasFrame  = batasi(batasFrame, 5, 240);
  zoomAvatar = batasi(zoomAvatar, 50, 300);
  sensitivitasMikrofon = batasi(sensitivitasMikrofon, 1, 100);
  obsPort = batasi(obsPort, 1, 65535);
  if (skalaUI != 100 && skalaUI != 125 && skalaUI != 150) skalaUI = 100;
  avatarW = batasi(avatarW, 160, 7680);
  avatarH = batasi(avatarH, 160, 4320);
  kontrolW = batasi(kontrolW, 380, 3840);
  kontrolH = batasi(kontrolH, 300, 2160);
  latarR = batasi(latarR, 0, 255);
  latarG = batasi(latarG, 0, 255);
  latarB = batasi(latarB, 0, 255);
  return true;
}

bool Settings::simpan(const char* jalur) const {
  std::error_code ec;
  fs::path p = fs::u8path(jalur);
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

  std::ofstream berkas(p, std::ios::trunc);
  if (!berkas) return false;

  berkas << "# Pengaturan Avatar Studio (otomatis, jangan diedit saat aplikasi berjalan)" << std::endl;
  berkas << "versi_pengaturan=" << versiPengaturan << std::endl;
  berkas << "batas_frame=" << batasFrame << std::endl;
  berkas << "skala_ui=" << skalaUI << std::endl;
  berkas << "zoom_avatar=" << zoomAvatar << std::endl;
  berkas << "sensitivitas_mikrofon=" << sensitivitasMikrofon << std::endl;
  berkas << "obs_host=" << obsHost << std::endl;
  berkas << "obs_port=" << obsPort << std::endl;
  berkas << "model_terakhir=" << modelTerakhir << std::endl;
  berkas << "avatar_x=" << avatarX << std::endl;
  berkas << "avatar_y=" << avatarY << std::endl;
  berkas << "avatar_w=" << avatarW << std::endl;
  berkas << "avatar_h=" << avatarH << std::endl;
  berkas << "kontrol_x=" << kontrolX << std::endl;
  berkas << "kontrol_y=" << kontrolY << std::endl;
  berkas << "kontrol_w=" << kontrolW << std::endl;
  berkas << "kontrol_h=" << kontrolH << std::endl;
  berkas << "latar_r=" << latarR << std::endl;
  berkas << "latar_g=" << latarG << std::endl;
  berkas << "latar_b=" << latarB << std::endl;
  berkas << "latar_transparan=" << (latarTransparan ? 1 : 0) << std::endl;
  berkas << "model_ofs_x=" << modelOfsX << std::endl;
  berkas << "model_ofs_y=" << modelOfsY << std::endl;
  berkas << "kamera_aktif=" << (kameraAktif ? 1 : 0) << std::endl;
  berkas << "kamera_perangkat=" << kameraPerangkat << std::endl;
  berkas << "kamera_sensitivitas=" << kameraSensitivitas << std::endl;
  berkas << "perangkat_mikrofon=" << perangkatMikrofon << std::endl;
  return berkas.good();
}