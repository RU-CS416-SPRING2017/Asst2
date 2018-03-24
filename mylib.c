#include <malloc.h>
#include "my_pthread_t.h"
#include <sys/mman.h>
#include <fcntl.h>

// Size macros
#define MEM_SIZE (8 * 1000 * 1000)
#define BLK_META_SIZE sizeof(struct blockMetadata)
#define DBL_BLK_META_SIZE (BLK_META_SIZE * 2)
#define MEM_META_SIZE sizeof(struct memoryMetadata)
#define BLK_SIZE(x) (x + DBL_BLK_META_SIZE) // x is payload size of block
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define PG_TBL_ROW_SIZE sizeof(struct pageTableRow)
#define SWAP_SIZE (MEM_SIZE * 2)
#define SHRD_MEM_SIZE (PAGE_SIZE * 4)

// Casting macros
#define VOID_PTR(x) ((void *) (x))
#define CHAR_PTR(x) ((char *) (x))
#define BLK_META_PTR(x) ((struct blockMetadata *) (x))
#define MEM_META_PTR(x) ((struct memoryMetadata *) (x))
#define PAGE_META_PTR(x) ((struct pageMetadata *) (x))
#define PG_TBL_ROW_PTR(x) ((struct pageTableRow *) (x))

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

// Repressents a row in the page table
struct pageTableRow {
    tcb * thread;
    unsigned int pageNumber;
    void * pysicalLocation;
    off_t virtualLocation;
};

// Metadata for memory
struct memoryMetadata {
    struct memoryPartition libraryMemory;
    struct pageTableRow * pageTable;
    size_t numPages;
    struct memoryPartition sharedMemory;
    int swapfile;
};

// "Main memory"
char * memory = NULL;

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

void onAccess(int sig, siginfo_t * si, void * unused) {
    printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int request) {

    // If memory is null, initialize it
    if (!memory) {

        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = onAccess;

        if (sigaction(SIGSEGV, &sa, NULL) == -1) {
            fprintf(stderr, "Fatal error setting up signal handler\n");
            exit(EXIT_FAILURE);
        }

        // Allocating space
        memory = memalign(PAGE_SIZE, MEM_SIZE);
        mprotect(memory, MEM_SIZE, PROT_NONE);
        // mprotect(memory, MEM_SIZE, PROT_WRITE|PROT_READ);
        struct memoryMetadata * memoryInfo = MEM_META_PTR(memory);
        
        // Calculating temporary numbers
        size_t spaceLeft = MEM_SIZE - MEM_META_SIZE - SHRD_MEM_SIZE;
        size_t numDiv = LIBRARY_MEMORY_WEIGHT + THREADS_MEMORY_WEIGHT;
        size_t divSize = spaceLeft / numDiv;
        size_t pageWithTableRowSize = PAGE_SIZE + PG_TBL_ROW_SIZE;
        size_t threadsMemorySize = divSize * THREADS_MEMORY_WEIGHT;
        size_t numSwapPages = SWAP_SIZE / PAGE_SIZE;
        // size_t swapPageTableSize = numSwapPages * PG_TBL_ROW_SIZE;
        size_t numMemPages = (threadsMemorySize - numSwapPages) / pageWithTableRowSize;
        // size_t memPageTableSize = numMemPages * PG_TBL_ROW_SIZE;
        size_t numPages = numSwapPages + numMemPages;
        // size_t pageTableSize = swapPageTableSize + memPageTableSize;
        threadsMemorySize = numMemPages * PAGE_SIZE;
        size_t libraryMemorySize = spaceLeft - threadsMemorySize;

        // Setting memory's metadata
        memoryInfo->libraryMemory = createPartition(memoryInfo + 1, libraryMemorySize);
        memoryInfo->sharedMemory = createPartition(memory + MEM_META_SIZE + spaceLeft, SHRD_MEM_SIZE);
        memoryInfo->swapfile = open("swapfile", O_CREAT|O_RDWR|O_TRUNC);
        memoryInfo->pageTable = PG_TBL_ROW_PTR(memory + MEM_META_SIZE + libraryMemorySize);
        memoryInfo->numPages = numPages;

        // Initializing page table
        char * memPages = CHAR_PTR(memoryInfo->pageTable + numPages);
        off_t i;
        for (i = 0; i < numMemPages; i++) {
            memoryInfo->pageTable[i].thread = NULL;
            memoryInfo->pageTable[i].pysicalLocation = memPages + (i * PAGE_SIZE);
            memoryInfo->pageTable[i].virtualLocation = -1;
        }
        off_t j;
        for (j = 0; j < numSwapPages; j++) {
            memoryInfo->pageTable[i].thread = NULL;
            memoryInfo->pageTable[i].pysicalLocation = NULL;
            memoryInfo->pageTable[i].virtualLocation = j;
            i++;
        }  
    }

    struct memoryMetadata * memoryInfo = MEM_META_PTR(memory);

    if (request == LIBRARYREQ) {
        return allocateFrom(size, &(memoryInfo->libraryMemory));

    } else if (request == THREADREQ) {

        // char * threadsMemory;
        // for (
        //     threadsMemory = memoryInfo->threadsMemory;
        //     threadsMemory <= (memory + MEM_SIZE - PAGE_SIZE);
        //     threadsMemory += PAGE_SIZE

        // ) {     
        //     struct pageMetadata * threadPage = PAGE_META_PTR(threadsMemory);
        //     if (!(threadPage->thread)) {
        //         initializePage(threadPage, currentTcb);
        //         return allocateFrom(size, &(threadPage->partition));
        //     } else if (currentTcb == threadPage->thread) {
        //         return allocateFrom(size, &(threadPage->partition));
        //     }
        // }

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
    // struct memoryMetadata * memoryInfo = MEM_META_PTR(memory);
    // if (request == LIBRARYREQ) { deallocateFrom(ptr, &(memoryInfo->libraryMemory)); }
    // else if (request == THREADREQ) { deallocateFrom(ptr, &(memoryInfo->threadsMemory)); }
}
