/*  Implementation of a file functions interface for reading/writing into
 *  memory.
 *
 *  Copyright (c) faluco / http://www.amxmodx.org/, 2006
 *  Version: $Id: memfile.h 4611 2011-12-05 17:46:53Z thiadmer $
 */

#ifndef _INCLUDE_MEMFILE_H
#define _INCLUDE_MEMFILE_H

typedef struct memfile_s
{
	char *name;
	char *base;
	size_t offs;
	size_t usedoffs;
	size_t size;
} memfile_t;

memfile_t *memfile_creat(const char *name, size_t init);
void memfile_destroy(memfile_t *mf);
void memfile_seek(memfile_t *mf, long seek);
int memfile_write(memfile_t *mf, const void *buffer, size_t size);
size_t memfile_read(memfile_t *mf, void *buffer, size_t maxsize);
size_t memfile_tell(const memfile_t *mf);

#endif //_INCLUDE_MEMFILE_H
