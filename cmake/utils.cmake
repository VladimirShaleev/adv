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
        if(ADVENTURE_MSVC_DYNAMIC_RUNTIME)
            set_target_properties(${ARG_TARGET} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        else()
            set_target_properties(${ARG_TARGET} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endif()
    if(NOT BUILD_SHARED_LIBS AND ARG_STATIC)
        target_compile_definitions(${ARG_TARGET} PUBLIC ${ARG_STATIC})
    endif()
endfunction()
