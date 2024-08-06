#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include "ctrack.hpp"

std::atomic<int> primeCount(0);

bool isPrime(int n) {
	CTRACK;
	if (n <= 1) return false;
	for (int i = 2; i <= std::sqrt(n); ++i) {
		if (n % i == 0) return false;
	}
	return true;
}

void countPrimesInRange(int start, int end) {
	CTRACK;
	for (int i = start; i <= end; ++i) {
		if (isPrime(i)) {
			primeCount++;
		}
	}
}

int main() {
	const int totalNumbers = 1000000;
	const int threadCount = 8;
	const int numbersPerThread = totalNumbers / threadCount;

	std::vector<std::thread> threads;

	for (int i = 0; i < threadCount; ++i) {
		int start = i * numbersPerThread + 1;
		int end = (i == threadCount - 1) ? totalNumbers : (i + 1) * numbersPerThread;
		threads.emplace_back(countPrimesInRange, start, end);
	}

	for (auto& thread : threads) {
		thread.join();
	}

	std::cout << "Total primes found: " << primeCount << std::endl;

	ctrack::result_print();

	return 0;
}