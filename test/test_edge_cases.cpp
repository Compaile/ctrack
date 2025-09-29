#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"

#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <functional>

TEST_CASE("Zero duration functions") {
    test_helpers::clear_ctrack();
    
    // Function with no actual work - just the CTRACK macro
    auto zero_duration = []() {
		CTRACK_NAME("zero_duration");
        // No sleep or work, just return immediately
    };
    
    const int call_count = 1000;
    for (int i = 0; i < call_count; i++) {
        zero_duration();
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == call_count);
    CHECK(stats.threads == 1);
    CHECK(stats.function_name.find("zero_duration") != std::string::npos);
    
    // Even zero-duration should have some overhead (nanoseconds)
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.center_mean.count() < 1000000); // Less than 1ms per call
    CHECK(stats.time_acc.count() >= 0);
    CHECK(stats.fastest_min.count() >= 0);
    CHECK(stats.slowest_max.count() >= stats.fastest_min.count());
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Single call scenarios") {
    test_helpers::clear_ctrack();
    
    // Test single call with no work
    {
        CTRACK_NAME("single_call_no_work");
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == 1);
    CHECK(stats.threads == 1);
    CHECK(stats.function_name == "single_call_no_work");
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.fastest_min == stats.slowest_max); // Single call, min == max
    CHECK(stats.center_min == stats.center_max);   // Single call, min == max
    
    // CV should be 0 for single call (no variation)
    CHECK(stats.cv == 0.0);
}

TEST_CASE("Very high call counts") {
    test_helpers::clear_ctrack();
    
    const int high_count = 100000;
    
    auto high_frequency_func = []() {
        CTRACK_NAME("high_frequency");
        // Minimal work to avoid compiler optimization
        volatile int x = 42;
        (void)x;
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < high_count; i++) {
        high_frequency_func();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == high_count);
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.fastest_min.count() >= 0);
    CHECK(stats.slowest_max.count() >= stats.fastest_min.count());
    
    // Verify no integer overflow occurred
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.time_acc.count() < std::numeric_limits<int64_t>::max() / 2);
    
    // Summary should also be consistent
    CHECK(tables.summary.rows.size() == 1);
    CHECK(tables.summary.rows[0].calls == high_count);
}

TEST_CASE("Empty tracking - no CTRACK calls") {
    test_helpers::clear_ctrack();
    
    // Do some work without any tracking
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    (void)sum;
    
    auto tables = ctrack::result_get_tables();
    
    // Should have no tracked functions
    CHECK(tables.summary.rows.empty());
    CHECK(tables.details.rows.empty());
    CHECK(tables.time_ctracked.count() == 0);
    CHECK(tables.time_total.count() > 0); // Should still measure total time
}

// Deep recursive function for extreme nesting
int deep_recursive_func(int depth, int max_depth) {
    if (depth >= max_depth) {
        CTRACK_NAME("recursive_base");
        return depth;
    }
    
    CTRACK_NAME("recursive_call");
    return deep_recursive_func(depth + 1, max_depth);
}

TEST_CASE("Extremely nested scenarios") {
    test_helpers::clear_ctrack();
    
    const int nesting_depth = 25;
    int result = deep_recursive_func(0, nesting_depth);
    
    CHECK(result == nesting_depth);
    
    auto tables = ctrack::result_get_tables();
    
    // Should have two function entries: recursive_call and recursive_base
    REQUIRE(tables.details.rows.size() == 2);
    
    // Find the recursive call stats
    const ctrack::detail_stats* recursive_stats = nullptr;
    const ctrack::detail_stats* base_stats = nullptr;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name == "recursive_call") {
            recursive_stats = &stats;
        } else if (stats.function_name == "recursive_base") {
            base_stats = &stats;
        }
    }
    
    REQUIRE(recursive_stats != nullptr);
    REQUIRE(base_stats != nullptr);
    
    // Recursive call should be called nesting_depth times
    CHECK(recursive_stats->calls == nesting_depth);
    // Base case should be called once
    CHECK(base_stats->calls == 1);
    
    // All calls should be on the same thread
    CHECK(recursive_stats->threads == 1);
    CHECK(base_stats->threads == 1);
    
    // Verify timing relationships are maintained
    CHECK(recursive_stats->time_acc.count() >= base_stats->time_acc.count());
}

TEST_CASE("Functions with same name but different locations") {
    test_helpers::clear_ctrack();
    
    // Lambda 1
    auto func1 = []() {
        CTRACK_NAME("same_function_name");
        test_helpers::sleep_ms(5);
    };
    
    // Lambda 2 - different location, same name
    auto func2 = []() {
        CTRACK_NAME("same_function_name");
        test_helpers::sleep_ms(5);
    };
    
    // Call both functions multiple times
    for (int i = 0; i < 10; i++) {
        func1();
        func2();
    }
    
    auto tables = ctrack::result_get_tables();
    
    // Should have separate entries for each location
    // (Note: ctrack might aggregate by name, depending on implementation)
    CHECK(tables.details.rows.size() >= 1);
    CHECK(tables.details.rows.size() <= 2);
    
    // Total calls should be 20
    int total_calls = 0;
    for (const auto& stats : tables.details.rows) {
        CHECK(stats.function_name == "same_function_name");
        total_calls += stats.calls;
    }
    CHECK(total_calls == 20);
}

TEST_CASE("Very long running functions") {
    test_helpers::clear_ctrack();
    
    auto long_running_func = []() {
        CTRACK_NAME("long_runner");
        test_helpers::sleep_ms(500); // 500ms
    };
    
    const int call_count = 3;
    for (int i = 0; i < call_count; i++) {
        long_running_func();
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == call_count);
    CHECK(stats.function_name == "long_runner");
    
    // Each call should be around 500ms
    auto expected_per_call = std::chrono::nanoseconds(500 * 1000000); // 500ms in ns
    CHECK(stats.center_mean.count() >= expected_per_call.count() * 0.8);
    CHECK(stats.center_mean.count() <= expected_per_call.count() * 1.3);
    
    // Total time should be around 1.5 seconds
    auto expected_total = std::chrono::nanoseconds(1500 * 1000000); // 1.5s in ns
    CHECK(stats.time_acc.count() >= expected_total.count() * 0.8);
    CHECK(stats.time_acc.count() <= expected_total.count() * 1.3);
    
    // Verify no overflow in timing calculations
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.center_mean.count() > 0);
  
}

TEST_CASE("Mix of very fast and very slow functions") {
    test_helpers::clear_ctrack();
    
    auto fast_func = []() {
        CTRACK_NAME("fast_function");
        // Just volatile operations
        volatile int x = 1;
        x *= 2;
        (void)x;
    };
    
    auto slow_func = []() {
        CTRACK_NAME("slow_function");
        test_helpers::sleep_ms(100);
    };
    
    // Call fast function many times, slow function few times
    const int fast_calls = 1000;
    const int slow_calls = 5;
    
    for (int i = 0; i < fast_calls; i++) {
        fast_func();
    }
    
    for (int i = 0; i < slow_calls; i++) {
        slow_func();
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 2);
    
    const ctrack::detail_stats* fast_stats = nullptr;
    const ctrack::detail_stats* slow_stats = nullptr;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name == "fast_function") {
            fast_stats = &stats;
        } else if (stats.function_name == "slow_function") {
            slow_stats = &stats;
        }
    }
    
    REQUIRE(fast_stats != nullptr);
    REQUIRE(slow_stats != nullptr);
    
    CHECK(fast_stats->calls == fast_calls);
    CHECK(slow_stats->calls == slow_calls);
    
    // Slow function should have much higher mean time
    CHECK(slow_stats->center_mean.count() > fast_stats->center_mean.count() * 1000);
    
    // Both should have valid statistics
    CHECK(fast_stats->cv >= 0.0);
    CHECK(slow_stats->cv >= 0.0);
    CHECK(fast_stats->time_acc.count() > 0);
    CHECK(slow_stats->time_acc.count() > 0);
}

TEST_CASE("Rapid successive calls - stress test") {
    test_helpers::clear_ctrack();
    
    const int rapid_count = 50000;
    std::atomic<int> counter{0};
    
    auto rapid_func = [&counter]() {
        CTRACK_NAME("rapid_calls");
        counter.fetch_add(1, std::memory_order_relaxed);
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Rapid fire calls
    for (int i = 0; i < rapid_count; i++) {
        rapid_func();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    CHECK(counter.load() == rapid_count);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == rapid_count);
    CHECK(stats.function_name == "rapid_calls");
    
    // Verify timing is reasonable (should be very fast per call)
    CHECK(stats.center_mean.count() < 10000); // Less than 10 microseconds per call
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.time_acc.count() <= total_time.count() * 2); // Allow some overhead
    
    // Verify statistics are computed correctly even with rapid calls
    CHECK(stats.fastest_min.count() >= 0);
    CHECK(stats.slowest_max.count() >= stats.fastest_min.count());
    CHECK(stats.cv >= 0.0);
    CHECK(stats.sd.count() >= 0);
}

TEST_CASE("Boundary condition - maximum thread count") {
    test_helpers::clear_ctrack();
    
    const int thread_count = std::min(20, static_cast<int>(std::thread::hardware_concurrency() * 2));
    std::vector<std::future<void>> futures;
    test_helpers::ThreadBarrier barrier(thread_count);
    
    auto threaded_func = [&barrier]() {
        CTRACK_NAME("multithreaded_boundary");
        barrier.wait(); // Synchronize all threads
        test_helpers::sleep_ms(5); // 5ms work
    };
    
    // Launch multiple threads
    for (int i = 0; i < thread_count; i++) {
        futures.push_back(std::async(std::launch::async, threaded_func));
    }
    
    // Wait for all threads
    for (auto& future : futures) {
        future.wait();
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == thread_count);
    CHECK(stats.function_name == "multithreaded_boundary");
    CHECK(stats.threads == thread_count);
    
    // Verify multithreaded timing makes sense
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.center_mean.count() >= 5000000); // At least 5ms per call
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Precision edge case - very small time differences") {
    test_helpers::clear_ctrack();
    
    auto micro_work_func = []() {
        CTRACK_NAME("micro_work");
        // Minimal CPU work that should complete in nanoseconds
        volatile int result = 0;
        for (int i = 0; i < 10; i++) {
            result += i * i;
        }
        (void)result;
    };
    
    const int call_count = 10000;
    for (int i = 0; i < call_count; i++) {
        micro_work_func();
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == call_count);
    
    // Even micro work should be measurable
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.time_acc.count() > 0);
    CHECK(stats.fastest_min.count() >= 0);
    CHECK(stats.slowest_max.count() >= stats.fastest_min.count());
    
    // Standard deviation and CV should be computed correctly
    CHECK(stats.sd.count() >= 0);
    CHECK(stats.cv >= 0.0);
    
    // For very small times, ensure no division by zero or overflow
    if (stats.center_mean.count() > 0) {
        double calculated_cv = static_cast<double>(stats.sd.count()) / static_cast<double>(stats.center_mean.count());
        CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 20.0));
    }
}

TEST_CASE("Memory stress - tracking large number of unique functions") {
    test_helpers::clear_ctrack();
    
    // Create many unique function names
    const int unique_functions = 100;
    std::vector<std::function<void()>> functions;
    
    for (int i = 0; i < unique_functions; i++) {
        std::string name = "unique_func_" + std::to_string(i);
        functions.emplace_back([name]() {
            CTRACK_NAME(name.c_str());
            test_helpers::sleep_ms(5);
        });
    }
    
    // Call each function once
    for (auto& func : functions) {
        func();
    }
    
    auto tables = ctrack::result_get_tables();
    CHECK(tables.details.rows.size() == unique_functions);
    CHECK(tables.summary.rows.size() == unique_functions);
    
    // Verify each function is tracked correctly
    std::set<std::string> found_names;
    for (const auto& stats : tables.details.rows) {
        CHECK(stats.calls == 1);
        CHECK(stats.threads == 1);
        CHECK(stats.function_name.find("unique_func_") == 0);
        found_names.insert(stats.function_name);
    }
    
    CHECK(found_names.size() == unique_functions);
}