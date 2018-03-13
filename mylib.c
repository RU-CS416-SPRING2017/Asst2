#include "mylib.h"

// 8 * 1024 * 1024 = 2^23
#define MEMORY_SIZE 8388608
#define METADATA_SIZE sizeof(struct memoryMetadata)
#define BLOCK_SIZE(x) x + (METADATA_SIZE * 2)

struct memoryMetadata {
    size_t size;
    int used;
};

char memory[MEMORY_SIZE];

void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {
    
    struct memoryMetadata * head = (struct memoryMetadata *) memory;
    
    while (head->used) {
        head += BLOCK_SIZE(head->size);
    }

    if (((((char *) head) - memory) + BLOCK_SIZE(size)) > MEMORY_SIZE) {
        return 0;
    }

    head->used = 1;
    head->size = size;

    return head + METADATA_SIZE;
}
