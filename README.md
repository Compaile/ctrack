# CTRACK ![MIT](https://img.shields.io/badge/license-MIT-blue.svg)

An open-source benchmark and tracking library for C++ projects, designed to provide deep insights into function performance with minimal overhead.

CTRACK is a powerful tool that can be seamlessly integrated into both development and production environments. It allows developers to effortlessly monitor applications and identify bottlenecks, requiring minimal setup and maintenance.

## Features

- Single header file
- No dependencies (optional tbb for non msvc to use paralell result calculation) 
- Easy to use (just 1 line per function you want to track)
- Minimal overhead (can record tens of millions events per second)
- Optimized for multi-threaded environments
- Requires C++17
- Compatible with major compilers out of the box
- Multiple output formats (stdout(with color), string, and more soon JSON export, SQL)
- Suitable for both development and high-performance production systems
- Can be easily customized to filter out noise and only print events that matter. Recording levels can be adjusted based on the environment (production/development) to balance between insight and performance.
- Includes a beautiful table print functionality for clear and readable output. While easy-to-use functions are provided for accessing CTRACK events, more experienced users can access all data directly for advanced analysis.


## Goals

1. Help developers identify areas for performance improvement
2. Monitor application performance in production


## Basic Usage 

CTRACK is easy to use and provides powerful performance insights. Here's how to get started:

1. To track a function, simply add the `CTRACK;` macro at the beginning of the function body:

```cpp
void myFunction() {
    CTRACK;
    // Your function code here
}
```

2. To print the results, you have two options:

   a. Print colored results to the console:
   ```cpp
   ctrack::result_print();
   ```

   b. Get the results as a string (useful for logging or custom output):
   ```cpp
   std::string results = ctrack::result_as_string();
   ```

### Example

```cpp
#include "ctrack.hpp"

void expensiveOperation() {
    CTRACK;
    // Simulating some work
    for (int i = 0; i < 1000000; ++i) {
        // Do something
    }
}

int main() {
    for (int i = 0; i < 100; ++i) {
        expensiveOperation();
    }

    // Print results to console
    ctrack::result_print();

    return 0;
}
```

This basic usage will automatically track the performance of `expensiveOperation` and provide you with insights when you call `result_print()`.

For more complex scenarios, configuration options, and advanced features, please refer to the [Advanced Usage](#advanced-usage) section below.
Additionally, be sure to check out the examples directory in the repository for more detailed usage examples and best practices.



## Metrics & Output

CTRACK provides comprehensive performance metrics through two main components: the Summary Table and the Detail Table. These tables offer different levels of insight into your application's performance.

### Time Units

All times in CTRACK are presented and automatically converted in easily understandable units:
- ns (nanoseconds)
- μs (microseconds) printed as mcs
- ms (milliseconds)
- s (seconds)

### General Metrics

- **min, mean, med, max**: The fastest (min), average (mean), median (med), and slowest (max) execution times for a specific CTRACK event.

- **time a - time active**: Total time the event was active, useful for multithreaded environments. For example, if a 100ms function is called by 10 threads simultaneously, time active will show 100ms instead of 1000ms.

- **time ae - time active exclusive**: Subtracts the time spent in child functions that are also tracked. Intelligently handles recursion and overlapping timeframes.

- **time [x-y]**: Shows event times within specified percentile ranges (default [0-100] and [1-99]) to exclude outliers.

- **sd - Standard Deviation**: Displays the variability in function execution times.

- **cv - Coefficient of Variation**: Unitless version of standard deviation (sd / mean) for comparing variability across functions with different scales.

- **time acc**: Simple sum of execution times for all calls to a tracked function.

- **threads**: Number of different threads that called a specific function.

### Summary Table

![image](https://github.com/user-attachments/assets/0232a57c-8a3e-4a7b-a143-62bb8e2b9825)


#### Summary Header
- Start and End time of tracking
- Total time
- Time tracked (time spent in tracked functions)
- Time tracked percentage

#### Summary Entries
- Filename, function, line
- Number of calls
- Time active exclusive for [0-100] and [center interval] (in percent and absolute)
- Time active for [0-100] in absolute

The summary table is sorted by the active exclusive [center interval] metric.

### Detail Table

![image](https://github.com/user-attachments/assets/ae9cf8e9-52b8-42ed-95fa-c95a418da510)

For each function:
- Filename, function, line
- Time accumulated
- Standard Deviation
- Coefficient of Variation (cv)
- Number of calls
- Number of calling threads

Each entry shows 3 blocks for the fastest, center, and slowest events.

### Output Format

- String output: Summary table followed by Detail tables (slowest to fastest)
- Console output: Reversed order (Detail tables followed by Summary table)

This comprehensive set of metrics allows for deep insight into your application's performance, helping you identify bottlenecks and optimize effectively.

For more advanced usage and customization options, please refer to the [Advanced Usage](#advanced-usage) section below.

## Installation

CTRACK is designed to be easy to integrate into your C++ projects. There are two primary ways to use CTRACK:

### 1. Header-Only Inclusion

CTRACK is a header-only library, which means you can start using it by simply including the main header file in your project:

```cpp
#include "ctrack.hpp"
```

This method is straightforward and doesn't require any additional setup or build process.

Note: If you are using a compiler which needs TBB for C++ standard parallel algorithms, you need to link to -ltbb. You can always fall back to sequential result calculation by setting
CTRACK_DISABLE_EXECUTION_POLICY. The recording will be unchanged, but the printing/calculating of the stats will be a bit slower.

### 2. CMake Package

For projects using CMake, CTRACK can be installed and used as a CMake package. This method provides better integration with your build system and makes it easier to manage dependencies.

To use CTRACK as a CMake package:

1. Install CTRACK using CMake:

   ```bash
   git clone https://github.com/your-repo/ctrack.git
   cd ctrack
   mkdir build && cd build
   cmake ..
   cmake --build . --target install
   ```

2. In your project's `CMakeLists.txt`, add:

   ```cmake
   find_package(ctrack REQUIRED)
   target_link_libraries(your_target PRIVATE ctrack::ctrack)
   ```

Note: If you are using a compiler which needs TBB for C++ standard parallel algorithms, you need to link to tbb. 
  ```target_link_libraries( your_target PRIVATE  TBB::tbb )   ```
You can always fall back to sequential result calculation by setting
CTRACK_DISABLE_EXECUTION_POLICY. The recording will be unchanged, but the printing/calculating of the stats will be a bit slower.

For more detailed examples of how to use CTRACK with CMake, please refer to the `examples` directory in the CTRACK repository.

Choose the installation method that best fits your project's needs and structure. Both methods provide full access to CTRACK's features and capabilities.


## Advanced Usage

### Customizing Output Settings

You can fine-tune CTRACK's output using the `ctrack_result_settings` struct:

```cpp
struct ctrack_result_settings {
    unsigned int non_center_percent = 1;
    double min_percent_active_exclusive = 0.5; // between 0-100, default 0.5%
    double percent_exclude_fastest_active_exclusive = 0.0; // between 0-100
};
```

- `non_center_percent`: Defines the range for the center interval (e.g., 1 means [1-99])
- `min_percent_active_exclusive`: Excludes events active for less than the specified percentage
- `percent_exclude_fastest_active_exclusive`: Excludes the fastest n% of functions to reduce noise

### Advanced CTRACK Calls

CTRACK offers different tracking levels:

- `CTRACK`: Standard tracking
- `CTRACK_DEV`: Development-specific tracking
- `CTRACK_PROD`: Production-specific tracking

You can selectively disable tracking groups:

- `CTRACK_DISABLE_DEV`: Disables all `CTRACK_DEV` calls

To completely disable CTRACK at compile time, define `CTRACK_DISABLE`.

### Custom Naming

Use custom names for CTRACK calls instead of function names:

```cpp
CTRACK_NAME("myname")
CTRACK_DEV_NAME("mydevname")
CTRACK_PROD_NAME("myprodname")
```

This is useful for large functions where you want multiple CTRACK entries with distinct names.

### Code-Level Access

The `result_print` and `result_as_string` functions are concise and located at the bottom of the CTRACK header. You can easily modify these or create custom functions to change the order, enable/disable colors, etc.

The `calc_stats_and_clear` function produces the `ctrack_result` object. Instead of printing tables, you can access this data directly for custom analysis or integration with other systems.

### Example: Customizing Output

```cpp
ctrack_result_settings settings;
settings.non_center_percent = 2;
settings.min_percent_active_exclusive = 1.0;
settings.percent_exclude_fastest_active_exclusive = 5.0;

std::string custom_result = ctrack::result_as_string(settings);
```

This advanced usage allows you to tailor CTRACK to your specific needs, from fine-tuning output to integrating with complex systems and workflows.

## Performance Benchmarks

The recording of events in this project is extremely fast. You can use the example projects to test it on your own system.

- On an i9-12900KS:

    CTRACK can record 10,000,000 events in 132ms
    This translates to over 75 million events per second

The calculation of results is also efficient. However, the primary focus of ctrack is to have nearly zero overhead for tracking while allowing some overhead for calculating statistics at the end.
Would you like me to explain any part of this new section or suggest any modifications?

## Why Another Benchmark Library?

While there are several excellent benchmarking and profiling tools available, CTRACK fills a unique niche in the C++ performance analysis ecosystem. Here's why CTRACK stands out:

1. **Production-Ready**: Unlike libraries such as Google Benchmark, which require specific benchmark calls, CTRACK can be seamlessly used in both development and production environments.

2. **Legacy-Friendly**: CTRACK is designed to easily integrate with large, established projects that might be challenging to instrument with other tracking solutions.

3. **Lightweight and Fast**: Traditional profiling tools like MSVC Performance Analyzer or Intel VTune can struggle with millions of events. CTRACK maintains high performance even under heavy load.

4. **No Complex Setup**: Unlike full-featured profilers, CTRACK doesn't require an extensive setup process, making it ideal for quick deployments and CI/CD pipelines.

5. **Platform Independent**: CTRACK works across different platforms without modification, unlike some platform-specific profiling tools.

6. **Simplicity**: Many developers resort to manual timing using `std::chrono`. CTRACK provides a more robust solution with similar ease of use.

7. **Scalability**: From small libraries to massive codebases, CTRACK adapts to your needs.

8. **Flexible Configuration**: Easily enable, disable, or customize logging levels to suit different environments (development vs. production).

9. **Instant Bottleneck Detection**: CTRACK's unique "time active" and "time active exclusive" metrics allow developers to instantly spot bottlenecks, even in complex multithreaded codebases. This feature sets CTRACK apart from other tools that struggle to provide clear insights in concurrent environments.

CTRACK combines the ease of use of manual timing with the robustness of professional benchmarking tools, all in a package that's production-ready and highly adaptable. Its ability to quickly identify performance issues in multithreaded scenarios makes useful tool for modern C++ development.


## Inspired By

CTRACK stands on the shoulders of giants in the C++ performance analysis ecosystem. We're grateful to the following projects and their maintainers for pioneering innovative approaches to performance measurement, particularly the timing-by-lifetime concepts that became foundational to CTRACK's design:

- [Darknet](https://github.com/hank-ai/darknet) 
- [dlib](https://dlib.net/dlib/timing.h.html) 
- [Tracy Profiler](https://github.com/wolfpld/tracy)
- [Google Benchmark](https://github.com/google/benchmark)
- [CppBenchmark](https://github.com/chronoxor/CppBenchmark)
- [nanobench](https://github.com/martinus/nanobench)


## Contributing

We welcome and encourage contributions from the community! Your input helps make CTRACK better for everyone. Here's how you can contribute:

### Roadmap
- [ ] JSON Export Support
- [ ] SQL Export Support
- [ ] Handling Complex Circular

### Pull Requests
We're always excited to receive pull requests that improve CTRACK. When submitting a PR, please ensure:

1. Your code adheres to the project's coding standards.
2. Your contributions are MIT License compliant.

### Bug Reports
Found a bug? We want to hear about it! Please open an issue on our GitHub repository with:

1. A clear, descriptive title.
2. A detailed description of the issue, including steps to reproduce.
3. Your environment details (OS, compiler version, etc.).

### Feature Requests
Have an idea for a new feature? Feel free to open an issue to discuss it. We're always looking for ways to make CTRACK more useful.


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

