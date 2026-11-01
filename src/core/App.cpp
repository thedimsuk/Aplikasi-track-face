#include "core/App.hpp"

#include "gui/Gui.hpp"
#include "render/AvatarRenderer.hpp"
#include "render/CubismKit.hpp"
#include "render/ModelLive2D.hpp"

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef AVATAR_RESOURCE_DIR
#define AVATAR_RESOURCE_DIR "resources"
#endif

// ---------------------------------------------------------------------------
// Utilitas
// ---------------------------------------------------------------------------

namespace {

std::mutex g_mutexLog;
std::ofstream g_berkasLog;

const char* namaPrioritasLog(SDL_LogPriority prioritas) {
  switch (prioritas) {
    case SDL_LOG_PRIORITY_VERBOSE: return "VERBOSE";
    case SDL_LOG_PRIORITY_DEBUG: return "DEBUG";
    case SDL_LOG_PRIORITY_INFO: return "INFO";
    case SDL_LOG_PRIORITY_WARN: return "WARN";
    case SDL_LOG_PRIORITY_ERROR: return "ERROR";
    case SDL_LOG_PRIORITY_CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
  }
}

const char* namaKategoriLog(int kategori) {
  switch (kategori) {
    case SDL_LOG_CATEGORY_APPLICATION: return "APPLICATION";
    case SDL_LOG_CATEGORY_ERROR: return "ERROR";
    case SDL_LOG_CATEGORY_ASSERT: return "ASSERT";
    case SDL_LOG_CATEGORY_SYSTEM: return "SYSTEM";
    case SDL_LOG_CATEGORY_AUDIO: return "AUDIO";
    case SDL_LOG_CATEGORY_VIDEO: return "VIDEO";
    case SDL_LOG_CATEGORY_RENDER: return "RENDER";
    case SDL_LOG_CATEGORY_INPUT: return "INPUT";
    case SDL_LOG_CATEGORY_TEST: return "TEST";
    default: return "CUSTOM";
  }
}

void tulisLog(void*, int kategori, SDL_LogPriority prioritas,
              const char* pesan) {
  const std::uint64_t waktu = SDL_GetTicks64();
  const char* teks = pesan ? pesan : "(pesan kosong)";
  const std::time_t sekarang = std::time(nullptr);
  std::tm waktuLokal{};
#ifdef _WIN32
  localtime_s(&waktuLokal, &sekarang);
#else
  localtime_r(&sekarang, &waktuLokal);
#endif
  char capWaktu[32]{};
  std::strftime(capWaktu, sizeof(capWaktu), "%Y-%m-%d %H:%M:%S",
                &waktuLokal);
  std::lock_guard<std::mutex> kunci(g_mutexLog);
  const char* kategoriTampil = namaKategoriLog(kategori);
  std::fprintf(stderr, "[%s][%llu ms][%s][%s] %s\n", capWaktu,
               static_cast<unsigned long long>(waktu),
               namaPrioritasLog(prioritas), kategoriTampil, teks);
  std::fflush(stderr);
  if (g_berkasLog) {
    g_berkasLog << '[' << capWaktu << "][" << waktu << " ms]["
                << namaPrioritasLog(prioritas) << "][" << kategoriTampil
                << "] " << teks << '\n';
    g_berkasLog.flush();
  }
}

std::string mulaiLog() {
  char* prefPath = SDL_GetPrefPath("Avatar Studio", "Avatar Studio");
  if (!prefPath) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Folder log tidak dapat ditentukan: %s", SDL_GetError());
    return {};
  }

  const std::filesystem::path jalur =
      std::filesystem::u8path(prefPath) / "avatar-studio.log";
  SDL_free(prefPath);
  g_berkasLog.open(jalur, std::ios::out | std::ios::app);
  if (!g_berkasLog) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Log tidak bisa ditulis: %s",
                jalur.u8string().c_str());
    return {};
  }

  SDL_LogSetOutputFunction(tulisLog, nullptr);
  SDL_Log("Log diagnostik: %s", jalur.u8string().c_str());
  return jalur.u8string();
}

}  // namespace

void App::tampilkanGalat(const char* judul, const char* isi,
                         SDL_Window* induk) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, judul, isi, induk);
}

bool App::jendelaTerlihat(SDL_Window* w) {
  if (!w) return false;
  const Uint32 f = SDL_GetWindowFlags(w);
  return (f & SDL_WINDOW_SHOWN) && !(f & SDL_WINDOW_MINIMIZED);
}

static void aturIkonAplikasi(SDL_Window* jendela) {
  std::vector<std::filesystem::path> kandidatIkon;
  if (char* jalurDasar = SDL_GetBasePath()) {
    kandidatIkon.emplace_back(std::filesystem::u8path(jalurDasar) /
                              "../share/avatar-studio/avatar-studio.bmp");
    SDL_free(jalurDasar);
  }
  kandidatIkon.emplace_back(
      std::filesystem::path(AVATAR_RESOURCE_DIR) / "avatar-studio.bmp");

  SDL_Surface* ikon = nullptr;
  for (const auto& jalur : kandidatIkon) {
    const std::string jalurUtf8 = jalur.u8string();
    ikon = SDL_LoadBMP(jalurUtf8.c_str());
    if (ikon) break;
  }
  if (!ikon) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Ikon aplikasi gagal dimuat: %s",
                SDL_GetError());
    return;
  }

  SDL_SetWindowIcon(jendela, ikon);
  SDL_FreeSurface(ikon);
}

class PemulihKonteksGL {
 public:
  PemulihKonteksGL()
      : jendela_(SDL_GL_GetCurrentWindow()),
        konteks_(SDL_GL_GetCurrentContext()) {}

  ~PemulihKonteksGL() {
    if (jendela_ && konteks_ &&
        (SDL_GL_GetCurrentWindow() != jendela_ ||
         SDL_GL_GetCurrentContext() != konteks_) &&
        SDL_GL_MakeCurrent(jendela_, konteks_) != 0) {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                   "Gagal memulihkan konteks OpenGL: %s", SDL_GetError());
    }
  }

 private:
  SDL_Window* jendela_;
  SDL_GLContext konteks_;
};

// Tidur sampai tenggat waktu tertentu (mikrodetik) — tanpa busy loop.
static void tidurSampai(std::uint64_t tenggat, std::uint64_t freq) {
  const std::uint64_t sekarang = SDL_GetPerformanceCounter();
  if (sekarang >= tenggat) return;
  const double detik =
      static_cast<double>(tenggat - sekarang) / static_cast<double>(freq);
  std::this_thread::sleep_for(std::chrono::duration<double>(detik));
}

// ---------------------------------------------------------------------------
// Inisialisasi
// ---------------------------------------------------------------------------

App::~App() { bersihkan(); }

bool App::init() {
  // 1) SDL video
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    const std::string detail =
        std::string("Komponen tampilan (SDL2) tidak bisa dimulai.\n\n"
                    "Detail teknis: ") +
        SDL_GetError();
    tampilkanGalat("Gagal memulai aplikasi", detail.c_str());
    return false;
  }
  mulaiLog();
  const char* jalurPengaturan = Settings::jalurBawaan();
  const bool pengaturanDimuat = settings_.muat(jalurPengaturan);
  SDL_Log("Pengaturan %s: %s", jalurPengaturan,
          pengaturanDimuat ? "berhasil dibaca" : "tidak ditemukan; memakai bawaan");
  SDL_Log("Avatar Studio mulai. Model terakhir: %s",
          settings_.modelTerakhir.empty() ? "(belum ada)"
                                          : settings_.modelTerakhir.c_str());
#ifdef AVATAR_HAVE_CUBISM
  SDL_Log("Build mendukung Cubism SDK.");
#else
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
               "Build ini tidak menyertakan Cubism SDK.");
#endif

  // 2) Konteks kompatibilitas OpenGL — renderer Cubism GLES2 memakai
  //    status OpenGL legacy yang tidak tersedia di profil Core.
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // 3) Jendela avatar — bersih, hanya model + latar (untuk OBS)
  Uint32 flagsAvatar =
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
      SDL_WINDOW_SHOWN;
  winAvatar_ = SDL_CreateWindow(
      "Avatar Studio", settings_.avatarX, settings_.avatarY,
      settings_.avatarW, settings_.avatarH, flagsAvatar);
  if (!winAvatar_) {
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    winAvatar_ = SDL_CreateWindow(
        "Avatar Studio", settings_.avatarX, settings_.avatarY,
        settings_.avatarW, settings_.avatarH, flagsAvatar);
  }
  if (!winAvatar_) {
    const std::string detail =
        std::string("Jendela avatar tidak bisa dibuat.\n\n"
                    "Detail teknis: ") +
        SDL_GetError();
    tampilkanGalat("Gagal membuat jendela avatar", detail.c_str());
    return false;
  }
  aturIkonAplikasi(winAvatar_);

  ctxAvatar_ = SDL_GL_CreateContext(winAvatar_);
  if (!ctxAvatar_) {
    tampilkanGalat("Gagal membuat konteks OpenGL",
                   "Aplikasi tidak bisa membuat konteks grafis OpenGL 3.3.\n\n"
                   "Coba perbarui driver grafis (Mesa), lalu jalankan ulang "
                   "aplikasi.\n\nDetail teknis ada di pesan berikutnya.",
                   winAvatar_);
    tampilkanGalat("Detail teknis", SDL_GetError(), winAvatar_);
    return false;
  }
  SDL_GL_SetSwapInterval(0); // pacing manual sesuai fps pengatur (hemat CPU)

  // 4) Jendela kontrol — berbagi konteks GL dengan avatar
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  winKontrol_ = SDL_CreateWindow(
      "Pengaturan Avatar Studio", settings_.kontrolX, settings_.kontrolY,
      settings_.kontrolW, settings_.kontrolH,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
          SDL_WINDOW_SHOWN);
  if (!winKontrol_) {
    const std::string detail =
        std::string("Jendela pengaturan tidak bisa dibuat.\n\n"
                    "Detail teknis: ") +
        SDL_GetError();
    tampilkanGalat("Gagal membuat jendela pengaturan", detail.c_str(),
                   winAvatar_);
    return false;
  }
  aturIkonAplikasi(winKontrol_);

  ctxKontrol_ = SDL_GL_CreateContext(winKontrol_);
  if (!ctxKontrol_) {
    tampilkanGalat("Gagal membuat konteks OpenGL (jendela pengaturan)",
                   "Coba perbarui driver grafis (Mesa), lalu jalankan ulang "
                   "aplikasi.",
                   winKontrol_);
    tampilkanGalat("Detail teknis", SDL_GetError(), winKontrol_);
    return false;
  }
  SDL_GL_SetSwapInterval(0);

  SDL_Log("OpenGL: %s",
          reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  int sampel = 0;
  if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &sampel) == 0 && sampel > 0)
    glEnable(GL_MULTISAMPLE);

  // 5) Renderer avatar
  renderer_ = new AvatarRenderer();
  renderer_->setWarnaLatar(settings_.latarR, settings_.latarG,
                           settings_.latarB);
  renderer_->setLatarTransparan(settings_.latarTransparan);
  renderer_->setPosisiModel(settings_.modelOfsX, settings_.modelOfsY);
  renderer_->setZoom(settings_.zoomAvatar / 100.0f);
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  // 6) GUI — jendela kontrol (konteks aktif = kontrol)
  SDL_GL_MakeCurrent(winKontrol_, ctxKontrol_);
  gui_ = new Gui();
  if (!gui_->initKontrol(winKontrol_, ctxKontrol_, settings_)) {
    tampilkanGalat("Gagal memuat antarmuka",
                   "Komponen antarmuka (ImGui) tidak bisa dimulai. "
                   "Coba jalankan ulang aplikasi.",
                   winKontrol_);
    return false;
  }

  // 7) GUI overlay menu avatar (konteks aktif = avatar)
  SDL_GL_MakeCurrent(winAvatar_, ctxAvatar_);
  if (!gui_->initOverlay(winAvatar_, ctxAvatar_,
                         settings_.skalaUI / 100.0f)) {
    tampilkanGalat("Gagal memuat menu avatar",
                   "Menu klik-kanan jendela avatar tidak bisa dibuat.",
                   winAvatar_);
    return false;
  }

  // 8) Muat lagi model terakhir (bila masih ada) — pengguna tidak perlu
  //    memilih ulang tiap kali membuka aplikasi.
  if (!settings_.modelTerakhir.empty()) {
    std::error_code ecModel;
    if (std::filesystem::exists(std::filesystem::u8path(settings_.modelTerakhir),
                                ecModel)) {
      if (!muatModel(settings_.modelTerakhir))
        SDL_Log("Model terakhir gagal dimuat: %s", statusModel().c_str());
    } else {
      statusModel_ =
          "Model yang terakhir dipakai tidak ditemukan lagi. "
          "Pilih folder model lewat tombol \"Pilih folder model...\" di bawah.";
      settings_.modelTerakhir.clear();
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Aksi dari GUI
// ---------------------------------------------------------------------------

void App::mintaBukaJendelaKontrol() {
  if (!winKontrol_) return;
  SDL_ShowWindow(winKontrol_);
  SDL_RaiseWindow(winKontrol_);
}

void App::mintaKeluar() { berjalan_ = false; }

bool App::muatModel(const std::string& jalur) {
  SDL_Log("Memulai pemuatan model dari: %s", jalur.c_str());
  std::error_code ec;
  std::filesystem::path folder = std::filesystem::u8path(jalur);
  if (std::filesystem::is_regular_file(folder, ec)) folder = folder.parent_path();
  const std::string galatFolder =
      ModelLive2D::periksaFolder(folder.u8string());
  if (!galatFolder.empty()) {
    statusModel_ = galatFolder;
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Validasi folder model gagal (%s): %s",
                 folder.u8string().c_str(), galatFolder.c_str());
    return false;
  }

  const std::string jalurModel =
      ModelLive2D::cariModel3Json(folder.u8string());
  if (jalurModel.empty()) {
    statusModel_ = "Berkas .model3.json tidak ditemukan.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Berkas .model3.json tidak ditemukan di: %s",
                 folder.u8string().c_str());
    return false;
  }
  SDL_Log("Berkas konfigurasi model ditemukan: %s", jalurModel.c_str());

  PemulihKonteksGL pemulihKonteks;
  if (SDL_GL_MakeCurrent(winAvatar_, ctxAvatar_) != 0) {
    statusModel_ = std::string("Konteks OpenGL avatar gagal diaktifkan: ") +
                   SDL_GetError();
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s", statusModel_.c_str());
    return false;
  }
#ifdef AVATAR_HAVE_CUBISM
  std::filesystem::path folderShader =
      std::filesystem::u8path(AVATAR_CUBISM_SHADER_DIR);
  if (char* jalurDasar = SDL_GetBasePath()) {
    const std::filesystem::path shaderTerpasang =
        std::filesystem::u8path(jalurDasar) /
        "../share/avatar-studio";
    SDL_free(jalurDasar);
    std::error_code ecShader;
    if (std::filesystem::is_directory(shaderTerpasang, ecShader))
      folderShader = shaderTerpasang;
  }
  const std::string folderShaderUtf8 = folderShader.u8string();
  SDL_Log("Folder shader Cubism: %s", folderShaderUtf8.c_str());
  if (!CubismKit::mulai(folderShaderUtf8.c_str())) {
    statusModel_ = "Cubism Framework gagal diinisialisasi.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", statusModel_.c_str());
    return false;
  }
#endif

  if (!renderer_->muatModel(
          jalurModel,
          std::filesystem::u8path(jalurModel).parent_path().u8string())) {
    statusModel_ = renderer_->pesanGalatModel();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Pemuatan model gagal (%s): %s", jalurModel.c_str(),
                 statusModel_.c_str());
    return false;
  }
  statusModel_ = "Model berhasil dimuat.";
  settings_.modelOfsX = 0;
  settings_.modelOfsY = 0;
  renderer_->setPosisiModel(0.0f, 0.0f);
  // Simpan folder yang sudah divalidasi (bukan jalur berkas asli, agar
  // pemuatan ulang di init() selalu mengarah ke folder yang benar).
  settings_.modelTerakhir = folder.u8string();
  SDL_Log("Model berhasil dimuat: %s; posisi direset ke tengah.",
          renderer_->namaModel().c_str());
  return true;
}

bool App::mulaiMikrofon() {
  const bool berhasil = audioMouth_.mulai(settings_.sensitivitasMikrofon,
                                          settings_.perangkatMikrofon);
  if (berhasil && audioMouth_.namaPerangkat() != "Default sistem")
    settings_.perangkatMikrofon = audioMouth_.namaPerangkat();
  return berhasil;
}

void App::berhentiMikrofon() { audioMouth_.berhenti(); }

// ---------------------------------------------------------------------------
// Kamera tracker (optional)
// ---------------------------------------------------------------------------

bool App::mulaiKamera(const std::string& perangkat) {
  return kameraTracker_.mulai(perangkat);
}

void App::berhentiKamera() { kameraTracker_.berhenti(); }

void App::aturSensitivitasMikrofon(int sensitivitas) {
  settings_.sensitivitasMikrofon = std::clamp(sensitivitas, 1, 100);
  audioMouth_.aturSensitivitas(settings_.sensitivitasMikrofon);
}

void App::aturPerangkatMikrofon(const std::string& namaPerangkat) {
  settings_.perangkatMikrofon = namaPerangkat;
  if (audioMouth_.aktif() &&
      audioMouth_.mulai(settings_.sensitivitasMikrofon,
                        settings_.perangkatMikrofon) &&
      audioMouth_.namaPerangkat() != "Default sistem") {
    settings_.perangkatMikrofon = audioMouth_.namaPerangkat();
  }
}

void App::aturWarnaLatar(int r, int g, int b) {
  settings_.latarR = std::clamp(r, 0, 255);
  settings_.latarG = std::clamp(g, 0, 255);
  settings_.latarB = std::clamp(b, 0, 255);
  if (renderer_)
    renderer_->setWarnaLatar(settings_.latarR, settings_.latarG,
                             settings_.latarB);
}

void App::aturLatarTransparan(bool transparan) {
  settings_.latarTransparan = transparan;
  if (renderer_) renderer_->setLatarTransparan(transparan);
}

void App::aturPosisiModel(float x, float y) {
  settings_.modelOfsX = static_cast<int>(x);
  settings_.modelOfsY = static_cast<int>(y);
  if (renderer_) renderer_->setPosisiModel(settings_.modelOfsX,
                                           settings_.modelOfsY);
}

void App::aturZoomAvatar(int persen) {
  settings_.zoomAvatar = std::clamp(persen, 50, 300);
  if (renderer_) renderer_->setZoom(settings_.zoomAvatar / 100.0f);
}

bool App::sambungObs(const std::string& password) {
  return obs_.sambung(settings_.obsHost, settings_.obsPort, password);
}

void App::putusObs() { obs_.putus(); }

bool App::modelAktif() const {
  return renderer_ && renderer_->adaModel();
}

std::string App::namaModel() const {
  return renderer_ ? renderer_->namaModel() : std::string{};
}

std::string App::statusModel() const {
  if (renderer_ && !renderer_->pesanGalatModel().empty())
    return renderer_->pesanGalatModel();
  return statusModel_;
}

// ---------------------------------------------------------------------------
// Event — satu loop untuk kedua jendela
// ---------------------------------------------------------------------------

void App::tanganiEvent() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    gui_->prosesEvent(e); // ImGui menerima semua event (disaring per jendela)

    switch (e.type) {
      case SDL_QUIT:
        berjalan_ = false;
        break;

      case SDL_DROPFILE:
        if (e.drop.file) {
          if (!muatModel(e.drop.file))
            SDL_Log("Gagal memuat model: %s", statusModel().c_str());
        }
        SDL_free(e.drop.file);
        break;

      case SDL_WINDOWEVENT: {
        if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
          if (winKontrol_ &&
              e.window.windowID == SDL_GetWindowID(winKontrol_)) {
            // Menutup jendela kontrol hanya menyembunyikannya —
            // jendela avatar tidak boleh terganggu.
            SDL_HideWindow(winKontrol_);
          } else if (winAvatar_ &&
                     e.window.windowID == SDL_GetWindowID(winAvatar_)) {
            berjalan_ = false;
          }
        }
        break;
      }

      case SDL_MOUSEBUTTONDOWN: {
        // Klik kanan di jendela avatar → menu kecil (buka pengaturan/keluar)
        if (e.button.button == SDL_BUTTON_RIGHT && winAvatar_ &&
            e.button.windowID == SDL_GetWindowID(winAvatar_)) {
          gui_->mintaMenuAvatar();
        }
        // Klik kiri di jendela avatar → mulai geser-geser model
        if (e.button.button == SDL_BUTTON_LEFT && winAvatar_ &&
            e.button.windowID == SDL_GetWindowID(winAvatar_)) {
          dragAktif_ = true;
          dragMouseX_ = e.button.x;
          dragMouseY_ = e.button.y;
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        if (dragAktif_ && winAvatar_ &&
            e.motion.windowID == SDL_GetWindowID(winAvatar_)) {
          const int dx = e.motion.x - dragMouseX_;
          const int dy = e.motion.y - dragMouseY_;
          dragMouseX_ = e.motion.x;
          dragMouseY_ = e.motion.y;
          aturPosisiModel(settings_.modelOfsX + dx,
                          settings_.modelOfsY + dy);
        }
        break;
      }

      case SDL_MOUSEBUTTONUP: {
        if (e.button.button == SDL_BUTTON_LEFT)
          dragAktif_ = false;
        break;
      }

      default:
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void App::renderAvatar() {
  if (SDL_GL_MakeCurrent(winAvatar_, ctxAvatar_) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                 "Tidak bisa mengaktifkan konteks OpenGL avatar: %s",
                 SDL_GetError());
    return;
  }
  int w = 0, h = 0;
  SDL_GL_GetDrawableSize(winAvatar_, &w, &h); // ukuran piksel asli (HiDPI)
  const std::uint64_t sekarang = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
  const float delta = frameSebelumnya_ == 0
                          ? 1.0f / static_cast<float>(settings_.batasFrame)
                          : static_cast<float>(
                                static_cast<double>(sekarang -
                                                    frameSebelumnya_) /
                                freq);
  frameSebelumnya_ = sekarang;

  // Terapkan data kamera tracker (atau reset) SEBELUM perbarui agar parameter
  // Cubism menggunakan nilai yang benar pada frame ini.
  if (kameraTracker_.aktif() && renderer_->adaModel()) {
    const DataKamera kd = kameraTracker_.dataTerbaru();
    if (kd.aktif)
      renderer_->terapkanKamera(kd.eyeOpen);
    else
      renderer_->resetKamera();
  } else {
    renderer_->resetKamera();
  }

  renderer_->perbarui(delta, audioMouth_.tingkatSuara());
  renderer_->render(w, h);
  if (gui_->overlayAktif()) gui_->frameOverlay(*this); // hanya saat menu buka
  SDL_GL_SwapWindow(winAvatar_);
}

void App::renderKontrol() {
  SDL_GL_MakeCurrent(winKontrol_, ctxKontrol_);
  gui_->frameKontrol(*this);
  SDL_GL_SwapWindow(winKontrol_);
}

// ---------------------------------------------------------------------------
// Loop utama
// ---------------------------------------------------------------------------

int App::run() {
  const std::uint64_t freq = SDL_GetPerformanceFrequency();
  std::uint64_t tenggat = SDL_GetPerformanceCounter(); // mulai segera

  while (berjalan_) {
    tanganiEvent();
    if (!berjalan_) break;

    const bool tampilAvatar = jendelaTerlihat(winAvatar_);
    const bool tampilKontrol = jendelaTerlihat(winKontrol_);

    // Render hanya jendela yang terlihat (jendela tersembunyi = hemat CPU).
    if (tampilAvatar) {
      renderAvatar();

      // Pengukur fps (diperbarui tiap ~0,5 detik)
      fpsHitung_++;
      if (fpsMulai_ == 0) fpsMulai_ = SDL_GetPerformanceCounter();
      const std::uint64_t sekarangFps = SDL_GetPerformanceCounter();
      if (sekarangFps - fpsMulai_ >= freq / 2) {
        fps_ = static_cast<double>(fpsHitung_) *
               static_cast<double>(freq) /
               static_cast<double>(sekarangFps - fpsMulai_);
        fpsHitung_ = 0;
        fpsMulai_ = sekarangFps;
      }
    }
    if (tampilKontrol) renderKontrol();

    const std::uint64_t periode =
        freq / static_cast<std::uint64_t>(std::max(5, settings_.batasFrame));

    if (tampilAvatar || tampilKontrol) {
      // Tata waktu: tidur sampai tenggat — tanpa busy loop.
      const std::uint64_t sekarang = SDL_GetPerformanceCounter();
      if (sekarang < tenggat) {
        tidurSampai(tenggat, freq);
      } else if (sekarang - tenggat > periode * 2) {
        tenggat = sekarang; // sistem tersendat → jangan menumpuk keterlambatan
      }
      tenggat += periode;
    } else {
      // Tidak ada jendela terlihat → tidur panjang, hampir tanpa CPU.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      tenggat = SDL_GetPerformanceCounter() + periode;
    }
  }

  bersihkan();
  return 0;
}

// ---------------------------------------------------------------------------
// Pembersihan
// ---------------------------------------------------------------------------

void App::simpanGeometriJendela() {
  // Simpan posisi/ukuran hanya bila jendela sedang tampil.
  if (winAvatar_ && jendelaTerlihat(winAvatar_)) {
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(winAvatar_, &x, &y);
    SDL_GetWindowSize(winAvatar_, &w, &h);
    settings_.avatarX = x;
    settings_.avatarY = y;
    settings_.avatarW = w;
    settings_.avatarH = h;
  }
  if (winKontrol_ && jendelaTerlihat(winKontrol_)) {
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(winKontrol_, &x, &y);
    SDL_GetWindowSize(winKontrol_, &w, &h);
    settings_.kontrolX = x;
    settings_.kontrolY = y;
    settings_.kontrolW = w;
    settings_.kontrolH = h;
  }
}

void App::bersihkan() {
  if (sudahBeres_) return;
  sudahBeres_ = true;

  audioMouth_.berhenti();
  obs_.putus();
  simpanGeometriJendela();
  settings_.simpan(Settings::jalurBawaan());

  if (gui_) {
    // Matikan backend saat konteks GL miliknya masih bisa dipakai.
    if (ctxKontrol_) SDL_GL_MakeCurrent(winKontrol_, ctxKontrol_);
    gui_->shutdown();
    delete gui_;
    gui_ = nullptr;
  }

  if (renderer_) {
    if (winAvatar_ && ctxAvatar_)
      SDL_GL_MakeCurrent(winAvatar_, ctxAvatar_);
    renderer_->bersihkanModel();
    delete renderer_;
    renderer_ = nullptr;
    CubismKit::berhenti();
  }

  if (ctxKontrol_) {
    SDL_GL_DeleteContext(ctxKontrol_);
    ctxKontrol_ = nullptr;
  }
  if (ctxAvatar_) {
    SDL_GL_DeleteContext(ctxAvatar_);
    ctxAvatar_ = nullptr;
  }
  if (winKontrol_) {
    SDL_DestroyWindow(winKontrol_);
    winKontrol_ = nullptr;
  }
  if (winAvatar_) {
    SDL_DestroyWindow(winAvatar_);
    winAvatar_ = nullptr;
  }

  SDL_Quit();
}
