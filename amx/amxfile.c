/* Text file I/O module for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2003-2007
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
 *  Version: $Id: amxfile.c 3764 2007-05-22 10:29:16Z thiadmer $
 */
#if defined _UNICODE || defined __UNICODE__ || defined UNICODE
# if !defined UNICODE   /* for Windows */
#   define UNICODE
# endif
# if !defined _UNICODE  /* for C library */
#   define _UNICODE
# endif
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined __MSDOS__
  #include <io.h>
  #include <malloc.h>
#endif
#if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows
  #include <windows.h>
#endif
#if defined LINUX || defined __FreeBSD__ || defined __OpenBSD__ || defined MACOS
  #include <dirent.h>
#endif
#include "osdefs.h"
#include "amx.h"

#include "fpattern.c"

#if !defined AMXFILE_VAR
  #define AMXFILE_VAR   "AMXFILE"
#elif AMXFILE_VAR==""
  #undef AMXFILE_VAR
#endif

#if !defined sizearray
  #define sizearray(a)  (sizeof(a)/sizeof((a)[0]))
#endif

#if defined _UNICODE
# include <tchar.h>
#elif !defined __T
  typedef char          TCHAR;
# define __T(string)    string
# define _tcscat        strcat
# define _tcschr        strchr
# define _tcscpy        strcpy
# define _tcsdup        strdup
# define _tcslen        strlen
# define _tcsncpy       strncpy
# define _tcspbrk       strpbrk
# define _tcsrchr       strrchr
# define _tfopen        fopen
# define _tfputs        fputs
# define _tgetenv       getenv
# define _tremove       remove
# define _trename       rename
# define _tstat         _stat
#endif

#if !defined UNUSED_PARAM
  #define UNUSED_PARAM(p) ((void)(p))
#endif

enum filemode {
  io_read,      /* file must exist */
  io_write,     /* creates a new file */
  io_readwrite, /* file must exist */
  io_append,    /* file must exist, opened for writing only and seek to the end */
};

enum seek_whence {
  seek_start,
  seek_current,
  seek_end,
};


/* This function only stores unpacked strings. UTF-8 is used for
 * Unicode, and packed strings can only store 7-bit and 8-bit
 * character sets (ASCII, Latin-1).
 */
static size_t fgets_cell(FILE *fp,cell *string,size_t max,int utf8mode)
{
  size_t index;
  fpos_t pos;
  cell c;
  int follow,lastcr;
  cell lowmark;

  assert(sizeof(cell)>=4);
  assert(fp!=NULL);
  assert(string!=NULL);
  if (max==0)
    return 0;

  /* get the position, in case we have to back up */
  fgetpos(fp, &pos);

  index=0;
  follow=0;
  lowmark=0;
  lastcr=0;
  for ( ;; ) {
    assert(index<max);
    if (index==max-1)
      break;                    /* string fully filled */
    if ((c=fgetc(fp))==EOF) {
      if (!utf8mode || follow==0)
        break;                  /* no more characters */
      /* If an EOF happened halfway an UTF-8 code, the string cannot be
       * UTF-8 mode, and we must restart.
       */
      index=0;
      fsetpos(fp, &pos);
      continue;
    } /* if */

    /* 8-bit characters are unsigned */
    if (c<0)
      c=-c;

    if (utf8mode) {
      if (follow>0 && (c & 0xc0)==0x80) {
        /* leader code is active, combine with earlier code */
        string[index]=(string[index] << 6) | ((unsigned char)c & 0x3f);
        if (--follow==0) {
          /* encoding a character in more bytes than is strictly needed,
           * is not really valid UTF-8; we are strict here to increase
           * the chance of heuristic dectection of non-UTF-8 text
           * (JAVA writes zero bytes as a 2-byte code UTF-8, which is invalid)
           */
          if (string[index]<lowmark)
            utf8mode=0;
          /* the code positions 0xd800--0xdfff and 0xfffe & 0xffff do not
           * exist in UCS-4 (and hence, they do not exist in Unicode)
           */
          if (string[index]>=0xd800 && string[index]<=0xdfff
              || string[index]==0xfffe || string[index]==0xffff)
            utf8mode=0;
          index++;
        } /* if */
      } else if (follow==0 && (c & 0x80)==0x80) {
        /* UTF-8 leader code */
        if ((c & 0xe0)==0xc0) {
          /* 110xxxxx 10xxxxxx */
          follow=1;
          lowmark=0x80;
          string[index]=c & 0x1f;
        } else if ((c & 0xf0)==0xe0) {
          /* 1110xxxx 10xxxxxx 10xxxxxx (16 bits, BMP plane) */
          follow=2;
          lowmark=0x800;
          string[index]=c & 0x0f;
        } else if ((c & 0xf8)==0xf0) {
          /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
          follow=3;
          lowmark=0x10000;
          string[index]=c & 0x07;
        } else if ((c & 0xfc)==0xf8) {
          /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
          follow=4;
          lowmark=0x200000;
          string[index]=c & 0x03;
        } else if ((c & 0xfe)==0xfc) {
          /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (31 bits) */
          follow=5;
          lowmark=0x4000000;
          string[index]=c & 0x01;
        } else {
          /* this is invalid UTF-8 */
          utf8mode=0;
        } /* if */
      } else if (follow==0 && (c & 0x80)==0x00) {
        /* 0xxxxxxx (US-ASCII) */
        string[index++]=c;
        if (c==__T('\n'))
          break;        /* read newline, done */
      } else {
        /* this is invalid UTF-8 */
        utf8mode=0;
      } /* if */
      if (!utf8mode) {
        /* UTF-8 mode was switched just off, which means that non-conforming
         * UTF-8 codes were found, which means in turn that the string is
         * probably not intended as UTF-8; start over again
         */
        index=0;
        fsetpos(fp, &pos);
      } /* if */
    } else {
      string[index++]=c;
      if (c==__T('\n')) {
        break;                  /* read newline, done */
      } else if (lastcr) {
        ungetc(c,fp);           /* carriage return was read, no newline follows */
        break;
      } /* if */
      lastcr=(c==__T('\r'));
    } /* if */
  } /* for */
  assert(index<max);
  string[index]=__T('\0');

  return index;
}

static size_t fputs_cell(FILE *fp,cell *string,int utf8mode)
{
  size_t count=0;

  assert(sizeof(cell)>=4);
  assert(fp!=NULL);
  assert(string!=NULL);

  while (*string!=0) {
    if (utf8mode) {
      cell c=*string;
      if (c<0x80) {
        /* 0xxxxxxx */
        fputc((unsigned char)c,fp);
      } else if (c<0x800) {
        /* 110xxxxx 10xxxxxx */
        fputc((unsigned char)((c>>6) & 0x1f | 0xc0),fp);
        fputc((unsigned char)(c & 0x3f | 0x80),fp);
      } else if (c<0x10000) {
        /* 1110xxxx 10xxxxxx 10xxxxxx (16 bits, BMP plane) */
        fputc((unsigned char)((c>>12) & 0x0f | 0xe0),fp);
        fputc((unsigned char)((c>>6) & 0x3f | 0x80),fp);
        fputc((unsigned char)(c & 0x3f | 0x80),fp);
      } else if (c<0x200000) {
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        fputc((unsigned char)((c>>18) & 0x07 | 0xf0),fp);
        fputc((unsigned char)((c>>12) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>6) & 0x3f | 0x80),fp);
        fputc((unsigned char)(c & 0x3f | 0x80),fp);
      } else if (c<0x4000000) {
        /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        fputc((unsigned char)((c>>24) & 0x03 | 0xf8),fp);
        fputc((unsigned char)((c>>18) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>12) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>6) & 0x3f | 0x80),fp);
        fputc((unsigned char)(c & 0x3f | 0x80),fp);
      } else {
        /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (31 bits) */
        fputc((unsigned char)((c>>30) & 0x01 | 0xfc),fp);
        fputc((unsigned char)((c>>24) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>18) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>12) & 0x3f | 0x80),fp);
        fputc((unsigned char)((c>>6) & 0x3f | 0x80),fp);
        fputc((unsigned char)(c & 0x3f | 0x80),fp);
      } /* if */
    } else {
      /* not UTF-8 mode */
      fputc((unsigned char)*string,fp);
    } /* if */
    string++;
    count++;
  } /* while */
  return count;
}

static size_t fgets_char(FILE *fp, char *string, size_t max)
{
  size_t index;
  int c,lastcr;

  index=0;
  lastcr=0;
  for ( ;; ) {
    assert(index<max);
    if (index==max-1)
      break;                    /* string fully filled */
    if ((c=fgetc(fp))==EOF)
      break;                    /* no more characters */
    string[index++]=(char)c;
    if (c==__T('\n')) {
      break;                    /* read newline, done */
    } else if (lastcr) {
      ungetc(c,fp);             /* carriage return was read, no newline follows */
      break;
    } /* if */
    lastcr=(c==__T('\r'));
  } /* for */
  assert(index<max);
  string[index]=__T('\0');

  return index;
}

#if defined __WIN32__ || defined _WIN32 || defined WIN32
#if defined _UNICODE
wchar_t *_wgetenv(wchar_t *name)
{
static wchar_t buffer[_MAX_PATH];
  buffer[0]=L'\0';
  GetEnvironmentVariable(name,buffer,sizearray(buffer));
  return buffer[0]!=L'\0' ? buffer : NULL;
}
#else
char *getenv(const char *name)
{
static char buffer[_MAX_PATH];
  buffer[0]='\0';
  GetEnvironmentVariable(name,buffer,sizearray(buffer));
  return buffer[0]!='\0' ? buffer : NULL;
}
#endif
#endif

static char *completename(TCHAR *dest, TCHAR *src, size_t size)
{
  #if defined AMXFILE_VAR
    TCHAR *prefix,*ptr;
    size_t len;

    /* only files below a specific path are accessible */
    prefix=getenv(AMXFILE_VAR);

    /* if no specific path for files is present, use the "temporary" path */
    if (prefix==NULL)
      prefix=getenv(__T("tmp"));    /* common under Windows and Unix */
    if (prefix==NULL)
      prefix=getenv(__T("temp"));   /* common under Windows */
    if (prefix==NULL)
      prefix=getenv(__T("tmpdir")); /* common under Unix */

    /* if no path for files is defined, and no temporary directory exists,
     * fail the function; this is for security reasons.
     */
    if (prefix==NULL)
      return NULL;

    if (_tcslen(prefix)+1>=size) /* +1 because directory separator is appended */
      return NULL;
    _tcscpy(dest,prefix);
    /* append a directory separator (if not already present) */
    len=_tcslen(dest);
    if (len==0)
      return NULL;              /* empty start directory is not allowed */
    if (dest[len-1]!=__T(DIRSEP_CHAR) && dest[len-1]!=__T('/') && len+1<size) {
      dest[len]=__T(DIRSEP_CHAR);
      dest[len+1]=__T('\0');
    } /* if */
    assert(_tcslen(dest)<size);

    /* for DOS/Windows and Unix/Linux, skip everyting up to a comma, because
     * this is used to indicate a protocol (e.g. file://C:/myfile.txt)
     */
    #if DIRSEP_CHAR!=':'
      if ((ptr=_tcsrchr(src,__T(':')))!=NULL) {
        src=ptr+1;              /* skip protocol/drive and colon */
        /* a "drive" specifier is sometimes ended with a vertical bar instead
         * of a colon in URL specifications
         */
        if ((ptr=_tcschr(src,__T('|')))!=NULL)
          src=ptr+1;            /* skip drive and vertical bar */
        while (src[0]==__T(DIRSEP_CHAR) || src[0]==__T('/'))
          src++;                /* skip slashes behind the protocol/drive letter */
      } /* if */
    #endif

    /* skip an initial backslash or a drive specifier in the source */
    if ((src[0]==__T(DIRSEP_CHAR) || src[0]==__T('/')) && (src[1]==__T(DIRSEP_CHAR) || src[1]==__T('/'))) {
      /* UNC path */
      char separators[]={__T(DIRSEP_CHAR),__T('/'),__T('\0')};
      src+=2;
      ptr=_tcspbrk(src,separators);
      if (ptr!=NULL)
        src=ptr+1;
    } else if (src[0]==__T(DIRSEP_CHAR) || src[0]==__T('/')) {
      /* simple path starting from the root directory */
      src++;
    } /* if */

    /* disallow any "../" specifications in the source path
     * (the check below should be stricter, but directory names with
     * trailing periods are rare anyway)
     */
    for (ptr=src; *ptr!=__T('\0'); ptr++)
      if (ptr[0]==__T('.') && (ptr[1]==__T(DIRSEP_CHAR) || ptr[1]==__T('/')))
        return NULL;            /* path name is not allowed */

    /* concatenate the drive letter to the destination path */
    if (_tcslen(dest)+_tcslen(src)>=size)
      return NULL;
    _tcscat(dest,src);

    /* change forward slashes into proper directory separators */
    #if DIRSEP_CHAR!='/'
      while ((ptr=_tcschr(dest,__T('/')))!=NULL)
        *ptr=__T(DIRSEP_CHAR);
    #endif
    return dest;

  #else
    if (_tcslen(src)>=size)
      return NULL;
    _tcscpy(dest,src);
    /* change forward slashes into proper directory separators */
    #if DIRSEP_CHAR!='/'
      while ((ptr=_tcschr(dest,__T('/')))!=NULL)
        *ptr=__T(DIRSEP_CHAR);
    #endif
    return dest;
  #endif
}

/* File: fopen(const name[], filemode: mode) */
static cell AMX_NATIVE_CALL n_fopen(AMX *amx, const cell *params)
{
  TCHAR *attrib,*altattrib;
  TCHAR *name,fullname[_MAX_PATH];
  FILE *f = NULL;

  altattrib=NULL;
  switch (params[2] & 0x7fff) {
  case io_read:
    attrib=__T("rb");
    break;
  case io_write:
    attrib=__T("wb");
    break;
  case io_readwrite:
    attrib=__T("r+b");
    altattrib=__T("w+b");
    break;
  case io_append:
    attrib=__T("ab");
    break;
  default:
    return 0;
  } /* switch */

  /* get the filename */
  amx_StrParam(amx,params[1],name);
  if (name!=NULL && completename(fullname,name,sizearray(fullname))!=NULL) {
    f=_tfopen(fullname,attrib);
    if (f==NULL && altattrib!=NULL)
      f=_tfopen(fullname,altattrib);
  } /* if */
  return (cell)f;
}

/* fclose(File: handle) */
static cell AMX_NATIVE_CALL n_fclose(AMX *amx, const cell *params)
{
  UNUSED_PARAM(amx);
  return fclose((FILE*)params[1]) == 0;
}

/* fwrite(File: handle, const string[]) */
static cell AMX_NATIVE_CALL n_fwrite(AMX *amx, const cell *params)
{
  int r = 0;
  cell *cptr;
  char *str;
  int len;

  amx_GetAddr(amx,params[2],&cptr);
  amx_StrLen(cptr,&len);
  if (len==0)
    return 0;

  if ((ucell)*cptr>UNPACKEDMAX) {
    /* the string is packed, write it as an ASCII/ANSI string */
    if ((str=(char*)alloca(len + 1))!=NULL) {
      amx_GetString(str,cptr,0,len);
      r=fputs(str,(FILE*)params[1]);
    } /* if */
  } else {
    /* the string is unpacked, write it as UTF-8 */
    r=fputs_cell((FILE*)params[1],cptr,1);
  } /* if */
  return r;
}

/* fread(File: handle, string[], size=sizeof string, bool:pack=false) */
static cell AMX_NATIVE_CALL n_fread(AMX *amx, const cell *params)
{
  int chars,max;
  char *str;
  cell *cptr;

  max=(int)params[3];
  if (max<=0)
    return 0;
  if (params[4])
    max*=sizeof(cell);

  amx_GetAddr(amx,params[2],&cptr);
  str=(char *)alloca(max);
  if (str==NULL || cptr==NULL) {
    amx_RaiseError(amx, AMX_ERR_NATIVE);
    return 0;
  } /* if */

  if (params[4]) {
    /* store as packed string, read an ASCII/ANSI string */
    chars=fgets_char((FILE*)params[1],str,max);
    assert(chars<max);
    amx_SetString(cptr,str,(int)params[4],0,max);
  } else {
    /* store and unpacked string, interpret UTF-8 */
    chars=fgets_cell((FILE*)params[1],cptr,max,1);
  } /* if */

  assert(chars<max);
  return chars;
}

/* fputchar(File: handle, value, bool:utf8 = true) */
static cell AMX_NATIVE_CALL n_fputchar(AMX *amx, const cell *params)
{
  size_t result;

  UNUSED_PARAM(amx);
  if (params[3]) {
    cell str[2];
    str[0]=params[2];
    str[1]=0;
    result=fputs_cell((FILE*)params[1],str,1);
  } else {
    fputc((int)params[2],(FILE*)params[1]);
  } /* if */
  assert(result==0 || result==1);
  return result;
}

/* fgetchar(File: handle, bool:utf8 = true) */
static cell AMX_NATIVE_CALL n_fgetchar(AMX *amx, const cell *params)
{
  cell str[2];
  size_t result;

  UNUSED_PARAM(amx);
  if (params[2]) {
    result=fgets_cell((FILE*)params[1],str,2,1);
  } else {
    str[0]=fgetc((FILE*)params[1]);
    result= (str[0]!=EOF);
  } /* if */
  assert(result==0 || result==1);
  if (result==0)
    return EOF;
  else
    return str[0];
}

#if PAWN_CELL_SIZE==16
  #define aligncell amx_Align16
#elif PAWN_CELL_SIZE==32
  #define aligncell amx_Align32
#elif PAWN_CELL_SIZE==64 && (defined _I64_MAX || defined HAVE_I64)
  #define aligncell amx_Align64
#else
  #error Unsupported cell size
#endif

/* fblockwrite(File: handle, buffer[], size=sizeof buffer) */
static cell AMX_NATIVE_CALL n_fblockwrite(AMX *amx, const cell *params)
{
  cell *cptr;
  cell count;

  amx_GetAddr(amx,params[2],&cptr);
  if (cptr!=NULL) {
    cell max=params[3];
    ucell v;
    for (count=0; count<max; count++) {
      v=(ucell)*cptr++;
      if (fwrite(aligncell(&v),sizeof(cell),1,(FILE*)params[1])!=1)
        break;          /* write error */
    } /* for */
  } /* if */
  return count;
}

/* fblockread(File: handle, buffer[], size=sizeof buffer) */
static cell AMX_NATIVE_CALL n_fblockread(AMX *amx, const cell *params)
{
  cell *cptr;
  cell count;

  amx_GetAddr(amx,params[2],&cptr);
  if (cptr!=NULL) {
    cell max=params[3];
    ucell v;
    for (count=0; count<max; count++) {
      if (fread(&v,sizeof(cell),1,(FILE*)params[1])!=1)
        break;          /* write error */
      *cptr++=(cell)*aligncell(&v);
    } /* for */
  } /* if */
  return count;
}

/* File: ftemp() */
static cell AMX_NATIVE_CALL n_ftemp(AMX *amx, const cell *params)
{
  UNUSED_PARAM(amx);
  UNUSED_PARAM(params);
  return (cell)tmpfile();
}

/* fseek(File: handle, position, seek_whence: whence=seek_start) */
static cell AMX_NATIVE_CALL n_fseek(AMX *amx, const cell *params)
{
  int whence;
  switch (params[3]) {
  case seek_start:
    whence=SEEK_SET;
    break;
  case seek_current:
    whence=SEEK_CUR;
    break;
  case seek_end:
    whence=SEEK_END;
    //if (params[2]>0)
    //  params[2]=-params[2];
    break;
  default:
    return 0;
  } /* switch */
  UNUSED_PARAM(amx);
  return lseek(fileno((FILE*)params[1]),params[2],whence);
}

/* bool: fremove(const name[]) */
static cell AMX_NATIVE_CALL n_fremove(AMX *amx, const cell *params)
{
  int r=1;
  TCHAR *name,fullname[_MAX_PATH];

  amx_StrParam(amx,params[1],name);
  if (name!=NULL && completename(fullname,name,sizearray(fullname))!=NULL)
    r=_tremove(fullname);
  return r==0;
}

/* bool: frename(const oldname[], const newname[]) */
static cell AMX_NATIVE_CALL n_fremove(AMX *amx, const cell *params)
{
  int r=1;
  TCHAR *name,oldname[_MAX_PATH],newname[_MAX_PATH];

  amx_StrParam(amx,params[1],name);
  if (name!=NULL && completename(oldname,name,sizearray(oldname))!=NULL) {
    amx_StrParam(amx,params[2],name);
    if (name!=NULL && completename(newname,name,sizearray(newname))!=NULL) {
      r=_trename(oldname,newname);
  } /* if */
  return r==0;
}

/* flength(File: handle) */
static cell AMX_NATIVE_CALL n_flength(AMX *amx, const cell *params)
{
  long l,c;
  int fn=fileno((FILE*)params[1]);
  c=lseek(fn,0,SEEK_CUR); /* save the current position */
  l=lseek(fn,0,SEEK_END); /* return the file position at its end */
  lseek(fn,c,SEEK_SET);   /* restore the file pointer */
  UNUSED_PARAM(amx);
  return l;
}

static int match_optcopy(TCHAR *out,int outlen,const TCHAR *in,int skip)
{
  if (out==NULL || skip!=0 || outlen<=0)
    return 0;
  _tcsncpy(out,in,outlen);
  out[outlen-1]='\0';
  return 1;
}

static int matchfiles(const TCHAR *path,int skip,TCHAR *out,int outlen)
{
  int count=0;
  const TCHAR *basename;
  #if DIRSEP_CHAR!='/'
    TCHAR *ptr;
  #endif
  #if defined __WIN32__
    HANDLE hfind;
    WIN32_FIND_DATA fd;
  #else
    /* assume LINUX, FreeBSD, OpenBSD, or some other variant */
    DIR *dir;
    struct dirent *entry;
    TCHAR dirname[_MAX_PATH];
  #endif

  basename=_tcsrchr(path,DIRSEP_CHAR);
  basename=(basename==NULL) ? path : basename+1;
  #if DIRSEP_CHAR!='/'
    ptr=_tcsrchr(basename,DIRSEP_CHAR);
    basename=(ptr==NULL) ? basename : ptr+1;
  #endif

  #if defined __WIN32__
    if ((hfind=FindFirstFile(path,&fd))!=INVALID_HANDLE_VALUE) {
      do {
        if (fpattern_match(basename,fd.cFileName,-1,FALSE)) {
          count++;
          if (match_optcopy(out,outlen,fd.cFileName,skip--))
            break;
        } /* if */
      } while (FindNextFile(hfind,&fd));
      FindClose(hfind);
    } /* if */
  #else
    /* copy directory part only (zero-terminate) */
    if (basename==path) {
      strcpy(dirname,".");
    } else {
      strncpy(dirname,path,(int)(basename-path));
      dirname[(int)(basename-path)]=_T('\0');
    } /* if */
    if ((dir=opendir(dirname))!=NULL) {
      while ((entry=readdir(dir))!=NULL) {
        if (fpattern_match(basename,entry->d_name,-1,TRUE)) {
          count++;
          if (match_optcopy(out,outlen,entry->d_name,skip--))
            break;
        } /* if */
      } /* while */
      closedir(dir);
    } /* if */
  #endif
  return count;
}

/* fexist(const pattern[]) */
static cell AMX_NATIVE_CALL n_fexist(AMX *amx, const cell *params)
{
  int r=0;
  TCHAR *name,fullname[_MAX_PATH];

  amx_StrParam(amx,params[1],name);
  if (name!=NULL && completename(fullname,name,sizearray(fullname))!=NULL)
    r=matchfiles(fullname,0,NULL,0);
  return r;
}

/* bool: fmatch(filename[], const pattern[], index=0, maxlength=sizeof filename) */
static cell AMX_NATIVE_CALL n_fmatch(AMX *amx, const cell *params)
{
  TCHAR *name,fullname[_MAX_PATH]="";
  cell *cptr;

  amx_StrParam(amx,params[2],name);
  if (name!=NULL && completename(fullname,name,sizearray(fullname))!=NULL) {
    if (!matchfiles(fullname,params[3],fullname,sizearray(fullname))) {
      fullname[0]='\0';
    } else {
      /* copy the string into the destination */
      amx_GetAddr(amx,params[1],&cptr);
      amx_SetString(cptr,fullname,1,0,params[4]);
    } /* if */
  } /* if */
  return fullname[0]!='\0';
}

/* bool: fstat(const name[], &size = 0, &timestamp = 0) */
static cell AMX_NATIVE_CALL n_fstat(AMX *amx, const cell *params)
{
  #if !(defined __WIN32__ || defined _WIN32 || defined WIN32)
    #define _stat(n,b)  stat(n,b)
  #endif
  TCHAR *name,fullname[_MAX_PATH]="";
  cell *cptr;
  int result=0;

  amx_StrParam(amx,params[1],name);
  if (name!=NULL && completename(fullname,name,sizearray(fullname))!=NULL) {
    struct stat stbuf;
    if (_tstat(name, &stbuf) == 0) {
      amx_GetAddr(amx,params[2],&cptr);
      *cptr=stbuf.st_size;
      amx_GetAddr(amx,params[3],&cptr);
      *cptr=stbuf.st_mtime;
      result=1;
    } /* if */
  } /* if */
  return result;
}


#if defined __cplusplus
  extern "C"
#endif
AMX_NATIVE_INFO file_Natives[] = {
  { "fopen",       n_fopen },
  { "fclose",      n_fclose },
  { "fwrite",      n_fwrite },
  { "fread",       n_fread },
  { "fputchar",    n_fputchar },
  { "fgetchar",    n_fgetchar },
  { "fblockwrite", n_fblockwrite },
  { "fblockread",  n_fblockread },
  { "ftemp",       n_ftemp },
  { "fseek",       n_fseek },
  { "flength",     n_flength },
  { "fremove",     n_fremove },
  { "frename",     n_frename },
  { "fexist",      n_fexist },
  { "fmatch",      n_fmatch },
  { "fstat",       n_fstat },
  { NULL, NULL }        /* terminator */
};

int AMXEXPORT amx_FileInit(AMX *amx)
{
  return amx_Register(amx, file_Natives, -1);
}

int AMXEXPORT amx_FileCleanup(AMX *amx)
{
  UNUSED_PARAM(amx);
  return AMX_ERR_NONE;
}
