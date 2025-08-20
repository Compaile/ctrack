#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "test_helpers.hpp"
#include "ctrack.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// Helper function to test statistical calculations with known timing patterns
void test_function_with_sleep(int sleep_ms) {
    CTRACK;
    test_helpers::sleep_ms(sleep_ms);
}

TEST_CASE("High variance timing statistics") {
    test_helpers::clear_ctrack();
    
    // Test with high variance sleep times from 5ms to 50ms
    std::vector<int> sleep_times = {5, 8, 10, 20, 50, 10, 5, 30, 15, 6};
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    test_helpers::ExpectedStats expected(sleep_times);
    
    // Basic validation
    CHECK(stats.calls == static_cast<int>(sleep_times.size()));
    CHECK(stats.function_name == "test_function_with_sleep");
    CHECK(stats.threads == 1);
    
    // Timing validation with generous tolerance for sleep timing variability
    CHECK(test_helpers::within_tolerance(stats.center_min, expected.min, 30.0));
    CHECK(test_helpers::within_tolerance(stats.center_max, expected.max, 30.0));
    CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 25.0));
    
    // Statistical measures validation
    CHECK(test_helpers::within_tolerance(static_cast<double>(stats.sd.count()), expected.std_dev_ns, 35.0));
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 35.0));
    
    // Verify CV = sd/mean relationship
    double calculated_cv = static_cast<double>(stats.sd.count()) / static_cast<double>(stats.center_mean.count());
    CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 5.0));
    
    // Logical relationships
    CHECK(stats.center_min <= stats.center_mean);
    CHECK(stats.center_mean <= stats.center_max);
    CHECK(stats.sd.count() >= 0);
    CHECK(stats.cv >= 0.0);
}

TEST_CASE("Low variance timing statistics") {
    test_helpers::clear_ctrack();
    
    // Test with low variance - all times similar (10ms +/- 1ms)
    std::vector<int> sleep_times = {9, 10, 11, 10, 9, 10, 11, 10, 9, 10};
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    test_helpers::ExpectedStats expected(sleep_times);
    
    // Basic validation
    CHECK(stats.calls == static_cast<int>(sleep_times.size()));
    
    // Low variance should result in low CV
    CHECK(stats.cv < 0.5); // CV should be relatively low for similar times
    
    // Standard deviation should be relatively small
    double mean_val = static_cast<double>(stats.center_mean.count());
    double sd_val = static_cast<double>(stats.sd.count());
    CHECK(sd_val / mean_val < 0.5); // SD should be less than 50% of mean for low variance
    
    // Verify statistical calculations
    CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 20.0));
    CHECK(test_helpers::within_tolerance(static_cast<double>(stats.sd.count()), expected.std_dev_ns, 30.0));
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 30.0));
}

TEST_CASE("Zero variance - identical timing") {
    test_helpers::clear_ctrack();
    
    // All identical sleep times
    std::vector<int> sleep_times = {10, 10, 10, 10, 10};
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    // With identical times, min, mean, and max should be very close
    CHECK(test_helpers::within_tolerance(stats.center_min, stats.center_mean, 10.0));
    CHECK(test_helpers::within_tolerance(stats.center_mean, stats.center_max, 10.0));
    
    // CV should be very low (approaching zero) for identical times
    CHECK(stats.cv < 0.2); // Very low coefficient of variation
    
    // Standard deviation should be very low for identical times
    double mean_val = static_cast<double>(stats.center_mean.count());
    double sd_val = static_cast<double>(stats.sd.count());
    CHECK(sd_val / mean_val < 0.2); // Very low relative standard deviation
}

TEST_CASE("Bimodal distribution timing") {
    test_helpers::clear_ctrack();
    
    // Two distinct clusters: fast (5ms) and slow (25ms)
    std::vector<int> sleep_times = {5, 5, 5, 5, 25, 25, 25, 25};
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    test_helpers::ExpectedStats expected(sleep_times);
    
    // Bimodal distributions typically have high variance
    CHECK(stats.cv > 0.3); // Should have relatively high CV due to bimodal nature
    
    // Min and max should span the two clusters
    CHECK(test_helpers::within_tolerance(stats.center_min, std::chrono::nanoseconds(5 * 1000000), 25.0));
    CHECK(test_helpers::within_tolerance(stats.center_max, std::chrono::nanoseconds(25 * 1000000), 25.0));
    
    // Mean should be between the two clusters
    CHECK(stats.center_mean > std::chrono::nanoseconds(10 * 1000000)); // Greater than fast cluster
    CHECK(stats.center_mean < std::chrono::nanoseconds(20 * 1000000)); // Less than slow cluster
    
    // Verify statistical calculations
    CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 25.0));
    CHECK(test_helpers::within_tolerance(static_cast<double>(stats.sd.count()), expected.std_dev_ns, 35.0));
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 35.0));
}

TEST_CASE("Percentile calculations with different settings") {
 
    
    // Use a wide range of sleep times for percentile testing
    std::vector<int> sleep_times = {5, 7, 10, 12, 15, 20, 25, 30, 45, 60};

    
    // Test with default settings (5% exclusion on each end)
    {
        test_helpers::clear_ctrack();
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }

        ctrack::ctrack_result_settings settings;
        settings.non_center_percent = 5;
        auto tables = ctrack::result_get_tables(settings);
        REQUIRE(tables.details.rows.size() == 1);
        const auto& stats = tables.details.rows[0];
        
        // With 5% on each side center is still all events
        CHECK(stats.center_min < std::chrono::nanoseconds(7 * 1000000)); 
        CHECK(stats.center_max > std::chrono::nanoseconds(60 * 1000000)); 
        
        // Percentile ranges should reflect the settings
        CHECK(stats.fastest_range == 5);
        CHECK(stats.slowest_range == 95);
    }
    
    // Test with 10% exclusion
    {
        test_helpers::clear_ctrack();
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }

        ctrack::ctrack_result_settings settings;
        settings.non_center_percent = 10;
        auto tables = ctrack::result_get_tables(settings);
        REQUIRE(tables.details.rows.size() == 1);
        const auto& stats = tables.details.rows[0];
        
        // With 10% exclusion on 10 samples, should exclude more outliers
        CHECK(stats.fastest_range == 10);
        CHECK(stats.slowest_range == 90);
        
        // Center range should be even more constrained
        CHECK(stats.center_min >= std::chrono::nanoseconds(6 * 1000000)); // Should exclude 5ms
        CHECK(stats.center_max <= std::chrono::nanoseconds(60 * 1000000)); // Should exclude 60ms
    }
    
    // Test with 1% exclusion (minimal outlier removal)
    {
        test_helpers::clear_ctrack();
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }

        ctrack::ctrack_result_settings settings;
        settings.non_center_percent = 1;
        auto tables = ctrack::result_get_tables(settings);
        REQUIRE(tables.details.rows.size() == 1);
        const auto& stats = tables.details.rows[0];
        
        CHECK(stats.fastest_range == 1);
        CHECK(stats.slowest_range == 99);
        
        // With 1% exclusion, center should be very close to full range
        CHECK(test_helpers::within_tolerance(stats.center_min, std::chrono::nanoseconds(5 * 1000000), 30.0));
        CHECK(test_helpers::within_tolerance(stats.center_max, std::chrono::nanoseconds(60 * 1000000), 30.0));
    }
}

TEST_CASE("Outlier handling verification") {
    test_helpers::clear_ctrack();
    
    // Create dataset with clear outliers
    std::vector<int> sleep_times = {100, 10, 10, 10, 10, 10, 10, 10, 10, 5}; // 100ms and 5ms are outliers
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    // Test with 10% exclusion to remove outliers
    ctrack::ctrack_result_settings settings;
    settings.non_center_percent = 10;
    auto tables = ctrack::result_get_tables(settings);
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    // Center statistics should exclude the extreme outliers
    CHECK(stats.center_min >= std::chrono::nanoseconds(5 * 1000000)); // Should exclude 5ms
    CHECK(stats.center_max < std::chrono::nanoseconds(50 * 1000000)); // Should exclude 100ms
    
    // Center mean should be close to 10ms (the dominant value)
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::nanoseconds(10 * 1000000), 25.0));
    
    
    // Verify that fastest and slowest stats capture the outliers
    CHECK(stats.fastest_min <= std::chrono::nanoseconds(6 * 1000000)); // Should include 5ms
    CHECK(stats.fastest_min >= std::chrono::nanoseconds(5 * 1000000)); // Should include 5ms
    CHECK(stats.slowest_max >= std::chrono::nanoseconds(50 * 1000000)); // Should include 100ms
}

TEST_CASE("Statistical consistency across multiple calls") {
    test_helpers::clear_ctrack();
    
    // Test with systematic pattern
    std::vector<int> sleep_times = {5, 10, 15, 20, 25};
    
    // Call each timing multiple times to increase sample size
    for (int repeat = 0; repeat < 4; ++repeat) {
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    // Should have 20 total calls (4 repeats * 5 different times)
    CHECK(stats.calls == 20);
    
    // Statistical measures should be consistent with the pattern
    test_helpers::ExpectedStats expected({5, 10, 15, 20, 25}); // Use single instance for expected
    
    // Mean should be around 15ms (middle value)
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::nanoseconds(15 * 1000000), 25.0));
    
    // CV should reflect the systematic spread
    CHECK(stats.cv > 0.2); // Should have reasonable variance
    CHECK(stats.cv < 0.8); // But not excessive
    
    // Verify statistical calculations remain consistent
    double calculated_cv = static_cast<double>(stats.sd.count()) / static_cast<double>(stats.center_mean.count());
    CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 5.0));
}

TEST_CASE("Edge case: single execution") {
    test_helpers::clear_ctrack();
    
    // Single execution
    test_function_with_sleep(10);
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    CHECK(stats.calls == 1);
    
    // With single execution, min == mean == max
    CHECK(stats.center_min == stats.center_mean);
    CHECK(stats.center_mean == stats.center_max);
    
    // Standard deviation should be zero or very close to zero
    CHECK(stats.sd.count() < 1000000); // Less than 1ms (should be 0 or very small)
    
    // CV should be zero or very close to zero for single execution
    CHECK(stats.cv < 0.01); // Very low CV for single execution
    
}

TEST_CASE("Statistical validation with extreme values") {
    test_helpers::clear_ctrack();
    
    // Test with extreme timing differences
    std::vector<int> sleep_times = {5, 6, 100, 5, 6}; // Mix of very fast and one very slow
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    test_helpers::ExpectedStats expected(sleep_times);
    
    // Should have very high CV due to extreme difference
    CHECK(stats.cv > 1.0); // CV > 100% indicates high variability
    
    // Standard deviation should be large relative to mean
    double mean_val = static_cast<double>(stats.center_mean.count());
    double sd_val = static_cast<double>(stats.sd.count());
    CHECK(sd_val > mean_val * 0.5); // SD should be significant portion of mean
    
    // Verify calculations with generous tolerance due to extreme values
    CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 40.0));
    CHECK(test_helpers::within_tolerance(static_cast<double>(stats.sd.count()), expected.std_dev_ns, 50.0));
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 50.0));
    
    // CV consistency check
    double calculated_cv = sd_val / mean_val;
    CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 10.0));
}

TEST_CASE("Percentile range validation") {
    test_helpers::clear_ctrack();
    
    // Create dataset with 20 samples for clear percentile testing
    std::vector<int> sleep_times;
    for (int i = 1; i <= 20; ++i) {
        sleep_times.push_back(i + 4); // 5ms to 24ms
    }
    
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    // Test 5% exclusion (should exclude 1 sample from each end with 20 samples)
    ctrack::ctrack_result_settings settings;
    settings.non_center_percent = 5;
    auto tables = ctrack::result_get_tables(settings);
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    // With 5% exclusion on 20 samples, should exclude fastest (1ms) and slowest (24ms)
    CHECK(stats.center_min > std::chrono::nanoseconds(1 * 1000000));
    CHECK(stats.center_max < std::chrono::nanoseconds(24 * 1000000));
    
    // Center mean should be around 14.5ms 
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::nanoseconds(14500000), 20.0));
    
    // Fastest and slowest should capture the extremes
    CHECK(test_helpers::within_tolerance(stats.fastest_min, std::chrono::nanoseconds(5 * 1000000), 25.0));
    CHECK(test_helpers::within_tolerance(stats.slowest_max, std::chrono::nanoseconds(24 * 1000000), 25.0));
}

TEST_CASE("Complex bimodal distribution with statistical validation") {
    test_helpers::clear_ctrack();
    
    // Create a more complex bimodal distribution
    // Fast cluster: 3-7ms, Slow cluster: 30-40ms
    std::vector<int> sleep_times = {
        3, 4, 5, 6, 7,           // Fast cluster
        30, 32, 35, 38, 40,     // Slow cluster
        4, 5, 6,                // More fast samples
        31, 36, 39              // More slow samples
    };
    
    for (int ms : sleep_times) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    test_helpers::ExpectedStats expected(sleep_times);
    
    // Bimodal should have high CV due to two distinct clusters
    CHECK(stats.cv > 0.4); // High coefficient of variation
    
    // Mean should be between the clusters but closer to center
    CHECK(stats.center_mean > std::chrono::nanoseconds(15 * 1000000));
    CHECK(stats.center_mean < std::chrono::nanoseconds(25 * 1000000));
    
    // Standard deviation should be significant
    double mean_val = static_cast<double>(stats.center_mean.count());
    double sd_val = static_cast<double>(stats.sd.count());
    CHECK(sd_val > mean_val * 0.3); // SD should be substantial portion of mean
    
    // Verify statistical accuracy with generous tolerance for bimodal
    CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 30.0));
    CHECK(test_helpers::within_tolerance(static_cast<double>(stats.sd.count()), expected.std_dev_ns, 40.0));
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 40.0));
    
    // CV formula verification
    double calculated_cv = sd_val / mean_val;
    CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 5.0));
}

TEST_CASE("Extreme outlier impact on percentile exclusion") {
 
    
    // Dataset with extreme outliers to test percentile robustness
    std::vector<int> sleep_times = {
        10, 10, 10, 10, 10,     // Normal cluster
        10, 10, 10, 10, 10,     // More normal
        1,                       // Extreme fast outlier
        500                      // Extreme slow outlier
    };
    
  
    // Test different exclusion levels
    std::vector<double> exclusion_levels = {1.0, 5.0, 10.0, 20.0};
    
    for (double exclusion : exclusion_levels) {
        test_helpers::clear_ctrack();
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }

        ctrack::ctrack_result_settings settings;
        settings.non_center_percent = static_cast<unsigned int>(exclusion);
        auto tables = ctrack::result_get_tables(settings);
        REQUIRE(tables.details.rows.size() == 1);
        const auto& stats = tables.details.rows[0];
        
        // Higher exclusion should result in center stats closer to 10ms
        if (exclusion >= 10.0) {
            // Should exclude outliers effectively
            CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::nanoseconds(10 * 1000000), 15.0));
        }
        
        // Verify percentile ranges are set correctly
        CHECK(stats.fastest_range == static_cast<int>(exclusion));
        CHECK(stats.slowest_range == static_cast<int>(100-exclusion));
        
        // Logical consistency checks
        CHECK(stats.center_min <= stats.center_mean);
        CHECK(stats.center_mean <= stats.center_max);

    }
}

TEST_CASE("Progressive variance analysis") {
    test_helpers::clear_ctrack();
    
    // Test with progressively increasing variance
    std::vector<std::vector<int>> variance_tests = {
        {10, 10, 10, 10, 10},                    // No variance
        {9, 10, 10, 10, 11},                     // Very low variance
        {8, 9, 10, 11, 12},                      // Low variance
        {5, 8, 10, 12, 15},                      // Medium variance
        {2, 6, 10, 14, 18},                      // High variance
        {1, 3, 10, 17, 20}                       // Very high variance
    };
    
    std::vector<double> expected_cv_progression;
    
    for (size_t test_idx = 0; test_idx < variance_tests.size(); ++test_idx) {
        test_helpers::clear_ctrack();
        
        const auto& sleep_times = variance_tests[test_idx];
        for (int ms : sleep_times) {
            test_function_with_sleep(ms);
        }
        
        auto tables = ctrack::result_get_tables();
        REQUIRE(tables.details.rows.size() == 1);
        const auto& stats = tables.details.rows[0];
        
        test_helpers::ExpectedStats expected(sleep_times);
        
        // Verify statistical calculations
        CHECK(test_helpers::within_tolerance(stats.center_mean, expected.mean, 25.0));
        CHECK(test_helpers::within_tolerance_absolute(stats.sd.count(), static_cast<int64_t>( expected.std_dev_ns), 10*1000000));
        CHECK(std::abs(stats.cv- expected.cv) <  0.05);
        
        // Store CV for progression analysis
        expected_cv_progression.push_back(stats.cv);
        
        // CV should increase with variance (except for first case which might be ~0)
        if (test_idx > 0) {
            // Each test should have higher or equal CV than the previous
            // (allowing some tolerance for measurement variance)
            CHECK(stats.cv >= expected_cv_progression[test_idx - 1] - 0.1);
        }
        
        // Verify CV formula consistency
        double mean_val = static_cast<double>(stats.center_mean.count());
        double sd_val = static_cast<double>(stats.sd.count());
        if (mean_val > 0) {
            double calculated_cv = sd_val / mean_val;
            CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 5.0));
        }
    }
    
    // Overall progression check: CV should generally increase
    CHECK(expected_cv_progression.back() > expected_cv_progression.front());
}

TEST_CASE("Statistical stability with large sample sizes") {
    test_helpers::clear_ctrack();
    
    // Test statistical stability with 100 samples
    std::vector<int> base_pattern = {8, 10, 12}; // Simple pattern
    std::vector<int> large_sample;
    
    // Repeat pattern to create 99 samples (33 of each)
    for (int repeat = 0; repeat < 33; ++repeat) {
        for (int ms : base_pattern) {
            large_sample.push_back(ms);
        }
    }
    
    for (int ms : large_sample) {
        test_function_with_sleep(ms);
    }
    
    auto tables = ctrack::result_get_tables();
    REQUIRE(tables.details.rows.size() == 1);
    const auto& stats = tables.details.rows[0];
    
    CHECK(stats.calls == 99);
    
    // With large sample, statistics should be very stable
    // Mean should be exactly 10ms (perfect average of 8, 10, 12)
    CHECK(test_helpers::within_tolerance(stats.center_mean, std::chrono::nanoseconds(10 * 1000000), 15.0));
    
    // CV should be consistent with the pattern
    test_helpers::ExpectedStats expected(base_pattern);
    CHECK(test_helpers::within_tolerance(stats.cv, expected.cv, 20.0));
    
    // Standard deviation should be stable
    double mean_val = static_cast<double>(stats.center_mean.count());
    double sd_val = static_cast<double>(stats.sd.count());
    double calculated_cv = sd_val / mean_val;
    CHECK(test_helpers::within_tolerance(stats.cv, calculated_cv, 3.0)); // Very tight tolerance for large sample
    
    // Min and max should match the pattern
    CHECK(test_helpers::within_tolerance(stats.center_min, std::chrono::nanoseconds(8 * 1000000), 20.0));
    CHECK(test_helpers::within_tolerance(stats.center_max, std::chrono::nanoseconds(12 * 1000000), 20.0));
}