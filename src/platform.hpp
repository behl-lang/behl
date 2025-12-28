#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#    define BEHL_PLATFORM_WINDOWS 1
#    define BEHL_PLATFORM_POSIX 0
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#    define BEHL_PLATFORM_POSIX 1
#    define BEHL_PLATFORM_WINDOWS 0
#else
#    error "Unsupported platform"
#endif

// MSVC doesn't report __cplusplus correctly unless /Zc:__cplusplus is set
// Use _MSVC_LANG instead for MSVC, which is always set correctly
#if defined(_MSVC_LANG)
#    define BEHL_CPLUSPLUS _MSVC_LANG
#else
#    define BEHL_CPLUSPLUS __cplusplus
#endif

#ifdef _MSC_VER
#    define BEHL_FORCEINLINE __forceinline
#else
#    define BEHL_FORCEINLINE inline __attribute__((always_inline))
#endif

#if BEHL_CPLUSPLUS >= 202302L
#    define BEHL_UNREACHABLE() std::unreachable()
#else
#    ifdef _MSC_VER
#        define BEHL_UNREACHABLE() __assume(0)
#    else
#        define BEHL_UNREACHABLE() __builtin_unreachable()
#    endif
#endif
