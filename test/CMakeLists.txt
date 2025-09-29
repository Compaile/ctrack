# Test configuration for ctrack
cmake_minimum_required(VERSION 3.12)

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Common test compilation flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define test executables
set(TEST_SOURCES
    test_basic.cpp
    test_multithreaded.cpp
    test_nested.cpp
    test_statistics.cpp
    test_results.cpp
    test_edge_cases.cpp
)

# Create test executables and register with CTest
foreach(test_source ${TEST_SOURCES})
    # Get test name from filename
    string(REPLACE ".cpp" "" test_name ${test_source})
    
    # Create executable
    add_executable(${test_name} ${test_source})
    
    # Link with ctrack
    target_link_libraries(${test_name} ctrack)
    
    # Add pthread support for multithreading tests
    if(NOT WIN32)
        target_link_libraries(${test_name} pthread)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Set test properties for better output
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "FAILED"
    )
endforeach()

# Add a custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all ctrack tests"
)