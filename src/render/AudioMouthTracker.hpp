#pragma once

#include <SDL.h>

#include <atomic>
#include <string>
#include <vector>

class AudioMouthTracker {
 public:
  AudioMouthTracker() = default;
  ~AudioMouthTracker();

  AudioMouthTracker(const AudioMouthTracker&) = delete;
  AudioMouthTracker& operator=(const AudioMouthTracker&) = delete;

  // Mulai capture audio. namaPerangkat kosong = gunakan default sistem.
  // Daftar perangkat bisa didapat lewat daftarPerangkatCapture().
  bool mulai(int sensitivitas, const std::string& namaPerangkat = {});
  void berhenti();
  void aturSensitivitas(int sensitivitas);

  bool aktif() const { return device_ != 0; }
  float tingkatSuara() const { return tingkatSuara_.load(); }
  float levelInput() const { return levelInput_.load(); }
  const std::string& status() const { return status_; }
  const std::string& namaPerangkat() const { return namaPerangkat_; }

  // Daftar perangkat audio capture yang tersedia (SDL).
  static std::vector<std::string> daftarPerangkatCapture();

 private:
  static void tangkap(void* konteks, Uint8* data, int panjang);
  void prosesAudio(const Uint8* data, int panjang);

  SDL_AudioDeviceID device_ = 0;
  bool ownsAudioSubsystem_ = false;
  SDL_AudioFormat format_ = AUDIO_S16SYS;
  Uint8 kanal_ = 1;
  std::atomic<float> tingkatSuara_{0.0f};
  std::atomic<float> levelInput_{0.0f};
  std::atomic<float> ambang_{0.03f};
  std::string status_ = "Mikrofon belum dimulai.";
  std::string namaPerangkat_; // nama perangkat yang dipakai (untuk GUI)
};
