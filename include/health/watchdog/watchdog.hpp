// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#pragma once

#include <health/types.hpp>
#include <array>
#include <cstddef>
#include <cstdio>
#include <time.h>

namespace health::watchdog {

inline Timestamp now_ns() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<Timestamp>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

/// Watchdog entry for a single monitored component.
struct WatchdogEntry {
    std::uint8_t id;
    Timestamp    timeout_ns;      ///< Maximum time between kicks.
    Timestamp    last_kick{0};    ///< Timestamp of last kick.
    Status       status{Status::Offline};
    std::uint64_t kick_count{0};
    std::uint64_t timeout_count{0};
};

/// Multi-channel watchdog timer.
///
/// Monitors multiple components via periodic "kicks."
/// If a component doesn't kick within its timeout, it's flagged as faulty.
///
/// @tparam MaxChannels  Maximum monitored components.
template <std::size_t MaxChannels>
class WatchdogTimer {
public:
    /// Register a channel with a timeout.
    bool register_channel(std::uint8_t id, Timestamp timeout_ns) {
        if (count_ >= MaxChannels) return false;
        entries_[count_] = {id, timeout_ns, 0, Status::Offline, 0, 0};
        ++count_;
        return true;
    }

    /// Kick (heartbeat) from a channel.
    bool kick(std::uint8_t id, Timestamp now) {
        auto* entry = find(id);
        if (!entry) return false;
        entry->last_kick = now;
        entry->status    = Status::Healthy;
        ++entry->kick_count;
        return true;
    }

    /// Evaluate all channels against their timeouts.
    /// @param now  Current timestamp.
    /// @return     Number of timed-out channels.
    std::size_t evaluate(Timestamp now) {
        std::size_t timed_out = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            auto& e = entries_[i];
            if (e.status == Status::Offline) continue;

            if (e.last_kick > 0 && (now - e.last_kick) > e.timeout_ns) {
                if (e.status != Status::Faulty) {
                    e.status = Status::Faulty;
                    ++e.timeout_count;
                    ++timed_out;
                }
            }
        }
        return timed_out;
    }

    /// Get status of a channel.
    [[nodiscard]] Status channel_status(std::uint8_t id) const {
        auto const* entry = find(id);
        return entry ? entry->status : Status::Offline;
    }

    /// Count channels in a given status.
    [[nodiscard]] std::size_t count_status(Status s) const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            if (entries_[i].status == s) ++n;
        }
        return n;
    }

    [[nodiscard]] std::size_t channel_count() const { return count_; }

    /// Print status of all channels.
    void print_status() const {
        for (std::size_t i = 0; i < count_; ++i) {
            auto const& e = entries_[i];
            std::printf("  ch %u: %.*s (kicks=%lu, timeouts=%lu)\n", e.id,
                        static_cast<int>(status_name(e.status).size()),
                        status_name(e.status).data(),
                        static_cast<unsigned long>(e.kick_count),
                        static_cast<unsigned long>(e.timeout_count));
        }
    }

private:
    WatchdogEntry* find(std::uint8_t id) {
        for (std::size_t i = 0; i < count_; ++i) {
            if (entries_[i].id == id) return &entries_[i];
        }
        return nullptr;
    }

    WatchdogEntry const* find(std::uint8_t id) const {
        for (std::size_t i = 0; i < count_; ++i) {
            if (entries_[i].id == id) return &entries_[i];
        }
        return nullptr;
    }

    std::array<WatchdogEntry, MaxChannels> entries_{};
    std::size_t count_{0};
};

}  // namespace health::watchdog
