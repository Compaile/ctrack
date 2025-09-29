#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include "ctrack.hpp"

double estimate_pi(int points) {
    CTRACK;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    int inside_circle = 0;
    for (int i = 0; i < points; ++i) {
        double x = dis(gen);
        double y = dis(gen);
        if (x*x + y*y <= 1.0) {
            inside_circle++;
        }
    }

    return 4.0 * inside_circle / points;
}

void run_estimations(int iterations, int points_per_estimation) {
    for (int i = 0; i < iterations; ++i) {
        estimate_pi(points_per_estimation);
    }
}

int main() {
    const int total_estimations = 1000;
    const int points_per_estimation = 100000;
    const int thread_count = std::thread::hardware_concurrency();
    const int estimations_per_thread = total_estimations / thread_count;

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(run_estimations, estimations_per_thread, points_per_estimation);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Completed " << total_estimations << " pi estimations" << std::endl;
    ctrack::result_print();

    return 0;
}