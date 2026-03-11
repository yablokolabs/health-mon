// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <health/types.hpp>
#include <health/voter/tmr_voter.hpp>
#include <health/watchdog/watchdog.hpp>
#include <cstddef>
#include <cstdio>

namespace health::manager {

/// Degraded mode policy.
enum class DegradedPolicy : std::uint8_t {
    Continue,    ///< Continue with reduced redundancy.
    Failsafe,    ///< Switch to safe defaults.
    Shutdown,    ///< Orderly shutdown.
};

/// Combined health manager — voter + watchdog + degraded mode.
///
/// Provides a single interface for monitoring system health:
/// - TMR voting on redundant sensor inputs
/// - Watchdog monitoring of partition heartbeats
/// - Degraded mode management based on fault count
///
/// @tparam MaxChannels  Maximum watchdog channels.
template <std::size_t MaxChannels = 16>
class HealthManager {
public:
    struct Config {
        double tolerance;             ///< TMR vote tolerance.
        std::size_t max_faults;       ///< Max faults before degraded mode.
        DegradedPolicy policy;        ///< What to do in degraded mode.
    };

    explicit HealthManager(Config cfg)
        : voter_(cfg.tolerance), config_(cfg) {}

    /// Access the TMR voter.
    [[nodiscard]] voter::TMRVoter& voter() { return voter_; }
    [[nodiscard]] voter::TMRVoter const& voter() const { return voter_; }

    /// Access the watchdog timer.
    [[nodiscard]] watchdog::WatchdogTimer<MaxChannels>& watchdog() { return watchdog_; }

    /// Evaluate overall system health.
    /// @param now  Current timestamp.
    /// @return     Current system status.
    Status evaluate(Timestamp now) {
        std::size_t timeouts = watchdog_.evaluate(now);
        total_faults_ += timeouts;

        std::size_t faulty   = watchdog_.count_status(Status::Faulty);
        std::size_t healthy  = watchdog_.count_status(Status::Healthy);

        if (faulty == 0 && voter_.disagreement_rate() < 0.05) {
            system_status_ = Status::Healthy;
        } else if (total_faults_ >= config_.max_faults) {
            system_status_ = Status::Faulty;
        } else if (faulty > 0 || voter_.disagreement_rate() >= 0.05) {
            system_status_ = Status::Degraded;
        }

        (void)healthy;  // Used for future health scoring
        return system_status_;
    }

    [[nodiscard]] Status system_status() const { return system_status_; }
    [[nodiscard]] DegradedPolicy policy() const { return config_.policy; }
    [[nodiscard]] std::uint64_t total_faults() const { return total_faults_; }

    /// Should the system shut down?
    [[nodiscard]] bool should_shutdown() const {
        return system_status_ == Status::Faulty && config_.policy == DegradedPolicy::Shutdown;
    }

    void print_status() const {
        std::printf("System: %.*s (total faults: %lu)\n",
                    static_cast<int>(status_name(system_status_).size()),
                    status_name(system_status_).data(),
                    static_cast<unsigned long>(total_faults_));
        watchdog_.print_status();
    }

private:
    voter::TMRVoter                       voter_;
    watchdog::WatchdogTimer<MaxChannels>  watchdog_;
    Config                                config_;
    Status                                system_status_{Status::Offline};
    std::uint64_t                         total_faults_{0};
};

}  // namespace health::manager
