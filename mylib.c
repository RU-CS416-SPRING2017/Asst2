#include "my_pthread_t.h"

// Size macros
#define MEM_SIZE (8 * 1024 * 1024)
#define BLK_META_SIZE sizeof(struct blockMetadata)
#define DBL_BLK_META_SIZE (BLK_META_SIZE * 2)
#define MEM_META_SIZE sizeof(struct memoryMetadata)
#define BLK_SIZE(x) (x + DBL_BLK_META_SIZE) // x is payload size of block
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define PAGE_META_SIZE sizeof(struct pageMetadata)

// Casting macros
#define VOID_PTR(x) ((void *) (x))
#define CHAR_PTR(x) ((char *) (x))
#define BLK_META_PTR(x) ((struct blockMetadata *) (x))
#define MEM_META_PTR(x) ((struct memoryMetadata *) (x))
#define PAGE_META_PTR(x) ((struct pageMetadata *) (x))

// These macros determine how much of memory should
// be partitioned for the thread library vs threads
#define LIBRARY_MEMORY_WEIGHT 1
#define THREADS_MEMORY_WEIGHT 1

// Metadata for a block in memory.
// Used in head and tail.
struct blockMetadata {
    int used;
    size_t payloadSize;
};

// Holds info for a partition in memory
struct memoryPartition {
    struct blockMetadata * firstHead;
    struct blockMetadata * lastTail;
};

// Metada for a thread page.
struct pageMetadata {
    tcb * thread;
    struct memoryPartition partition;
    int inUse; //checks if page is in use
    int pageCount; //page counter

};

// Metadata for memory
struct memoryMetadata {
    struct memoryPartition libraryMemory;
    char * threadsMemory;
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

// Creates a partition
struct memoryPartition createPartition(void * partition, size_t size) {
    size_t payloadSize = size - DBL_BLK_META_SIZE;
    setBlockMetadata(partition, 0, payloadSize);
    struct memoryPartition ret;
    ret.firstHead = partition;
    ret.lastTail = getTail(partition);
    return ret;
}

// Initializes a page at page for thread
void initializePage(struct pageMetadata * page, tcb * thread) {
    page->thread = thread;
    page->partition = createPartition(page + 1, PAGE_SIZE - PAGE_META_SIZE);
}

struct pageMetadata * getPage(char * mem, tcb * thread) {

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

    if (!(memoryInfo->threadsMemory)) {
        
        size_t spaceLeft = MEM_SIZE - MEM_META_SIZE;
        size_t numDiv = LIBRARY_MEMORY_WEIGHT + THREADS_MEMORY_WEIGHT;
        size_t divSize = spaceLeft / numDiv;

        size_t threadsMemorySize = divSize * THREADS_MEMORY_WEIGHT;
        size_t libraryMemorySize = spaceLeft - threadsMemorySize;

        memoryInfo->libraryMemory = createPartition(memoryInfo + 1, libraryMemorySize);
        memoryInfo->threadsMemory = CHAR_PTR(memoryInfo->libraryMemory.lastTail + 1);
    }

    if (request == LIBRARYREQ) {
        return allocateFrom(size, &(memoryInfo->libraryMemory));

    } else if (request == THREADREQ) {

        char * threadsMemory;
        for (
            threadsMemory = memoryInfo->threadsMemory;
            threadsMemory <= (memory + MEM_SIZE - PAGE_SIZE);
            threadsMemory += PAGE_SIZE

        ) {     
            struct pageMetadata * threadPage = PAGE_META_PTR(threadsMemory);
            if (!(threadPage->thread)) {
                initializePage(threadPage, currentTcb);
                return allocateFrom(size, &(threadPage->partition));
            } else if (currentTcb == threadPage->thread) {
                return allocateFrom(size, &(threadPage->partition));
            }
        }

        return 0;

    } else { return 0; }
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
    if (request == LIBRARYREQ) { deallocateFrom(ptr, &(memoryInfo->libraryMemory)); }
    else if (request == THREADREQ) { deallocateFrom(ptr, &(memoryInfo->threadsMemory)); }
}
