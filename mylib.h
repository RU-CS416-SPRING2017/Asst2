#ifndef mylib_h
#define mylib_h

#include <sys/types.h>

#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ)
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ)

void * myallocate(size_t size, char * fileName, int lineNumber, int requester);
void mydeallocate(void * ptr, char * fileName, int lineNumber, int requester);

#endif
