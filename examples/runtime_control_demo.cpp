#include <iostream>
#include <thread>
#include <chrono>
#include "ctrack.hpp"

// NOTE: This example requires CTRACK_ENABLE_RUNTIME_CONTROL to be defined
// Compile with: -DCTRACK_ENABLE_RUNTIME_CONTROL
// Or CMake: cmake -DENABLE_RUNTIME_CONTROL=ON

void expensive_calculation(int iterations) {
    CTRACK;
    volatile double result = 0.0;
    for (int i = 0; i < iterations; ++i) {
        result += std::sqrt(i * 3.14159);
    }
}

void lightweight_work(int count) {
    CTRACK;
    volatile int sum = 0;
    for (int i = 0; i < count; ++i) {
        sum += i;
    }
}

int main() {
#ifdef CTRACK_ENABLE_RUNTIME_CONTROL
    std::cout << "=== Runtime Control Demo ===" << std::endl;
    std::cout << "Runtime control is ENABLED\n" << std::endl;

    // Phase 1: Tracking enabled (default)
    std::cout << "Phase 1: Tracking ENABLED (tracking heavy operations)" << std::endl;
    for (int i = 0; i < 5; ++i) {
        expensive_calculation(100000);
    }

    // Phase 2: Disable tracking for lightweight operations
    std::cout << "Phase 2: Tracking DISABLED (skipping lightweight operations)" << std::endl;
    ctrack::disable();

    for (int i = 0; i < 1000; ++i) {
        lightweight_work(100);  // These won't be tracked
    }

    // Phase 3: Re-enable for more heavy operations
    std::cout << "Phase 3: Tracking RE-ENABLED (tracking more heavy operations)" << std::endl;
    ctrack::enable();

    for (int i = 0; i < 5; ++i) {
        expensive_calculation(100000);
    }

    // Check state
    std::cout << "\nCurrent tracking state: "
              << (ctrack::is_enabled() ? "ENABLED" : "DISABLED") << std::endl;

    // Print results - should only show Phase 1 and Phase 3
    std::cout << "\n=== Performance Results ===" << std::endl;
    std::cout << "Expected: ~10 calls to expensive_calculation(), 0 calls to lightweight_work()" << std::endl;
    ctrack::result_print();

#else
    std::cout << "ERROR: This example requires CTRACK_ENABLE_RUNTIME_CONTROL" << std::endl;
    std::cout << "Compile with: -DCTRACK_ENABLE_RUNTIME_CONTROL" << std::endl;
    std::cout << "Or CMake: cmake -DENABLE_RUNTIME_CONTROL=ON" << std::endl;
    return 1;
#endif

    return 0;
}
