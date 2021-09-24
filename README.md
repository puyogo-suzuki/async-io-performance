# async-io-performance
Research on Asynchronous I/O performance

# How to use
TBD

# APIs

## Unix Synchronous API
This is synchronous, and blocking.

### Key Functions

```c
ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
```

## Unix Asynchronous API

TBD

## Linux Asynchronous API

TBD

# Benchmark
I'm going to implement epoll version and other OSes.
I'll bench after implementing.

## 32KiB file
### [sync-unix.c](sync-unix.c)

```
real    0m1.358s
user    0m0.000s
sys     0m0.369s
```

### [async-unix.c](async-unix.c)

```
real    0m1.271s
user    0m0.000s
sys     0m0.471s
```

### [async-unix2.c](async-unix2.c)

```
real    0m1.251s
user    0m0.030s
sys     0m0.449s
```

### [async-linux.c](async-linux.c)

```
real    0m0.097s
user    0m0.003s
sys     0m0.000s
```