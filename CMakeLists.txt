cmake_minimum_required(VERSION 3.21)

include(cmake/info.cmake)

if(EMSCRIPTEN)
    set(ADV_BUILD_DEFAULT OFF)
else()
    set(ADV_BUILD_DEFAULT ON)
endif()
option(ADV_BUILD_ADV "Build the Adv compiler library" ON)
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
cmake_dependent_option(ADV_ENABLE_INSTALL "Enable installation" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(ADV_BUILD_DOC "Build doc" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
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
target_link_libraries(utils PRIVATE fmt::fmt)
target_include_directories(utils PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(utils PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/utils")
target_include_directories(utils PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(ADV_TARGETS utils)

# include(cmake/advc-compile.cmake)
# 
# file(GLOB ADV_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std/*.adv")
# adv_compile(NAME STD
#     WARN_AS_ERRORS TEXT
#     IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/std"
#     SOURCES ${ADV_SOURCES})

file(GLOB VM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/*.cpp")
list(FILTER VM_SOURCES EXCLUDE REGEX "\\.js\\.cpp")
add_library(vm ${VM_SOURCES} ${ADVC_STD_OUTPUTS})
add_library(${PROJECT_NAME}::vm ALIAS vm)
set_default_properties(TARGET vm STATIC VM_STATIC_BUILD)
target_link_libraries(vm PRIVATE utils)
target_link_libraries(vm PRIVATE fmt::fmt)
target_precompile_headers(vm PRIVATE "src/vm/vm.hpp")
target_include_directories(vm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vm")
target_include_directories(vm PRIVATE $<TARGET_PROPERTY:utils,INCLUDE_DIRECTORIES>)
target_include_directories(vm PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
list(APPEND ADV_TARGETS vm)

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
    list(FILTER ADV_SOURCES EXCLUDE REGEX "\\.js\\.cpp")
    add_library(adv ${ADV_SOURCES} ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
    add_library(${PROJECT_NAME}::adv ALIAS adv)
    set_default_properties(TARGET adv STATIC ADV_STATIC_BUILD)
    target_link_libraries(adv PRIVATE utils)
    target_link_libraries(adv PRIVATE vm)
    target_link_libraries(adv PRIVATE fmt::fmt)
    target_precompile_headers(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/compiler.hpp")
    target_compile_definitions(adv PRIVATE $<$<CONFIG:Debug>:YYDEBUG>)
    target_include_directories(adv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/")
    target_include_directories(adv PRIVATE $<TARGET_PROPERTY:utils,INCLUDE_DIRECTORIES>)
    target_include_directories(adv PRIVATE "$<$<BOOL:${FLEX_INCLUDE_DIR}>:${FLEX_INCLUDE_DIR}>")
    target_include_directories(adv PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(adv PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    list(APPEND ADV_TARGETS adv)
    if(ADV_BUILD_TOOL)
        add_executable(advc "${CMAKE_CURRENT_SOURCE_DIR}/src/advc/main.cpp")
        add_executable(${PROJECT_NAME}::advc ALIAS advc)
        set_default_properties(TARGET advc)
        target_link_libraries(advc PRIVATE adv)
        target_link_libraries(advc PRIVATE argparse::argparse)
        list(APPEND ADV_TARGETS advc)
    endif()
endif()

if(EMSCRIPTEN)
    target_compile_options(utils PRIVATE -fwasm-exceptions)
    target_compile_options(vm PRIVATE -fwasm-exceptions)
    add_emscripten(TARGET vm-js 
        EXPORT_NAME VM
        PACKAGE_NAME @vladimirshaleev/adv-vm
        DIST vm-dist
        DESCRIPTION "Bytecode virtual machine for the Adventure scripting language"
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/vm/vm.js.cpp"
        DEPS utils vm fmt::fmt)
    if(ADV_BUILD_ADV)
        target_compile_options(adv PRIVATE -fwasm-exceptions)
        add_emscripten(TARGET adv-js 
            EXPORT_NAME adv
            PACKAGE_NAME @vladimirshaleev/adv
            DIST adv-dist
            DESCRIPTION "Adventure scripting language"
            SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/adv.js.cpp"
            DEPS utils adv)
    endif()
endif()

if(ADV_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(ADV_BUILD_DOC)
    add_subdirectory(doc)
endif()

if(ADV_ENABLE_INSTALL)
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in" 
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO)

    write_basic_package_version_file(${PROJECT_NAME}-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    
    install(TARGETS ${ADV_TARGETS} EXPORT ${PROJECT_NAME}-targets)
    install(EXPORT ${PROJECT_NAME}-targets 
        DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
        NAMESPACE ${PROJECT_NAME}::)
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/adv-compile.cmake"
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME})
    install(DIRECTORY include/ DESTINATION include)
endif()

if(ADV_BUILD_PACKAGES)
    include(cmake/package.cmake)
endif()
