/*  Implementation of a file functions interface for reading/writing into
 *  memory.
 *
 *  Copyright (c) faluco / http://www.amxmodx.org/, 2006
 *  Version: $Id: memfile.c 6932 2023-04-03 13:56:19Z thiadmer $
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef MACOS
  #include <malloc/malloc.h>
#else
  #include <stdlib.h>
#endif
#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

#include "sc.h"

memfile_t *memfile_creat(const char *name, size_t init)
{
	memfile_t *pmf;

	pmf = (memfile_t *)malloc(sizeof(memfile_t));
	if (!pmf)
		return NULL;

	pmf->size = init;
	pmf->base = (char *)malloc(init);
	if (!pmf->base)
	{
		free(pmf);
		return NULL;
	}
	pmf->usedoffs = 0;
	pmf->offs = 0;
	pmf->name = duplicatestring(name);

	return pmf;
}

void memfile_destroy(memfile_t *mf)
{
	assert(mf != NULL);
	free(mf->name);
	free(mf->base);
	free(mf);
}

void memfile_seek(memfile_t *mf, long seek)
{
	assert(mf != NULL);
	mf->offs = seek;
}

size_t memfile_tell(const memfile_t *mf)
{
	assert(mf != NULL);
	return mf->offs;
}

size_t memfile_read(memfile_t *mf, void *buffer, size_t maxsize)
{
	assert(mf != NULL);
	assert(buffer != NULL);
	if (!maxsize || mf->offs >= mf->usedoffs)
		return 0;

	if (mf->usedoffs - mf->offs < maxsize)
	{
		maxsize = mf->usedoffs - mf->offs;
		assert(maxsize > 0);
	}

	memcpy(buffer, mf->base + mf->offs, maxsize);

	mf->offs += maxsize;

	return maxsize;
}

int memfile_write(memfile_t *mf, const void *buffer, size_t size)
{
	assert(mf != NULL);
	assert(buffer != NULL);
	if (mf->offs + size > mf->size)
	{
	  char *orgbase = mf->base; /* save, in case realloc() fails */
		size_t newsize = (mf->size + size) * 2;
		mf->base = (char *)realloc(mf->base, newsize);
		if (!mf->base)
		{
		  mf->base = orgbase;     /* restore old pointer to avoid a memory leak */
			return 0;
		}
		mf->size = newsize;
	}
	memcpy(mf->base + mf->offs, buffer, size);
	mf->offs += size;

	if (mf->offs > mf->usedoffs)
	{
		mf->usedoffs = mf->offs;
	}

	return 1;
}
