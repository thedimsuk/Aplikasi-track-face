// ---------------------------------------------------------------------------
// Selubung (shim) header GLEW lokal untuk Cubism Framework di Linux.
//
// Framework resmi Live2D menyertakan <GL/glew.h> pada cabang
// CSM_TARGET_LINUX_GL. Audit sumber Framework (SDK 5-r.5) menunjukkan
// pemakaian GLEW terbatas pada:
//   1. include <GL/glew.h> (+ <GL/gl.h>),
//   2. macro GLEW_ARB_texture_barrier / GLEW_NV_texture_barrier yang hanya
//      dibaca (tiap cabang punya fallback tanpa texture barrier),
//   3. fungsi glTextureBarrier() yang hanya dipanggil di dalam cabang
//      macro di atas (tidak pernah tercapai bila macro bernilai 0).
// Tidak ada pemanggilan glewInit() di dalam Framework (hanya di contoh demo).
//
// Semua fungsi OpenGL 3.3 core yang dipakai Framework sudah diekspor
// langsung oleh Mesa (libGL), sehingga selubung ini cukup:
//   * menyertakan <GL/gl.h> sistem,
//   * mendefinisikan kedua macro sebagai 0,
//   * menyediakan deklarasi glTextureBarrier() yang tidak pernah dipanggil.
//
// Dengan ini pengguna TIDAK perlu memasang paket libglew-dev (hemat).
// ---------------------------------------------------------------------------
#pragma once

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#ifndef GLEW_ARB_texture_barrier
#define GLEW_ARB_texture_barrier 0
#endif

#ifndef GLEW_NV_texture_barrier
#define GLEW_NV_texture_barrier 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Dideklarasikan agar kompilasi lolos; tidak pernah dipanggil karena
// kedua macro di atas selalu 0 (ada jalur fallback tanpa texture barrier).
void glTextureBarrier(void);

#ifdef __cplusplus
}
#endif
