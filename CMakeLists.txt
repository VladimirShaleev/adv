cmake_minimum_required(VERSION 3.20)

include(cmake/version.cmake)

option(ADV_BUILD_TOOL "Build the advc compiler tool" ON)
if(ADV_BUILD_TOOL)
    list(APPEND VCPKG_MANIFEST_FEATURES "tool")
endif()

project(adv
    HOMEPAGE_URL "https://github.com/VladimirShaleev/adv"
    DESCRIPTION "An embeddable bytecode VM and compiler for the Adventure scripting language (vm, adv, and advc CLI)"
    VERSION ${ADV_VERSION}
    LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(cmake/utils.cmake)
include(cmake/flex-install.cmake)
include(cmake/bison-install.cmake)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ADV_MSVC_DYNAMIC_RUNTIME "Link dynamic runtime library instead of static" OFF)
cmake_dependent_option(ADV_ENABLE_INSTALL "Enable installation" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(ADV_BUILD_PACKAGES "Build packages" ON "ADV_ENABLE_INSTALL" OFF)

find_package(fmt CONFIG REQUIRED)
if(ADV_BUILD_TOOL)
    find_package(argparse CONFIG REQUIRED)
endif()

file(GLOB VM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.cpp")
add_library(vm ${VM_SOURCES})
add_library(${PROJECT_NAME}::vm ALIAS vm)
set_default_properties(TARGET vm STATIC VM_STATIC_BUILD)
target_precompile_headers(vm PRIVATE "src/vm/vm.hpp")
target_include_directories(vm PRIVATE ${PROJECT_SOURCE_DIR}/src/vm)
target_include_directories(vm PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

bison_target(parser
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/parser.yy"
    "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp")
flex_target(lexer
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/lexer.ll"
    "${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp")
add_flex_bison_dependency(lexer parser)
file(GLOB ADV_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/*.cpp")
list(FILTER ADV_SOURCES EXCLUDE REGEX "main.cpp")
add_library(adv ${ADV_SOURCES} ${BISON_parser_OUTPUTS} ${FLEX_lexer_OUTPUTS})
add_library(${PROJECT_NAME}::adv ALIAS adv)
set_default_properties(TARGET adv STATIC ADV_STATIC_BUILD)
target_link_libraries(adv PRIVATE vm)
target_link_libraries(adv PRIVATE fmt::fmt)
target_precompile_headers(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/compiler.hpp")
target_compile_definitions(adv PRIVATE $<$<CONFIG:Debug>:YYDEBUG>)
target_include_directories(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/")
target_include_directories(adv PRIVATE "$<$<BOOL:${FLEX_INCLUDE_DIR}>:${FLEX_INCLUDE_DIR}>")
target_include_directories(adv PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(adv PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(ADV_BUILD_TOOL)
    add_executable(advc "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/main.cpp")
    add_executable(${PROJECT_NAME}::advc ALIAS advc)
    set_default_properties(TARGET advc)
    target_link_libraries(advc PRIVATE adv)
    target_link_libraries(advc PRIVATE argparse::argparse)
endif()
