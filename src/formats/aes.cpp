#include "formats/aes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Aes {

namespace {

constexpr int kRounds = 14;
constexpr int kKeyWords = 8;
constexpr size_t kExpandedWords = (size_t)4 * (size_t)(kRounds + 1);

uint8_t XTime(uint8_t v) {
    return (uint8_t)((uint32_t)(v << 1U) ^ ((((uint32_t)v >> 7U) & 1U) * 0x1BU));
}

uint8_t Mul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    while (b != 0) {
        if ((b & 1U) != 0) r ^= a;
        a = XTime(a);
        b >>= 1U;
    }
    return r;
}

uint8_t Rotl8(uint8_t v, int n) {
    return (uint8_t)(((uint32_t)v << (uint32_t)n) | ((uint32_t)v >> (uint32_t)(8 - n)));
}

constexpr size_t kTableSize = 256;

struct Sboxes {
    std::array<uint8_t, kTableSize> forward{};
    std::array<uint8_t, kTableSize> inverse{};

    Sboxes() {
        std::array<uint8_t, kTableSize> pow_storage{};
        std::array<uint8_t, kTableSize> log_storage{};
        const std::span<uint8_t> pow(pow_storage);
        const std::span<uint8_t> log(log_storage);
        const std::span<uint8_t> fwd(forward);
        const std::span<uint8_t> inv(inverse);

        uint8_t x = 1;
        for (size_t i = 0; i < kTableSize - 1; i++) {
            pow[i] = x;
            log[x] = (uint8_t)i;
            x = (uint8_t)(x ^ XTime(x));
        }
        fwd[0] = 0x63;
        for (size_t a = 1; a < kTableSize; a++) {
            const uint8_t reciprocal = pow[(kTableSize - 1 - log[a]) % (kTableSize - 1)];
            fwd[a] = (uint8_t)(reciprocal ^ Rotl8(reciprocal, 1) ^ Rotl8(reciprocal, 2) ^
                               Rotl8(reciprocal, 3) ^ Rotl8(reciprocal, 4) ^ 0x63U);
        }
        for (size_t i = 0; i < kTableSize; i++)
            inv[fwd[i]] = (uint8_t)i;
    }
};

const Sboxes& Boxes() {
    static const Sboxes boxes;
    return boxes;
}

std::span<const uint8_t> Forward() {
    return {Boxes().forward};
}

std::span<const uint8_t> Inverse() {
    return {Boxes().inverse};
}

using Schedule = std::array<uint8_t, kExpandedWords * 4>;

Schedule ExpandKey(std::span<const uint8_t> key) {
    const std::span<const uint8_t> sbox = Forward();
    Schedule storage{};
    const std::span<uint8_t> w(storage);
    std::ranges::copy(key.first(kKeyBytes), w.begin());

    uint8_t rcon = 1;
    for (size_t i = kKeyWords; i < kExpandedWords; i++) {
        std::array<uint8_t, 4> word_storage{};
        const std::span<uint8_t> t(word_storage);
        std::ranges::copy(w.subspan((i - 1) * 4, 4), t.begin());
        if ((i % kKeyWords) == 0) {
            const uint8_t first = t[0];
            t[0] = (uint8_t)(sbox[t[1]] ^ rcon);
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[first];
            rcon = XTime(rcon);
        } else if ((i % kKeyWords) == 4) {
            for (auto& b : t)
                b = sbox[b];
        }
        for (size_t j = 0; j < 4; j++)
            w[(i * 4) + j] = (uint8_t)(w[((i - kKeyWords) * 4) + j] ^ t[j]);
    }
    return storage;
}

void AddRoundKey(std::span<uint8_t, kBlockBytes> state, const Schedule& schedule, size_t round) {
    const std::span<const uint8_t> w(schedule);
    for (size_t i = 0; i < kBlockBytes; i++)
        state[i] ^= w[(round * kBlockBytes) + i];
}

void InvShiftRows(std::span<uint8_t, kBlockBytes> s) {
    std::array<uint8_t, kBlockBytes> copy{};
    const std::span<uint8_t> in(copy);
    std::ranges::copy(s, in.begin());
    for (size_t col = 0; col < 4; col++) {
        for (size_t row = 0; row < 4; row++)
            s[(col * 4) + row] = in[(((col + 4 - row) % 4) * 4) + row];
    }
}

void InvSubBytes(std::span<uint8_t, kBlockBytes> s) {
    const std::span<const uint8_t> inv = Inverse();
    for (auto& b : s)
        b = inv[b];
}

void InvMixColumns(std::span<uint8_t, kBlockBytes> s) {
    for (size_t c = 0; c < 4; c++) {
        const size_t o = c * 4;
        const uint8_t a0 = s[o];
        const uint8_t a1 = s[o + 1];
        const uint8_t a2 = s[o + 2];
        const uint8_t a3 = s[o + 3];
        s[o] = (uint8_t)(Mul(a0, 14) ^ Mul(a1, 11) ^ Mul(a2, 13) ^ Mul(a3, 9));
        s[o + 1] = (uint8_t)(Mul(a0, 9) ^ Mul(a1, 14) ^ Mul(a2, 11) ^ Mul(a3, 13));
        s[o + 2] = (uint8_t)(Mul(a0, 13) ^ Mul(a1, 9) ^ Mul(a2, 14) ^ Mul(a3, 11));
        s[o + 3] = (uint8_t)(Mul(a0, 11) ^ Mul(a1, 13) ^ Mul(a2, 9) ^ Mul(a3, 14));
    }
}

void DecryptBlock(const Schedule& w, std::span<const uint8_t, kBlockBytes> in,
                  std::span<uint8_t, kBlockBytes> out) {
    std::array<uint8_t, kBlockBytes> state{};
    std::span<uint8_t, kBlockBytes> s(state);
    std::ranges::copy(in, s.begin());

    AddRoundKey(s, w, kRounds);
    for (size_t round = kRounds - 1; round > 0; round--) {
        InvShiftRows(s);
        InvSubBytes(s);
        AddRoundKey(s, w, round);
        InvMixColumns(s);
    }
    InvShiftRows(s);
    InvSubBytes(s);
    AddRoundKey(s, w, 0);
    std::ranges::copy(s, out.begin());
}

void XorInto(std::span<uint8_t> dst, std::span<const uint8_t> src) {
    for (size_t i = 0; i < dst.size(); i++)
        dst[i] ^= src[i];
}

std::span<const uint8_t, kBlockBytes> BlockAt(std::span<const uint8_t> buf, size_t at) {
    return buf.subspan(at).first<kBlockBytes>();
}

std::span<uint8_t, kBlockBytes> MutBlockAt(std::span<uint8_t> buf, size_t at) {
    return buf.subspan(at).first<kBlockBytes>();
}

}

bool DecryptCbcCts(std::span<const uint8_t> key, std::span<const uint8_t> iv,
                   std::span<const uint8_t> cipher, std::vector<uint8_t>& out, std::string& err) {
    if (key.size() != kKeyBytes) {
        err = "AES key must be " + std::to_string(kKeyBytes) + " bytes";
        return false;
    }
    if (iv.size() != kBlockBytes) {
        err = "AES IV must be " + std::to_string(kBlockBytes) + " bytes";
        return false;
    }
    if (cipher.size() < kBlockBytes) {
        err = "ciphertext is shorter than one AES block";
        return false;
    }

    const Schedule w = ExpandKey(key);
    const size_t tail = cipher.size() % kBlockBytes;
    const size_t whole = cipher.size() - tail;
    const size_t body = (tail == 0) ? whole : (whole - kBlockBytes);

    out.assign(cipher.size(), 0);
    const std::span<uint8_t> plain(out);
    std::span<const uint8_t, kBlockBytes> prev = iv.first<kBlockBytes>();
    for (size_t at = 0; at < body; at += kBlockBytes) {
        DecryptBlock(w, BlockAt(cipher, at), MutBlockAt(plain, at));
        XorInto(plain.subspan(at, kBlockBytes), prev);
        prev = BlockAt(cipher, at);
    }
    if (tail == 0) return true;

    std::array<uint8_t, kBlockBytes> stolen{};
    const std::span<uint8_t, kBlockBytes> stolen_span(stolen);
    DecryptBlock(w, BlockAt(cipher, body), stolen_span);

    const std::span<const uint8_t> cipher_tail = cipher.subspan(body + kBlockBytes, tail);
    const std::span<uint8_t> plain_tail = plain.subspan(body + kBlockBytes, tail);
    for (size_t i = 0; i < tail; i++)
        plain_tail[i] = (uint8_t)(stolen_span[i] ^ cipher_tail[i]);

    std::array<uint8_t, kBlockBytes> merged{};
    const std::span<uint8_t, kBlockBytes> merged_span(merged);
    std::ranges::copy(cipher_tail, merged_span.begin());
    std::ranges::copy(stolen_span.subspan(tail), merged_span.subspan(tail).begin());
    DecryptBlock(w, merged_span, MutBlockAt(plain, body));
    XorInto(plain.subspan(body, kBlockBytes), prev);
    return true;
}

}
