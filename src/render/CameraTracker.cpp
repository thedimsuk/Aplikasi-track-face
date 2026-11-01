#include "render/CameraTracker.hpp"

#include <utility>

#ifdef _WIN32

std::vector<PerangkatKamera> CameraTracker::g_perangkatCache_;
std::chrono::steady_clock::time_point CameraTracker::g_waktuScanTerakhir_ =
    std::chrono::steady_clock::time_point();

CameraTracker::CameraTracker() = default;
CameraTracker::~CameraTracker() = default;

bool CameraTracker::mulai(const std::string&) {
  setStatus("Pelacakan mata dari kamera saat ini hanya tersedia di Linux.");
  return false;
}

void CameraTracker::berhenti() {
  berjalan_ = false;
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = {};
  status_ = "Pelacakan mata dari kamera belum tersedia di Windows.";
}

DataKamera CameraTracker::dataTerbaru() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

std::string CameraTracker::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

void CameraTracker::setStatus(std::string status) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = std::move(status);
}

std::vector<PerangkatKamera> CameraTracker::daftarPerangkat(bool) {
  return {};
}

#else

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <utility>

namespace {

int xioctl(int fd, unsigned long request, void* arg) {
  int result;
  do {
    result = ioctl(fd, request, arg);
  } while (result == -1 && errno == EINTR);
  return result;
}

std::string namaKartu(const v4l2_capability& capability) {
  const char* nama = reinterpret_cast<const char*>(capability.card);
  return nama && *nama ? nama : "Kamera V4L2";
}

constexpr std::array<std::uint32_t, 6> formatDidukung = {
    V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_RGB24,
    V4L2_PIX_FMT_BGR24, V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_NV12};

bool formatDikenali(std::uint32_t format) {
  return std::find(formatDidukung.begin(), formatDidukung.end(), format) !=
         formatDidukung.end();
}

bool perangkatKompatibel(int fd, v4l2_capability& capability) {
  if (xioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) return false;
  const std::uint32_t caps = (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
                                 ? capability.device_caps
                                 : capability.capabilities;
  if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING))
    return false;

  for (std::uint32_t index = 0;; ++index) {
    v4l2_fmtdesc format{};
    format.index = index;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_ENUM_FMT, &format) < 0) break;
    if (formatDikenali(format.pixelformat)) return true;
  }
  return false;
}

unsigned char batasByte(int nilai) {
  return static_cast<unsigned char>(std::clamp(nilai, 0, 255));
}

void yuvKeRgb(int y, int u, int v, unsigned char* rgb) {
  const int c = std::max(0, y - 16);
  const int d = u - 128;
  const int e = v - 128;
  rgb[0] = batasByte((298 * c + 409 * e + 128) >> 8);
  rgb[1] = batasByte((298 * c - 100 * d - 208 * e + 128) >> 8);
  rgb[2] = batasByte((298 * c + 516 * d + 128) >> 8);
}

}  // namespace

std::vector<PerangkatKamera> CameraTracker::g_perangkatCache_;
std::chrono::steady_clock::time_point CameraTracker::g_waktuScanTerakhir_ =
    std::chrono::steady_clock::time_point();

CameraTracker::CameraTracker() = default;

CameraTracker::~CameraTracker() { berhenti(); }

bool CameraTracker::mulai(const std::string& perangkat) {
  berhenti();
  if (!bukaPerangkat(perangkat)) return false;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = {};
  }
  berjalan_ = true;
  setStatus("Kamera aktif (" + namaPerangkat_ + "), mendeteksi wajah...");
  thread_ = std::thread(&CameraTracker::loopKamera, this);
  return true;
}

void CameraTracker::berhenti() {
  berjalan_ = false;
  if (thread_.joinable()) thread_.join();
  tutupPerangkat();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = {};
  }
  setStatus("Kamera dihentikan.");
}

DataKamera CameraTracker::dataTerbaru() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

std::string CameraTracker::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (berjalan_ && !data_.status.empty()) return data_.status;
  return status_;
}

void CameraTracker::setStatus(std::string status) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = std::move(status);
}

void CameraTracker::loopKamera() {
  constexpr std::chrono::milliseconds periodeFrame(33);

  while (berjalan_) {
    const auto mulai = std::chrono::steady_clock::now();
    ambilFrame();
    const auto selesai = std::chrono::steady_clock::now();
    const auto telahHabis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                selesai - mulai);
    if (telahHabis < periodeFrame)
      std::this_thread::sleep_for(periodeFrame - telahHabis);
  }
}

bool CameraTracker::ambilFrame() {
  if (fd_ < 0 || bufferMmap_.empty()) return false;

  fd_set fdset;
  FD_ZERO(&fdset);
  FD_SET(fd_, &fdset);
  timeval timeout{};
  timeout.tv_usec = 100000;
  const int siap = select(fd_ + 1, &fdset, nullptr, nullptr, &timeout);
  if (siap <= 0) {
    if (siap < 0 && errno != EINTR)
      setStatus("Gagal menunggu frame kamera: " +
                std::string(std::strerror(errno)));
    return false;
  }

  v4l2_buffer buffer{};
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd_, VIDIOC_DQBUF, &buffer) < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      setStatus("Gagal mengambil frame kamera: " +
                std::string(std::strerror(errno)));
    return false;
  }

  bool berhasil = false;
  if (buffer.index < bufferMmap_.size() &&
      buffer.bytesused <= bufferMmap_[buffer.index].panjang) {
    berhasil = konversiKeRgb(
        static_cast<const unsigned char*>(bufferMmap_[buffer.index].alamat),
        buffer.bytesused);
    if (berhasil)
      prosesFrame(rgbFrame_.data(), lebarFrame_, tinggiFrame_);
  }

  if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
    setStatus("Gagal mengantrekan ulang frame kamera: " +
              std::string(std::strerror(errno)));
    return false;
  }
  return berhasil;
}

bool CameraTracker::konversiKeRgb(const unsigned char* sumber,
                                  std::size_t panjang) {
  if (!sumber || lebarFrame_ <= 0 || tinggiFrame_ <= 0) return false;
  const std::size_t lebar = static_cast<std::size_t>(lebarFrame_);
  const std::size_t tinggi = static_cast<std::size_t>(tinggiFrame_);
  if (tinggi > rgbFrame_.max_size() / 3 / lebar) return false;
  rgbFrame_.resize(lebar * tinggi * 3);

  std::size_t stride = bytesPerLine_;
  if (!stride) {
    if (format_ == V4L2_PIX_FMT_RGB24 || format_ == V4L2_PIX_FMT_BGR24)
      stride = lebar * 3;
    else if (format_ == V4L2_PIX_FMT_YUYV || format_ == V4L2_PIX_FMT_UYVY)
      stride = lebar * 2;
    else
      stride = lebar;
  }
  if (format_ == V4L2_PIX_FMT_RGB24 || format_ == V4L2_PIX_FMT_BGR24) {
    if (stride < lebar * 3 || panjang < stride * tinggi) return false;
    for (std::size_t y = 0; y < tinggi; ++y) {
      const unsigned char* baris = sumber + y * stride;
      unsigned char* tujuan = rgbFrame_.data() + y * lebar * 3;
      for (std::size_t x = 0; x < lebar; ++x) {
        const std::size_t offset = x * 3;
        const std::size_t merah = format_ == V4L2_PIX_FMT_RGB24 ? 0 : 2;
        tujuan[offset] = baris[offset + merah];
        tujuan[offset + 1] = baris[offset + 1];
        tujuan[offset + 2] = baris[offset + 2 - merah];
      }
    }
    return true;
  }

  if (format_ == V4L2_PIX_FMT_YUYV || format_ == V4L2_PIX_FMT_UYVY) {
    if ((lebar & 1U) || stride < lebar * 2 || panjang < stride * tinggi)
      return false;
    for (std::size_t y = 0; y < tinggi; ++y) {
      const unsigned char* baris = sumber + y * stride;
      unsigned char* tujuan = rgbFrame_.data() + y * lebar * 3;
      for (std::size_t x = 0; x < lebar; x += 2) {
        const unsigned char* pasangan = baris + x * 2;
        const int y0 = format_ == V4L2_PIX_FMT_YUYV ? pasangan[0] : pasangan[1];
        const int u = format_ == V4L2_PIX_FMT_YUYV ? pasangan[1] : pasangan[0];
        const int y1 = format_ == V4L2_PIX_FMT_YUYV ? pasangan[2] : pasangan[3];
        const int v = format_ == V4L2_PIX_FMT_YUYV ? pasangan[3] : pasangan[2];
        yuvKeRgb(y0, u, v, tujuan + x * 3);
        yuvKeRgb(y1, u, v, tujuan + (x + 1) * 3);
      }
    }
    return true;
  }

  if (format_ == V4L2_PIX_FMT_YUV420) {
    const std::size_t strideKroma = (stride + 1) / 2;
    const std::size_t tinggiKroma = (tinggi + 1) / 2;
    const std::size_t panjangLuma = stride * tinggi;
    const std::size_t panjangKroma = strideKroma * tinggiKroma;
    if (stride < lebar || panjang < panjangLuma + panjangKroma * 2)
      return false;
    const unsigned char* planeU = sumber + panjangLuma;
    const unsigned char* planeV = planeU + panjangKroma;
    for (std::size_t y = 0; y < tinggi; ++y) {
      unsigned char* tujuan = rgbFrame_.data() + y * lebar * 3;
      for (std::size_t x = 0; x < lebar; ++x) {
        yuvKeRgb(sumber[y * stride + x],
                 planeU[(y / 2) * strideKroma + x / 2],
                 planeV[(y / 2) * strideKroma + x / 2],
                 tujuan + x * 3);
      }
    }
    return true;
  }

  if (format_ == V4L2_PIX_FMT_NV12) {
    const std::size_t panjangLuma = stride * tinggi;
    if ((lebar & 1U) || stride < lebar ||
        panjang < panjangLuma + stride * ((tinggi + 1) / 2))
      return false;
    const unsigned char* planeUv = sumber + panjangLuma;
    for (std::size_t y = 0; y < tinggi; ++y) {
      unsigned char* tujuan = rgbFrame_.data() + y * lebar * 3;
      for (std::size_t x = 0; x < lebar; ++x) {
        const unsigned char* uv = planeUv + (y / 2) * stride + (x / 2) * 2;
        yuvKeRgb(sumber[y * stride + x], uv[0], uv[1], tujuan + x * 3);
      }
    }
    return true;
  }
  return false;
}

void CameraTracker::prosesFrame(const unsigned char* rgb, int lebar,
                                int tinggi) {
  if (!rgb || lebar <= 0 || tinggi <= 0) return;

  DataKamera baru;
  baru.aktif = true;
  const int skala = std::max(1, std::max(lebar, tinggi) / 320);
  const int gridLebar = (lebar + skala - 1) / skala;
  const int gridTinggi = (tinggi + skala - 1) / skala;
  std::vector<unsigned char> kulit(
      static_cast<std::size_t>(gridLebar) * gridTinggi, 0);

  for (int gy = 0; gy < gridTinggi; ++gy) {
    const int y = std::min(gy * skala, tinggi - 1);
    if (y > tinggi * 4 / 5) continue;
    for (int gx = 0; gx < gridLebar; ++gx) {
      const int x = std::min(gx * skala, lebar - 1);
      const unsigned char* piksel =
          rgb + (static_cast<std::size_t>(y) * lebar + x) * 3;
      const int r = piksel[0];
      const int g = piksel[1];
      const int b = piksel[2];
      const int cb = 128 - (43 * r + 85 * g - 128 * b) / 256;
      const int cr = 128 + (128 * r - 107 * g - 21 * b) / 256;
      if (cb >= 70 && cb <= 140 && cr >= 125 && cr <= 180 &&
          r > g && r > b) {
        kulit[static_cast<std::size_t>(gy) * gridLebar + gx] = 1;
      }
    }
  }

  struct Komponen {
    int piksel = 0;
    int kiri = 0;
    int kanan = 0;
    int atas = 0;
    int bawah = 0;
    double sumX = 0.0;
    double sumY = 0.0;
    double skor = 0.0;
  } wajah;
  std::vector<unsigned char> dikunjungi(kulit.size(), 0);
  std::vector<int> antrean;

  for (int gy = 0; gy < gridTinggi; ++gy) {
    for (int gx = 0; gx < gridLebar; ++gx) {
      const int awal = gy * gridLebar + gx;
      if (!kulit[static_cast<std::size_t>(awal)] ||
          dikunjungi[static_cast<std::size_t>(awal)])
        continue;

      Komponen komponen;
      komponen.kiri = komponen.kanan = gx;
      komponen.atas = komponen.bawah = gy;
      antrean.clear();
      antrean.push_back(awal);
      dikunjungi[static_cast<std::size_t>(awal)] = 1;
      for (std::size_t indeks = 0; indeks < antrean.size(); ++indeks) {
        const int posisi = antrean[indeks];
        const int px = posisi % gridLebar;
        const int py = posisi / gridLebar;
        ++komponen.piksel;
        komponen.sumX += px;
        komponen.sumY += py;
        komponen.kiri = std::min(komponen.kiri, px);
        komponen.kanan = std::max(komponen.kanan, px);
        komponen.atas = std::min(komponen.atas, py);
        komponen.bawah = std::max(komponen.bawah, py);
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = px + dx;
            const int ny = py + dy;
            if (nx < 0 || nx >= gridLebar || ny < 0 || ny >= gridTinggi)
              continue;
            const int tetangga = ny * gridLebar + nx;
            if (!kulit[static_cast<std::size_t>(tetangga)] ||
                dikunjungi[static_cast<std::size_t>(tetangga)])
              continue;
            dikunjungi[static_cast<std::size_t>(tetangga)] = 1;
            antrean.push_back(tetangga);
          }
        }
      }

      const int lebarKomponen = komponen.kanan - komponen.kiri + 1;
      const int tinggiKomponen = komponen.bawah - komponen.atas + 1;
      const float rasio = static_cast<float>(lebarKomponen) / tinggiKomponen;
      if (komponen.piksel < 40 || lebarKomponen < gridLebar / 12 ||
          tinggiKomponen < gridTinggi / 12 || rasio < 0.30f ||
          rasio > 1.8f)
        continue;

      const double pusatX =
          komponen.sumX / komponen.piksel / std::max(1, gridLebar - 1);
      const double pusatY =
          komponen.sumY / komponen.piksel / std::max(1, gridTinggi - 1);
      if (pusatX < 0.20 || pusatX > 0.80 || pusatY < 0.10 ||
          pusatY > 0.72)
        continue;
      const double jarakPusat =
          std::pow((pusatX - 0.5) * 1.4, 2.0) +
          std::pow((pusatY - 0.4) * 1.2, 2.0);
      komponen.skor = komponen.piksel / (1.0 + jarakPusat);
      if (komponen.skor > wajah.skor) wajah = komponen;
    }
  }

  if (wajah.piksel >= 40) {
    baru.aktif = true;
    const float kiri = static_cast<float>(wajah.kiri * skala);
    const float kanan =
        static_cast<float>(std::min(lebar - 1, (wajah.kanan + 1) * skala));
    const float atas = static_cast<float>(wajah.atas * skala);
    const float bawah =
        static_cast<float>(std::min(tinggi - 1, (wajah.bawah + 1) * skala));
    const float lebarWajah = std::max(1.0f, kanan - kiri);
    const float tinggiWajah = std::max(1.0f, bawah - atas);
    const int langkahFitur = std::max(1, static_cast<int>(lebarWajah / 100.0f));
    auto tinggiFiturGelap = [&](float x1, float x2, float y1, float y2,
                                float minLebar, float maxLebar,
                                float minTinggi, float maxTinggi) {
      const int xAwal =
          std::clamp(static_cast<int>(kiri + lebarWajah * x1), 0, lebar - 1);
      const int xAkhir =
          std::clamp(static_cast<int>(kiri + lebarWajah * x2), xAwal + 1, lebar);
      const int yAwal =
          std::clamp(static_cast<int>(atas + tinggiWajah * y1), 0, tinggi - 1);
      const int yAkhir =
          std::clamp(static_cast<int>(atas + tinggiWajah * y2), yAwal + 1, tinggi);

      double jumlah = 0.0;
      int sampel = 0;
      for (int y = yAwal; y < yAkhir; y += langkahFitur) {
        for (int x = xAwal; x < xAkhir; x += langkahFitur) {
          const unsigned char* p =
              rgb + (static_cast<std::size_t>(y) * lebar + x) * 3;
          jumlah += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
          ++sampel;
        }
      }
      if (sampel < 4) return 0.0f;
      const double ambangGelap =
          std::min(jumlah / sampel - 24.0, jumlah / sampel * 0.68);
      const int roiLebar = (xAkhir - xAwal + langkahFitur - 1) / langkahFitur;
      const int roiTinggi = (yAkhir - yAwal + langkahFitur - 1) / langkahFitur;
      std::vector<unsigned char> gelap(
          static_cast<std::size_t>(roiLebar) * roiTinggi, 0);
      for (int y = yAwal; y < yAkhir; y += langkahFitur) {
        for (int x = xAwal; x < xAkhir; x += langkahFitur) {
          const unsigned char* p =
              rgb + (static_cast<std::size_t>(y) * lebar + x) * 3;
          const double terang = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
          if (terang < ambangGelap)
            gelap[static_cast<std::size_t>((y - yAwal) / langkahFitur) *
                      roiLebar +
                  (x - xAwal) / langkahFitur] = 1;
        }
      }

      std::vector<unsigned char> dikunjungi(gelap.size(), 0);
      std::vector<int> antreanGelap;
      float tinggiTerbaik = -1.0f;
      int pikselTerbaik = 0;
      for (int ry = 0; ry < roiTinggi; ++ry) {
        for (int rx = 0; rx < roiLebar; ++rx) {
          const int awal = ry * roiLebar + rx;
          if (!gelap[static_cast<std::size_t>(awal)] ||
              dikunjungi[static_cast<std::size_t>(awal)])
            continue;
          int kiriGelap = rx, kananGelap = rx, atasGelap = ry, bawahGelap = ry;
          int jumlahGelap = 0;
          antreanGelap.clear();
          antreanGelap.push_back(awal);
          dikunjungi[static_cast<std::size_t>(awal)] = 1;
          for (std::size_t i = 0; i < antreanGelap.size(); ++i) {
            const int posisi = antreanGelap[i];
            const int px = posisi % roiLebar;
            const int py = posisi / roiLebar;
            ++jumlahGelap;
            kiriGelap = std::min(kiriGelap, px);
            kananGelap = std::max(kananGelap, px);
            atasGelap = std::min(atasGelap, py);
            bawahGelap = std::max(bawahGelap, py);
            for (int dy = -1; dy <= 1; ++dy) {
              for (int dx = -1; dx <= 1; ++dx) {
                const int nx = px + dx;
                const int ny = py + dy;
                if (nx < 0 || nx >= roiLebar || ny < 0 || ny >= roiTinggi)
                  continue;
                const int tetangga = ny * roiLebar + nx;
                if (!gelap[static_cast<std::size_t>(tetangga)] ||
                    dikunjungi[static_cast<std::size_t>(tetangga)])
                  continue;
                dikunjungi[static_cast<std::size_t>(tetangga)] = 1;
                antreanGelap.push_back(tetangga);
              }
            }
          }
          const float lebarKomponen =
              static_cast<float>((kananGelap - kiriGelap + 1) * langkahFitur) /
              lebarWajah;
          const float tinggiKomponen =
              static_cast<float>((bawahGelap - atasGelap + 1) * langkahFitur) /
              tinggiWajah;
          if (jumlahGelap < 2 || lebarKomponen < minLebar ||
              lebarKomponen > maxLebar || tinggiKomponen < minTinggi ||
              tinggiKomponen > maxTinggi)
            continue;
          if (jumlahGelap > pikselTerbaik) {
            pikselTerbaik = jumlahGelap;
            tinggiTerbaik = tinggiKomponen;
          }
        }
      }
      return tinggiTerbaik;
    };

    const float mataKiri =
        tinggiFiturGelap(0.16f, 0.47f, 0.28f, 0.52f, 0.02f, 0.30f,
                         0.012f, 0.12f);
    const float mataKanan =
        tinggiFiturGelap(0.53f, 0.84f, 0.28f, 0.52f, 0.02f, 0.30f,
                         0.012f, 0.12f);
    const float mata = std::max(mataKiri, mataKanan);
    baru.eyeOpen = mata < 0.0f
                       ? 1.0f
                       : std::clamp((mata - 0.018f) / 0.065f, 0.0f, 1.0f);

    char pesan[128];
    std::snprintf(pesan, sizeof(pesan),
                  "Wajah terdeteksi; bukaan mata diestimasi dari gambar.");
    baru.status = pesan;
  } else {
    baru.aktif = false;
    baru.eyeOpen = 1.0f;
    baru.status = "Wajah belum terdeteksi. Pastikan wajah terlihat jelas, "
                  "cukup terang, dan berada di tengah kamera.";
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (baru.aktif && data_.aktif) {
    constexpr float bobotBaru = 0.35f;
    baru.eyeOpen =
        data_.eyeOpen * (1.0f - bobotBaru) + baru.eyeOpen * bobotBaru;
  }
  data_ = std::move(baru);
}

bool CameraTracker::bukaPerangkat(const std::string& path) {
  fd_ = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd_ < 0) {
    setStatus("Tidak bisa membuka kamera " + path + ": " +
              std::strerror(errno) +
              ". Periksa izin perangkat dan pastikan DroidCam sedang aktif.");
    return false;
  }
  perangkat_ = path;

  v4l2_capability capability{};
  if (!perangkatKompatibel(fd_, capability)) {
    setStatus(path +
              " bukan kamera V4L2 streaming yang kompatibel, atau tidak "
              "mendukung format gambar yang diperlukan.");
    tutupPerangkat();
    return false;
  }
  namaPerangkat_ = namaKartu(capability);

  std::uint32_t formatDipilih = 0;
  for (const std::uint32_t kandidat : formatDidukung) {
    bool tersedia = false;
    for (std::uint32_t index = 0;; ++index) {
      v4l2_fmtdesc format{};
      format.index = index;
      format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (xioctl(fd_, VIDIOC_ENUM_FMT, &format) < 0) break;
      if (format.pixelformat == kandidat) {
        tersedia = true;
        break;
      }
    }
    if (tersedia) {
      formatDipilih = kandidat;
      break;
    }
  }
  if (!formatDipilih) {
    setStatus("Kamera " + namaPerangkat_ +
              " tidak menyediakan format gambar yang didukung.");
    tutupPerangkat();
    return false;
  }

  v4l2_format format{};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = 640;
  format.fmt.pix.height = 480;
  format.fmt.pix.pixelformat = formatDipilih;
  format.fmt.pix.field = V4L2_FIELD_ANY;
  if (xioctl(fd_, VIDIOC_S_FMT, &format) < 0 ||
      format.fmt.pix.pixelformat != formatDipilih) {
    setStatus("Gagal mengatur format video kamera " + namaPerangkat_ + ": " +
              std::strerror(errno));
    tutupPerangkat();
    return false;
  }
  lebarFrame_ = static_cast<int>(format.fmt.pix.width);
  tinggiFrame_ = static_cast<int>(format.fmt.pix.height);
  format_ = format.fmt.pix.pixelformat;
  bytesPerLine_ = format.fmt.pix.bytesperline;
  if (lebarFrame_ <= 0 || tinggiFrame_ <= 0 || lebarFrame_ > 4096 ||
      tinggiFrame_ > 4096) {
    setStatus("Ukuran gambar kamera tidak valid atau terlalu besar.");
    tutupPerangkat();
    return false;
  }

  v4l2_requestbuffers request{};
  request.count = 4;
  request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  request.memory = V4L2_MEMORY_MMAP;
  if (xioctl(fd_, VIDIOC_REQBUFS, &request) < 0 || request.count == 0) {
    setStatus("Kamera " + namaPerangkat_ +
              " tidak menyediakan buffer streaming: " +
              std::strerror(errno));
    tutupPerangkat();
    return false;
  }

  bufferMmap_.reserve(request.count);
  for (std::uint32_t index = 0; index < request.count; ++index) {
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = index;
    if (xioctl(fd_, VIDIOC_QUERYBUF, &buffer) < 0) {
      setStatus("Gagal menyiapkan buffer kamera: " +
                std::string(std::strerror(errno)));
      tutupPerangkat();
      return false;
    }
    void* alamat = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd_, buffer.m.offset);
    if (alamat == MAP_FAILED) {
      setStatus("Gagal memetakan buffer kamera: " +
                std::string(std::strerror(errno)));
      tutupPerangkat();
      return false;
    }
    bufferMmap_.push_back({alamat, buffer.length});
    if (xioctl(fd_, VIDIOC_QBUF, &buffer) < 0) {
      setStatus("Gagal mengantrekan buffer kamera: " +
                std::string(std::strerror(errno)));
      tutupPerangkat();
      return false;
    }
  }

  v4l2_buf_type tipe = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (xioctl(fd_, VIDIOC_STREAMON, &tipe) < 0) {
    setStatus("Gagal memulai streaming " + namaPerangkat_ + ": " +
              std::strerror(errno));
    tutupPerangkat();
    return false;
  }
  streaming_ = true;
  return true;
}

void CameraTracker::tutupPerangkat() {
  if (fd_ >= 0) {
    if (streaming_) {
      v4l2_buf_type tipe = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(fd_, VIDIOC_STREAMOFF, &tipe);
      streaming_ = false;
    }
    for (const BufferMmap& buffer : bufferMmap_) {
      if (buffer.alamat && buffer.panjang)
        munmap(buffer.alamat, buffer.panjang);
    }
    bufferMmap_.clear();
    v4l2_requestbuffers request{};
    request.count = 0;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    xioctl(fd_, VIDIOC_REQBUFS, &request);
    ::close(fd_);
    fd_ = -1;
  }
  lebarFrame_ = 0;
  tinggiFrame_ = 0;
  bytesPerLine_ = 0;
  format_ = 0;
  perangkat_.clear();
  namaPerangkat_.clear();
  rgbFrame_.clear();
}

std::vector<PerangkatKamera> CameraTracker::daftarPerangkat(bool paksaScan) {
  const auto sekarang = std::chrono::steady_clock::now();
  bool perluScan = paksaScan || g_perangkatCache_.empty();
  if (!perluScan) {
    const auto durasi = std::chrono::duration_cast<std::chrono::seconds>(
                            sekarang - g_waktuScanTerakhir_)
                            .count();
    perluScan = durasi >= g_intervalScanDetik;
  }

  if (perluScan) {
    std::vector<PerangkatKamera> hasil;
    char path[64];
    for (int i = 0; i < 64; ++i) {
      std::snprintf(path, sizeof(path), "/dev/video%d", i);
      struct stat info{};
      if (stat(path, &info) != 0 || !S_ISCHR(info.st_mode)) continue;
      const int fd = ::open(path, O_RDONLY | O_NONBLOCK);
      if (fd < 0) continue;

      v4l2_capability capability{};
      if (perangkatKompatibel(fd, capability))
        hasil.push_back({path, namaKartu(capability)});
      ::close(fd);
    }
    g_perangkatCache_ = std::move(hasil);
    g_waktuScanTerakhir_ = sekarang;
  }
  return g_perangkatCache_;
}

#endif
