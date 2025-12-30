cmake_minimum_required(VERSION 3.26)

project(Behl
    VERSION 0.0.1
    DESCRIPTION "Lua-inspired scripting language with C-like syntax, implemented in C++20."
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/behl-lang/behl"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(BEHL_IS_TOP_LEVEL TRUE)
else()
    set(BEHL_IS_TOP_LEVEL FALSE)
endif()

option(BUILD_SHARED_LIBS "Build Behl as a shared library" OFF)
option(BEHL_BUILD_CLI "Build Behl CLI executable" ${BEHL_IS_TOP_LEVEL})
option(BEHL_BUILD_TESTS "Build Behl tests" ${BEHL_IS_TOP_LEVEL})
option(BEHL_BUILD_BENCHMARKS "Build Behl benchmarks" ${BEHL_IS_TOP_LEVEL})

# Compiler options and optimizations
add_library(compiler_opts INTERFACE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(compiler_opts INTERFACE
        /MP # Multi-processor compilation
        /W4 # Level 4 warnings
        /WX # Warnings as errors
        /permissive- # Strict standards conformance
        /w44062 # Enumerator in switch not handled
        /w44242 # Conversion with possible loss of data
        /w44263 # Non-virtual member function hides virtual one
        /w44265 # Class has virtual functions but destructor is not virtual
        /w44464 # Signed/unsigned mismatch
        /w45038 # Narrowing conversion
        /wd4251 # Disable 'class needs dll-interface' (common in libs)
        /wd4275 # Disable non-dll-interface base class
        /utf-8  # Warning C4819 utf-8 source file
        $<$<CONFIG:Release>:/Oi> # Enable intrinsic functions
        $<$<CONFIG:Release>:/GS-> # Disable security checks
        $<$<CONFIG:Release>:/Zi> # Debug info in release
    )
    target_link_options(compiler_opts INTERFACE
        $<$<CONFIG:Release>:/DEBUG>
    )
else()
    target_compile_options(compiler_opts INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion -Wnull-dereference
        -Wdouble-promotion -Wformat=2
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(compiler_opts INTERFACE -Werror)
    endif()
    if(EMSCRIPTEN)
        target_compile_options(compiler_opts INTERFACE -fwasm-exceptions)
        target_link_options(compiler_opts INTERFACE -fwasm-exceptions)
    endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if(BEHL_BUILD_TESTS OR BEHL_BUILD_BENCHMARKS)
    add_subdirectory(thirdparty)
endif()

set(BEHL_PRIVATE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_call.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_global.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_load.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_pin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_stack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/api_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast/ast.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast/ast_dump.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast/ast_holder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast/ast_holder.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast/ast_transformer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/compiler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/proto.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/proto.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/charconv_compat.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/format.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/hash_map.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/print.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/string.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/vector.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config_internal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exceptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/export_transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/export_transform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/semantics_pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/semantics_pass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc_list.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc_object.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc_state.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc_types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gc_types_fmt.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_closure.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_function.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_proto.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_string.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_table.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc/gco_userdata.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_fs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_gc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_os.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/lib_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/process/process_platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/process/process_posix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libs/process/process_win32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/ast_context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/constant_folding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/constant_folding.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/dead_store_elimination.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/dead_store_elimination.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/loop_optimization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/ast/loop_optimization.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/optimization.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/optimization/optimization_pass.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/state.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/state_debug.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/bytecode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/bytecode.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/bytecode_meta.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/frame.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/integer_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/upvalue.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/value.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_arithmetic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_bitwise.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_controlflow.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_debug.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_detail.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_load.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_metatable.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_operands.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_table.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm_upvalues.hpp
)

set(BEHL_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/config.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/debug.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/exceptions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/export.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl/behl.hpp
)

set(BEHL_PRIVATE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/behl
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ast
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vm
)

add_library(libbehl)

set_target_properties(libbehl PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    add_library(behl_natvis INTERFACE)
    target_sources(behl_natvis INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/behl.natvis>)
    target_link_libraries(libbehl INTERFACE behl_natvis)
endif()

target_sources(libbehl
    PRIVATE 
        ${BEHL_PRIVATE_SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        ${BEHL_PUBLIC_HEADERS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${BEHL_PRIVATE_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "include" FILES ${BEHL_PUBLIC_HEADERS})

target_include_directories(libbehl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/behl
)

target_link_libraries(libbehl
    PRIVATE
    $<BUILD_INTERFACE:compiler_opts>
)

target_compile_definitions(libbehl PRIVATE BEHL_BUILDING_LIBRARY)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(libbehl PUBLIC BEHL_SHARED_LIBRARY)
endif()

# Tests
if(BEHL_BUILD_TESTS)
    set(BEHL_TEST_SOURCES
        tests/main.cpp
        tests/api_tests.cpp
        tests/bitwise_tests.cpp
        tests/callstack_tests.cpp
        tests/cfunction_tests.cpp
        tests/compare_tests.cpp
        tests/const_tests.cpp
        tests/controflow_tests.cpp
        tests/debug_tests.cpp
        tests/defer_tests.cpp
        tests/edge_case_tests.cpp
        tests/error_tests.cpp
        tests/format_tests.cpp
        tests/string_format_tests.cpp
        tests/function_tests.cpp
        tests/gc_list_tests.cpp
        tests/gc_stress_tests.cpp
        tests/gc_tests.cpp
        tests/lexer_tests.cpp
        tests/load_tests.cpp
        tests/logical_operator_tests.cpp
        tests/metatable_tests.cpp
        tests/module_tests.cpp
        tests/multireturn_tests.cpp
        tests/numeric_tests.cpp
        tests/numeric_literal_tests.cpp
        tests/integer_wrapping_tests.cpp
        tests/operator_tests.cpp
        tests/parser_tests.cpp
        tests/pcall_tests.cpp
        tests/pinning_tests.cpp
        tests/process_tests.cpp
        tests/recursion_tests.cpp
        tests/register_tests.cpp
        tests/regression_tests.cpp
        tests/table_tests.cpp
        tests/tonumber_tests.cpp
        tests/tostring_tests.cpp
        tests/typecheck_tests.cpp
        tests/userdata_tests.cpp
        tests/varargs_tests.cpp
        tests/variable_tests.cpp
        tests/vector_tests.cpp
        tests/while_loop_tests.cpp
        tests/scoping_tests.cpp
        tests/operator_precedence_tests.cpp
        tests/closure_tests.cpp
        tests/loop_tests.cpp
        tests/optimizations_tests.cpp
        tests/table_construction_tests.cpp
    )

    add_executable(behl_tests)
    set_target_properties(behl_tests PROPERTIES FOLDER "tests")
    target_sources(behl_tests PRIVATE ${BEHL_TEST_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests PREFIX "tests" FILES ${BEHL_TEST_SOURCES})

    target_include_directories(behl_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(behl_tests
        PRIVATE
            compiler_opts
            libbehl
            GTest::gtest_main
    )

    include(CTest)
    enable_testing()

    include(GoogleTest)
    gtest_discover_tests(behl_tests
        DISCOVERY_TIMEOUT 60
    )
endif()

# CLI executable
if(BEHL_BUILD_CLI AND NOT EMSCRIPTEN)
    set(BEHL_SOURCES src/cli.cpp)

    add_executable(behl)
    target_sources(behl PRIVATE ${BEHL_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${BEHL_SOURCES})

    target_include_directories(behl
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(behl
        PRIVATE
            libbehl
            compiler_opts
    )
endif()

# LSP Server (WASM)
if(EMSCRIPTEN)
    set(BEHL_LSP_SOURCES
        src/lsp/lsp_server.cpp
        src/lsp/lsp_server.hpp
        src/lsp/lsp_wasm.cpp
        src/lsp/module_analyzer.cpp
        src/lsp/module_analyzer.hpp
        src/lsp/symbol_collector.cpp
        src/lsp/symbol_collector.hpp
    )
    
    add_executable(behl_lsp ${BEHL_LSP_SOURCES})
    
    target_include_directories(behl_lsp
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp
    )
    
    target_link_libraries(behl_lsp
        PRIVATE
            libbehl
    )
    
    target_compile_options(behl_lsp PRIVATE -fwasm-exceptions)
    set_target_properties(behl_lsp PROPERTIES
        LINK_FLAGS "-s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME='createBehlLSP' -lembind -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=node -s FORCE_FILESYSTEM=1 -s EXPORTED_RUNTIME_METHODS=['FS'] -fwasm-exceptions"
    )
    
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${BEHL_LSP_SOURCES})
endif()

# Benchmarks
if(BEHL_BUILD_BENCHMARKS AND NOT EMSCRIPTEN)
    set(BENCHMARK_SOURCES
        benchmarks/lexer_benchmarks.cpp
        benchmarks/parser_benchmarks.cpp
        benchmarks/semantic_benchmarks.cpp
        benchmarks/compiler_benchmarks.cpp
        benchmarks/scriptcall_benchmarks.cpp
    )

    add_executable(benchmarks)
    set_target_properties(benchmarks PROPERTIES FOLDER "benchmarks")

    target_sources(benchmarks PRIVATE ${BENCHMARK_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks PREFIX "benchmarks" FILES ${BENCHMARK_SOURCES})

    target_include_directories(benchmarks
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(benchmarks
        PRIVATE
            compiler_opts
            libbehl
            benchmark::benchmark_main
    )
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS libbehl
    EXPORT behlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(MSVC)
    install(TARGETS behl_natvis
        EXPORT behlTargets
        OPTIONAL
    )
    install(FILES behl.natvis
        DESTINATION ${CMAKE_INSTALL_DATADIR}/behl
        OPTIONAL
    )
endif()

install(EXPORT behlTargets
    FILE behlTargets.cmake
    NAMESPACE behl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/behl
)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/behlConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/behlConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/behlConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/behl
)

# pkg-config
configure_file(cmake/behl.pc.in behl.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/behl.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
