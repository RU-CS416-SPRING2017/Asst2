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

// Sets the payloadSize in the head and tail of block
void setBlockPayloadSize(void * block, size_t payloadSize) {
    struct memoryMetadata * head = META_PTR(block);
    head->payloadSize = payloadSize;
    getTail(head)->payloadSize = payloadSize;
}

// Sets used flag in the head and tail of block
void setBlockUsed(void * block, int used) {
    struct memoryMetadata * head = META_PTR(block);
    head->used = used;
    getTail(head)->used = used;
}

// Sets metadata in the head and tail of block
void setBlockMetadata(void * block, int used, size_t payloadSize) {
    struct memoryMetadata * head = META_PTR(block);
    head->used = used;
    head->payloadSize = payloadSize;
    *getTail(head) = *head;
}

// Allocates memory of size between firstHead and lastTail.
// Returns 0 if no space available, else returns pointer
// to allocated memory.
void * allocateBetween(size_t size, struct memoryMetadata * firstHead, struct memoryMetadata * lastTail) {

    struct memoryMetadata * head = firstHead;
    
    while (head->used || size > head->payloadSize) {
        head = META_PTR(CHAR_PTR(head) + BLOCK_SIZE(head->payloadSize));
        if ((head - 1) == lastTail) { return 0; }
    }

    if ((size + TOTAL_METADATA_SIZE) >= head->payloadSize) {
        setBlockUsed(head, 1);
        return head + 1;
    }

    size_t nextPayloadSize = head->payloadSize - (size + TOTAL_METADATA_SIZE);
    setBlockMetadata(head, 1, size);
    setBlockMetadata(getTail(head) + 1, 0, nextPayloadSize);

    return head + 1;
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int requester) {

    if (MEMORY_HEAD->payloadSize == 0) {
        setBlockMetadata(memory, 0, MEMORY_SIZE - TOTAL_METADATA_SIZE);
    }
    
    return allocateBetween(size, MEMORY_HEAD, MEMORY_TAIL);
}

// Deallocates a block between firstHead and lastTail where the payload is refrenced by ptr
void deallocateBetween(void * ptr, struct memoryMetadata * firstHead, struct memoryMetadata * lastTail) {

    struct memoryMetadata * head = META_PTR(ptr) - 1;
    struct memoryMetadata * tail = getTail(head);
    
    if (head != firstHead) {
        struct memoryMetadata * previousTail = head - 1;
        if (previousTail->used == 0) {
            size_t newPayloadSize = head->payloadSize + previousTail->payloadSize + TOTAL_METADATA_SIZE;
            head = getHead(previousTail);
            setBlockPayloadSize(head, newPayloadSize);
        }
    }

    if (tail != lastTail) {
        struct memoryMetadata * nextHead = tail + 1;
        if (nextHead->used == 0) {
            size_t newPayloadSize = tail->payloadSize + nextHead->payloadSize + TOTAL_METADATA_SIZE;
            tail = getTail(nextHead);
            setBlockPayloadSize(head, newPayloadSize);
        }
    }

    setBlockUsed(head, 0);
}

// Frees memory refrenced by ptr that was previously allocated with myallocate
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request) {
    deallocateBetween(ptr, MEMORY_HEAD, MEMORY_TAIL);
}
