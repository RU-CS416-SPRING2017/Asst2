#include "mylib.h"

// Size macros
#define MEM_SIZE (8 * 1024 * 1024)
#define BLK_META_SIZE sizeof(struct blockMetadata)
#define DBL_BLK_META_SIZE (BLK_META_SIZE * 2)
#define MEM_META_SIZE sizeof(struct memoryMetadata)
#define BLK_SIZE(x) (x + DBL_BLK_META_SIZE) // x is payload size of block

// Casting macros
#define VOID_PTR(x) ((void *) (x))
#define CHAR_PTR(x) ((char *) (x))
#define BLK_META_PTR(x) ((struct blockMetadata *) (x))
#define MEM_META_PTR(x) ((struct memoryMetadata *) (x))

// These macros determine how much of memory should
// be partitioned for the thread library vs threads
#define LIBRARY_MEMORY_WEIGHT 1
#define THREADS_MEMORY_WEIGHT 1

// Metadata for a block in memory.
// Used in head and tail.
struct blockMetadata {
    size_t payloadSize;
    int used;
};

// Holds info for a partition in memory
struct memoryPartition {
    struct blockMetadata * firstHead;
    struct blockMetadata * lastTail;
};

// Metadata for memory
struct memoryMetadata {
    int initialized;
    struct memoryPartition library;
    struct memoryPartition threads;
};

// "Main memory"
char memory[MEM_SIZE] = { 0 };

// Returns the tail of a block whose initialized head is given
struct blockMetadata * getTail(struct blockMetadata * head) {
    return BLK_META_PTR(CHAR_PTR(head) + BLK_META_SIZE + head->payloadSize);
}

// Returns the head of a block whose initialized tail is given
struct blockMetadata * getHead(struct blockMetadata * tail) {
    return BLK_META_PTR(CHAR_PTR(tail) - BLK_META_SIZE - tail->payloadSize);
}

// Sets the payloadSize in the head and tail of block
void setBlockPayloadSize(void * block, size_t payloadSize) {
    struct blockMetadata * head = BLK_META_PTR(block);
    head->payloadSize = payloadSize;
    getTail(head)->payloadSize = payloadSize;
}

// Sets used flag in the head and tail of block
void setBlockUsed(void * block, int used) {
    struct blockMetadata * head = BLK_META_PTR(block);
    head->used = used;
    getTail(head)->used = used;
}

// Sets metadata in the head and tail of block
void setBlockMetadata(void * block, int used, size_t payloadSize) {
    struct blockMetadata * head = BLK_META_PTR(block);
    head->used = used;
    head->payloadSize = payloadSize;
    *getTail(head) = *head;
}

// Allocates memory of size between firstHead and lastTail.
// Returns 0 if no space available, else returns pointer
// to allocated memory.
void * allocateFrom(size_t size, struct memoryPartition * partition) {

    struct blockMetadata * head = partition->firstHead;
    
    while (head->used || size > head->payloadSize) {
        head = BLK_META_PTR(CHAR_PTR(head) + BLK_SIZE(head->payloadSize));
        if ((head - 1) ==partition->lastTail) { return 0; }
    }

    if ((size + DBL_BLK_META_SIZE) >= head->payloadSize) {
        setBlockUsed(head, 1);
        return head + 1;
    }

    size_t nextPayloadSize = head->payloadSize - (size + DBL_BLK_META_SIZE);
    setBlockMetadata(head, 1, size);
    setBlockMetadata(getTail(head) + 1, 0, nextPayloadSize);

    return head + 1;
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int request) {

    struct memoryMetadata * memoryInfo = MEM_META_PTR(memory);

    if (!(memoryInfo->initialized)) {
        
        size_t spaceLeft = MEM_SIZE - MEM_META_SIZE;
        size_t numDiv = LIBRARY_MEMORY_WEIGHT + THREADS_MEMORY_WEIGHT;
        size_t divSize = spaceLeft / numDiv;

        size_t threadsPartitionSize = divSize * THREADS_MEMORY_WEIGHT;
        size_t threadsPayloadSize = threadsPartitionSize - DBL_BLK_META_SIZE;
        size_t libraryPartitionSize = spaceLeft - threadsPartitionSize;
        size_t libraryPayloadSize = libraryPartitionSize - DBL_BLK_META_SIZE;

        struct blockMetadata * libraryHead = BLK_META_PTR(memoryInfo + 1);
        setBlockMetadata(libraryHead, 0, libraryPayloadSize);
        struct blockMetadata * libraryTail = getTail(libraryHead);

        struct blockMetadata * threadsHead = libraryTail + 1;
        setBlockMetadata(threadsHead, 0, threadsPayloadSize);
        struct blockMetadata * threadsTail = getTail(threadsHead);

        memoryInfo->library.firstHead = libraryHead;
        memoryInfo->library.lastTail = libraryTail;
        memoryInfo->threads.firstHead = threadsHead;
        memoryInfo->threads.lastTail = threadsTail;
    }

    if (request == LIBRARYREQ) { return allocateFrom(size, &(memoryInfo->library)); }
    else if (request == THREADREQ) { return allocateFrom(size, &(memoryInfo->threads)); }
    else { return 0; }
}

// Deallocates a block between firstHead and lastTail where the payload is refrenced by ptr
void deallocateFrom(void * ptr, struct memoryPartition * partition) {

    if (ptr >= VOID_PTR(partition->firstHead + 1) && ptr < VOID_PTR(partition->lastTail)) {

        struct blockMetadata * head = BLK_META_PTR(ptr) - 1;
        struct blockMetadata * tail = getTail(head);
        
        if (head != partition->firstHead) {
            struct blockMetadata * previousTail = head - 1;
            if (previousTail->used == 0) {
                size_t newPayloadSize = head->payloadSize + previousTail->payloadSize + DBL_BLK_META_SIZE;
                head = getHead(previousTail);
                setBlockPayloadSize(head, newPayloadSize);
            }
        }

        if (tail != partition->lastTail) {
            struct blockMetadata * nextHead = tail + 1;
            if (nextHead->used == 0) {
                size_t newPayloadSize = tail->payloadSize + nextHead->payloadSize + DBL_BLK_META_SIZE;
                tail = getTail(nextHead);
                setBlockPayloadSize(head, newPayloadSize);
            }
        }

        setBlockUsed(head, 0);
    }
}

// Frees memory refrenced by ptr that was previously allocated with myallocate
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request) {
    struct memoryMetadata * memoryInfo = MEM_META_PTR(memory);
    if (request == LIBRARYREQ) { deallocateFrom(ptr, &(memoryInfo->library)); }
    else if (request == THREADREQ) { deallocateFrom(ptr, &(memoryInfo->threads)); }
}
