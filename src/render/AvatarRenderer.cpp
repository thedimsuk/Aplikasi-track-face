#include "render/AvatarRenderer.hpp"

#include <SDL_opengl.h>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>

bool AvatarRenderer::muatModel(const std::string& model3Path,
                               const std::string& folderModel) {
  return model_.muatModel(model3Path, folderModel);
}

void AvatarRenderer::perbarui(float deltaDetik, float tingkatSuara) {
  model_.aturMulut(tingkatSuara);
  model_.perbarui(deltaDetik);
}

void AvatarRenderer::terapkanKamera(float mata) {
  model_.aturMata(mata);
}

void AvatarRenderer::resetKamera() { model_.resetKamera(); }

void AvatarRenderer::bersihkanModel() { model_.buang(); }

void AvatarRenderer::setWarnaLatar(int r, int g, int b) {
  r_ = r / 255.0f;
  g_ = g / 255.0f;
  b_ = b / 255.0f;
}

void AvatarRenderer::setLatarTransparan(bool transparan) {
  transparan_ = transparan;
}

void AvatarRenderer::setPosisiModel(float x, float y) {
  model_.aturPosisi(x, y);
}

void AvatarRenderer::render(int lebar, int tinggi) {
  glViewport(0, 0, lebar, tinggi);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_SCISSOR_TEST);
  if (transparan_)
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  else
    glClearColor(r_, g_, b_, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Mulai frame offscreen manager sebelum menggambar model (wajib untuk
  // clipping mask / offscreen buffer — pola contoh resmi Live2D).
  Live2D::Cubism::Framework::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance()->BeginFrameProcess();

  model_.gambar(lebar, tinggi);

  // Selesaikan frame offscreen manager setelah menggambar semua model.
  auto* offscreenManager =
      Live2D::Cubism::Framework::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance();
  offscreenManager->EndFrameProcess();
  offscreenManager->ReleaseStaleRenderTextures();
}
