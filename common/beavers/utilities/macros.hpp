#pragma once

/// Expands to `TRUE` if given at least one variadic argument, to nothing otherwise.
#define HAS_ARGS(...) __VA_OPT__(TRUE)

/// Token-pastes A and B after macro-expanding both (unlike a bare `##`).
#define CONCAT(A, B) CONCAT_IMPL(A, B)
#define CONCAT_IMPL(A, B) A##B

/**
 * @def UNRECOVERABLE_NOEXCEPT
 * @brief `noexcept` for functions whose only possible exception is unrecoverable.
 *
 * Marks functions that allocate (or otherwise can throw ONLY conditions the
 * process cannot reasonably recover from - std::bad_alloc under heap
 * exhaustion/fragmentation). With UNRECOVERABLE_EXCEPTIONS_TERMINATE
 * defined (the default, via the CMake option of the same name) it expands to
 * `noexcept`, so such a throw becomes std::terminate at the throw site instead
 * of unwinding into code that cannot fix it. Without the definition it expands
 * to nothing and the exception propagates - for consumers who prefer to catch.
 *
 * ABI-visible, same rule as BOOST_ASIO_RECYCLING_ALLOCATOR_CACHE_SIZE in the
 * root CMakeLists: noexcept is part of the function type since C++17, so two
 * TUs disagreeing on the definition ODR-violate. Set it once, tree-wide.
 */
#ifdef UNRECOVERABLE_EXCEPTIONS_TERMINATE
    #define UNRECOVERABLE_NOEXCEPT noexcept
#else
    #define UNRECOVERABLE_NOEXCEPT
#endif
