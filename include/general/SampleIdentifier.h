#pragma once

#include <cstddef>
#include <utility>

// Hash for SampleIdentifier (pair<int,int>) tailored to expected 0-256 range
using SampleIdentifier = std::pair<int, int>;

namespace std {
template <>
struct hash<SampleIdentifier> {
    size_t operator()(const SampleIdentifier& s) const noexcept {
        return (static_cast<size_t>(s.first & 0x1ff) << 9) ^ static_cast<size_t>(s.second & 0x1ff);
    }
};
} // namespace std
