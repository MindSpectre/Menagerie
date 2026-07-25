#pragma once

#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

namespace menagerie::multithread {
    /// Cedes a pipeline slot without yielding to the scheduler: `_mm_pause()`
    /// on x86, the `yield` instruction on AArch64. Use in tight spin loops
    /// that expect to succeed soon and do not want a full context switch.
    inline void pause_arc_agnostic() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
    #error unsupported architecture
#endif
    }
}  // namespace menagerie::multithread
