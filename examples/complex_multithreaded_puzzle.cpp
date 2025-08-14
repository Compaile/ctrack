#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <mutex>
#include "ctrack.hpp"

std::mutex cout_mutex;
std::atomic<int> global_counter(0);

void sleepy_function(int ms) {
    CTRACK;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int recursive_work(int n) {
    CTRACK;
    if (n <= 1) return 1;
    sleepy_function(1);
    return recursive_work(n - 1) + recursive_work(n - 2);
}

void nested_function_a(int depth) {
    CTRACK;
    if (depth > 0) {
        sleepy_function(5);
        nested_function_a(depth - 1);
    }
}

void nested_function_b(int depth) {
    CTRACK;
    if (depth > 0) {
        sleepy_function(3);
        nested_function_b(depth - 1);
    }
    if (depth == 3) { // Hidden slow path
        sleepy_function(100);
    }
}

void complex_operation(int id) {
    CTRACK;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 10);

    for (int i = 0; i < 5; ++i) {
        int random_num = dis(gen);
        if (random_num % 2 == 0) {
            nested_function_a(random_num);
        } else {
            nested_function_b(random_num);
        }
        
        if (random_num == 7) { // Rare slow path
            recursive_work(20);
        }
        
        global_counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Thread " << id << " completed." << std::endl;
    }
}

int main() {
    const int thread_count = 4;
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(complex_operation, i);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Global counter: " << global_counter << std::endl;
    ctrack::result_print();

    return 0;
}