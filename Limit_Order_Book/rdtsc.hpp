#pragma once
#include <cstdint>
#include <cstdio>
#include <time.h>

inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile (
        "rdtscp\n\t"
        "mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        : "=r"(lo), "=r"(hi)
        :
        : "%eax", "%ecx", "%edx"
    );
    return ((uint64_t)hi << 32) | lo;
}

inline double detect_ghz() {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    uint64_t c0 = rdtsc();
    struct timespec req = {0, 10000000};
    nanosleep(&req, nullptr);
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    uint64_t c1 = rdtsc();
    double elapsed_ns = (t1.tv_sec  - t0.tv_sec)  * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    double cycles = (double)(c1 - c0);
    return cycles / elapsed_ns;
}

