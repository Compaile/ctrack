# Taken from https://github.com/ClickHouse/ClickHouse/blob/master/cmake/warnings.cmake,
# slightly modified to fit into ctrack style.

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Control maximum size of stack frames. It can be important if the code is run in fibers with small stack size.
# Only in release build because debug has too large stack frames.
if ((NOT CMAKE_BUILD_TYPE_UC STREQUAL "DEBUG") AND (NOT SANITIZE) AND (NOT CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"))
    add_warning(frame-larger-than=65536)
endif ()

# Add some warnings that are not available even with -Wall -Wextra -Wpedantic.
# We want to get everything out of the compiler for code quality.
add_warning(everything)
add_warning(pedantic)
no_warning(zero-length-array)
no_warning(c++98-compat-pedantic)
no_warning(c++98-compat)
no_warning(c++20-compat) # Use constinit in C++20 without warnings
no_warning(sign-conversion)
no_warning(implicit-int-conversion)
no_warning(implicit-int-float-conversion)
no_warning(ctad-maybe-unsupported) # clang 9+, linux-only
no_warning(disabled-macro-expansion)
no_warning(documentation-unknown-command)
no_warning(double-promotion)
no_warning(exit-time-destructors)
no_warning(float-equal)
no_warning(global-constructors)
no_warning(missing-prototypes)
no_warning(missing-variable-declarations)
no_warning(padded)
no_warning(switch-enum)
no_warning(undefined-func-template)
no_warning(unused-template)
no_warning(vla)
no_warning(weak-template-vtables)
no_warning(weak-vtables)
no_warning(thread-safety-negative) # experimental flag, too many false positives
no_warning(enum-constexpr-conversion) # breaks magic-enum library in clang-16
no_warning(unsafe-buffer-usage) # too aggressive
no_warning(switch-default) # conflicts with "defaults in a switch covering all enum values"

# Styling decisions for ctrack
no_warning(shadow-field-in-constructor)
no_warning(newline-eof)