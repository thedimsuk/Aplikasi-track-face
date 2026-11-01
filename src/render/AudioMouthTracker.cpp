#include "render/AudioMouthTracker.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

float nilaiSampel(const Uint8* data, SDL_AudioFormat format) {
  const int bits = SDL_AUDIO_BITSIZE(format);
  if (SDL_AUDIO_ISFLOAT(format) && bits == 32) {
    Uint32 mentah = 0;
    std::memcpy(&mentah, data, sizeof(mentah));
    const bool formatBigEndian = SDL_AUDIO_ISBIGENDIAN(format);
    const bool hostBigEndian = SDL_BYTEORDER == SDL_BIG_ENDIAN;
    if (formatBigEndian != hostBigEndian) mentah = SDL_Swap32(mentah);
    float nilai = 0.0f;
    std::memcpy(&nilai, &mentah, sizeof(nilai));
    return std::isfinite(nilai) ? std::clamp(nilai, -1.0f, 1.0f) : 0.0f;
  }

  if (bits != 8 && bits != 16 && bits != 32) return 0.0f;
  Uint32 mentah = 0;
  const int byte = bits / 8;
  const bool formatBigEndian = SDL_AUDIO_ISBIGENDIAN(format);
  for (int i = 0; i < byte; ++i) {
    const int indeks = formatBigEndian ? i : byte - i - 1;
    mentah = (mentah << 8) | data[indeks];
  }

  const double skala = static_cast<double>(Uint64{1} << (bits - 1));
  if (SDL_AUDIO_ISSIGNED(format)) {
    const Uint64 mask = (Uint64{1} << bits) - 1;
    const Sint64 bertanda =
        (mentah & (Uint32{1} << (bits - 1)))
            ? static_cast<Sint64>(mentah) - static_cast<Sint64>(mask) - 1
            : static_cast<Sint64>(mentah);
    return static_cast<float>(bertanda / skala);
  }
  return static_cast<float>((static_cast<double>(mentah) - skala) / skala);
}

}  // namespace

AudioMouthTracker::~AudioMouthTracker() { berhenti(); }

bool AudioMouthTracker::mulai(int sensitivitas,
                              const std::string& namaPerangkat) {
  berhenti();
  aturSensitivitas(sensitivitas);

  ownsAudioSubsystem_ = SDL_WasInit(SDL_INIT_AUDIO) == 0;
  if (ownsAudioSubsystem_ && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    ownsAudioSubsystem_ = false;
    status_ = std::string("Audio tidak bisa dimulai: ") + SDL_GetError();
    return false;
  }

  SDL_AudioSpec diinginkan{};
  diinginkan.freq = 16000;
  diinginkan.format = AUDIO_S16SYS;
  diinginkan.channels = 1;
  diinginkan.samples = 512;
  diinginkan.callback = tangkap;
  diinginkan.userdata = this;

  std::vector<std::string> calonPerangkat;
  const auto tambahCalon = [&calonPerangkat](const std::string& nama) {
    if (std::find(calonPerangkat.begin(), calonPerangkat.end(), nama) ==
        calonPerangkat.end()) {
      calonPerangkat.push_back(nama);
    }
  };
  const auto perangkatLoopback = [](const std::string& nama) {
    std::string namaKecil = nama;
    std::transform(namaKecil.begin(), namaKecil.end(), namaKecil.begin(),
                   [](unsigned char karakter) {
                     return static_cast<char>(std::tolower(karakter));
                   });
    return namaKecil.find("loopback") != std::string::npos ||
           namaKecil.find("monitor") != std::string::npos;
  };
  const std::vector<std::string> perangkatTersedia =
      daftarPerangkatCapture();
  if (!namaPerangkat.empty()) tambahCalon(namaPerangkat);
  for (const std::string& nama : perangkatTersedia) {
    if (!nama.empty() && !perangkatLoopback(nama)) tambahCalon(nama);
  }
  tambahCalon({});
  for (const std::string& nama : perangkatTersedia) {
    if (!nama.empty() && perangkatLoopback(nama)) tambahCalon(nama);
  }

  std::string galatTerakhir;
  bool formatTidakDidukung = false;
  for (const std::string& calon : calonPerangkat) {
    SDL_AudioSpec didapat{};
    const char* nama = calon.empty() ? nullptr : calon.c_str();
    const SDL_AudioDeviceID device = SDL_OpenAudioDevice(
        nama, SDL_TRUE, &diinginkan, &didapat,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE |
            SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (!device) {
      galatTerakhir = SDL_GetError();
      continue;
    }

    const int bits = SDL_AUDIO_BITSIZE(didapat.format);
    if ((SDL_AUDIO_ISFLOAT(didapat.format) && bits != 32) ||
        (!SDL_AUDIO_ISFLOAT(didapat.format) && bits != 8 && bits != 16 &&
         bits != 32)) {
      SDL_CloseAudioDevice(device);
      formatTidakDidukung = true;
      continue;
    }

    device_ = device;
    format_ = didapat.format;
    kanal_ = didapat.channels;
    namaPerangkat_ = calon.empty() ? "Default sistem" : calon;
    tingkatSuara_ = 0.0f;
    levelInput_ = 0.0f;
    SDL_PauseAudioDevice(device_, 0);

    const bool otomatis = calon != namaPerangkat;
    status_ = "Mikrofon aktif: " + namaPerangkat_ + " (" +
              std::to_string(didapat.freq) + " Hz)." +
              (otomatis ? " Dipilih otomatis setelah perangkat pilihan gagal."
                        : " Ucapkan sesuatu dan lihat indikator level input.");
    return true;
  }

  if (ownsAudioSubsystem_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  ownsAudioSubsystem_ = false;
  status_ = formatTidakDidukung
                ? "Perangkat capture ditemukan, tetapi format audionya tidak "
                  "didukung."
                : "Mikrofon tidak bisa dibuka. Perangkat default dan seluruh "
                  "perangkat capture yang terdeteksi sudah dicoba: " +
                      (galatTerakhir.empty() ? "galat tidak diketahui."
                                             : galatTerakhir);
  return false;
}

void AudioMouthTracker::berhenti() {
  if (device_) {
    SDL_PauseAudioDevice(device_, 1);
    SDL_CloseAudioDevice(device_);
    device_ = 0;
    if (ownsAudioSubsystem_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    ownsAudioSubsystem_ = false;
  }
  tingkatSuara_ = 0.0f;
  levelInput_ = 0.0f;
  status_ = "Mikrofon dihentikan.";
}

void AudioMouthTracker::aturSensitivitas(int sensitivitas) {
  const int nilai = std::clamp(sensitivitas, 1, 100);
  const float sensitivitasNormal =
      static_cast<float>(nilai - 1) / 99.0f;
  const float sensitivitasRendah = 1.0f - sensitivitasNormal;
  ambang_ = 0.02f + sensitivitasRendah * sensitivitasRendah * 0.18f;
}

void AudioMouthTracker::tangkap(void* konteks, Uint8* data, int panjang) {
  static_cast<AudioMouthTracker*>(konteks)->prosesAudio(data, panjang);
}

void AudioMouthTracker::prosesAudio(const Uint8* data, int panjang) {
  const int bytesPerSample = SDL_AUDIO_BITSIZE(format_) / 8;
  const int bytesPerFrame = bytesPerSample * kanal_;
  if (bytesPerFrame <= 0 || panjang < bytesPerFrame) return;
  const int jumlahFrame = panjang / bytesPerFrame;

  double jumlahKuadrat = 0.0;
  for (int frame = 0; frame < jumlahFrame; ++frame) {
    const Uint8* awal = data + frame * bytesPerFrame;
    double mono = 0.0;
    for (int kanal = 0; kanal < kanal_; ++kanal) {
      mono += nilaiSampel(awal + kanal * bytesPerSample, format_);
    }
    mono /= kanal_;
    jumlahKuadrat += mono * mono;
  }

  const float rms =
      static_cast<float>(std::sqrt(jumlahKuadrat / jumlahFrame));
  levelInput_ = std::clamp(rms * 10.0f, 0.0f, 1.0f);
  const float ambang = ambang_.load();
  const float sensitivitasRendah =
      std::clamp((ambang - 0.02f) / 0.18f, 0.0f, 1.0f);
  const float rentangBuka = 0.08f + sensitivitasRendah * 0.32f;
  tingkatSuara_ =
      std::clamp((rms - ambang) / rentangBuka, 0.0f, 1.0f);
}

std::vector<std::string> AudioMouthTracker::daftarPerangkatCapture() {
  std::vector<std::string> hasil;
  const bool inisialisasiLokal = SDL_WasInit(SDL_INIT_AUDIO) == 0;
  if (inisialisasiLokal && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    return hasil;

  int jumlah = SDL_GetNumAudioDevices(SDL_TRUE);
  if (jumlah >= 0) {
    for (int i = 0; i < jumlah; ++i) {
      const char* nama = SDL_GetAudioDeviceName(i, SDL_TRUE);
      if (nama && *nama) hasil.emplace_back(nama);
    }
  }
  if (inisialisasiLokal) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  return hasil;
}
