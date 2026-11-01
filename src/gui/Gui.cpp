#include "gui/Gui.hpp"

#include "core/App.hpp"
#include "core/Settings.hpp"

#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifndef AVATAR_RESOURCE_DIR
#define AVATAR_RESOURCE_DIR "resources"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Pencarian font: utamakan font bundel (Lato), fallback ke font sistem Debian.
// ---------------------------------------------------------------------------
std::string cariFont(const char* namaBundel, const char* namaSistem) {
  std::error_code ec;
  if (char* jalurDasar = SDL_GetBasePath()) {
    const fs::path terpasang =
        fs::u8path(jalurDasar) / "../share/avatar-studio/fonts" / namaBundel;
    SDL_free(jalurDasar);
    if (fs::exists(terpasang, ec)) return terpasang.u8string();
  }

  const fs::path bundel =
      fs::path(AVATAR_RESOURCE_DIR) / "fonts" / namaBundel;
  if (fs::exists(bundel, ec)) return bundel.u8string();

  const fs::path sistem =
      fs::path("/usr/share/fonts/truetype/dejavu") / namaSistem;
  if (fs::exists(sistem, ec)) return sistem.u8string();

  return {};
}

// ---------------------------------------------------------------------------
// Tema gelap yang nyaman + kontras cukup, jarak lega (target klik >= 32 px).
// ---------------------------------------------------------------------------
void terapkanTema(ImGuiStyle& g, float skala) {
  ImGui::StyleColorsDark(&g);

  g.WindowRounding    = 6.0f;
  g.ChildRounding     = 6.0f;
  g.FrameRounding     = 5.0f;
  g.GrabRounding      = 5.0f;
  g.PopupRounding     = 6.0f;
  g.ScrollbarRounding = 6.0f;
  g.TabRounding       = 5.0f;

  g.WindowPadding    = ImVec2(14.0f, 12.0f);
  g.FramePadding     = ImVec2(10.0f, 8.0f);  // tinggi tombol ~33 px @ font 17
  g.ItemSpacing      = ImVec2(10.0f, 8.0f);
  g.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  g.ScrollbarSize    = 16.0f;
  g.WindowBorderSize = 1.0f;
  g.FrameBorderSize  = 0.0f;

  const ImVec4 teks       (0.93f, 0.94f, 0.96f, 1.00f);
  const ImVec4 teksRedup  (0.70f, 0.73f, 0.78f, 1.00f);
  const ImVec4 aksen      (0.24f, 0.52f, 0.90f, 1.00f);
  const ImVec4 latarWin   (0.082f, 0.090f, 0.105f, 1.00f);
  const ImVec4 latarAnak  (0.110f, 0.120f, 0.140f, 1.00f);
  const ImVec4 latarTombol(0.155f, 0.170f, 0.200f, 1.00f);

  g.Colors[ImGuiCol_Text]                 = teks;
  g.Colors[ImGuiCol_TextDisabled]         = teksRedup;
  g.Colors[ImGuiCol_WindowBg]             = latarWin;
  g.Colors[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
  g.Colors[ImGuiCol_PopupBg]              = latarWin;
  g.Colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
  g.Colors[ImGuiCol_FrameBg]              = latarTombol;
  g.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
  g.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
  g.Colors[ImGuiCol_TitleBg]              = latarWin;
  g.Colors[ImGuiCol_TitleBgActive]        = latarAnak;
  g.Colors[ImGuiCol_Button]               = latarTombol;
  g.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.45f, 0.80f, 1.00f);
  g.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.16f, 0.38f, 0.70f, 1.00f);
  g.Colors[ImGuiCol_Header]               = ImVec4(0.17f, 0.28f, 0.44f, 1.00f);
  g.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.36f, 0.58f, 1.00f);
  g.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.16f, 0.32f, 0.52f, 1.00f);
  g.Colors[ImGuiCol_CheckMark]            = aksen;
  g.Colors[ImGuiCol_SliderGrab]           = aksen;
  g.Colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.32f, 0.62f, 1.00f, 1.00f);
  g.Colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
  g.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.16f, 0.32f, 0.52f, 0.40f);
  g.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.07f, 0.08f, 0.90f);
  g.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.30f, 0.34f, 1.00f);
  g.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.34f, 0.37f, 0.42f, 1.00f);
  g.Colors[ImGuiCol_ScrollbarGrabActive]  = aksen;
  g.Colors[ImGuiCol_NavHighlight]         = aksen;
  g.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.02f, 0.02f, 0.03f, 0.55f);

  g.ScaleAllSizes(skala); // skala 100% / 125% / 150%
}

#ifdef _WIN32
namespace {

std::string pilihPathDialog(bool pilihFolder) {
  const HRESULT inisialisasi =
      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                  COINIT_DISABLE_OLE1DDE);
  if (FAILED(inisialisasi) && inisialisasi != RPC_E_CHANGED_MODE) return {};
  const bool perluUninitialize = SUCCEEDED(inisialisasi);

  IFileOpenDialog* dialog = nullptr;
  HRESULT hasil = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog));
  if (SUCCEEDED(hasil)) {
    FILEOPENDIALOGOPTIONS opsi = 0;
    hasil = dialog->GetOptions(&opsi);
    if (SUCCEEDED(hasil)) {
      opsi |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
      opsi |= pilihFolder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST;
      hasil = dialog->SetOptions(opsi);
    }
    if (SUCCEEDED(hasil)) {
      if (pilihFolder) {
        hasil = dialog->SetTitle(L"Pilih folder model Live2D");
      } else {
        const COMDLG_FILTERSPEC filter[] = {
            {L"Model Live2D (*.model3.json)", L"*.model3.json"},
            {L"Semua berkas", L"*.*"}};
        hasil = dialog->SetFileTypes(2, filter);
        if (SUCCEEDED(hasil))
          hasil = dialog->SetTitle(L"Pilih berkas model Live2D");
      }
    }
    if (SUCCEEDED(hasil)) hasil = dialog->Show(nullptr);

    IShellItem* item = nullptr;
    if (SUCCEEDED(hasil)) hasil = dialog->GetResult(&item);
    PWSTR path = nullptr;
    if (SUCCEEDED(hasil))
      hasil = item->GetDisplayName(SIGDN_FILESYSPATH, &path);

    std::string pathUtf8;
    if (SUCCEEDED(hasil) && path) {
      const int panjang = WideCharToMultiByte(
          CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
      if (panjang > 1) {
        pathUtf8.resize(static_cast<std::size_t>(panjang));
        WideCharToMultiByte(CP_UTF8, 0, path, -1, pathUtf8.data(), panjang,
                            nullptr, nullptr);
        pathUtf8.pop_back();
      }
    }
    if (path) CoTaskMemFree(path);
    if (item) item->Release();
    dialog->Release();
    if (perluUninitialize) CoUninitialize();
    return pathUtf8;
  }

  if (dialog) dialog->Release();
  if (perluUninitialize) CoUninitialize();
  return {};
}

}  // namespace

std::string dialogFolderSistem() { return pilihPathDialog(true); }

std::string dialogBerkasSistem() { return pilihPathDialog(false); }

#else

// Pangkas spasi/enter di ujung (keluaran dialog sistem).
std::string pangkas(std::string s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                        s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  return s;
}

// Buka dialog pilih folder bawaan sistem.
// Coba zenity/kdialog (mengembalikan jalur via stdout) terlebih dahulu.
// Kalau tidak ada, buka file manager (nautilus/thunar/pcmanfm/xdg-open)
// agar pengguna bisa mencari folder, lalu salin jalurnya ke kolom manual.
// Kembalikan jalur folder atau string kosong bila dibatalkan/tidak ada dialog.
std::string dialogFolderSistem() {
  // 1) Coba dialog yang mengembalikan jalur via stdout.
  const char* perintahDialog =
      "if command -v zenity >/dev/null 2>&1; then "
      "zenity --file-selection --directory "
      "--title='Pilih folder model Live2D' 2>/dev/null; "
      "elif command -v kdialog >/dev/null 2>&1; then "
      "kdialog --getexistingdirectory . 2>/dev/null; fi";
  FILE* pipa = ::popen(perintahDialog, "r");
  if (pipa) {
    char buf[4096] = {};
    std::string hasil;
    if (std::fgets(buf, sizeof(buf), pipa)) hasil = pangkas(buf);
    ::pclose(pipa);
    if (!hasil.empty()) return hasil;
  }

  // 2) Tidak ada dialog yang bisa mengembalikan jalur: buka file manager
  //    agar pengguna bisa mencari folder secara visual.
  const char* perintahFileManager =
      "if command -v nautilus >/dev/null 2>&1; then "
      "nautilus ~ 2>/dev/null & "
      "elif command -v thunar >/dev/null 2>&1; then "
      "thunar ~ 2>/dev/null & "
      "elif command -v pcmanfm >/dev/null 2>&1; then "
      "pcmanfm ~ 2>/dev/null & "
      "elif command -v xdg-open >/dev/null 2>&1; then "
      "xdg-open ~ 2>/dev/null & "
      "fi";
  ::system(perintahFileManager);
  return {};
}

// Buka dialog pilih berkas bawaan sistem.
// Coba zenity/kdialog (mengembalikan jalur via stdout) terlebih dahulu.
// Kalau tidak ada, buka file manager (nautilus/thunar/pcmanfm/xdg-open)
// agar pengguna bisa mencari berkas, lalu salin jalurnya ke kolom manual.
// Kembalikan jalur berkas (.model3.json) atau string kosong bila dibatalkan/tidak ada dialog.
std::string dialogBerkasSistem() {
  // 1) Coba dialog yang mengembalikan jalur via stdout.
  const char* perintahDialog =
      "if command -v zenity >/dev/null 2>&1; then "
      "zenity --file-selection "
      "--title='Pilih berkas model Live2D (.model3.json)' 2>/dev/null; "
      "elif command -v kdialog >/dev/null 2>&1; then "
      "kdialog --getopenfilename . '*.model3.json' 2>/dev/null; fi";
  FILE* pipa = ::popen(perintahDialog, "r");
  if (pipa) {
    char buf[4096] = {};
    std::string hasil;
    if (std::fgets(buf, sizeof(buf), pipa)) hasil = pangkas(buf);
    ::pclose(pipa);
    if (!hasil.empty()) return hasil;
  }

  // 2) Tidak ada dialog yang bisa mengembalikan jalur: buka file manager
  //    agar pengguna bisa mencari berkas secara visual.
  const char* perintahFileManager =
      "if command -v nautilus >/dev/null 2>&1; then "
      "nautilus ~ 2>/dev/null & "
      "elif command -v thunar >/dev/null 2>&1; then "
      "thunar ~ 2>/dev/null & "
      "elif command -v pcmanfm >/dev/null 2>&1; then "
      "pcmanfm ~ 2>/dev/null & "
      "elif command -v xdg-open >/dev/null 2>&1; then "
      "xdg-open ~ 2>/dev/null & "
      "fi";
  ::system(perintahFileManager);
  return {};
}

#endif

// Minta pengguna memilih berkas model .model3.json: dialog sistem bila ada,
// kalau tidak, ketik/tempel jalur berkas secara manual.
std::string Gui::pilihBerkasModel() {
  const std::string dariDialog = dialogBerkasSistem();
  if (!dariDialog.empty()) return dariDialog;
  // Tidak ada dialog sistem (atau dibatalkan): pakai kolom ketik manual.
  mintaJalurManual_ = true;
  return {};
}

// Minta pengguna memilih folder model: dialog sistem bila ada,
// kalau tidak, ketik/tempel jalur folder secara manual.
std::string Gui::pilihFolderModel() {
  const std::string dariDialog = dialogFolderSistem();
  if (!dariDialog.empty()) return dariDialog;
  // Tidak ada dialog sistem (atau dibatalkan): pakai kolom ketik manual.
  mintaJalurManual_ = true;
  return {};
}

// Cari model contoh Hiyori di folder SDK (bila SDK ikut di direktori proyek).
std::string Gui::cariModelContoh() {
  namespace fs = std::filesystem;
  fs::path dasar;
  if (char* jalurDasar = SDL_GetBasePath()) {
    dasar = fs::u8path(jalurDasar);
    SDL_free(jalurDasar);
    dasar = dasar.parent_path();
  }
  if (dasar.empty()) {
    std::error_code ec;
    dasar = fs::current_path(ec);
    if (ec) dasar = ".";
  }
  // Dari folder build/... naik sampai ketemu third_party.
  for (fs::path d = dasar; !d.empty(); d = d.parent_path()) {
    const fs::path contoh =
        d / "third_party" / "CubismSdk-Native" / "Samples" / "Resources" /
        "Hiyori";
    std::error_code ec2;
    if (fs::is_directory(contoh, ec2)) return contoh.u8string();
    if (d == d.parent_path()) break;
  }
  return {};
}

// Muat jalur model yang diketik pengguna secara manual.
void muatJalurManual(App& app, const char* jalur, std::string& pesanInfo) {
  std::string p = jalur;
  while (!p.empty() && std::isspace(static_cast<unsigned char>(p.back())))
    p.pop_back();
  if (p.empty()) return;
  if (app.muatModel(p))
    pesanInfo = "Model berhasil dimuat: " + app.namaModel() + ".";
  else
    pesanInfo.clear();
}

// Muat pasangan font utama+tebal pada skala tertentu (fallback bila hilang).
void muatFontPair(float skala, ImFont*& keluarReg, ImFont*& keluarTebal,
                  ImGuiIO& io) {
  keluarReg = nullptr;
  keluarTebal = nullptr;

  const std::string jalurReg = cariFont("Lato-Regular.ttf", "DejaVuSans.ttf");
  const std::string jalurTeb = cariFont("Lato-Bold.ttf", "DejaVuSans-Bold.ttf");

  if (!jalurReg.empty())
    keluarReg = io.Fonts->AddFontFromFileTTF(jalurReg.c_str(), 17.0f * skala);
  if (!keluarReg) keluarReg = io.Fonts->AddFontDefault();


  keluarTebal = keluarReg;
  if (!jalurTeb.empty()) {
    ImFont* tebal =
        io.Fonts->AddFontFromFileTTF(jalurTeb.c_str(), 19.0f * skala);
    if (tebal) keluarTebal = tebal;
  }
}

// ---------------------------------------------------------------------------
// Inisialisasi jendela kontrol
// ---------------------------------------------------------------------------
bool Gui::initKontrol(SDL_Window* jendela, SDL_GLContext konteks, Settings& s) {
  IMGUI_CHECKVERSION();
  ctxKontrol_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctxKontrol_);

  ImGuiIO& io = ImGui::GetIO();
  std::snprintf(obsHost_, sizeof(obsHost_), "%s", s.obsHost.c_str());
  io.IniFilename = nullptr; // pengaturan kita sendiri, bukan imgui.ini
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // dukungan keyboard

  const float skala = s.skalaUI / 100.0f;
  ImFont* fontReg = nullptr;
  muatFontPair(skala, fontReg, fontTebal_, io);
  io.FontDefault = fontReg;

  terapkanTema(ImGui::GetStyle(), skala);

  if (!ImGui_ImplSDL2_InitForOpenGL(jendela, konteks)) return false;
  if (!ImGui_ImplOpenGL3_Init("#version 330")) return false;

  winKontrol_ = jendela;
  return true;
}

// ---------------------------------------------------------------------------
// Inisialisasi menu klik-kanan jendela avatar (konteks ImGui terpisah)
// ---------------------------------------------------------------------------
bool Gui::initOverlay(SDL_Window* jendela, SDL_GLContext konteks, float skala) {
  IMGUI_CHECKVERSION();
  ctxOverlay_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctxOverlay_);

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;

  // Font sedikit lebih kecil untuk menu ringan, tetap mengikuti skala UI.
  ImFont* font = nullptr;
  const std::string jalur = cariFont("Lato-Regular.ttf", "DejaVuSans.ttf");
  if (!jalur.empty())
    font = io.Fonts->AddFontFromFileTTF(jalur.c_str(), 16.0f * skala);
  if (!font) font = io.Fonts->AddFontDefault();
  io.FontDefault = font;

  terapkanTema(ImGui::GetStyle(), skala);

  if (!ImGui_ImplSDL2_InitForOpenGL(jendela, konteks)) return false;
  if (!ImGui_ImplOpenGL3_Init("#version 330")) return false;

  winOverlay_ = jendela;
  return true;
}

void Gui::prosesEvent(const SDL_Event& e) {
  // Kedua backend memfilter event sesuai windowID milik mereka sendiri.
  if (ctxKontrol_) {
    ImGui::SetCurrentContext(ctxKontrol_);
    ImGui_ImplSDL2_ProcessEvent(&e);
  }
  if (ctxOverlay_) {
    ImGui::SetCurrentContext(ctxOverlay_);
    ImGui_ImplSDL2_ProcessEvent(&e);
  }
}

void Gui::mintaMenuAvatar() { permintaanMenu_ = true; }

bool Gui::overlayAktif() const { return permintaanMenu_ || menuTerbuka_; }

// ---------------------------------------------------------------------------
// Frame jendela kontrol.
// ---------------------------------------------------------------------------
void Gui::frameKontrol(App& app) {
  if (!ctxKontrol_) return;
  ImGui::SetCurrentContext(ctxKontrol_);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings;

  ImGui::Begin("Pengaturan Avatar", nullptr, flags);

  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("Avatar Studio");
  ImGui::PopFont();
  ImGui::Separator();
  ImGui::Spacing();

  // ---- Bagian 1: Model (paling sering dipakai) ----
  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("1. Model karakter");
  ImGui::PopFont();
  if (app.modelAktif()) {
    ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.55f, 1.0f),
                       "Aktif: %s", app.namaModel().c_str());
    ImGui::TextWrapped(
        "Gerakan: kepala tetap ke depan, mulut mengikuti mikrofon, "
        "kedip otomatis acak setiap sekitar 1-2 detik.");
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
                       "Belum ada model yang tampil.");
    ImGui::TextWrapped("%s", app.statusModel().c_str());
  }
  ImGui::Spacing();
  // Tombol utama: besar dan mudah diklik (target >= 32px).
  ImGui::PushFont(fontTebal_);
  if (ImGui::Button("Pilih folder model...", ImVec2(-1.0f, 44.0f))) {
    const std::string dipilih = pilihFolderModel();
    if (!dipilih.empty()) {
      if (app.muatModel(dipilih))
        pesanInfo_ = "Model berhasil dimuat: " + app.namaModel() + ".";
      else
        pesanInfo_.clear();
    }
  }
  ImGui::PopFont();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Buka jendela untuk memilih folder berisi model Live2D "
                      "(berkas .model3.json).");
  // Tombol pilih berkas — langsung pilih berkas .model3.json (bukan folder).
  ImGui::PushFont(fontTebal_);
  if (ImGui::Button("Pilih berkas model...", ImVec2(-1.0f, 44.0f))) {
    const std::string dipilih = pilihBerkasModel();
    if (!dipilih.empty()) {
      if (app.muatModel(dipilih))
        pesanInfo_ = "Model berhasil dimuat: " + app.namaModel() + ".";
      else
        pesanInfo_.clear();
    }
  }
  ImGui::PopFont();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Buka dialog untuk memilih berkas model Live2D "
                      "secara langsung (berkas .model3.json).");
  // Tombol contoh — hanya muncul bila model contoh bawaan SDK ditemukan.
  const std::string contoh = cariModelContoh();
  if (!contoh.empty() && !app.modelAktif()) {
    if (ImGui::Button("Coba model contoh (Hiyori)", ImVec2(-1.0f, 36.0f))) {
      if (app.muatModel(contoh))
        pesanInfo_ = "Model contoh berhasil dimuat.";
      else
        pesanInfo_.clear();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Contoh bawaan untuk mencoba tanpa mengunduh model.");
  }
  if (!pesanInfo_.empty()) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", pesanInfo_.c_str());
  }
  ImGui::Spacing();
  ImGui::TextWrapped("Cara lain: seret (drag & drop) folder model ke jendela "
                     "avatar.");

  // Input jalur manual — hanya tampil bila dialog sistem tidak tersedia atau
  // dibatalkan (pilihFolderModel()/pilihBerkasModel() menyetel mintaJalurManual_ = true).
  if (mintaJalurManual_) {
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("Ketik atau tempel jalur folder atau berkas model (.model3.json) "
                       "di bawah ini, lalu tekan Enter atau klik \"Muat jalur ini\".:");
    static char jalurManual[1024] = {};
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##JalurModelManual", jalurManual,
                         sizeof(jalurManual),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      muatJalurManual(app, jalurManual, pesanInfo_);
      mintaJalurManual_ = false;
    }
    if (ImGui::Button("Batal##Manual", ImVec2(80.0f, 0.0f))) {
      mintaJalurManual_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Muat jalur ini##Manual", ImVec2(150.0f, 0.0f))) {
      muatJalurManual(app, jalurManual, pesanInfo_);
      mintaJalurManual_ = false;
    }
    if (!pesanInfo_.empty()) {
      ImGui::Spacing();
      ImGui::TextWrapped("%s", pesanInfo_.c_str());
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("2. Mulut & animasi mata");
  ImGui::PopFont();
  if (app.mikrofonAktif()) {
    if (ImGui::Button("Hentikan mikrofon", ImVec2(-1.0f, 36.0f)))
      app.berhentiMikrofon();
  } else if (ImGui::Button("Mulai mikrofon", ImVec2(-1.0f, 36.0f))) {
    app.mulaiMikrofon();
  }
  ImGui::TextWrapped("%s", app.statusMikrofon().c_str());

  // Pemilihan perangkat audio mikrofon (cache daftar, segarkan saat perlu).
  static bool mikrofonSegar_ = false;
  static std::vector<std::string> mikrofonTersedia_;
  if (!mikrofonSegar_) {
    mikrofonTersedia_ = AudioMouthTracker::daftarPerangkatCapture();
    mikrofonTersedia_.insert(mikrofonTersedia_.begin(), "(default sistem)");
    mikrofonSegar_ = true;
  }
  int mikrofonTerpilih_ = 0;
  const std::string& mikrofonSaatIni = app.pengaturan().perangkatMikrofon;
  for (size_t i = 0; i < mikrofonTersedia_.size(); ++i) {
    if (mikrofonTersedia_[i] == mikrofonSaatIni ||
        (mikrofonSaatIni.empty() && mikrofonTersedia_[i] == "(default sistem)")) {
      mikrofonTerpilih_ = static_cast<int>(i);
      break;
    }
  }
  if (ImGui::BeginCombo("Perangkat mikrofon",
                        mikrofonTersedia_[mikrofonTerpilih_].c_str())) {
    for (int i = 0; i < static_cast<int>(mikrofonTersedia_.size()); ++i) {
      bool isSelected = (mikrofonTerpilih_ == i);
      if (ImGui::Selectable(mikrofonTersedia_[i].c_str(), isSelected)) {
        mikrofonTerpilih_ = i;
        std::string dipilih = mikrofonTersedia_[i];
        if (dipilih == "(default sistem)") dipilih = "";
        app.aturPerangkatMikrofon(dipilih);
      }
      if (isSelected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Segarkan##Mikrofon")) {
    mikrofonSegar_ = false;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Segarkan daftar perangkat audio capture (misal setelah "
                      "menyambung mikrofon USB atau kamera handphone).");

  int sensitivitas = app.pengaturan().sensitivitasMikrofon;
  if (ImGui::SliderInt("Sensitivitas mikrofon", &sensitivitas, 1, 100))
    app.aturSensitivitasMikrofon(sensitivitas);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Nilai rendah menahan noise dan hanya merespons suara "
                      "yang sangat kuat. Nilai tinggi menangkap suara lebih "
                      "pelan.");
  ImGui::ProgressBar(app.levelInputMikrofon(), ImVec2(-1.0f, 0.0f),
                     "Level input mikrofon");
  ImGui::TextWrapped(
      "Input mikrofon diproses lokal sebagai level suara; suara keras lain "
      "juga dapat menggerakkan mulut. Mikrofon DroidCam, bila tersedia di "
      "sistem audio Linux, dapat dipilih terpisah dari kameranya. Pada "
      "sensitivitas minimum, hanya suara sangat keras yang membuka mulut.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("3. Tampilan & performa");
  ImGui::PopFont();
  ImGui::SliderInt("Batas FPS", &app.pengaturan().batasFrame, 5, 120);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Makin kecil makin hemat baterai/CPU. "
                      "30 sudah cukup mulus untuk avatar.");
  float warnaLatar[3] = {
      app.pengaturan().latarR / 255.0f,
      app.pengaturan().latarG / 255.0f,
      app.pengaturan().latarB / 255.0f};
  if (ImGui::ColorEdit3("Warna latar avatar", warnaLatar)) {
    app.aturWarnaLatar(static_cast<int>(warnaLatar[0] * 255.0f + 0.5f),
                       static_cast<int>(warnaLatar[1] * 255.0f + 0.5f),
                       static_cast<int>(warnaLatar[2] * 255.0f + 0.5f));
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Hijau bawaan cocok untuk efek hapus latar (Chroma Key) "
                      "di OBS.");
  bool transparan = app.pengaturan().latarTransparan;
  if (ImGui::Checkbox("Latar transparan", &transparan))
    app.aturLatarTransparan(transparan);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Hapus warna latar agar jendela avatar transparan. "
                      "Cocok untuk OBS Window Capture tanpa chroma key.");

  ImGui::Spacing();
  ImGui::Text("Posisi model: %d, %d",
              app.pengaturan().modelOfsX, app.pengaturan().modelOfsY);
  int posX = app.pengaturan().modelOfsX;
  int posY = app.pengaturan().modelOfsY;
  if (ImGui::SliderInt("Posisi X (kiri-kanan)", &posX, -500, 500))
    app.aturPosisiModel(posX, app.pengaturan().modelOfsY);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Geser model ke kiri atau kanan.");
  if (ImGui::SliderInt("Posisi Y (naik-turun)", &posY, -500, 500))
    app.aturPosisiModel(app.pengaturan().modelOfsX, posY);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Geser model ke atas atau bawah.");
  if (ImGui::Button("Reset posisi model", ImVec2(-1.0f, 30.0f)))
    app.aturPosisiModel(0, 0);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Kembalikan posisi model ke (0, 0).");

  ImGui::Text("FPS avatar: %.1f", app.fpsTerkini());
  if (ImGui::SliderInt("Zoom avatar (%)", &app.pengaturan().zoomAvatar, 50,
                       300))
    app.aturZoomAvatar(app.pengaturan().zoomAvatar);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Memperbesar model tanpa mengubah resolusi render.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // ---- Bagian 4: Kamera wajah (optional) ----
  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("4. Kamera wajah (optional)");
  ImGui::PopFont();
#ifdef _WIN32
  ImGui::TextWrapped(
      "Pelacakan mata melalui kamera belum tersedia di Windows. Gerakan mulut "
      "tetap mengikuti mikrofon, dan model tetap dapat digunakan.");
#else
  ImGui::TextWrapped(
      "Deteksi bukaan mata dengan kamera V4L2. Gerakan mulut hanya mengikuti "
      "mikrofon. Model perlu memiliki parameter ParamEyeLOpen/R. Kamera tidak mengubah "
      "posisi atau zoom avatar. "
      "DroidCam harus sudah aktif dan terdeteksi Linux sebagai kamera V4L2. "
      "Estimasi ringan ini bukan face tracking AI; pastikan wajah terang dan "
      "berada di tengah gambar.");

  // Deteksi perangkat video V4L2 (termasuk kamera handphone via USB yang muncul
  // sebagai /dev/videoX). Di-cache agar tidak memindai /dev/video* tiap frame.
  if (!perangkatSegar_) {
    perangkatTersedia_ = CameraTracker::daftarPerangkat(true);
    perangkatSegar_ = true;

    perangkatTerpilih_ = 0;
    for (int i = 0; i < static_cast<int>(perangkatTersedia_.size()); ++i) {
      if (perangkatTersedia_[i].path == app.pengaturan().kameraPerangkat) {
        perangkatTerpilih_ = i;
        break;
      }
    }
  }

  if (perangkatTersedia_.empty()) {
    ImGui::TextWrapped(
        "Kamera V4L2 yang kompatibel belum ditemukan. Jalankan DroidCam di "
        "HP, sambungkan ke komputer, lalu segarkan daftar.");
  } else {
    const PerangkatKamera& pilihan = perangkatTersedia_[perangkatTerpilih_];
    const std::string labelPilihan =
        pilihan.nama + " (" + pilihan.path + ")";
    if (ImGui::BeginCombo("Perangkat kamera", labelPilihan.c_str())) {
      for (int i = 0; i < static_cast<int>(perangkatTersedia_.size()); ++i) {
        const PerangkatKamera& perangkat = perangkatTersedia_[i];
        const std::string label =
            perangkat.nama + " (" + perangkat.path + ")";
        const bool isSelected = perangkatTerpilih_ == i;
        if (ImGui::Selectable(label.c_str(), isSelected)) {
          perangkatTerpilih_ = i;
          app.pengaturan().kameraPerangkat = perangkat.path;
          if (app.kameraAktif()) app.mulaiKamera(perangkat.path);
        }
        if (isSelected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Pilih kamera V4L2. DroidCam biasanya muncul di sini dengan nama "
          "perangkat dan jalur /dev/videoN.");
    ImGui::SameLine();
  }
  if (ImGui::SmallButton("Segarkan##Kamera")) {
    perangkatSegar_ = false;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "Pindai ulang perangkat kamera V4L2 sekarang, termasuk DroidCam yang "
        "baru disambungkan.");
  if (app.kameraAktif()) {
    if (ImGui::Button("Hentikan kamera", ImVec2(-1.0f, 36.0f)))
      app.berhentiKamera();
    const DataKamera kd = app.dataKamera();
    ImGui::Spacing();
    ImGui::ProgressBar(kd.eyeOpen, ImVec2(-1.0f, 0.0f), "Kedap mata");
  } else if (!perangkatTersedia_.empty()) {
    if (ImGui::Button("Mulai kamera", ImVec2(-1.0f, 36.0f))) {
      app.pengaturan().kameraPerangkat =
          perangkatTersedia_[perangkatTerpilih_].path;
      app.mulaiKamera(app.pengaturan().kameraPerangkat);
    }
  }
  ImGui::TextWrapped("%s", app.statusKamera().c_str());
#endif

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PushFont(fontTebal_);
  ImGui::TextUnformatted("5. Koneksi OBS");
  ImGui::PopFont();
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputText("Alamat OBS", obsHost_, sizeof(obsHost_)))
    app.pengaturan().obsHost = obsHost_;
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::InputInt("Port obs-websocket", &app.pengaturan().obsPort))
    app.pengaturan().obsPort =
        std::clamp(app.pengaturan().obsPort, 1, 65535);
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputText("Password OBS", obsPassword_, sizeof(obsPassword_),
                   ImGuiInputTextFlags_Password);
  if (app.obsTerhubung()) {
    if (ImGui::Button("Putuskan OBS", ImVec2(-1.0f, 36.0f)))
      app.putusObs();
  } else if (ImGui::Button("Hubungkan ke OBS", ImVec2(-1.0f, 36.0f))) {
    app.sambungObs(obsPassword_);
    std::fill(std::begin(obsPassword_), std::end(obsPassword_), '\0');
  }
  ImGui::TextWrapped("%s", app.statusObs().c_str());
  if (!app.sceneObs().empty())
    ImGui::TextWrapped("Scene aktif OBS: %s", app.sceneObs().c_str());
  ImGui::TextWrapped(
      "Untuk gambar avatar, tambahkan sumber Window Capture di OBS dan pilih "
      "jendela Avatar Studio. Tracking tampil sinkron karena OBS menangkap "
      "jendela avatar langsung.");

  ImGui::Spacing();

  ImGui::Spacing();

  if (ImGui::Button("Keluar dari aplikasi")) app.mintaKeluar();

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ---------------------------------------------------------------------------
// Menu klik-kanan jendela avatar — hanya digambar saat menu terbuka
// ---------------------------------------------------------------------------
void Gui::frameOverlay(App& app) {
  if (!ctxOverlay_) return;
  ImGui::SetCurrentContext(ctxOverlay_);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  if (permintaanMenu_) {
    ImGui::OpenPopup("MenuAvatar");
    permintaanMenu_ = false;
  }

  if (ImGui::BeginPopup("MenuAvatar")) {
    if (ImGui::MenuItem("Buka jendela pengaturan"))
      app.mintaBukaJendelaKontrol();
    ImGui::Separator();
    if (ImGui::MenuItem("Keluar")) app.mintaKeluar();
    ImGui::EndPopup();
  }

  menuTerbuka_ = ImGui::IsPopupOpen("MenuAvatar");

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Gui::shutdown() {
  // Konteks kontrol lebih dulu, lalu overlay — masing-masing dengan backend-nya.
  if (ctxKontrol_) {
    ImGui::SetCurrentContext(ctxKontrol_);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(ctxKontrol_);
    ctxKontrol_ = nullptr;
  }
  if (ctxOverlay_) {
    ImGui::SetCurrentContext(ctxOverlay_);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(ctxOverlay_);
    ctxOverlay_ = nullptr;
  }
  winKontrol_ = nullptr;
  winOverlay_ = nullptr;
}
