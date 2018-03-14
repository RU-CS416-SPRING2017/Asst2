#include "mylib.h"

// Size macros
#define MEMORY_SIZE (8 * 1024 * 1024)
#define METADATA_SIZE sizeof(struct memoryMetadata)
#define TOTAL_METADATA_SIZE (METADATA_SIZE * 2)
#define BLOCK_SIZE(x) x + TOTAL_METADATA_SIZE // x is payload of block

// Casting macros
#define CHAR_PTR(x) ((char *) (x))
#define META_PTR(x) ((struct memoryMetadata *) (x))

// Macros to metadata ends of the memory
#define MEMORY_HEAD META_PTR(memory)
#define MEMORY_TAIL META_PTR(memory + MEMORY_SIZE - METADATA_SIZE)

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

// Initializes a block of memory with metadata
void initializeBlock(void * block, int used, size_t payloadSize) {
    struct memoryMetadata * head = META_PTR(block);
    head->used = used;
    head->payloadSize = payloadSize;
    *getTail(head) = *head;
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {
    
    struct memoryMetadata * head = MEMORY_HEAD;

    if (head->payloadSize == 0) {
        initializeBlock(memory, 0, MEMORY_SIZE - TOTAL_METADATA_SIZE);
    }
    
    while (head->used || head->payloadSize < size) {
        head = META_PTR(CHAR_PTR(head) + BLOCK_SIZE(head->payloadSize));
        if ((head - 1) == MEMORY_TAIL) { return 0; }
    }

    if ((size + TOTAL_METADATA_SIZE) >= head->payloadSize) {
        head->used = 1;
        getTail(head)->used = 1;
        return head + 1;
    }

    size_t nextPayloadSize = head->payloadSize - (size + METADATA_SIZE);
    initializeBlock(head, 1, size);
    initializeBlock(getTail(head) + 1, 0, nextPayloadSize);

    return head + 1;
}

// Frees memory refrenced by ptr that was previously allocated with myallocate
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request) {

    struct memoryMetadata * head = META_PTR(ptr) - 1;
    struct memoryMetadata * tail = getTail(head);
    
    if (head != MEMORY_HEAD) {
        struct memoryMetadata * previousTail = head - 1;
        if (previousTail->used == 0) {
            size_t newPayloadSize = head->payloadSize + previousTail->payloadSize + TOTAL_METADATA_SIZE;
            head = getHead(previousTail);
            initializeBlock(head, 0, newPayloadSize);
        }
    }

    if (tail != MEMORY_TAIL) {
        struct memoryMetadata * nextHead = tail + 1;
        if (nextHead->used == 0) {
            size_t newPayloadSize = tail->payloadSize + nextHead->payloadSize + TOTAL_METADATA_SIZE;
            tail = getTail(nextHead);
            initializeBlock(head, 0, newPayloadSize);
        }
    }

    head->used = 0;
    tail->used = 0;
}
