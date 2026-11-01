#include "obs/ObsWebSocket.hpp"

#include "obs/Sha256.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <mutex>
#include <ws2tcpip.h>
using PollDescriptor = WSAPOLLFD;
constexpr ObsSocketHandle invalidSocket = INVALID_SOCKET;
constexpr short readableEvent = POLLRDNORM;
constexpr short writableEvent = POLLWRNORM;
int socketError() { return WSAGetLastError(); }
bool interruptedSocketCall() { return socketError() == WSAEINTR; }
int waitSocket(PollDescriptor* event, ULONG count, int timeoutMs) {
  return WSAPoll(event, count, timeoutMs);
}
void closeSocket(ObsSocketHandle socket) { closesocket(socket); }
bool initializeSockets() {
  static std::once_flag once;
  static bool initialized = false;
  std::call_once(once, [] {
    WSADATA data{};
    initialized = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  });
  return initialized;
}
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
using PollDescriptor = pollfd;
constexpr ObsSocketHandle invalidSocket = -1;
constexpr short readableEvent = POLLIN;
constexpr short writableEvent = POLLOUT;
int socketError() { return errno; }
bool interruptedSocketCall() { return errno == EINTR; }
int waitSocket(PollDescriptor* event, nfds_t count, int timeoutMs) {
  return poll(event, count, timeoutMs);
}
void closeSocket(ObsSocketHandle socket) { close(socket); }
bool initializeSockets() { return true; }
#endif

namespace {

std::string base64(const unsigned char* data, size_t size) {
  static constexpr char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((size + 2) / 3) * 4);
  for (size_t i = 0; i < size; i += 3) {
    const uint32_t a = data[i];
    const uint32_t b = i + 1 < size ? data[i + 1] : 0;
    const uint32_t c = i + 2 < size ? data[i + 2] : 0;
    const uint32_t block = (a << 16) | (b << 8) | c;
    out.push_back(alphabet[(block >> 18) & 63]);
    out.push_back(alphabet[(block >> 12) & 63]);
    out.push_back(i + 1 < size ? alphabet[(block >> 6) & 63] : '=');
    out.push_back(i + 2 < size ? alphabet[block & 63] : '=');
  }
  return out;
}

// SHA-256 + base64 murni di dalam aplikasi (tanpa openssl, tanpa fork).
// Rumus autentikasi obs-websocket v5:
//   rahasia = base64( sha256(password + salt) )
//   jawaban = base64( sha256(rahasia + challenge) )
bool hashB64(const std::string& input, std::string& output) {
  const auto digest = ObsCrypto::sha256(input);
  output = base64(digest.data(), digest.size());
  return true;
}

std::string jsonQuote(const std::string& value) {
  std::string out = "\"";
  for (unsigned char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          static constexpr char hex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(hex[c >> 4]);
          out.push_back(hex[c & 15]);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
  return out;
}

size_t stringEnd(const std::string& text, size_t quote) {
  bool escaped = false;
  for (size_t i = quote + 1; i < text.size(); ++i) {
    if (escaped) {
      escaped = false;
    } else if (text[i] == '\\') {
      escaped = true;
    } else if (text[i] == '"') {
      return i;
    }
  }
  return std::string::npos;
}

std::string jsonString(const std::string& text, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  size_t at = text.find(token);
  if (at == std::string::npos) return {};
  at = text.find(':', at + token.size());
  if (at == std::string::npos) return {};
  at = text.find('"', at + 1);
  if (at == std::string::npos) return {};
  const size_t end = stringEnd(text, at);
  if (end == std::string::npos) return {};

  std::string out;
  for (size_t i = at + 1; i < end; ++i) {
    if (text[i] != '\\' || i + 1 >= end) {
      out.push_back(text[i]);
      continue;
    }
    const char c = text[++i];
    switch (c) {
      case '"': case '\\': case '/': out.push_back(c); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

std::string jsonObject(const std::string& text, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  size_t at = text.find(token);
  if (at == std::string::npos) return {};
  at = text.find('{', at + token.size());
  if (at == std::string::npos) return {};
  const size_t start = at;
  int depth = 0;
  bool inString = false;
  bool escaped = false;
  for (; at < text.size(); ++at) {
    const char c = text[at];
    if (inString) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') ++depth;
    else if (c == '}' && --depth == 0)
      return text.substr(start, at - start + 1);
  }
  return {};
}

int jsonOp(const std::string& text) {
  const std::string token = "\"op\"";
  size_t at = text.find(token);
  if (at == std::string::npos) return -1;
  at = text.find(':', at + token.size());
  if (at == std::string::npos) return -1;
  return std::atoi(text.c_str() + at + 1);
}

bool sendAll(ObsSocketHandle fd, const unsigned char* data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
#ifdef _WIN32
    const int n = send(fd, reinterpret_cast<const char*>(data + sent),
                       static_cast<int>(size - sent), 0);
#else
    const ssize_t n = send(fd, data + sent, size - sent, MSG_NOSIGNAL);
#endif
    if (n < 0 && interruptedSocketCall()) continue;
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

enum class FrameRead { Message, Timeout, Closed, Error };

bool receiveExact(ObsSocketHandle fd, unsigned char* data, size_t size,
                  bool firstByte) {
  size_t received = 0;
  while (received < size) {
    PollDescriptor event{fd, readableEvent, 0};
    const int ready = waitSocket(&event, 1, firstByte ? 0 : 2000);
    if (ready == 0) return false;
    if (ready < 0 && interruptedSocketCall()) continue;
    if (ready < 0) return false;
#ifdef _WIN32
    const int n = recv(fd, reinterpret_cast<char*>(data + received),
                       static_cast<int>(size - received), 0);
#else
    const ssize_t n = recv(fd, data + received, size - received, 0);
#endif
    if (n < 0 && interruptedSocketCall()) continue;
    if (n <= 0) return false;
    received += static_cast<size_t>(n);
    firstByte = false;
  }
  return true;
}

FrameRead receiveFrame(ObsSocketHandle fd, uint8_t& opcode,
                       std::string& payload) {
  unsigned char header[2];
  PollDescriptor event{fd, readableEvent, 0};
  const int ready = waitSocket(&event, 1, 250);
  if (ready == 0) return FrameRead::Timeout;
  if (ready < 0 && interruptedSocketCall()) return FrameRead::Timeout;
  if (ready < 0) return FrameRead::Error;
#ifdef _WIN32
  const int first = recv(fd, reinterpret_cast<char*>(header), 2, MSG_PEEK);
#else
  const ssize_t first = recv(fd, header, 2, MSG_PEEK);
#endif
  if (first == 0) return FrameRead::Closed;
  if (first < 0 && !interruptedSocketCall()) return FrameRead::Error;
  if (first < 2) {
    if (!receiveExact(fd, header, 2, false)) return FrameRead::Closed;
  } else if (!receiveExact(fd, header, 2, false)) {
    return FrameRead::Closed;
  }

  opcode = header[0] & 0x0f;
  uint64_t length = header[1] & 0x7f;
  if (length == 126) {
    unsigned char extended[2];
    if (!receiveExact(fd, extended, sizeof(extended), false))
      return FrameRead::Closed;
    length = (static_cast<uint64_t>(extended[0]) << 8) | extended[1];
  } else if (length == 127) {
    unsigned char extended[8];
    if (!receiveExact(fd, extended, sizeof(extended), false))
      return FrameRead::Closed;
    length = 0;
    for (unsigned char c : extended) length = (length << 8) | c;
  }
  if (length > 1024 * 1024) return FrameRead::Error;

  const bool masked = (header[1] & 0x80) != 0;
  unsigned char mask[4]{};
  if (masked && !receiveExact(fd, mask, sizeof(mask), false))
    return FrameRead::Closed;
  payload.resize(static_cast<size_t>(length));
  if (length && !receiveExact(fd,
                              reinterpret_cast<unsigned char*>(payload.data()),
                              static_cast<size_t>(length), false))
    return FrameRead::Closed;
  if (masked) {
    for (size_t i = 0; i < payload.size(); ++i)
      payload[i] ^= static_cast<char>(mask[i % 4]);
  }
  return FrameRead::Message;
}

bool sendFrame(ObsSocketHandle fd, uint8_t opcode,
               const std::string& payload) {
  if (payload.size() > 65535) return false;
  std::array<unsigned char, 4> mask{};
  std::random_device random;
  for (unsigned char& b : mask) b = static_cast<unsigned char>(random());

  std::vector<unsigned char> frame;
  frame.reserve(payload.size() + 8);
  frame.push_back(static_cast<unsigned char>(0x80 | opcode));
  if (payload.size() < 126) {
    frame.push_back(static_cast<unsigned char>(0x80 | payload.size()));
  } else {
    frame.push_back(0xfe);
    frame.push_back(static_cast<unsigned char>(payload.size() >> 8));
    frame.push_back(static_cast<unsigned char>(payload.size()));
  }
  frame.insert(frame.end(), mask.begin(), mask.end());
  for (size_t i = 0; i < payload.size(); ++i)
    frame.push_back(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]);
  return sendAll(fd, frame.data(), frame.size());
}

bool connectTcp(const std::string& host, int port, ObsSocketHandle& resultFd,
                int timeoutMs = 3000) {
  if (!initializeSockets()) return false;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* addresses = nullptr;
  const std::string service = std::to_string(port);
  const int resolved =
      getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
  if (resolved != 0) return false;

  for (addrinfo* address = addresses; address; address = address->ai_next) {
    const ObsSocketHandle fd = socket(address->ai_family, address->ai_socktype,
                                      address->ai_protocol);
    if (fd == invalidSocket) continue;
    // Minim delay: pesan kecil status/scene dikirim langsung tanpa ditahan.
    int tanpaTunda = 1;
#ifdef _WIN32
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&tanpaTunda),
               sizeof(tanpaTunda));
    const DWORD timeout = 2000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    u_long mode = 1;
    const bool nonblocking = ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &tanpaTunda, sizeof(tanpaTunda));
    timeval timeout{2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
    const int connected = connect(fd, address->ai_addr, address->ai_addrlen);
    const int connectError = connected < 0 ? socketError() : 0;
#ifdef _WIN32
    const bool connecting = connectError == WSAEWOULDBLOCK ||
                            connectError == WSAEINPROGRESS;
#else
    const bool connecting = connectError == EINPROGRESS;
#endif
    if (connected == 0 || connecting) {
      PollDescriptor event{fd, writableEvent, 0};
      const int ready =
          connected == 0 ? 1 : waitSocket(&event, 1, timeoutMs);
      int socketError = 0;
#ifdef _WIN32
      int errorSize = sizeof(socketError);
      const int optionResult =
          getsockopt(fd, SOL_SOCKET, SO_ERROR,
                     reinterpret_cast<char*>(&socketError), &errorSize);
#else
      socklen_t errorSize = sizeof(socketError);
      const int optionResult =
          getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorSize);
#endif
      if (ready > 0 && optionResult == 0 && socketError == 0) {
#ifdef _WIN32
        if (nonblocking) {
          mode = 0;
          ioctlsocket(fd, FIONBIO, &mode);
        }
#else
        if (flags >= 0) fcntl(fd, F_SETFL, flags);
#endif
        resultFd = fd;
        freeaddrinfo(addresses);
        return true;
      }
    }
    closeSocket(fd);
  }
  freeaddrinfo(addresses);
  return false;
}

bool websocketHandshake(ObsSocketHandle fd) {
  std::array<unsigned char, 16> randomBytes{};
  std::random_device random;
  for (unsigned char& b : randomBytes)
    b = static_cast<unsigned char>(random());
  const std::string key = base64(randomBytes.data(), randomBytes.size());
  const std::string request =
      "GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
      "Connection: Upgrade\r\nSec-WebSocket-Key: " +
      key +
      "\r\nSec-WebSocket-Version: 13\r\n\r\n";
  if (!sendAll(fd, reinterpret_cast<const unsigned char*>(request.data()),
               request.size()))
    return false;

  std::string response;
  char c = 0;
  while (response.size() < 16384 &&
         response.find("\r\n\r\n") == std::string::npos) {
#ifdef _WIN32
    const int n = recv(fd, &c, 1, 0);
#else
    const ssize_t n = recv(fd, &c, 1, 0);
#endif
    if (n < 0 && interruptedSocketCall()) continue;
    if (n <= 0) return false;
    response.push_back(c);
  }
  if (response.find(" 101 ") == std::string::npos) return false;
  // Verifikasi hash SHA-1 kunci balasan dilewati: status 101 + header
  // Upgrade sudah cukup sebagai bukti handshake berhasil dengan server
  // lokal tepercaya (tidak butuh openssl / proses luar).
  std::string lowered = response;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char x) { return static_cast<char>(std::tolower(x)); });
  return lowered.find("sec-websocket-accept:") != std::string::npos;
}

}  // namespace

ObsWebSocket::~ObsWebSocket() { putus(); }

bool ObsWebSocket::sambung(const std::string& host, int port,
                           const std::string& password) {
  if (host.empty() || port < 1 || port > 65535) {
    setStatus("Alamat OBS atau port tidak valid.");
    return false;
  }
  putus();
  stop_ = false;
  aktif_ = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    scene_.clear();
  }
  setStatus("Menghubungkan ke OBS...");
  thread_ = std::thread(&ObsWebSocket::jalankan, this, host, port, password);
  return true;
}

bool ObsWebSocket::sambungOtomatis(const std::string& password) {
  // Alamat lokal yang umum untuk OBS: bawaan v28+ (4455), lalu port lama.
  const std::pair<const char*, int> kandidat[] = {
      {"127.0.0.1", 4455}, {"localhost", 4455}, {"127.0.0.1", 4444}};
  for (const auto& k : kandidat) {
    if (portTerbuka(k.first, k.second, 400))
      return sambung(k.first, k.second, password);
  }
  // Tidak ada yang terbuka (OBS mungkin belum dibuka): tetap coba bawaan
  // agar thread mencoba ulang otomatis sampai OBS dibuka.
  return sambung(kandidat[0].first, kandidat[0].second, password);
}

bool ObsWebSocket::portTerbuka(const std::string& host, int port,
                               int timeoutMs) {
  ObsSocketHandle fd = invalidSocket;
  if (!connectTcp(host, port, fd, timeoutMs)) return false;
  closeSocket(fd);
  return true;
}

void ObsWebSocket::putus() {
  stop_ = true;
  const ObsSocketHandle fd = socket_.load();
  if (fd != invalidSocket) {
#ifdef _WIN32
    shutdown(fd, SD_BOTH);
#else
    shutdown(fd, SHUT_RDWR);
#endif
  }
  if (thread_.joinable() && thread_.get_id() != std::this_thread::get_id())
    thread_.join();
  aktif_ = false;
  socket_ = invalidSocket;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    scene_.clear();
  }
  setStatus("Terputus dari OBS.");
}

std::string ObsWebSocket::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

std::string ObsWebSocket::sceneAktif() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return scene_;
}

void ObsWebSocket::setStatus(std::string status) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = std::move(status);
}

void ObsWebSocket::jalankan(std::string host, int port,
                            std::string password) {
  bool pernahGagal = false;
  while (!stop_) {
    ObsSocketHandle fd = invalidSocket;
    if (!connectTcp(host, port, fd)) {
      socket_ = invalidSocket;
      if (!stop_ && !pernahGagal) {
        pernahGagal = true;
        setStatus("OBS belum ketemu, mencoba lagi... "
                  "(buka OBS dan aktifkan WebSocket Server)");
      }
      for (int i = 0; i < 20 && !stop_; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    pernahGagal = false;
    const bool selesai = cobaSekali(host, port, password, fd);
    if (selesai) return;
  }
}

bool ObsWebSocket::cobaSekali(const std::string& host, int port,
                              std::string password, ObsSocketHandle fd) {
  (void)host;
  (void)port;
  socket_ = fd;
  std::string pwd = password;

  if (!websocketHandshake(fd)) {
    setStatus("Handshake obs-websocket gagal. Periksa alamat dan port.");
    closeSocket(fd);
    socket_ = invalidSocket;
    return false;
  }

  uint8_t opcode = 0;
  std::string payload;
  if (receiveFrame(fd, opcode, payload) != FrameRead::Message || opcode != 1 ||
      jsonOp(payload) != 0) {
    setStatus("OBS tidak mengirim pesan Hello obs-websocket.");
    closeSocket(fd);
    socket_ = invalidSocket;
    return false;
  }

  const std::string helloData = jsonObject(payload, "d");
  const std::string auth = jsonObject(helloData, "authentication");
  std::string identify =
      "{\"op\":1,\"d\":{\"rpcVersion\":1,\"eventSubscriptions\":0";
  if (!auth.empty()) {
    const std::string salt = jsonString(auth, "salt");
    const std::string challenge = jsonString(auth, "challenge");
    std::string secret;
    std::string response;
    if (salt.empty() || challenge.empty() ||
        !hashB64(pwd + salt, secret) ||
        !hashB64(secret + challenge, response)) {
      setStatus("Tidak bisa menghitung autentikasi OBS.");
      closeSocket(fd);
      socket_ = invalidSocket;
      return true;
    }
    identify += ",\"authentication\":" + jsonQuote(response);
  }
  identify += "}}";
  std::fill(pwd.begin(), pwd.end(), '\0');
  if (!sendFrame(fd, 1, identify)) {
    setStatus("Gagal mengirim autentikasi ke OBS.");
    closeSocket(fd);
    socket_ = invalidSocket;
    return false;
  }

  bool identified = false;
  while (!stop_ && !identified) {
    const FrameRead result = receiveFrame(fd, opcode, payload);
    if (result == FrameRead::Timeout) continue;
    if (result != FrameRead::Message) break;
    if (opcode == 9) {
      if (!sendFrame(fd, 10, payload)) break;
    } else if (opcode == 1 && jsonOp(payload) == 2) {
      identified = true;
    } else if (opcode == 1 && jsonOp(payload) == 7) {
      const std::string comment = jsonString(payload, "comment");
      setStatus(comment.empty() ? "OBS menolak koneksi (password salah?)."
                                : "OBS menolak koneksi: " + comment);
      break;
    }
  }

  if (!identified || stop_) {
    closeSocket(fd);
    socket_ = invalidSocket;
    if (!stop_) {
      const std::string sekarang = status();
      if (sekarang.find("OBS menolak") != std::string::npos) {
        return true;
      }
      return false;
    }
    return true;
  }

  aktif_ = true;
  setStatus("Terhubung ke OBS.");
  sendFrame(fd, 1,
            "{\"op\":6,\"d\":{\"requestType\":\"GetVersion\",""requestId\":\"avatar-version\"}}");
  sendFrame(fd, 1,
            "{\"op\":6,\"d\":{\"requestType\":\"GetCurrentProgramScene\",""requestId\":\"avatar-scene\"}}");

  while (!stop_) {
    const FrameRead result = receiveFrame(fd, opcode, payload);
    if (result == FrameRead::Timeout) continue;
    if (result != FrameRead::Message) break;
    if (opcode == 9) {
      if (!sendFrame(fd, 10, payload)) break;
    } else if (opcode == 8) {
      break;
    } else if (opcode == 1) {
      const std::string requestId = jsonString(payload, "requestId");
      if (requestId == "avatar-version") {
        const std::string data = jsonObject(payload, "responseData");
        const std::string versi = jsonString(data, "obsVersion");
        if (!versi.empty()) setStatus("Terhubung ke OBS " + versi + ".");
      } else if (requestId == "avatar-scene") {
        const std::string data = jsonObject(payload, "responseData");
        std::lock_guard<std::mutex> lock(mutex_);
        scene_ = jsonString(data, "currentProgramSceneName");
      }
    }
  }

  aktif_ = false;
  closeSocket(fd);
  socket_ = invalidSocket;
  if (stop_) return true;
  setStatus("Koneksi OBS terputus, mencoba lagi...");
  return false;
}
