add_executable(ctrack_benchmark ctrack_benchmark.cpp)
target_link_libraries(ctrack_benchmark PRIVATE ctrack)

# Enable threading support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(ctrack_benchmark PRIVATE Threads::Threads)

# Add filesystem library if needed (for older compilers)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(ctrack_benchmark PRIVATE stdc++fs)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(ctrack_benchmark PRIVATE c++fs)
    endif()
endif()

# Set output directory
set_target_properties(ctrack_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark"
)