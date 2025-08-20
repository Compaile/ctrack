#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"

// Test functions with predictable timing
void simple_function_5ms()
{
    CTRACK;
    test_helpers::sleep_ms(5);
}

void simple_function_10ms()
{
    CTRACK;
    test_helpers::sleep_ms(10);
}

void simple_function_20ms()
{
    CTRACK;
    test_helpers::sleep_ms(20);
}

void zero_duration_function()
{
    CTRACK;
    // No delay - should have near-zero execution time
}

void varying_sleep_function(int sleep_ms)
{
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

// Nested function hierarchy
void nested_child_function()
{
    CTRACK;
    test_helpers::sleep_ms(5);
}

void nested_parent_function()
{
    CTRACK;
    test_helpers::sleep_ms(5);
    nested_child_function();
}

// Recursive function
int recursive_factorial(int n)
{
    CTRACK;
    test_helpers::sleep_ms(5); // 5ms delay per call
    if (n <= 1)
        return 1;
    return n * recursive_factorial(n - 1);
}

TEST_CASE("Basic single function tracking - 5ms sleep")
{
    test_helpers::clear_ctrack();

    // Execute function 10 times with 5ms sleep
    for (int i = 0; i < 100; i++)
    {
        simple_function_5ms();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "simple_function_5ms");
    CHECK(stats.calls == 100);
    CHECK(stats.threads == 1);
    CHECK(stats.line > 0);

    
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(5)));
    CHECK(test_helpers::within_tolerance(stats.center_med, std::chrono::milliseconds(5)));
    CHECK(stats.center_min <= stats.center_mean);
    CHECK(stats.center_mean <= stats.center_max);
    CHECK(stats.fastest_min <= stats.fastest_mean);
    CHECK(stats.fastest_mean <= stats.center_mean);
    CHECK(stats.center_mean <= stats.slowest_mean);
    CHECK(stats.slowest_mean <= stats.slowest_max);

    // Verify accumulated time
    CHECK(test_helpers::within_tolerance(stats.time_acc, std::chrono::milliseconds(500)));

    // CV should be relatively low for consistent timing
    CHECK(stats.cv >= 0.0);
    CHECK(stats.cv < 1.0); // Should be less than 100%
}

TEST_CASE("Basic single function tracking - 10ms sleep")
{
    test_helpers::clear_ctrack();

    // Execute function 5 times with 10ms sleep
    for (int i = 0; i < 5; i++)
    {
        simple_function_10ms();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "simple_function_10ms");
    CHECK(stats.calls == 5);
    CHECK(stats.threads == 1);

    // Verify timing statistics
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(10)));
    CHECK(test_helpers::within_tolerance(stats.time_acc, std::chrono::milliseconds(50), std::chrono::milliseconds(10)));

    // Verify timing relationships
    // CHECK(stats.center_min <= stats.center_max);
    // CHECK(stats.fastest_min <= stats.slowest_max);
}

TEST_CASE("Basic single function tracking - 20ms sleep")
{
    test_helpers::clear_ctrack();

    // Execute function 3 times with 20ms sleep
    for (int i = 0; i < 3; i++)
    {
        simple_function_20ms();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "simple_function_20ms");
    CHECK(stats.calls == 3);
    CHECK(stats.threads == 1);

    // Verify timing statistics
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(20)));
    CHECK(test_helpers::within_tolerance(stats.time_acc, std::chrono::milliseconds(60), std::chrono::milliseconds(6)));
}

TEST_CASE("Zero duration function tracking")
{
    test_helpers::clear_ctrack();

    // Execute zero-duration function 100 times
    for (int i = 0; i < 100; i++)
    {
        zero_duration_function();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "zero_duration_function");
    CHECK(stats.calls == 100);
    CHECK(stats.threads == 1);

    // Timing should be very small but non-negative
    CHECK(stats.time_acc.count() >= 0);
    CHECK(stats.center_mean.count() >= 0);
    CHECK(stats.center_min.count() >= 0);
    CHECK(stats.center_max.count() >= 0);

    // Should have very low execution times (less than 1ms total)
    CHECK(stats.time_acc < std::chrono::milliseconds(1));
}

TEST_CASE("Single call scenario")
{
    test_helpers::clear_ctrack();

    // Execute function only once
    simple_function_10ms();

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "simple_function_10ms");
    CHECK(stats.calls == 1);
    CHECK(stats.threads == 1);

    // For single call, min/mean/max should be very close
    CHECK(stats.center_min <= stats.center_mean);
    CHECK(stats.center_mean <= stats.center_max);
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(10)));

    // CV should be 0 or very small for single measurement
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Varying sleep times statistics")
{
    test_helpers::clear_ctrack();

    // Test with varying sleep times: 5ms, 8ms, 10ms, 15ms, 20ms
    std::vector<int> sleep_times = {5, 8, 10, 15, 20};

    for (int sleep_time : sleep_times)
    {
        varying_sleep_function(sleep_time);
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "varying_sleep_function");
    CHECK(stats.calls == 5);
    CHECK(stats.threads == 1);

    // Expected mean: (5+8+10+15+20)/5 = 11.6ms
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(12), std::chrono::milliseconds(3))); // 20% tolerance for variance

    // Expected total: 5+8+10+15+20 = 58ms
    CHECK(test_helpers::within_tolerance(stats.time_acc, std::chrono::milliseconds(58)));

    // Min should be close to 5ms, max close to 20ms
    CHECK(test_helpers::within_tolerance(stats.center_min, std::chrono::milliseconds(5) ,std::chrono::milliseconds(2)));
    CHECK(test_helpers::within_tolerance(stats.center_max, std::chrono::milliseconds(20), std::chrono::milliseconds(5)));

    // CV should be higher due to variance
    CHECK(stats.cv > 0.2); // Should have significant coefficient of variation
}

TEST_CASE("Nested function calls tracking")
{
    test_helpers::clear_ctrack();

    // Execute nested function calls 5 times
    for (int i = 0; i < 5; i++)
    {
        nested_parent_function();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 2); // Parent and child functions

    // Find parent and child functions in results
    const ctrack::detail_stats *parent_stats = nullptr;
    const ctrack::detail_stats *child_stats = nullptr;

    for (const auto &stats : tables.details.rows)
    {
        if (stats.function_name == "nested_parent_function")
        {
            parent_stats = &stats;
        }
        else if (stats.function_name == "nested_child_function")
        {
            child_stats = &stats;
        }
    }

    REQUIRE(parent_stats != nullptr);
    REQUIRE(child_stats != nullptr);

    // Verify parent function
    CHECK(parent_stats->calls == 5);
    CHECK(parent_stats->threads == 1);
    // Parent function: 5ms own + child call time
    CHECK(parent_stats->center_mean >= std::chrono::milliseconds(8)); // At least 8ms (5ms + 3ms)

    // Verify child function
    CHECK(child_stats->calls == 5);
    CHECK(child_stats->threads == 1);
    CHECK(test_helpers::within_tolerance(child_stats->center_mean, std::chrono::milliseconds(5)));

    // Parent should take longer than child (includes child execution)
    CHECK(parent_stats->center_mean > child_stats->center_mean);
}

TEST_CASE("Recursive function calls tracking")
{
    test_helpers::clear_ctrack();

    // Calculate factorial of 4 (should make 4 recursive calls: 4, 3, 2, 1)
    int result = recursive_factorial(4);
    CHECK(result == 24); // 4! = 24

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify basic properties
    CHECK(stats.function_name == "recursive_factorial");
    CHECK(stats.calls == 4); // Called for n=4, 3, 2, 1
    CHECK(stats.threads == 1);

    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::milliseconds(12)));

	// Total time should be around 5+10+15+20 = 50ms (approx)
    CHECK(test_helpers::within_tolerance(stats.time_acc, std::chrono::milliseconds(50)));

    // All calls should have similar timing
    CHECK(stats.cv < 0.5); // Should have low variance
}

TEST_CASE("Multiple different functions tracking")
{
    test_helpers::clear_ctrack();

    // Call different functions
    simple_function_5ms();
    simple_function_10ms();
    simple_function_20ms();
    zero_duration_function();

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 4);

    // Verify we have all four functions
    std::set<std::string> function_names;
    for (const auto &stats : tables.details.rows)
    {
        function_names.insert(stats.function_name);

        // All should have exactly 1 call
        CHECK(stats.calls == 1);
        CHECK(stats.threads == 1);
        CHECK(stats.line > 0);
    }

    CHECK(function_names.count("simple_function_5ms") == 1);
    CHECK(function_names.count("simple_function_10ms") == 1);
    CHECK(function_names.count("simple_function_20ms") == 1);
    CHECK(function_names.count("zero_duration_function") == 1);
}

TEST_CASE("Summary table validation")
{
    test_helpers::clear_ctrack();

    // Execute multiple functions with known patterns
    for (int i = 0; i < 10; i++)
    {
        simple_function_5ms();
    }
    for (int i = 0; i < 5; i++)
    {
        simple_function_10ms();
    }

    auto tables = ctrack::result_get_tables();

    // Verify summary table has same number of entries as detail table
    CHECK(tables.summary.rows.size() == tables.details.rows.size());
    CHECK(tables.summary.rows.size() == 2);

    // Verify summary table entries
    for (const auto &summary_row : tables.summary.rows)
    {
        CHECK(summary_row.line > 0);
        CHECK(summary_row.calls > 0);
        CHECK(summary_row.percent_ae_bracket >= 0.0);
        CHECK(summary_row.percent_ae_bracket <= 100.0);
        CHECK(summary_row.percent_ae_all >= 0.0);
        CHECK(summary_row.percent_ae_all <= 100.0);
        CHECK(summary_row.time_ae_all.count() >= 0);
        CHECK(summary_row.time_a_all.count() >= 0);

        // Function name should be one of our test functions
        CHECK((summary_row.function_name == "simple_function_5ms" ||
               summary_row.function_name == "simple_function_10ms"));
    }
}

TEST_CASE("Timing relationships validation")
{
    test_helpers::clear_ctrack();

    // Execute function multiple times to get meaningful statistics
    for (int i = 0; i < 100; i++)
    {
        simple_function_10ms();
    }

    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);

    const auto &stats = tables.details.rows[0];

    // Verify all timing relationships
    CHECK(stats.fastest_min <= stats.fastest_mean);
    CHECK(stats.fastest_mean <= stats.center_min);
    CHECK(stats.center_min <= stats.center_mean);
	CHECK(test_helpers::within_tolerance(stats.center_med, stats.center_mean, std::chrono::milliseconds(1)));
    CHECK(stats.center_med <= stats.center_max);
    CHECK(stats.center_max <= stats.slowest_mean);
    CHECK(stats.slowest_mean <= stats.slowest_max);

    // Time accumulation relationships
    CHECK(stats.center_time_a <= stats.time_acc);
    CHECK(stats.center_time_ae <= stats.center_time_a);

    // Standard deviation should be non-negative
    CHECK(stats.sd.count() >= 0);

    // Coefficient of variation should be non-negative
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Meta information validation")
{
    auto start_time = std::chrono::high_resolution_clock::now();
    test_helpers::clear_ctrack();

    // Execute some functions
    simple_function_5ms();
    simple_function_10ms();

    auto tables = ctrack::result_get_tables();
    auto end_time = std::chrono::high_resolution_clock::now();

    // Verify meta information
    CHECK(tables.start_time <= tables.end_time);
    CHECK(tables.start_time >= start_time);
    CHECK(tables.end_time <= end_time);

    CHECK(tables.time_total.count() > 0);
    CHECK(tables.time_ctracked.count() > 0);
    CHECK(tables.time_ctracked <= tables.time_total);

    // Should have tracked approximately 15ms (5ms + 10ms)
    CHECK(test_helpers::within_tolerance(tables.time_ctracked, std::chrono::milliseconds(15), std::chrono::milliseconds(5)));
}