# Avatar Studio

Aplikasi desktop untuk menampilkan model **Live2D** sebagai avatar VTuber.
Dibuat dengan C++17, SDL2, OpenGL, dan Dear ImGui; antarmuka aplikasi
menggunakan Bahasa Indonesia.

Avatar Studio menyediakan jendela avatar terpisah dari panel pengaturan,
pemuatan model Live2D, gerakan mulut dari mikrofon, kedipan otomatis, serta
koneksi kontrol OBS. Pelacakan bukaan mata melalui kamera V4L2 tersedia pada
Linux.

> **Catatan penting:** Cubism SDK for Native tidak disertakan. Untuk memuat
> model Live2D, unduh SDK resmi dari Live2D dan siapkan secara terpisah.

## Isi

- [Fitur](#fitur)
- [Prasyarat](#prasyarat)
- [Instalasi otomatis](#instalasi-otomatis)
- [Build manual](#build-manual)
- [Menyiapkan Cubism SDK](#menyiapkan-cubism-sdk)
- [Menggunakan aplikasi](#menggunakan-aplikasi)
- [OBS dan latar transparan](#obs-dan-latar-transparan)
- [Pengaturan dan log](#pengaturan-dan-log)
- [Pemecahan masalah](#pemecahan-masalah)
- [Lisensi dan komponen pihak ketiga](#lisensi-dan-komponen-pihak-ketiga)

## Fitur

- Jendela avatar dan panel pengaturan yang terpisah.
- Memuat model dengan menyeret folder atau berkas `.model3.json` ke jendela
  avatar.
- Gerakan mulut berdasarkan level suara mikrofon; audio diproses lokal dan
  tidak direkam atau ditranskripsikan.
- Kedipan otomatis. Di Linux, kamera V4L2 opsional dapat mengestimasi bukaan
  mata.
- Pengaturan zoom, posisi avatar, warna latar, dan batas FPS.
- Kontrol koneksi OBS melalui obs-websocket. Video tetap ditangkap langsung
  oleh OBS, bukan dikirim melalui WebSocket.

## Prasyarat

### Linux

Target yang didokumentasikan adalah Debian 12/13 dan turunannya, menggunakan
X11. Build memerlukan CMake, GCC/G++, OpenGL development files, dan SDL2.
Instalasi otomatis di bawah memasang dependensi build tersebut.

Kamera wajah menggunakan V4L2 dan hanya tersedia di Linux. OBS Studio bersifat
opsional; pasang melalui APT bila diperlukan:

```bash
sudo apt install obs-studio
```

### Windows

Build dan instalasi otomatis ditujukan untuk Windows 10/11 x64. Skrip Windows
memerlukan `winget` dari **App Installer**, koneksi internet, dan izin
administrator untuk memasang Visual Studio Build Tools.

### Sumber proyek

Sebelum menjalankan perintah, unduh atau salin seluruh folder proyek ke
komputer. Jalankan perintah dari folder utama proyek—folder yang berisi
`CMakeLists.txt`, `src/`, dan `tools/`.

## Instalasi otomatis

Skrip akan memasang dependensi build, membangun aplikasi, lalu memasang atau
memperbarui aplikasi. Koneksi internet diperlukan. Pengaturan pengguna tidak
dihapus saat aplikasi diperbarui.

### Debian/Ubuntu

Buka terminal di folder proyek, lalu jalankan:

```bash
sh tools/install-linux.sh
```

Skrip menggunakan APT dan mungkin meminta kata sandi `sudo`. Jika paket
`libsdl2-dev` tidak dapat dipasang, skrip mencoba membangun SDL2 lokal melalui
`tools/bangun-sdl2-lokal.sh`. Fallback ini memerlukan alat build dan `curl`.
Skrip instalasi ini tidak ditujukan untuk Fedora, Arch Linux, atau distribusi
non-APT.

Setelah berhasil, jalankan aplikasinya dengan:

```bash
avatar-studio
```

### Windows 10/11

Buka PowerShell di folder proyek, lalu jalankan:

```powershell
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
.\tools\install-windows.ps1
```

Perubahan execution policy hanya berlaku untuk jendela PowerShell tersebut.
Skrip menggunakan `winget` untuk menyiapkan Git, CMake, Visual Studio 2022
Build Tools (komponen C++), dan Microsoft Visual C++ Redistributable; skrip
juga mengunduh serta menyiapkan vcpkg dan dependensi build. Build Tools cukup
besar dan proses pemasangannya dapat memakan waktu.

Aplikasi dipasang di `%LOCALAPPDATA%\Programs\Avatar Studio`, lalu pintasan
dibuat di menu Start. Bila skrip melaporkan Git atau CMake belum tersedia
setelah pemasangan, tutup dan buka kembali PowerShell, lalu jalankan ulang
skrip.

Skrip instalasi Windows membangun aplikasi tanpa Cubism SDK bila SDK belum
tersedia. Aplikasi dapat terpasang, tetapi model Live2D tidak dapat dimuat
sampai SDK disiapkan dan aplikasi dibangun ulang.

## Build manual

### Linux

Pasang kebutuhan build bila belum tersedia:

```bash
sudo apt update
sudo apt install build-essential cmake git libgl-dev libx11-dev \
  libsdl2-dev pkg-config
```

Lalu, dari folder utama proyek:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/avatar-studio
```

Dear ImGui diambil otomatis oleh CMake saat konfigurasi pertama, sehingga
koneksi internet diperlukan. Jika `libsdl2-dev` bermasalah pada sistem,
jangan jalankan perintah `apt install` di atas untuk SDL2; gunakan fallback
SDL2 lokal:

```bash
sudo apt install build-essential cmake curl git libgl-dev libx11-dev \
  pkg-config
sh tools/bangun-sdl2-lokal.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/avatar-studio
```

Untuk membuat paket Debian:

```bash
cpack --config build/CPackConfig.cmake -G DEB -B build
```

Pasang paket yang dibuat (nama berkas memuat arsitektur komputer):

```bash
sudo apt install ./build/avatar-studio_0.1.0_amd64.deb
```

Ganti `amd64` jika nama paket yang dihasilkan berbeda. Alternatifnya, pasang
hasil build langsung ke `/usr/local`:

```bash
sudo cmake --install build --prefix /usr/local
```

### Windows 10/11

Instal Visual Studio 2022 atau Build Tools dengan workload **Desktop
development with C++**, Git, dan CMake. Siapkan vcpkg jika belum tersedia:

```powershell
git clone https://github.com/microsoft/vcpkg.git "$env:LOCALAPPDATA\vcpkg"
& "$env:LOCALAPPDATA\vcpkg\bootstrap-vcpkg.bat"
```

Kemudian, dari PowerShell di folder utama proyek, konfigurasi dan build:

```powershell
cmake -S . -B build-windows `
  -DCMAKE_TOOLCHAIN_FILE="$env:LOCALAPPDATA/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build-windows --config Release
cpack --config build-windows/CPackConfig.cmake -G ZIP -C Release -B build-windows
```

Manifest vcpkg proyek memasang SDL2 dan GLEW. Paket ZIP dibuat di
`build-windows/`; ekstrak paket, lalu jalankan `bin\avatar-studio.exe`.

## Menyiapkan Cubism SDK

Cubism SDK for Native milik Live2D **tidak disertakan** di proyek atau paket
aplikasi. Unduh sendiri dari [halaman unduhan resmi Live2D](https://www.live2d.com/en/sdk/download/native/)
(akun Live2D mungkin diperlukan), lalu ekstrak SDK.

Salah satu susunan yang dikenali otomatis adalah:

```text
third_party/CubismSdk-Native/
├── Core/
│   ├── include/Live2DCubismCore.h
│   └── lib/
└── Framework/
```

Folder tersebut diabaikan oleh Git dan tidak boleh dimasukkan ke repositori.
Sebagai alternatif, simpan SDK di lokasi lain dan tunjukkan lokasinya saat
konfigurasi:

```bash
cmake -S . -B build \
  -DCUBISM_SDK_ROOT="/path/ke/CubismSdk-Native"
```

Di PowerShell gunakan path Windows, misalnya:

```powershell
cmake -S . -B build-windows `
  -DCMAKE_TOOLCHAIN_FILE="$env:LOCALAPPDATA/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCUBISM_SDK_ROOT="C:/SDK/CubismSdk-Native"
```

SDK harus menyediakan header, library Core yang cocok dengan platform dan
compiler, serta Framework. Pada Windows, konfigurasi proyek mencari library
MSVC x64 di `Core/lib/windows/x86_64/<versi-toolset-MSVC>/`. Jika SDK atau
library platform yang sesuai tidak ditemukan, build tetap dapat berhasil
tanpa dukungan pemuatan model Live2D; CMake akan menampilkan peringatan.

## Menggunakan aplikasi

1. Jalankan Avatar Studio. Panel **Pengaturan Avatar Studio** dipakai untuk
   mengatur aplikasi; jendela **Avatar Studio** menampilkan model.
2. Seret folder model atau berkas `.model3.json` ke jendela avatar. Model
   memerlukan berkas `.moc3` dan tekstur yang dirujuk oleh konfigurasi model.
   Aplikasi mencari `.model3.json` di dalam subfolder.
3. Pilih perangkat di bagian **Perangkat mikrofon**, lalu tekan **Mulai
   mikrofon**. Sesuaikan **Sensitivitas mikrofon** bila suara tidak memicu
   mulut atau suara latar terlalu mudah memicunya. Daftar perangkat dapat
   diperbarui dengan **Segarkan**.
4. Mata berkedip otomatis. Di Linux, buka bagian **Kamera wajah** dan pilih
   kamera V4L2 bila ingin mengaktifkan estimasi bukaan mata. Hasilnya adalah
   estimasi ringan berbasis warna kulit dan kontras, bukan pengenalan wajah
   atau pelacakan landmark AI. Cahaya redup atau posisi wajah yang tidak
   sesuai dapat membuat hasilnya tidak stabil.
5. Atur **Zoom avatar (%)**, **Posisi model**, warna latar, serta **Batas FPS**
   sesuai kebutuhan.

Gerakan mulut hanya mengikuti mikrofon. Kamera tidak mengendalikan gerakan
mulut, posisi, atau zoom avatar.

## OBS dan latar transparan

### Koneksi OBS

1. Gunakan OBS Studio 28 atau lebih baru dan aktifkan server WebSocket melalui
   **Tools → WebSocket Server Settings**.
2. Gunakan alamat dan port server OBS (bawaan `127.0.0.1:4455`), lalu masukkan
   password yang sama di aplikasi dan tekan **Hubungkan ke OBS**. Password
   hanya dipakai untuk koneksi dan tidak disimpan.
3. Tambahkan sumber **Window Capture** di OBS dan pilih jendela **Avatar
   Studio** untuk menangkap gambar avatar.

WebSocket hanya dipakai untuk kontrol/status OBS; video ditangkap langsung
oleh sumber Window Capture. Untuk gerakan yang stabil, samakan **Batas FPS**
aplikasi dengan FPS OBS, misalnya 30.

### Latar transparan

Aktifkan **Latar transparan** di bagian **Tampilan & performa**. Transparansi
bergantung pada dukungan alpha oleh sistem grafis, kompositor desktop, dan
metode penangkapan jendela yang digunakan. Jika latar tetap terlihat di desktop
atau OBS, pilih warna hijau bawaan dan gunakan filter **Chroma Key** di OBS.

Zoom tidak meningkatkan resolusi tekstur atau jumlah piksel hasil render.
Untuk gambar yang lebih tajam, besarkan jendela avatar dan atur resolusi
output OBS sesuai kebutuhan.

## Pengaturan dan log

- Linux: pengaturan berada di `~/.config/avatar-vtuber/pengaturan.ini`.
  Jika `XDG_CONFIG_HOME` disetel, direktori tersebut digunakan sebagai
  pengganti `~/.config`.
- Windows: pengaturan disimpan di direktori preferensi pengguna yang disediakan
  SDL dan ditulis ketika aplikasi ditutup.
- Log Linux biasanya berada di
  `~/.local/share/Avatar Studio/Avatar Studio/avatar-studio.log`. Log memuat
  informasi diagnostik, termasuk proses pemuatan model.

## Pemecahan masalah

| Masalah | Yang perlu diperiksa |
| --- | --- |
| Model tidak dapat dimuat | Pastikan Cubism SDK cocok dengan platform dan tersedia **saat build**. Periksa berkas `.model3.json`, `.moc3`, dan tekstur model. |
| Kamera tidak muncul | Kamera V4L2 hanya didukung pada Linux. Tekan **Segarkan** dan pastikan kamera terlihat sebagai perangkat `/dev/videoN`. |
| Mikrofon tidak terdeteksi | Periksa perangkat input sistem, pilih perangkat yang benar, lalu tekan **Segarkan**. |
| Latar transparan tetap berwarna | Kompositor atau metode Window Capture mungkin tidak meneruskan alpha. Coba warna hijau dan filter Chroma Key OBS. |
| Perintah build gagal | Baca pesan CMake/build di terminal. CMake memerlukan akses internet untuk mengambil Dear ImGui saat konfigurasi awal. |
| OBS tidak tersambung | Pastikan server WebSocket OBS aktif dan alamat, port, serta password cocok. |

## Lisensi dan komponen pihak ketiga

- Font Lato dibundel di `resources/fonts/` berdasarkan lisensi SIL Open Font
  License.
- Dear ImGui, SDL2, dan komponen build lain mengikuti lisensi masing-masing.
- Cubism SDK for Native memiliki ketentuan Live2D tersendiri: unduh dan gunakan
  sesuai lisensi resmi. SDK tidak boleh disalin ke repositori atau paket
  distribusi proyek ini.
