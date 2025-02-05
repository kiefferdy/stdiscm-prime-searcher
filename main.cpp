#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <cmath>
#include <chrono>
#include <ctime>
#include <algorithm>

static std::mutex g_collectMutex;
static std::vector<long> g_collectedPrimes;
static std::mutex g_printMutex;

void readConfig(const std::string& filename, long &threads, long &maxNumber)
{
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        std::exit(1);
    }

    std::string line;
    bool threadsSet = false, maxNumberSet = false;

    while (std::getline(inFile, line)) {
        if (line.rfind("threads=", 0) == 0) {
            std::string value = line.substr(8);
            try {
                threads = std::stol(value);
                if (threads <= 0) throw std::invalid_argument("Non-positive threads");
                threadsSet = true;
            } catch (...) {
                std::cerr << "Invalid thread count in config: " << value << std::endl;
                std::exit(1);
            }
        } else if (line.rfind("maxNumber=", 0) == 0) {
            std::string value = line.substr(10);
            try {
                maxNumber = std::stol(value);
                if (maxNumber <= 1) throw std::invalid_argument("Invalid max number");
                maxNumberSet = true;
            } catch (...) {
                std::cerr << "Invalid max number in config: " << value << std::endl;
                std::exit(1);
            }
        }
    }

    inFile.close();

    if (!threadsSet || !maxNumberSet) {
        std::cerr << "Config file is missing 'threads=' or 'maxNumber=' entries." << std::endl;
        std::exit(1);
    }
}

// ============================================================================
// SCHEME A: Range Partition
//
// We split [1..maxNumber] into 'numThreads' contiguous chunks.
//
// Two printing modes:
//   A1: Print primes immediately from each thread.
//   A2: Collect primes in a global vector and print them all at the end.
// ============================================================================
bool isPrimeSingleThread(long n)
{
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;

    long limit = static_cast<long>(std::sqrt(static_cast<long double>(n)));
    for (long d = 3; d <= limit; d += 2) {
        if (n % d == 0) {
            return false;
        }
    }
    return true;
}

void workerRangeSchemeA(long threadId,
                        long startNum,
                        long endNum,
                        bool printImmediately)
{
    for (long n = startNum; n <= endNum; ++n)
    {
        if (isPrimeSingleThread(n))
        {
            if (printImmediately) {
                // Print right now
                std::lock_guard<std::mutex> lk(g_printMutex);
                std::cout << "[Thread " << threadId
                          << "] Found prime: " << n
                          << " (time=" << std::time(nullptr) << ")\n";
            }
            else {
                // Print later
                std::lock_guard<std::mutex> lk(g_collectMutex);
                g_collectedPrimes.push_back(n);
            }
        }
    }
}

// ============================================================================
// SCHEME B: Divisor Splitting
//
// For each number n in [2..maxNumber]:
//   - Spawn at most 'numThreads' threads (from the config).
//   - Only check divisors in [2..floor(sqrt(n))].
//   - Partition that set of divisors among the threads, so each thread
//     checks a subrange. If any thread finds a divisor, n is not prime.
//
// The prime numbers are then either printed immediately or after.
// ============================================================================
void workerCheckDivRange(long n, long startDiv, long endDiv,
                         bool &compositeFound,
                         std::mutex &flagMutex)
{
    for (long d = startDiv; d <= endDiv; ++d)
    {
        // Early exit if already found a divisor
        {
            std::lock_guard<std::mutex> guard(flagMutex);
            if (compositeFound) return;
        }
        if (n % d == 0)
        {
            std::lock_guard<std::mutex> guard(flagMutex);
            compositeFound = true;
            return;
        }
    }
}

bool isPrimeByDivisorThreads(long n, long numThreads)
{
    if (n < 2)  return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;

    long limit = static_cast<long>(std::sqrt(static_cast<long double>(n)));
    if (limit <= 2) {
        return true;
    }

    bool compositeFound = false;
    std::mutex flagMutex;

    std::vector<long> divisors;
    for (long d = 3; d <= limit; d += 2) {
        divisors.push_back(d);
    }

    if (divisors.empty()) {
        return true;
    }

    long totalDivs = static_cast<long>(divisors.size());
    long chunkSize = totalDivs / numThreads;
    if (chunkSize == 0) {
        chunkSize = totalDivs; 
        numThreads = 1;
    }

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    long startIndex = 0;
    for (long t = 0; t < numThreads; ++t)
    {
        long endIndex = (t == numThreads - 1)
                       ? (totalDivs - 1)
                       : (startIndex + chunkSize - 1);

        if (startIndex > totalDivs - 1) break;

        long startDiv = divisors[startIndex];
        long endDiv   = divisors[endIndex];

        threads.emplace_back(workerCheckDivRange,
                             n,
                             startDiv,
                             endDiv,
                             std::ref(compositeFound),
                             std::ref(flagMutex));

        startIndex = endIndex + 1;
    }

    for (auto &th : threads) {
        th.join();
    }

    return !compositeFound;
}

void runSchemeB(long maxNumber, long numThreads, bool printImmediately)
{
    for (long n = 2; n <= maxNumber; ++n)
    {
        bool prime = isPrimeByDivisorThreads(n, numThreads);
        if (prime)
        {
            if (printImmediately)
            {
                std::lock_guard<std::mutex> lk(g_printMutex);
                std::cout << "[B-scheme] Found prime: " << n
                          << " (time=" << std::time(nullptr) << ")\n";
            }
            else
            {
                std::lock_guard<std::mutex> lk(g_collectMutex);
                g_collectedPrimes.push_back(n);
            }
        }
    }
}

int main()
{
    // 1) Read config
    long numThreads = 0;
    long maxNumber = 0;
    readConfig("config.txt", numThreads, maxNumber);
    std::cout << "Config says: threads=" << numThreads
              << ", maxNumber=" << maxNumber << "\n\n";

    // 2) Let user pick which scheme (A or B) and print mode
    int choice;
    do {
        std::cout << "Choose approach:\n"
                  << "  1) Scheme A (range partition) + immediate printing\n"
                  << "  2) Scheme A (range partition) + print after\n"
                  << "  3) Scheme B (divisor-splitting, up to sqrt) + immediate printing\n"
                  << "  4) Scheme B (divisor-splitting, up to sqrt) + print after\n"
                  << "Enter choice (1-4): ";
        std::cin >> choice;

        if (std::cin.fail() || choice < 1 || choice > 4) {
            std::cin.clear(); // Clear the error flag
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
            std::cerr << "Invalid choice. Please enter a number between 1 and 4.\n";
        }
    } while (choice < 1 || choice > 4);

    bool printImmediately = (choice == 1 || choice == 3);

    auto startTime = std::chrono::steady_clock::now();
    std::time_t startWallClock = std::time(nullptr);
    std::cout << "\n=== Run started at " << std::ctime(&startWallClock) << std::endl;

    g_collectedPrimes.clear();

    // 4) Launch Scheme A or B
    std::vector<std::thread> threadsA;
    threadsA.reserve(numThreads);

    if (choice == 1 || choice == 2)
    {
        // Scheme A
        long rangeSize = maxNumber / numThreads;
        long start = 1;
        for (long i = 0; i < numThreads; ++i)
        {
            long end = (i == numThreads - 1)
                      ? maxNumber
                      : (start + rangeSize - 1);

            threadsA.emplace_back(workerRangeSchemeA,
                                  i,
                                  start,
                                  end,
                                  printImmediately);
            start = end + 1;
        }
    }
    else if (choice == 3 || choice == 4)
    {
        // Scheme B
        runSchemeB(maxNumber, numThreads, printImmediately);
    }
    else
    {
        std::cerr << "Invalid choice.\n";
        return 1;
    }

    // 5) Join threads
    for (auto &t : threadsA) {
        t.join();
    }

    // 6) If printing is to be done after
    if (!printImmediately)
    {
        std::sort(g_collectedPrimes.begin(), g_collectedPrimes.end());
        std::cout << "\n=== Primes found:\n";
        for (long p : g_collectedPrimes) {
            std::cout << p << " ";
        }
        std::cout << std::endl;
    }

    // 7) Print end time and total elapsed
    auto endTime = std::chrono::steady_clock::now();
    std::time_t endWallClock = std::time(nullptr);
    std::cout << "\n=== Run ended at " << std::ctime(&endWallClock) << std::endl;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       endTime - startTime).count();
    std::cout << "Total elapsed time: " << elapsed << " ms\n\n";

    return 0;
}
