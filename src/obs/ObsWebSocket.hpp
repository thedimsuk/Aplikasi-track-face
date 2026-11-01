#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
using ObsSocketHandle = SOCKET;
#else
using ObsSocketHandle = int;
#endif

class ObsWebSocket {
 public:
  ObsWebSocket() = default;
  ~ObsWebSocket();

  ObsWebSocket(const ObsWebSocket&) = delete;
  ObsWebSocket& operator=(const ObsWebSocket&) = delete;

  // Hubungkan ke alamat tertentu. Selalu kembalikan true selama parameter
  // valid (hasil akhir terlihat di status() karena prosesnya di thread latar).
  bool sambung(const std::string& host, int port, const std::string& password);

  // Cari OBS di alamat lokal yang umum, lalu hubungkan. Praktis untuk
  // orang awam: tidak perlu mengetik alamat/port bila masih bawaan OBS.
  bool sambungOtomatis(const std::string& password);

  void putus();

  bool aktif() const { return aktif_.load(); }
  std::string status() const;
  std::string sceneAktif() const;

  // Cek cepat (blocking singkat) apakah port TCP terbuka. Dipakai oleh
  // sambungOtomatis agar tidak menunggu lama.
  static bool portTerbuka(const std::string& host, int port, int timeoutMs);

 private:
  void jalankan(std::string host, int port, std::string password);
  // Satu kali percobaan koneksi+identifikasi. true = terhubung penuh.
  bool cobaSekali(const std::string& host, int port, std::string password,
                  ObsSocketHandle fd);
  void setStatus(std::string status);

  std::atomic<bool> stop_{false};
  std::atomic<bool> aktif_{false};
  std::atomic<ObsSocketHandle> socket_{static_cast<ObsSocketHandle>(-1)};
  mutable std::mutex mutex_;
  std::thread thread_;
  std::string status_ = "Belum terhubung ke OBS.";
  std::string scene_;
};
