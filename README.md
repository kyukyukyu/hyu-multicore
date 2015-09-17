# Programming Projects in 2015 Fall Multicore Programming Class @ HYU

## How to Build

```shell
make
```

## How to Run

### Project #1: Finding prime numbers between two positive integers using threads

```shell
cd bin
./homework --num_thread 8 --start 90000000 --end 100000000 --verbose
```

- `-n` or `--num_thread`: The number of threads to use.
- `-s` or `--start`: The exclusive lower bound for finding prime numbers.
- `-e` or `--end`: The exclusive upper bound for finding prime numbers.
- `-v` or `--verbose`: *Optional.* If used, found prime numbers will be printed to `stdout`.
