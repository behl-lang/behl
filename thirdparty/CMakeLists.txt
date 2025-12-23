include(FetchContent)

# GoogleTest
if(BEHL_BUILD_TESTS)
    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        message(STATUS "GoogleTest not found on system, fetching from source")
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        )

        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)  # Disable gmock
        FetchContent_MakeAvailable(googletest)

        set_target_properties(gtest PROPERTIES FOLDER "thirdparty")
        set_target_properties(gtest_main PROPERTIES FOLDER "thirdparty")

        # For WASM: GoogleTest needs exception support
        if(EMSCRIPTEN)
            target_compile_options(gtest PRIVATE -fwasm-exceptions)
            target_link_options(gtest PRIVATE -fwasm-exceptions)
            target_compile_options(gtest_main PRIVATE -fwasm-exceptions)
            target_link_options(gtest_main PRIVATE -fwasm-exceptions)
        endif()
    else()
        message(STATUS "Using system GoogleTest")
    endif()
endif()

# GoogleBenchmark
if(BEHL_BUILD_BENCHMARKS AND NOT EMSCRIPTEN)
    find_package(benchmark QUIET)
    
    if(NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found on system, fetching from source")
        FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG        v1.9.4
        )

        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(googlebenchmark)
        set_target_properties(benchmark PROPERTIES FOLDER "thirdparty")
        set_target_properties(benchmark_main PROPERTIES FOLDER "thirdparty")
    else()
        message(STATUS "Using system Google Benchmark")
    endif()
endif()
