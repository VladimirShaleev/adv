set(ADVC_BOOTSTRAP_EXECUTABLE "")
if(DEFINED CURRENT_HOST_INSTALLED_DIR)
    find_program(ADVC_BOOTSTRAP_EXECUTABLE NAMES advc PATHS "${CURRENT_HOST_INSTALLED_DIR}/tools/adv" NO_DEFAULT_PATH)
endif()
if(NOT ADVC_BOOTSTRAP_EXECUTABLE)
    if(DEFINED VCPKG_INSTALLED_DIR)
        file(GLOB _vcpkg_adv_candidates 
            "${VCPKG_INSTALLED_DIR}/*/tools/adv/advc"
            "${VCPKG_INSTALLED_DIR}/*/tools/adv/advc.exe")
        if(_vcpkg_adv_candidates)
            list(GET _vcpkg_adv_candidates 0 ADVC_BOOTSTRAP_EXECUTABLE)
        endif()
    endif()
endif()
if(NOT ADVC_BOOTSTRAP_EXECUTABLE OR ADV_BUILD_ADV)
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
    list(APPEND ADV_SOURCES ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
endif()
if(NOT ADVC_BOOTSTRAP_EXECUTABLE)
    if(ADV_BOOTSTRAP)
        message(STATUS "Host advc not found in vcpkg directories. Falling back to advc-bootstrap.")
        add_executable(advc-bootstrap ${ADV_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/src/advc/main.cpp")
        set_default_properties(TARGET advc-bootstrap)
        target_precompile_headers(advc-bootstrap PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/compiler.hpp")
        target_link_libraries(advc-bootstrap PRIVATE utils)
        target_link_libraries(advc-bootstrap PRIVATE $<$<BOOL:${ADV_USE_FMT}>:fmt::fmt>)
        target_link_libraries(advc-bootstrap PRIVATE argparse::argparse)
        target_include_directories(advc-bootstrap PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/adv/")
        target_include_directories(advc-bootstrap PRIVATE $<TARGET_PROPERTY:utils,INCLUDE_DIRECTORIES>)
        target_include_directories(advc-bootstrap PRIVATE "$<$<BOOL:${FLEX_INCLUDE_DIR}>:${FLEX_INCLUDE_DIR}>")
        target_include_directories(advc-bootstrap PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        set(ADVC_BOOTSTRAP_EXECUTABLE advc-bootstrap)
    else()
        message(STATUS "ADV_BOOTSTRAP is OFF. Using pre-generated std.")
    endif()
endif()
if(ADVC_BOOTSTRAP_EXECUTABLE)
    set(STD_HEADERS)
    file(GLOB STD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std/*.adv")
    foreach(SOURCE ${STD_SOURCES})
        get_filename_component(FILENAME "${SOURCE}" NAME_WE)
        set(MODULE_NAME ${FILENAME})
        set(HEADER_NAME ${FILENAME})
        string(APPEND MODULE_NAME ".adb")
        string(APPEND HEADER_NAME ".hpp")
        cmake_path(APPEND STD_BYTECODE_FILE "${CMAKE_CURRENT_BINARY_DIR}" "${MODULE_NAME}")
        cmake_path(APPEND STD_HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/std" "${HEADER_NAME}")
        list(APPEND STD_HEADERS ${STD_HEADER_FILE})
        add_custom_command(
            OUTPUT ${STD_HEADER_FILE}
            COMMAND ${ADVC_BOOTSTRAP_EXECUTABLE} -i "${CMAKE_CURRENT_SOURCE_DIR}/std/" -w -o ${STD_BYTECODE_FILE} ${SOURCE}
            COMMAND ${CMAKE_COMMAND} 
                -DINPUT_FILE="${STD_BYTECODE_FILE}"
                -DOUTPUT_FILE="${STD_HEADER_FILE}"
                -DARRAY_NAME="${FILENAME}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin-to-header.cmake"
            VERBATIM
            DEPENDS
                ${SOURCE}
                ${STD_SOURCES}
                ${ADVC_BOOTSTRAP_EXECUTABLE}
            COMMENT "[bootstrap] Building ${MODULE_NAME}")
    endforeach()
    set_source_files_properties(${STD_HEADERS} PROPERTIES GENERATED TRUE)
else()
    file(GLOB STD_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std/*.hpp")
endif()
