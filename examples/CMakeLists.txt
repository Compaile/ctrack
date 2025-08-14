
# Create executables for each example
add_executable(basic_singlethreaded basic_singlethreaded.cpp)
add_executable(multithreaded_prime_counter multithreaded_prime_counter.cpp)
add_executable(ctrack_overhead_test ctrack_overhead_test.cpp)
add_executable(high_variance_pi_estimation high_variance_pi_estimation.cpp)
add_executable(complex_multithreaded_puzzle complex_multithreaded_puzzle.cpp)

# Link the ctrack library to each example
target_link_libraries(basic_singlethreaded PRIVATE ctrack)
target_link_libraries(multithreaded_prime_counter PRIVATE ctrack)
target_link_libraries(ctrack_overhead_test PRIVATE ctrack)
target_link_libraries(high_variance_pi_estimation PRIVATE ctrack)
target_link_libraries(complex_multithreaded_puzzle PRIVATE ctrack)