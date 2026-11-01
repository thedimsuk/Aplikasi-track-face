cmake_minimum_required(VERSION 3.16)

# Aplikasi desktop Avatar Studio — proyek C++17 (antarmuka Bahasa Indonesia)
cmake_policy(SET CMP0091 NEW)
project(AvatarStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Build rilis secara bawaan agar hemat resource.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipe build" FORCE)
endif()

# ---------------------------------------------------------------------------
# SDL2 — cari di sistem atau di prefix package manager (mis. vcpkg).
# Linux juga mendukung build lokal dari tools/bangun-sdl2-lokal.sh.
# ---------------------------------------------------------------------------
set(_sdl2_lokal "${CMAKE_SOURCE_DIR}/third_party/sdl2-local")
if(UNIX AND (NOT SDL2_DIR OR SDL2_DIR STREQUAL "SDL2_DIR-NOTFOUND")
   AND EXISTS "${_sdl2_lokal}/lib/cmake/SDL2/SDL2Config.cmake")
  set(SDL2_DIR "${_sdl2_lokal}/lib/cmake/SDL2" CACHE PATH
      "Folder konfigurasi CMake SDL2 lokal" FORCE)
  message(STATUS "SDL2: memakai build lokal di ${_sdl2_lokal}")
endif()

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  message(FATAL_ERROR
    "SDL2 tidak ditemukan.\n"
    "Pasang SDL2 development package untuk platform ini, lalu jalankan cmake lagi.")
endif()

# Target SDL2: SDL2::SDL2 (shared, paket sistem) atau SDL2::SDL2-static
# (build lokal statis). Pilih salah satu yang tersedia.
if(TARGET SDL2::SDL2)
  set(AVATAR_SDL2_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
  set(AVATAR_SDL2_TARGET SDL2::SDL2-static)
else()
  message(FATAL_ERROR
    "Konfigurasi CMake SDL2 tidak menyediakan target yang dikenal "
    "(SDL2::SDL2 / SDL2::SDL2-static).")
endif()

# The local static SDL build may lack PulseAudio headers and therefore expose
# only ALSA. Prefer the system SDL runtime when available so PipeWire/PulseAudio
# capture devices remain accessible without development packages.
if(UNIX AND SDL2_DIR MATCHES "${_sdl2_lokal}")
  find_file(AVATAR_SDL2_RUNTIME
    NAMES libSDL2-2.0.so.0
    PATHS
    "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
    "/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
    NO_DEFAULT_PATH
  )
  if(AVATAR_SDL2_RUNTIME)
    set(AVATAR_SDL2_TARGET "${AVATAR_SDL2_RUNTIME}")
    set(AVATAR_SDL2_INCLUDE_DIR "${_sdl2_lokal}/include/SDL2")
    message(STATUS "SDL2: memakai runtime sistem untuk dukungan PipeWire/PulseAudio")
  endif()
endif()

find_package(OpenGL QUIET)
if(NOT OPENGL_FOUND)
  message(FATAL_ERROR
    "Library OpenGL development tidak ditemukan untuk platform ini.")
endif()

# ImGui (FetchContent, tag tetap), font bundel, dan deteksi Cubism SDK.
include("${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake")

# ---------------------------------------------------------------------------
# Sumber aplikasi
# ---------------------------------------------------------------------------
set(AVATAR_SOURCES
  src/main.cpp
  src/core/App.cpp
  src/core/Settings.cpp
  src/gui/Gui.cpp
  src/obs/ObsWebSocket.cpp
  src/render/AudioMouthTracker.cpp
  src/render/AvatarRenderer.cpp
  src/render/CameraTracker.cpp
  src/render/ModelLive2D.cpp
  src/render/CubismKit.cpp
)

if(WIN32)
  add_executable(avatar-studio WIN32 ${AVATAR_SOURCES})
else()
  add_executable(avatar-studio ${AVATAR_SOURCES})
endif()

target_include_directories(avatar-studio PRIVATE "${CMAKE_SOURCE_DIR}/src")
if(AVATAR_SDL2_INCLUDE_DIR)
  target_include_directories(avatar-studio PRIVATE "${AVATAR_SDL2_INCLUDE_DIR}")
endif()

target_link_libraries(avatar-studio PRIVATE
  ${AVATAR_SDL2_TARGET}
  OpenGL::GL
  avatar-imgui
)

if(AVATAR_HAVE_CUBISM)
  target_compile_definitions(avatar-studio PRIVATE AVATAR_HAVE_CUBISM)
  target_compile_definitions(avatar-studio PRIVATE
    "AVATAR_CUBISM_SHADER_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")
  target_include_directories(avatar-studio PRIVATE
    "${CUBISM_SDK_DIR}/Samples/OpenGL/thirdParty/stb")
  target_link_libraries(avatar-studio PRIVATE Framework avatar-cubism-core)
  add_dependencies(avatar-studio avatar-cubism-shader)
endif()

# Folder resource dipakai sebagai fallback untuk menjalankan aplikasi dari
# folder build. Instalasi mencari resource di sebelah executable terlebih dulu.
target_compile_definitions(avatar-studio PRIVATE
  "AVATAR_RESOURCE_DIR=\"${CMAKE_SOURCE_DIR}/resources\""
)

if(WIN32)
  target_link_libraries(avatar-studio PRIVATE ole32 shell32 ws2_32)
  if(NOT TARGET SDL2::SDL2main)
    message(FATAL_ERROR "SDL2::SDL2main tidak tersedia; target Windows memerlukannya.")
  endif()
  target_link_libraries(avatar-studio PRIVATE SDL2::SDL2main)
  target_sources(avatar-studio PRIVATE resources/avatar-studio.rc)
  target_compile_definitions(avatar-studio PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
  target_compile_options(avatar-studio PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(avatar-studio PRIVATE -Wall -Wextra)
endif()

install(TARGETS avatar-studio RUNTIME DESTINATION bin)
if(UNIX)
  install(FILES resources/avatar-studio.desktop
    DESTINATION share/applications)
  install(FILES resources/avatar-studio-logo.png
    DESTINATION share/icons/hicolor/512x512/apps RENAME asu.png)
endif()
install(FILES resources/avatar-studio.bmp
  DESTINATION share/avatar-studio)
install(DIRECTORY resources/fonts/
  DESTINATION share/avatar-studio/fonts)
if(AVATAR_HAVE_CUBISM)
  install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/FrameworkShaders"
    DESTINATION share/avatar-studio)
endif()

if(WIN32)
  set(CPACK_GENERATOR ZIP)
  set(CPACK_PACKAGE_FILE_NAME
    "AvatarStudio-${PROJECT_VERSION}-Windows-${CMAKE_SYSTEM_PROCESSOR}")
  if(TARGET SDL2::SDL2)
    get_target_property(_sdl2_dll SDL2::SDL2 IMPORTED_LOCATION_RELEASE)
    if(NOT _sdl2_dll AND VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
      find_file(_sdl2_dll SDL2.dll
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        NO_DEFAULT_PATH)
    endif()
    if(_sdl2_dll AND _sdl2_dll MATCHES "\\.dll$")
      install(FILES "${_sdl2_dll}" DESTINATION bin)
    endif()
  endif()
  if(AVATAR_HAVE_CUBISM AND TARGET GLEW::GLEW)
    get_target_property(_glew_dll GLEW::GLEW IMPORTED_LOCATION_RELEASE)
    if(NOT _glew_dll AND VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
      find_file(_glew_dll glew32.dll
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        NO_DEFAULT_PATH)
    endif()
    if(_glew_dll AND _glew_dll MATCHES "\\.dll$")
      install(FILES "${_glew_dll}" DESTINATION bin)
    endif()
  endif()
elseif(UNIX)
  set(CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Avatar Studio")
  set(CPACK_DEBIAN_PACKAGE_SECTION video)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6, libstdc++6, libsdl2-2.0-0, libopengl0")
endif()
set(CPACK_PACKAGE_NAME avatar-studio)
set(CPACK_PACKAGE_VENDOR "Avatar Studio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Avatar Live2D dengan pelacakan wajah dan audio")
include(CPack)
