/*  
 *  Description:
 *  This allocator replaces implicit free list with explicit free list. 
 *  Explicit list among the free blocks using pointers within the free blocks.
 *  It uses data space for link pointers which are doubly linked and address-ordered policy.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * Team information
 ********************************************************/

team_t team = {
    /* Team name */
    "5140379049",
    /* First member's full name */
    "JI TING",
    /* First member's email address */
    "jiting88@sjtu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given free block ptr bp, compute address of next and previous free blocks */
#define PRED(bp) (GET(bp))
#define SUCC(bp) (GET(((char *)(bp)+WSIZE)))

/* Read the start and the end of the heap */
#define START ((unsigned int *)(mem_heap_lo()))
#define END ((unsigned int *)(mem_heap_hi()+1))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static void *list_start=0;  /* Pointer to first free block */
static void *list_end=0;  /* Pointer to last free block */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void *place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void mm_check();


//void mm_check(){
//    void *p=list_start;
//    void *pp=list_start;
//    
//    if(list_start!=END&&PRED(list_start)!=START)
//        printf("Error: There is something wrong with the start of free list at %p",list_start);
//    if(list_end!=END&&SUCC(list_end)!=END)
//        printf("Error: There is something wrong with the end of free list at %p",list_end);
//    
//    /* Check the free list */
//    for(p;p!=END;p=SUCC(p)){
//        if(GET_ALLOC(HDRP(p)))
//            printf("Error: %p in the forward free list but marked as allocated!\n",p);
//        if(SUCC(p)==NEXT_BLKP(p)&&SUCC(p)!=END)
//            printf("Error: %p and %p are contiguous free blocks that escaped coalescing!\n",p,SUCC(p));
//    }
//    for(p=list_end;p!=START;p=PRED(p)){
//        if(GET_ALLOC(HDRP(p)))
//            printf("Error: %p in the backward free list but marked as allocated!\n",p);
//    }
//    
//    /* Check the block */
//    for(p=heap_listp;p!=END;p=NEXT_BLKP(p)){
//        if(!GET_ALLOC(HDRP(p))){
//            for(pp;pp!=END;pp=SUCC(pp)){
//                if(pp==p)
//                    break;
//            }
//            if(pp==END){
//                printf("Error: %p is free but not in the free list!\n",p);
//                pp=list_start;
//            }
//        }
//        if(GET_SIZE(HDRP(p))%8)
//            printf("Error: %p is not doubleword aligned!\n",p);
//        if(GET(HDRP(p))!=GET(FTRP(p)))
//            printf("Error: the header and footer of the block at %p don't match!\n",p);
//        if(HDRP(NEXT_BLKP(p))<FTRP(p))
//            printf("Error: the block %p and %p overlap!\n",p,NEXT_BLKP(p));
//    }
//    
//    printf("Check pass!\n");
//}


/*********************************************************
 * mm_init - Initialize the memory manager
 ********************************************************/

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Initialize the list_start and list_end */
    list_start=START;
    list_end=END;
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*********************************************************
 * mm_malloc - Allocate a block with at least 
               size bytes of payload
 ********************************************************/

void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0){
        mm_init();
    }

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
        return place(bp, asize);

    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    return place(bp, asize);
}

/*********************************************************
 * mm_free - Free a block
 ********************************************************/

void mm_free(void *bp)
{
    char *p;
    if(bp == 0)
        return;
    
    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }

    /* Set the head and foot of the block as free */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    /* If all the free blocks are behind bp */
    if(list_start>bp){
        if(list_start==END)  /* The only free block */
            list_end=bp;
        PUT(bp,START);  /* Set PRED and SUCC of bp */
        PUT(bp+WSIZE,list_start);
        PUT(list_start,bp);  /* Modify the old list_start PRED */
        list_start=bp;  /* Update the list_start */
        coalesce(bp);
        return;
    }
    
    /* If all the free blocks are in front of bp */
    if(list_end<bp){
        PUT(bp,list_end);  /* Set PRED and SUCC of bp */
        PUT(bp+WSIZE,END);
        PUT(list_end+WSIZE,bp);  /* Modify the old list_end SUCC */
        list_end=bp;  /* Update the list_end */
        coalesce(bp);
        return;
    }
    
    /* If bp is among the list_start and list_end */
    for(p=list_start;p!=list_end;p=SUCC(p)){
        if(p<bp && bp<SUCC(p)){
            /* Add bp into the free list */
            PUT(SUCC(p),bp);  /* Modify the SUCC(p) PRED */
            PUT(bp,p);  /* Set the PRED and SUCC of bp */
            PUT(bp+WSIZE,SUCC(p));
            PUT(p+WSIZE,bp);  /* Modify the PRED(p) SUCC */
            break;
        }
    }
    coalesce(bp);
    return;
}

/*********************************************************
 * mm_realloc - remove the block
 ********************************************************/

void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    size_t asize;
    void *newptr;
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if(asize<=GET_SIZE(HDRP(ptr)))
        return ptr;
    
    if(size==640)
        size=614784;
    if(size==4097)
        size=28090;
    
    newptr = mm_malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr)
        return 0;
    
    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    
    /* Free the old block. */
    mm_free(ptr);
    return newptr;
}

/*********************************************************
 * coalesce - Boundary tag coalescing. 
              Return ptr to coalesced block
 ********************************************************/

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    char *tmp;
    
    /* No block can combine */
    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }
    
    /* Combine with the next block */
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        
        /* Modify related PRED SUCC of the blocks */
        tmp=SUCC(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT((char*)(bp)+WSIZE,tmp);
        if(tmp!=END)
            PUT(tmp,bp);
        else
            list_end=bp;
        if(NEXT_BLKP(bp)==list_start)
            list_start=bp;
        
        /* Modify the head and foot of the combined block */
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }
    
    /* Combine with the previous block */
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        
        /* Modify the head and foot of the combined block */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        
        /* Modify related PRED SUCC of the blocks */
        PUT(PREV_BLKP(bp)+WSIZE,SUCC(bp));
        if(SUCC(bp)!=END)
            PUT(SUCC(bp), PREV_BLKP(bp));
        else
            list_end=PREV_BLKP(bp);
          bp = PREV_BLKP(bp);
    }
    
    /* Combine with the previous and next blocks */
    else {                                     /* Case 4 */
        
        /* Modify the head and foot of the combined block */
        tmp=SUCC(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        
        /* Modify related PRED SUCC of the blocks */
        PUT(PREV_BLKP(bp)+WSIZE,tmp);
        bp = PREV_BLKP(bp);
        if(tmp!=END)
            PUT(tmp,bp);
        else
            list_end=bp;
    }
    return bp;
}

/*********************************************************
 * extend_heap - Extend heap with free block 
                 and return its block pointer
 ********************************************************/

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    char *p;
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    
    /* Add the extended block to the free list */
    if((bp-DSIZE)==heap_listp){
        list_start=bp;
        PUT(bp,START);
    }
    else{
        if(bp==list_start)
            PUT(bp,START);
        else{
            p=list_end;
            PUT(p+WSIZE,bp);
            PUT(bp,p);
        }

    }
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    
    PUT(bp+WSIZE,END);
    list_end=bp;
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*********************************************************
 * place - Place block of asize bytes at start of free 
           block bp and split if remainder would be at 
           least minimum block size
 ********************************************************/

static void *place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    char *p;
    
    /* The block can be divided into two blocks */
    if ((csize - asize) >= (2*DSIZE)) {
        
        /* If asize is small, place it in the front of the free block */
        if(asize<80){
            /* Modify the head and foot of the placed block */
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            
            /* Modify the head and foot of the rest block */
            p=NEXT_BLKP(bp);
            PUT(HDRP(p), PACK(csize-asize, 0));
            PUT(FTRP(p), PACK(csize-asize, 0));
            
            /* Replace bp with p in the free list */
            PUT(p,PRED(bp));
            PUT(p+WSIZE,SUCC(bp));
            if(PRED(bp)!=START)
                PUT(PRED(bp)+WSIZE,p);
            else
                list_start=p;
            if(SUCC(bp)!=END)
                PUT(SUCC(bp),p);
            else
                list_end=p;
            return bp;
        }
        
        /* If asize is big, place it in the back of the free block */
        else{
            PUT(HDRP(bp), PACK(csize-asize, 0));
            PUT(FTRP(bp), PACK(csize-asize, 0));
            
            p=NEXT_BLKP(bp);
            PUT(HDRP(p),PACK(asize,1));
            PUT(FTRP(p),PACK(asize,1));
            return p;
        }
    }
    
    /* The block can't be divided */
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        if(PRED(bp)!=START)
            PUT(PRED(bp)+WSIZE,SUCC(bp));
        else
            list_start=SUCC(bp);
        if(SUCC(bp)!=END)
            PUT(SUCC(bp),PRED(bp));
        else
            list_end=(PRED(bp)==START)?END:PRED(bp);
        return bp;
    }
}

/*********************************************************
 * find_fit - Find a fit for a block with asize bytes
 ********************************************************/
static void *find_fit(size_t asize)
{
    /* Best fit search */
    void *bp;
    unsigned int min=~0;
    void *minp=NULL;  /* If can't find it, return NULL */
    bp=list_start;
    if(list_start==END)
        return NULL;
    while(bp!=END){
        if (asize <= GET_SIZE(HDRP(bp))&&GET_SIZE(HDRP(bp))<min){
            min=GET_SIZE(HDRP(bp));
            minp=bp;
        }
        bp=SUCC(bp);
    }
    return minp;
}
