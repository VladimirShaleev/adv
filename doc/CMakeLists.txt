find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(STATUS "No Doxygen found. Documentation won't be built")
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    doxygen-awesome-css
    GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
    GIT_TAG 46483f1e5a70ffb9ecd3b82d0a1cd1b24edf13da) # v2.5.0
FetchContent_MakeAvailable(doxygen-awesome-css)
set(DOXYGEN_CSS "${doxygen-awesome-css_SOURCE_DIR}" CACHE STRING "doxygen-awesome")
FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR DOXYGEN_CSS)

file(GLOB ADV_DOC_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/vm/*"
    "${PROJECT_SOURCE_DIR}/include/adv/*")
set(ADV_DOC_MARKDOWN
    "${CMAKE_CURRENT_LIST_DIR}/introduction.md")

set(DOXYGEN_PROJECT_LOGO) # TODO
set(DOXYGEN_PROJECT_ICON) # TODO
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(DOXYGEN_INLINE_INHERITED_MEMB YES)
set(DOXYGEN_STRIP_FROM_PATH "${PROJECT_SOURCE_DIR}/include/")
set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
set(DOXYGEN_QT_AUTOBRIEF YES)
set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
set(DOXYGEN_HIDE_UNDOC_MEMBERS YES)
set(DOXYGEN_HIDE_UNDOC_CLASSES YES)
set(DOXYGEN_HIDE_FRIEND_COMPOUNDS YES)
set(DOXYGEN_CASE_SENSE_NAMES YES)
set(DOXYGEN_HIDE_SCOPE_NAMES YES)
set(DOXYGEN_SORT_MEMBER_DOCS YES)
set(DOXYGEN_LAYOUT_FILE "${CMAKE_CURRENT_LIST_DIR}/layout.xml")
set(DOXYGEN_WARN_NO_PARAMDOC YES)
set(DOXYGEN_WARN_IF_UNDOC_ENUM_VAL YES)
set(DOXYGEN_WARN_LOGFILE "${CMAKE_CURRENT_BINARY_DIR}/doxygen-log.txt")
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_LIST_DIR}/introduction.md")
set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
set(DOXYGEN_CLANG_OPTIONS "-I${PROJECT_SOURCE_DIR}/include")
set(DOXYGEN_HTML_HEADER "${CMAKE_CURRENT_LIST_DIR}/header.html")
set(DOXYGEN_HTML_FOOTER "${CMAKE_CURRENT_LIST_DIR}/footer.html")
set(DOXYGEN_HTML_EXTRA_STYLESHEET
    "${DOXYGEN_CSS}/doxygen-awesome.css"
    "${DOXYGEN_CSS}/doxygen-awesome-sidebar-only.css"
    "${DOXYGEN_CSS}/doxygen-awesome-sidebar-only-darkmode-toggle.css"
    "${CMAKE_CURRENT_LIST_DIR}/adv-doc.css")
set(DOXYGEN_HTML_EXTRA_FILES
    "${DOXYGEN_CSS}/doxygen-awesome-darkmode-toggle.js"
    "${DOXYGEN_CSS}/doxygen-awesome-fragment-copy-button.js"
    "${DOXYGEN_CSS}/doxygen-awesome-paragraph-link.js"
    "${DOXYGEN_CSS}/doxygen-awesome-interactive-toc.js"
    "${DOXYGEN_CSS}/doxygen-awesome-tabs.js"
    "${CMAKE_CURRENT_LIST_DIR}/adv-doc.js")
set(DOXYGEN_HTML_COLORSTYLE LIGHT)
set(DOXYGEN_HTML_COLORSTYLE_HUE 220)
set(DOXYGEN_HTML_COLORSTYLE_SAT 100)
set(DOXYGEN_HTML_COLORSTYLE_GAMMA 80)
set(DOXYGEN_SITEMAP_URL "https://vladimirshaleev.github.io/adv/")
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_ENUM_VALUES_PER_LINE 1)
set(DOXYGEN_TREEVIEW_WIDTH 335)
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_PREDEFINED DOXYGEN)
set(DOXYGEN_HAVE_DOT YES)
set(DOXYGEN_COLLABORATION_GRAPH NO)
set(DOXYGEN_TEMPLATE_RELATIONS YES)
set(DOXYGEN_INCLUDE_GRAPH NO)
set(DOXYGEN_INCLUDED_BY_GRAPH NO)
set(DOXYGEN_DIRECTORY_GRAPH NO)
set(DOXYGEN_DOT_IMAGE_FORMAT svg)
set(DOXYGEN_GENERATE_LEGEND YES)

doxygen_add_docs(html
    "${CMAKE_CURRENT_LIST_DIR}/vm-api.h"
    "${CMAKE_CURRENT_LIST_DIR}/adv-api.h"
    ${ADV_DOC_SOURCES}
    ${ADV_DOC_MARKDOWN}
    USE_STAMP_FILE)

# Remove unused sections in ToC
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixtoc.cmake"
    [=[
    file(READ "${SOURCE}" TEXT)
    string(REPLACE ",\n    [ \"Data Structures\", \"annotated.html\", [\n      [ \"Data Structures\", \"annotated.html\", \"annotated_dup\" ]\n    ] ],\n    [ \"Files\", \"files.html\", [\n      [ \"File List\", \"files.html\", \"files_dup\" ]\n    ] ]" "" TEXT "${TEXT}")
    file(WRITE "${TARGET}" "${TEXT}")
    ]=])
add_custom_command(
    OUTPUT navtreedata
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE=${CMAKE_CURRENT_BINARY_DIR}/html/navtreedata.js
      -DTARGET=${CMAKE_CURRENT_BINARY_DIR}/html/navtreedata.js
      -P ${CMAKE_CURRENT_BINARY_DIR}/fixtoc.cmake
    DEPENDS html "${CMAKE_CURRENT_BINARY_DIR}/fixtoc.cmake"
    VERBATIM)

# Remove unused sections in menu
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixmenu.cmake"
    [=[
    file(READ "${SOURCE}" TEXT)
    string(REPLACE ",\n{text:\"Data Structures\",url:\"annotated.html\",children:[\n{text:\"Data Structures\",url:\"annotated.html\"}]},\n{text:\"Files\",url:\"files.html\",children:[\n{text:\"File List\",url:\"files.html\"}]}" "" TEXT "${TEXT}")
    file(WRITE "${TARGET}" "${TEXT}")
    ]=])
add_custom_command(
    OUTPUT menudata
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_BINARY_DIR}/html/menudata.js
        -DTARGET=${CMAKE_CURRENT_BINARY_DIR}/html/menudata.js
        -P ${CMAKE_CURRENT_BINARY_DIR}/fixmenu.cmake
    DEPENDS html "${CMAKE_CURRENT_BINARY_DIR}/fixmenu.cmake"
    VERBATIM)

# Override search data
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixsearch.cmake"
    [=[
    set(TEXT "")
    string(APPEND TEXT "var indexSectionsWithContent =\n{\n  0: \"acdefhiklmnprstv\",\n  1: \"av\",\n  2: \"av\",\n  3: \"av\",\n  4: \"av\",\n  5: \"av\",\n  6: \"av\",\n  7: \"acefmstv\"\n};\n\n")
    string(APPEND TEXT "var indexSectionNames =\n{\n  0: \"all\",\n  1: \"classes\",\n  2: \"files\",\n  3: \"functions\",\n  4: \"typedefs\",\n  5: \"enums\",\n  6: \"enumvalues\",\n  7: \"groups\"\n};\n\n")
    string(APPEND TEXT "var indexSectionLabels =\n{\n  0: \"All\",\n  1: \"Structures\",\n  2: \"Files\",\n  3: \"Functions\",\n  4: \"Typedefs\",\n  5: \"Enumerations\",\n  6: \"Enumeration Values\",\n  7: \"APIs\"\n};\n")
    file(WRITE "${TARGET}" "${TEXT}")
    ]=])
add_custom_command(
    OUTPUT searchdata
    COMMAND ${CMAKE_COMMAND}
        -DTARGET=${CMAKE_CURRENT_BINARY_DIR}/html/search/searchdata.js
        -P ${CMAKE_CURRENT_BINARY_DIR}/fixsearch.cmake
    DEPENDS html "${CMAKE_CURRENT_BINARY_DIR}/fixsearch.cmake"
    VERBATIM)

add_custom_target(doc ALL DEPENDS html navtreedata menudata searchdata)

if(ADV_ENABLE_INSTALL)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html" DESTINATION doc COMPONENT doc)
endif()
