#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/* Type alias for bit sequences. */
typedef unsigned long seq_t;

#define SEQ_SIZE (8 * sizeof(seq_t))

/*
 * Finds prime numbers between a and b, stores them in prime_numbers (only if
 * prime_numbers is not NULL), and returns the number of found prime numbers.
 */
size_t find_prime_numbers(const unsigned long a,
                          const unsigned long b,
                          unsigned long** prime_numbers);
/*
 * Allocates space for the array of bit sequences which is used for seqs in
 * find_prime_numbers(), and returns the length of array. The upper bound of
 * search range and pointer to seqs should be provided.
 */
size_t alloc_seqs(const unsigned long b, seq_t** seqs);
/*
 * For every odd number smaller than sqrt(b), marks the multiples of the number
 * non-prime. In multi-threaded fashion.
 */
void sieve_map(const size_t n_seqs, const unsigned long b, seq_t* const seqs);
/*
 * Filter the result of sieve_map() to count the number of prime numbers
 * between a and b, and if required, fill the array of them. prime_numbers
 * should be NULL if filling the array is not required. Returns the number of
 * counted prime numbers.
 */
size_t sieve_filter(const seq_t* seqs, const size_t n_seqs,
                    const unsigned long a, const unsigned long b,
                    unsigned long* const prime_numbers);
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
  /* The number of prime numbers between a and b. */
  size_t n_prime = 0;
  /*
   * Flag that says if found prime numbers should be stored in prime_numbers.
   */
  int store_numbers = !(prime_numbers == NULL);
  /*
   * Pointer to the array of bit sequences. Each bit represents if an odd
   * number is prime or not. A bit with bigger significance represents bigger
   * number, and the sequence of bigger numbers is stored at bigger index. For
   * example, if sizeof(seq_t) is 32, seqs[0] is a bit sequence whose LSB
   * represents whether 1 is prime, and MSB represents whether 63 is prime.
   * Similarly, LSB of seqs[1] for 65, and MSB of seqs[1] for 127. Each bit is
   * set 1 if corresponding number is not prime, or 0 otherwise.
   */
  seq_t* seqs;
  /* The length of seqs. */
  size_t n_seqs;
  if (store_numbers) {
    /*
     * Allocate space for the list of prime numbers. The number of prime
     * numbers is unknown here. Therefore, allocate space for the half of
     * numbers between a and b.
     */
    *prime_numbers =
        (unsigned long*) malloc((b - a) / 2 * sizeof(unsigned long));
  }
  n_seqs = alloc_seqs(b, &seqs);
  /* Simple, ancient algorithm comes here: Sieve of Eratosthenes. */
  sieve_map(n_seqs, b, seqs);
  n_prime = sieve_filter(seqs, n_seqs, a, b,
                         store_numbers ? *prime_numbers : NULL);
  /* Set them free. */
  free(seqs);
  return n_prime;
}

size_t alloc_seqs(const unsigned long b, seq_t** seqs) {
  /* Rounding up b / SEQ_SIZE. */
  size_t n_seqs = (b + SEQ_SIZE / 2) / SEQ_SIZE;
  *seqs = (seq_t*) calloc(n_seqs, sizeof(seq_t));
  return n_seqs;
}

void sieve_map(const size_t n_seqs, const unsigned long b, seq_t* const seqs) {
}

size_t sieve_filter(const seq_t* seqs, const size_t n_seqs,
                    const unsigned long a, const unsigned long b,
                    unsigned long* const prime_numbers) {
  size_t n_prime = 0;
  return n_prime;
}

void print_prime_numbers(const unsigned long* prime_numbers,
                         const size_t n_prime) {
  size_t i;
  for (i = 0; i < n_prime; ++i) {
    printf("%lu\n", prime_numbers[i]);
  }
}