#ifndef mylib_h
#define mylib_h

#include <sys/types.h>

#define THREADREQ 1
#define LIBRARYREQ 0

#define malloc(size) threadAllocate(size)
#define free(ptr) threadDeallocate(ptr)

void * threadAllocate(size_t size);
void * shalloc(size_t size);
void threadDeallocate(void * ptr);

#endif
