# Multithreaded Prime Number Finder

This C++ program finds prime numbers up to a specified limit using multithreading. It supports two prime-finding schemes with options to print the results immediately or after computation.

## Features

- **Scheme A: Range Partition**
  - Divides the number range into chunks processed by different threads.
  - Two modes:
    - **A1:** Immediate prime output.
    - **A2:** Collect primes and output after processing.

- **Scheme B: Divisor Splitting**
  - For each number, checks for primality using multiple threads to divide the divisor range.
  - Two modes:
    - **B1:** Immediate prime output.
    - **B2:** Collect primes and output after processing.

## Requirements

- C++11 or later (due to threading support)
- g++ compiler or any compatible C++ compiler

## Compilation

```bash
g++ -std=c++11 -pthread -o main main.cpp
```

## Configuration

Create a `config.txt` file in the same directory as the executable with the following format:

```
threads=4
maxNumber=100000
```

- **threads:** Number of threads to use.
- **maxNumber:** The upper limit for prime checking.

## Running the Program

```bash
./main
```

You will be prompted to choose the desired scheme and output mode:

```
Choose approach:
  1) Scheme A (range partition) + immediate printing
  2) Scheme A (range partition) + print after
  3) Scheme B (divisor-splitting, up to sqrt) + immediate printing
  4) Scheme B (divisor-splitting, up to sqrt) + print after
Enter choice:
```

Enter the corresponding number to start the computation.
