#include "mylib.h"

// Size macros
#define MEMORY_SIZE (8 * 1024 * 1024)
#define METADATA_SIZE sizeof(struct memoryMetadata)
#define BLOCK_SIZE(x) x + (METADATA_SIZE * 2) // x is payload of block

// Casting macros
#define CHAR_PTR(x) ((char *) (x))
#define META_PTR(x) ((struct memoryMetadata *) (x))

// Metadata for a block of memory.
// Used in head and tail.
struct memoryMetadata {
    size_t payloadSize;
    int used;
};

// "Main memory"
char memory[MEMORY_SIZE];

// Returns the tail of a block whose initialized head is given
struct memoryMetadata * getTail(struct memoryMetadata * head) {
    return META_PTR(CHAR_PTR(head) + METADATA_SIZE + head->payloadSize);
}

// Returns the head of a block whose initialized tail is given
struct memoryMetadata * getHead(struct memoryMetadata * tail) {
    return META_PTR(CHAR_PTR(tail) - METADATA_SIZE - tail->payloadSize);
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {
    
    struct memoryMetadata * head = META_PTR(memory);
    
    while (head->used) {
        head = META_PTR(CHAR_PTR(head) + BLOCK_SIZE(head->payloadSize));
    }

    if (((CHAR_PTR(head) - memory) + BLOCK_SIZE(size)) > MEMORY_SIZE) {
        return 0;
    }

    head->used = 1;
    head->payloadSize = size;
    *getTail(head) = *head;

    return CHAR_PTR(head) + METADATA_SIZE;
}
