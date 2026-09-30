find_package(BISON 3.7.4)
if(NOT BISON_FOUND)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    include(ExternalProject)
    if(CMAKE_HOST_WIN32 OR WIN32 OR DEFINED ENV{SystemRoot})
        set(WINFLEXBISON_VERSION_INSTALL "2.5.24")
        message(STATUS "bison not found, try download bison from winflexbison ${WINFLEXBISON_VERSION_INSTALL}")

        set(WINBISON_URL "https://github.com/lexxmark/winflexbison/releases/download/v${WINFLEXBISON_VERSION_INSTALL}/win_flex_bison-${WINFLEXBISON_VERSION_INSTALL}.zip")
        set(WINBISON_INSTALL_DIR "${CMAKE_BINARY_DIR}/winbison")
        set(BISON_EXECUTABLE "${WINBISON_INSTALL_DIR}/win_bison.exe")

        ExternalProject_Add(
            winbison
            URL ${WINBISON_URL}
            URL_HASH MD5=6b549d43e34ece0e8ed05af92daa31c4
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND 
                ${CMAKE_COMMAND} -E copy_directory
                <SOURCE_DIR>/
                ${WINBISON_INSTALL_DIR}
            BUILD_BYPRODUCTS ${BISON_EXECUTABLE})
        
        add_custom_target(project_bison DEPENDS winbison)
    else()
        set(BISON_VERSION_INSTALL "3.8.2")
        message(STATUS "bison not found, try download and build bison ${BISON_VERSION_INSTALL}")
        include(cmake/m4-install.cmake)

        set(BISON_URL "https://ftp.gnu.org/gnu/bison/bison-${BISON_VERSION_INSTALL}.tar.gz")
        set(BISON_INSTALL_DIR "${CMAKE_BINARY_DIR}/bison_install")
        set(BISON_EXECUTABLE "${BISON_INSTALL_DIR}/bin/bison")

        set(BUILD_TRIPLET "${CMAKE_HOST_SYSTEM_PROCESSOR}")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(BUILD_TRIPLET "x86_64-linux-gnu")
            elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
                set(BUILD_TRIPLET "aarch64-linux-gnu")
            endif()
        elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
            if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(BUILD_TRIPLET "x86_64-apple-darwin")
            elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
                set(BUILD_TRIPLET "aarch64-apple-darwin")
            endif()
        endif()
        set(CONFIGURE_OPTS --build=${BUILD_TRIPLET})
        
        if(POLICY CMP0135)  
            cmake_policy(SET CMP0135 OLD)  
        endif()

        ExternalProject_Add(
            bison_build
            URL ${BISON_URL}
            URL_HASH MD5=1e541a097cda9eca675d29dd2832921f
            DEPENDS ${M4_DEPENDS}
            CONFIGURE_COMMAND <SOURCE_DIR>/configure
                --prefix=${BISON_INSTALL_DIR}
                "CFLAGS=-O3"
                "M4=${M4_EXECUTABLE}"
                ${CONFIGURE_OPTS}
            BUILD_COMMAND make -j${NPROC}
            INSTALL_COMMAND make install
            BUILD_IN_SOURCE 1
            BUILD_BYPRODUCTS ${BISON_EXECUTABLE}
            STEP_TARGETS build install)

        add_custom_target(project_bison DEPENDS bison_build-install)
    endif()

    macro(BISON_TARGET_option_report_file BisonOutput ReportFile)
        if("${ReportFile}" STREQUAL "")
            get_filename_component(BISON_TARGET_output_path "${BisonOutput}" PATH)
            get_filename_component(BISON_TARGET_output_name "${BisonOutput}" NAME_WE)
            set(BISON_TARGET_verbose_file "${BISON_TARGET_output_path}/${BISON_TARGET_output_name}.output")
        else()
            set(BISON_TARGET_verbose_file "${ReportFile}")
            list(APPEND BISON_TARGET_cmdopt "--report-file=${BISON_TARGET_verbose_file}")
        endif()
        if(NOT IS_ABSOLUTE "${BISON_TARGET_verbose_file}")
            set(BISON_TARGET_verbose_file "${CMAKE_CURRENT_BINARY_DIR}/${BISON_TARGET_verbose_file}")
        endif()
    endmacro()

    macro(BISON_TARGET_option_defines BisonOutput Header)
        if("${Header}" STREQUAL "")
            string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\2" _fileext "${BisonOutput}")
            string(REPLACE "c" "h" _fileext ${_fileext})
            string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1${_fileext}" BISON_TARGET_output_header "${BisonOutput}")
            list(APPEND BISON_TARGET_cmdopt "-d")
        else()
            set(BISON_TARGET_output_header "${Header}")
            list(APPEND BISON_TARGET_cmdopt "--defines=${BISON_TARGET_output_header}")
        endif()
    endmacro()

    macro(BISON_TARGET_option_extraopts Options)
        set(BISON_TARGET_cmdopt "")
        set(BISON_TARGET_extraopts "${Options}")
        separate_arguments(BISON_TARGET_extraopts)
        list(APPEND BISON_TARGET_cmdopt ${BISON_TARGET_extraopts})
    endmacro()

    macro(BISON_TARGET Name BisonInput BisonOutput)
        set(BISON_TARGET_outputs "${BisonOutput}")
        set(BISON_TARGET_extraoutputs "")
        set(BISON_TARGET_PARAM_OPTIONS)
        set(BISON_TARGET_PARAM_ONE_VALUE_KEYWORDS COMPILE_FLAGS DEFINES_FILE REPORT_FILE)
        set(BISON_TARGET_PARAM_MULTI_VALUE_KEYWORDS VERBOSE)
        cmake_parse_arguments(
            BISON_TARGET_ARG
            "${BISON_TARGET_PARAM_OPTIONS}"
            "${BISON_TARGET_PARAM_ONE_VALUE_KEYWORDS}"
            "${BISON_TARGET_PARAM_MULTI_VALUE_KEYWORDS}"
            ${ARGN})
        if(NOT "${BISON_TARGET_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
            message(SEND_ERROR "Usage")
        elseif("${BISON_TARGET_ARG_VERBOSE}" MATCHES ";")
            message(SEND_ERROR "Usage")
        else()
            BISON_TARGET_option_extraopts("${BISON_TARGET_ARG_COMPILE_FLAGS}")
            BISON_TARGET_option_defines("${BisonOutput}" "${BISON_TARGET_ARG_DEFINES_FILE}")
            BISON_TARGET_option_report_file("${BisonOutput}" "${BISON_TARGET_ARG_REPORT_FILE}")
            if(NOT "${BISON_TARGET_ARG_VERBOSE}" STREQUAL "")
                BISON_TARGET_option_verbose(${Name} ${BisonOutput} "${BISON_TARGET_ARG_VERBOSE}")
            else()
            set(BISON_TARGET_args "${ARGN}")
            list(FIND BISON_TARGET_args "VERBOSE" BISON_TARGET_args_indexof_verbose)
            if(${BISON_TARGET_args_indexof_verbose} GREATER -1)
                BISON_TARGET_option_verbose(${Name} ${BisonOutput} "")
            endif()
        endif()
            list(APPEND BISON_TARGET_outputs "${BISON_TARGET_output_header}")
            set(_BisonInput "${BisonInput}")
            set(_BISON_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
            if(NOT IS_ABSOLUTE "${_BisonInput}")
                set(_BisonInput "${CMAKE_CURRENT_SOURCE_DIR}/${_BisonInput}")
            endif()
            add_custom_command(OUTPUT ${BISON_TARGET_outputs}
                COMMAND ${BISON_EXECUTABLE} ${BISON_TARGET_cmdopt} -o ${BisonOutput} ${_BisonInput}
                VERBATIM
                DEPENDS ${_BisonInput} project_bison
                COMMENT "[BISON][${Name}] Building parser with bison ${BISON_VERSION}"
                WORKING_DIRECTORY ${_BISON_WORKING_DIRECTORY})
            unset(_BISON_WORKING_DIRECTORY)
            set(BISON_${Name}_DEFINED TRUE)
            set(BISON_${Name}_INPUT ${_BisonInput})
            set(BISON_${Name}_OUTPUTS ${BISON_TARGET_outputs} ${BISON_TARGET_extraoutputs})
            set(BISON_${Name}_COMPILE_FLAGS ${BISON_TARGET_cmdopt})
            set(BISON_${Name}_OUTPUT_SOURCE "${BisonOutput}")
            set(BISON_${Name}_OUTPUT_HEADER "${BISON_TARGET_output_header}")
            unset(_BisonInput)
        endif()
    endmacro()
endif()
