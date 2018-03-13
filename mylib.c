#include "mylib.h"
#define MEMSIZE 8 * 1024 * 1024

char memory[MEMSIZE];

void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {
    return memory;
}