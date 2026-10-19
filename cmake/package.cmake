find_program(ADV_DPKG_BUILDPACKAGE_FOUND dpkg-buildpackage)
find_program(ADV_RPMBUILD_FOUND rpmbuild)
find_program(ADV_WIX_FOUND wix)

cmake_dependent_option(ADV_BUILD_DEB_PACKAGE "Create DEB package" ON "ADV_DPKG_BUILDPACKAGE_FOUND" OFF)
cmake_dependent_option(ADV_BUILD_RPM_PACKAGE "Create RPM package" ON "ADV_RPMBUILD_FOUND" OFF)
cmake_dependent_option(ADV_BUILD_MSI_INSTALLER "Create MSI installer" ON "ADV_WIX_FOUND" OFF)

list(APPEND CPACK_SOURCE_GENERATOR TBZ2 TGZ TXZ ZIP)
list(APPEND CPACK_SOURCE_IGNORE_FILES /.git/ /.github/ /.vscode/ /.vs/ /build/ .gitignore .DS_Store)

if(ADV_BUILD_DEB_PACKAGE)
    list(APPEND CPACK_GENERATOR "DEB")
endif()

if(ADV_BUILD_RPM_PACKAGE)
    list(APPEND CPACK_GENERATOR "RPM")
endif()

if(ADV_BUILD_MSI_INSTALLER)
    list(APPEND CPACK_GENERATOR "WIX")
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Vladimir Shaleev")
set(CPACK_PACKAGE_CONTACT "vladimirshaleev@gmail.com")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)

set(CPACK_DEBIAN_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Vladimir Shaleev <vladimirshaleev@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
if(ADV_USE_FMT)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfmt-dev")
endif()

if(ADV_BUILD_DEB_PACKAGE)
    configure_file(
        ${PROJECT_SOURCE_DIR}/cmake/resources/copyright.in
        ${CMAKE_BINARY_DIR}/copyright
        @ONLY
    )
    install(
        FILES "${CMAKE_BINARY_DIR}/copyright"
        DESTINATION "share/doc/${PROJECT_NAME}"
    )
endif()

set(CPACK_RPM_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_RPM_PACKAGE_LICENSE "MIT License")
set(CPACK_RPM_COMPRESSION_TYPE lzma)
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST
    /etc
    /etc/init.d
    /usr
    /usr/bin
    /usr/include
    /usr/lib
    /usr/libx32
    /usr/lib64
    /usr/share
    /usr/share/aclocal
    /usr/share/doc
    /usr/share/cmake
    /usr/share/pkgconfig
)

if(ADV_BUILD_MSI_INSTALLER)
    set(CPACK_WIX_VERSION 4)
    set(CPACK_WIX_UPGRADE_GUID "0ED64B29-8343-496D-B44A-D46D83C5D9E4")
    set(CPACK_WIX_CULTURES "en-US;de-DE;ja-JP;it-IT;pt-BR;zh-CN;zh-TW;ru-RU")
    set(CPACK_WIX_LICENSE_RTF "${PROJECT_SOURCE_DIR}/cmake/resources/LICENSE.rtf")
    set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/src/advc/icon.ico")
    set(CPACK_WIX_UI_BANNER "${PROJECT_SOURCE_DIR}/cmake/resources/banner.png")
    set(CPACK_WIX_UI_DIALOG "${PROJECT_SOURCE_DIR}/cmake/resources/dialog.png")
endif()

include(CPack)
