#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct PerangkatKamera {
  std::string path;
  std::string nama;
};

// ---------------------------------------------------------------------------
// Estimasi mata dari kamera, dikonsumsi renderer avatar.
// ---------------------------------------------------------------------------
struct DataKamera {
  bool aktif = false;
  float eyeOpen = 0.0f; // 0 .. 1 (kedap mata)
  std::string status;     // pesan status untuk GUI
};

// ---------------------------------------------------------------------------
// CameraTracker — menangkap frame kamera via V4L2 di Linux, mendeteksi wajah
// secara sederhana dan mengestimasi bukaan mata.
// Optional: dapat dimatikan sepenuhnya dari GUI.
// ---------------------------------------------------------------------------
class CameraTracker {
 public:
  CameraTracker();
  ~CameraTracker();

  CameraTracker(const CameraTracker&) = delete;
  CameraTracker& operator=(const CameraTracker&) = delete;

  // Mulai menangkap dari perangkat V4L2 (contoh: "/dev/video0").
  // Kembalikan true bila berhasil membuka dan memulai thread capture.
  bool mulai(const std::string& perangkat);
  void berhenti();

  // Daftar perangkat V4L2 streaming yang kompatibel (termasuk DroidCam).
  // Cocok untuk kamera handphone yang muncul sebagai V4L2 via kabel USB.
  // paksaScan = abaikan cache dan scan ulang /dev/video* segera.
  static std::vector<PerangkatKamera> daftarPerangkat(bool paksaScan = false);

  bool aktif() const { return berjalan_.load(); }
  DataKamera dataTerbaru() const;
   std::string status() const;

 private:
   struct BufferMmap {
     void* alamat = nullptr;
     std::size_t panjang = 0;
   };

   void loopKamera();
   bool bukaPerangkat(const std::string& path);
   void tutupPerangkat();
   bool ambilFrame();
   bool konversiKeRgb(const unsigned char* sumber, std::size_t panjang);
   void prosesFrame(const unsigned char* rgb, int lebar, int tinggi);
   void setStatus(std::string status);

   int fd_ = -1;
   std::thread thread_;
  std::atomic<bool> berjalan_{false};
  mutable std::mutex mutex_;
  DataKamera data_;
  std::string status_ = "Kamera belum dimulai.";
  std::string perangkat_;
  std::string namaPerangkat_;

  std::vector<BufferMmap> bufferMmap_;
  std::vector<unsigned char> rgbFrame_;
  std::uint32_t format_ = 0;
  std::uint32_t bytesPerLine_ = 0;
  int lebarFrame_ = 0;
  int tinggiFrame_ = 0;
  bool streaming_ = false;

  // Cache daftar perangkat + timestamp untuk invalidasi periodik.
  static std::vector<PerangkatKamera> g_perangkatCache_;
  static std::chrono::steady_clock::time_point g_waktuScanTerakhir_;
  static constexpr int g_intervalScanDetik = 10; // scan ulang tiap 10 detik
};