# Programming Projects in 2015 Fall Multicore Programming Class @ HYU

## How to Build

```shell
make
```

## How to Run

### Project #2: Simple MVCC

```shell
cd bin
./homework --num_thread 5 --duration 180
```

- `-n` or `--num_thread`: The number of threads to use.
- `-d` or `--duration`: Duration for running MVCC, in seconds.
- `-v` or `--verify`: *Optional.* If used, data variables which are read based
on read-view are checked if their constant invariant is kept.
