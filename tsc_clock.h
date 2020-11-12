#include <stdint.h>
#include <sys/time.h>

typedef unsigned __int128 uint128_t;

class TscClock {
public:
    //
    // Maintenance functions
    //

    // Call before calibration if cpu MHz is known
    void setMHz(double mhz);

    // Initial clock calibration. Call once
    // delay_us - microsecond delay for calibration. Longer is more accurate.
    void init(uint64_t delay_us = 1000);
    
    // Periodic re-calibration. Call when you have a chance
    void recalibrate();

    //
    // Get time functions. Convert rdtsc result to nanos.
    //

    // Read raw cycles
    static uint64_t readTsc();

    // Cycles diff to ns diff
    uint64_t cyclesToNs(uint64_t cycles) const;

    // Cycles to epoch ns
    uint64_t cyclesToEpochNs(uint64_t cycles) const;

    // Cycles to timespec
    timespec cyclesToTimeSpec(uint64_t cycles) const;

    // Get current epoch ns
    uint64_t getCurEpochNs() const;

    // Get current time as timespec
    timespec getCurTimeSpec() const;

    //
    // Info
    //

    uint64_t getClockGettimeLatency() const { return clock_gettime_latency_; }

    uint64_t getRdtscLatency() const { return rdtsc_latency_; }

    double getCyclesPerNs() const { return 1e6 / cyclesToNs(1e6); }

    double getPreciseCyclesPerNs() const { return 1e18 / cyclesToNs(1e18); }

private:
    struct SyncPoint
    {
        uint64_t ns_ = 0;
        uint64_t cycles_ = 0;
    };

    uint64_t scaled_ns_per_cycle_ = 0;
    uint64_t start_ns_ = 0;
    SyncPoint p_start_;

    double mhz_ = 0.0;

    uint64_t rdtsc_latency_ = 0;
    uint64_t clock_gettime_latency_ = 0;

    void measureCallLatencies();

    // Get matching tsc and nanos
    SyncPoint getSyncTimePoint();
};

//
// Implement inline functions
//

inline uint64_t TscClock::readTsc() {
    union {
        struct { uint32_t lo, hi; };
        int64_t ts;
    } u;
    asm volatile("rdtsc" : "=a"(u.lo), "=d"(u.hi) : : "memory");
    return u.ts;
}

// Cycles diff to ns diff
inline uint64_t TscClock::cyclesToNs(uint64_t cycles) const {
    // Scaled multiplication by ns to cycles ratio
    return (uint128_t(cycles) * uint128_t(scaled_ns_per_cycle_)) >> 64;
}

// Cycles to epoch ns
inline uint64_t TscClock::cyclesToEpochNs(uint64_t cycles) const {
    return start_ns_ + cyclesToNs(cycles);
}

// Cycles to timespec
inline timespec TscClock::cyclesToTimeSpec(uint64_t cycles) const {
    uint64_t ns = cyclesToEpochNs(cycles);
    timespec ts;
    ts.tv_sec = ns / 1'000'000'000;
    ts.tv_nsec = ns % 1'000'000'000;
    return ts;
}

// Get current epoch ns
inline uint64_t TscClock::getCurEpochNs() const {
    return cyclesToEpochNs(readTsc());
}

// Get current time as timespec
inline timespec TscClock::getCurTimeSpec() const {
    return cyclesToTimeSpec(readTsc());
}

