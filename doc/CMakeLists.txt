find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(STATUS "No Doxygen found. Documentation won't be built")
    return()
endif()

file(GLOB ADV_DOC_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/vm/*"
    "${PROJECT_SOURCE_DIR}/include/adv/*")

doxygen_add_docs(html
    "${CMAKE_CURRENT_LIST_DIR}/groups.h"
    ${ADV_DOC_SOURCES}
    # ${ADV_DOC_MARKDOWN}
    USE_STAMP_FILE)

if(ADV_ENABLE_INSTALL)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html" DESTINATION doc COMPONENT doc)
endif()
