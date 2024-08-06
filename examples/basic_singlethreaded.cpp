#include <iostream>
#include <cmath>
#include "ctrack.hpp" // Assuming this is the header for your library

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

int main() {

    
    // Perform calculations
    double sum = sum_of_squares(1000);
    unsigned long long fact = factorial(20);
    int fib = fibonacci(30);

    // Print results
    std::cout << "Sum of squares: " << sum << std::endl;
    std::cout << "Factorial: " << fact << std::endl;
    std::cout << "Fibonacci: " << fib << std::endl;

    // Print benchmarking results
     ctrack::result_print();
    //std::cout << ctrack::result_as_string() << std::endl;
    return 0;
}