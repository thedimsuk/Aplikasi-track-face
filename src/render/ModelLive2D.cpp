#include "render/ModelLive2D.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <vector>

#include <SDL.h>

namespace fs = std::filesystem;

namespace {

bool bacaBerkas(const fs::path& jalur, std::vector<unsigned char>& isi) {
  std::ifstream berkas(jalur, std::ios::binary | std::ios::ate);
  if (!berkas) return false;
  const std::streamoff ukuran = berkas.tellg();
  if (ukuran <= 0 || ukuran > 256LL * 1024 * 1024) return false;
  isi.resize(static_cast<size_t>(ukuran));
  berkas.seekg(0);
  return static_cast<bool>(
      berkas.read(reinterpret_cast<char*>(isi.data()), ukuran));
}

bool model3(const fs::directory_entry& entri) {
  if (!entri.is_regular_file()) return false;
  const std::string nama = entri.path().filename().string();
  constexpr const char* akhiran = ".model3.json";
  constexpr size_t panjang = sizeof(".model3.json") - 1;
  return nama.size() > panjang &&
         nama.compare(nama.size() - panjang, panjang, akhiran) == 0;
}

}  // namespace

std::string ModelLive2D::cariModel3Json(const std::string& folder) {
  std::error_code ec;
  const fs::path dasar = fs::u8path(folder);
  if (!fs::is_directory(dasar, ec)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Pencarian model gagal; bukan folder atau tidak dapat dibaca: %s (%s)",
                 folder.c_str(), ec ? ec.message().c_str() : "bukan direktori");
    return {};
  }

  for (fs::directory_iterator it(dasar, ec), akhir; !ec && it != akhir;
       it.increment(ec)) {
    std::error_code itemError;
    if (model3(*it)) {
      SDL_Log("Model3 ditemukan di tingkat folder langsung: %s",
              it->path().u8string().c_str());
      return it->path().u8string();
    }
  }

  ec.clear();
  fs::recursive_directory_iterator it(
      dasar, fs::directory_options::skip_permission_denied, ec);
  const fs::recursive_directory_iterator akhir;
  for (; !ec && it != akhir; it.increment(ec)) {
    std::error_code itemError;
    if (model3(*it)) {
      SDL_Log("Model3 ditemukan dalam subfolder: %s",
              it->path().u8string().c_str());
      return it->path().u8string();
    }
  }
  if (ec)
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Pencarian rekursif model terhenti: %s", ec.message().c_str());
  return {};
}

std::string ModelLive2D::periksaFolder(const std::string& folder) {
  std::error_code ec;
  const fs::path dasar = fs::u8path(folder);
  if (folder.empty())
    return "Belum ada folder yang dipilih. Klik \"Pilih folder model...\" "
           "atau seret folder model ke jendela avatar.";
  if (!fs::exists(dasar, ec))
    return "Folder tidak ditemukan:\n" + folder +
           "\n\nMungkin folder dipindah atau dihapus. "
           "Pilih ulang folder model yang benar.";
  if (!fs::is_directory(dasar, ec))
    return "Yang dipilih adalah berkas, bukan folder.\n\n"
           "Pilih folder model (folder yang di dalamnya ada berkas "
           "berakhiran .model3.json), bukan salah satu berkas di dalamnya.";

  const std::string jalurModel = cariModel3Json(folder);
  if (jalurModel.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Tidak menemukan berkas .model3.json di folder: %s",
                 folder.c_str());
    return "Tidak ada berkas .model3.json di folder ini.\n\n"
           "Pilih folder model Live2D yang benar — di dalamnya harus ada "
           "berkas berakhiran .model3.json, termasuk di subfoldernya.";
  }

  std::vector<unsigned char> json;
  if (!bacaBerkas(fs::u8path(jalurModel), json)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Tidak dapat membaca berkas model JSON: %s",
                 jalurModel.c_str());
    return "Berkas .model3.json tidak bisa dibaca. Mungkin rusak.\n\n"
           "Coba unduh ulang model, lalu pilih lagi.";
  }

  const std::string teks(json.begin(), json.end());
  const size_t posisiMoc = teks.find(".moc3");
  if (posisiMoc == std::string::npos) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Model JSON tidak mencantumkan rujukan .moc3: %s",
                 jalurModel.c_str());
    return "Berkas .model3.json tidak merujuk ke berkas .moc3.\n\n"
           "Mungkin model belum lengkap. Coba unduh ulang model.";
  }

  const size_t kutipBuka = teks.rfind('"', posisiMoc);
  const size_t kutipTutup = teks.find('"', posisiMoc);
  if (kutipBuka == std::string::npos || kutipTutup == std::string::npos)
    return "Rujukan berkas .moc3 di .model3.json tidak valid.\n\n"
           "Coba unduh ulang model, lalu pilih lagi.";

  const std::string rel =
      teks.substr(kutipBuka + 1, kutipTutup - kutipBuka - 1);
  if (rel.empty() || rel.size() > 512)
    return "Jalur berkas .moc3 di .model3.json tidak valid.\n\n"
           "Coba unduh ulang model, lalu pilih lagi.";

  std::error_code mocError;
  const fs::path lokasiModel = fs::u8path(jalurModel).parent_path();
  if (!fs::is_regular_file(lokasiModel / fs::u8path(rel), mocError)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Berkas moc tidak ditemukan: %s",
                 (lokasiModel / fs::u8path(rel)).u8string().c_str());
    return "Berkas .moc3 tidak ketemu di folder model.\n\n"
           "Dicari: " + rel + "\n"
           "Pastikan seluruh isi folder model ikut tersalin — jangan hanya "
           "berkas .model3.json-nya saja.";
  }
  SDL_Log("Validasi model berhasil: %s", jalurModel.c_str());
  return {};
}

#ifndef AVATAR_HAVE_CUBISM

ModelLive2D::ModelLive2D() = default;
ModelLive2D::~ModelLive2D() = default;

bool ModelLive2D::muatModel(const std::string&, const std::string&) {
  galat_ =
      "Cubism SDK belum ditemukan saat aplikasi dibangun. Pasang SDK resmi "
      "Live2D, lalu bangun ulang aplikasi.";
  return false;
}

void ModelLive2D::buang() { ada_ = false; }
void ModelLive2D::perbarui(float) {}
void ModelLive2D::aturZoom(float zoom) {
  zoom_ = std::clamp(zoom, 0.5f, 3.0f);
}
void ModelLive2D::gambar(int, int) {}
void ModelLive2D::aturPosisi(float, float) {}

void ModelLive2D::aturMata(float) {}
void ModelLive2D::resetKamera() {}

#else

#include <CubismFramework.hpp>
#include <CubismModelSettingJson.hpp>
#include <CubismDefaultParameterId.hpp>
#include <Id/CubismIdManager.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Model/CubismModel.hpp>
#include <Model/CubismUserModel.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL_opengl.h>

using namespace Live2D::Cubism::Framework;

struct ModelLive2D::Rincian {
  CubismUserModel* model = nullptr;
  CubismModelSettingJson* pengaturan = nullptr;
  std::vector<GLuint> tekstur;
  std::mt19937 generator{std::random_device{}()};
  float waktu = 0.0f;
  float waktuMenujuKedip =
      std::uniform_real_distribution<float>(1.0f, 2.0f)(generator);
  float waktuKedip = -1.0f;
  float tingkatMulut = 0.0f;
  float mulutTerbuka = 0.0f;
  // Parameter mata dari kamera (optional)
  float kameraMata = 1.0f;
  bool kameraAktif = false;
  bool renderPertamaDicatat = false;
  GLenum galatRenderTerakhir = GL_NO_ERROR;
};

ModelLive2D::ModelLive2D() = default;

ModelLive2D::~ModelLive2D() { buang(); }

bool ModelLive2D::muatModel(const std::string& model3Path,
                            const std::string& folderModel) {
  buang();
  galat_.clear();
  SDL_Log("ModelLive2D: membaca JSON %s", model3Path.c_str());

  std::vector<unsigned char> json;
  if (!bacaBerkas(fs::u8path(model3Path), json)) {
    galat_ = "Berkas .model3.json tidak bisa dibaca.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", galat_.c_str(),
                 model3Path.c_str());
    return false;
  }

  auto* rincian = new Rincian;
  rincian_ = rincian;
  rincian->pengaturan = new CubismModelSettingJson(
      reinterpret_cast<const csmByte*>(json.data()),
      static_cast<csmSizeInt>(json.size()));
  const char* namaMoc = rincian->pengaturan->GetModelFileName();
  if (!namaMoc || !*namaMoc) {
    galat_ = "Konfigurasi model tidak mencantumkan berkas .moc3.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", galat_.c_str());
    buang();
    return false;
  }

  const fs::path dasar = fs::u8path(folderModel);
  const fs::path jalurMoc = dasar / fs::u8path(namaMoc);
  SDL_Log("ModelLive2D: membaca moc %s", jalurMoc.u8string().c_str());
  std::vector<unsigned char> moc;
  if (!bacaBerkas(jalurMoc, moc)) {
    galat_ = "Berkas model .moc3 tidak bisa dibaca.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", galat_.c_str(),
                 jalurMoc.u8string().c_str());
    buang();
    return false;
  }

  rincian->model = new CubismUserModel;
  rincian->model->LoadModel(reinterpret_cast<const csmByte*>(moc.data()),
                            static_cast<csmSizeInt>(moc.size()), true);
  CubismModel* model = rincian->model->GetModel();
  if (!model) {
    galat_ = "Cubism gagal membangun model dari berkas .moc3.";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", galat_.c_str());
    buang();
    return false;
  }
  SDL_Log("ModelLive2D: moc terbaca; kanvas %.0f x %.0f, %d drawable.",
          static_cast<double>(model->GetCanvasWidth()),
          static_cast<double>(model->GetCanvasHeight()),
          static_cast<int>(model->GetDrawableCount()));

  SDL_Log("ModelLive2D: menyiapkan renderer dan buffer clipping.");
  rincian->model->CreateRenderer(1024, 1024);
  auto* renderer =
      rincian->model->GetRenderer<Rendering::CubismRenderer_OpenGLES2>();
  if (!renderer) {
    galat_ = "Renderer Live2D gagal dibuat.";
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s", galat_.c_str());
    buang();
    return false;
  }
  SDL_Log("ModelLive2D: renderer siap.");

  rincian->tekstur.resize(
      static_cast<size_t>(rincian->pengaturan->GetTextureCount()), 0);
  for (csmInt32 i = 0; i < rincian->pengaturan->GetTextureCount(); ++i) {
    const char* namaTekstur = rincian->pengaturan->GetTextureFileName(i);
    if (!namaTekstur || !*namaTekstur) continue;

    const fs::path jalurTekstur = dasar / fs::u8path(namaTekstur);
    SDL_Log("ModelLive2D: membaca tekstur %s", jalurTekstur.u8string().c_str());
    int lebar = 0;
    int tinggi = 0;
    int kanal = 0;
    std::vector<unsigned char> dataTekstur;
    unsigned char* piksel = nullptr;
    if (bacaBerkas(jalurTekstur, dataTekstur) &&
        dataTekstur.size() <= static_cast<std::size_t>(
                                  std::numeric_limits<int>::max())) {
      piksel = stbi_load_from_memory(
          dataTekstur.data(), static_cast<int>(dataTekstur.size()), &lebar,
          &tinggi, &kanal, 4);
    }
    if (!piksel) {
      galat_ = "Tekstur model tidak bisa dimuat: " + jalurTekstur.u8string();
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", galat_.c_str());
      buang();
      return false;
    }
    SDL_Log("ModelLive2D: dekode tekstur selesai (%d x %d).", lebar, tinggi);

    GLuint idTekstur = 0;
    glGenTextures(1, &idTekstur);
    glBindTexture(GL_TEXTURE_2D, idTekstur);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lebar, tinggi, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, piksel);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(piksel);
    if (glGetError() != GL_NO_ERROR) {
      if (idTekstur) glDeleteTextures(1, &idTekstur);
      galat_ = "OpenGL gagal mengunggah tekstur model.";
      SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                   "%s (file: %s, ukuran: %d x %d)", galat_.c_str(),
                   jalurTekstur.u8string().c_str(), lebar, tinggi);
      buang();
      return false;
    }
    SDL_Log("ModelLive2D: tekstur berhasil diunggah (%d x %d).", lebar,
            tinggi);
    rincian->tekstur[static_cast<size_t>(i)] = idTekstur;
    renderer->BindTexture(i, idTekstur);
  }
  renderer->IsPremultipliedAlpha(false);
  // Ukuran buffer clipping mask (mata, rias, dll) agar elemen yang dipotong
  // tampil dengan benar — mengikuti pola contoh resmi Live2D.
  renderer->SetDrawableClippingMaskBufferSize(1024.0f, 1024.0f);
  renderer->SetOffscreenClippingMaskBufferSize(1024.0f, 1024.0f);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Atur matriks model dari informasi layout (pola contoh resmi Live2D)
  csmMap<csmString, csmFloat32> layout;
  if (rincian->pengaturan->GetLayoutMap(layout)) {
    rincian->model->GetModelMatrix()->SetupFromLayout(layout);
  }

  nama_ = fs::u8path(model3Path).stem().stem().u8string();
  // Simpan kondisi awal parameter (pola contoh resmi: SaveParameters
  // dipanggil sekali setelah model selesai dimuat).
  model->SaveParameters();
  ada_ = true;
  SDL_Log("ModelLive2D: selesai memuat %s.", nama_.c_str());
  return true;
}

void ModelLive2D::buang() {
  if (!rincian_) {
    ada_ = false;
    return;
  }
  for (GLuint& tekstur : rincian_->tekstur) {
    if (tekstur) glDeleteTextures(1, &tekstur);
    tekstur = 0;
  }
  // Destruktor CubismUserModel resmi sudah memanggil DeleteRenderer()
  // sendiri — jangan dipanggil manual di sini (panggilan ganda).
  delete rincian_->model;
  delete rincian_->pengaturan;
  delete rincian_;
  rincian_ = nullptr;
  ada_ = false;
  nama_.clear();
}

void ModelLive2D::aturMulut(float tingkatSuara) {
  if (!rincian_) return;
  rincian_->tingkatMulut = std::clamp(tingkatSuara, 0.0f, 1.0f);
}

void ModelLive2D::aturZoom(float zoom) {
  zoom_ = std::clamp(zoom, 0.5f, 3.0f);
}

void ModelLive2D::aturPosisi(float x, float y) {
  ofsX_ = x;
  ofsY_ = y;
}

void ModelLive2D::aturMata(float terbuka) {
  if (!rincian_) return;
  rincian_->kameraMata = std::clamp(terbuka, 0.0f, 1.0f);
  rincian_->kameraAktif = true;
}

void ModelLive2D::resetKamera() {
  if (!rincian_) return;
  rincian_->kameraAktif = false;
  rincian_->kameraMata = 1.0f;
}

void ModelLive2D::perbarui(float deltaDetik) {
  if (!rincian_ || !rincian_->model) return;
  const float delta = std::clamp(deltaDetik, 0.0f, 0.1f);
  rincian_->waktu += delta;
  const float kecepatanMulut =
      rincian_->tingkatMulut > rincian_->mulutTerbuka ? 18.0f : 8.0f;
  const float perataanMulut = 1.0f - std::exp(-delta * kecepatanMulut);
  rincian_->mulutTerbuka +=
      (rincian_->tingkatMulut - rincian_->mulutTerbuka) * perataanMulut;

  CubismModel* model = rincian_->model->GetModel();
  model->LoadParameters();
  CubismIdManager* id = CubismFramework::GetIdManager();
  // Setiap parameter dicek dulu keberadaannya: tidak semua model punya
  // parameter standar (mis. model tanpa mata = tanpa ParamEyeOpen).
  // Menulis ke ID yang tidak ada = crash di dalam Core.
  const CubismIdHandle idNapas = id->GetId(DefaultParameterId::ParamBreath);
  const CubismIdHandle idMataKiri = id->GetId(DefaultParameterId::ParamEyeLOpen);
  const CubismIdHandle idMataKanan = id->GetId(DefaultParameterId::ParamEyeROpen);
  const CubismIdHandle idMulut = id->GetId("ParamMouthOpenY");
  const float napas = (std::sin(rincian_->waktu * 2.0f) + 1.0f) * 0.5f;
  if (model->GetParameterIndex(idNapas) >= 0)
    model->AddParameterValue(idNapas, napas * 0.5f);

  const bool adaMataKiri = model->GetParameterIndex(idMataKiri) >= 0;
  const bool adaMataKanan = model->GetParameterIndex(idMataKanan) >= 0;
  float buka = rincian_->kameraAktif ? rincian_->kameraMata : 1.0f;
  if (!rincian_->kameraAktif) {
    if (rincian_->waktuKedip < 0.0f) {
      rincian_->waktuMenujuKedip -= delta;
      if (rincian_->waktuMenujuKedip <= 0.0f)
        rincian_->waktuKedip = 0.0f;
    }

    if (rincian_->waktuKedip >= 0.0f) {
      constexpr float durasiTutup = 0.08f;
      constexpr float durasiBuka = 0.10f;
      constexpr float durasiKedip = durasiTutup + durasiBuka;
      const float waktuKedip = rincian_->waktuKedip;
      buka = waktuKedip < durasiTutup
                 ? 1.0f - waktuKedip / durasiTutup
                 : (waktuKedip - durasiTutup) / durasiBuka;
      rincian_->waktuKedip += delta;
      if (rincian_->waktuKedip >= durasiKedip) {
        rincian_->waktuKedip = -1.0f;
        rincian_->waktuMenujuKedip =
            std::uniform_real_distribution<float>(1.0f, 2.0f)(
                rincian_->generator);
      }
    }
  }
  if (adaMataKiri) model->SetParameterValue(idMataKiri, buka);
  if (adaMataKanan) model->SetParameterValue(idMataKanan, buka);
  if (model->GetParameterIndex(idMulut) >= 0)
    model->SetParameterValue(idMulut, rincian_->mulutTerbuka);
  model->Update();
  // Pola contoh resmi: Load -> ubah -> Update -> Save, agar parameter
  // tidak menumpuk antar-frame.
  model->SaveParameters();
}

void ModelLive2D::gambar(int lebar, int tinggi) {
  if (!rincian_ || !rincian_->model || lebar <= 0 || tinggi <= 0) return;
  auto* renderer =
      rincian_->model->GetRenderer<Rendering::CubismRenderer_OpenGLES2>();
  if (!renderer) return;

  CubismModel* model = rincian_->model->GetModel();
  CubismModelMatrix* matriksModel = rincian_->model->GetModelMatrix();

  // ModelMatrix sudah diskalakan ke tinggi 2.0 saat dibuat. Jangan panggil
  // SetWidth/SetHeight di sini: keduanya mengalikan matriks, sehingga skala
  // akan terus menumpuk setiap frame dan model akhirnya keluar dari layar.
  const float rasioJendela =
      static_cast<float>(lebar) / static_cast<float>(tinggi);
  const float lebarKanvas = model->GetCanvasWidth();
  const float tinggiKanvas = model->GetCanvasHeight();
  const float rasioKanvas =
      lebarKanvas > 0.0f && tinggiKanvas > 0.0f
          ? lebarKanvas / tinggiKanvas
          : rasioJendela;

  CubismMatrix44 proyeksi;
  proyeksi.LoadIdentity();
  const float skalaX = std::min(1.0f, rasioJendela / rasioKanvas);
  proyeksi.Scale(skalaX * zoom_, zoom_);
  proyeksi.MultiplyByMatrix(matriksModel);

  // Terjemahan offset geser-geser diterapkan di ruang layar (NDC),
  // SETELAH model diproyeksikan, sehingga tidak mengganggu matriks model
  // bawaan Cubism dan tidak menumpuk antar-frame.
  // Y dibalik karena koordinat layar positif ke bawah.
  const float dxNdc = ofsX_ * 2.0f / lebar;
  const float dyNdc = -ofsY_ * 2.0f / tinggi;
  CubismMatrix44 translasi;
  translasi.LoadIdentity();
  translasi.Translate(dxNdc, dyNdc);
  translasi.MultiplyByMatrix(&proyeksi); // translasi = Translate * proyeksi
  proyeksi = translasi;

  renderer->SetMvpMatrix(&proyeksi);
  renderer->DrawModel();
  const GLenum galatGl = glGetError();
  if (galatGl != GL_NO_ERROR) {
    if (galatGl != rincian_->galatRenderTerakhir) {
      SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                   "OpenGL error setelah menggambar model: 0x%04x",
                   static_cast<unsigned int>(galatGl));
      rincian_->galatRenderTerakhir = galatGl;
    }
  } else {
    rincian_->galatRenderTerakhir = GL_NO_ERROR;
    if (!rincian_->renderPertamaDicatat) {
      SDL_Log("ModelLive2D: frame render pertama berhasil; viewport %d x %d, "
              "zoom %.2f, offset %.1f/%.1f.",
              lebar, tinggi, static_cast<double>(zoom_),
              static_cast<double>(ofsX_), static_cast<double>(ofsY_));
      rincian_->renderPertamaDicatat = true;
    }
  }
}

#endif
