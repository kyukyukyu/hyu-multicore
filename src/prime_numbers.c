#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Finds prime numbers between a and b, stores them in prime_numbers (only if
 * prime_numbers is not NULL), and returns the number of found prime numbers.
 */
size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          unsigned long** prime_numbers);
/*
 * Prints the list of prime numbers. The array of prime numbers and the number
 * of them should be provided.
 */
void print_prime_numbers(const unsigned long* prime_numbers,
                         const size_t n_prime);

int main(void) {
  /* Lower bound for finding prime numbers. */
  const unsigned long a = 1;
  /* Upper bound for finding prime numbers. */
  const unsigned long b = 1000000000;  /* 10^9 */
  /* The number of prime numbers in the range. */
  size_t n_prime;
  /* The list of found prime numbers. */
  unsigned long* prime_numbers;
  n_prime = find_prime_numbers(a, b, &prime_numbers);
  print_prime_numbers(prime_numbers, n_prime);
  printf("Total number of prime numbers between %lu and %lu is %lu.\n",
         a, b, n_prime);
  /* Set them free. */
  free(prime_numbers);
  return 0;
}

size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          unsigned long** prime_numbers) {
  size_t n_prime = 0;
  int store_numbers = !(prime_numbers == NULL);
  if (store_numbers) {
    /*
     * Allocate space for the list of prime numbers. The number of prime
     * numbers is unknown here. Therefore, allocate space for the half of
     * numbers between a and b.
     */
    *prime_numbers =
        (unsigned long*) malloc((b - a) / 2 * sizeof(unsigned long));
  }
  return n_prime;
}

void print_prime_numbers(const unsigned long* prime_numbers,
                         const size_t n_prime) {
  size_t i;
  for (i = 0; i < n_prime; ++i) {
    printf("%lu\n", prime_numbers[i]);
  }
}
