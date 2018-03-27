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

C programs that requires my thread library and memory manager need to include the folowing line:

```c
#include "my_pthread_t.h"
```

## Prelude

### Main Memory

The "main memory", "RAM", or "pysical memory" is a contiguous allocation of `MEM_SIZE` bytes referenced  by `memory`. The first `sizeof(struct memoryMetadata)` bytes is metadata. The metadata gives information on where the thread library "partition", page table, and shared memory partition is located. It tells the how many memory pages and swap file pages threre are. The metadata also stores the file descriptor for the swap file. <-- done

not done ------>

A "partition" is a chunk of memory that consists of atleast one "block" of memory. If there is more than one "block" in a partition, the "blocks" must be contiguous.

A "block" is a chunk of memory that consists of "head" metadata, a "payload", and "tail" metadata. "Head" and "tail" metadata are the same except "head" metadata resides in the first `sizeof(sturct blockMetadata)` bytes of the "block" and "tail" metadata resides in the last `sizeof(sturct blockMetadata)` bytes. The "payload" resides inbetween the "head" and "tail" metadata. The first `sizeof(int)` bytes of the "head" and "tail" metadata tell if the "block" is used, and the rest of the meatadata tells the size of the payload.

### `myallocate`

When called, `myallocate` first checks if `memory` is initialized. If not, `memory` is initialized by setting the metadata and "partitions" for the thread library and threads according to `LIBRARY_MEMORY_WEIGHT` and `THREADS_MEMORY_WEIGHT`. `myallocate` allocates the requested size of memory in the requested "partition" using a first-fit algorithm.

### `mydeallocate`

When called, `mydeallocate` first checks if `ptr` is in the requested "parition". If it is, `mydeallocate` sets the corresponding "block" to free and coalesces with imediate neighboring "blocks" if they are free.