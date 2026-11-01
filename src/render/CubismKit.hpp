#pragma once

// ---------------------------------------------------------------------------
// CubismKit — pengelola global Cubism SDK untuk aplikasi ini.
//
// Tanggung jawab:
//   * menyalakan (StartUp + Initialize) dan mematikan (Dispose) Framework,
//   * menyediakan allocator + fungsi baca berkas untuk Framework,
//   * menyimpan folder shader Framework ("FrameworkShaders/...").
//
// Pola ini mengikuti contoh resmi Live2D (LAppDelegate::InitializeCubism).
// Dipanggil dari AvatarRenderer: mulai() sekali setelah konteks GL avatar
// aktif, dan berhenti() saat aplikasi ditutup.
// ---------------------------------------------------------------------------
class CubismKit {
 public:
  // Nyalakan Framework. folderShader = folder yang berisi subfolder
  // "FrameworkShaders" hasil salinan build (atau SDK). Aman dipanggil
  // berulang (hanya yang pertama berpengaruh). Kembalikan false bila SDK
  // tidak ada saat build — dengan begitu renderer memberi pesan jelas.
  static bool mulai(const char* folderShader);

  // Matikan Framework. Aman dipanggil walau mulai() belum/gagal.
  static void berhenti();

  // true bila Framework sudah menyala dan siap dipakai.
  static bool menyala() { return menyala_; }

  // Folder shader untuk LoadFileFunction (kosong bila belum mulai()).
  static const char* folderShader();

 private:
  CubismKit() = delete;

  static bool menyala_;
};
