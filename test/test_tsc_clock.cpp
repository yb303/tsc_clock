#include <tsc_clock.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

uint64_t getSystemCurEpochNs() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1'000'000'000lu + ts.tv_nsec;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TscClock clock;
    clock.init(100000);

    printf("rdtsc cycles: %lu\n", clock.getRdtscLatency());
    printf("clock_gettime cycles: %lu\n", clock.getClockGettimeLatency());
    printf("cycle/ns: %.18f\n", clock.getPreciseCyclesPerNs());

    static const int T = 10;
    static const int N = 20;
    uint64_t ns_clock[N];
    uint64_t ns_system[N];

    for (int j = 0; j < T; j++) {
        for (int i = 0; i < N; i++) {
            ns_clock[i] = clock.getCurEpochNs();
            ns_system[i] = getSystemCurEpochNs();
        }
        for (int i = 1; i < N; i++) {
            printf("%20lu (+%7lu) vs %20lu (+%7lu) diff %ld\n",
                   ns_clock[i], ns_clock[i] - ns_clock[i-1],
                   ns_system[i], ns_system[i]- ns_system[i-1],
                   ns_clock[i] - ns_system[i]);
        }
        usleep(10'000); // 10ms
        printf("recalibrating...");
        uint64_t t1 = clock.readTsc();
        clock.recalibrate();
        uint64_t t2 = clock.readTsc();
        printf(" recalibration cycles: %lu\n", t2 - t1 - clock.getRdtscLatency());
        printf("cycle/ns: %.18f\n", clock.getPreciseCyclesPerNs());
    }

    return 0;
}
