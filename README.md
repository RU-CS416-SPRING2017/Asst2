# Asst2: Segmented Paging

## Usage

Files needed to use this library:

* `my_pthread_t.h`
* `mylib.h`
* `my_pthread.c`
* `mylib.c`

Use `gcc` to compile the following source files:

* `my_pthread.c`
* `mylib.c`

### API

This is the API for the new functions added in Asst2 available to threads created with this library.

#### Synopsis

```c
#include "my_pthread_t.h"

void * threadAllocate(size_t size);
void * shalloc(size_t size);
void threadDeallocate(void * ptr);

#define malloc(size) threadAllocate(size)
#define free(ptr) threadDeallocate(ptr)
```

#### Description

The `threadAllocate()` function allocates `size` bytes and returns a pointer to the allocated memory. This memory con only be accessed by the calling thread. The memory is not initialized. If `size` is  0, then `threadAllocate()` returns `NULL`.

The `shalloc()` function is the same as `threadAllocate()` except that the allocated memory is accessible to all threads.

The `threadDeallocate()` function frees the memory space pointed to by `ptr`, which must have been returned by a previous call to `threadAllocate()` or `shalloc()`. Otherwise, or if `threadDeallocate(ptr)` has already been called before, undefined behavior occurs. If ptr is `NULL`, no operation is performed.

#### Return Value

The `threadAllocate()` and `shalloc()` functions return a pointer to the allocated memory that is suitably aligned for any kind of variable. The difference between the two is that the allocated memory from `threadAllocate()` can only be accessed by the calling thread while the allocated memory from `shalloc()` can be accessed by any thread. On error, these functions return `NULL`. An error occurs if there is not enough memory to allocate or if `threadAllocate()` is called before atleast one thread has been created through this library. `NULL` is also returned by a successful call to `threadAllocate()` or `shalloc()` with a `size` of zero.

The `threadDeallocate()` function returns no value.

## Prelude

### How Main Memory is Divided

The "main memory", "RAM", or "pysical memory" is a contiguous allocation of `MEM_SIZE` bytes referenced  by `memory`. The first `sizeof(struct memoryMetadata)` bytes is metadata. The metadata gives information on where the thread library "partition", page table, and shared memory "partition" is located. It tells the how many memory pages and swap file pages threre are. The metadata also stores the file descriptor for the swap file.

### Definitions

A "partition" is a chunk of memory that consists of atleast one "block" of memory. If there is more than one "block" in a partition, the "blocks" must be contiguous.

A "block" is a chunk of memory that consists of a "head", a "payload", and a "tail". the "Head" and "tail" are both identical metadata where the first `sizeof(int)` bytes tell if the "block" is used, and the rest of the meatadata tells the size of the payload. As the name implies, the "head" resides in the first `sizeof(sturct blockMetadata)` bytes of the "block" and "tail" resides in the last `sizeof(sturct blockMetadata)` bytes. The "payload" is placed inbetween the "head" and "tail" metadata.

### Main Memory Divisions

#### Thread Library "Partition"

This "partition" is used to allocate memory on library calls to `myallocate`.

The thread library "partition" starts right after `memory`'s metadata, pricicely at the address `memory + sizeof(struct memoryMetadata)`. It's size is calculated on initialization by first finding the space left in `memory` after reserving space for the metadata and shared memory "partition". The leftover space is then devided based on

## Implementation



### `myallocate`

When called, `myallocate` first checks if `memory` is initialized. If not, `memory` is initialized by setting the metadata and "partitions" for the thread library and threads according to `LIBRARY_MEMORY_WEIGHT` and `THREADS_MEMORY_WEIGHT`. `myallocate` allocates the requested size of memory in the requested "partition" using a first-fit algorithm.

### `mydeallocate`

When called, `mydeallocate` first checks if `ptr` is in the requested "parition". If it is, `mydeallocate` sets the corresponding "block" to free and coalesces with imediate neighboring "blocks" if they are free.