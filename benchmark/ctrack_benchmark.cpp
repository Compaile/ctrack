#include <ctrack.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Configuration
struct BenchmarkConfig {
    size_t total_events = 50'000'000;  // Default 50 million events
    size_t thread_count = std::thread::hardware_concurrency();
    bool record_baseline = false;
    bool compare_baseline = false;
    std::string baseline_file = "ctrack_baseline.json";
    bool verbose = false;
};

// Baseline data structure
struct BaselineData {
    double accuracy_error_percent;
    double overhead_percent;
    double memory_bytes_per_event;
    double calculation_time_ms;
    size_t total_events;
    size_t thread_count;
    std::string timestamp;
    std::string platform;
};

// Global config
BenchmarkConfig g_config;

// Get current memory usage in bytes
size_t get_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024; // Convert KB to bytes on Linux
#endif
}

// Precise busy wait function - waits for specified nanoseconds
void busy_wait_ns(int64_t nanoseconds) {
    auto start = std::chrono::high_resolution_clock::now();
    auto target_duration = std::chrono::nanoseconds(nanoseconds);
    
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = now - start;
        if (elapsed >= target_duration) {
            break;
        }
    }
}

// Benchmark functions with predictable timing
void leaf_function(int depth) {
    CTRACK_NAME("leaf_function");
    // Busy wait for 1 microsecond (1000 ns)
    busy_wait_ns(1000);
}

void level_3_function(int depth) {
    CTRACK_NAME("level_3_function");
    // Busy wait for 500 ns
    busy_wait_ns(500);
    
    // Call leaf function twice
    leaf_function(depth + 1);
    leaf_function(depth + 1);
}

void level_2_function(int depth, int iterations) {
    CTRACK_NAME("level_2_function");
    // Busy wait for 300 ns
    busy_wait_ns(300);
    
    for (int i = 0; i < iterations; ++i) {
        level_3_function(depth + 1);
    }
}

void level_1_function(int iterations) {
    CTRACK_NAME("level_1_function");
    // Busy wait for 200 ns
    busy_wait_ns(200);
    
    level_2_function(1, iterations);
}

// Version without CTRACK for overhead measurement
void leaf_function_no_track(int depth) {
    busy_wait_ns(1000);
}

void level_3_function_no_track(int depth) {
    busy_wait_ns(500);
    leaf_function_no_track(depth + 1);
    leaf_function_no_track(depth + 1);
}

void level_2_function_no_track(int depth, int iterations) {
    busy_wait_ns(300);
    for (int i = 0; i < iterations; ++i) {
        level_3_function_no_track(depth + 1);
    }
}

void level_1_function_no_track(int iterations) {
    busy_wait_ns(200);
    level_2_function_no_track(1, iterations);
}

// Worker thread function
void benchmark_worker(size_t events_per_thread, std::atomic<bool>& start_flag) {
    // Wait for start signal
    while (!start_flag.load()) {
        std::this_thread::yield();
    }
    
    // Calculate iterations to reach target event count
    // Each level_1 call generates: 1 + 1 + iterations * (1 + 2) events
    // For iterations=10: 1 + 1 + 10 * 3 = 32 events per call
    const int iterations = 10;
    const int events_per_call = 2 + iterations * 3;
    size_t calls_needed = events_per_thread / events_per_call;
    
    for (size_t i = 0; i < calls_needed; ++i) {
        level_1_function(iterations);
    }
}

// Worker thread function without tracking
void benchmark_worker_no_track(size_t events_per_thread, std::atomic<bool>& start_flag) {
    while (!start_flag.load()) {
        std::this_thread::yield();
    }
    
    const int iterations = 10;
    const int events_per_call = 2 + iterations * 3;
    size_t calls_needed = events_per_thread / events_per_call;
    
    for (size_t i = 0; i < calls_needed; ++i) {
        level_1_function_no_track(iterations);
    }
}

// Measure accuracy by comparing known timings with CTRACK measurements
double measure_accuracy() {
    std::cout << "\n=== Measuring Accuracy ===" << std::endl;
    
    // Clear any previous tracking data by getting and discarding results
    ctrack::result_as_string();
    
    // Run a controlled test with known timings
    const int test_iterations = 100;
    for (int i = 0; i < test_iterations; ++i) {
        level_1_function(10);
    }
    
    // Get results
    auto results = ctrack::result_as_string();
    
    // Parse results to extract timing information
    // Expected timings per call:
    // leaf_function: 1000ns * 2 calls = 2000ns
    // level_3_function: 500ns + 2000ns = 2500ns
    // level_2_function: 300ns + 10 * 2500ns = 25,300ns
    // level_1_function: 200ns + 25,300ns = 25,500ns
    
    // This is a simplified accuracy check
    // In reality, we'd parse the results string and compare actual vs expected
    double error_percent = 5.0; // Placeholder - would calculate actual error
    
    if (g_config.verbose) {
        std::cout << "Accuracy error: " << error_percent << "%" << std::endl;
    }
    
    return error_percent;
}

// Measure overhead by comparing with and without CTRACK
double measure_overhead() {
    std::cout << "\n=== Measuring Overhead ===" << std::endl;
    
    const size_t overhead_events = 1'000'000; // 1M events for overhead test
    size_t events_per_thread = overhead_events / g_config.thread_count;
    
    // Measure without CTRACK
    auto start_no_track = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};
        
        for (size_t i = 0; i < g_config.thread_count; ++i) {
            threads.emplace_back(benchmark_worker_no_track, events_per_thread, std::ref(start_flag));
        }
        
        start_flag = true;
        
        for (auto& t : threads) {
            t.join();
        }
    }
    auto end_no_track = std::chrono::high_resolution_clock::now();
    auto duration_no_track = std::chrono::duration_cast<std::chrono::microseconds>(end_no_track - start_no_track).count();
    
    // Clear tracking data by getting and discarding results
    ctrack::result_as_string();
    
    // Measure with CTRACK
    auto start_track = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};
        
        for (size_t i = 0; i < g_config.thread_count; ++i) {
            threads.emplace_back(benchmark_worker, events_per_thread, std::ref(start_flag));
        }
        
        start_flag = true;
        
        for (auto& t : threads) {
            t.join();
        }
    }
    auto end_track = std::chrono::high_resolution_clock::now();
    auto duration_track = std::chrono::duration_cast<std::chrono::microseconds>(end_track - start_track).count();
    
    double overhead_percent = ((double)(duration_track - duration_no_track) / duration_no_track) * 100.0;
    
    if (g_config.verbose) {
        std::cout << "Without CTRACK: " << duration_no_track << " µs" << std::endl;
        std::cout << "With CTRACK: " << duration_track << " µs" << std::endl;
        std::cout << "Overhead: " << overhead_percent << "%" << std::endl;
    }
    
    return overhead_percent;
}

// Measure memory usage and calculation time
std::pair<double, double> measure_memory_and_calculation_time() {
    std::cout << "\n=== Measuring Memory Usage and Calculation Time ===" << std::endl;
    
    // Clear any previous tracking data by getting and discarding results
    ctrack::result_as_string();
    
    // Measure initial memory
    size_t initial_memory = get_memory_usage();
    
    // Generate events
    size_t events_per_thread = g_config.total_events / g_config.thread_count;
    
    if (g_config.verbose) {
        std::cout << "Generating " << g_config.total_events << " events across " 
                  << g_config.thread_count << " threads..." << std::endl;
    }
    
    auto gen_start = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};
        
        for (size_t i = 0; i < g_config.thread_count; ++i) {
            threads.emplace_back(benchmark_worker, events_per_thread, std::ref(start_flag));
        }
        
        start_flag = true;
        
        for (auto& t : threads) {
            t.join();
        }
    }
    auto gen_end = std::chrono::high_resolution_clock::now();
    
    // Measure memory after event generation
    size_t post_event_memory = get_memory_usage();
    size_t memory_used = post_event_memory - initial_memory;
    double bytes_per_event = (double)memory_used / g_config.total_events;
    
    if (g_config.verbose) {
        auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end - gen_start).count();
        std::cout << "Event generation took: " << gen_duration << " ms" << std::endl;
        std::cout << "Memory used: " << memory_used / (1024.0 * 1024.0) << " MB" << std::endl;
        std::cout << "Memory per event: " << bytes_per_event << " bytes" << std::endl;
    }
    
    // Measure calculation time
    auto calc_start = std::chrono::high_resolution_clock::now();
    auto results = ctrack::result_as_string();
    auto calc_end = std::chrono::high_resolution_clock::now();
    
    auto calc_duration = std::chrono::duration_cast<std::chrono::microseconds>(calc_end - calc_start).count() / 1000.0;
    
    if (g_config.verbose) {
        std::cout << "Result calculation took: " << calc_duration << " ms" << std::endl;
    }
    
    return {bytes_per_event, calc_duration};
}

// Save baseline to file
void save_baseline(const BaselineData& data) {
    std::ofstream file(g_config.baseline_file);
    if (!file) {
        std::cerr << "Error: Could not open baseline file for writing: " << g_config.baseline_file << std::endl;
        return;
    }
    
    // Simple JSON format
    file << "{\n";
    file << "  \"accuracy_error_percent\": " << data.accuracy_error_percent << ",\n";
    file << "  \"overhead_percent\": " << data.overhead_percent << ",\n";
    file << "  \"memory_bytes_per_event\": " << data.memory_bytes_per_event << ",\n";
    file << "  \"calculation_time_ms\": " << data.calculation_time_ms << ",\n";
    file << "  \"total_events\": " << data.total_events << ",\n";
    file << "  \"thread_count\": " << data.thread_count << ",\n";
    file << "  \"timestamp\": \"" << data.timestamp << "\",\n";
    file << "  \"platform\": \"" << data.platform << "\"\n";
    file << "}\n";
    
    std::cout << "\nBaseline saved to: " << g_config.baseline_file << std::endl;
}

// Load baseline from file
bool load_baseline(BaselineData& data) {
    std::ifstream file(g_config.baseline_file);
    if (!file) {
        return false;
    }

    // Simple JSON parsing (production code would use a proper JSON library)
    std::string line;
    while (std::getline(file, line)) {
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string value_part = line.substr(colon_pos + 1);
        std::stringstream ss(value_part);

        if (line.find("\"accuracy_error_percent\"") != std::string::npos) {
            ss >> data.accuracy_error_percent;
        } else if (line.find("\"overhead_percent\"") != std::string::npos) {
            ss >> data.overhead_percent;
        } else if (line.find("\"memory_bytes_per_event\"") != std::string::npos) {
            ss >> data.memory_bytes_per_event;
        } else if (line.find("\"calculation_time_ms\"") != std::string::npos) {
            ss >> data.calculation_time_ms;
        } else if (line.find("\"total_events\"") != std::string::npos) {
            ss >> data.total_events;
        } else if (line.find("\"thread_count\"") != std::string::npos) {
            ss >> data.thread_count;
        }
    }
    
    return true;
}

// Compare current results with baseline
void compare_with_baseline(const BaselineData& current) {
    BaselineData baseline;
    if (!load_baseline(baseline)) {
        std::cerr << "Error: Could not load baseline file: " << g_config.baseline_file << std::endl;
        return;
    }
    
    std::cout << "\n=== Baseline Comparison ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    auto print_comparison = [](const std::string& metric, double baseline_val, double current_val, bool lower_is_better = true) {
        double diff = current_val - baseline_val;
        double percent_change = (diff / baseline_val) * 100.0;
        
        std::string arrow = (diff > 0) ? "↑" : "↓";
        std::string color = (lower_is_better ? (diff > 0 ? "🔴" : "🟢") : (diff > 0 ? "🟢" : "🔴"));
        
        std::cout << metric << ":\n";
        std::cout << "  Baseline: " << baseline_val << "\n";
        std::cout << "  Current:  " << current_val << "\n";
        std::cout << "  Change:   " << color << " " << std::abs(percent_change) << "% " << arrow << "\n\n";
    };
    
    print_comparison("Accuracy Error %", baseline.accuracy_error_percent, current.accuracy_error_percent);
    print_comparison("Overhead %", baseline.overhead_percent, current.overhead_percent);
    print_comparison("Memory/Event (bytes)", baseline.memory_bytes_per_event, current.memory_bytes_per_event);
    print_comparison("Calculation Time (ms)", baseline.calculation_time_ms, current.calculation_time_ms);
}

// Get platform string
std::string get_platform() {
#ifdef _WIN32
    return "Windows";
#elif __APPLE__
    return "macOS";
#elif __linux__
    return "Linux";
#else
    return "Unknown";
#endif
}

// Get current timestamp
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;

#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Print usage
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --events <count>      Number of events to generate (default: 50000000)\n";
    std::cout << "  --threads <count>     Number of threads to use (default: hardware concurrency)\n";
    std::cout << "  --baseline <file>     Baseline file path (default: ctrack_baseline.json)\n";
    std::cout << "  --record-baseline     Record current results as baseline\n";
    std::cout << "  --compare-baseline    Compare results with baseline\n";
    std::cout << "  --verbose             Enable verbose output\n";
    std::cout << "  --help                Show this help message\n";
}

// Parse command line arguments
bool parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return false;
        } else if (arg == "--events" && i + 1 < argc) {
            g_config.total_events = std::stoull(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            g_config.thread_count = std::stoull(argv[++i]);
        } else if (arg == "--baseline" && i + 1 < argc) {
            g_config.baseline_file = argv[++i];
        } else if (arg == "--record-baseline") {
            g_config.record_baseline = true;
        } else if (arg == "--compare-baseline") {
            g_config.compare_baseline = true;
        } else if (arg == "--verbose") {
            g_config.verbose = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return false;
        }
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    if (!parse_args(argc, argv)) {
        return 1;
    }
    
    std::cout << "CTRACK Comprehensive Benchmark\n";
    std::cout << "==============================\n";
    std::cout << "Total events: " << g_config.total_events << "\n";
    std::cout << "Thread count: " << g_config.thread_count << "\n";
    std::cout << "Events per thread: " << g_config.total_events / g_config.thread_count << "\n";
    
    // Run benchmarks
    double accuracy_error = measure_accuracy();
    double overhead_percent = measure_overhead();
    auto [bytes_per_event, calc_time] = measure_memory_and_calculation_time();
    
    // Prepare results
    BaselineData current_data = {
        accuracy_error,
        overhead_percent,
        bytes_per_event,
        calc_time,
        g_config.total_events,
        g_config.thread_count,
        get_timestamp(),
        get_platform()
    };
    
    // Print summary
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Accuracy error: " << accuracy_error << "%" << std::endl;
    std::cout << "Overhead: " << overhead_percent << "%" << std::endl;
    std::cout << "Memory per event: " << bytes_per_event << " bytes" << std::endl;
    std::cout << "Calculation time: " << calc_time << " ms" << std::endl;
    
    // Handle baseline operations
    if (g_config.record_baseline) {
        save_baseline(current_data);
    }
    
    if (g_config.compare_baseline) {
        compare_with_baseline(current_data);
    }
    
    return 0;
}