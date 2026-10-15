cmake_minimum_required(VERSION 3.21)

include(cmake/info.cmake)

if(EMSCRIPTEN)
    set(ADV_BUILD_DEFAULT OFF)
else()
    set(ADV_BUILD_DEFAULT ON)
endif()
option(ADV_BUILD_ADV "Build the Adv compiler library" ${ADV_BUILD_DEFAULT})
option(ADV_BUILD_TOOL "Build the advc compiler tool" ${ADV_BUILD_DEFAULT})
option(ADV_BUILD_TESTS "Build tests" ${ADV_BUILD_DEFAULT})
if(ADV_BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()
if(ADV_BUILD_TOOL)
    if(NOT ADV_BUILD_ADV)
        message(FATAL_ERROR "ADV_BUILD_TOOL requires ADV_BUILD_ADV")
    endif()
    list(APPEND VCPKG_MANIFEST_FEATURES "tool")
endif()

project(adv
    HOMEPAGE_URL ${ADV_HOMEPAGE}
    DESCRIPTION ${ADV_DESCRIPTION}
    VERSION ${ADV_VERSION}
    LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(cmake/utils.cmake)
include(cmake/update-version.cmake)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ADV_MSVC_DYNAMIC_RUNTIME "Link dynamic runtime library instead of static" OFF)
cmake_dependent_option(ADV_BUILD_DOC "Build doc" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(ADV_ENABLE_INSTALL "Enable installation" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(ADV_BUILD_PACKAGES "Build packages" ON "ADV_ENABLE_INSTALL" OFF)

find_package(fmt CONFIG REQUIRED)
if(ADV_BUILD_TOOL)
    find_package(argparse CONFIG REQUIRED)
endif()

file(GLOB UTILS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp")
add_library(utils OBJECT ${UTILS_SOURCES})
set_default_properties(TARGET utils)
if(EMSCRIPTEN)
    target_compile_options(utils PRIVATE -fexceptions)
endif()
target_link_libraries(utils PRIVATE fmt::fmt)
target_include_directories(utils PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/utils")
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB VM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.cpp")
add_library(vm ${VM_SOURCES})
add_library(${PROJECT_NAME}::vm ALIAS vm)
set_default_properties(TARGET vm STATIC VM_STATIC_BUILD)
target_link_libraries(vm PRIVATE utils)
target_link_libraries(vm PRIVATE fmt::fmt)
target_precompile_headers(vm PRIVATE "src/vm/vm.hpp")
target_include_directories(vm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vm")
target_include_directories(vm PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(ADV_BUILD_ADV)
    include(cmake/flex-install.cmake)
    include(cmake/bison-install.cmake)
    bison_target(parser
        "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/parser.yy"
        "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp")
    flex_target(scanner
        "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/scanner.ll"
        "${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp")
    add_flex_bison_dependency(scanner parser)
    file(GLOB ADV_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/*.cpp")
    add_library(adv ${ADV_SOURCES} ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
    add_library(${PROJECT_NAME}::adv ALIAS adv)
    set_default_properties(TARGET adv STATIC ADV_STATIC_BUILD)
    target_link_libraries(adv PRIVATE utils)
    target_link_libraries(adv PRIVATE vm)
    target_link_libraries(adv PRIVATE fmt::fmt)
    target_precompile_headers(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/compiler.hpp")
    target_compile_definitions(adv PRIVATE $<$<CONFIG:Debug>:YYDEBUG>)
    target_include_directories(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/")
    target_include_directories(adv PRIVATE "$<$<BOOL:${FLEX_INCLUDE_DIR}>:${FLEX_INCLUDE_DIR}>")
    target_include_directories(adv PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(adv PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    if(ADV_BUILD_TOOL)
        add_executable(advc "${CMAKE_CURRENT_SOURCE_DIR}/src/advc/main.cpp")
        add_executable(${PROJECT_NAME}::advc ALIAS advc)
        set_default_properties(TARGET advc)
        target_link_libraries(advc PRIVATE adv)
        target_link_libraries(advc PRIVATE argparse::argparse)
    endif()
endif()

if(EMSCRIPTEN)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/dist")
    target_compile_options(vm PRIVATE -fexceptions)

    set(VM_JS_LINK_OPTIONS
        -sWASM=1
        -sMODULARIZE=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORT_NAME=VM
        -sDISABLE_EXCEPTION_CATCHING=0)

    foreach(VM_JS_TARGET IN ITEMS adv-vm adv-vm-esm)
        add_executable(${VM_JS_TARGET} src/vm.js.cpp)
        target_link_libraries(${VM_JS_TARGET} PRIVATE vm embind)
        set_default_properties(TARGET ${VM_JS_TARGET})
        target_compile_options(${VM_JS_TARGET} PRIVATE -fexceptions)
        target_link_options(${VM_JS_TARGET} PRIVATE ${VM_JS_LINK_OPTIONS})
        set_target_properties(${VM_JS_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/dist")
    endforeach()

    target_link_options(adv-vm PRIVATE --emit-tsd "${PROJECT_BINARY_DIR}/dist/adv-vm.d.ts")
    target_link_options(adv-vm-esm PRIVATE -sEXPORT_ES6=1 --emit-tsd "${PROJECT_BINARY_DIR}/dist/adv-vm.esm.d.ts")
    set_target_properties(adv-vm PROPERTIES OUTPUT_NAME "adv-vm" SUFFIX ".js")
    set_target_properties(adv-vm-esm PROPERTIES OUTPUT_NAME "adv-vm.esm" SUFFIX ".mjs")

    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/package.json.in"
        "${PROJECT_BINARY_DIR}/dist/package.json"
        @ONLY)
    configure_file(
        "${PROJECT_SOURCE_DIR}/README.md"
        "${PROJECT_BINARY_DIR}/dist/README.md"
        COPYONLY)
endif()

if(ADV_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(ADV_BUILD_DOC)
  add_subdirectory(doc)
endif()
