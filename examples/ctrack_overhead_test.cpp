#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "ctrack.hpp"

void empty_function() {
    CTRACK;
    // Function is intentionally empty
}

void run_empty_functions(int count) {
    for (int i = 0; i < count; ++i) {
        empty_function();
    }
}

int main() {
    const int total_calls = 10000000; // 10 million calls in total
    const int thread_count = std::thread::hardware_concurrency();
    const int calls_per_thread = total_calls / thread_count;

    std::cout << "Running performance test with " << thread_count << " threads." << std::endl;
    std::cout << "Total function calls: " << total_calls << std::endl;

    std::vector<std::thread> threads;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(run_empty_functions, calls_per_thread);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Total execution time: " << duration.count() << " milliseconds" << std::endl;

    ctrack::result_print();

    return 0;
}