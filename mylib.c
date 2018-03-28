#include <malloc.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "my_pthread_t.h"

// Size macros
#define MEM_SIZE (8 * 1000 * 1000)
#define SWAP_SIZE (MEM_SIZE * 2)
#define SHRD_MEM_SIZE (pageSize * 4)
#define MEM_META_SIZE sizeof(struct memoryMetadata)
#define BLK_META_SIZE sizeof(struct blockMetadata)
#define DBL_BLK_META_SIZE (BLK_META_SIZE * 2)
#define BLK_SIZE(payloadSize) (payloadSize + DBL_BLK_META_SIZE)
#define PG_TBL_ROW_SIZE sizeof(struct pageTableRow)
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
#define THRD_MEM (THRD_META_PTR(MEM_PGS))
#define THRD_MEM_PART (THRD_MEM->partition)
#define SWAP_FILE (MEM_INFO->swapfile)
#define SHRD_MEM_PART (MEM_INFO->sharedMemory)

// Shorthand macros
#define COPY_PAGE(dest, page) memcpy(dest, page, pageSize)

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

// Stores page size on memory initialization
long pageSize;

// Variables from the thread library
extern char block;
extern tcb * currentTcb;

// Seeks swapFile and exits on error
void seekSwapFile(off_t offset) {
    if (lseek(SWAP_FILE, offset, SEEK_SET) == ((off_t) -1)) {
        fprintf(stderr, "Error seeking swapFile: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Reads a page from swapFile at the current seeked
// position and stores it into buf. Exits on error.
void readSwapFilePage(void * buf) {
    if (read(SWAP_FILE, buf, pageSize) == -1) {
        fprintf(stderr, "Error reading from swapFile: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Writes a page from buf into swapFile at the
// current seeked position. Exits on error.
void writeSwapFilePage(void * buf) {
    if (write(SWAP_FILE, buf, pageSize) == -1) {
        fprintf(stderr, "Error writing to swapFile: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Protects numPages pages starting at startPage and exits on error
void protectPages(void * startPage, size_t numPages) {
    if (mprotect(startPage, numPages * pageSize, PROT_NONE) == -1) {
        fprintf(stderr, "Error protecting %ld pages starting at %p: %s\n", numPages, startPage, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Unprotects numPages pages starting at startPage and exits on error
void unprotectPages(void * startPage, size_t numPages) {
    if (mprotect(startPage, numPages * pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
        fprintf(stderr, "Error unprotecting %ld pages starting at %p: %s\n", numPages, startPage, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

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

// Creates a size bytes partition starting at ptr and returns it
struct memoryPartition createPartition(void * ptr, size_t size) {
    size_t payloadSize = size - DBL_BLK_META_SIZE;
    setBlockMetadata(ptr, 0, payloadSize);
    struct memoryPartition ret;
    ret.firstHead = ptr;
    ret.lastTail = getTail(ptr);
    return ret;
}

// Adds size bytes to the current size of partition
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

// Protects all memory pages of the given thread
void protectAllPages(tcb * thread) {
    off_t i;
    for (i = 0; i < NUM_MEM_PGS; i++) {
        if (PG_TBL[i].thread == thread) {
            protectPages(PG_TBL[i].physicalLocation, 1);
        }
    }
}

// Unprotects all memory pages of the given thread
void unprotectAllPages(tcb * thread) {
    off_t i;
    for (i = 0; i < NUM_MEM_PGS; i++) {
        if (PG_TBL[i].thread == thread) {
            unprotectPages(PG_TBL[i].physicalLocation, 1);
        }
    }
}

// Swaps the 2 pages. Ends up with row1 refrencing the same
// memory but now with the page that was originally in row2's
// memory. Threads and page numbers are also swapped.
void swapPages(struct pageTableRow * row1, struct pageTableRow * row2) {

    // Don't swap if rows are the same
    if (row1 != row2) {

        char temp[pageSize];

        // Copy row1 into temp
        if (row1->physicalLocation) {
            COPY_PAGE(temp, row1->physicalLocation);
        } else {
            seekSwapFile(row1->virtualLocation);
            readSwapFilePage(temp);
        }

        // Copy row2 into row1 and temp into row2
        if (row2->physicalLocation) {
            if (row1->physicalLocation) {
                COPY_PAGE(row1->physicalLocation, row2->physicalLocation);
            } else {
                seekSwapFile(row1->virtualLocation);
                writeSwapFilePage(row2->physicalLocation);
            }
            COPY_PAGE(row2->physicalLocation, temp);
        } else {
            if (row1->physicalLocation) {
                seekSwapFile(row2->virtualLocation);
                readSwapFilePage(row1->physicalLocation);
            } else {
                char temp2[pageSize];
                seekSwapFile(row2->virtualLocation);
                readSwapFilePage(temp2);
                seekSwapFile(row1->virtualLocation);
                writeSwapFilePage(temp2);
            }
            seekSwapFile(row2->virtualLocation);
            writeSwapFilePage(temp);
        }

        // Swap the tcbs and pageNumbers of both threads
        tcb * tempTcb = row1->thread;
        row1->thread = row2->thread;
        row2->thread = tempTcb;
        unsigned long tempPageNumber = row1->pageNumber;
        row1->pageNumber = row2->pageNumber;
        row2->pageNumber = tempPageNumber;
    }
}

// This function is fired when a thread is trying
// to access it's page but it's not there.
void onBadAccess(int sig, siginfo_t * si, void * unused) {

    // Calculating the page number accessed
    unsigned long offset = UNSGND_LONG(si->si_addr) - UNSGND_LONG(MEM_PGS);
    unsigned long pageNumber = offset / pageSize;

    struct pageTableRow * pageAccessed = PG_TBL + pageNumber;
    struct pageTableRow * pageWanted = NULL;
    struct pageTableRow * firstFreePage = NULL;

    // Search page table for appropriate page
    unsigned long i;
    for (i = 0; i < NUM_PGS; i++) {
        if (PG_TBL[i].thread == currentTcb && PG_TBL[i].pageNumber == pageNumber) {
            pageWanted = PG_TBL + i;
            break;
        } else if (!firstFreePage && !PG_TBL[i].thread) {
            firstFreePage = PG_TBL + i;
        }
    }

    // Swap pages if thread owns the the page it was trying to access
    if (pageWanted) {
        unprotectPages(pageAccessed->physicalLocation, 1);
        swapPages(pageAccessed, pageWanted);
        protectPages(pageWanted->physicalLocation, 1);

    // Give the thread an unused page if it doesn't have one and then swap
    } else if (firstFreePage) {
        if (firstFreePage->physicalLocation) {
            unprotectPages(firstFreePage->physicalLocation, 1);
        }
        if (firstFreePage != pageAccessed) {
            unprotectPages(pageAccessed->physicalLocation, 1);
            swapPages(pageAccessed, firstFreePage);
            if (firstFreePage->physicalLocation) {
                protectPages(firstFreePage->physicalLocation, 1);
            }
        }
        pageAccessed->thread = currentTcb;
        pageAccessed->pageNumber = pageNumber;

        // If this is the thread's first page, initialize it's metadata
        if (!pageNumber) {
            struct threadMemoryMetadata * threadMeta = THRD_META_PTR(pageAccessed->physicalLocation);
            threadMeta->partition = createPartition(threadMeta + 1, pageSize - THRD_META_SIZE);
        }
    }  
}

// Last function called before
// program exits. Closes swapFile.
void cleanup() {
    close(SWAP_FILE);
}

// Initializes the memory manager. Exits on error.
void initializeMemory() {

    // Storing system page size
    pageSize = sysconf(_SC_PAGE_SIZE);
    if (pageSize == -1) {
        fprintf(stderr, "Error storing system page size: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Allocating page alligned memory space
    memory = memalign(pageSize, MEM_SIZE);
    if (!memory) {
        fprintf(stderr, "Error allocating alligned memory\n");
        exit(EXIT_FAILURE);
    }
    
    // Calculating numbers for properly alligned boundries in memory
    size_t libPlusThreadsSpace = MEM_SIZE - MEM_META_SIZE - SHRD_MEM_SIZE;
    size_t numDiv = LIBRARY_MEMORY_WEIGHT + THREADS_MEMORY_WEIGHT;
    size_t divSize = libPlusThreadsSpace / numDiv;
    size_t pageWithTableRowSize = pageSize + PG_TBL_ROW_SIZE;
    size_t threadsMemorySize = divSize * THREADS_MEMORY_WEIGHT;
    size_t libraryMemorySize = libPlusThreadsSpace - threadsMemorySize;
    size_t numSwapPages = SWAP_SIZE / pageSize;
    size_t memPgPlusMemTblSpace = threadsMemorySize - (numSwapPages * PG_TBL_ROW_SIZE);
    size_t numMemPages = memPgPlusMemTblSpace / pageWithTableRowSize;
    size_t numPages = numSwapPages + numMemPages;
    size_t pageTableSize = numPages * PG_TBL_ROW_SIZE;
    while ((MEM_META_SIZE + libraryMemorySize + pageTableSize) % pageSize) {
        libraryMemorySize--;
        threadsMemorySize = libPlusThreadsSpace - libraryMemorySize;
        memPgPlusMemTblSpace = threadsMemorySize - (numSwapPages * PG_TBL_ROW_SIZE);
        numMemPages = memPgPlusMemTblSpace / pageWithTableRowSize;
        numPages = numSwapPages + numMemPages;
        pageTableSize = numPages * PG_TBL_ROW_SIZE;
    }

    // Setting memory's metadata based on calculated numbers
    LIB_MEM_PART = createPartition(MEM_INFO + 1, libraryMemorySize);
    SHRD_MEM_PART = createPartition(memory + MEM_SIZE - SHRD_MEM_SIZE, SHRD_MEM_SIZE);
    PG_TBL = PG_TBL_ROW_PTR(memory + MEM_META_SIZE + libraryMemorySize);
    NUM_MEM_PGS = numMemPages;
    NUM_SWAP_PGS = numSwapPages;
    SWAP_FILE = open("swapFile", O_CREAT|O_RDWR|O_TRUNC, S_IRUSR|S_IWUSR);
    if (SWAP_FILE == -1) {
        fprintf(stderr, "Error opening swapFile: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Setting signal handler to be fired as the
    // last function before the program exits
    atexit(cleanup);

    // Create 16MB swap file
    seekSwapFile(SWAP_SIZE - 1);
    if (write(SWAP_FILE, "\0", 1) == -1) {
        fprintf(stderr, "Error initializing swapFile to 16MB: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Initializing the page table
    off_t i;
    for (i = 0; i < numMemPages; i++) {
        PG_TBL[i].thread = NULL;
        PG_TBL[i].physicalLocation = MEM_PGS + (i * pageSize);
        PG_TBL[i].virtualLocation = -1;
    }
    off_t j;
    for (j = 0; j < numSwapPages; j += pageSize) {
        PG_TBL[i].thread = NULL;
        PG_TBL[i].physicalLocation = NULL;
        PG_TBL[i].virtualLocation = j;
        i++;
    }

    // Setting signal handler to be fired on bad page access
    protectPages(MEM_PGS, numMemPages);
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = onBadAccess;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        fprintf(stderr, "Error setting up signal handler for bad page access: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Returns 1 if thread can extend its pages else returns 0
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

// Allocates size bytes from partition. Returns a pointer
// to the allocated memory or NULL if there is no space.
void * allocateFrom(size_t size, struct memoryPartition * partition) {

    struct blockMetadata * head = partition->firstHead;
    
    // Searches for first free block with at least size bytes
    while (head->used || size > head->payloadSize) {
        head = BLK_META_PTR(CHAR_PTR(head) + BLK_SIZE(head->payloadSize));
        if ((head - 1) == partition->lastTail) { return NULL; }
    }

    // Doesn't split the found block if splitting would waste space
    if ((size + DBL_BLK_META_SIZE) >= head->payloadSize) {
        setBlockUsed(head, 1);
        return head + 1;
    }

    // Splits the found block
    size_t nextPayloadSize = head->payloadSize - (size + DBL_BLK_META_SIZE);
    setBlockMetadata(head, 1, size);
    setBlockMetadata(getTail(head) + 1, 0, nextPayloadSize);

    return head + 1;
}

// Allocates size bytes from the approprite partition and returns a pointer
// to the allocation. Returns NULL if no space or on bad request. Undefined
// behavior occurs if this function is called as a thread before creating a
// thread from the thread library.
void * myallocate(size_t size, char * fileName, int lineNumber, int request) {

    // Initialize the memory manager if not already
    if (!memory) {
        initializeMemory();
    }

    // Allocate from the thread library's partition
    if (request == LIBRARYREQ) {
        return allocateFrom(size, &LIB_MEM_PART);

    // Allocate from thread address space
    } else if (request == THREADREQ) {

        // Block scheduler for thread safety
        block = 1;

        // If needed and possible, extend the thread's partition.
        // Allocate from the thread's partition.
        void * ret = allocateFrom(size, &(THRD_MEM->partition));
        while (!ret && canExtend(currentTcb)) {
            extendPartition(&(THRD_MEM->partition), pageSize);
            ret = allocateFrom(size, &(THRD_MEM->partition));
        }

        // Unblock the scheduler and return
        block = 0;
        return ret;        

    } else { return NULL; }
}

// Allocates size bytes from memory as a thread.
// Returns NULL if no space of size is 0. Undefined
// behavior occurs if this function is called before
// creating a thread from the thread library.
void * threadAllocate(size_t size) {
    if (!size || !currentTcb) { return NULL; }
    return myallocate(size, __FILE__, __LINE__, THREADREQ);
}

// Returns a pointer of size bytes from shared
// memory. Returns NULL if no space or size is 0.
void * shalloc(size_t size) {

    // Initialize the memory manager if not already
    if (!memory) {
        initializeMemory();
    }

    if (!size) { return NULL; }

    block = 1;
    void * ret = allocateFrom(size, &SHRD_MEM_PART);
    block = 0;
    return ret;
}

// Deallocates ptr's block from partition. If ptr is not
// in partition return 0, else return 1. Undefined behavior
// if ptr isn't a pointer previously returned.
int deallocateFrom(void * ptr, struct memoryPartition * partition) {

    // Check if ptr is in partition
    if (ptr >= VOID_PTR(partition->firstHead + 1) && ptr < VOID_PTR(partition->lastTail)) {

        // Get head and tail from ptr
        struct blockMetadata * head = BLK_META_PTR(ptr) - 1;
        struct blockMetadata * tail = getTail(head);
        
        // Coallese with neihboring blocks
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

        // Free
        setBlockUsed(head, 0);
        return 1;

    } else { return 0; }
}

// Frees memory refrenced by ptr that was previously allocated with
// myallocate. Undifined behavior occurs if ptr was already freed or
// if ptr wasn't retrned by an allocating fucntion.
void mydeallocate(void * ptr, char * fileName, int lineNumber, int request) {

    // Deallocate from library partition
    if (request == LIBRARYREQ) { deallocateFrom(ptr, &LIB_MEM_PART); }

    // Deallocate from thread partition or shared partition in a thread-safe manor
    else if (request == THREADREQ) {
        block = 1;
        if (!deallocateFrom(ptr, &(THRD_MEM->partition))) {
            deallocateFrom(ptr, &SHRD_MEM_PART);
        }
        block = 0;
    }
}

// Frees ptr's block if ptr is in the shared partition
// or the thread's partition. Undifined behavior occurs
// if ptr was already freed or if ptr wasn't retrned by
// an allocating fucntion.
void threadDeallocate(void * ptr) {
    mydeallocate(ptr, __FILE__, __LINE__, THREADREQ);
}
