#pragma once

#include <menagerie/beavers>
// CROW_PARAMS below is the main macro; everything above it is implementation
// plumbing for stringifying a variadic parameter list as "name=value, name2=value2".


#define CROW_ENTER_FUNCTION() LOG_INF() << "Entering function " << __func__

// Addons
#define COUNT_ARGS(...) COUNT_ARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

// Individual parameter formatters: FMT_N stringifies N comma-separated arguments as
// "p1=<value>, p2=<value>, ..." (stream-insertion form, for use inside `<<` chains).
#define FMT_1(p1) #p1 "=" << p1
#define FMT_2(p1, p2) FMT_1(p1) << ", " << #p2 "=" << p2
#define FMT_3(p1, p2, p3) FMT_2(p1, p2) << ", " << #p3 "=" << p3
#define FMT_4(p1, p2, p3, p4) FMT_3(p1, p2, p3) << ", " << #p4 "=" << p4
#define FMT_5(p1, p2, p3, p4, p5) FMT_4(p1, p2, p3, p4) << ", " << #p5 "=" << p5
#define FMT_6(p1, p2, p3, p4, p5, p6) FMT_5(p1, p2, p3, p4, p5) << ", " << #p6 "=" << p6
#define FMT_7(p1, p2, p3, p4, p5, p6, p7) FMT_6(p1, p2, p3, p4, p5, p6) << ", " << #p7 "=" << p7
#define FMT_8(p1, p2, p3, p4, p5, p6, p7, p8) FMT_7(p1, p2, p3, p4, p5, p6, p7) << ", " << #p8 "=" << p8

// Dispatcher macro: routes to the FMT_N matching the actual argument count.
#define DISPATCH_FMT(N) FMT_##N
#define CALL_FMT(N, ...) DISPATCH_FMT(N)(__VA_ARGS__)

/// Renders up to 8 named arguments as a " p1=<value>, p2=<value>, ..." string, e.g.
/// `CROW_PARAMS(user_id, retries)` with user_id=7, retries=2 yields " user_id=7, retries=2".
/// Intended for splicing into a LOG_* stream call to trace a function's parameters.
#define CROW_PARAMS(...)                                                                                               \
    ([&]() {                                                                                                           \
        std::ostringstream oss;                                                                                        \
        oss << " " << CALL_FMT(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__);                                                  \
        return oss.str();                                                                                              \
    })()
