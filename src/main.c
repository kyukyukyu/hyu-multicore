/*
 * Entry point of the program that does simple MVCC (Multi-Version Concurrency
 * Control) for two variables. See mvcc.h for how it works in detail.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mvcc.h"

#define ARG_ERROR(msg) \
    (fprintf(stderr, "Please provide correct argument: %s\n", (msg)))

/* Parses arguments passed to the program, and populates global variable
 * g_program_options with the result. The number of arguments and their values
 * should be given as input. Returns 0 if the parsing was successful.
 * Otherwise, corresponding error messeage is printed to stderr, and nonzero
 * value is returned. */
static int parse_args(int argc, char* argv[]);
/* Computes throughput of all threads and fairness based on the value of global
 * variable g_n_updates and prints them to stdout. Returns 0 if the printing
 * was successful. */
static int print_stats();

/* Program options. This is populated in main.c, and should not be modified
 * in any other files. */
static program_options_t g_program_options;

/* Pointer to the array of numbers of UPDATE operations for each thread. */
static int* g_n_updates;

/* Main function of the program. Returns 0 if program has finished without any
 * problem.*/
int main(int argc, char* argv[]) {
  if (parse_args(argc, argv)) {
    /* Parsing arguments was not successful. */
    fputs("Parsing arguments was not successful.\n", stderr);
    return 1;
  }
  /* Allocate memory space for UPDATE operation counts. */
  g_n_updates = (int*) calloc(g_program_options.n_threads, sizeof(int));
  if (NULL == g_n_updates) {
    /* Allocation was not successful. */
    fputs("Allocation of memory space for UPDATE operation counts was not "
          "successful.\n", stderr);
    return 1;
  }
  /* Initialize random seed. */
  srand48(time(NULL));
  if (run_mvcc(&g_program_options, g_n_updates)) {
    /* Something went wrong while running MVCC. */
    fputs("Something went wrong while running MVCC.\n", stderr);
    return 1;
  }
  if (print_stats()) {
    /* Printing statistics data was not successful. */
    return 1;
  }
  /* Free memory space allocated in run_mvcc(). */
  free(g_n_updates);
  return 0;
}

int parse_args(int argc, char* argv[]) {
  /* Return value of getopt_long(). */
  int c;
  while (1) {
    static struct option long_options[] = {
      {"num_thread", required_argument, NULL, 'n'},
      {"duration", required_argument, NULL, 'd'},
      {"verify", no_argument, &g_program_options.verify, 'v'},
      {0, 0, 0, 0}
    };
    /* Get next option. */
    c = getopt_long(argc, argv, "n:d:v", long_options, NULL);
    if (c == -1) {
      /* No more option to get. */
      break;
    }
    switch (c) {
    case 0:
      /* --verify is set. */
      break;

    case 'n':
      /* -n or --num_thread is set. */
      sscanf(optarg, "%d", &g_program_options.n_threads);
      break;

    case 'd':
      /* -d or --duration is set. */
      sscanf(optarg, "%d", &g_program_options.duration);
      break;

    case 'v':
      /* -v is set. */
      g_program_options.verify = 1;
      break;

    default:
      fprintf(stderr, "What?? '%c' ('%d')\n", c, c);
      return 1;
    }
  }
  if (g_program_options.n_threads <= 0) {
    ARG_ERROR("\"num_thread\" should be greater than 0");
    return 1;
  }
  if (g_program_options.duration <= 0) {
    ARG_ERROR("\"duration\" should be greater than 0");
    return 1;
  }
  return 0;
}

int print_stats() {
  /* Number of threads. */
  int n_threads = g_program_options.n_threads;
  /* Duration in seconds. */
  int duration = g_program_options.duration;
  /* Throughput. sum((n_updates / duration) for n_updates in g_n_updates) */
  double throughput = 0.0;
  /* Fairness. (s * s) / (n_threads * s2)
   * where s = sum(n_updates for n_updates in g_n_updates),
   *       s2 = sum(n_updates * n_updates for n_updates in g_n_updates) */
  double fairness = 0.0;
  /* s. */
  unsigned int s = 0;
  /* s2. */
  unsigned int s2 = 0;
  /* Thread ID. */
  int tid;
  for (tid = 0; tid < n_threads; ++tid) {
    /* Number of updates for thread #tid. */
    unsigned int n_updates = g_n_updates[tid];
    s += n_updates;
    s2 += n_updates * n_updates;
    throughput += (double) n_updates / (double) duration;
  }
  fairness = (double) (s * s) / (double) (n_threads * s2);
  /* Print throughput and fairness. */
  return (printf("Throughput: %lf\n", throughput) <= 0 ||
          printf("Fairness: %lf\n", fairness) <= 0);
}
