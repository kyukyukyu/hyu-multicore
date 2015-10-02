/*
 * Entry point of the program that does simple MVCC (Multi-Version Concurrency
 * Control) for two variables. See mvcc.h for how it works in detail.
 *
 * Author: Sanggyu Nam <pokeplus@gmail.com>
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "mvcc.h"

/* Parses arguments passed to the program, and populates global variable
 * g_program_options with the result. The number of arguments and their values
 * should be given as input. Returns 0 if the parsing was successful.
 * Otherwise, corresponding error messeage is printed to stderr, and nonzero
 * value is returned. */
int parse_args(int argc, char* argv[]);
/* Computes throughput of all threads and fairness based on the value of global
 * variable g_n_updates and prints them to stdout. Returns 0 if the printing
 * was successful. */
int print_stats();

/* Main function of the program. Returns 0 if program has finished without any
 * problem.*/
int main(int argc, char* argv[]) {
  if (parse_args(argc, argv)) {
    /* Parsing arguments was not successful. */
    fputs("Parsing arguments was not successful.\n", stderr);
    return 1;
  }
  if (run_mvcc()) {
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
