option(DISABLE_PAR "Disable parallel processing" OFF)
if(NOT MSVC AND NOT DISABLE_PAR)
        # For other compilers, check for TBB
        find_package(TBB QUIET)
        if(TBB_FOUND)
            message(STATUS "TBB found. Enabling parallel execution.")    
        else()
            message(STATUS "TBB not found. Disabling parallel execution.")
            set(DISABLE_PAR ON)
        endif()
elseif(DISABLE_PAR)
        message(STATUS "DISABLE_PAR set. Disabling parallel execution.")
endif()


# Function to enable parallel execution on non msvc
function(enable_parallel_execution target)
    if(DISABLE_PAR)
        target_compile_definitions(${target} PRIVATE CTRACK_DISABLE_EXECUTION_POLICY)
    elseif(NOT MSVC)
       target_link_libraries(${target} PRIVATE TBB::tbb)
    endif()
endfunction()

# Create executables for each example
add_executable(basic_singlethreaded basic_singlethreaded.cpp)
add_executable(multithreaded_prime_counter multithreaded_prime_counter.cpp)
add_executable(ctrack_overhead_test ctrack_overhead_test.cpp)
add_executable(high_variance_pi_estimation high_variance_pi_estimation.cpp)
add_executable(complex_multithreaded_puzzle complex_multithreaded_puzzle.cpp)

#check for paralell 
enable_parallel_execution(basic_singlethreaded)
enable_parallel_execution(multithreaded_prime_counter)
enable_parallel_execution(ctrack_overhead_test)
enable_parallel_execution(high_variance_pi_estimation)
enable_parallel_execution(complex_multithreaded_puzzle)

# Link the ctrack library to each example
target_link_libraries(basic_singlethreaded PRIVATE ctrack)
target_link_libraries(multithreaded_prime_counter PRIVATE ctrack)
target_link_libraries(ctrack_overhead_test PRIVATE ctrack)
target_link_libraries(high_variance_pi_estimation PRIVATE ctrack)
target_link_libraries(complex_multithreaded_puzzle PRIVATE ctrack)