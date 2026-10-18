function(set_default_properties)
    set(one_value TARGET STATIC)
    cmake_parse_arguments(ARG "" "${one_value}" "" ${ARGV})
    target_compile_features(${ARG_TARGET} PRIVATE cxx_std_20)
    set_target_properties(${ARG_TARGET} PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set_target_properties(${ARG_TARGET} PROPERTIES 
        VERSION ${PROJECT_VERSION} 
        SOVERSION ${PROJECT_VERSION_MAJOR})
    set_target_properties(${ARG_TARGET} PROPERTIES
        CXX_VISIBILITY_PRESET hidden 
        VISIBILITY_INLINES_HIDDEN ON)
    if(MSVC)
        target_compile_options(${ARG_TARGET} PRIVATE /Zc:preprocessor)
    endif()
    if(WIN32)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            if(DEFINED ADV_MSVC_DYNAMIC_RUNTIME AND ADV_MSVC_DYNAMIC_RUNTIME)
                set_target_properties(${ARG_TARGET} PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            else()
                set_target_properties(${ARG_TARGET} PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            endif()
        endif()
    endif()
    if(NOT BUILD_SHARED_LIBS AND ARG_STATIC)
        target_compile_definitions(${ARG_TARGET} PUBLIC ${ARG_STATIC})
    endif()
endfunction()

function(add_emscripten)
    set(one_value TARGET EXPORT_NAME PACKAGE_NAME DIST DESCRIPTION)
    set(multi_value SOURCES DEPS)
    cmake_parse_arguments(ARG "" "${one_value}" "${multi_value}" ${ARGV})
    set(UTILS_PACKAGE_NAME "${ARG_PACKAGE_NAME}")
    set(UTILS_TARGET "${ARG_TARGET}")
    set(UTILS_JS_DESCRIPTION "${ARG_DESCRIPTION}")
    set(UTILS_JS_OUTPUT "${PROJECT_BINARY_DIR}/${ARG_DIST}")
    file(MAKE_DIRECTORY "${UTILS_JS_OUTPUT}")
    set(UTILS_JS_LINK_OPTIONS
        -sWASM=1
        -sMODULARIZE=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORT_NAME=${ARG_EXPORT_NAME}
        -fwasm-exceptions
        --bind)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(UTILS_JS_WASM "\"${ARG_TARGET}.wasm\",\n    \"${ARG_TARGET}.esm.wasm\",\n    ")
        list(APPEND UTILS_JS_LINK_OPTIONS "-sSINGLE_FILE=0")
    else()
        list(APPEND UTILS_JS_LINK_OPTIONS "-sSINGLE_FILE=1")
    endif()
    foreach(UTILS_JS_TARGET IN ITEMS ${ARG_TARGET} ${ARG_TARGET}-esm)
        add_executable(${UTILS_JS_TARGET} ${ARG_SOURCES})
        target_link_libraries(${UTILS_JS_TARGET} PRIVATE ${ARG_DEPS})
        set_default_properties(TARGET ${UTILS_JS_TARGET})
        target_compile_options(${UTILS_JS_TARGET} PRIVATE -fwasm-exceptions)
        target_link_options(${UTILS_JS_TARGET} PRIVATE ${UTILS_JS_LINK_OPTIONS})
        set_target_properties(${UTILS_JS_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${UTILS_JS_OUTPUT}")
    endforeach()
    target_link_options(${ARG_TARGET} PRIVATE --emit-tsd "${UTILS_JS_OUTPUT}/${ARG_TARGET}.d.ts")
    target_link_options(${ARG_TARGET}-esm PRIVATE -sEXPORT_ES6=1 --emit-tsd "${UTILS_JS_OUTPUT}/${ARG_TARGET}.esm.d.ts")
    set_target_properties(${ARG_TARGET} PROPERTIES OUTPUT_NAME "${ARG_TARGET}" SUFFIX ".js")
    set_target_properties(${ARG_TARGET}-esm PROPERTIES OUTPUT_NAME "${ARG_TARGET}.esm" SUFFIX ".mjs")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/package.json.in" "${UTILS_JS_OUTPUT}/package.json" @ONLY)
    configure_file("${PROJECT_SOURCE_DIR}/README.md" "${UTILS_JS_OUTPUT}/README.md" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/LICENSE" "${UTILS_JS_OUTPUT}/LICENSE" COPYONLY)
endfunction()
