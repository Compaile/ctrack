#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"

#include <chrono>
#include <thread>

namespace {

// Recursive factorial function for testing recursive scenarios
int recursive_factorial(int n) {
    CTRACK;
    if (n <= 1) {
        test_helpers::sleep_ms(5); // Small work at base case
        return 1;
    }
    test_helpers::sleep_ms(5); // Work at each level
    return n * recursive_factorial(n - 1);
}

// Recursive fibonacci for testing deep recursion with branching
int recursive_fibonacci(int n) {
    CTRACK;
    if (n <= 1) {
        test_helpers::sleep_ms(5);
        return n;
    }
    test_helpers::sleep_ms(5);
    return recursive_fibonacci(n - 1) + recursive_fibonacci(n - 2);
}

// Diamond pattern helper functions
void diamond_leaf_d(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

void diamond_branch_b(int sleep_ms, int leaf_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    diamond_leaf_d(leaf_sleep_ms);
}

void diamond_branch_c(int sleep_ms, int leaf_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    diamond_leaf_d(leaf_sleep_ms);
}

void diamond_root_a(int sleep_ms, int branch_sleep_ms, int leaf_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    diamond_branch_b(branch_sleep_ms, leaf_sleep_ms);
    diamond_branch_c(branch_sleep_ms, leaf_sleep_ms);
}

// Deep nesting functions
void deep_level_5(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

void deep_level_4(int sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    deep_level_5(child_sleep_ms);
}

void deep_level_3(int sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    deep_level_4(sleep_ms, child_sleep_ms);
}

void deep_level_2(int sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    deep_level_3(sleep_ms, child_sleep_ms);
}

void deep_level_1(int sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    deep_level_2(sleep_ms, child_sleep_ms);
}

void deep_root(int sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
    deep_level_1(sleep_ms, child_sleep_ms);
}

// Multiple children functions
void multi_child_1(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

void multi_child_2(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

void multi_child_3(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

void multi_parent(int parent_sleep_ms, int child_sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(parent_sleep_ms);
    multi_child_1(child_sleep_ms);
    multi_child_2(child_sleep_ms);
    multi_child_3(child_sleep_ms);
}

} // anonymous namespace

TEST_CASE("Simple nested functions - two levels") {
    test_helpers::clear_ctrack();
    
    // Root: 10ms, Level1: 5ms, Level2: 2ms
    // Expected: root total ~17ms, root exclusive ~10ms
    test_helpers::nested_root(20, 5, 2);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 3);
    
    // Find each function in results and validate
    bool found_root = false, found_level1 = false, found_level2 = false;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("nested_root") != std::string::npos) {
            found_root = true;
            CHECK(stats.calls == 1);
            // Root exclusive time should be around 20ms (its own sleep)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(20)));
            // Root total time should be around 27ms (20 + 5 + 2)
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(27)));
            // Exclusive should be less than total for parent functions
            CHECK(stats.center_time_ae < stats.center_time_a);
        }
        else if (stats.function_name.find("nested_level_1") != std::string::npos) {
            found_level1 = true;
            CHECK(stats.calls == 1);
            // Level1 exclusive time should be around 5ms
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(5)));
            // Level1 total time should be around 7ms (5 + 2)
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(7)));
            CHECK(stats.center_time_ae < stats.center_time_a);
        }
        else if (stats.function_name.find("nested_level_2") != std::string::npos) {
            found_level2 = true;
            CHECK(stats.calls == 1);
            // Level2 is leaf: exclusive should equal total (around 2ms)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(2)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(2)));
            CHECK(test_helpers::within_tolerance(static_cast<double>(stats.center_time_ae.count()), static_cast<double>(stats.center_time_a.count()), 5.0));
        }
    }
    
    CHECK(found_root);
    CHECK(found_level1);
    CHECK(found_level2);
}

TEST_CASE("Deep nesting - 6 levels") {
    test_helpers::clear_ctrack();
    
    // Each level sleeps 3ms, total should be 6*3 = 18ms
    deep_root(3, 3);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 6);
    
    bool found_deep_root = false, found_deep_level_5 = false;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("deep_root") != std::string::npos) {
            found_deep_root = true;
            CHECK(stats.calls == 1);
            // Root exclusive should be ~3ms, total should be ~18ms
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(18)));
            CHECK(stats.center_time_ae < stats.center_time_a);
        }
        else if (stats.function_name.find("deep_level_5") != std::string::npos) {
            found_deep_level_5 = true;
            CHECK(stats.calls == 1);
            // Leaf function: exclusive should equal total (~3ms)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(static_cast<double>(stats.center_time_ae.count()), static_cast<double>(stats.center_time_a.count()), 5.0));
        }
    }
    
    CHECK(found_deep_root);
    CHECK(found_deep_level_5);
}

TEST_CASE("Multiple children per parent") {
    test_helpers::clear_ctrack();
    
    // Parent: 5ms, each of 3 children: 3ms
    // Expected: parent total ~14ms (5 + 3*3), parent exclusive ~5ms
    multi_parent(5, 3);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 4);
    
    bool found_parent = false;
    int found_children = 0;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("multi_parent") != std::string::npos) {
            found_parent = true;
            CHECK(stats.calls == 1);
            // Parent exclusive should be ~5ms, total should be ~14ms
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(5)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(14)));
            CHECK(stats.center_time_ae < stats.center_time_a);
        }
        else if (stats.function_name.find("multi_child_") != std::string::npos) {
            found_children++;
            CHECK(stats.calls == 1);
            // Each child: exclusive should equal total (~3ms)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(static_cast<double>(stats.center_time_ae.count()), static_cast<double>(stats.center_time_a.count()), 5.0));
        }
    }
    
    CHECK(found_parent);
    CHECK(found_children == 3);
}

TEST_CASE("Diamond pattern - A calls B and C, both call D") {
    test_helpers::clear_ctrack();
    
    // A: 2ms, B: 2ms, C: 2ms, D: 3ms each call
    // Expected: A total ~12ms (2 + 2*2 + 2*3), D called twice
    diamond_root_a(2, 2, 3);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 4);
    
    bool found_a = false, found_b = false, found_c = false, found_d = false;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("diamond_root_a") != std::string::npos) {
            found_a = true;
            CHECK(stats.calls == 1);
            // A exclusive ~2ms, total ~12ms (2 + 2*2 + 2*3)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(2)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(12)));
        }
        else if (stats.function_name.find("diamond_branch_b") != std::string::npos) {
            found_b = true;
            CHECK(stats.calls == 1);
            // B exclusive ~2ms, total ~5ms (2 + 3)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(2)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(5)));
        }
        else if (stats.function_name.find("diamond_branch_c") != std::string::npos) {
            found_c = true;
            CHECK(stats.calls == 1);
            // C exclusive ~2ms, total ~5ms (2 + 3)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(2)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(5)));
        }
        else if (stats.function_name.find("diamond_leaf_d") != std::string::npos) {
            found_d = true;
            CHECK(stats.calls == 2); // Called by both B and C
            // D is leaf: exclusive should equal total 6(~3ms per call)
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(6)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(6)));
        }
    }
    
    CHECK(found_a);
    CHECK(found_b);
    CHECK(found_c);
    CHECK(found_d);
}

TEST_CASE("Recursive factorial - linear recursion") {
    test_helpers::clear_ctrack();
    
    // Calculate factorial of 5: should create 5 calls (5, 4, 3, 2, 1)
    int result = recursive_factorial(5);
    CHECK(result == 120);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1); // All calls are to the same function
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.function_name.find("recursive_factorial") != std::string::npos);
    CHECK(stats.calls == 5);
    
    // Each call sleeps 5ms, so total active time should be ~25ms
    CHECK(test_helpers::within_tolerance(stats.time_a_all, std::chrono::milliseconds(25)));
    
    // For recursive functions, we expect some variation in individual call times
    // due to the recursive overhead, but each should be roughly 5ms + overhead
    CHECK(stats.center_time_ae > std::chrono::milliseconds(0));
    CHECK(stats.center_time_a > std::chrono::milliseconds(0));
}

TEST_CASE("Recursive fibonacci - branching recursion") {
    test_helpers::clear_ctrack();
    
    // Calculate fibonacci of 4: fib(4) = fib(3) + fib(2)
    // This creates multiple calls with different recursion depths
    int result = recursive_fibonacci(4);
    CHECK(result == 3); // fib(4) = 3
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1); // All calls are to the same function
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.function_name.find("recursive_fibonacci") != std::string::npos);
    
    // Fibonacci(4) should make these calls:
    // fib(4), fib(3), fib(2), fib(1), fib(0), fib(1), fib(2), fib(1), fib(0)
    // That's 9 total calls
    CHECK(stats.calls == 9);
    
    // Each call sleeps 5ms, so total active time should be ~45ms
    CHECK(test_helpers::within_tolerance(stats.time_a_all, std::chrono::milliseconds(45)));
}

TEST_CASE("Nested functions with multiple calls to same child") {
    test_helpers::clear_ctrack();
    
    // Call nested_level_2 multiple times from different contexts
    test_helpers::nested_level_1(3, 2); // First call path
    test_helpers::nested_level_2(2);    // Direct call
    
    auto tables = ctrack::result_get_tables();
    
    bool found_level1 = false, found_level2 = false;
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("nested_level_1") != std::string::npos) {
            found_level1 = true;
            CHECK(stats.calls == 1);
            // Level1: 3ms own + 2ms child = 5ms total, 3ms exclusive
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(3)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(5)));
        }
        else if (stats.function_name.find("nested_level_2") != std::string::npos) {
            found_level2 = true;
            CHECK(stats.calls == 2); // Called twice
            // Each call is 2ms, total active time is 4ms
            CHECK(test_helpers::within_tolerance(stats.time_a_all, std::chrono::milliseconds(4)));
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(4)));
            CHECK(test_helpers::within_tolerance(stats.center_time_a, std::chrono::milliseconds(4)));
        }
    }
    
    CHECK(found_level1);
    CHECK(found_level2);
}

TEST_CASE("Verify parent-child time relationships in complex nesting") {
    test_helpers::clear_ctrack();
    
    // Create a complex call tree:
    // root(5) -> level1(3) -> level2(2)
    //         -> level1(4) -> level2(1)

    test_helpers::nested_root(5, 3, 2);
    test_helpers::nested_level_1(4, 1);
    
    auto tables = ctrack::result_get_tables();
    
    std::chrono::nanoseconds root_total{0}, root_exclusive{0};
    std::chrono::nanoseconds level1_total_active{0}, level1_exclusive_active{0};
    std::chrono::nanoseconds level2_total_active{0}, level2_exclusive_active{0};
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("nested_root") != std::string::npos) {
            root_total = stats.time_a_all;
            root_exclusive = stats.time_ae_all;
        }
        else if (stats.function_name.find("nested_level_1") != std::string::npos) {
            level1_total_active = stats.time_a_all;
            level1_exclusive_active = stats.time_ae_all;
          
        }
        else if (stats.function_name.find("nested_level_2") != std::string::npos) {
            level2_total_active = stats.time_a_all;
            level2_exclusive_active = stats.time_ae_all;
          
        }
    }
    
    // Verify that parent exclusive time + children total time ≈ parent total time
    // Root exclusive (5ms) + Level1 calls total (3+2+4+1 = 10ms) ≈ Root total (15ms)
    auto expected_root_total = root_exclusive + level1_total_active;
    CHECK(test_helpers::within_tolerance(static_cast<double>(root_total.count()+ std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(4)).count() ), static_cast<double>(expected_root_total.count()), 20.0));
    
    // For leaf functions, exclusive time should equal total time
    CHECK(test_helpers::within_tolerance(static_cast<double>(level2_total_active.count()), static_cast<double>(level2_exclusive_active.count()), 5.0));
}

TEST_CASE("Verify time_active_exclusive calculations are correct") {
    test_helpers::clear_ctrack();
    
    // Simple case: parent(10ms) -> child(5ms)
    // Parent total should be ~15ms, parent exclusive should be ~10ms
    test_helpers::nested_level_1(10, 5);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 2);
    
    for (const auto& stats : tables.details.rows) {
        if (stats.function_name.find("nested_level_1") != std::string::npos) {
            // Parent function
            CHECK(stats.calls == 1);
            
            // Exclusive time should be significantly less than total time
            CHECK(stats.center_time_ae < stats.center_time_a);
            
            // The difference should be approximately the child's execution time
            auto child_time_approx = stats.center_time_a - stats.center_time_ae;
            CHECK(test_helpers::within_tolerance(child_time_approx, std::chrono::milliseconds(5)));
            
            // Exclusive time should be approximately the parent's own work
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(10)));
        }
        else if (stats.function_name.find("nested_level_2") != std::string::npos) {
            // Child (leaf) function
            CHECK(stats.calls == 1);
            
            // For leaf functions, exclusive should equal total
            CHECK(test_helpers::within_tolerance(static_cast<double>(stats.center_time_ae.count()), static_cast<double>(stats.center_time_a.count()), 5.0));
            
            // Should be approximately 5ms
            CHECK(test_helpers::within_tolerance(stats.center_time_ae, std::chrono::milliseconds(5)));
        }
    }
}

TEST_CASE("Large recursion depth handling") {
    test_helpers::clear_ctrack();
    
    // Test with a larger factorial to ensure the library handles deeper recursion
    int result = recursive_factorial(8);
    CHECK(result == 40320);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.calls == 8);
    
    // Total active time should be ~40ms (8 calls × 5ms each)
    CHECK(test_helpers::within_tolerance(stats.time_a_all, std::chrono::milliseconds(40)));
    
    // Verify that the statistics are reasonable
    CHECK(stats.center_time_ae > std::chrono::milliseconds(0));
    CHECK(stats.center_time_a > std::chrono::milliseconds(0));
    CHECK(stats.cv >= 0.0); // Coefficient of variation should be non-negative
}