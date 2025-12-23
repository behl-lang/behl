#pragma once

// Export/Import macros for shared library builds
#ifdef BEHL_SHARED_LIBRARY
    #ifdef _WIN32
        #ifdef BEHL_BUILDING_LIBRARY
            #define BEHL_API __declspec(dllexport)
        #else
            #define BEHL_API __declspec(dllimport)
        #endif
    #else
        // GCC/Clang visibility attribute
        #ifdef BEHL_BUILDING_LIBRARY
            #define BEHL_API __attribute__((visibility("default")))
        #else
            #define BEHL_API
        #endif
    #endif
#else
    // Static library build - no export/import needed
    #define BEHL_API
#endif
