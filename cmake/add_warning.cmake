# Taken from https://github.com/ClickHouse/ClickHouse/blob/master/cmake/add_warnings.cmake

include (CheckCXXCompilerFlag)

# Try to add -Wflag if compiler supports it
macro (add_warning flag)
    string (REPLACE "-" "_" underscored_flag ${flag})
    string (REPLACE "+" "x" underscored_flag ${underscored_flag})

    check_cxx_compiler_flag("-W${flag}" SUPPORTS_CXXFLAG_${underscored_flag})

    if (SUPPORTS_CXXFLAG_${underscored_flag})
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W${flag}")
    else ()
        message (STATUS "Flag -W${flag} is unsupported")
    endif ()

endmacro ()

# Try to add -Wno flag if compiler supports it
macro (no_warning flag)
    add_warning(no-${flag})
endmacro ()


# The same but only for specified target.
macro (target_add_warning target flag)
    string (REPLACE "-" "_" underscored_flag ${flag})
    string (REPLACE "+" "x" underscored_flag ${underscored_flag})

    check_cxx_compiler_flag("-W${flag}" SUPPORTS_CXXFLAG_${underscored_flag})

    if (SUPPORTS_CXXFLAG_${underscored_flag})
        target_compile_options (${target} PRIVATE "-W${flag}")
    else ()
        message (STATUS "Flag -W${flag} is unsupported")
    endif ()
endmacro ()

macro (target_no_warning target flag)
    target_add_warning(${target} no-${flag})
endmacro ()
