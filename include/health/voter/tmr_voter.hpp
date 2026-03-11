// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <health/types.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace health::voter {

/// Result of a TMR vote.
struct VoteResult {
    double  value;          ///< Voted output value.
    bool    valid;          ///< True if a majority agreed.
    int     agreement;      ///< Number of channels that agree (2 or 3 for TMR).
    int     faulty_channel; ///< Index of dissenting channel (-1 if all agree).
};

/// Triple Modular Redundancy (TMR) voter.
///
/// Compares three independent channel values and outputs the
/// majority value. Detects single-channel faults.
///
/// Voting strategy:
///   - If all 3 agree within tolerance → output average, all healthy
///   - If 2 agree, 1 disagrees → output average of agreeing pair, flag faulty
///   - If none agree → output median, flag degraded
class TMRVoter {
public:
    /// @param tolerance  Maximum difference for two values to be considered "agreeing."
    explicit TMRVoter(double tolerance = 1.0) : tolerance_(tolerance) {}

    /// Vote on three channel values.
    [[nodiscard]] VoteResult vote(double a, double b, double c) const {
        ++vote_count_;

        bool ab = std::abs(a - b) <= tolerance_;
        bool bc = std::abs(b - c) <= tolerance_;
        bool ac = std::abs(a - c) <= tolerance_;

        // All three agree
        if (ab && bc) {
            return {(a + b + c) / 3.0, true, 3, -1};
        }

        // Two agree, one disagrees
        if (ab) {
            ++disagreement_count_;
            return {(a + b) / 2.0, true, 2, 2};
        }
        if (bc) {
            ++disagreement_count_;
            return {(b + c) / 2.0, true, 2, 0};
        }
        if (ac) {
            ++disagreement_count_;
            return {(a + c) / 2.0, true, 2, 1};
        }

        // No agreement — output median
        ++disagreement_count_;
        double median = a;
        if ((a >= b && a <= c) || (a <= b && a >= c)) median = a;
        else if ((b >= a && b <= c) || (b <= a && b >= c)) median = b;
        else median = c;

        return {median, false, 0, -1};
    }

    [[nodiscard]] std::uint64_t vote_count() const { return vote_count_; }
    [[nodiscard]] std::uint64_t disagreement_count() const { return disagreement_count_; }

    [[nodiscard]] double disagreement_rate() const {
        return vote_count_ > 0
                   ? static_cast<double>(disagreement_count_) / static_cast<double>(vote_count_)
                   : 0.0;
    }

private:
    double tolerance_;
    mutable std::uint64_t vote_count_{0};
    mutable std::uint64_t disagreement_count_{0};
};

/// Duplex (dual-channel) cross-comparator.
struct DuplexResult {
    double value;
    bool   agreement;
};

class DuplexVoter {
public:
    explicit DuplexVoter(double tolerance = 1.0) : tolerance_(tolerance) {}

    [[nodiscard]] DuplexResult compare(double a, double b) const {
        bool agree = std::abs(a - b) <= tolerance_;
        return {(a + b) / 2.0, agree};
    }

private:
    double tolerance_;
};

}  // namespace health::voter
