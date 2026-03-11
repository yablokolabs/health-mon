// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <cstdint>
#include <string_view>

namespace health {

/// Component health status.
enum class Status : std::uint8_t {
    Healthy,    ///< Operating normally.
    Degraded,   ///< Functional but not nominal.
    Faulty,     ///< Not producing valid output.
    Offline,    ///< Not responding / not started.
};

/// Redundancy mode.
enum class RedundancyMode : std::uint8_t {
    Simplex,    ///< Single channel, no redundancy.
    Duplex,     ///< Dual channel, cross-compare.
    TMR,        ///< Triple modular redundancy, majority vote.
};

using Timestamp = std::uint64_t;

[[nodiscard]] constexpr auto status_name(Status s) -> std::string_view {
    switch (s) {
        case Status::Healthy:  return "healthy";
        case Status::Degraded: return "degraded";
        case Status::Faulty:   return "faulty";
        case Status::Offline:  return "offline";
    }
    return "unknown";
}

}  // namespace health
