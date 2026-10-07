#pragma once

#include <string>
#include <vector>

struct _EXCEPTION_POINTERS;

namespace Support {

inline constexpr unsigned kMaxStackFrames = 24;

unsigned CaptureStackAddresses(_EXCEPTION_POINTERS* from, unsigned long long* out,
                               unsigned max_frames);

unsigned ScanStackForReturns(_EXCEPTION_POINTERS* from, unsigned long long* out,
                             unsigned max_frames);

[[nodiscard]] std::vector<std::string> DescribeAddresses(const unsigned long long* addresses,
                                                         unsigned count);

[[nodiscard]] std::vector<std::string> CaptureStackTrace(_EXCEPTION_POINTERS* from,
                                                         unsigned max_frames = kMaxStackFrames);

void LogStackTrace(const char* tag, const std::vector<std::string>& frames);

}
