/*  Simple garbage collector for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2004-2008
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
 *  Version: $Id: amxgc.c 3902 2008-01-23 17:40:01Z thiadmer $
 */
#include <assert.h>
#include <limits.h>
#include <stdlib.h>     /* for malloc()/free() */
#include <string.h>     /* for memset() */
#include "amx.h"
#include "amxgc.h"

typedef struct tagGCPAIR {
  cell value;
  int count;
} GCPAIR;

typedef struct tagGCINFO {
  GCPAIR *table;
  GC_FREE callback;
  int exponent;
  int flags;
  int count;
} GCINFO;

#define SHIFT1          (sizeof(cell)*4)
#define MASK1           (~(((cell)-1) << SHIFT1))
#define FOLD1(p)        ( ((p) & MASK1) ^ (((p) >> SHIFT1) & MASK1) )
                        /* call FOLD1(c) if the table exponent < SHIFT1 */
#define SHIFT2          (sizeof(cell)*2)
#define MASK2           (~(((cell)-1) << SHIFT2))
#define FOLD2(p)        ( ((p) & MASK2) ^ (((p) >> SHIFT2) & MASK2) )
                        /* call FOLD2(c) if the table size < MASK2 */
#define SHIFT3          (sizeof(cell))
#define MASK3           (~(((cell)-1)<<SHIFT3))
#define FOLD3(p)        ( ((p) & MASK3) ^ (((p) >> SHIFT3) & MASK3) )
                        /* call FOLD3(c) if the table size < MASK3 */
#define MASK(exp)       (~(((cell)-1) << (exp)))

static unsigned increments[17] = { 1, 1, 1, 3, 5, 7, 17, 31, 67, 127, 257,
                                   509, 1021, 2053, 4099, 8191, 16381 };

static unsigned char inverse[256] = {
  255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,
  239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,
  223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,
  207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,
  191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,
  175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,
  159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,
  143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,
  127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112,
  111,110,109,108,107,106,105,104,103,102,101,100, 99, 98, 97, 96,
   95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80,
   79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64,
   63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
   47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
   31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
   15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0
};

static GCINFO SharedGC;


int gc_setcallback(GC_FREE callback)
{
  SharedGC.callback=callback;
  return GC_ERR_NONE;
}

int gc_settable(int exponent, int flags)
{
  if (exponent==0) {
    gc_clean();         /* delete all "live" objects first */
    if (SharedGC.table!=NULL) {
      free(SharedGC.table);
      SharedGC.table=NULL;
    } /* if */
    SharedGC.exponent=0;
    SharedGC.flags=0;
    SharedGC.count=0;
  } else {
    int size,oldsize;
    GCPAIR *table,*oldtable;

    if (exponent<7 || (1L<<exponent)>INT_MAX)
      return GC_ERR_PARAMS;
    size=(1<<exponent);
    /* the hash table should not hold more elements than the new size */
    if (SharedGC.count>size)
      return GC_ERR_PARAMS;
    /* allocate the new table */
    table=malloc(size*sizeof(*table));
    if (table==NULL)
      return GC_ERR_MEMORY;
    /* save the statistics of the old table */
    oldtable=SharedGC.table;
    oldsize=(1<<SharedGC.exponent);
    /* clear and set the new table */
    memset(table,0,size*sizeof(*table));
    SharedGC.table=table;
    SharedGC.exponent=exponent;
    SharedGC.count=flags;
    SharedGC.count=0;           /* old table in initially empty */
    /* re-mark all objects in the old table */
    if (oldtable!=NULL) {
      int index;
      for (index=0; index<oldsize; index++)
        if (oldtable[index].value!=0)
          gc_mark(oldtable[index].value);
      free(oldtable);
    } /* if */
  } /* if */
  return GC_ERR_NONE;
}

int gc_tablestat(int *exponent,int *percentage)
{
  if (exponent!=NULL)
    *exponent=SharedGC.exponent;
  if (percentage!=NULL) {
    int size=(1L<<SharedGC.exponent);
    /* calculate with floating point to avoid integer overflow */
    double p=100.0*SharedGC.count/size;
    *percentage=(int)p;
  } /* if */
  return GC_ERR_NONE;
}

int gc_mark(cell value)
{
  int index,incr,incridx,mask;
  cell v,t;
  unsigned char *minorbyte;

  if (SharedGC.table==NULL)
    return GC_ERR_INIT;
  if (SharedGC.count>=(1<<SharedGC.exponent)) {
    int err;
    if ((SharedGC.flags & GC_AUTOGROW)==0)
      return GC_ERR_TABLEFULL;
    err=gc_settable(SharedGC.exponent+1,SharedGC.flags);
    if (err!=GC_ERR_NONE)
      return err;
  } /* if */
  assert(SharedGC.count<(1<<SharedGC.exponent));

  /* first "fold" the value, to make maximum use of all bits */
  v=value;
  if (SharedGC.exponent<SHIFT1)
    v=FOLD1(v);
  if (SharedGC.exponent<SHIFT2)
    v=FOLD2(v);
  if (SharedGC.exponent<SHIFT3)
    v=FOLD3(v);
  /* swap the bits of the minor byte */
  minorbyte=(unsigned char*)&v;
  *minorbyte=inverse[*minorbyte];
  /* truncate the value to the required number of bits */
  mask=MASK(SharedGC.exponent);
  index=(v & mask);

  incridx= (SharedGC.exponent<sizeof increments / sizeof increments[0]) ?
              SharedGC.exponent :
              (sizeof increments / sizeof increments[0]) - 1;
  assert(incridx<sizeof increments / sizeof increments[0]);
  incr=increments[incridx];
  while ((t=SharedGC.table[index].value)!=0 && t!=value) {
    assert(incr>0);
    index=(index+incr) & mask;
    if (incridx>0)
      incr=increments[--incridx];
  } /* while */

  if (t!=0) {
    assert(t==value);
    assert(SharedGC.table[index].value==value);
    return GC_ERR_DUPLICATE;
  } /* if */

  SharedGC.table[index].value=value;
  assert(SharedGC.table[index].count==0);

  return GC_ERR_NONE;
}

static void scansection(cell *start,size_t size)
{
  int index,incr,incridx,incridx_org,mask;
  cell v,t;
  unsigned char *minorbyte;

  assert(SharedGC.table!=NULL);
  assert((size % sizeof(cell))==0);
  assert(start!=NULL);
  size/=sizeof(cell); /* from number of bytes to number of cells */

  incridx_org= (SharedGC.exponent<sizeof increments / sizeof increments[0]) ?
                  SharedGC.exponent :
                  (sizeof increments / sizeof increments[0]) - 1;
  assert(incridx_org<sizeof increments / sizeof increments[0]);

  minorbyte=(unsigned char*)&v;
  mask=MASK(SharedGC.exponent);

  while (size>0) {
    v=*start;
    /* first "fold" the value, to make maximum use of all bits */
    if (SharedGC.exponent<SHIFT1)
      v=FOLD1(v);
    if (SharedGC.exponent<SHIFT2)
      v=FOLD2(v);
    if (SharedGC.exponent<SHIFT3)
      v=FOLD3(v);
    /* swap the bits of the minor byte */
    assert(minorbyte==(unsigned char*)&v);
    *minorbyte=inverse[*minorbyte];
    /* truncate the value to the required number of bits */
    index=(v & mask);

    /* find it in the table */
    incridx=incridx_org;
    incr=increments[incridx];
    while ((t=SharedGC.table[index].value)!=*start && t!=0) {
      assert(incr>0);
      index=(index+incr) & mask;
      if (incridx>0)
        incr=increments[--incridx];
    } /* while */

    /* if found, mark it */
    if (t!=0) {
      assert(t==*start);
      assert(SharedGC.table[index].value==*start);
      SharedGC.table[index].count+=1;
    } /* if */

    size--;
    start++;
  } /* while */
}

int gc_scan(AMX *amx)
{
  AMX_HEADER *hdr;
  unsigned char *data;

  if (amx==NULL)
    return GC_ERR_PARAMS;
  if (SharedGC.table==NULL)
    return GC_ERR_INIT;

  hdr=(AMX_HEADER*)amx->base;

  /* scan data segment */
  data=amx->data ? amx->data : amx->base+(int)hdr->dat;
  scansection((cell *)data, hdr->hea - hdr->dat);
  /* scan stack */
  scansection((cell *)(data + amx->hlw), amx->hea - amx->hlw);
  /* scan heap */
  scansection((cell *)(data + amx->stk), amx->stp - amx->stk);

  return GC_ERR_NONE;
}

int gc_clean(void)
{
  int size;
  GCPAIR *item;

  if (SharedGC.table==NULL)
    return GC_ERR_INIT;
  if (SharedGC.callback==NULL)
    return GC_ERR_CALLBACK;

  size=(1<<SharedGC.exponent);
  item=SharedGC.table;
  while (size>0) {
    if (item->value!=0) {
      if (item->count==0) {
        SharedGC.callback(item->value);
        item->value=0;
      } /* if */
      item->count=0;
    } /* if */
    size--;
    item++;
  } /* while */
  return GC_ERR_NONE;
}
