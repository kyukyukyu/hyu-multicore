/*
 * A program that finds prime numbers between two integers using a modified
 * version of sieve of Eratosthenes, in multithreaded fashion.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#include "prime_numbers.h"
#include "taskqueue.h"
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  /* Flag for verbose finding. If nonzero, found prime numbers will be printed
   * to stdout, one number in one line. */
  static int verbose = 0;
  /* Lower bound for finding prime numbers. Defined with default value. */
  unsigned long a = 1;
  /* Upper bound for finding prime numbers. */
  unsigned long b;
  /* The number of threads to be used. */
  int n_threads;
  /* Return value from getopt_long() will be stored here. */
  int c;
  if (argc < 2) {
    fputs("Usage: homework -n num_thread [-s start] -e end [-v]",
          stderr);
    return 1;
  }
  while (1) {
    static struct option long_options[] = {
      {"num_thread", required_argument, NULL, 'n'},
      {"start", required_argument, NULL, 's'},
      {"end", required_argument, NULL, 'e'},
      {"verbose", no_argument, &verbose, 1},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "n:s:e:v", long_options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 0:
      /* --verbose is used. */
      break;

    case 'n':
      /* -n or --num_thread is used. */
      sscanf(optarg, "%d", &n_threads);
      break;

    case 's':
      /* -s or --start is used. */
      sscanf(optarg, "%lu", &a);
      break;

    case 'e':
      /* -e or --end is used. */
      sscanf(optarg, "%lu", &b);
      break;

    case 'v':
      /* -v is used. */
      verbose = 1;
      break;

    default:
      fprintf(stderr, "What?? '%c' (%d)\n", c, c);
      return 1;
    }
  }
  if (n_threads <= 0) {
    fputs("Please provide correct argument: \"num_thread\" should be greater "
          "than zero", stderr);
    return 1;
  }
  if (a >= b) {
    fputs("Please provide correct argument: \"start\" should be less than "
          "\"end\"", stderr);
    return 1;
  }
  /* The number of prime numbers in the range. */
  size_t n_prime;
  n_prime = find_prime_numbers(a, b, n_threads, verbose);
  printf("Total number of prime numbers between %lu and %lu is %lu.\n",
         a, b, n_prime);
  return 0;
}


