// ---------------------------------------------------------------------------
// CubismKit — implementasi pengelola global Cubism SDK.
//
// Tanpa SDK (AVATAR_HAVE_CUBISM tidak didefinisikan): semua fungsi menjadi
// stub yang aman — mulai() mengembalikan false agar lapisan atas bisa
// menampilkan pesan "SDK belum dipasang, lihat README".
// ---------------------------------------------------------------------------
#include "render/CubismKit.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>

#include <SDL.h>

bool CubismKit::menyala_ = false;
namespace {
std::string g_folderShader;
}  // namespace

#ifdef AVATAR_HAVE_CUBISM

#include <CubismFramework.hpp>

namespace Csm = Live2D::Cubism::Framework;

// Alokasi memori untuk Framework — mengikuti contoh resmi (malloc/free).
class AlokasiAvatar : public Csm::ICubismAllocator {
 public:
  void* Allocate(const Csm::csmSizeType ukuran) override {
    return std::malloc(ukuran);
  }
  void Deallocate(void* memori) override { std::free(memori); }
  void* AllocateAligned(const Csm::csmSizeType ukuran,
                        const Csm::csmUint32 perataan) override {
    // Alokasi sejajar sederhana (pola contoh resmi LAppAllocator).
    const std::size_t offset = perataan - 1 + sizeof(void*);
    void* alok = Allocate(ukuran + static_cast<Csm::csmSizeType>(offset));
    if (!alok) return nullptr;
    std::size_t alamat =
        reinterpret_cast<std::size_t>(alok) + sizeof(void*);
    const std::size_t sisa = alamat % perataan;
    if (sisa) alamat += (perataan - sisa);
    (reinterpret_cast<void**>(alamat))[-1] = alok;
    return reinterpret_cast<void*>(alamat);
  }
  void DeallocateAligned(void* memoriSejajar) override {
    if (!memoriSejajar) return;
    void** awal = static_cast<void**>(memoriSejajar);
    Deallocate(awal[-1]);
  }
};

namespace {

// Baca seluruh isi berkas sebagai byte (untuk Framework + model + shader).
Csm::csmByte* muatBerkas(std::string p, Csm::csmSizeInt* ukuran) {
  if (!ukuran) return nullptr;

  // Shader Framework diminta dengan jalur relatif ("FrameworkShaders/..."):
  // arahkan ke folder shader hasil salinan build.
  const std::string awalan = "FrameworkShaders/";
  if (p.compare(0, awalan.size(), awalan) == 0 && !g_folderShader.empty()) {
    p = g_folderShader + "/" + p;
  }

  std::ifstream berkas(std::filesystem::u8path(p),
                       std::ios::binary | std::ios::ate);
  if (!berkas) {
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cubism tidak dapat membaca: %s",
                 p.c_str());
    return nullptr;
  }
  const std::streamsize besar = berkas.tellg();
  if (besar <= 0 || besar > 100 * 1024 * 1024) {
    SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                 "Ukuran berkas Cubism tidak valid (%lld byte): %s",
                 static_cast<long long>(besar), p.c_str());
    return nullptr;
  }
  berkas.seekg(0, std::ios::beg);
  char* buf = new (std::nothrow) char[static_cast<std::size_t>(besar)];
  if (!buf) {
    SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                 "Memori tidak cukup untuk membaca berkas Cubism: %s",
                 p.c_str());
    return nullptr;
  }
  if (!berkas.read(buf, besar)) {
    delete[] buf;
    SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Gagal membaca berkas Cubism: %s",
                 p.c_str());
    return nullptr;
  }
  *ukuran = static_cast<Csm::csmSizeInt>(besar);
  return reinterpret_cast<Csm::csmByte*>(buf);
}

void lepasBerkas(Csm::csmByte* data) {
  delete[] reinterpret_cast<char*>(data);
}

void catatPesan(const Csm::csmChar* pesan) {
  SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,
                 "[Cubism] %s", pesan ? pesan : "");
}

AlokasiAvatar g_alokasi;
Csm::CubismFramework::Option g_opsi;

}  // namespace

bool CubismKit::mulai(const char* folderShader) {
  if (menyala_) return true;
  if (folderShader) g_folderShader = folderShader;
  g_opsi.LogFunction = catatPesan;
  g_opsi.LoggingLevel = Csm::CubismFramework::Option::LogLevel_Verbose;
  g_opsi.LoadFileFunction = muatBerkas;
  g_opsi.ReleaseBytesFunction = lepasBerkas;
  if (!Csm::CubismFramework::StartUp(&g_alokasi, &g_opsi)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "CubismFramework::StartUp gagal.");
    return false;
  }
  Csm::CubismFramework::Initialize();
  menyala_ = true;
  SDL_Log("Cubism Framework berhasil diinisialisasi.");
  return true;
}

void CubismKit::berhenti() {
  if (!menyala_) return;
  Csm::CubismFramework::Dispose();
  menyala_ = false;
}

const char* CubismKit::folderShader() { return g_folderShader.c_str(); }

#else  // ---- Tanpa SDK: stub aman ----

bool CubismKit::mulai(const char*) { return false; }
void CubismKit::berhenti() {}
const char* CubismKit::folderShader() { return ""; }

#endif
