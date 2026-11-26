vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO VladimirShaleev/adv
    REF 032820b761407e88401d42b0743703061d4bcffa
    SHA512 2c9f3a66736a3955bdac4eb6fd613afce34c92dcfd95ff7bfd09e9161fcbc8be3fdff528a2479586f946fea1153f2bfaee219d85c44a6c63e64a9b3c7ff86e56
    HEAD_REF main
)

string(COMPARE EQUAL "${TARGET_TRIPLET}" "${HOST_TRIPLET}" ADV_BUILD_BINARIES)
string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "dynamic" ADV_DYNAMIC_CRT)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" ADV_SHARED_LIBS)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DADV_USE_FMT=ON
        -DADV_FMT_HEADER_ONLY=ON
        -DADV_BOOTSTRAP=ON
        -DADV_BUILD_ADV=ON
        -DADV_BUILD_TOOL=${ADV_BUILD_BINARIES}
        -DADV_BUILD_TESTS=OFF
        -DADV_ENABLE_INSTALL=ON
        -DADV_BUILD_DOC=OFF
        -DADV_BUILD_PACKAGES=OFF
        -DADV_MSVC_DYNAMIC_RUNTIME=${ADV_DYNAMIC_CRT}
        -DBUILD_SHARED_LIBS=${ADV_SHARED_LIBS}
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

if(ADV_BUILD_BINARIES)
    if(VCPKG_TARGET_IS_WINDOWS)
        vcpkg_copy_tools(TOOL_NAMES advc AUTO_CLEAN)
    else()
        string(REPLACE "." ";" VERSION_LIST ${VERSION})
        list(GET VERSION_LIST 0 VERSION_MAJOR)
        list(GET VERSION_LIST 1 VERSION_MINOR)
        list(GET VERSION_LIST 2 VERSION_PATCH)
        vcpkg_copy_tools(TOOL_NAMES advc advc-${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} AUTO_CLEAN)
    endif()
else()
    file(COPY "${CURRENT_HOST_INSTALLED_DIR}/tools/${PORT}" DESTINATION "${CURRENT_PACKAGES_DIR}/tools")
endif()

vcpkg_cmake_config_fixup(PACKAGE_NAME adv CONFIG_PATH share/cmake/adv)

if(NOT ADV_BUILD_BINARIES)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/adv-targets-vcpkg.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/adv-targets-vcpkg.cmake" COPYONLY)
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/vcpkg-cmake-wrapper.cmake" @ONLY)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
