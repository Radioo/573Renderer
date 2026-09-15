#include "formats/avs_lz77.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace AvsLz77 {

namespace {

constexpr int kWindow = 4096;
constexpr int kWindowMask = kWindow - 1;
constexpr int kMaxMatch = 18;
constexpr int kLongestLiteralRun = 2;
constexpr int kMinMatch = 3;
constexpr int kNil = kWindow;
constexpr int kStart = kWindow - kMaxMatch;
constexpr int kRoots = 256;
constexpr std::size_t kGroupBytes = 17;
constexpr uint32_t kPosMask = 0xFFFU;

class Encoder {
public:
    explicit Encoder(std::span<const uint8_t> src)
        : src_(src), text_(kWindow + kMaxMatch - 1, 0), lson_(kWindow + 1 + kRoots, kNil),
          rson_(kWindow + 1 + kRoots, kNil), dad_(kWindow + 1 + kRoots, kNil) {}

    std::vector<uint8_t> Run();

private:
    void Insert(int r);
    void ReplaceNode(int p, int r);
    void Delete(int p);
    int EmitItem();
    void Slide(uint8_t c);
    void Drain();
    void Finish();

    std::span<const uint8_t> src_;
    std::size_t in_ = 0;
    std::vector<uint8_t> text_;
    std::vector<int> lson_;
    std::vector<int> rson_;
    std::vector<int> dad_;
    int match_length_ = 0;
    int match_position_ = 0;
    int s_ = 0;
    int r_ = kStart;
    int length_ = 0;
    std::array<uint8_t, kGroupBytes> group_{};
    std::size_t group_size_ = 1;
    uint8_t mask_ = 1;
    std::vector<uint8_t> out_;
};

void Encoder::Insert(int r) {
    int p = kWindow + 1 + text_[static_cast<std::size_t>(r)];
    int cmp = 1;
    rson_[r] = kNil;
    lson_[r] = kNil;
    match_length_ = 0;
    for (;;) {
        std::vector<int>& child = cmp >= 0 ? rson_ : lson_;
        if (child[p] == kNil) {
            child[p] = r;
            dad_[r] = p;
            return;
        }
        p = child[p];
        int i = 1;
        for (; i < kMaxMatch; i++) {
            cmp = static_cast<int>(text_[static_cast<std::size_t>(r + i)]) -
                  static_cast<int>(text_[static_cast<std::size_t>(p + i)]);
            if (cmp != 0) break;
        }
        if (i > match_length_) {
            match_position_ = p;
            match_length_ = i;
            if (i >= kMaxMatch) break;
        }
    }
    ReplaceNode(p, r);
}

void Encoder::ReplaceNode(int p, int r) {
    dad_[r] = dad_[p];
    lson_[r] = lson_[p];
    rson_[r] = rson_[p];
    dad_[lson_[p]] = r;
    dad_[rson_[p]] = r;
    if (rson_[dad_[p]] == p) {
        rson_[dad_[p]] = r;
    } else {
        lson_[dad_[p]] = r;
    }
    dad_[p] = kNil;
}

void Encoder::Delete(int p) {
    if (dad_[p] == kNil) return;
    int q = 0;
    if (rson_[p] == kNil) {
        q = lson_[p];
    } else if (lson_[p] == kNil) {
        q = rson_[p];
    } else {
        q = lson_[p];
        if (rson_[q] != kNil) {
            while (rson_[q] != kNil)
                q = rson_[q];
            rson_[dad_[q]] = lson_[q];
            dad_[lson_[q]] = dad_[q];
            lson_[q] = lson_[p];
            dad_[lson_[p]] = q;
        }
        rson_[q] = rson_[p];
        dad_[rson_[p]] = q;
    }
    dad_[q] = dad_[p];
    if (rson_[dad_[p]] == p) {
        rson_[dad_[p]] = q;
    } else {
        lson_[dad_[p]] = q;
    }
    dad_[p] = kNil;
}

int Encoder::EmitItem() {
    int length = std::min(match_length_, length_);
    if (length <= kLongestLiteralRun) {
        length = 1;
        group_[0] = static_cast<uint8_t>(group_[0] | mask_);
        group_.at(group_size_++) = text_[static_cast<std::size_t>(r_)];
    } else {
        const auto token = static_cast<uint32_t>((((r_ - match_position_) & kWindowMask) << 4) |
                                                 (length - kMinMatch));
        group_.at(group_size_++) = static_cast<uint8_t>(token >> 8U);
        group_.at(group_size_++) = static_cast<uint8_t>(token & 0xFFU);
    }
    mask_ = static_cast<uint8_t>(mask_ << 1U);
    if (mask_ == 0) {
        out_.insert(out_.end(), group_.begin(),
                    group_.begin() + static_cast<std::ptrdiff_t>(group_size_));
        group_[0] = 0;
        group_size_ = 1;
        mask_ = 1;
    }
    return length;
}

void Encoder::Slide(uint8_t c) {
    Delete(s_);
    text_[static_cast<std::size_t>(s_)] = c;
    if (s_ < kMaxMatch - 1) text_[static_cast<std::size_t>(s_ + kWindow)] = c;
    s_ = (s_ + 1) & kWindowMask;
    r_ = (r_ + 1) & kWindowMask;
    Insert(r_);
}

void Encoder::Drain() {
    Delete(s_);
    s_ = (s_ + 1) & kWindowMask;
    r_ = (r_ + 1) & kWindowMask;
    length_--;
    if (length_ != 0) Insert(r_);
}

void Encoder::Finish() {
    group_.at(group_size_++) = 0;
    group_.at(group_size_++) = 0;
    out_.insert(out_.end(), group_.begin(),
                group_.begin() + static_cast<std::ptrdiff_t>(group_size_));
}

std::vector<uint8_t> Encoder::Run() {
    out_.reserve(src_.size() + (src_.size() / 8) + kGroupBytes);
    while (length_ < kMaxMatch && in_ < src_.size()) {
        text_[static_cast<std::size_t>(r_ + length_)] = src_[in_];
        in_++;
        length_++;
    }
    if (length_ != 0) {
        for (int i = 1; i <= kMaxMatch; i++)
            Insert(r_ - i);
        Insert(r_);
    }
    while (length_ != 0) {
        const int last = EmitItem();
        int i = 0;
        for (; i < last && in_ < src_.size(); i++)
            Slide(src_[in_++]);
        for (; i < last; i++)
            Drain();
    }
    Finish();
    return std::move(out_);
}

}

std::vector<uint8_t> Decompress(std::span<const uint8_t> src, std::size_t expected_size) {
    std::vector<uint8_t> out;
    if (expected_size != 0) out.reserve(expected_size);

    std::array<uint8_t, kWindow> window{};
    auto pos = static_cast<uint32_t>(kStart);

    std::size_t si = 0;
    uint32_t flags = 0;
    int flagbits = 0;

    while (si < src.size()) {
        if (flagbits == 0) {
            flags = src[si++];
            flagbits = 8;
            if (si >= src.size()) break;
        }
        const bool literal = (flags & 1U) != 0;
        flags >>= 1U;
        flagbits--;

        if (literal) {
            const uint8_t b = src[si++];
            out.push_back(b);
            window.at(pos) = b;
            pos = (pos + 1) & kPosMask;
        } else {
            if (si + 1 >= src.size()) break;
            const uint32_t token = (static_cast<uint32_t>(src[si]) << 8U) | src[si + 1];
            si += 2;
            const uint32_t distance = token >> 4U;
            if (distance == 0) break;
            const uint32_t length = (token & 0xFU) + static_cast<uint32_t>(kMinMatch);
            const uint32_t from = (pos - distance) & kPosMask;
            for (uint32_t i = 0; i < length; i++) {
                const uint8_t c = window.at((from + i) & kPosMask);
                out.push_back(c);
                window.at(pos) = c;
                pos = (pos + 1) & kPosMask;
            }
        }
        if (expected_size != 0 && out.size() >= expected_size) break;
    }
    return out;
}

std::vector<uint8_t> Compress(std::span<const uint8_t> src) {
    return Encoder(src).Run();
}

}
