#pragma once

#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "ctrack.hpp"

namespace test_helpers {

    inline  void precise_busy_wait(std::chrono::nanoseconds duration) {
        auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::high_resolution_clock::now() - start < duration) {
            // Busy wait - burns CPU but very precise
        }
    }

// Sleep for a specific number of milliseconds with reasonable precision
inline void sleep_ms(int milliseconds) {
    std::chrono::milliseconds ms(milliseconds);
    precise_busy_wait(ms);
}

// Sleep for a specific number of microseconds
inline void sleep_us(int microseconds) {
	std::chrono::microseconds us(microseconds);
	precise_busy_wait(us);

}







// Simple tolerance check for nanoseconds (as int64_t)
// Relative tolerance with default 15%, minimum 1ms
inline bool within_tolerance_relative(int64_t actual_ns, int64_t expected_ns, double tolerance_percent = 20.0) {
    int64_t tolerance = std::max(
        static_cast<int64_t>(std::abs(expected_ns) * tolerance_percent / 100.0),
        int64_t(1000000)  // 1ms minimum
    );
    return std::abs(actual_ns - expected_ns) <= tolerance;
}

// Absolute tolerance in nanoseconds
inline bool within_tolerance_absolute(int64_t actual_ns, int64_t expected_ns, int64_t tolerance_ns) {
    return std::abs(actual_ns - expected_ns) <= tolerance_ns;
}



// Simple wrapper for chrono types - just call .count()
template<typename Rep1, typename Period1, typename Rep2, typename Period2>
inline bool within_tolerance(std::chrono::duration<Rep1, Period1> actual, 
                            std::chrono::duration<Rep2, Period2> expected) {
    auto actual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(actual).count();
    auto expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(expected).count();
    return within_tolerance_relative(actual_ns, expected_ns, 20.0);
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
inline bool within_tolerance(std::chrono::duration<Rep1, Period1> actual, 
                            std::chrono::duration<Rep2, Period2> expected, 
                            double tolerance_percent) {
    auto actual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(actual).count();
    auto expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(expected).count();
    return within_tolerance_relative(actual_ns, expected_ns, tolerance_percent);
}

template<typename Rep1, typename Period1, typename Rep2, typename Period2, typename Rep3, typename Period3>
inline bool within_tolerance(std::chrono::duration<Rep1, Period1> actual, 
                            std::chrono::duration<Rep2, Period2> expected,
                            std::chrono::duration<Rep3, Period3> tolerance) {
    auto actual_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(actual).count();
    auto expected_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(expected).count();
    auto tolerance_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tolerance).count();
    return within_tolerance_absolute(actual_ns, expected_ns, tolerance_ns);
}

// For doubles
inline bool within_tolerance(double actual, double expected, double tolerance_percent = 20.0) {
    if (expected == 0.0) {
        return std::abs(actual) < 0.001;
    }
    double tolerance = std::abs(expected) * (tolerance_percent / 100.0);
    return std::abs(actual - expected) <= tolerance;
}

// Thread barrier for synchronizing multiple threads
class ThreadBarrier {
public:
    explicit ThreadBarrier(size_t count) : threshold(count), count(count), generation(0) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        auto gen = generation;
        
        if (--count == 0) {
            generation++;
            count = threshold;
            cond.notify_all();
        } else {
            cond.wait(lock, [this, gen] { return gen != generation; });
        }
    }
    
private:
    std::mutex mutex;
    std::condition_variable cond;
    size_t threshold;
    size_t count;
    size_t generation;
};

// Calculate expected statistics for a series of sleep times
struct ExpectedStats {
    std::chrono::nanoseconds min;
    std::chrono::nanoseconds max;
    std::chrono::nanoseconds mean;
    std::chrono::nanoseconds median;
    std::chrono::nanoseconds total;
    double std_dev_ns;
    double cv;
    
    ExpectedStats(const std::vector<int>& sleep_times_ms) {
        if (sleep_times_ms.empty()) {
            min = max = mean = median = total = std::chrono::nanoseconds(0);
            std_dev_ns = cv = 0.0;
            return;
        }
        
        // Convert to nanoseconds
        std::vector<int64_t> times_ns;
        for (int ms : sleep_times_ms) {
            times_ns.push_back(static_cast<int64_t>(ms) * 1000000);
        }
        
        // Calculate basic stats
        auto minmax = std::minmax_element(times_ns.begin(), times_ns.end());
        min = std::chrono::nanoseconds(*minmax.first);
        max = std::chrono::nanoseconds(*minmax.second);
        
        int64_t sum = std::accumulate(times_ns.begin(), times_ns.end(), int64_t(0));
        total = std::chrono::nanoseconds(sum);
        mean = std::chrono::nanoseconds(sum / times_ns.size());
        
        // Calculate median
        std::vector<int64_t> sorted = times_ns;
        std::sort(sorted.begin(), sorted.end());
        if (sorted.size() % 2 == 0) {
            median = std::chrono::nanoseconds((sorted[sorted.size()/2 - 1] + sorted[sorted.size()/2]) / 2);
        } else {
            median = std::chrono::nanoseconds(sorted[sorted.size()/2]);
        }
        
        // Calculate standard deviation
        double mean_val = static_cast<double>(mean.count());
        double variance = 0.0;
        for (int64_t t : times_ns) {
            double diff = static_cast<double>(t) - mean_val;
            variance += diff * diff;
        }
        variance /= times_ns.size();
        std_dev_ns = std::sqrt(variance);
        
        // Calculate CV
        cv = (mean_val > 0) ? (std_dev_ns / mean_val) : 0.0;
    }
};

// Validate summary row
inline bool validate_summary_row(const ctrack::summary_row& row, 
                                  const std::string& expected_function,
                                  int expected_calls,
                                  double tolerance_percent = 15.0) {
    if (row.function_name != expected_function) return false;
    if (row.calls != expected_calls) return false;
    if (row.line <= 0) return false;
    
    // Percentages should be between 0 and 100
    if (row.percent_ae_bracket < 0 || row.percent_ae_bracket > 100) return false;
    if (row.percent_ae_all < 0 || row.percent_ae_all > 100) return false;
    
    // Times should be non-negative
    if (row.time_ae_all.count() < 0) return false;
    if (row.time_a_all.count() < 0) return false;
    
    return true;
}

// Validate detail stats
inline bool validate_detail_stats(const ctrack::detail_stats& stats,
                                   const std::string& expected_function,
                                   int expected_calls,
                                   int expected_threads = 1,
                                   double tolerance_percent = 15.0) {
    if (stats.function_name != expected_function) return false;
    if (stats.calls != expected_calls) return false;
    if (stats.threads != expected_threads) return false;
    if (stats.line <= 0) return false;
    
    // All times should be non-negative
    if (stats.time_acc.count() < 0) return false;
    if (stats.sd.count() < 0) return false;

    if (stats.center_min.count() < 0) return false;
    if (stats.center_mean.count() < 0) return false;
    if (stats.center_med.count() < 0) return false;
    if (stats.center_time_a.count() < 0) return false;
    if (stats.center_time_ae.count() < 0) return false;
    if (stats.center_max.count() < 0) return false;

    
    // CV should be non-negative
    if (stats.cv < 0) return false;

    bool does_non_center_exist = (expected_calls * stats.fastest_range / 100 )>0;

    if (does_non_center_exist) {
    
        if (stats.fastest_min.count() < 0) return false;
        if (stats.fastest_mean.count() < 0) return false;
        if (stats.slowest_mean.count() < 0) return false;
        if (stats.slowest_max.count() < 0) return false;

        // Logical relationships
        if (stats.fastest_min > stats.fastest_mean) return false;
        if (stats.fastest_mean > stats.center_mean) return false;
        if (stats.center_mean > stats.slowest_mean) return false;
        if (stats.center_min > stats.center_max) return false;
        if (stats.slowest_mean > stats.slowest_max) return false;
    
    }


    
    return true;
}

// Helper to clear ctrack data between tests
inline void clear_ctrack() {
    // Get current results to clear the internal state
    ctrack::result_get_tables();
}

// Test function with predictable timing
template<size_t N>
inline void test_function_with_sleep(int sleep_time_ms, const char(&name)[N]) {
    CTRACK_NAME(name);  // String literals have static storage duration
    sleep_ms(sleep_time_ms);
}

// Overload for empty/runtime strings
inline void test_function_with_sleep(int sleep_time_ms) {
    CTRACK;
    sleep_ms(sleep_time_ms);
}

// Nested test functions
inline void nested_level_2(int sleep_time_ms) {
    CTRACK;
    sleep_ms(sleep_time_ms);
}

inline void nested_level_1(int sleep_time_ms, int child_sleep_ms) {
    CTRACK;
    sleep_ms(sleep_time_ms);
    nested_level_2(child_sleep_ms);
}

inline void nested_root(int sleep_time_ms, int level1_sleep_ms, int level2_sleep_ms) {
    CTRACK;
    sleep_ms(sleep_time_ms);
    nested_level_1(level1_sleep_ms, level2_sleep_ms);
}

} // namespace test_helpers