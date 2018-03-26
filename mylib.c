#include <malloc.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "my_pthread_t.h"

// Size macros
#define MEM_SIZE (8 * 1000 * 1000)
#define BLK_META_SIZE sizeof(struct blockMetadata)
#define DBL_BLK_META_SIZE (BLK_META_SIZE * 2)
#define MEM_META_SIZE sizeof(struct memoryMetadata)
#define BLK_SIZE(payloadSize) (payloadSize + DBL_BLK_META_SIZE)
// #define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define PG_TBL_ROW_SIZE sizeof(struct pageTableRow)
#define SWAP_SIZE (MEM_SIZE * 2)
#define SHRD_MEM_SIZE (PAGE_SIZE * 4)
#define THRD_META_SIZE sizeof(struct threadMemoryMetadata)

// Casting macros
#define VOID_PTR(x) ((void *) (x))
#define CHAR_PTR(x) ((char *) (x))
#define BLK_META_PTR(x) ((struct blockMetadata *) (x))
#define MEM_META_PTR(x) ((struct memoryMetadata *) (x))
#define PAGE_META_PTR(x) ((struct pageMetadata *) (x))
#define PG_TBL_ROW_PTR(x) ((struct pageTableRow *) (x))
#define THRD_META_PTR(x) ((struct threadMemoryMetadata *) (x))
#define UNSGND_LONG(x) ((unsigned long) (x))

// Direct access macros
#define MEM_INFO MEM_META_PTR(memory)
#define LIB_MEM_PART (MEM_INFO->libraryMemory)
#define PG_TBL (MEM_INFO->pageTable)
#define NUM_MEM_PGS (MEM_INFO->numMemPages)
#define NUM_SWAP_PGS (MEM_INFO->numSwapPages)
#define NUM_PGS (NUM_MEM_PGS + NUM_SWAP_PGS)
#define MEM_PGS CHAR_PTR(PG_TBL + NUM_PGS)
#define SWAP_FILE (MEM_INFO->swapfile)
#define SHRD_MEM_PART (MEM_INFO->sharedMemory)
#define THRD_MEM (THRD_META_PTR(MEM_PGS))

// Shorthand macros
#define SEEK_SWAP_FILE(offset) lseek(SWAP_FILE, offset, SEEK_SET)
#define READ_SWAP_PAGE(dest) read(SWAP_FILE, dest, PAGE_SIZE)
#define WRITE_SWAP_PAGE(src) write(SWAP_FILE, src, PAGE_SIZE)
#define COPY_PAGE(dest, page) memcpy(dest, page, PAGE_SIZE)
#define PROTECT(startPage, numPages) mprotect(startPage, numPages * PAGE_SIZE, PROT_NONE)
#define UNPROTECT(startPage, numPages) mprotect(startPage, numPages * PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC)

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
    unsigned long pageNumber;
    void * physicalLocation;
    off_t virtualLocation;
};

// Metadata for thread's memory
struct threadMemoryMetadata {
    struct memoryPartition partition;
};

// Metadata for memory
struct memoryMetadata {
    struct memoryPartition libraryMemory;
    struct pageTableRow * pageTable;
    size_t numMemPages;
    size_t numSwapPages;
    int swapfile;
    struct memoryPartition sharedMemory;
};

// "Main memory"
char * memory = NULL;
long PAGE_SIZE;
extern char block;
extern tcb * currentTcb;

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

// Adds size bytes to the current size of the partition
void extendPartition(struct memoryPartition * partition, size_t size) {
    if (partition->lastTail->used) {
        struct blockMetadata * newHead = partition->lastTail + 1;
        setBlockMetadata(newHead, 0, size - DBL_BLK_META_SIZE);
        partition->lastTail = getTail(newHead);
    } else {
        struct blockMetadata * lastHead = getHead(partition->lastTail);
        setBlockMetadata(lastHead, 0, lastHead->payloadSize + size);
        partition->lastTail = getTail(lastHead);
    }
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

// Protects all (mem) pages of the given thread
void protectAllPages(tcb * thread) {
    off_t i;
    for (i = 0; i < NUM_MEM_PGS; i++) {
        if (PG_TBL[i].thread == thread) {
            PROTECT(PG_TBL[i].physicalLocation, 1);
        }
    }
}

// Unprotects all (mem) pages of the given thread
void unprotectAllPages(tcb * thread) {
    off_t i;
    for (i = 0; i < NUM_MEM_PGS; i++) {
        if (PG_TBL[i].thread == thread) {
            UNPROTECT(PG_TBL[i].physicalLocation, 1);
        }
    }
}

// Swaps the 2 pages
void swapPages(struct pageTableRow * row1, struct pageTableRow * row2) {

    if (row1 != row2) {

        char temp[PAGE_SIZE];

        // Copy row1 into temp
        if (row1->physicalLocation) {
            COPY_PAGE(temp, row1->physicalLocation);
        } else {
            SEEK_SWAP_FILE(row1->virtualLocation);
            READ_SWAP_PAGE(temp);
        }

        // Copy row2 into row1 and temp into row2
        if (row2->physicalLocation) {
            if (row1->physicalLocation) {
                COPY_PAGE(row1->physicalLocation, row2->physicalLocation);
            } else {
                SEEK_SWAP_FILE(row1->virtualLocation);
                WRITE_SWAP_PAGE(row2->physicalLocation);
            }
            COPY_PAGE(row2->physicalLocation, temp);
        } else {
            if (row1->physicalLocation) {
                SEEK_SWAP_FILE(row2->virtualLocation);
                READ_SWAP_PAGE(row1->physicalLocation);
            } else {
                char temp2[PAGE_SIZE];
                SEEK_SWAP_FILE(row2->virtualLocation);
                READ_SWAP_PAGE(temp2);
                SEEK_SWAP_FILE(row1->virtualLocation);
                WRITE_SWAP_PAGE(temp2);
            }
            SEEK_SWAP_FILE(row2->virtualLocation);
            WRITE_SWAP_PAGE(temp);
        }

        // Swap the tcbs of both threads
        tcb * tempTcb = row1->thread;
        row1->thread = row2->thread;
        row2->thread = tempTcb;
        unsigned long tempPageNumber = row1->pageNumber;
        row1->pageNumber = row2->pageNumber;
        row2->pageNumber = tempPageNumber;
    }
}

void onBadAccess(int sig, siginfo_t * si, void * unused) {

    unsigned long offset = UNSGND_LONG(si->si_addr) - UNSGND_LONG(MEM_PGS);
    unsigned long pageNumber = offset / PAGE_SIZE;
    struct pageTableRow * pageAccessed = PG_TBL + pageNumber;
    struct pageTableRow * pageWanted = NULL;
    struct pageTableRow * firstFreePage = NULL;

    unsigned long i;
    for (i = 0; i < NUM_PGS; i++) {
        if (PG_TBL[i].thread == currentTcb && PG_TBL[i].pageNumber == pageNumber) {
            pageWanted = PG_TBL + i;
            break;
        } else if (!firstFreePage && !PG_TBL[i].thread) {
            firstFreePage = PG_TBL + i;
        }
    }

    if (pageWanted) {
        UNPROTECT(pageAccessed->physicalLocation, 1);
        swapPages(pageAccessed, pageWanted);
        PROTECT(pageWanted->physicalLocation, 1);
    } else if (firstFreePage) {
        if (firstFreePage->physicalLocation) { UNPROTECT(firstFreePage->physicalLocation, 1); }
        if (firstFreePage != pageAccessed) {
            UNPROTECT(pageAccessed->physicalLocation, 1);
            swapPages(pageAccessed, firstFreePage);
            PROTECT(firstFreePage->physicalLocation, 1);
        }
        pageAccessed->thread = currentTcb;
        pageAccessed->pageNumber = pageNumber;
        if (!pageNumber) {
            struct threadMemoryMetadata * threadMeta = THRD_META_PTR(pageAccessed->physicalLocation);
            threadMeta->partition = createPartition(threadMeta + 1, PAGE_SIZE - THRD_META_SIZE);
        }
    }  
}

void cleanup() {
    close(SWAP_FILE);
}

void initializeMemory() {

    PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

    // Allocating space
    memory = memalign(PAGE_SIZE, MEM_SIZE);
    
    // Calculating temporary numbers
    size_t libPlusThreadsSpace = MEM_SIZE - MEM_META_SIZE - SHRD_MEM_SIZE;
    size_t numDiv = LIBRARY_MEMORY_WEIGHT + THREADS_MEMORY_WEIGHT;
    size_t divSize = libPlusThreadsSpace / numDiv;
    size_t pageWithTableRowSize = PAGE_SIZE + PG_TBL_ROW_SIZE;
    size_t threadsMemorySize = divSize * THREADS_MEMORY_WEIGHT;
    size_t libraryMemorySize = libPlusThreadsSpace - threadsMemorySize;
    size_t numSwapPages = SWAP_SIZE / PAGE_SIZE;
    size_t memPgPlusMemTblSpace = threadsMemorySize - (numSwapPages * PG_TBL_ROW_SIZE);
    size_t numMemPages = memPgPlusMemTblSpace / pageWithTableRowSize;
    size_t numPages = numSwapPages + numMemPages;
    size_t pageTableSize = numPages * PG_TBL_ROW_SIZE;
    while ((MEM_META_SIZE + libraryMemorySize + pageTableSize) % PAGE_SIZE) {
        libraryMemorySize--;
        threadsMemorySize = libPlusThreadsSpace - libraryMemorySize;
        memPgPlusMemTblSpace = threadsMemorySize - (numSwapPages * PG_TBL_ROW_SIZE);
        numMemPages = memPgPlusMemTblSpace / pageWithTableRowSize;
        numPages = numSwapPages + numMemPages;
        pageTableSize = numPages * PG_TBL_ROW_SIZE;
    }

    // Setting memory's metadata
    LIB_MEM_PART = createPartition(MEM_INFO + 1, libraryMemorySize);
    SHRD_MEM_PART = createPartition(memory + MEM_SIZE - SHRD_MEM_SIZE, SHRD_MEM_SIZE);
    PG_TBL = PG_TBL_ROW_PTR(memory + MEM_META_SIZE + libraryMemorySize);
    NUM_MEM_PGS = numMemPages;
    NUM_SWAP_PGS = numSwapPages;
    SWAP_FILE = open("swapfile", O_CREAT|O_RDWR|O_TRUNC, S_IRUSR|S_IWUSR);

    // Fire cleanup when program exits
    atexit(cleanup);

    // Initializing page table
    off_t i;
    for (i = 0; i < numMemPages; i++) {
        PG_TBL[i].thread = NULL;
        PG_TBL[i].physicalLocation = MEM_PGS + (i * PAGE_SIZE);
        PG_TBL[i].virtualLocation = -1;
    }
    off_t j;
    for (j = 0; j < numSwapPages; j += PAGE_SIZE) {
        PG_TBL[i].thread = NULL;
        PG_TBL[i].physicalLocation = NULL;
        PG_TBL[i].virtualLocation = j;
        i++;
    }

    // Setting signal handler to be fired when pages
    // are are accessed.
    PROTECT(MEM_PGS, numMemPages);
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = onBadAccess;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "Fatal error setting up signal handler\n");
        exit(EXIT_FAILURE);
    }
}

// Returns 1 if thread can extend its pages
// else returns 0
int canExtend(tcb * thread) {
    unsigned int numThreadPages;
    unsigned int totalUsedPages;
    unsigned int i;
    for (i = 0; i < NUM_PGS; i++) {
        if (PG_TBL[i].thread) {
            totalUsedPages++;
            if (PG_TBL[i].thread == thread) {
                numThreadPages++;
                if (numThreadPages == NUM_MEM_PGS) { return 0; }
            }
        }
    }
    if (totalUsedPages == NUM_PGS) { return 0; }
    return 1;
}

// Allocates size bytes in memory and returns a pointer to it
void * myallocate(size_t size, char * fileName, int lineNumber, int request) {

    // If memory is null, initialize it
    if (!memory) {
        initializeMemory();
    }

    if (request == LIBRARYREQ) {
        return allocateFrom(size, &LIB_MEM_PART);

    } else if (request == THREADREQ) {

        block = 1;

        void * ret = allocateFrom(size, &(THRD_MEM->partition));
        if (canExtend(currentTcb)) {
            while (!ret) {
                extendPartition(&(THRD_MEM->partition), PAGE_SIZE);
                ret = allocateFrom(size, &(THRD_MEM->partition));
            }
        }

        block = 0;
        return ret;        

    } else { return NULL; }
}

void * threadAllocate(size_t size) {
    return myallocate(size, __FILE__, __LINE__, THREADREQ);
}

// Returns a shared regiion of memory
void * shalloc(size_t size) {

    // If memory is null, initialize it
    if (!memory) {
        initializeMemory();
    }

    block = 1;
    void * ret = allocateFrom(size, &SHRD_MEM_PART);
    block = 0;
    return ret;
}

// Deallocates a block between firstHead and lastTail where the payload is refrenced by ptr
// returns 1 if succesfull and 0 otherwise
int deallocateFrom(void * ptr, struct memoryPartition * partition) {

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
        return 1;

    } else { return 0; }
}

// Frees memory refrenced by ptr that was previously allocated with myallocate
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request) {
    if (request == LIBRARYREQ) { deallocateFrom(ptr, &LIB_MEM_PART); }
    else if (request == THREADREQ) {
        block = 1;
        if (!deallocateFrom(ptr, &(THRD_MEM->partition))) {
            deallocateFrom(ptr, &SHRD_MEM_PART);
        }
        block = 0;
    }
}

void threadDeallocate(void * ptr) {
    mydeallocate(ptr, __FILE__, __LINE__, THREADREQ);
}
