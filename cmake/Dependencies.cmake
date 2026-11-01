# Dependensi pihak ketiga:
#   1. Dear ImGui      — diambil otomatis lewat FetchContent, TAG TETAP agar hasil build stabil.
#   2. Font Lato (OFL) — dibundel di resources/fonts; diunduh hanya bila belum ada.
#   3. Cubism SDK      — TIDAK pernah diunduh otomatis (lisensi Live2D). Hanya dideteksi.
include(FetchContent)

# ---------------------------------------------------------------------------
# Dear ImGui — tag dikunci
# ---------------------------------------------------------------------------
set(AVATAR_IMGUI_TAG "v1.91.9" CACHE STRING "Tag Dear ImGui yang dipakai")
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        ${AVATAR_IMGUI_TAG}
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(avatar-imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)

target_include_directories(avatar-imgui PUBLIC
  "${imgui_SOURCE_DIR}"
  "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(avatar-imgui PUBLIC ${AVATAR_SDL2_TARGET} OpenGL::GL)

# Header SDL2 bisa berada di include/ atau include/SDL2 tergantung paket.
find_path(AVATAR_SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
if(AVATAR_SDL2_INCLUDE_DIR)
  target_include_directories(avatar-imgui PUBLIC "${AVATAR_SDL2_INCLUDE_DIR}")
endif()

# ---------------------------------------------------------------------------
# Font bundel (Lato, lisensi SIL OFL — boleh disertakan di repositori)
# ---------------------------------------------------------------------------
set(AVATAR_FONT_DIR "${CMAKE_SOURCE_DIR}/resources/fonts")
file(MAKE_DIRECTORY "${AVATAR_FONT_DIR}")

function(avatar_unduh_font nama url)
  set(_tujuan "${AVATAR_FONT_DIR}/${nama}")
  if(NOT EXISTS "${_tujuan}")
    message(STATUS "Mengunduh font ${nama} ...")
    file(DOWNLOAD "${url}" "${_tujuan}" STATUS _status)
    list(GET _status 0 _kode)
    if(NOT _kode EQUAL 0)
      file(REMOVE "${_tujuan}")
      message(WARNING
        "Gagal mengunduh font ${nama}.\n"
        "Aplikasi tetap memakai font sistem (DejaVu Sans) bila font bundel tidak ada.")
    endif()
  endif()
endfunction()

avatar_unduh_font("Lato-Regular.ttf"
  "https://raw.githubusercontent.com/google/fonts/main/ofl/lato/Lato-Regular.ttf")
avatar_unduh_font("Lato-Bold.ttf"
  "https://raw.githubusercontent.com/google/fonts/main/ofl/lato/Lato-Bold.ttf")

# ---------------------------------------------------------------------------
# Cubism SDK for Native — deteksi saja, tidak pernah disertakan/diunduh.
# Live2D meminta pengguna mengunduh SDK sendiri dari situs resmi (lihat README).
# ---------------------------------------------------------------------------
set(CUBISM_SDK_ROOT "" CACHE PATH
  "Folder berisi CubismSdk-Native (hasil unduhan manual dari live2d.com)")

set(_cubism_kandidat
  "${CUBISM_SDK_ROOT}"
  "$ENV{CUBISM_SDK_ROOT}"
  "${CMAKE_SOURCE_DIR}/third_party/CubismSdk-Native"
)
set(CUBISM_SDK_DIR "")
foreach(_k IN LISTS _cubism_kandidat)
  if(_k AND EXISTS "${_k}/Core/include/Live2DCubismCore.h")
    set(CUBISM_SDK_DIR "${_k}")
    break()
  endif()
endforeach()

if(CUBISM_SDK_DIR)
  set(AVATAR_CUBISM_CORE_LIBRARY "")
  if(WIN32 AND MSVC)
    set(_cubism_msvc_toolset "${MSVC_TOOLSET_VERSION}")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(_cubism_arch x86_64)
    else()
      set(_cubism_arch x86)
    endif()
    set(AVATAR_CUBISM_CORE_LIBRARY
      "${CUBISM_SDK_DIR}/Core/lib/windows/${_cubism_arch}/${_cubism_msvc_toolset}/Live2DCubismCore_MD.lib")
  elseif(UNIX)
    set(AVATAR_CUBISM_CORE_LIBRARY
      "${CUBISM_SDK_DIR}/Core/lib/linux/x86_64/libLive2DCubismCore.a")
  endif()

  if(AVATAR_CUBISM_CORE_LIBRARY AND EXISTS "${AVATAR_CUBISM_CORE_LIBRARY}")
    message(STATUS "Cubism SDK ditemukan: ${CUBISM_SDK_DIR}")
    set(AVATAR_HAVE_CUBISM TRUE)
    add_library(avatar-cubism-core STATIC IMPORTED)
    set_target_properties(avatar-cubism-core PROPERTIES
      IMPORTED_LOCATION "${AVATAR_CUBISM_CORE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CUBISM_SDK_DIR}/Core/include"
    )

    set(FRAMEWORK_SOURCE OpenGL)
    add_subdirectory("${CUBISM_SDK_DIR}/Framework"
                     "${CMAKE_CURRENT_BINARY_DIR}/cubism-framework")
    target_link_libraries(Framework PUBLIC avatar-cubism-core)

    if(WIN32)
      find_package(GLEW CONFIG REQUIRED)
      target_compile_definitions(Framework PUBLIC
        CSM_TARGET_WIN_GL NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
      target_link_libraries(Framework PUBLIC GLEW::GLEW opengl32)
    else()
      target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
      target_include_directories(Framework PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/glew-shim")
    endif()

    set(AVATAR_CUBISM_SHADER_SRC
        "${CUBISM_SDK_DIR}/Framework/src/Rendering/OpenGL/Shaders/Standard")
    set(AVATAR_CUBISM_SHADER_DST
        "${CMAKE_CURRENT_BINARY_DIR}/FrameworkShaders")
    add_custom_target(avatar-cubism-shader ALL
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${AVATAR_CUBISM_SHADER_SRC}" "${AVATAR_CUBISM_SHADER_DST}"
      COMMENT "Menyalin shader Cubism ke folder build")
  else()
    set(AVATAR_HAVE_CUBISM FALSE)
    message(WARNING
      "Library Cubism SDK yang cocok dengan platform/compiler tidak ditemukan. "
      "Build dilanjutkan tanpa pemuatan model Live2D.")
  endif()
else()
  set(AVATAR_HAVE_CUBISM FALSE)
  message(WARNING
    "Cubism SDK for Native BELUM ditemukan.\n"
    "Aplikasi tetap bisa dibangun, tetapi model Live2D belum bisa dimuat.\n"
    "Cara mengunduh dan menaruh SDK ada di README.md (bagian 'Cubism SDK').\n"
    "Setelah diunduh, jalankan cmake dengan -DCUBISM_SDK_ROOT=/path/ke/CubismSdk-Native")
endif()
