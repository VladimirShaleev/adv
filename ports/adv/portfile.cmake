vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO VladimirShaleev/adv
    REF d15e6df3fbb16b9bf0277295d055ebbde662d16a
    SHA512 3643c64d24dd9306cfa66ddd33e4f811fc3034086218eb467f22e6fc01bd1120f539ea65759c9af2ca378a619a5b92a7fc3aa676c7ce05372c955d8fac6377ac
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
