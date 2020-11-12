#include "tsc_clock.h"

#include <limits>
#include <algorithm>

#include <unistd.h>
#include <time.h>

void TscClock::setMHz(double mhz) {
    mhz_ = mhz;
}

void TscClock::measureCallLatencies() {
    // Measure rdtsc
    rdtsc_latency_ = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < 100; i++) {
        // Make 2 reading per iteration, to not measure the loop's cmp+jump
        uint64_t t1 = readTsc();
        uint64_t t2 = readTsc();
        rdtsc_latency_ = std::min(rdtsc_latency_, t2 - t1);
    }
    // Measure clock_gettime
    clock_gettime_latency_ = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < 100; i++) {
        uint64_t t1 = readTsc();
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t t2 = readTsc();
        clock_gettime_latency_ = std::min(clock_gettime_latency_, t2 - t1);
    }
    // Remove rdtsc overhead
    clock_gettime_latency_ -= rdtsc_latency_;
}

// Initial clock calibration
void TscClock::init(uint64_t delay_us) {
    measureCallLatencies();

    // Sync ns and cycles
    p_start_ = getSyncTimePoint();

    if (mhz_) {
        // If the cpu MHz is known, one point is enough to calibrate
        scaled_ns_per_cycle_ = (uint128_t(1'000'000) << 64) / uint128_t(mhz_ * 1000);
    }
    else {
        // If the cpu MHz is unknown, measure it using a second sync point
        usleep(delay_us);
        SyncPoint p_end = getSyncTimePoint();

        uint64_t cycles = p_end.cycles_ - p_start_.cycles_;
        uint64_t ns = p_end.ns_ - p_start_.ns_;
        scaled_ns_per_cycle_ = (uint128_t(ns) << 64) / uint128_t(cycles);
    }
    start_ns_ = p_start_.ns_ - cyclesToNs(p_start_.cycles_);
}

// Periodic re-calibration
void TscClock::recalibrate() {
    SyncPoint p_end = getSyncTimePoint();

    uint64_t expected_ns = cyclesToEpochNs(p_end.cycles_);
    uint64_t ns_diff = std::abs(int64_t(expected_ns - p_end.ns_));
    if (!mhz_ && ns_diff < 50'000'000) {
        // If system time and tsc time diverged more than 50ms, we assume there
        // was a PTP time adjustment.
        // Recalculate ns_per_cycle only if MHz is unknown, and we see no
        // system time adjustment.
        // Otherwise, we recalculate only the starting point.
        uint64_t cycles = p_end.cycles_ - p_start_.cycles_;
        uint64_t ns = p_end.ns_ - p_start_.ns_;
        scaled_ns_per_cycle_ = (uint128_t(ns) << 64) / uint128_t(cycles);
    }
    start_ns_ = p_end.ns_ - cyclesToNs(p_end.cycles_);
}

// Get matching tsc and nanos
TscClock::SyncPoint TscClock::getSyncTimePoint() {
    // We're assuming clock_gettime calls rdtsc right at the start
    // But make sure the clock_gettime call is short enough.
    // Getting preempted between the rdtsc calls is not great.
    SyncPoint ret;
    uint64_t good_latency = clock_gettime_latency_ * 1.5;
    uint64_t min_cycles = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < 10; i++) {
        uint64_t t1 = readTsc();
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t t2 = readTsc();
        uint64_t dt = t2 - t1;
        if (dt >= min_cycles)
            continue;
        min_cycles = dt;
        ret.ns_ = ts.tv_sec * 1'000'000'000 + ts.tv_nsec;
        ret.cycles_ = t1 + rdtsc_latency_; // Adjust to same point
        if (dt <= good_latency)
            break;
    }
    return ret;
}

