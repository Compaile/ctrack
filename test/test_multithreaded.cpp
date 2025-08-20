#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <future>
#include <random>

using namespace test_helpers;

// Helper function for multi-threaded testing
void multithreaded_test_function(int sleep_time_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_time_ms);
}

// Named function for testing

void named_test_function_1(int sleep_time_ms) {
    CTRACK_NAME("named_test_function_1");
    test_helpers::sleep_ms(sleep_time_ms);
}

void named_test_function_2(int sleep_time_ms) {
    CTRACK_NAME("named_test_function_2");
    test_helpers::sleep_ms(sleep_time_ms);
}

void named_test_function_3(int sleep_time_ms) {
    CTRACK_NAME("named_test_function_3");
    test_helpers::sleep_ms(sleep_time_ms);
}




// Nested function for testing call hierarchy in threads
void nested_child(int sleep_time_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_time_ms);
}

void nested_parent(int child_sleep_ms, int parent_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(parent_sleep_ms);
    nested_child(child_sleep_ms);
}

TEST_CASE("Multiple threads same function") {
    clear_ctrack();
    const int num_threads = 4;
    const int calls_per_thread = 5;
    const int sleep_time_ms = 20;
    
    ThreadBarrier barrier(num_threads);
    std::vector<std::thread> threads;
    
    // Start threads synchronized
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&barrier, calls_per_thread, sleep_time_ms]() {
            barrier.wait(); // Synchronize start
            for (int j = 0; j < calls_per_thread; j++) {
                multithreaded_test_function(sleep_time_ms);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Validate results
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.function_name == "multithreaded_test_function");
    CHECK(stats.calls == num_threads * calls_per_thread);
    CHECK(stats.threads == num_threads);
    
    // Validate that time_acc represents total accumulated time across all threads
    auto expected_total_time = std::chrono::milliseconds(num_threads * calls_per_thread * sleep_time_ms);
    CHECK(within_tolerance(stats.time_acc, expected_total_time, 20.0));
    
    // Summary table should match
    REQUIRE(tables.summary.rows.size() == 1);
    const auto& summary = tables.summary.rows[0];
    CHECK(summary.function_name == "multithreaded_test_function");
    CHECK(summary.calls == num_threads * calls_per_thread);
}

TEST_CASE("Thread count tracking with different thread counts") {
    SUBCASE("2 threads") {
        clear_ctrack();
        const int num_threads = 2;
        const int sleep_time_ms = 10;
        
        ThreadBarrier barrier(num_threads);
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&barrier, sleep_time_ms]() {
                barrier.wait();
                multithreaded_test_function(sleep_time_ms);
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto tables = ctrack::result_get_tables();
        const auto& stats = tables.details.rows[0];
        CHECK(stats.threads == 2);
        CHECK(stats.calls == 2);
    }
    
    SUBCASE("8 threads") {
        clear_ctrack();
        const int num_threads = 8;
        const int sleep_time_ms = 5;
        
        ThreadBarrier barrier(num_threads);
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&barrier, sleep_time_ms]() {
                barrier.wait();
                multithreaded_test_function(sleep_time_ms);
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto tables = ctrack::result_get_tables();
        const auto& stats = tables.details.rows[0];
        CHECK(stats.threads == 8);
        CHECK(stats.calls == 8);
    }
}

TEST_CASE("Per-thread event isolation") {
    clear_ctrack();
    const int num_threads = 3;
    const int sleep_time_ms = 8;
    
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    
    // Each thread calls a uniquely named function
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, sleep_time_ms, &completed_threads]() {

            if((i+1) % 2 == 0) {
                named_test_function_1(sleep_time_ms);
            } else if ((i+1) % 3 == 0) {
                named_test_function_2(sleep_time_ms);
            } else {
                named_test_function_3(sleep_time_ms);
			}

            completed_threads.fetch_add(1);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    CHECK(completed_threads.load() == num_threads);
    
    auto tables = ctrack::result_get_tables();
    CHECK(tables.details.rows.size() == num_threads);
    
    // Each function should show up with 1 call and 1 thread
    for (const auto& stats : tables.details.rows) {
        CHECK(stats.calls == 1);
        CHECK(stats.threads == 1);
        CHECK(stats.function_name.substr(0, 20) == "named_test_function_");
        
        auto expected_time = std::chrono::milliseconds(sleep_time_ms);
        CHECK(within_tolerance(stats.time_acc, expected_time, 20.0));
    }
}

TEST_CASE("Concurrent access thread safety") {
    clear_ctrack();
    const int num_threads = 6;
    const int calls_per_thread = 20;
    const int sleep_time_ms = 5; // Short sleep to stress test timing
    
    std::vector<std::thread> threads;
    std::atomic<int> total_calls{0};
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([calls_per_thread, sleep_time_ms, &total_calls]() {
            for (int j = 0; j < calls_per_thread; j++) {
                multithreaded_test_function(sleep_time_ms);
                total_calls.fetch_add(1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    CHECK(total_calls.load() == num_threads * calls_per_thread);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == num_threads * calls_per_thread);
    CHECK(stats.threads == num_threads);
    
    // Verify accumulated time is reasonable
    auto min_expected_time = std::chrono::milliseconds(num_threads * calls_per_thread * sleep_time_ms);
    CHECK(stats.time_acc >= min_expected_time * 0.8); // Allow some tolerance for timing variations
}

TEST_CASE("Thread barrier synchronization scenarios") {
    clear_ctrack();
    const int num_threads = 4;
    const int sleep_time_ms = 100;
    
    ThreadBarrier start_barrier(num_threads);
    ThreadBarrier end_barrier(num_threads);
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&start_barrier, &end_barrier, sleep_time_ms]() {
            start_barrier.wait(); // All threads start simultaneously
            multithreaded_test_function(sleep_time_ms);
            end_barrier.wait(); // All threads finish simultaneously
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto wall_clock_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto tables = ctrack::result_get_tables();
    const auto& stats = tables.details.rows[0];
    
    CHECK(stats.calls == num_threads);
    CHECK(stats.threads == num_threads);
    
    // Since threads run in parallel, the wall clock time should be approximately
    // equal to the sleep time (not num_threads * sleep_time)
    CHECK(within_tolerance(wall_clock_time, std::chrono::milliseconds(sleep_time_ms), 50.0));
    
    // But accumulated time should be the sum of all thread times
    auto expected_acc_time = std::chrono::milliseconds(num_threads * sleep_time_ms);
    CHECK(within_tolerance(stats.time_acc, expected_acc_time, 20.0));
}

TEST_CASE("Mixed function calls across threads") {
    clear_ctrack();
    const int num_threads = 4;
    const int sleep_time_ms = 5;
    
    ThreadBarrier barrier(num_threads);
    std::vector<std::thread> threads;
    
    // Each thread calls both functions
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&barrier, sleep_time_ms, i]() {
            barrier.wait();
            
            // Call multithreaded_test_function
            multithreaded_test_function(sleep_time_ms);
            
 
            if(( i+1) % 2 == 0) {
                named_test_function_1(sleep_time_ms);
            } else {
                named_test_function_2(sleep_time_ms);
			}

        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto tables = ctrack::result_get_tables();
    CHECK(tables.details.rows.size() == 3); // multithreaded_test_function + 2 mixed_function variants
    
    // Find each function in the results
    const ctrack::detail_stats* multithreaded_stats = nullptr;
    const ctrack::detail_stats* mixed0_stats = nullptr;
    const ctrack::detail_stats* mixed1_stats = nullptr;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name == "multithreaded_test_function") {
            multithreaded_stats = &stats;
        } else if (stats.function_name == "named_test_function_1") {
            mixed0_stats = &stats;
        } else if (stats.function_name == "named_test_function_2") {
            mixed1_stats = &stats;
        }
    }
    
    REQUIRE(multithreaded_stats != nullptr);
    REQUIRE(mixed0_stats != nullptr);
    REQUIRE(mixed1_stats != nullptr);
    
    // multithreaded_test_function called by all 4 threads
    CHECK(multithreaded_stats->calls == 4);
    CHECK(multithreaded_stats->threads == 4);
    
    // Each mixed function called by 2 threads (i % 2)
    CHECK(mixed0_stats->calls == 2);
    CHECK(mixed0_stats->threads == 2);
    CHECK(mixed1_stats->calls == 2);
    CHECK(mixed1_stats->threads == 2);
}

TEST_CASE("Nested function calls in multithreaded environment") {
    clear_ctrack();
    const int num_threads = 3;
    const int parent_sleep_ms = 5;
    const int child_sleep_ms = 5;
    
    ThreadBarrier barrier(num_threads);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&barrier, parent_sleep_ms, child_sleep_ms]() {
            barrier.wait();
            nested_parent(child_sleep_ms, parent_sleep_ms);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto tables = ctrack::result_get_tables();
    CHECK(tables.details.rows.size() == 2); // nested_parent + nested_child
    
    // Find parent and child functions
    const ctrack::detail_stats* parent_stats = nullptr;
    const ctrack::detail_stats* child_stats = nullptr;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name == "nested_parent") {
            parent_stats = &stats;
        } else if (stats.function_name == "nested_child") {
            child_stats = &stats;
        }
    }
    
    REQUIRE(parent_stats != nullptr);
    REQUIRE(child_stats != nullptr);
    
    // Both functions called by all threads
    CHECK(parent_stats->calls == num_threads);
    CHECK(parent_stats->threads == num_threads);
    CHECK(child_stats->calls == num_threads);
    CHECK(child_stats->threads == num_threads);
    
    // Parent time should be greater than child time (includes child + own work)
    CHECK(parent_stats->time_acc > child_stats->time_acc);
    
    // Validate timing expectations
    auto expected_child_time = std::chrono::milliseconds(num_threads * child_sleep_ms);
    auto expected_parent_total_time = std::chrono::milliseconds(num_threads * (parent_sleep_ms + child_sleep_ms));
    
    CHECK(within_tolerance(child_stats->time_acc, expected_child_time, 25.0));
    CHECK(within_tolerance(parent_stats->time_acc, expected_parent_total_time, 25.0));
}

TEST_CASE("High contention stress test") {
    clear_ctrack();
    const int num_threads = 10;
    const int calls_per_thread = 50;
    const int sleep_time_ms = 5; // Milliseconds for stable testing
    
    std::vector<std::thread> threads;
    std::atomic<int> completed_calls{0};
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([calls_per_thread, sleep_time_ms, &completed_calls]() {
            for (int j = 0; j < calls_per_thread; j++) {
                {
                    CTRACK;
                    sleep_ms(sleep_time_ms);
                }
                completed_calls.fetch_add(1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    CHECK(completed_calls.load() == num_threads * calls_per_thread);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == num_threads * calls_per_thread);
    CHECK(stats.threads == num_threads);
    
    // Verify no data corruption occurred during high contention
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.fastest_min.count() > 0);
    CHECK(stats.center_mean.count() > 0);
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Random timing variations across threads") {
    clear_ctrack();
    const int num_threads = 5;
    const int calls_per_thread = 100;
    
    ThreadBarrier barrier(num_threads);
    std::vector<std::thread> threads;
    std::random_device rd;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&barrier, calls_per_thread, &rd]() {
            std::mt19937 gen(rd() + static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id())));
            std::uniform_int_distribution<> dis(1, 10); // 1-10ms random sleep
            
            barrier.wait();
            
            for (int j = 0; j < calls_per_thread; j++) {
                int sleep_time = dis(gen);
                multithreaded_test_function(sleep_time);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto tables = ctrack::result_get_tables();
    const auto& stats = tables.details.rows[0];
    
    CHECK(stats.calls == num_threads * calls_per_thread);
    CHECK(stats.threads == num_threads);
    
    // With random timing, we should see some variation
    CHECK(stats.cv > 0.0); // Coefficient of variation should be > 0
    CHECK(stats.fastest_min < stats.slowest_max); // Should have range
    
    // Basic sanity checks
    CHECK(stats.fastest_min <= stats.center_mean);
    CHECK(stats.center_mean <= stats.slowest_max);
}

