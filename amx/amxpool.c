/*  Simple allocation from a memory pool, with automatic release of
 *  least-recently used blocks (LRU blocks).
 *
 *  These routines are as simple as possible, and they are neither re-entrant
 *  nor thread-safe. Their purpose is to have a standard implementation for
 *  systems where overlays are used and malloc() is not available.
 *
 *  The algorithm uses a first-fit strategy. It keeps all blocks in a single
 *  list (both used blocks and free blocks are in the same list). Every memory
 *  block must have a unique number that identifies the block. This unique
 *  number allows to search for the presence of the block in the pool and for
 *  "conditional allocation".
 *
 *
 *  Copyright (c) ITB CompuPhase, 2007
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id$
 */
#include <assert.h>
#include "amx.h"
#include "amxpool.h"

#if !defined NULL
  #define NULL  ((void*)0)
#endif

#define MIN_BLOCKSIZE 32

typedef struct tagARENA {
  unsigned blocksize;
  short index;      /* overlay index, -1 if free */
  unsigned short lru;
} ARENA;

static void *pool_base;
static unsigned pool_size;
static unsigned short pool_lru;

/* amx_poolinit() initializes the memory pool for the allocated blocks. */
void amx_poolinit(void *pool, unsigned size)
{
  assert(pool!=NULL);
  assert(size>sizeof(ARENA));

  /* save parameters in global variables, then "free" the entire pool */
  pool_base=pool;
  pool_size=size;
  pool_lru=0;
  amx_poolfree(NULL);
}

/* amx_poolfree() releases a block allocated earlier. The parameter must have
 * the same value as that returned by an earlier call to amx_poolalloc(). That
 * is, the "block" parameter must point directly behind the arena header of the
 * block.
 * When parameter "block" is NULL, the pool is re-initialized (meaning that
 * all blocks are freed).
 */
void amx_poolfree(void *block)
{
  ARENA *hdr,*hdr2;
  unsigned sz;

  assert(pool_base!=NULL);
  assert(pool_size>sizeof(ARENA));

  /* special case: if "block" is NULL, create a single free space */
  if (block==NULL) {
    /* store an arena header at the start of the pool */
    hdr=(ARENA*)pool_base;
    hdr->blocksize=pool_size-sizeof(ARENA);
    hdr->index=-1;
    hdr->lru=0;
  } else {
    hdr=(ARENA*)((char*)block-sizeof(ARENA));
    assert((char*)hdr>=(char*)pool_base && (char*)hdr<(char*)pool_base+pool_size);
    assert(hdr->blocksize<pool_size);

    /* free this block */
    hdr->index=-1;

    /* try to coalesce with the next block */
    hdr2=(ARENA*)((char*)hdr+hdr->blocksize+sizeof(ARENA));
    if (hdr2->index==-1)
      hdr->blocksize+=hdr2->blocksize+sizeof(ARENA);

    /* try to coalesce with the previous block */
    if ((void*)hdr!=pool_base) {
      sz=pool_size;
      hdr2=(ARENA*)pool_base;
      while (sz>0 && (char*)hdr2+hdr2->blocksize+sizeof(ARENA)!=(char*)hdr) {
        assert(sz<=pool_size);
        sz-=hdr2->blocksize+sizeof(ARENA);
        hdr2=(ARENA*)((char*)hdr2+hdr2->blocksize+sizeof(ARENA));
      } /* while */
      assert((char*)hdr2+hdr2->blocksize+sizeof(ARENA)==(char*)hdr);
      if (hdr2->index==-1)
        hdr2->blocksize+=hdr->blocksize+sizeof(ARENA);
    } /* if */
  } /* if */
}

/* amx_poolfind() returns the address of the memory block with the given index,
 * or NULL if no such block exists. Parameter "index" should not be -1, because
 * -1 represents a free block (actually, only positive values are valid).
 */
void *amx_poolfind(int index)
{
  ARENA *hdr;
  unsigned sz;

  assert(index>=0);
  sz=pool_size;
  hdr=(ARENA*)pool_base;
  while (sz>0 && hdr->index!=index) {
    assert(sz<=pool_size);
    assert((char*)hdr>=(char*)pool_base && (char*)hdr<(char*)pool_base+pool_size);
    sz-=hdr->blocksize+sizeof(ARENA);
    hdr=(ARENA*)((char*)hdr+hdr->blocksize+sizeof(ARENA));
  } /* while */
  assert(sz<=pool_size);
  return (sz==0) ? NULL : (void*)((char*)hdr+sizeof(ARENA));
}

/* amx_poolalloc() allocates the requested number of bytes from the pool and
 * returns a header to the start of it. Every block in the pool is prefixed
 * with an "arena header"; the return value of this function points just
 * behind this arena header.
 *
 * The block with the specified "index" should not already exist in the pool.
 * In other words, parameter "index" should be unique for every of memory block,
 * and the block should not change in size. Use amx_poolfind() to verify whether
 * a block is already in the pool (and optionally amx_poolfree() to remove it).
 *
 * If no block of sufficient size is available, the routine frees blocks until
 * the requested amount of memory can be allocated. There is no intelligent
 * algorithm involved: the routine just frees the least-recently used block at
 * every iteration (without considering the size of the block or whether that
 * block is adjacent to a free block).
 */
void *amx_poolalloc(unsigned size,int index)
{
  ARENA *hdr,*hdrlru;
  unsigned sz;
  unsigned short minlru;

  assert(size>0);
  assert(index>=0 && index<=SHRT_MAX);
  assert(amx_poolfind(index)==NULL);

  /* align the size to a cell boundary */
  if ((size % sizeof(cell))!=0)
    size+=sizeof(cell)-(size % sizeof(cell));
  if (size+sizeof(ARENA)>pool_size)
    return NULL;  /* requested block does not fit in the pool */

  /* find a block large enough to get the size plus an arena header; at
   * the same time, detect the block with the lowest LRU
   * if no block of sufficient size can be found, the routine then frees
   * the block with the lowest LRU count and tries again
   */
  do {
    sz=pool_size;
    hdr=(ARENA*)pool_base;
    hdrlru=hdr;
    minlru=USHRT_MAX;
    while (sz>0) {
      assert(sz<=pool_size);
      assert((char*)hdr>=(char*)pool_base && (char*)hdr<(char*)pool_base+pool_size);
      if (hdr->index==-1 && hdr->blocksize>=size)
        break;
      if (hdr->index!=-1 && hdr->lru<minlru) {
        minlru=hdr->lru;
        hdrlru=hdr;
      } /* if */
      sz-=hdr->blocksize+sizeof(ARENA);
      hdr=(ARENA*)((char*)hdr+hdr->blocksize+sizeof(ARENA));
    } /* while */
    assert(sz<=pool_size);
    if (sz==0) {
      /* free up memory and try again */
      assert(hdrlru->index!=-1);
      amx_poolfree((char*)hdrlru+sizeof(ARENA));
    } /* if */
  } while (sz==0);

  /* see whether to allocate the entire free block, or to cut it in two blocks */
  if (hdr->blocksize>size+MIN_BLOCKSIZE+sizeof(ARENA)) {
    /* cut the block in two */
    ARENA *next=(ARENA*)((char*)hdr+size+sizeof(ARENA));
    next->blocksize=hdr->blocksize-size-sizeof(ARENA);
    next->index=-1;
    next->lru=0;
  } else {
    size=hdr->blocksize;
  } /* if */
  hdr->blocksize=size;
  hdr->index=(short)index;
  hdr->lru=++pool_lru;

  /* special case: if the overlay LRU count wrapped back to zero, set the
   * LRU count of all blocks to zero, but set the count of the block just
   * allocated to 1
   */
  if (pool_lru==0) {
    ARENA *hdr2;
    sz=pool_size;
    hdr2=(ARENA*)pool_base;
    while (sz>0) {
      assert(sz<=pool_size);
      hdr2->lru=0;
      sz-=hdr2->blocksize+sizeof(ARENA);
      hdr2=(ARENA*)((char*)hdr2+hdr2->blocksize+sizeof(ARENA));
    } /* while */
    assert(sz==0);
    hdr->lru=++pool_lru;
  } /* if */

  return (void*)((char*)hdr+sizeof(ARENA));
}
