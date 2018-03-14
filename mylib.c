#include "mylib.h"

// 8 * 1024 * 1024 = 2^23
#define MEMORY_SIZE 8388608
#define METADATA_SIZE sizeof(struct memoryMetadata)
// Block size of block with payload x
#define BLOCK_SIZE(x) x + (METADATA_SIZE * 2)

#define CHAR_PTR(x) ((char *) (x))
#define META_PTR(x) ((struct memoryMetadata *) (x))

struct memoryMetadata {
    size_t size;
    int used;
};

char memory[MEMORY_SIZE];

void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {
    
    struct memoryMetadata * head = META_PTR(memory);
    
    while (head->used) {
        head = META_PTR(CHAR_PTR(head) + BLOCK_SIZE(head->size));
    }

    if (((CHAR_PTR(head) - memory) + BLOCK_SIZE(size)) > MEMORY_SIZE) {
        return 0;
    }

    head->used = 1;
    head->size = size;

    return CHAR_PTR(head) + METADATA_SIZE;
}
