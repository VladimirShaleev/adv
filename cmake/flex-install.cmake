find_package(FLEX 2.6.4)
if(NOT FLEX_INCLUDE_DIR)
    unset(FLEX_FOUND)
endif()
if(NOT FLEX_FOUND)
    include(ExternalProject)
    if(CMAKE_HOST_WIN32 OR WIN32 OR DEFINED ENV{SystemRoot})
        set(WINFLEXBISON_VERSION_INSTALL "2.5.24")
        message(STATUS "flex not found, try download flex from winflexbison ${WINFLEXBISON_VERSION_INSTALL}")

        set(WINFLEX_URL "https://github.com/lexxmark/winflexbison/releases/download/v${WINFLEXBISON_VERSION_INSTALL}/win_flex_bison-${WINFLEXBISON_VERSION_INSTALL}.zip")
        set(WINFLEX_INSTALL_DIR "${CMAKE_BINARY_DIR}/winflex")
        set(FLEX_EXECUTABLE "${WINFLEX_INSTALL_DIR}/win_flex.exe")
        set(FLEX_INCLUDE_DIR "${WINFLEX_INSTALL_DIR}")
        
        ExternalProject_Add(
            winflex
            URL ${WINFLEX_URL}
            URL_HASH MD5=6b549d43e34ece0e8ed05af92daa31c4
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND 
                ${CMAKE_COMMAND} -E copy_directory
                <SOURCE_DIR>/
                ${WINFLEX_INSTALL_DIR}
            BUILD_BYPRODUCTS ${FLEX_EXECUTABLE})
        
        add_custom_target(project_flex DEPENDS winflex)
    else()
        set(FLEX_VERSION_INSTALL "2.6.4")
        message(STATUS "flex not found, try download and build flex ${FLEX_VERSION_INSTALL}")
        include(cmake/m4-install.cmake)

        set(FLEX_URL "https://github.com/westes/flex/releases/download/v${FLEX_VERSION_INSTALL}/flex-${FLEX_VERSION_INSTALL}.tar.gz")
        set(FLEX_INSTALL_DIR "${CMAKE_BINARY_DIR}/flex_install")
        set(FLEX_EXECUTABLE "${FLEX_INSTALL_DIR}/bin/flex")
        set(FLEX_INCLUDE_DIR "${FLEX_INSTALL_DIR}/include")

        if(POLICY CMP0135)  
            cmake_policy(SET CMP0135 OLD)  
        endif()

        include(ProcessorCount)
        ProcessorCount(NPROC)
        if(NPROC EQUAL 0)
            set(NPROC 1)
        endif()

        ExternalProject_Add(
            flex_build
            URL ${FLEX_URL}
            URL_HASH MD5=2882e3179748cc9f9c23ec593d6adc8d
            DEPENDS ${M4_DEPENDS}
            CONFIGURE_COMMAND <SOURCE_DIR>/configure
                --prefix=${FLEX_INSTALL_DIR}
                --disable-bootstrap
                --disable-libfl
                "CFLAGS=-O3"
                "M4=${M4_EXECUTABLE}"
            BUILD_COMMAND make -j${NPROC}
            INSTALL_COMMAND make install
            BUILD_IN_SOURCE 1
            BUILD_BYPRODUCTS ${FLEX_EXECUTABLE}
            STEP_TARGETS build install)
            
        add_custom_target(project_flex DEPENDS flex_build-install)
    endif()
endif()

macro(FLEX_TARGET Name Input Output)
    set(FLEX_TARGET_PARAM_OPTIONS)
    set(FLEX_TARGET_PARAM_ONE_VALUE_KEYWORDS COMPILE_FLAGS DEFINES_FILE)
    set(FLEX_TARGET_PARAM_MULTI_VALUE_KEYWORDS)
    cmake_parse_arguments(
        FLEX_TARGET_ARG
        "${FLEX_TARGET_PARAM_OPTIONS}"
        "${FLEX_TARGET_PARAM_ONE_VALUE_KEYWORDS}"
        "${FLEX_TARGET_MULTI_VALUE_KEYWORDS}"
        ${ARGN})
    set(FLEX_TARGET_usage "FLEX_TARGET(<Name> <Input> <Output> [COMPILE_FLAGS <string>] [DEFINES_FILE <string>]")
    if(NOT "${FLEX_TARGET_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(SEND_ERROR ${FLEX_TARGET_usage})
    else()
        set(_flex_INPUT "${Input}")
        set(_flex_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}")
        if(NOT IS_ABSOLUTE "${_flex_INPUT}")
            set(_flex_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${_flex_INPUT}")
        endif()

        set(_flex_OUTPUT "${Output}")
        if(NOT IS_ABSOLUTE ${_flex_OUTPUT})
            set(_flex_OUTPUT "${_flex_WORKING_DIR}/${_flex_OUTPUT}")
        endif()
        set(_flex_TARGET_OUTPUTS "${_flex_OUTPUT}")

        set(_flex_EXE_OPTS "")
        if(NOT "${FLEX_TARGET_ARG_COMPILE_FLAGS}" STREQUAL "")
            set(_flex_EXE_OPTS "${FLEX_TARGET_ARG_COMPILE_FLAGS}")
            separate_arguments(_flex_EXE_OPTS)
        endif()

        set(_flex_OUTPUT_HEADER "")
        if(NOT "${FLEX_TARGET_ARG_DEFINES_FILE}" STREQUAL "")
            set(_flex_OUTPUT_HEADER "${FLEX_TARGET_ARG_DEFINES_FILE}")
            if(IS_ABSOLUTE "${_flex_OUTPUT_HEADER}")
            set(_flex_OUTPUT_HEADER_ABS "${_flex_OUTPUT_HEADER}")
            else()
            set(_flex_OUTPUT_HEADER_ABS "${_flex_WORKING_DIR}/${_flex_OUTPUT_HEADER}")
            endif()
            list(APPEND _flex_TARGET_OUTPUTS "${_flex_OUTPUT_HEADER_ABS}")
            list(APPEND _flex_EXE_OPTS --header-file=${_flex_OUTPUT_HEADER_ABS})
        endif()

        get_filename_component(_flex_EXE_NAME_WE "${FLEX_EXECUTABLE}" NAME_WE)
        set(_flex_DEPS "${_flex_INPUT}")
        if(TARGET project_flex)
            set(_flex_DEPS ${_flex_DEPS} project_flex)
        endif()
        add_custom_command(OUTPUT ${_flex_TARGET_OUTPUTS}
            COMMAND ${FLEX_EXECUTABLE} ${_flex_EXE_OPTS} -o${_flex_OUTPUT} ${_flex_INPUT}
            VERBATIM
            DEPENDS ${_flex_DEPS}
            COMMENT "[FLEX][${Name}] Building scanner with ${_flex_EXE_NAME_WE} ${FLEX_VERSION}"
            WORKING_DIRECTORY ${_flex_WORKING_DIR})

        set(FLEX_${Name}_DEFINED TRUE)
        set(FLEX_${Name}_OUTPUTS ${_flex_TARGET_OUTPUTS})
        set(FLEX_${Name}_INPUT ${_flex_INPUT})
        set(FLEX_${Name}_COMPILE_FLAGS ${_flex_EXE_OPTS})
        set(FLEX_${Name}_OUTPUT_HEADER ${_flex_OUTPUT_HEADER})
        unset(_flex_EXE_NAME_WE)
        unset(_flex_EXE_OPTS)
        unset(_flex_DEPS)
        unset(_flex_INPUT)
        unset(_flex_OUTPUT)
        unset(_flex_OUTPUT_HEADER)
        unset(_flex_OUTPUT_HEADER_ABS)
        unset(_flex_TARGET_OUTPUTS)
        unset(_flex_WORKING_DIR)
    endif()
endmacro()

macro(ADD_FLEX_BISON_DEPENDENCY FlexTarget BisonTarget)
    if(NOT FLEX_${FlexTarget}_OUTPUTS)
        message(SEND_ERROR "Flex target `${FlexTarget}' does not exist.")
    endif()
    if(NOT BISON_${BisonTarget}_OUTPUT_HEADER)
        message(SEND_ERROR "Bison target `${BisonTarget}' does not exist.")
    endif()
    set_source_files_properties(${FLEX_${FlexTarget}_OUTPUTS}
        PROPERTIES OBJECT_DEPENDS ${BISON_${BisonTarget}_OUTPUT_HEADER})
endmacro()
