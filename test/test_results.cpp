#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"

#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

TEST_CASE("Summary row validation - basic fields") {
    test_helpers::clear_ctrack();
    
    // Execute a simple tracked function
    test_helpers::test_function_with_sleep(10, "test_function");
    
    auto tables = ctrack::result_get_tables();
    
    REQUIRE(tables.summary.rows.size() == 1);
    
    const auto& row = tables.summary.rows[0];
    
    // Basic field validation
    CHECK(row.function_name == "test_function");
    CHECK(row.calls == 1);
    CHECK(row.line > 0);
    CHECK_FALSE(row.filename.empty());
    
    // Percentage fields should be between 0-100
    CHECK(row.percent_ae_bracket >= 0.0);
    CHECK(row.percent_ae_bracket <= 100.0);
    CHECK(row.percent_ae_all >= 0.0);
    CHECK(row.percent_ae_all <= 100.0);
    
    // Time fields should be non-negative
    CHECK(row.time_ae_all.count() >= 0);
    CHECK(row.time_a_all.count() >= 0);
    
    // For a single function, it should account for 100% of tracked time
    CHECK(test_helpers::within_tolerance(row.percent_ae_all, 100.0, 5.0));
    CHECK(test_helpers::within_tolerance(row.percent_ae_bracket, 100.0, 5.0));
}

TEST_CASE("Summary row validation - multiple functions with different timing") {
    test_helpers::clear_ctrack();
    
    // Create functions with different execution times
    test_helpers::test_function_with_sleep(5, "fast_function");
    test_helpers::test_function_with_sleep(20, "slow_function");
    
    auto tables = ctrack::result_get_tables();
    
    REQUIRE(tables.summary.rows.size() == 2);
    
    // Find functions by name (order might vary)
    const ctrack::summary_row* fast_row = nullptr;
    const ctrack::summary_row* slow_row = nullptr;
    
    for (const auto& row : tables.summary.rows) {
        if (row.function_name == "fast_function") {
            fast_row = &row;
        } else if (row.function_name == "slow_function") {
            slow_row = &row;
        }
    }
    
    REQUIRE(fast_row != nullptr);
    REQUIRE(slow_row != nullptr);
    
    // Validate both rows
    CHECK(test_helpers::validate_summary_row(*fast_row, "fast_function", 1));
    CHECK(test_helpers::validate_summary_row(*slow_row, "slow_function", 1));
    
    // Slow function should have higher percentage than fast function
    CHECK(slow_row->percent_ae_all > fast_row->percent_ae_all);
    CHECK(slow_row->time_ae_all > fast_row->time_ae_all);
    
    // Combined percentages should approximately equal 100%
    double total_percent = fast_row->percent_ae_all + slow_row->percent_ae_all;
    CHECK(test_helpers::within_tolerance(total_percent, 100.0, 5.0));
}

TEST_CASE("Detail stats validation - comprehensive field check") {
    test_helpers::clear_ctrack();
    
    int calls = 100;
    // Execute function multiple times for meaningful statistics
    for (int i = 0; i < calls; i++) {
        test_helpers::test_function_with_sleep(10, "detail_test_function");
    }
    
    auto tables = ctrack::result_get_tables();
    
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    
    // Basic info fields
    CHECK(stats.function_name == "detail_test_function");
    CHECK(stats.calls == calls);
    CHECK(stats.threads == 1);
    CHECK(stats.line > 0);
    CHECK_FALSE(stats.filename.empty());
    
    // All time fields should be non-negative
    CHECK(stats.time_acc.count() >= 0);
    CHECK(stats.sd.count() >= 0);
    CHECK(stats.fastest_min.count() >= 0);
    CHECK(stats.fastest_mean.count() >= 0);
    CHECK(stats.center_min.count() >= 0);
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.center_med.count() >= 0);
    CHECK(stats.center_time_a.count() >= 0);
    CHECK(stats.center_time_ae.count() >= 0);
    CHECK(stats.center_max.count() >= 0);
    CHECK(stats.slowest_mean.count() >= 0);
    CHECK(stats.slowest_max.count() >= 0);
    
    // CV should be non-negative
    CHECK(stats.cv >= 0);
    
    // Logical time relationships
    CHECK(stats.fastest_min <= stats.fastest_mean);
    CHECK(stats.fastest_mean <= stats.center_mean);
    CHECK(stats.center_mean <= stats.slowest_mean);
    CHECK(stats.center_min <= stats.center_max);
    CHECK(stats.slowest_mean <= stats.slowest_max);
    
    // Range fields
    CHECK(stats.fastest_range >= 0);
    CHECK(stats.slowest_range >= 0);
    CHECK(stats.fastest_range <= 100);
    CHECK(stats.slowest_range <= 100);
    
    // Use validation helper
    CHECK(test_helpers::validate_detail_stats(stats, "detail_test_function", calls, 1));
}

TEST_CASE("Result meta information validation") {
    test_helpers::clear_ctrack();
    
    auto start = std::chrono::high_resolution_clock::now();
    test_helpers::test_function_with_sleep(10, "meta_test");
    test_helpers::test_function_with_sleep(15, "meta_test2");
    auto end = std::chrono::high_resolution_clock::now();
    
    auto tables = ctrack::result_get_tables();
    
    // Meta information should be valid
    CHECK(tables.time_total.count() > 0);
    CHECK(tables.time_ctracked.count() > 0);
    CHECK(tables.time_ctracked <= tables.time_total);
    CHECK(tables.start_time <= tables.end_time);
    
    // Time range should be reasonable
    auto expected_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    CHECK(tables.time_total <= expected_duration * 2); // Allow some overhead
    
    // Settings should be preserved
    CHECK(tables.settings.non_center_percent == 1); // Default value
    CHECK(tables.settings.min_percent_active_exclusive == 0.0); // Default value
    CHECK(tables.settings.percent_exclude_fastest_active_exclusive == 0.0); // Default value
}

TEST_CASE("Result settings filtering - min_percent_active_exclusive") {
    test_helpers::clear_ctrack();
    
    // Create functions with significantly different execution times
    // Small function: ~25ms total
    for (int i = 0; i < 5; i++) {
        test_helpers::test_function_with_sleep(5, "small_function");
    }
    
    // Large function: ~100ms total
    for (int i = 0; i < 10; i++) {
        test_helpers::test_function_with_sleep(10, "large_function");
    }
    
    // Test with no filtering (default)
    {
        ctrack::ctrack_result_settings settings;
        settings.min_percent_active_exclusive = 0.0;
        
        auto tables = ctrack::result_get_tables(settings);
        
        // Should include both functions
        CHECK(tables.summary.rows.size() == 2);
        CHECK(tables.details.rows.size() == 2);
    }
    
    test_helpers::clear_ctrack();
    
    // Recreate the same pattern
    for (int i = 0; i < 5; i++) {
        test_helpers::test_function_with_sleep(5, "small_function");
    }
    for (int i = 0; i < 10; i++) {
        test_helpers::test_function_with_sleep(10, "large_function");
    }
    
    // Test with filtering that should exclude small function
    {
        ctrack::ctrack_result_settings settings;
        settings.min_percent_active_exclusive = 25.0; // Should filter out small contributors
        
        auto tables = ctrack::result_get_tables(settings);
        
        // Should only include large_function
        CHECK(tables.summary.rows.size() == 1);
        CHECK(tables.details.rows.size() == 1);
        
        if (!tables.summary.rows.empty()) {
            CHECK(tables.summary.rows[0].function_name == "large_function");
        }
        if (!tables.details.rows.empty()) {
            CHECK(tables.details.rows[0].function_name == "large_function");
        }
    }
}

TEST_CASE("Result settings - percent_exclude_fastest_active_exclusive") {
    test_helpers::clear_ctrack();
    int calls = 100;
    // Execute function multiple times to have fastest/slowest distribution
    for (int i = 0; i < calls; i++) {
        // Vary sleep time slightly to create distribution
        int sleep_time = 8 + (i % 5); // 8-12ms range
        test_helpers::test_function_with_sleep(sleep_time, "variable_function");
    }
    
    // Test with different exclusion percentages
    ctrack::ctrack_result_settings settings;
    settings.percent_exclude_fastest_active_exclusive = 10.0; // Exclude fastest 10%
    
    auto tables = ctrack::result_get_tables(settings);
    
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& stats = tables.details.rows[0];
    CHECK(stats.function_name == "variable_function");
    CHECK(stats.calls == calls);
    
    // Settings should be preserved
    CHECK(tables.settings.percent_exclude_fastest_active_exclusive == 10.0);
    
    // Validate the stats structure
    CHECK(test_helpers::validate_detail_stats(stats, "variable_function", calls, 1));
}

TEST_CASE("Result settings - non_center_percent variations") {
    test_helpers::clear_ctrack();
    
    // Execute function multiple times
    for (int i = 0; i < 50; i++) {
        test_helpers::test_function_with_sleep(5, "center_test");
    }
    
    // Test different non_center_percent values
    std::vector<unsigned int> test_values = {1, 5, 10};
    
    for (unsigned int center_percent : test_values) {
        test_helpers::clear_ctrack();
        
        // Recreate the same pattern
        for (int i = 0; i < 50; i++) {
            test_helpers::test_function_with_sleep(5, "center_test");
        }
        
        ctrack::ctrack_result_settings settings;
        settings.non_center_percent = center_percent;
        
        auto tables = ctrack::result_get_tables(settings);
        
        REQUIRE(tables.details.rows.size() == 1);
        
        const auto& stats = tables.details.rows[0];
        
        // Settings should be preserved
        CHECK(tables.settings.non_center_percent == center_percent);
        
        // Range fields should reflect the percentage
        CHECK(stats.fastest_range == center_percent);
        CHECK(stats.slowest_range == 100-center_percent);
        
        // Validate structure
        CHECK(test_helpers::validate_detail_stats(stats, "center_test", 50, 1));
    }
}

TEST_CASE("Summary table sorting and ordering") {
    test_helpers::clear_ctrack();
    
    // Create functions with different execution times and call counts
    test_helpers::test_function_with_sleep(5, "function_a");
    
    for (int i = 0; i < 3; i++) {
        test_helpers::test_function_with_sleep(10, "function_b");
    }
    
    test_helpers::test_function_with_sleep(30, "function_c");
    
    auto tables = ctrack::result_get_tables();
    
    REQUIRE(tables.summary.rows.size() == 3);
    
    // Rows should be ordered by total time (descending)
    // function_c should be first (30ms), then function_b (30ms total), then function_a (5ms)
    for (size_t i = 0; i < tables.summary.rows.size() - 1; i++) {
        CHECK(tables.summary.rows[i].time_ae_all >= tables.summary.rows[i + 1].time_ae_all);
    }
    
    // Verify all rows are valid
    for (const auto& row : tables.summary.rows) {
        CHECK(row.calls > 0);
        CHECK(row.line > 0);
        CHECK_FALSE(row.function_name.empty());
        CHECK_FALSE(row.filename.empty());
        CHECK(row.percent_ae_all >= 0.0);
        CHECK(row.percent_ae_all <= 100.0);
        CHECK(row.time_ae_all.count() >= 0);
        CHECK(row.time_a_all.count() >= 0);
    }
}

TEST_CASE("Empty results - no tracked functions") {
    test_helpers::clear_ctrack();
    
    // Don't execute any tracked functions
    auto tables = ctrack::result_get_tables();
    
    // Should have empty tables
    CHECK(tables.summary.rows.empty());
    CHECK(tables.details.rows.empty());
    
    // Meta information should still be valid
    CHECK(tables.time_total.count() >= 0);
    CHECK(tables.time_ctracked.count() >= 0);
    CHECK(tables.start_time <= tables.end_time);
    
    // Settings should have default values
    CHECK(tables.settings.non_center_percent == 1);
    CHECK(tables.settings.min_percent_active_exclusive == 0.0);
    CHECK(tables.settings.percent_exclude_fastest_active_exclusive == 0.0);
}

TEST_CASE("Complex settings combination") {
    test_helpers::clear_ctrack();
    
    // Create a complex scenario with multiple functions
    for (int i = 0; i < 10; i++) {
        test_helpers::test_function_with_sleep(1, "tiny_function");
    }
    
    for (int i = 0; i < 20; i++) {
        test_helpers::test_function_with_sleep(5, "small_function");
    }
    
    for (int i = 0; i < 10; i++) {
        test_helpers::test_function_with_sleep(20, "large_function");
    }
    
    // Apply complex filtering
    ctrack::ctrack_result_settings settings;
    settings.non_center_percent = 5;
    settings.min_percent_active_exclusive = 15.0; // Should filter out tiny_function
    settings.percent_exclude_fastest_active_exclusive = 5.0;
    
    auto tables = ctrack::result_get_tables(settings);
    
    // Should filter out tiny_function due to min_percent_active_exclusive
    CHECK(tables.summary.rows.size() <= 2);
    CHECK(tables.details.rows.size() <= 2);
    
    // Settings should be preserved
    CHECK(tables.settings.non_center_percent == 5);
    CHECK(tables.settings.min_percent_active_exclusive == 15.0);
    CHECK(tables.settings.percent_exclude_fastest_active_exclusive == 5.0);
    
    // Verify all remaining functions meet the criteria
    for (const auto& row : tables.summary.rows) {
        CHECK(row.function_name != "tiny_function"); // Should be filtered out
        CHECK(test_helpers::validate_summary_row(row, row.function_name, row.calls));
    }
    
    for (const auto& stats : tables.details.rows) {
        CHECK(stats.function_name != "tiny_function"); // Should be filtered out
        CHECK(test_helpers::validate_detail_stats(stats, stats.function_name, stats.calls, stats.threads));
        
        // Range percentages should match settings
        CHECK(stats.fastest_range == 5);
        CHECK(stats.slowest_range == 95);
    }
}

TEST_CASE("Multiple calls same function - statistical validation") {
    test_helpers::clear_ctrack();
    
    // Execute the same function with varying sleep times
    std::vector<int> sleep_times = {5, 8, 10, 12, 15, 18, 20, 22, 25, 30};
    
    for (int sleep_time : sleep_times) {
        test_helpers::test_function_with_sleep(sleep_time, "stats_function");
    }
    
    auto tables = ctrack::result_get_tables();
    
    REQUIRE(tables.summary.rows.size() == 1);
    REQUIRE(tables.details.rows.size() == 1);
    
    const auto& summary = tables.summary.rows[0];
    const auto& details = tables.details.rows[0];
    
    // Summary validation
    CHECK(summary.function_name == "stats_function");
    CHECK(summary.calls == 10);
    CHECK(test_helpers::validate_summary_row(summary, "stats_function", 10));
    
    // Details validation
    CHECK(details.function_name == "stats_function");
    CHECK(details.calls == 10);
    CHECK(details.threads == 1);
    CHECK(test_helpers::validate_detail_stats(details, "stats_function", 10, 1));
    
    // Statistical properties
    CHECK(details.cv >= 0.0); // Coefficient of variation should be positive for varied data
    CHECK(details.sd.count() > 0); // Standard deviation should be positive for varied data
    
    // Time ordering should be maintained

    CHECK(details.center_min <= details.center_mean);
    CHECK(details.center_mean <= details.center_max);

}

TEST_CASE("Result consistency across multiple calls") {
    test_helpers::clear_ctrack();
    
    // Execute some functions
    test_helpers::test_function_with_sleep(10, "consistent_test");
    test_helpers::test_function_with_sleep(15, "consistent_test2");
    
    // Get results multiple times with same settings
    ctrack::ctrack_result_settings settings;
    settings.non_center_percent = 2;
    
    auto tables1 = ctrack::result_get_tables(settings);
    
    test_helpers::clear_ctrack();
    
    // Recreate same pattern
    test_helpers::test_function_with_sleep(10, "consistent_test");
    test_helpers::test_function_with_sleep(15, "consistent_test2");
    
    auto tables2 = ctrack::result_get_tables(settings);
    
    // Results should be consistent
    CHECK(tables1.summary.rows.size() == tables2.summary.rows.size());
    CHECK(tables1.details.rows.size() == tables2.details.rows.size());
    
    // Settings should match
    CHECK(tables1.settings.non_center_percent == tables2.settings.non_center_percent);
    CHECK(tables1.settings.min_percent_active_exclusive == tables2.settings.min_percent_active_exclusive);
    CHECK(tables1.settings.percent_exclude_fastest_active_exclusive == tables2.settings.percent_exclude_fastest_active_exclusive);
    
    // Function names and call counts should match
    if (tables1.summary.rows.size() == tables2.summary.rows.size()) {
        for (size_t i = 0; i < tables1.summary.rows.size(); i++) {
            CHECK(tables1.summary.rows[i].function_name == tables2.summary.rows[i].function_name);
            CHECK(tables1.summary.rows[i].calls == tables2.summary.rows[i].calls);
        }
    }
}