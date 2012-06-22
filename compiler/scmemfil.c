/*  Pawn compiler
 *
 *  Routines to maintain a "text file" in memory, based on memory interface
 *  functions by faluco of the AMX Mod X team.
 *
 *  Copyright (c) ITB CompuPhase, 2003-2012
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  Version: $Id: scmemfil.c 4733 2012-06-22 08:39:46Z thiadmer $
 */

#include <assert.h>
#include <string.h>
#if defined FORTIFY
  #include <alloc/fortify.h>
#endif
#include "sc.h"


SC_FUNC memfile_t *mfcreate(const char *filename)
{
  return memfile_creat(filename, 4096);
}

SC_FUNC void mfclose(memfile_t *mf)
{
  memfile_destroy(mf);
}

SC_FUNC int mfdump(memfile_t *mf)
{
  FILE *fp;
  int okay;

  assert(mf!=NULL);
  /* create the file */
  fp=fopen(mf->name, "wb");
  if (fp==NULL)
    return 0;

  okay = (fwrite(mf->base, mf->usedoffs, 1, fp)==(size_t)mf->usedoffs);

  fclose(fp);
  return okay;
}

SC_FUNC size_t mflength(const memfile_t *mf)
{
  return mf->usedoffs;
}

SC_FUNC size_t mfseek(memfile_t *mf,long offset,int whence)
{
  long length;

  assert(mf!=NULL);
  if (mf->usedoffs == 0)
    return 0L;          /* early exit: not a single byte in the file */

  /* find the size of the memory file */
  length=(long)mflength(mf);

  /* convert the offset to an absolute position */
  switch (whence) {
  case SEEK_SET:
    break;
  case SEEK_CUR:
    offset+=(long)mf->offs;
    break;
  case SEEK_END:
    assert(offset<=0);
    offset+=(long)length;
    break;
  } /* switch */

  /* clamp to the file length limit */
  if (offset<0)
    offset=0;
  else if (offset>length)
    offset=length;

  /* set new position and return it */
  memfile_seek(mf, (size_t)offset);
  return offset;
}

SC_FUNC size_t mfwrite(memfile_t *mf,const unsigned char *buffer,size_t size)
{
  return (memfile_write(mf, buffer, size) ? size : 0);
}

SC_FUNC size_t mfread(memfile_t *mf,unsigned char *buffer,size_t size)
{
  return memfile_read(mf, buffer, size);
}

SC_FUNC char *mfgets(memfile_t *mf,char *string,size_t size)
{
  char *ptr;
  size_t read;
  long seek;

  assert(mf!=NULL);

  read=mfread(mf,(unsigned char *)string,size);
  if (read==0)
    return NULL;
  seek=0L;

  /* make sure that the string is zero-terminated */
  assert(read<=size);
  if (read<size) {
    string[read]='\0';
  } else {
    string[size-1]='\0';
    seek=-1;            /* undo reading the character that gets overwritten */
  } /* if */

  /* find the first '\n' */
  ptr=strchr(string,'\n');
  if (ptr!=NULL) {
    *(ptr+1)='\0';
    seek=(long)(ptr-string)+1-(long)read;
  } /* if */

  /* undo over-read */
  assert(seek<=0);      /* should seek backward only */
  if (seek!=0)
    mfseek(mf,seek,SEEK_CUR);

  return string;
}

SC_FUNC int mfputs(memfile_t *mf,const char *string)
{
  size_t written,length;

  assert(mf!=NULL);

  length=strlen(string);
  written=mfwrite(mf,(unsigned char *)string,length);
  return written==length;
}
