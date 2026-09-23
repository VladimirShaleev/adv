cmake_minimum_required(VERSION 3.20)

include(cmake/version.cmake)

option(ADV_BUILD_TOOL "Build the advc compiler tool" ON)
if(ADV_BUILD_TOOL)
    list(APPEND VCPKG_MANIFEST_FEATURES "tool")
endif()

project(adventure
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
if(ADVENTURE_BUILD_TOOL)
    find_package(argparse CONFIG REQUIRED)
endif()
