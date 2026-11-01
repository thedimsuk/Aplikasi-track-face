#pragma once

// ---------------------------------------------------------------------------
// SHA-256 mandiri (tanpa openssl / tanpa fork proses).
// Implementasi ringkas domain-publik mengikuti standar FIPS-180-4.
// Dipakai hanya untuk autentikasi obs-websocket:
//   rahasia   = base64( sha256(password + salt) )
//   jawaban   = base64( sha256(rahasia + challenge) )
// ---------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ObsCrypto {

inline std::array<unsigned char, 32> sha256(const unsigned char* data,
                                            size_t panjang) {
  static constexpr uint32_t K[64] = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  auto putarKanan = [](uint32_t v, unsigned n) {
    return (v >> n) | (v << (32 - n));
  };

  uint32_t h0 = 0x6a09e667u, h1 = 0xbb67ae85u, h2 = 0x3c6ef372u,
           h3 = 0xa54ff53au, h4 = 0x510e527fu, h5 = 0x9b05688cu,
           h6 = 0x1f83d9abu, h7 = 0x5be0cd19u;

  // Penambahan padding: data + 0x80 + nol + panjang-bit (64 bit, big-endian).
  const uint64_t panjangBit = static_cast<uint64_t>(panjang) * 8u;
  std::vector<unsigned char> pesan(data, data + panjang);
  pesan.push_back(0x80);
  while (pesan.size() % 64 != 56) pesan.push_back(0x00);
  for (int i = 7; i >= 0; --i)
    pesan.push_back(static_cast<unsigned char>((panjangBit >> (i * 8)) & 0xFFu));

  // Proses tiap blok 512-bit.
  for (size_t blok = 0; blok < pesan.size(); blok += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(pesan[blok + i * 4]) << 24) |
             (static_cast<uint32_t>(pesan[blok + i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(pesan[blok + i * 4 + 2]) << 8) |
             (static_cast<uint32_t>(pesan[blok + i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 =
          putarKanan(w[i - 15], 7) ^ putarKanan(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 =
          putarKanan(w[i - 2], 17) ^ putarKanan(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, hh = h7;
    for (int i = 0; i < 64; ++i) {
      const uint32_t S1 = putarKanan(e, 6) ^ putarKanan(e, 11) ^ putarKanan(e, 25);
      const uint32_t ch = (e & f) ^ ((~e) & g);
      const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
      const uint32_t S0 = putarKanan(a, 2) ^ putarKanan(a, 13) ^ putarKanan(a, 22);
      const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t t2 = S0 + mj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
    h5 += f;
    h6 += g;
    h7 += hh;
  }

  std::array<unsigned char, 32> hasil{};
  const uint32_t h[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
  for (int i = 0; i < 8; ++i) {
    hasil[i * 4] = static_cast<unsigned char>((h[i] >> 24) & 0xFFu);
    hasil[i * 4 + 1] = static_cast<unsigned char>((h[i] >> 16) & 0xFFu);
    hasil[i * 4 + 2] = static_cast<unsigned char>((h[i] >> 8) & 0xFFu);
    hasil[i * 4 + 3] = static_cast<unsigned char>(h[i] & 0xFFu);
  }
  return hasil;
}

inline std::array<unsigned char, 32> sha256(const std::string& teks) {
  return sha256(reinterpret_cast<const unsigned char*>(teks.data()), teks.size());
}

}  // namespace ObsCrypto
