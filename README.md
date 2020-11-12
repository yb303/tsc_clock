# Tsc Clock

Low latency clock implementation based on the cpu's time stamp counter (TSC), using the `rdtsc` instruction on x86.

In cases where latency is paramount, you can get a clock reading, in nanoseconds, much faster than other alternatives.

**Tsc Clock** returns the nanos from epoch in about 25 cycles, while `clock_gettime` takes closer to 200 cycles.
This latency is built up from one TSC read (20 to 25 cycles), one addition and one multiplication, assuming variables are cached.

### Implementation Details
Although discouraged in many places, using `rdtsc` reliably and accurately is possible if these details are taken care of:

#### Hardware support
The TSC, on x86 server systems, starting from Nehalem, runs at constant rate, and never stops, across all P states. I'm not 100% sure about C states.
You have to verify that `/proc/cpuinfo` contains `nonstop_tsc`, `constant_tsc`, and better also have `tsc_reliable`.
Note that the TSC runs at the cpu's base frequency. If, for example, your cpu's base frequency is 2800MHz, but it switched to 1800MHZ, or to 3400MHz, for whatever reason, the TSC is still incrementing at 2800MHz.

#### OS support
The OS may restrict reading the TSC to kernel mode (ring 0) but this was never the case for Linux (nor for Windows).
**Tsc Clock** relies on `clock_gettime` for calibration, so this function should also be relatively fast and without too much variance.
Other interfaces, like `std::chrono::system_time`, use `clock_gettime` under the covers.
On modern Linux systems `clock_gettime` is provided in vdso so the call does not go into the kernel.
If `clock_gettime` does go into the kernel, the calibration will be less accurate as the switch from user space to kernel space takes at least few hundred cycles, and with much higher variance then the vdso.
Any higher than normal latency of `clock_gettime`, due to context switching, minor preemption, or any other reason, has to be mitigated by calling it again until the latency is good enough.

#### The compiler may reorder the reading of the TSC
This has to be prevented when calibrating, even though any sane compiler would not reorder across non-inlined function boundary. This is taken care of in the `readTsc` function.

#### Calculation accuracy of returned nanoseconds
**Tsc Clock** is calculating the time, in nanos, using this relation: time = *ratio x TSC + starting\_point*
Where the *ratio* is 1.0 divided by cpu GHz. Typical cpu frequencies of 2 to 4GHz bring the [ulp](https://en.wikipedia.org/wiki/Unit_in_the_last_place) size to about *5.55e-17*, or 1/1.8e16.
1.8e16 cycles corresponds to about 69.5 days on a 3GHz cpu, or 52 days on a 4GHz cpu.
This means that a machine that was restarted 3 months ago will not get 1ns granularity, but closer to 2ns. I've also seen a machine jumping 256ns at a time!
Anyway, the solution is to use scaled integer multiplication, rather then double. That gives us 64 bits of precision instead of the 53 bits we get with double.

#### The TSC reading may drift away from the correct wall clock time
`clock_gettime` is doing its own estimation based on the TSC (as here) and re-calibrating on timer interrupts.
Similarly, **Tsc Clock** re-calibrates to `clock_gettime`.
`recalibrate()` has to be called periodically. It reads the TSC, calls `clock_gettime`, and re-calculates a few variables. This takes a few tens of nanoseconds on a modern server. This means you have to find the right time to call it even when running latency critical code.
How much can **Tsc Clock** drift relative to `clock_gettime`? We have to assume `clock_gettime` is not wrong by more than 1 nanosecond.
An error of 1ns over 10ms calibration, will accumulate to 100ns over 1 second. So if we want to limit the drift to 10ns, we have to recalibrate after 100ms.
When recalibrating after 100ms, we limit the error again to 1ns, but this time over 100ms, so to limit the drift, again, to 10ns, we now have recalibrate again in 1000ms...
Regardless of the above logic, I tend to recalibrate multiple times per second, whenever I find some idle time.

#### Clock reading latency
We do not want to calibrate on one core and measure on another as getting the clock member variables from another core means a cache miss. At least once in a while.
Handling this is outside the scope of the clock itself. I could add some mapping of clock instances per cpu but this would also cost some latency.
I run a clock instance per thread, or per cpu, to minimize cache misses. This is the same thing when running on isolated cores. And this is what you're supposed to do anyway with latency critical code.

#### Daylight saving time, leap seconds etc
DST is a feature of the local time. **Tsc Clock** is returning nanos in UTC time, like `clock_gettime`.
Leap seconds are a problematic part because we're not aware of the kernel's time keeping variables like `clock_gettime`.
On re-calibration, **Tsc Clock** is comparing the current time from `clock_gettime` to the expected TSC-based time. If the drift is small enough, we assume this is only a drift, and recalculate the ratio and starting point normally. Otherwise we assume this is not a drift, but a time jump, and only adjust the starting point to reflect it, and keeping the ratio untouched.
What is small enough? it is currently hardwired to 50ms, which is half of the smallest leap second update I've heard of. To accumulate 50ms of drift we'll need a run of over 5 days after an initial calibration of 10ms. This is big enough to make it an exteremely unlikly drift. Maybe it can be smarter in the future.
The time between re-calibrations across a leap second update may still return wrong times. This is a limit I'm happy to live with, especially as `clock_gettime` is also an estimate, and could be wrong between timer interrupts.
Bottom line - to detect leap seconds in good time, please re-calibrate often.

### Example code
```
class MyClass {
    TscClock clock_;
    ...

    MyClass() {
        clock_.init();
    }

    void hotLoop() {
        while (keepGoing_) {
            msgRecvTimeNs_ = clock_.getCurEpochNs();
            while (getMsg()) {
                processMsg();
                auto msgEndTimeNs = clock_.getCurEpochNs();
                accumulateProcessingTime(msgEndTimeNs - msgRecvTimeNs_);
                msgRecvTimeNs_ = msgEndTimeNs;
            }

            // Idle
            clock_.recalibrate();
        }
    }
};
```

### Language, build, and supported environments
I use C++17 but I don't think I've used anything here that needs more than C++98.
I built it with g++ 9.3, on cygwin, WSL, and proper Linux, all on x86_64.
To build, cd to the `tsc_clock` directory and run `cmake . && make`.
This will create `libtsc_clock.a` in the `lib` directory, and `test_tsc_clock` in the `bin` directory. Or you can just copy the 2 files into your code base.
**Tsc Clock** is only as good as the underlying `clock_gettime`. WSL and cygwin show 100ns granularity, so don't expect much there.
You can run the test progy to see some stats.
