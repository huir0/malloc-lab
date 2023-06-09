/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "8",
    /* First member's full name */
    "h.kim",
    /* First member's email address */
    "github.com/huir0",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* added macros*/

#define WSIZE 8                             // word and header/footer size (bytes)
#define DSIZE 16                            // double word size (bytes)
#define CHUNKSIZE (1<<12)                   // extend heap by this amount (bytes)

#define MAX(x, y) ((x) > (y)? (x) : (y))

// pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))    

// read and write a word at address p
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

// read the size and allocated fields from address
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// given block ptr bp, compute address of its header and footer
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define NEXT_FIT

#ifdef NEXT_FIT
static char *pointp; // pointer to search block
#endif

static char *heap_listp; /* always points prologue block*/


static void *coalesce(void *bp) {
    // check if prev and next blocks are allocated or free
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));       // get size of current block

    if (prev_alloc && next_alloc) {         // case 1 : prev and next allocated
        return bp;
    }

    else if (prev_alloc && !next_alloc) {   // case 2 : prev allocated, next free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // increase size by size of next block
        PUT(HDRP(bp), PACK(size, 0));       // update header
        PUT(FTRP(bp), PACK(size, 0));       // update footer
    }

    else if (!prev_alloc && next_alloc) {   // case 3 : prev free, next allocated
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  // increase size by size of prev block
        PUT(FTRP(bp), PACK(size, 0));       // update footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // update header of prev block
        bp = PREV_BLKP(bp);                 // move bp to prev block
    }

    else {                                  // case 4 : next and prev free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // update header of prev block
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    // update footer of next block
        bp = PREV_BLKP(bp);                 // move bp to prev block
    }
    #ifdef NEXT_FIT
        pointp = bp;
    #endif
    return bp;  // return pointer to coalesced block
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           // free block header
    PUT(FTRP(bp), PACK(size, 0));           // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // new epilogue header

#ifdef NEXT_FIT                             // pointer to next block for next fit 
    pointp = NEXT_BLKP(bp);
#endif

    return coalesce(bp);
}



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             // alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // epilogue header
    heap_listp += (2*WSIZE);

#ifdef NEXT_FIT
    pointp = heap_listp;
#endif

    // extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


static void *find_fit(size_t asize) {
    void *bp;

#ifdef NEXT_FIT
    /* next fit */
    void *old_pointp = pointp; // save the original value of pointp
    // search from pointp to end of heap
    for (bp = pointp; GET_SIZE(HDRP(bp)); bp = NEXT_BLKP(bp)) {
        // if block is free and large enough
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            pointp = NEXT_BLKP(bp); // update pointp
            return bp;              // return pointer to found block
        }
    }
    // search from beginning of heap to old_pointp
    for (bp = heap_listp; bp < old_pointp; bp = NEXT_BLKP(bp)) {
        // if block is free and large enough
        if ((!GET_ALLOC(HDRP(bp))) && GET_SIZE(HDRP(bp)) >= asize) {
        pointp = NEXT_BLKP(bp);     // update pointp
        return bp;                  // return pointer to found block
        }
    }
    return NULL;                    // no suitable block found
#else
    /* first fit */
    // search from beginning of heap to end
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // if block is free and large enough
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;              // return pointer to found block
        }
    }
    return NULL;
#endif

    /* best fit */ 
    // void *best = NULL;
    // for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = GET_NEXT(bp)) {
    //     if (asize <= GET_SIZE(HDRP(bp))) {
    //         if (best == NULL) best = bp;
    //         esle if (GET_SIZE(HDRP(bp)) <= GET_SIZE(HDRP(best))) best = bp;
    //     }
    // }
    // if (best != NULL) return best;
    // return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));      // get current size of block

    // if block can be split
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize ,1));      // set header of first block
        PUT(FTRP(bp), PACK(asize, 1));      // set footer of first block
        bp = NEXT_BLKP(bp);                 // move to second block

#ifdef NEXT_FIT
        pointp = bp;  // update pointp
#endif

        PUT(HDRP(bp), PACK(csize-asize, 0));    // set header of second block
        PUT(FTRP(bp), PACK(csize-asize, 0));    // set footer of second block
    }
    else {                                  // use entire block
        PUT(HDRP(bp), PACK(csize, 1));      // set header
        PUT(FTRP(bp), PACK(csize, 1));      // set footer

#ifdef NEXT_FIT
    pointp = NEXT_BLKP(bp);  // update pointp
#endif

    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;           // adjusted block size
    size_t extendsize;      // amount to extend heap if no fit
    char * bp;

    // ignore spurious requests
    if (size == 0) return NULL;

    // adjust block size to include overhead and alignment requirements
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // search the free list for a fit
    if ((bp = find_fit(asize)) != NULL)  {
        place(bp, asize);
        return bp;
    }

    // no fit found. get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));  // get size of block

    PUT(HDRP(ptr), PACK(size, 0));      // set header to indicate block is free
    PUT(FTRP(ptr), PACK(size, 0));      // set footer to indicate block is free
    coalesce(ptr);                      // merge with adjacent free blocks
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;         // save pointer to old blocks
    void *newptr;               // pointer to new block
    size_t copySize;            // size of data to be copied
    
    newptr = mm_malloc(size);   // allocate new block
    if (newptr == NULL)         // if allocation failed return NULL
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);   // get size of old block
    if (size < copySize)        // if new size is smaller than old size
      copySize = size;          // only copy new size bytes
    memcpy(newptr, oldptr, copySize);   // copy data from old block to new block
    mm_free(oldptr);            // free old block
    return newptr;              // return pointer to new block
}














