#include <iostream>
#include <cmath>
#include <string>
#include "ctrack.hpp"

// Function to calculate the sum of squares from 1 to n
double sum_of_squares(int n) {
    CTRACK; // Start tracking this function
    double sum = 0;
    for (int i = 1; i <= n; ++i) {
        sum += i * i;
    }
    return sum;
}

// Function to calculate the factorial of n
unsigned long long factorial(int n) {
    CTRACK; // Start tracking this function
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// Function to calculate the nth Fibonacci number
int fibonacci(int n) {
    CTRACK; // Start tracking this function
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void demonstrate_save_mode() {
    std::cout << "=== SAVE MODE ===" << std::endl;
    std::cout << "Running calculations and saving events to file..." << std::endl;
    
    // Perform calculations
    double sum = sum_of_squares(1000);
    unsigned long long fact = factorial(20);
    int fib = fibonacci(30);
    
    // Print results
    std::cout << "Sum of squares: " << sum << std::endl;
    std::cout << "Factorial: " << fact << std::endl;
    std::cout << "Fibonacci: " << fib << std::endl;
    
    // Save events to file instead of printing
    if (ctrack::save_events_to_file("ctrack_events.bin")) {
        std::cout << "Events saved successfully to ctrack_events.bin" << std::endl;
    } else {
        std::cout << "Failed to save events" << std::endl;
    }
}

void demonstrate_load_mode() {
    std::cout << "\n=== LOAD MODE ===" << std::endl;
    std::cout << "Loading events from file and printing statistics..." << std::endl;
    
    // Load events from file and print statistics
    ctrack::result_print_from_file("ctrack_events.bin");
}

void demonstrate_save_with_result_save() {
    std::cout << "\n=== USING result_save() ===" << std::endl;
    std::cout << "Running more calculations..." << std::endl;
    
    // Run some more calculations
    sum_of_squares(500);
    factorial(15);
    fibonacci(25);
    
    // Use the convenience function
    if (ctrack::result_save("ctrack_events2.bin")) {
        std::cout << "Events saved successfully to ctrack_events2.bin using result_save()" << std::endl;
    }
    
    std::cout << "\nLoading and printing from ctrack_events2.bin:" << std::endl;
    ctrack::result_print_from_file("ctrack_events2.bin");
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string mode(argv[1]);
        
        if (mode == "save") {
            demonstrate_save_mode();
        } else if (mode == "load") {
            demonstrate_load_mode();
        } else if (mode == "both") {
            demonstrate_save_mode();
            demonstrate_load_mode();
        } else {
            std::cout << "Usage: " << argv[0] << " [save|load|both]" << std::endl;
            std::cout << "  save - Run calculations and save events to file" << std::endl;
            std::cout << "  load - Load events from file and print statistics" << std::endl;
            std::cout << "  both - Demonstrate both save and load operations" << std::endl;
            return 1;
        }
    } else {
        // Default: demonstrate all features
        demonstrate_save_mode();
        demonstrate_load_mode();
        demonstrate_save_with_result_save();
    }
    
    return 0;
}