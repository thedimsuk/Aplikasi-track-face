#pragma once

#include "render/ModelLive2D.hpp"

#include <string>

// ---------------------------------------------------------------------------
// AvatarRenderer — menggambar isi jendela avatar.
// ---------------------------------------------------------------------------
class AvatarRenderer {
public:
  // Warna latar dalam satuan 0–255 (misalnya hijau chroma key OBS).
  void setWarnaLatar(int r, int g, int b);
  // Latar transparan (alpha = 0) bila true, opaque bila false.
  void setLatarTransparan(bool transparan);
  // Posisi model dalam piksel layar (untuk geser-geser).
  void setPosisiModel(float x, float y);
  void setZoom(float zoom) { model_.aturZoom(zoom); }

  bool muatModel(const std::string& model3Path, const std::string& folderModel);
  void perbarui(float deltaDetik, float tingkatSuara);
  void bersihkanModel();

  // Teruskan estimasi mata dari kamera ke model.
  void terapkanKamera(float mata);

  // Nonaktifkan pengaruh kamera tracker pada model.
  void resetKamera();

  bool adaModel() const { return model_.ada(); }
  const std::string& namaModel() const { return model_.nama(); }
  const std::string& pesanGalatModel() const { return model_.pesanGalat(); }

  // Gambar satu frame. ukuran piksel = ukuran drawable jendela (HiDPI).
  void render(int lebar, int tinggi);

private:
  float r_ = 0.0f, g_ = 0.694f, b_ = 0.251f; // hijau bawaan
  bool  transparan_ = false;                 // latar transparan (alpha = 0)
  ModelLive2D model_;
};
