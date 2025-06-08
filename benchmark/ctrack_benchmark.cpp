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
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Configuration
struct BenchmarkConfig
{
    size_t total_events = 50'000'000; // Default 50 million events
    size_t thread_count = std::thread::hardware_concurrency();
    bool record_baseline = false;
    bool compare_baseline = false;
    std::string baseline_file = "ctrack_baseline.json";
    bool verbose = false;
};

// Baseline data structure
struct BaselineData
{
    double accuracy_error_percent;
    double accuracy_error_ms_per_event;
    double overhead_percent;
    double overhead_ms;
    double overhead_ns_per_event;
    double memory_bytes_per_event;
    double calculation_time_ms;
    double peak_calc_memory_mb;
    size_t total_events;
    size_t thread_count;
    std::string timestamp;
    std::string platform;
};

// Global config
BenchmarkConfig g_config;

// Get current memory usage in bytes
size_t get_memory_usage()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024; // Convert KB to bytes on Linux
#endif
}

// Precise busy wait function - waits for specified nanoseconds
void busy_wait_ns(int64_t nanoseconds)
{
    auto start = std::chrono::high_resolution_clock::now();
    auto target_duration = std::chrono::nanoseconds(nanoseconds);

    while (true)
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = now - start;
        if (elapsed >= target_duration)
        {
            break;
        }
    }
}

// Benchmark functions with predictable timing
void leaf_function(int depth)
{
    CTRACK_NAME("leaf_function");
    // Busy wait for 1 microsecond (1000 ns)
    busy_wait_ns(1000);
}

void level_3_function(int depth)
{
    CTRACK_NAME("level_3_function");
    // Busy wait for 500 ns
    busy_wait_ns(500);

    // Call leaf function twice
    leaf_function(depth + 1);
    leaf_function(depth + 1);
}

void level_2_function(int depth, int iterations)
{
    CTRACK_NAME("level_2_function");
    // Busy wait for 300 ns
    busy_wait_ns(300);

    for (int i = 0; i < iterations; ++i)
    {
        level_3_function(depth + 1);
    }
}

void level_1_function(int iterations)
{
    CTRACK_NAME("level_1_function");
    // Busy wait for 200 ns
    busy_wait_ns(200);

    level_2_function(1, iterations);
}

// Version without CTRACK for overhead measurement
void leaf_function_no_track(int depth)
{
    busy_wait_ns(1000);
}

void level_3_function_no_track(int depth)
{
    busy_wait_ns(500);
    leaf_function_no_track(depth + 1);
    leaf_function_no_track(depth + 1);
}

void level_2_function_no_track(int depth, int iterations)
{
    busy_wait_ns(300);
    for (int i = 0; i < iterations; ++i)
    {
        level_3_function_no_track(depth + 1);
    }
}

void level_1_function_no_track(int iterations)
{
    busy_wait_ns(200);
    level_2_function_no_track(1, iterations);
}

// Worker thread function
void benchmark_worker(size_t events_per_thread, std::atomic<bool> &start_flag)
{
    // Wait for start signal
    while (!start_flag.load())
    {
        std::this_thread::yield();
    }

    // Calculate iterations to reach target event count
    // Each level_1 call generates: 1 + 1 + iterations * (1 + 2) events
    // For iterations=10: 1 + 1 + 10 * 3 = 32 events per call
    const int iterations = 10;
    const int events_per_call = 2 + iterations * 3;
    size_t calls_needed = events_per_thread / events_per_call;

    for (size_t i = 0; i < calls_needed; ++i)
    {
        level_1_function(iterations);
    }
}

// Worker thread function without tracking
void benchmark_worker_no_track(size_t events_per_thread, std::atomic<bool> &start_flag)
{
    while (!start_flag.load())
    {
        std::this_thread::yield();
    }

    const int iterations = 10;
    const int events_per_call = 2 + iterations * 3;
    size_t calls_needed = events_per_thread / events_per_call;

    for (size_t i = 0; i < calls_needed; ++i)
    {
        level_1_function_no_track(iterations);
    }
}

// Parse timing from CTRACK results string for a specific function
double parse_function_timing(const std::string &results, const std::string &function_name)
{
    // Look for the Details section first
    size_t details_pos = results.find("Details");
    if (details_pos == std::string::npos)
    {
        return -1.0; // Details section not found
    }

    // Look for the function name after the Details section
    size_t func_pos = results.find(function_name, details_pos);
    if (func_pos == std::string::npos)
    {
        return -1.0; // Function not found in Details section
    }

    // Find the line containing this function in the Details section
    size_t line_start = results.rfind('\n', func_pos);
    if (line_start == std::string::npos)
        line_start = details_pos;
    else
        line_start++; // Skip the newline

    size_t line_end = results.find('\n', func_pos);
    if (line_end == std::string::npos)
        line_end = results.length();

    std::string line = results.substr(line_start, line_end - line_start);

    // Look for the "time acc" column value (4th column after filename, function, line)
    // Split by | and find the 4th field
    std::vector<std::string> fields;
    std::istringstream iss(line);
    std::string field;

    while (std::getline(iss, field, '|'))
    {
        // Trim whitespace
        field.erase(0, field.find_first_not_of(" \t"));
        field.erase(field.find_last_not_of(" \t") + 1);
        if (!field.empty())
        {
            fields.push_back(field);
        }
    }

    // The time acc should be in the 4th field (0-indexed: filename=0, function=1, line=2, time_acc=3)
    if (fields.size() > 3)
    {
        std::string time_acc = fields[3];

        // Parse value and unit from time_acc (e.g., "2.09 ms")
        std::istringstream time_iss(time_acc);
        double value;
        std::string unit;

        if (time_iss >> value >> unit)
        {
            // Convert to nanoseconds based on unit
            if (unit == "s")
                return value * 1e9;
            else if (unit == "ms")
                return value * 1e6;
            else if (unit == "mcs")
                return value * 1e3;
            else if (unit == "ns")
                return value;
        }
    }

    return -1.0; // Could not parse
}

// Measure accuracy by comparing known timings with CTRACK measurements
std::pair<double, double> measure_accuracy()
{
    std::cout << "\n=== Measuring Accuracy ===" << std::endl;

    // Clear any previous tracking data by getting and discarding results
    ctrack::result_as_string();

    // Run a controlled test with known timings
    const int test_iterations = 100;
    for (int i = 0; i < test_iterations; ++i)
    {
        level_1_function(10);
    }

    // Get results
    auto results = ctrack::result_as_string();

    // Expected timings per iteration (in nanoseconds):
    // leaf_function: 1000ns (called 20 times per iteration) = 20,000ns total per iteration
    // level_3_function: 500ns + 2*1000ns = 2500ns (called 10 times per iteration) = 25,000ns total per iteration
    // level_2_function: 300ns + 10*2500ns = 25,300ns (called 1 time per iteration) = 25,300ns total per iteration
    // level_1_function: 200ns + 25,300ns = 25,500ns (called 1 time per iteration) = 25,500ns total per iteration

    struct ExpectedTiming
    {
        std::string name;
        double expected_total_ns;
        int call_count;
    };

    std::vector<ExpectedTiming> expected_timings = {
        {"leaf_function", 1000.0 * 20 * test_iterations, 20 * test_iterations},
        {"level_3_function", 2500.0 * 10 * test_iterations, 10 * test_iterations},
        {"level_2_function", 25300.0 * 1 * test_iterations, 1 * test_iterations},
        {"level_1_function", 25500.0 * 1 * test_iterations, 1 * test_iterations}};

    double total_expected_time = 0.0;
    double total_actual_time = 0.0;
    double max_absolute_error = 0.0;

    if (g_config.verbose)
    {
        std::cout << "Function accuracy analysis:" << std::endl;
    }

    for (const auto &timing : expected_timings)
    {
        double actual_ns = parse_function_timing(results, timing.name);
        if (actual_ns > 0)
        {
            double expected_ns = timing.expected_total_ns;
            double absolute_error = std::abs(actual_ns - expected_ns);
            double percent_error = (absolute_error / expected_ns) * 100.0;

            total_expected_time += expected_ns;
            total_actual_time += actual_ns;
            max_absolute_error = (std::max)(max_absolute_error, absolute_error);

            if (g_config.verbose)
            {
                std::cout << "  " << timing.name << ": expected " << expected_ns / 1e6 << " ms, got "
                          << actual_ns / 1e6 << " ms (error: " << percent_error << "%)" << std::endl;
            }
        }
        else if (g_config.verbose)
        {
            std::cout << "  " << timing.name << ": could not parse timing" << std::endl;
        }
    }

    double overall_error_percent = 0.0;
    double overall_error_ms = 0.0;

    if (total_expected_time > 0)
    {
        double total_absolute_error = std::abs(total_actual_time - total_expected_time);
        overall_error_percent = (total_absolute_error / total_expected_time) * 100.0;

        // Calculate total number of events across all functions
        double total_events = 0;
        for (const auto &timing : expected_timings)
        {
            total_events += timing.call_count;
        }

        // Convert to milliseconds per event
        overall_error_ms = (total_absolute_error / 1e6) / total_events; // Convert to milliseconds per event
    }

    if (g_config.verbose)
    {
        std::cout << "Overall accuracy error: " << overall_error_percent << "% (" << overall_error_ms << " ms per event)" << std::endl;
    }

    return {overall_error_percent, overall_error_ms};
}

// Measure overhead by comparing with and without CTRACK
std::tuple<double, double, double> measure_overhead()
{
    std::cout << "\n=== Measuring Overhead ===" << std::endl;

    const size_t overhead_events = 1'000'000; // 1M events for overhead test
    size_t events_per_thread = overhead_events / g_config.thread_count;

    // Measure without CTRACK
    auto start_no_track = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};

        for (size_t i = 0; i < g_config.thread_count; ++i)
        {
            threads.emplace_back(benchmark_worker_no_track, events_per_thread, std::ref(start_flag));
        }

        start_flag = true;

        for (auto &t : threads)
        {
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

        for (size_t i = 0; i < g_config.thread_count; ++i)
        {
            threads.emplace_back(benchmark_worker, events_per_thread, std::ref(start_flag));
        }

        start_flag = true;

        for (auto &t : threads)
        {
            t.join();
        }
    }
    auto end_track = std::chrono::high_resolution_clock::now();
    auto duration_track = std::chrono::duration_cast<std::chrono::microseconds>(end_track - start_track).count();

    double overhead_percent = ((double)(duration_track - duration_no_track) / duration_no_track) * 100.0;
    double overhead_ms = (duration_track - duration_no_track) / 1000.0;                               // Convert microseconds to milliseconds
    double overhead_ns_per_event = ((duration_track - duration_no_track) * 1000.0) / overhead_events; // nanoseconds per event

    if (g_config.verbose)
    {
        std::cout << "Without CTRACK: " << duration_no_track << " µs" << std::endl;
        std::cout << "With CTRACK: " << duration_track << " µs" << std::endl;
        std::cout << "Overhead: " << overhead_percent << "% (" << overhead_ms << " ms total, "
                  << overhead_ns_per_event << " ns per event)" << std::endl;
    }

    return {overhead_percent, overhead_ms, overhead_ns_per_event};
}

// Measure memory usage and calculation time
std::tuple<double, double, double> measure_memory_and_calculation_time()
{
    std::cout << "\n=== Measuring Memory Usage and Calculation Time ===" << std::endl;

    // Clear any previous tracking data by getting and discarding results
    ctrack::result_as_string();

    // Measure initial memory
    size_t initial_memory = get_memory_usage();

    // Generate events
    size_t events_per_thread = g_config.total_events / g_config.thread_count;

    if (g_config.verbose)
    {
        std::cout << "Generating " << g_config.total_events << " events across "
                  << g_config.thread_count << " threads..." << std::endl;
    }

    auto gen_start = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};

        for (size_t i = 0; i < g_config.thread_count; ++i)
        {
            threads.emplace_back(benchmark_worker, events_per_thread, std::ref(start_flag));
        }

        start_flag = true;

        for (auto &t : threads)
        {
            t.join();
        }
    }
    auto gen_end = std::chrono::high_resolution_clock::now();

    // Measure memory after event generation
    size_t post_event_memory = get_memory_usage();
    size_t memory_used = post_event_memory - initial_memory;
    double bytes_per_event = (double)memory_used / g_config.total_events;

    if (g_config.verbose)
    {
        auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(gen_end - gen_start).count();
        std::cout << "Event generation took: " << gen_duration << " ms" << std::endl;
        std::cout << "Memory used: " << memory_used / (1024.0 * 1024.0) << " MB" << std::endl;
        std::cout << "Memory per event: " << bytes_per_event << " bytes" << std::endl;
    }

    // Measure calculation time and peak memory usage
    std::atomic<bool> monitoring{true};
    std::atomic<size_t> peak_memory{post_event_memory};

    // Start memory monitoring thread
    std::thread monitor_thread([&monitoring, &peak_memory, initial_memory]()
                               {
        while (monitoring.load()) {
            size_t current_memory = get_memory_usage();
            size_t current_peak = peak_memory.load();
            while (current_memory > current_peak && 
                   !peak_memory.compare_exchange_weak(current_peak, current_memory)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Poll every 10ms
        } });

    auto calc_start = std::chrono::high_resolution_clock::now();
    auto results = ctrack::result_as_string();
    auto calc_end = std::chrono::high_resolution_clock::now();

    // Stop monitoring
    monitoring = false;
    monitor_thread.join();

    auto calc_duration = std::chrono::duration_cast<std::chrono::microseconds>(calc_end - calc_start).count() / 1000.0;
    double peak_calc_memory_mb = (peak_memory.load() - initial_memory) / (1024.0 * 1024.0);

    if (g_config.verbose)
    {
        std::cout << "Result calculation took: " << calc_duration << " ms" << std::endl;
        std::cout << "Peak memory during calculation: " << peak_calc_memory_mb << " MB" << std::endl;
    }

    return {bytes_per_event, calc_duration, peak_calc_memory_mb};
}

// Save baseline to file
void save_baseline(const BaselineData &data)
{
    std::ofstream file(g_config.baseline_file);
    if (!file)
    {
        std::cerr << "Error: Could not open baseline file for writing: " << g_config.baseline_file << std::endl;
        return;
    }

    // Simple JSON format
    file << "{\n";
    file << "  \"accuracy_error_percent\": " << data.accuracy_error_percent << ",\n";
    file << "  \"accuracy_error_ms_per_event\": " << data.accuracy_error_ms_per_event << ",\n";
    file << "  \"overhead_percent\": " << data.overhead_percent << ",\n";
    file << "  \"overhead_ms\": " << data.overhead_ms << ",\n";
    file << "  \"overhead_ns_per_event\": " << data.overhead_ns_per_event << ",\n";
    file << "  \"memory_bytes_per_event\": " << data.memory_bytes_per_event << ",\n";
    file << "  \"calculation_time_ms\": " << data.calculation_time_ms << ",\n";
    file << "  \"peak_calc_memory_mb\": " << data.peak_calc_memory_mb << ",\n";
    file << "  \"total_events\": " << data.total_events << ",\n";
    file << "  \"thread_count\": " << data.thread_count << ",\n";
    file << "  \"timestamp\": \"" << data.timestamp << "\",\n";
    file << "  \"platform\": \"" << data.platform << "\"\n";
    file << "}\n";

    std::cout << "\nBaseline saved to: " << g_config.baseline_file << std::endl;
}

// Load baseline from file
bool load_baseline(BaselineData &data)
{
    std::ifstream file(g_config.baseline_file);
    if (!file)
    {
        return false;
    }

    // Simple JSON parsing (production code would use a proper JSON library)
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("\"accuracy_error_percent\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.accuracy_error_percent = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"accuracy_error_ms_per_event\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.accuracy_error_ms_per_event = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"overhead_percent\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.overhead_percent = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"overhead_ms\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.overhead_ms = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"overhead_ns_per_event\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.overhead_ns_per_event = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"memory_bytes_per_event\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.memory_bytes_per_event = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"calculation_time_ms\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.calculation_time_ms = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"peak_calc_memory_mb\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.peak_calc_memory_mb = std::stod(line.substr(pos, end - pos));
        }
        else if (line.find("\"total_events\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.total_events = std::stoull(line.substr(pos, end - pos));
        }
        else if (line.find("\"thread_count\":") != std::string::npos)
        {
            size_t pos = line.find(": ") + 2;
            size_t end = line.find(",", pos);
            data.thread_count = std::stoull(line.substr(pos, end - pos));
        }
    }

    return true;
}

// Compare current results with baseline
void compare_with_baseline(const BaselineData &current)
{
    BaselineData baseline;
    if (!load_baseline(baseline))
    {
        std::cerr << "Error: Could not load baseline file: " << g_config.baseline_file << std::endl;
        return;
    }

    std::cout << "\n=== Baseline Comparison ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);    auto print_comparison = [](const std::string &metric, double baseline_val, double current_val, bool lower_is_better = true)
    {
        double diff = current_val - baseline_val;
        double percent_change = (diff / baseline_val) * 100.0;

        std::string direction = (diff > 0) ? "increased" : "decreased";
        std::string indicator = (lower_is_better ? (diff > 0 ? "worse" : "better") : (diff > 0 ? "better" : "worse"));

        std::cout << metric << ":\n";
        std::cout << "  Baseline: " << baseline_val << "\n";
        std::cout << "  Current:  " << current_val << "\n";
        std::cout << "  Change:   " << indicator << " - " << std::abs(percent_change) << "% " << direction << "\n\n";
    };

    print_comparison("Accuracy Error %", baseline.accuracy_error_percent, current.accuracy_error_percent);
    print_comparison("Accuracy Error (ms/event)", baseline.accuracy_error_ms_per_event, current.accuracy_error_ms_per_event);
    print_comparison("Overhead %", baseline.overhead_percent, current.overhead_percent);
    print_comparison("Overhead Time (ms)", baseline.overhead_ms, current.overhead_ms);
    print_comparison("Overhead per Event (ns)", baseline.overhead_ns_per_event, current.overhead_ns_per_event);
    print_comparison("Memory/Event (bytes)", baseline.memory_bytes_per_event, current.memory_bytes_per_event);
    print_comparison("Calculation Time (ms)", baseline.calculation_time_ms, current.calculation_time_ms);
    print_comparison("Peak Calc Memory (MB)", baseline.peak_calc_memory_mb, current.peak_calc_memory_mb);
}

// Get platform string
std::string get_platform()
{
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
std::string get_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
#ifdef _WIN32
    struct tm time_info;
    localtime_s(&time_info, &time_t);
    ss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
#else
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
#endif
    return ss.str();
}

// Print usage
void print_usage(const char *program_name)
{
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
bool parse_args(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help")
        {
            print_usage(argv[0]);
            return false;
        }
        else if (arg == "--events" && i + 1 < argc)
        {
            g_config.total_events = std::stoull(argv[++i]);
        }
        else if (arg == "--threads" && i + 1 < argc)
        {
            g_config.thread_count = std::stoull(argv[++i]);
        }
        else if (arg == "--baseline" && i + 1 < argc)
        {
            g_config.baseline_file = argv[++i];
        }
        else if (arg == "--record-baseline")
        {
            g_config.record_baseline = true;
        }
        else if (arg == "--compare-baseline")
        {
            g_config.compare_baseline = true;
        }
        else if (arg == "--verbose")
        {
            g_config.verbose = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    if (!parse_args(argc, argv))
    {
        return 1;
    }

    std::cout << "CTRACK Comprehensive Benchmark\n";
    std::cout << "==============================\n";
    std::cout << "Total events: " << g_config.total_events << "\n";
    std::cout << "Thread count: " << g_config.thread_count << "\n";
    std::cout << "Events per thread: " << g_config.total_events / g_config.thread_count << "\n";

    // Run benchmarks
    auto [accuracy_error_percent, accuracy_error_ms_per_event] = measure_accuracy();
    auto [overhead_percent, overhead_ms, overhead_ns_per_event] = measure_overhead();
    auto [bytes_per_event, calc_time, peak_calc_memory] = measure_memory_and_calculation_time();

    // Prepare results
    BaselineData current_data;
    current_data.accuracy_error_percent = accuracy_error_percent;
    current_data.accuracy_error_ms_per_event = accuracy_error_ms_per_event;
    current_data.overhead_percent = overhead_percent;
    current_data.overhead_ms = overhead_ms;
    current_data.overhead_ns_per_event = overhead_ns_per_event;
    current_data.memory_bytes_per_event = bytes_per_event;
    current_data.calculation_time_ms = calc_time;
    current_data.peak_calc_memory_mb = peak_calc_memory;
    current_data.total_events = g_config.total_events;
    current_data.thread_count = g_config.thread_count;
    current_data.timestamp = get_timestamp();
    current_data.platform = get_platform();

    // Print summary
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Accuracy error: " << accuracy_error_percent << "% (" << accuracy_error_ms_per_event << " ms per event)" << std::endl;
    std::cout << "Overhead: " << overhead_percent << "% (" << overhead_ms << " ms total, "
              << overhead_ns_per_event << " ns per event)" << std::endl;
    std::cout << "Memory per event: " << bytes_per_event << " bytes" << std::endl;
    std::cout << "Calculation time: " << calc_time << " ms" << std::endl;
    std::cout << "Peak calculation memory: " << peak_calc_memory << " MB" << std::endl;

    // Handle baseline operations
    if (g_config.record_baseline)
    {
        save_baseline(current_data);
    }

    if (g_config.compare_baseline)
    {
        compare_with_baseline(current_data);
    }

    return 0;
}