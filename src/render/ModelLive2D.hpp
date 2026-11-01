#pragma once

#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// ModelLive2D — pembungkus satu model Live2D (Cubism SDK resmi).
//
// Tanggung jawab:
//   * memuat .model3.json, .moc3, dan tekstur,
//   * menjaga arah kepala tetap ke depan dan menggerakkan mulut dari audio,
//   * menggambar model dengan renderer resmi Cubism.
//
// Bila SDK belum ada saat build (AVATAR_HAVE_CUBISM tidak didefinisikan),
// berkas ini menyediakan kelas pengganti (stub) agar aplikasi tetap bisa
// dibangun — tetapi muatModel() selalu mengembalikan pesan galat yang jelas.
// ---------------------------------------------------------------------------
class ModelLive2D {
 public:
  ModelLive2D();
  ~ModelLive2D();

  // Non-copyable (model memegang resource GPU + SDK).
  ModelLive2D(const ModelLive2D&) = delete;
  ModelLive2D& operator=(const ModelLive2D&) = delete;

  // Muat model dari folder. model3Path = jalur lengkap berkas .model3.json,
  // folderModel = folder yang memuatnya (untuk TextureManager bawaan).
  // Kembalikan true bila berhasil; bila gagal, alasan ramah-untuk-awam
  // bisa dibaca lewat pesanGalat().
  bool muatModel(const std::string& model3Path, const std::string& folderModel);

  // Cari berkas .model3.json pertama di folder atau subfoldernya. Kembalikan
  // jalur lengkap atau string kosong.
  // Dipakai untuk drag & drop folder: pengguna cukup menyeret folder model.
  static std::string cariModel3Json(const std::string& folder);

  // Periksa kelengkapan folder model SEBELUM memuat (ringan, tanpa GL).
  // Kembalikan string kosong bila lengkap, atau pesan galat Bahasa Indonesia
  // yang menjelaskan apa yang kurang dan cara memperbaikinya.
  static std::string periksaFolder(const std::string& folder);

  // Bebaskan model dari memori + GPU.
  void buang();

  // true bila ada model yang berhasil dimuat dan siap digambar.
  bool ada() const { return ada_; }

  // Nama model (nama berkas .model3.json tanpa ekstensi), untuk status GUI.
  const std::string& nama() const { return nama_; }

  // Pesan galat terakhir (Bahasa Indonesia, ramah untuk orang awam).
  const std::string& pesanGalat() const { return galat_; }

  // Perbarui animasi (panggil tiap frame SEBELUM gambar).
  // deltaDetik = waktu sejak frame sebelumnya (mis. 1/30).
  void perbarui(float deltaDetik);

  // Level audio 0..1; mengendalikan parameter buka mulut standar Cubism.
  void aturMulut(float tingkatSuara);
  void aturZoom(float zoom);
  // Geser model dalam piksel (positif = ke kanan/bawah).
  void aturPosisi(float x, float y);

  // --- Parameter mata dari kamera (optional) ----------------------------
  // Kedap mata dari kamera: 0 (tertutup) .. 1 (terbuka lebar).
  void aturMata(float terbuka);
  // Nonaktifkan pengaruh kamera pada mata (kembali ke kedip otomatis).
  void resetKamera();

  // Gambar model. lebar/tinggi = ukuran piksel jendela avatar.
  // Latar sudah dibersihkan oleh AvatarRenderer sebelum ini dipanggil.
  void gambar(int lebar, int tinggi);

 private:
  bool ada_ = false;
  std::string nama_;
  std::string galat_;

  // Waktu animasi internal (detik) — untuk kedip otomatis cadangan.
  float waktu_ = 0.0f;
  float zoom_ = 1.0f;
  float ofsX_ = 0.0f;
  float ofsY_ = 0.0f;

#ifdef AVATAR_HAVE_CUBISM
  // Pointer buram ke implementasi (PIMPL) agar header SDK tidak bocor
  // ke berkas lain. Didefinisikan di ModelLive2D.cpp.
  struct Rincian;
  Rincian* rincian_ = nullptr;
#endif
};
