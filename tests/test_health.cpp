// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Yabloko Labs

#include <health/voter/tmr_voter.hpp>
#include <health/watchdog/watchdog.hpp>
#include <health/manager/health_manager.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__);                \
            std::abort();                                                                 \
        }                                                                                 \
    } while (0)

// --- Test 1: TMR all agree ---
static void test_tmr_all_agree() {
    health::voter::TMRVoter voter(1.0);

    auto result = voter.vote(100.0, 100.5, 99.8);
    CHECK(result.valid);
    CHECK(result.agreement == 3);
    CHECK(result.faulty_channel == -1);
    CHECK(std::abs(result.value - 100.1) < 0.1);

    std::printf("  PASS: TMR all agree (value=%.2f)\n", result.value);
}

// --- Test 2: TMR one faulty ---
static void test_tmr_one_faulty() {
    health::voter::TMRVoter voter(1.0);

    // Channel 2 is way off
    auto result = voter.vote(100.0, 100.3, 999.0);
    CHECK(result.valid);
    CHECK(result.agreement == 2);
    CHECK(result.faulty_channel == 2);
    CHECK(std::abs(result.value - 100.15) < 0.1);

    std::printf("  PASS: TMR one faulty (ch %d flagged)\n", result.faulty_channel);
}

// --- Test 3: TMR no agreement ---
static void test_tmr_no_agreement() {
    health::voter::TMRVoter voter(1.0);

    auto result = voter.vote(10.0, 50.0, 90.0);
    CHECK(!result.valid);
    CHECK(result.agreement == 0);
    // Median should be 50
    CHECK(std::abs(result.value - 50.0) < 0.1);

    std::printf("  PASS: TMR no agreement (median=%.1f)\n", result.value);
}

// --- Test 4: Watchdog timeout detection ---
static void test_watchdog() {
    health::watchdog::WatchdogTimer<4> wd;

    wd.register_channel(1, 100'000'000);  // 100ms timeout
    wd.register_channel(2, 100'000'000);

    health::Timestamp t = 1'000'000'000;  // 1s
    wd.kick(1, t);
    wd.kick(2, t);

    // At t+50ms, both healthy
    CHECK(wd.evaluate(t + 50'000'000) == 0);
    CHECK(wd.channel_status(1) == health::Status::Healthy);

    // At t+200ms, both timed out
    CHECK(wd.evaluate(t + 200'000'000) == 2);
    CHECK(wd.channel_status(1) == health::Status::Faulty);
    CHECK(wd.channel_status(2) == health::Status::Faulty);

    // Kick channel 1, it recovers
    wd.kick(1, t + 200'000'000);
    CHECK(wd.channel_status(1) == health::Status::Healthy);

    std::printf("  PASS: watchdog timeout detection\n");
}

// --- Test 5: Duplex comparator ---
static void test_duplex() {
    health::voter::DuplexVoter voter(0.5);

    auto r1 = voter.compare(100.0, 100.3);
    CHECK(r1.agreement);
    CHECK(std::abs(r1.value - 100.15) < 0.1);

    auto r2 = voter.compare(100.0, 200.0);
    CHECK(!r2.agreement);

    std::printf("  PASS: duplex cross-compare\n");
}

// --- Test 6: Health manager integration ---
static void test_health_manager() {
    using namespace health::manager;

    HealthManager mgr({1.0, 3, DegradedPolicy::Shutdown});

    mgr.watchdog().register_channel(1, 100'000'000);
    mgr.watchdog().register_channel(2, 100'000'000);
    mgr.watchdog().register_channel(3, 100'000'000);

    health::Timestamp t = 1'000'000'000;
    mgr.watchdog().kick(1, t);
    mgr.watchdog().kick(2, t);
    mgr.watchdog().kick(3, t);

    // All healthy
    CHECK(mgr.evaluate(t + 50'000'000) == health::Status::Healthy);

    // One channel times out
    mgr.watchdog().kick(1, t + 150'000'000);
    mgr.watchdog().kick(2, t + 150'000'000);
    // Channel 3 doesn't kick
    CHECK(mgr.evaluate(t + 250'000'000) == health::Status::Degraded);

    // Accumulate enough faults → Faulty
    mgr.evaluate(t + 400'000'000);
    mgr.evaluate(t + 600'000'000);
    CHECK(mgr.total_faults() >= 3);
    CHECK(mgr.system_status() == health::Status::Faulty);
    CHECK(mgr.should_shutdown());

    std::printf("  PASS: health manager integration\n");
}

int main() {
    std::printf("health-mon tests\n");
    std::printf("=================\n");

    test_tmr_all_agree();
    test_tmr_one_faulty();
    test_tmr_no_agreement();
    test_watchdog();
    test_duplex();
    test_health_manager();

    std::printf("\nAll 6 tests passed.\n");
    return 0;
}
