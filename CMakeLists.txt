cmake_minimum_required(VERSION 3.31)

project(prebyte-engine VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(PREBYTE_ENGINE_BUILD_SHARED "Build shared prebyte_engine library" ON)
option(PREBYTE_ENGINE_BUILD_STATIC "Build static prebyte_engine library" ON)
option(PREBYTE_ENGINE_BUILD_C "Build C FFI library" ON)
option(PREBYTE_ENGINE_BUILD_TESTS "Build engine test binary" ON)
option(PREBYTE_ENGINE_BUILD_FUZZERS "Build libFuzzer targets" OFF)
option(PREBYTE_ENGINE_ENABLE_COVERAGE "Enable GCC/GCov coverage instrumentation" OFF)
option(PREBYTE_ENGINE_ENABLE_SANITIZERS "Enable Clang sanitizer instrumentation" OFF)
set(PREBYTE_ENGINE_SANITIZERS "address,undefined"
    CACHE STRING "Comma-separated Clang sanitizers: address, undefined, thread, memory")
set(PREBYTE_MSAN_LIBCXX_PREFIX ""
    CACHE PATH "Install prefix of a MemorySanitizer-instrumented libc++ (required for reliable MSan runs)")

include(CTest)

find_package(Lua REQUIRED)

if(TARGET Lua::Lua)
    set(PREBYTE_LUA_TARGET Lua::Lua)
else()
    set(PREBYTE_LUA_TARGET ${LUA_LIBRARIES})
endif()

set(PREBYTE_PLATFORM_LIBS)
if(UNIX AND NOT APPLE)
    list(APPEND PREBYTE_PLATFORM_LIBS m)
endif()
if(CMAKE_DL_LIBS)
    list(APPEND PREBYTE_PLATFORM_LIBS ${CMAKE_DL_LIBS})
endif()

file(GLOB CONFIG_SOURCES CONFIGURE_DEPENDS src/config/*.cpp)
file(GLOB IO_SOURCES CONFIGURE_DEPENDS src/io/*.cpp)
file(GLOB_RECURSE RUNTIME_SOURCES CONFIGURE_DEPENDS src/runtime/*.cpp)
file(GLOB SUPPORT_SOURCES CONFIGURE_DEPENDS src/support/*.cpp)
file(GLOB TEMPLATE_AST_SOURCES CONFIGURE_DEPENDS src/template/ast/*.cpp)
file(GLOB TEMPLATE_LEXER_SOURCES CONFIGURE_DEPENDS src/template/lexer/*.cpp)
file(GLOB TEMPLATE_PARSER_SOURCES CONFIGURE_DEPENDS src/template/parser/*.cpp)

set(PREBYTE_ENGINE_SOURCES
    ${CONFIG_SOURCES}
    ${IO_SOURCES}
    ${RUNTIME_SOURCES}
    ${SUPPORT_SOURCES}
    ${TEMPLATE_AST_SOURCES}
    ${TEMPLATE_LEXER_SOURCES}
    ${TEMPLATE_PARSER_SOURCES}
    src/PrebyteEngine.cpp
    src/datatypes/Data.cpp
    src/parser/EnvParser.cpp
    src/parser/FileParser.cpp
    src/parser/IniParser.cpp
    src/parser/JsonParser.cpp
    src/parser/TomlParser.cpp
    src/parser/YamlParser.cpp
)

add_library(prebyte_engine_objects OBJECT ${PREBYTE_ENGINE_SOURCES})
target_include_directories(prebyte_engine_objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${LUA_INCLUDE_DIR}
)
target_link_libraries(prebyte_engine_objects
    PUBLIC
        ${PREBYTE_LUA_TARGET}
        ${PREBYTE_PLATFORM_LIBS}
)
set_target_properties(prebyte_engine_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(prebyte_engine_configure_target target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_23)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${target_name}
        PUBLIC
            ${PREBYTE_LUA_TARGET}
            ${PREBYTE_PLATFORM_LIBS}
    )
endfunction()

if(PREBYTE_ENGINE_BUILD_STATIC)
    add_library(prebyte_engine_static STATIC $<TARGET_OBJECTS:prebyte_engine_objects>)
    prebyte_engine_configure_target(prebyte_engine_static)
    add_library(prebyte::engine_static ALIAS prebyte_engine_static)
endif()

if(PREBYTE_ENGINE_BUILD_SHARED)
    add_library(prebyte_engine SHARED $<TARGET_OBJECTS:prebyte_engine_objects>)
    prebyte_engine_configure_target(prebyte_engine)
    target_compile_definitions(prebyte_engine PRIVATE PREBYTE_ENGINE_EXPORTS)
    set_target_properties(prebyte_engine PROPERTIES
        OUTPUT_NAME prebyte_engine
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    add_library(prebyte::engine ALIAS prebyte_engine)
endif()

if(PREBYTE_ENGINE_BUILD_C)
    if(NOT TARGET prebyte_engine AND NOT TARGET prebyte_engine_static)
        message(FATAL_ERROR "PREBYTE_ENGINE_BUILD_C requires PREBYTE_ENGINE_BUILD_SHARED=ON or PREBYTE_ENGINE_BUILD_STATIC=ON")
    endif()

    add_library(prebyte_engine_c SHARED src/ffi/prebyte_engine_c.cpp)
    target_compile_features(prebyte_engine_c PUBLIC c_std_11)
    target_compile_definitions(prebyte_engine_c PRIVATE PREBYTE_ENGINE_C_EXPORTS)
    target_include_directories(prebyte_engine_c
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    if(TARGET prebyte_engine)
        target_link_libraries(prebyte_engine_c PUBLIC prebyte_engine)
    else()
        target_link_libraries(prebyte_engine_c PUBLIC prebyte_engine_static)
    endif()
    set_target_properties(prebyte_engine_c PROPERTIES
        OUTPUT_NAME prebyte_engine_c
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    add_library(prebyte::engine_c ALIAS prebyte_engine_c)
endif()

if(PREBYTE_ENGINE_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "PREBYTE_ENGINE_ENABLE_SANITIZERS requires Clang")
    endif()

    if(PREBYTE_ENGINE_SANITIZERS STREQUAL "")
        set(PREBYTE_ENGINE_SANITIZERS "address,undefined")
    endif()

    string(REPLACE "," ";" PREBYTE_ENGINE_SANITIZER_LIST "${PREBYTE_ENGINE_SANITIZERS}")
    set(PREBYTE_ENGINE_HAS_ADDRESS_SANITIZER OFF)
    set(PREBYTE_ENGINE_HAS_THREAD_SANITIZER OFF)
    set(PREBYTE_ENGINE_HAS_MEMORY_SANITIZER OFF)

    foreach(PREBYTE_ENGINE_SANITIZER IN LISTS PREBYTE_ENGINE_SANITIZER_LIST)
        string(STRIP "${PREBYTE_ENGINE_SANITIZER}" PREBYTE_ENGINE_SANITIZER)
        if(PREBYTE_ENGINE_SANITIZER STREQUAL "address")
            set(PREBYTE_ENGINE_HAS_ADDRESS_SANITIZER ON)
        elseif(PREBYTE_ENGINE_SANITIZER STREQUAL "thread")
            set(PREBYTE_ENGINE_HAS_THREAD_SANITIZER ON)
        elseif(PREBYTE_ENGINE_SANITIZER STREQUAL "memory")
            set(PREBYTE_ENGINE_HAS_MEMORY_SANITIZER ON)
        endif()
    endforeach()

    if(PREBYTE_ENGINE_HAS_THREAD_SANITIZER AND PREBYTE_ENGINE_HAS_MEMORY_SANITIZER)
        message(FATAL_ERROR "TSan and MSan cannot be combined")
    endif()
    if(PREBYTE_ENGINE_HAS_ADDRESS_SANITIZER AND PREBYTE_ENGINE_HAS_THREAD_SANITIZER)
        message(FATAL_ERROR "ASan and TSan cannot be combined")
    endif()
    if(PREBYTE_ENGINE_HAS_ADDRESS_SANITIZER AND PREBYTE_ENGINE_HAS_MEMORY_SANITIZER)
        message(FATAL_ERROR "ASan and MSan cannot be combined")
    endif()

    set(PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS -fsanitize=${PREBYTE_ENGINE_SANITIZERS} -fno-omit-frame-pointer -g)
    set(PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS -fsanitize=${PREBYTE_ENGINE_SANITIZERS})
    if(PREBYTE_ENGINE_HAS_MEMORY_SANITIZER)
        list(APPEND PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS -fsanitize-memory-track-origins=1)
        if(PREBYTE_MSAN_LIBCXX_PREFIX STREQUAL "")
            message(WARNING "PREBYTE_MSAN_LIBCXX_PREFIX is unset; MSan runs may report false positives from uninstrumented libstdc++")
        elseif(NOT EXISTS "${PREBYTE_MSAN_LIBCXX_PREFIX}/include/c++/v1/string")
            message(FATAL_ERROR "PREBYTE_MSAN_LIBCXX_PREFIX does not contain libc++ headers: ${PREBYTE_MSAN_LIBCXX_PREFIX}")
        else()
            list(APPEND PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS
                -stdlib=libc++
                -nostdinc++
                -isystem "${PREBYTE_MSAN_LIBCXX_PREFIX}/include/c++/v1")
            link_directories("${PREBYTE_MSAN_LIBCXX_PREFIX}/lib")
            list(APPEND PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS
                -stdlib=libc++
                "LINKER:-rpath,${PREBYTE_MSAN_LIBCXX_PREFIX}/lib"
                -lc++
                -lc++abi)
        endif()
    endif()

    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS}>")
    add_link_options(${PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS})
    target_compile_options(prebyte_engine_objects PUBLIC ${PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS})
    target_link_options(prebyte_engine_objects PUBLIC ${PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS})
    if(TARGET prebyte_engine_static)
        target_compile_options(prebyte_engine_static PUBLIC ${PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS})
        target_link_options(prebyte_engine_static PUBLIC ${PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS})
    endif()
    if(TARGET prebyte_engine)
        target_compile_options(prebyte_engine PUBLIC ${PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS})
        target_link_options(prebyte_engine PUBLIC ${PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS})
    endif()
    if(TARGET prebyte_engine_c)
        target_compile_options(prebyte_engine_c PUBLIC ${PREBYTE_ENGINE_SANITIZER_COMPILE_OPTIONS})
        target_link_options(prebyte_engine_c PUBLIC ${PREBYTE_ENGINE_SANITIZER_LINK_OPTIONS})
    endif()
endif()

install(FILES
    include/Engine.h
    include/PrebyteEngine.h
    include/prebyte_engine.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(TARGET prebyte_engine_static)
    install(TARGETS prebyte_engine_static
        EXPORT prebyte-engineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

if(TARGET prebyte_engine)
    install(TARGETS prebyte_engine
        EXPORT prebyte-engineTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(TARGET prebyte_engine_c)
    install(TARGETS prebyte_engine_c
        EXPORT prebyte-engineTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(EXPORT prebyte-engineTargets
    FILE prebyte-engineTargets.cmake
    NAMESPACE prebyte::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prebyte-engine
)

configure_package_config_file(
    cmake/prebyte-engine-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/prebyte-engine-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prebyte-engine
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/prebyte-engine-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/prebyte-engine-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/prebyte-engine-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prebyte-engine
)

if(PREBYTE_ENGINE_BUILD_TESTS)
    file(GLOB PREBYTE_ENGINE_UNIT_TEST_SOURCES CONFIGURE_DEPENDS tests/correctness/unit/*.cpp)
    file(GLOB PREBYTE_ENGINE_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS tests/correctness/integration/*.cpp)
    file(GLOB PREBYTE_ENGINE_PROPERTY_TEST_SOURCES CONFIGURE_DEPENDS tests/correctness/property/*.cpp)
    file(GLOB PREBYTE_ENGINE_SECURITY_TEST_SOURCES CONFIGURE_DEPENDS tests/security/*.cpp)
    file(GLOB PREBYTE_ENGINE_CONCURRENCY_TEST_SOURCES CONFIGURE_DEPENDS tests/concurrency/*.cpp)
    file(GLOB PREBYTE_ENGINE_REGRESSION_TEST_SOURCES CONFIGURE_DEPENDS tests/fault_tolerance/regression/*.cpp)

    set(PREBYTE_ENGINE_TEST_SOURCES
        ${PREBYTE_ENGINE_UNIT_TEST_SOURCES}
        ${PREBYTE_ENGINE_INTEGRATION_TEST_SOURCES}
        ${PREBYTE_ENGINE_PROPERTY_TEST_SOURCES}
        ${PREBYTE_ENGINE_SECURITY_TEST_SOURCES}
        ${PREBYTE_ENGINE_CONCURRENCY_TEST_SOURCES}
        ${PREBYTE_ENGINE_REGRESSION_TEST_SOURCES}
    )

    add_executable(prebyte_engine_tests
        tests/harness/TestHarness.cpp
        tests/harness/TestMain.cpp
        tests/support/EngineTestHelpers.cpp
        ${PREBYTE_ENGINE_TEST_SOURCES}
    )
    target_include_directories(prebyte_engine_tests PRIVATE
        tests
        tests/harness
        tests/support
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if(TARGET prebyte_engine_c)
        target_link_libraries(prebyte_engine_tests PRIVATE prebyte_engine_c)
    else()
        target_link_libraries(prebyte_engine_tests PRIVATE prebyte_engine_static)
    endif()

    if(PREBYTE_ENGINE_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(prebyte_engine_objects PRIVATE --coverage -O0 -g)
            target_compile_options(prebyte_engine_static PUBLIC --coverage -O0 -g)
            target_link_options(prebyte_engine_static PUBLIC --coverage)
            if(TARGET prebyte_engine)
                target_compile_options(prebyte_engine PUBLIC --coverage -O0 -g)
                target_link_options(prebyte_engine PUBLIC --coverage)
            endif()
            if(TARGET prebyte_engine_c)
                target_compile_options(prebyte_engine_c PRIVATE --coverage -O0 -g)
                target_link_options(prebyte_engine_c PRIVATE --coverage)
            endif()
            target_compile_options(prebyte_engine_tests PRIVATE --coverage -O0 -g)
            target_link_options(prebyte_engine_tests PRIVATE --coverage)
        endif()
    endif()

    enable_testing()

    set(PREBYTE_DISCOVERED_TEST_NAMES)
    set(PREBYTE_ENGINE_DISABLED_TEST_NAMES)
    if(UNIX AND NOT APPLE)
        list(APPEND PREBYTE_ENGINE_DISABLED_TEST_NAMES OutputWriter_writes_stdout_when_no_path_is_given_without_pipe)
    endif()

    foreach(PREBYTE_TEST_SOURCE IN LISTS PREBYTE_ENGINE_TEST_SOURCES)
        file(STRINGS ${PREBYTE_TEST_SOURCE} PREBYTE_TEST_DECLARATIONS REGEX "^[ \t]*TEST_CASE\\([A-Za-z_][A-Za-z0-9_]*\\)")
        foreach(PREBYTE_TEST_DECLARATION IN LISTS PREBYTE_TEST_DECLARATIONS)
            string(REGEX MATCH "TEST_CASE\\(([A-Za-z_][A-Za-z0-9_]*)\\)" PREBYTE_TEST_MATCH "${PREBYTE_TEST_DECLARATION}")
            set(PREBYTE_TEST_NAME ${CMAKE_MATCH_1})
            if(PREBYTE_TEST_NAME STREQUAL "")
                continue()
            endif()
            if(PREBYTE_TEST_NAME IN_LIST PREBYTE_ENGINE_DISABLED_TEST_NAMES)
                continue()
            endif()
            if(PREBYTE_TEST_NAME IN_LIST PREBYTE_DISCOVERED_TEST_NAMES)
                continue()
            endif()
            list(APPEND PREBYTE_DISCOVERED_TEST_NAMES ${PREBYTE_TEST_NAME})
            add_test(
                NAME ${PREBYTE_TEST_NAME}
                COMMAND $<TARGET_FILE:prebyte_engine_tests> --run-test ${PREBYTE_TEST_NAME}
            )
            set_tests_properties(${PREBYTE_TEST_NAME} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        endforeach()
    endforeach()
endif()

if(PREBYTE_ENGINE_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        message(FATAL_ERROR "PREBYTE_ENGINE_BUILD_FUZZERS requires Clang")
    endif()
    if(NOT PREBYTE_ENGINE_ENABLE_SANITIZERS)
        message(FATAL_ERROR "PREBYTE_ENGINE_BUILD_FUZZERS requires PREBYTE_ENGINE_ENABLE_SANITIZERS")
    endif()

    enable_testing()
    set(PREBYTE_ENGINE_FUZZ_TARGETS
        fuzz_template_lexer fuzz_template_parser fuzz_json_parser fuzz_yaml_parser
        fuzz_toml_parser fuzz_ini_parser fuzz_env_parser fuzz_compiled_template_serializer
        fuzz_settings_loader fuzz_include_resolver fuzz_file_parser fuzz_lua_chunk
        fuzz_render_pbt fuzz_structured_import fuzz_lua_sandbox
    )
    set(PREBYTE_ENGINE_FUZZ_SOURCES
        tests/fault_tolerance/fuzz/TemplateLexerFuzz.cpp
        tests/fault_tolerance/fuzz/TemplateParserFuzz.cpp
        tests/fault_tolerance/fuzz/JsonParserFuzz.cpp
        tests/fault_tolerance/fuzz/YamlParserFuzz.cpp
        tests/fault_tolerance/fuzz/TomlParserFuzz.cpp
        tests/fault_tolerance/fuzz/IniParserFuzz.cpp
        tests/fault_tolerance/fuzz/EnvParserFuzz.cpp
        tests/fault_tolerance/fuzz/CompiledTemplateSerializerFuzz.cpp
        tests/fault_tolerance/fuzz/SettingsLoaderFuzz.cpp
        tests/fault_tolerance/fuzz/IncludeResolverFuzz.cpp
        tests/fault_tolerance/fuzz/FileParserFuzz.cpp
        tests/fault_tolerance/fuzz/LuaChunkFuzz.cpp
        tests/fault_tolerance/fuzz/RenderPbtFuzz.cpp
        tests/fault_tolerance/fuzz/StructuredImportFuzz.cpp
        tests/fault_tolerance/fuzz/LuaSandboxFuzz.cpp
    )
    set(PREBYTE_ENGINE_FUZZ_HELPER_TARGETS fuzz_structured_import fuzz_lua_sandbox)
    list(LENGTH PREBYTE_ENGINE_FUZZ_TARGETS PREBYTE_ENGINE_FUZZ_COUNT)
    math(EXPR PREBYTE_ENGINE_FUZZ_LAST "${PREBYTE_ENGINE_FUZZ_COUNT} - 1")
    foreach(PREBYTE_FUZZ_INDEX RANGE ${PREBYTE_ENGINE_FUZZ_LAST})
        list(GET PREBYTE_ENGINE_FUZZ_TARGETS ${PREBYTE_FUZZ_INDEX} PREBYTE_FUZZ_TARGET)
        list(GET PREBYTE_ENGINE_FUZZ_SOURCES ${PREBYTE_FUZZ_INDEX} PREBYTE_FUZZ_SOURCE)
        set(PREBYTE_FUZZ_TARGET_SOURCES ${PREBYTE_FUZZ_SOURCE})
        if(PREBYTE_FUZZ_TARGET IN_LIST PREBYTE_ENGINE_FUZZ_HELPER_TARGETS)
            list(APPEND PREBYTE_FUZZ_TARGET_SOURCES tests/support/EngineTestHelpers.cpp)
        endif()
        add_executable(${PREBYTE_FUZZ_TARGET} ${PREBYTE_FUZZ_TARGET_SOURCES})
        target_include_directories(${PREBYTE_FUZZ_TARGET} PRIVATE
            tests/fault_tolerance/fuzz/support
            tests/support
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(${PREBYTE_FUZZ_TARGET} PRIVATE prebyte_engine_static)
        target_compile_options(${PREBYTE_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)
        target_link_options(${PREBYTE_FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)
    endforeach()

    add_executable(generate_fuzz_pbc_seed tests/fault_tolerance/fuzz/GeneratePbcSeed.cpp)
    target_include_directories(generate_fuzz_pbc_seed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(generate_fuzz_pbc_seed PRIVATE prebyte_engine_static)

    set(PREBYTE_ENGINE_FUZZ_PBC_SEED ${CMAKE_SOURCE_DIR}/tests/fault_tolerance/fuzz/seeds/pbc/minimal.pbc)
    add_custom_command(
        OUTPUT ${PREBYTE_ENGINE_FUZZ_PBC_SEED}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/tests/fault_tolerance/fuzz/seeds/pbc
        COMMAND generate_fuzz_pbc_seed ${PREBYTE_ENGINE_FUZZ_PBC_SEED}
        DEPENDS generate_fuzz_pbc_seed
        VERBATIM
    )
    add_custom_target(fuzz_pbc_seed DEPENDS ${PREBYTE_ENGINE_FUZZ_PBC_SEED})
    add_dependencies(fuzz_compiled_template_serializer fuzz_pbc_seed)
endif()
