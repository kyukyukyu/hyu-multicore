#include <sys/types.h>

/*
 * Finds prime numbers between a and b with a number of threads (excluding the
 * main one), and returns the number of found prime numbers. If verbose is
 * nonzero value, found prime numbers will be printed to stdout, one number in
 * one line.
 */
size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          const int n_threads,
                          const int verbose);

