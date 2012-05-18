/*  Script Arguments support module for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2005-2011
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
 *  Version: $Id: amxargs.c 4523 2011-06-21 15:03:47Z thiadmer $
 */
#if defined _UNICODE || defined __UNICODE__ || defined UNICODE
# if !defined UNICODE   /* for Windows */
#   define UNICODE
# endif
# if !defined _UNICODE  /* for C library */
#   define _UNICODE
# endif
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "osdefs.h"
#if defined __WIN32__ || defined __MSDOS__
  #include <malloc.h>
#endif
#if defined __WIN32__ || defined _Windows
  #include <windows.h>
#endif
#include "amx.h"

#if defined _UNICODE
# include <tchar.h>
#elif !defined __T
  typedef char          TCHAR;
# define __T(string)    string
# define _istdigit      isdigit
# define _tgetenv       getenv
# define _tcscat        strcat
# define _tcschr        strchr
# define _tcscpy        strcpy
# define _tcsdup        strdup
# define _tcslen        strlen
# define _tcsncmp       strncmp
# define _tcspbrk       strpbrk
# define _tcsrchr       strrchr
# define _tcstol        strtol
#endif

#if !defined AMXARGS_COLON
  #if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined __MSDOS__
    /* A ':' is also a separator for filenames (the disk drive identifier), and
     * therefore it is better not to use it as an name/value seperator for
     * command line argiments as well. So, by default, the library uses only
     * the '=' as the name/value separator.
     */
    #define AMXARGS_COLON 0
  #else
    #define AMXARGS_COLON 1
  #endif
#endif

#if !defined AMXARGS_SKIPARG
  /* The first option may be the name of the script (this is common if the
   * host application takes the name of the script as the first parameter).
   * This can optionally be ignored.
   */
  #define AMXARGS_SKIPARG 0
#endif


static const TCHAR *tokenize(const TCHAR *string, int index, int *length);
static const TCHAR *cmdline = NULL;

static const TCHAR *rawcmdline(void)
{
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
  #elif defined _Windows || defined __MSDOS__
    static char cmdbuffer[128];   /* DOS & Windows 3.1 are never in Unicode mode */
  #elif defined __LINUX__
    static char cmdbuffer[1024];  /* some arbitrary maximum */
  #endif
  const TCHAR *ptr;
  int skip = 0;

  if (cmdline == NULL) {
    #if defined __WIN32__ || defined _WIN32 || defined WIN32
      cmdline = GetCommandLine();
      skip++;
    #elif defined _Windows || defined __MSDOS__
      #if defined _Windows
        unsigned short _psp = GetCurrentPDB();
      #endif
      char _far *cmd = (char _far *)MK_FP(_psp, 128);
      unsigned char length = (unsigned char)*cmd++;
      assert(length < 128);
      assert(length < sizeof cmdbuffer);
      memcpy(cmdbuffer, cmd, length);
      cmdbuffer[length] = '\0';
      if ((cmd == strchr(cmdbuffer, '\r')) != NULL)
        *cmd = '\0';    /* also erase \r after the last option (if any) */
      cmdline = cmdbuffer;
    #elif defined __LINUX__
      /* Options in /proc/<pid>/cmdline are delimited with '\0' characters
       * rather than spaces.
       */
      FILE *fp;
      size_t fsize;
      sprintf(cmdbuffer, "/proc/%d/cmdline", getpid());
      if ((fp = fopen(cmdbuffer, "r")) != NULL) {
        char *ptr;
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize >= sizeof cmdbuffer)
          fsize = sizeof cmdbuffer - 1;
        fread(cmdbuffer, 1, fsize, fp);
        fclose(fp);
        cmdbuffer[fsize] = '\0';        /* terminate with double-zero */
        /* convert '\0' characters to spaces, for uniform parsing */
        for (ptr = cmdbuffer; *ptr != ' '; ptr = strchr(ptr, '\0') + 1)
          *ptr = ' ';
        cmdline = cmdbuffer;
        skip++;
      } /* if */
    #else
      #error Platform not supported
    #endif

    /* skip leading white space */
    while (*cmdline <= __T(' ') && *cmdline != __T('\0'))
      cmdline++;

    #if AMXARGS_SKIPARG
      skip++;
    #endif
    /* skip the first option(s), because it is the name of the host program
     * and the name of the script
     */
    if ((ptr = tokenize(cmdline, skip, NULL)) != NULL)
      cmdline = ptr;
    else
      cmdline = _tcschr(cmdline, __T('\0'));

  } /* if */

  return cmdline;
}

static const TCHAR *tokenize(const TCHAR *string, int index, int *length)
{
  const TCHAR *start = string;
  TCHAR endchar;
  assert(index >= 0);
  assert(start != NULL);
  while (*start == __T(' ') || *start == __T('\t'))
    start++;
  if (*start == __T('\0'))
    return NULL;
  if (*start == __T('"'))
    endchar = *start++;
  else
    endchar = __T(' ');
  while (index > 0 && start != NULL) {
    start = _tcschr(start, endchar);
    if (start != NULL) {
      assert(*start == endchar);
      if (endchar != __T(' '))
        start++;
      while (*start == __T(' ') || *start == __T('\t'))
        start++;
      if (*start == __T('"'))
        endchar = *start++;
      else
        endchar = __T(' ');
      } /* if */
    index--;
  } /* while */
  if (start != NULL && length != NULL) {
    const TCHAR *end;
    if ((end = _tcschr(start, endchar)) == NULL)
      end = _tcschr(start, __T('\0'));
    assert(end != NULL);
    *length = (int)(end - start);
  } /* if */
  return start;
}

static const TCHAR *matcharg(const TCHAR *key, int skip, int *length)
{
  const TCHAR *cmdline = rawcmdline();
  int index, optlen, keylen;
  const TCHAR *option, *vptr;

  keylen = (key != NULL) ? _tcslen(key) : 0;
  index = 0;
  while ((option = tokenize(cmdline, index, length)) != NULL) {
    /* check for a colon or an equal sign (':' or '=') */
    vptr = _tcschr(option, __T('='));
    #if AMXARGS_COLON
      if (vptr == NULL || (int)(vptr - option) > *length)
        vptr = _tcschr(option, __T(':'));
    #endif
    if (vptr != NULL && (int)(vptr - option) > *length)
      vptr = NULL;
    optlen = (vptr != NULL) ? (int)(vptr - option) : 0;
    if (keylen == 0 && vptr == NULL
        || keylen > 0 && keylen == optlen && _tcsncmp(option, key, optlen) == 0)
    {
      if (vptr != NULL)
        optlen++;               /* if ':' or '=' was found, skip it too */
      option += optlen;         /* point behind option */
      *length -= optlen;        /* length of the value, not of the option */
      assert(*length >= 0);
      if (skip-- == 0)
        break;
    } /* if */
    index++;
  } /* while */
  return option;
}


/* bool: argindex(index, value[], maxlength=sizeof value, bool:pack=false)
 * returns true if the option was found and false on error or if the parameter "index" is out of range
 */
static cell AMX_NATIVE_CALL n_argindex(AMX *amx, const cell *params)
{
  const TCHAR *cmdline = rawcmdline();
  const TCHAR *option;
  int length, max;
  TCHAR *str;
  cell *cptr;

  max = (int)params[3];
  if (max <= 0)
    return 0;
  cptr = amx_Address(amx, params[2]);

  if ((option = tokenize(cmdline, params[1], &length)) == NULL) {
    /* option not found, return an empty string */
    *cptr = 0;
    return 0;
  } /* if */

  if (params[4])
    max *= sizeof(cell);
  if (max > length + 1)
    max = length + 1;
  str = (TCHAR *)alloca(max*sizeof(TCHAR));
  if (str == NULL) {
    amx_RaiseError(amx, AMX_ERR_NATIVE);
    return 0;
  } /* if */
  memcpy(str, option, (max - 1) * sizeof(TCHAR));
  str[max - 1] = __T('\0');
  amx_SetString(cptr, (char*)str, (int)params[4], sizeof(TCHAR)>1, max);

  return 1;
}

/* bool: argstr(index=0, const option[]="", value[]="", maxlength=sizeof value, bool:pack=false)
 * returns true if the option was found and false otherwise
 */
static cell AMX_NATIVE_CALL n_argstr(AMX *amx, const cell *params)
{
  const TCHAR *option, *key;
  int length, max;
  TCHAR *str;
  cell *cptr;

  max = (int)params[4];
  if (max <= 0)
    return 0;
  amx_StrParam(amx, params[2], key);
  cptr = amx_Address(amx, params[3]);

  option = matcharg(key, (int)params[1], &length);
  if (option == NULL)
    return 0;           /* option not found */

  /* check whether we must write the value of the option at all; in case the
   * size is one cell and that cell is already zero, we do not write anything
   * back
   */
  assert(params[4] > 0);
  if (params[4] > 1 || *cptr != 0) {
    if (params[5])
      max *= sizeof(cell);
    if (max > length + 1)
      max = length + 1;
    str = (TCHAR *)alloca(max*sizeof(TCHAR));
    if (str == NULL) {
      amx_RaiseError(amx, AMX_ERR_NATIVE);
      return 0;
    } /* if */
    memcpy(str, option, (max - 1) * sizeof(TCHAR));
    str[max - 1] = __T('\0');
    amx_SetString(cptr, (char*)str, (int)params[5], sizeof(TCHAR)>1, max);
  } /* if */

  return 1;
}

/* bool: argvalue(index=0, const option[]="", &value=cellmin)
 * returns true if the option was found and false otherwise
 */
static cell AMX_NATIVE_CALL n_argvalue(AMX *amx, const cell *params)
{
  const TCHAR *option, *key;
  int length;
  cell *cptr;

  amx_StrParam(amx, params[2], key);
  cptr = amx_Address(amx, params[3]);

  option = matcharg(key, (int)params[1], &length);
  if (option == NULL)
    return 0;

  /* check whether we must write the value of the option at all */
  if (length > 0 && (_istdigit(*option) || *option == __T('-')))
    *cptr = _tcstol(option, NULL, 10);

  return 1;
}

/* argcount() */
static cell AMX_NATIVE_CALL n_argcount(AMX *amx, const cell *params)
{
  const TCHAR *cmdline = rawcmdline();
  cell count = 0;
  while (tokenize(cmdline, count, NULL) != NULL)
    count++;
  (void)amx;
  (void)params;
  return count;
}


#if defined __cplusplus
  extern "C"
#endif
const AMX_NATIVE_INFO args_Natives[] = {
  { "argcount",    n_argcount },
  { "argindex",    n_argindex },
  { "argstr",      n_argstr },
  { "argvalue",    n_argvalue },
  { NULL, NULL }        /* terminator */
};

int AMXEXPORT AMXAPI amx_ArgsInit(AMX *amx)
{
  return amx_Register(amx, args_Natives, -1);
}

int AMXEXPORT AMXAPI amx_ArgsCleanup(AMX *amx)
{
  (void)amx;
  return AMX_ERR_NONE;
}

/* A host application calls this function to set the command line for the
 * script. If the host does not do this, the library will use the global
 * options for the application (provided that it can find these). The buffer
 * that is passed in to this function is NOT copied, so it may not be freed
 * after the call.
 */
int AMXEXPORT AMXAPI amx_ArgsSetCmdLine(const TCHAR *cmd)
{
  cmdline = cmd;
  return AMX_ERR_NONE;
}
