#include "support/math/float_trig.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Support {

namespace {

constexpr double kS1 = -0x1.5555554cbac77p-3;
constexpr double kS2 = 0x1.11110896efbb2p-7;
constexpr double kS3 = -0x1.a00f9e2cae774p-13;
constexpr double kS4 = 0x1.6cd878c3b46a7p-19;

constexpr double kC0 = -0x1.ffffffd0c5e81p-2;
constexpr double kC1 = 0x1.55553e1053a42p-5;
constexpr double kC2 = -0x1.6c087e80f1e27p-10;
constexpr double kC3 = 0x1.99342e0ee5069p-16;

constexpr double kHalfPi = 1.57079632679489661923;
constexpr double kHalfPi2 = 2.0 * kHalfPi;
constexpr double kHalfPi3 = 3.0 * kHalfPi;
constexpr double kHalfPi4 = 4.0 * kHalfPi;

constexpr double kInvHalfPi = 6.36619772367581382433e-01;
constexpr double kHalfPiHead = 1.57079631090164184570e+00;
constexpr double kHalfPiTail = 1.58932547735281966916e-08;
constexpr double kRoundMagic = 0x1.8p52;

constexpr double kTwo24 = 1.67772160000000000000e+07;
constexpr double kTwoPow24Inverse = 5.96046447753906250000e-08;

constexpr std::int32_t kQuarterPi = 0x3f490fda;
constexpr std::int32_t kTinyArgument = 0x39800000;
constexpr std::int32_t kThreeQuarterPi = 0x4016cbe3;
constexpr std::int32_t kFiveQuarterPi = 0x407b53d1;
constexpr std::int32_t kSevenQuarterPi = 0x40afeddf;
constexpr std::int32_t kNineQuarterPi = 0x40e231d5;
constexpr std::int32_t kMediumLimit = 0x4dc90fdb;
constexpr std::int32_t kInfinity = 0x7f800000;
constexpr std::int32_t kSignMask = 0x7fffffff;

constexpr int kReductionTerms = 3;
constexpr int kHalfPiTerms = 4;
constexpr int kWorkingSize = 20;

constexpr std::array<std::int32_t, 66> kTwoOverPi = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163,
    0xABDEBB, 0xC561B7, 0x246E3A, 0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
    0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C,
    0x845F8B, 0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
    0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7, 0x8A5292,
    0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
    0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA,
    0x73A8C9, 0x60E27B, 0xC08C6B};

constexpr std::array<double, (std::size_t)kHalfPiTerms> kHalfPiParts = {
    1.57079625129699707031e+00, 7.54978941586159635335e-08, 5.39030252995776476554e-15,
    3.28200341580791294123e-22};

std::size_t Slot(int index) {
    return (std::size_t)index;
}

float Undefined(float x) {
    return (x == x) ? std::numeric_limits<float>::quiet_NaN() : x;
}

std::int32_t Word(float value) {
    return std::bit_cast<std::int32_t>(value);
}

float KernelSin(double x) {
    const double z = x * x;
    const double w = z * z;
    const double r = kS3 + (z * kS4);
    const double s = z * x;
    return (float)((x + (s * (kS1 + (z * kS2)))) + (s * w * r));
}

float KernelCos(double x) {
    const double z = x * x;
    const double w = z * z;
    const double r = kC2 + (z * kC3);
    return (float)(((1.0 + (z * kC0)) + (w * kC1)) + ((w * z) * r));
}

struct Reduction {
    std::array<std::int32_t, kWorkingSize> digits{};
    std::array<double, kWorkingSize> table{};
    std::array<double, kWorkingSize> product{};
    std::array<double, kWorkingSize> parts{};
    int last = kReductionTerms;
    int scale = 0;
    int quadrant = 0;
    int complement = 0;
    double remainder = 0.0;
};

void Distill(Reduction& work) {
    double z = work.product[Slot(work.last)];
    for (int i = 0, j = work.last; j > 0; i++, j--) {
        const auto fw = (double)(std::int32_t)(kTwoPow24Inverse * z);
        work.digits[Slot(i)] = (std::int32_t)(z - (kTwo24 * fw));
        z = work.product[Slot(j - 1)] + fw;
    }
    work.remainder = z;
}

void SplitInteger(Reduction& work) {
    double z = std::scalbn(work.remainder, work.scale);
    z -= 8.0 * std::floor(z * 0.125);
    work.quadrant = (std::int32_t)z;
    z -= (double)work.quadrant;
    work.complement = 0;
    const std::size_t top = Slot(work.last - 1);
    if (work.scale > 0) {
        const std::int32_t carry = work.digits[top] >> (24 - work.scale);
        work.quadrant += carry;
        work.digits[top] -= carry << (24 - work.scale);
        work.complement = work.digits[top] >> (23 - work.scale);
    } else if (work.scale == 0) {
        work.complement = work.digits[top] >> 23;
    } else if (z >= 0.5) {
        work.complement = 2;
    }
    work.remainder = z;
}

void Complement(Reduction& work) {
    work.quadrant += 1;
    std::int32_t carry = 0;
    for (int i = 0; i < work.last; i++) {
        const std::int32_t digit = work.digits[Slot(i)];
        if (carry == 0) {
            if (digit != 0) {
                carry = 1;
                work.digits[Slot(i)] = 0x1000000 - digit;
            }
        } else {
            work.digits[Slot(i)] = 0xffffff - digit;
        }
    }
    const std::size_t top = Slot(work.last - 1);
    if (work.scale == 1) work.digits[top] &= 0x7fffff;
    if (work.scale == 2) work.digits[top] &= 0x3fffff;
    if (work.complement == 2) {
        work.remainder = 1.0 - work.remainder;
        if (carry != 0) work.remainder -= std::scalbn(1.0, work.scale);
    }
}

bool NeedsMoreTerms(const Reduction& work) {
    if (work.remainder != 0.0) return false;
    std::int32_t merged = 0;
    for (int i = work.last - 1; i >= kReductionTerms; i--)
        merged |= work.digits[Slot(i)];
    return merged == 0;
}

void Normalise(Reduction& work) {
    if (work.remainder == 0.0) {
        work.last -= 1;
        work.scale -= 24;
        while (work.digits[Slot(work.last)] == 0) {
            work.last--;
            work.scale -= 24;
        }
        return;
    }
    const double z = std::scalbn(work.remainder, -work.scale);
    if (z >= kTwo24) {
        const auto fw = (double)(std::int32_t)(kTwoPow24Inverse * z);
        work.digits[Slot(work.last)] = (std::int32_t)(z - (kTwo24 * fw));
        work.last += 1;
        work.scale += 24;
        work.digits[Slot(work.last)] = (std::int32_t)fw;
        return;
    }
    work.digits[Slot(work.last)] = (std::int32_t)z;
}

double Recombine(Reduction& work) {
    double fw = std::scalbn(1.0, work.scale);
    for (int i = work.last; i >= 0; i--) {
        work.product[Slot(i)] = fw * (double)work.digits[Slot(i)];
        fw *= kTwoPow24Inverse;
    }
    for (int i = work.last; i >= 0; i--) {
        double sum = 0.0;
        for (int k = 0; k < kHalfPiTerms && k <= work.last - i; k++)
            sum += kHalfPiParts[Slot(k)] * work.product[Slot(i) + Slot(k)];
        work.parts[Slot(work.last) - Slot(i)] = sum;
    }
    double total = 0.0;
    for (int i = work.last; i >= 0; i--)
        total += work.parts[Slot(i)];
    return (work.complement == 0) ? total : -total;
}

int ReduceLarge(double x, int exponent, double& y) {
    Reduction work;
    const int base = std::max(0, (exponent - 3) / 24);
    work.scale = exponent - (24 * (base + 1));
    for (int i = 0; i <= kReductionTerms; i++)
        work.table[Slot(i)] = (double)kTwoOverPi[Slot(base) + Slot(i)];
    for (int i = 0; i <= kReductionTerms; i++)
        work.product[Slot(i)] = x * work.table[Slot(i)];

    for (;;) {
        Distill(work);
        SplitInteger(work);
        if (work.complement > 0) Complement(work);
        if (!NeedsMoreTerms(work)) break;
        int extra = 1;
        while (work.digits[Slot(kReductionTerms - extra)] == 0)
            extra++;
        for (int i = work.last + 1; i <= work.last + extra; i++) {
            work.table[Slot(i)] = (double)kTwoOverPi[Slot(base) + Slot(i)];
            work.product[Slot(i)] = x * work.table[Slot(i)];
        }
        work.last += extra;
    }

    Normalise(work);
    y = Recombine(work);
    return work.quadrant & 7;
}

int Reduce(float x, double& y) {
    const std::int32_t hx = Word(x);
    const std::int32_t ix = hx & kSignMask;
    if (ix < kMediumLimit) {
        const double fn = (((double)x * kInvHalfPi) + kRoundMagic) - kRoundMagic;
        const double r = (double)x - (fn * kHalfPiHead);
        const double w = fn * kHalfPiTail;
        y = r - w;
        return (int)fn;
    }
    if (ix >= kInfinity) {
        y = (double)Undefined(x);
        return 0;
    }
    const std::int32_t exponent = (ix >> 23) - 150;
    const auto scaled = std::bit_cast<float>(ix - (std::int32_t)((std::uint32_t)exponent << 23U));
    double reduced = 0.0;
    const int quadrant = ReduceLarge((double)scaled, exponent, reduced);
    if (hx < 0) {
        y = -reduced;
        return -quadrant;
    }
    y = reduced;
    return quadrant;
}

}

float Sinf(float x) {
    const std::int32_t hx = Word(x);
    const std::int32_t ix = hx & kSignMask;
    if (ix <= kQuarterPi) {
        if (ix < kTinyArgument && (int)x == 0) return x;
        return KernelSin((double)x);
    }
    if (ix <= kFiveQuarterPi) {
        if (ix > kThreeQuarterPi) return KernelSin(((hx > 0) ? kHalfPi2 : -kHalfPi2) - (double)x);
        if (hx > 0) return KernelCos((double)x - kHalfPi);
        return -KernelCos((double)x + kHalfPi);
    }
    if (ix <= kNineQuarterPi) {
        if (ix > kSevenQuarterPi) return KernelSin((double)x + ((hx > 0) ? -kHalfPi4 : kHalfPi4));
        if (hx > 0) return -KernelCos((double)x - kHalfPi3);
        return KernelCos((double)x + kHalfPi3);
    }
    if (ix >= kInfinity) return Undefined(x);
    double y = 0.0;
    switch (Reduce(x, y) & 3) {
    case 0:
        return KernelSin(y);
    case 1:
        return KernelCos(y);
    case 2:
        return KernelSin(-y);
    default:
        return -KernelCos(y);
    }
}

float Cosf(float x) {
    const std::int32_t hx = Word(x);
    const std::int32_t ix = hx & kSignMask;
    if (ix <= kQuarterPi) {
        if (ix < kTinyArgument && (int)x == 0) return 1.0F;
        return KernelCos((double)x);
    }
    if (ix <= kFiveQuarterPi) {
        if (ix > kThreeQuarterPi) return -KernelCos((double)x + ((hx > 0) ? -kHalfPi2 : kHalfPi2));
        if (hx > 0) return KernelSin(kHalfPi - (double)x);
        return KernelSin((double)x + kHalfPi);
    }
    if (ix <= kNineQuarterPi) {
        if (ix > kSevenQuarterPi) return KernelCos((double)x + ((hx > 0) ? -kHalfPi4 : kHalfPi4));
        if (hx > 0) return KernelSin((double)x - kHalfPi3);
        return KernelSin(-kHalfPi3 - (double)x);
    }
    if (ix >= kInfinity) return Undefined(x);
    double y = 0.0;
    switch (Reduce(x, y) & 3) {
    case 0:
        return KernelCos(y);
    case 1:
        return KernelSin(-y);
    case 2:
        return -KernelCos(y);
    default:
        return KernelSin(y);
    }
}

}
