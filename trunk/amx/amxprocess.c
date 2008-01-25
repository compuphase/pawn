/*  Process control and Foreign Function Interface module for the Pawn AMX
 *
 *  Copyright (c) ITB CompuPhase, 2005-2008
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
 *  Version: $Id: amxprocess.c 3902 2008-01-23 17:40:01Z thiadmer $
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
#elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
  #include <unistd.h>
  #include <dlfcn.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  /* The package libffi library (required for compiling this extension module
   * under Unix/Linux) is not included, because its license is more restrictive
   * than that of Pawn (even if ever so slightly). Recent versions of the GCC
   * compiler include libffi. A separate download of the libffi package is
   * available at http://sources.redhat.com/libffi/ and
   * http://sablevm.org/download/snapshot/.
   */
  #include <ffi.h>
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
# define _tcscmp        strcmp
# define _tcscpy        strcpy
# define _tcsdup        strdup
# define _tcslen        strlen
# define _tcsncmp       strncmp
# define _tcspbrk       strpbrk
# define _tcsrchr       strrchr
# define _tcstol        strtol
#endif


#define MAXPARAMS 32    /* maximum number of parameters to a called function */


typedef struct tagMODlIST {
  struct tagMODlIST _FAR *next;
  TCHAR _FAR *name;
  unsigned long inst;
  AMX *amx;
} MODLIST;

typedef struct tagPARAM {
  union {
    void *ptr;
    long val;
  } v;
  unsigned char type;
  unsigned char size;
  int range;
} PARAM;

#define BYREF 0x80  /* stored in the "type" field fo the PARAM structure */

static MODLIST ModRoot = { NULL };

/* pipes for I/O redirection */
#if defined __WIN32__ || defined _WIN32 || defined WIN32
  static HANDLE newstdin,newstdout,read_stdout,write_stdin;
#elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
  static int pipe_to[2]={-1,-1};
  static int pipe_from[2]={-1,-1};
  void *inst_ffi=NULL;          /* open handle for libffi */
#endif


static const TCHAR *skippath(const TCHAR *name)
{
  const TCHAR *ptr;

  assert(name != NULL);
  if ((ptr = _tcsrchr(name, __T(DIRSEP_CHAR))) == NULL)
    ptr = name;
  else
    ptr++;
  assert(ptr != NULL);
  return ptr;
}

static MODLIST _FAR *findlib(MODLIST *root, AMX *amx, const TCHAR *name)
{
  MODLIST _FAR *item;
  const TCHAR *ptr = skippath(name);

  for (item = root->next; item != NULL; item = item->next)
    if (_tcscmp(item->name, ptr) == 0 && item->amx == amx)
      return item;
  return NULL;
}

static MODLIST _FAR *addlib(MODLIST *root, AMX *amx, const TCHAR *name)
{
  MODLIST _FAR *item;
  const TCHAR *ptr = skippath(name);

  assert(findlib(root, amx, name) == NULL); /* should not already be there */

  if ((item = malloc(sizeof(MODLIST))) == NULL)
    goto error;
  memset(item, 0, sizeof(MODLIST));

  assert(ptr != NULL);
  if ((item->name = malloc((_tcslen(ptr) + 1) * sizeof(TCHAR))) == NULL)
    goto error;
  _tcscpy(item->name, ptr);

  #if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows
    item->inst = (unsigned long)LoadLibrary(name);
    #if !(defined __WIN32__ || defined _WIN32 || defined WIN32)
      if (item->inst <= 32)
        item->inst = 0;
    #endif
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    /* also load the FFI library, if this is the first call */
    inst_ffi=dlopen("libffi.so",RTLD_NOW);
    if (inst_ffi==NULL)
      inst_ffi=dlopen("libffi-2.00-beta.so",RTLD_NOW);
    if (inst_ffi==NULL)
      goto error;     /* failed to load either the old library or the new libbrary */
    item->inst = (unsigned long)dlopen(name,RTLD_NOW);
  #else
    #error Unsupported environment
  #endif
  if (item->inst == 0)
    goto error;

  item->amx = amx;

  item->next = root->next;
  root->next = item;
  return item;

error:
  if (item != NULL) {
    if (item->name != NULL)
      free(item->name);
    if (item->inst != 0) {
      #if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows
        FreeLibrary((HINSTANCE)item->inst);
      #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
        dlclose((void*)item->inst);
      #else
        #error Unsupported environment
      #endif
    } /* if */
    free(item);
  } /* if */
  return NULL;
}

static int freelib(MODLIST *root, AMX *amx, const TCHAR *name)
{
  MODLIST _FAR *item, _FAR *prev;
  const TCHAR *ptr;
  int count = 0;

  ptr = (name != NULL) ? skippath(name) : NULL;

  for (prev = root, item = prev->next; item != NULL; prev = item, item = prev->next) {
    if ((amx == NULL || amx == item->amx) && (ptr == NULL || _tcscmp(item->name, ptr) == 0)) {
      prev->next = item->next;  /* unlink first */
      assert(item->inst != 0);
      #if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows
        FreeLibrary((HINSTANCE)item->inst);
      #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
        dlclose((void*)item->inst);
      #else
        #error Unsupported environment
      #endif
      assert(item->name != NULL);
      free(item->name);
      free(item);
      count++;
    } /* if */
  } /* for */
  #if defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    if (amx==NULL && name==NULL && inst_ffi!=NULL)
      dlclose(inst_ffi);
  #endif
  return count;
}


#if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows

typedef long (CALLBACK* LIBFUNC)();

/*  push()
**
**  This function is the kind of programming trick that you don't even dare to
**  dream about! With the usual C calling convention, the caller cleans up the
**  stack after calling the function. This allows C functions to be flexible
**  with parameters, both in number and in type.
**  With the Pascal calling convention, used here, the callee (the function)
**  cleans up the stack. But here, function push() doesn't know about any
**  parameters. We neither declare any, nor indicate that the function has no
**  parameters (i.e. the function is not declared having 'void' parameters).
**  When we call function push(), the caller thinks the function cleans up the
**  stack (because of the Pascal calling convention), while the function does
**  not know that it has parameters, so it cannot clean them. As a result,
**  nobody cleans up the stack. Ergo, The parameter you pass to function push()
**  stays on the stack.
*/
static void PASCAL push() { }

LIBFUNC SearchProcAddress(unsigned long inst, const char *functionname)
{
  FARPROC lpfn;

  assert(inst!=0);
  lpfn=GetProcAddress((HINSTANCE)inst,functionname);
  #if defined __WIN32__
    if (lpfn==NULL && strlen(functionname)<128-1) {
      char str[128];
      strcpy(str,functionname);
      #if defined UNICODE
        strcat(str,"W");
      #else
        strcat(str,"A");
      #endif
      lpfn = GetProcAddress((HINSTANCE)inst,str);
    } /* if */
  #endif
  return (LIBFUNC)lpfn;
}

#else

typedef long (* LIBFUNC)();

LIBFUNC SearchProcAddress(unsigned long inst, const char *functionname)
{
  assert(inst!=0);
  return (LIBFUNC)dlsym((void*)inst, functionname);
}

#endif

static void *fillarray(AMX *amx, PARAM *param, cell *cptr)
{
  int i;
  void *vptr;

  vptr = malloc(param->range * (param->size / 8));
  if (vptr == NULL) {
    amx_RaiseError(amx, AMX_ERR_NATIVE);
    return NULL;
  } /* if */

  assert(param->range > 1);
  if (param->size == 8) {
    unsigned char *ptr = (unsigned char *)vptr;
    for (i = 0; i < param->range; i++)
      *ptr++ = (unsigned char)*cptr++;
  } else if (param->size == 16) {
    unsigned short *ptr = (unsigned short *)vptr;
    for (i = 0; i < param->range; i++)
      *ptr++ = (unsigned short)*cptr++;
  } else {
    unsigned long *ptr = (unsigned long *)vptr;
    for (i = 0; i < param->range; i++)
      *ptr++ = (unsigned long)*cptr++;
  } /* for */

  return vptr;
}

/* libcall(const libname[], const funcname[], const typestring[], ...)
 *
 * Loads the DLL or shared library if not yet loaded (the name comparison is
 * case sensitive).
 *
 * typestring format:
 *    Whitespace is permitted between the types, but not inside the type
 *    specification. The string "ii[4]&u16s" is equivalent to "i i[4] &u16 s",
 *    but easier on the eye.
 *
 * types:
 *    i = signed integer, 16-bit in Windows 3.x, else 32-bit in Win32 and Linux
 *    u = unsigned integer, 16-bit in Windows 3.x, else 32-bit in Win32 and Linux
 *    f = IEEE floating point, 32-bit
 *    p = packed string
 *    s = unpacked string
 *    The difference between packed and unpacked strings is only relevant when
 *    the parameter is passed by reference (see below).
 *
 * pass-by-value and pass-by-reference:
 *    By default, parameters are passed by value. To pass a parameter by
 *    reference, prefix the type letter with an "&":
 *    &i = signed integer passed by reference
 *    i = signed integer passed by value
 *    Same for '&u' versus 'u' and '&f' versus 'f'.
 *
 *    Arrays are passed by "copy & copy-back". That is, libcall() allocates a
 *    block of dynamic memory to copy the array into. On return from the foreign
 *    function, libcall() copies the array back to the abstract machine. The
 *    net effect is similar to pass by reference, but the foreign function does
 *    not work in the AMX stack directly. During the copy and the copy-back
 *    operations, libcall() may also transform the array elements, for example
 *    between 16-bit and 32-bit elements. This is done because Pawn only
 *    supports a single cell size, which may not fit the required integer size
 *    of the foreign function.
 *
 *    See "element ranges" for the syntax of passing an array.
 *
 *    Strings may either be passed by copy, or by "copy & copy-back". When the
 *    string is an output parameter (for the foreign function), the size of the
 *    array that will hold the return string must be indicated between square
 *    brackets behind the type letter (see "element ranges"). When the string
 *    is "input only", this is not needed --libcall() will determine the length
 *    of the input string itself.
 *
 *    The tokens 'p' and 's' are equivalent, but 'p[10]' and 's[10]' are not
 *    equivalent: the latter syntaxes determine whether the output from the
 *    foreign function will be stored as a packed or an unpacked string.
 *
 * element sizes:
 *    Add an integer behind the type letter; for example, 'i16' refers to a
 *    16-bit signed integer. Note that the value behind the type letter must
 *    be either 8, 16 or 32.
 *
 *    You should only use element size specifiers on the 'i' and 'u' types. That
 *    is, do not use these specifiers on 'f', 's' and 'p'.
 *
 * element ranges:
 *    For passing arrays, the size of the array may be given behind the type
 *    letter and optional element size. The token 'u[4]' indicates an array of
 *    four unsigned integers, which are typically 32-bit. The token 'i16[8]'
 *    is an array of 8 signed 16-bit integers. Arrays are always passed by
 *    "copy & copy-back"
 *
 * When compiled as Unicode, this library converts all strings to Unicode
 * strings.
 *
 * The calling convention for the foreign functions is assumed:
 * -  "__stdcall" for Win32,
 * -  "far pascal" for Win16
 * -  and the GCC default for Unix/Linux (_cdecl)
 *
 * C++ name mangling of the called function is not handled (there is no standard
 * convention for name mangling, so there is no portable way to convert C++
 * function names to mangled names). Win32 name mangling (used by default by
 * Microsoft compilers on functions declared as __stdcall) is also not handled.
 *
 * Returns the value of the called function.
 */
static cell AMX_NATIVE_CALL n_libcall(AMX *amx, const cell *params)
{
  const TCHAR *libname, *funcname, *typestring;
  MODLIST *item;
  int paramidx, typeidx, idx;
  PARAM ps[MAXPARAMS];
  cell *cptr,result;
  LIBFUNC LibFunc;
  #if defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    ffi_cif cif;
    ffi_type *ptypes[MAXPARAMS];
    void *pvalues[MAXPARAMS];
  #endif

  amx_StrParam(amx, params[1], libname);
  item = findlib(&ModRoot, amx, libname);
  if (item == NULL)
    item = addlib(&ModRoot, amx, libname);
  if (item == NULL) {
    amx_RaiseError(amx, AMX_ERR_NATIVE);
    return 0;
  } /* if */

  /* library is loaded, get the function */
  amx_StrParam(amx, params[2], funcname);
  LibFunc=(LIBFUNC)SearchProcAddress(item->inst, funcname);
  if (LibFunc==NULL) {
    amx_RaiseError(amx, AMX_ERR_NATIVE);
    return 0;
  } /* if */

  /* decode the parameters */
  paramidx=typeidx=0;
  amx_StrParam(amx, params[3], typestring);
  while (paramidx < MAXPARAMS && typestring[typeidx]!=__T('\0')) {
    /* skip white space */
    while (typestring[typeidx]!=__T('\0') && typestring[typeidx]<=__T(' '))
      typeidx++;
    if (typestring[typeidx]==__T('\0'))
      break;
    /* save "pass-by-reference" token */
    ps[paramidx].type=0;
    if (typestring[typeidx]==__T('&')) {
      ps[paramidx].type=BYREF;
      typeidx++;
    } /* if */
    /* store type character */
    ps[paramidx].type |= (unsigned char)typestring[typeidx];
    typeidx++;
    /* set default size, then check for an explicit size */
    #if defined __WIN32__ || defined _WIN32 || defined WIN32
      ps[paramidx].size=32;
    #elif defined _Windows
      ps[paramidx].size=16;
    #endif
    if (_istdigit(typestring[typeidx])) {
      ps[paramidx].size=(unsigned char)_tcstol(&typestring[typeidx],NULL,10);
      while (_istdigit(typestring[typeidx]))
        typeidx++;
    } /* if */
    /* set default range, then check for an explicit range */
    ps[paramidx].range=1;
    if (typestring[typeidx]=='[') {
      ps[paramidx].range=_tcstol(&typestring[typeidx+1],NULL,10);
      while (typestring[typeidx]!=']' && typestring[typeidx]!='\0')
        typeidx++;
      ps[paramidx].type |= BYREF; /* arrays are always passed by reference */
      typeidx++;                  /* skip closing ']' too */
    } /* if */
    /* get pointer to parameter */
    amx_GetAddr(amx,params[paramidx+4],&cptr);
    switch (ps[paramidx].type) {
    case 'i': /* signed integer */
    case 'u': /* unsigned integer */
    case 'f': /* floating point */
      assert(ps[paramidx].range==1);
      ps[paramidx].v.val=(int)*cptr;
      break;
    case 'i' | BYREF:
    case 'u' | BYREF:
    case 'f' | BYREF:
      ps[paramidx].v.ptr=cptr;
      if (ps[paramidx].range>1) {
        /* convert array and pass by address */
        ps[paramidx].v.ptr = fillarray(amx, &ps[paramidx], cptr);
      } /* if */
      break;
    case 'p':
    case 's':
    case 'p' | BYREF:
    case 's' | BYREF:
      if (ps[paramidx].type=='s' || ps[paramidx].type=='p') {
        int len;
        /* get length of input string */
        amx_StrLen(cptr,&len);
        len++;            /* include '\0' */
        /* check max. size */
        if (len<ps[paramidx].range)
          len=ps[paramidx].range;
        ps[paramidx].range=len;
      } /* if */
      ps[paramidx].v.ptr=malloc(ps[paramidx].range*sizeof(TCHAR));
      if (ps[paramidx].v.ptr==NULL)
        return amx_RaiseError(amx, AMX_ERR_NATIVE);
      amx_GetString((char *)ps[paramidx].v.ptr,cptr,sizeof(TCHAR)>1,UNLIMITED);
      break;
    default:
      /* invalid parameter type */
      return amx_RaiseError(amx, AMX_ERR_NATIVE);
    } /* switch */
    paramidx++;
  } /* while */
  if ((params[0]/sizeof(cell)) - 3 != (size_t)paramidx)
    return amx_RaiseError(amx, AMX_ERR_NATIVE); /* format string does not match number of parameters */

  #if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined _Windows
    /* push the parameters to the stack (left-to-right in 16-bit; right-to-left
     * in 32-bit)
     */
#if defined __WIN32__ || defined _WIN32 || defined WIN32
    for (idx=paramidx-1; idx>=0; idx--) {
#else
    for (idx=0; idx<paramidx; idx++) {
#endif
      if ((ps[idx].type=='i' || ps[idx].type=='u' || ps[idx].type=='f') && ps[idx].range==1) {
        switch (ps[idx].size) {
        case 8:
          push((unsigned char)(ps[idx].v.val & 0xff));
          break;
        case 16:
          push((unsigned short)(ps[idx].v.val & 0xffff));
          break;
        default:
          push(ps[idx].v.val);
        } /* switch */
      } else {
        push(ps[idx].v.ptr);
      } /* if */
    } /* for */

    /* call the function; all parameters are already pushed to the stack (the
     * function should remove the parameters from the stack)
     */
    result=LibFunc();
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    /* use libffi (foreign function interface) */
    for (idx = 0; idx < paramidx; idx++) {
      /* copy parameter types */
        switch (ps[idx].type) {
        case 'i': /* signed integer */
          assert(ps[idx].range==1);
          switch (ps[idx].size) {
          case 8:
            ptypes[idx] = &ffi_type_sint8;
            break;
          case 16:
            ptypes[idx] = &ffi_type_sint16;
            break;
          default:
            ptypes[idx] = &ffi_type_sint32;
          } /* switch */
          break;
        case 'u': /* unsigned integer */
          assert(ps[idx].range==1);
          switch (ps[idx].size) {
          case 8:
            ptypes[idx] = &ffi_type_uint8;
            break;
          case 16:
            ptypes[idx] = &ffi_type_uint16;
            break;
          default:
            ptypes[idx] = &ffi_type_uint32;
          } /* switch */
          break;
        case 'f': /* floating point */
          assert(ps[idx].range==1);
          ptypes[idx] = &ffi_type_float;
          break;
        default:  /* strings, arrays, fields passed by reference */
          ptypes[idx] = &ffi_type_pointer;
          break;
        /* switch */
      } /* if */
      /* copy pointer to parameter values */
      pvalues[idx] = &ps[idx].v;
    } /* for */
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, paramidx, &ffi_type_slong, ptypes);
    ffi_call(&cif, FFI_FN(LibFunc), (void*)&result, pvalues);
  #endif

  /* store return values and free allocated memory */
  for (idx=0; idx<paramidx; idx++) {
    switch (ps[idx].type) {
    case 'p':
    case 's':
      free(ps[idx].v.ptr);
      break;
    case 'p' | BYREF:
    case 's' | BYREF:
      amx_GetAddr(amx,params[idx+4],&cptr);
      amx_SetString(cptr,(char *)ps[idx].v.ptr,ps[idx].type==('p'|BYREF),sizeof(TCHAR)>1,UNLIMITED);
      free(ps[idx].v.ptr);
      break;
    case 'i':
    case 'u':
    case 'f':
      assert(ps[idx].range==1);
      break;
    case 'i' | BYREF:
    case 'u' | BYREF:
    case 'f' | BYREF:
      amx_GetAddr(amx,params[idx+4],&cptr);
      if (ps[idx].range==1) {
        /* modify directly in the AMX (no memory block was allocated */
        switch (ps[idx].size) {
        case 8:
          *cptr= (ps[idx].type==('i' | BYREF)) ? (long)((signed char)*cptr) : (*cptr & 0xff);
          break;
        case 16:
          *cptr= (ps[idx].type==('i' | BYREF)) ? (long)((short)*cptr) : (*cptr & 0xffff);
          break;
        } /* switch */
      } else {
        int i;
        for (i=0; i<ps[idx].range; i++) {
          switch (ps[idx].size) {
          case 8:
            *cptr= (ps[idx].type==('i' | BYREF)) ? ((signed char*)ps[idx].v.ptr)[i] : ((unsigned char*)ps[idx].v.ptr)[i];
            break;
          case 16:
            *cptr= (ps[idx].type==('i' | BYREF)) ? ((short*)ps[idx].v.ptr)[i] : ((unsigned short*)ps[idx].v.ptr)[i];
            break;
          default:
            *cptr= (ps[idx].type==('i' | BYREF)) ? ((long*)ps[idx].v.ptr)[i] : ((unsigned long*)ps[idx].v.ptr)[i];
          } /* switch */
        } /* for */
        free((char *)ps[idx].v.ptr);
      } /* if */
      break;
    default:
      assert(0);
    } /* switch */
  } /* for */

  return result;
}

/* bool: libfree(const libname[]="")
 * When the name is an empty string, this function frees all libraries (for this
 * abstract machine). The name comparison is case sensitive.
 * Returns true if one or more libraries were freed.
 */
static cell AMX_NATIVE_CALL n_libfree(AMX *amx, const cell *params)
{
  const TCHAR *libname;
  amx_StrParam(amx,params[1],libname);
  return freelib(&ModRoot,amx,libname) > 0;
}

static void closepipe(void)
{
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    if (newstdin!=NULL) {
      CloseHandle(newstdin);
      newstdin=NULL;
    } /* if */
    if (newstdout!=NULL) {
      CloseHandle(newstdout);
      newstdout=NULL;
    } /* if */
    if (read_stdout!=NULL) {
      CloseHandle(read_stdout);
      read_stdout=NULL;
    } /* if */
    if (write_stdin!=NULL) {
      CloseHandle(write_stdin);
      write_stdin=NULL;
    } /* if */
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    if (pipe_to[0]>=0) {
      close(pipe_to[0]);
      pipe_to[0]=-1;
    } /* if */
    if (pipe_to[1]>=0) {
      close(pipe_to[1]);
      pipe_to[1]=-1;
    } /* if */
    if (pipe_from[0]>=0) {
      close(pipe_from[0]);
      pipe_from[0]=-1;
    } /* if */
    if (pipe_from[1]>=0) {
      close(pipe_from[1]);
      pipe_from[1]=-1;
    } /* if */
  #endif
}

/* PID: procexec(const commandline[])
 * Executes a program. Returns an "id" representing the new process (or 0 on
 * failure).
 */
static cell AMX_NATIVE_CALL n_procexec(AMX *amx, const cell *params)
{
  TCHAR *pgmname;
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    BOOL IsWinNT;
    OSVERSIONINFO VerInfo;
    STARTUPINFO si;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    PROCESS_INFORMATION pi;
  #elif defined _Windows
    HINSTANCE hinst;
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
  	pid_t pid;
  #endif

  amx_StrParam(amx,params[1],pgmname);

  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    /* most of this code comes from a "Borland Network" article, combined
     * with some knowledge gained from a CodeProject article
     */
    closepipe();

    VerInfo.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
    GetVersionEx(&VerInfo);
    IsWinNT = VerInfo.dwPlatformId==VER_PLATFORM_WIN32_NT;

    if (IsWinNT) {       //initialize security descriptor (Windows NT)
      InitializeSecurityDescriptor(&sd,SECURITY_DESCRIPTOR_REVISION);
      SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
      sa.lpSecurityDescriptor = &sd;
    } else {
      sa.lpSecurityDescriptor = NULL;
    } /* if */
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;         //allow inheritable handles

    if (!CreatePipe(&newstdin,&write_stdin,&sa,0)) { //create stdin pipe
      amx_RaiseError(amx, AMX_ERR_NATIVE);
      return 0;
    } /* if */
    if (!CreatePipe(&read_stdout,&newstdout,&sa,0)) { //create stdout pipe
      closepipe();
      amx_RaiseError(amx, AMX_ERR_NATIVE);
      return 0;
    } /* if */

    GetStartupInfo(&si);      //set startupinfo for the spawned process
    si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    si.hStdOutput = newstdout;
    si.hStdError = newstdout;     //set the new handles for the child process
    si.hStdInput = newstdin;

    /* spawn the child process */
    if (!CreateProcess(NULL,(TCHAR*)pgmname,NULL,NULL,TRUE,CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi)) {
      closepipe();
      return 0;
    } /* if */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Sleep(100);
    return pi.dwProcessId;
  #elif defined _Windows
    hinst=WinExec(pgmname,SW_SHOW);
    if (hinst<=32)
      hinst=0;
    return (cell)hinst;
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    /* set up communication pipes first */
    closepipe();
    if (pipe(pipe_to)!=0 || pipe(pipe_from)!=0) {
      closepipe();
      amx_RaiseError(amx, AMX_ERR_NATIVE);
      return 0;
    } /* if */

    /* attempt to fork */
    if ((pid=fork())<0) {
      closepipe();
      amx_RaiseError(amx, AMX_ERR_NATIVE);
      return 0;
    } /* if */

    if (pid==0) {
      /* this is the child process */
      #define MAX_ARGS  10
      TCHAR *args[MAX_ARGS];
      int i;
      dup2(pipe_to[0],STDIN_FILENO);    /* replace stdin with the in side of the pipe */
      dup2(pipe_from[1],STDOUT_FILENO); /* replace stdout with the out side of the pipe */
      close(pipe_to[0]);                /* the pipes are no longer needed */
      close(pipe_to[1]);
      close(pipe_from[0]);
      close(pipe_from[1]);
      pipe_to[0]=-1;
      pipe_to[1]=-1;
      pipe_from[0]=-1;
      pipe_from[1]=-1;
      /* split off the option(s) */
      assert(MAX_ARGS>=2);              /* args[0] is reserved */
      memset(args,0,MAX_ARGS*sizeof(TCHAR*));
      args[0]=pgmname;
      for (i=1; i<MAX_ARGS && args[i-1]!=NULL; i++) {
        if ((args[i]=strchr(args[i-1],' '))!=NULL) {
          args[i][0]='\0';
          args[i]+=1;
        } /* if */
      } /* for */
      /* replace the child fork with a new process */
      if(execvp(pgmname,args)<0)
        return 0;
    } else {
      close(pipe_to[0]);                /* close unused pipes */
      close(pipe_from[1]);
      pipe_to[0]=-1;
      pipe_from[1]=-1;
    } /* if */
    return pid;
  #else
    return (system(pgmname)==0);
  #endif
}

/* bool: procwrite(const line[], bool:appendlf=false)
 */
static cell AMX_NATIVE_CALL n_procwrite(AMX *amx, const cell *params)
{
  const TCHAR *line;
  unsigned long num;

  amx_StrParam(amx,params[1],line);
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    if (write_stdin==NULL)
      return 0;
    WriteFile(write_stdin,line,_tcslen(line),&num,NULL); //send it to stdin
    if (params[2])
      WriteFile(write_stdin,__T("\n"),1,&num,NULL);
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    if (pipe_to[1]<0)
      return 0;
    write(pipe_to[1],line,_tcslen(line));
    if (params[2])
      write(pipe_to[1],__T("\n"),1);
  #endif
  return 1;
}

/* bool: procread(line[], size=sizeof line, bool:striplf=false, bool:packed=false)
 */
static cell AMX_NATIVE_CALL n_procread(AMX *amx, const cell *params)
{
  TCHAR line[128];
  cell *cptr;
  unsigned long num;
  int index;

  index=0;
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    if (read_stdout==NULL)
      return 0;
    do {
      if (!ReadFile(read_stdout,line+index,1,&num,NULL))
        break;
      index++;
    } while (index<sizeof(line)/sizeof(line[0])-1 && line[index-1]!=__T('\n'));
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    if (pipe_from[0]<0)
      return 0;
    do {
      if (read(pipe_from[0],line+index,1)<0)
        break;
      index++;
    } while (index<sizeof(line)/sizeof(line[0])-1 && line[index-1]!=__T('\n'));
  #endif

  if (params[3])
    while (index>0 && (line[index-1]==__T('\r') || line[index-1]==__T('\n')))
      index--;
  line[index]=__T('\0');

  amx_GetAddr(amx,params[1],&cptr);
  amx_SetString(cptr,line,params[4],sizeof(TCHAR)>1,params[2]);
  return 1;
}

/* procwait(PID:pid)
 * Waits until the process has terminated.
 */
static cell AMX_NATIVE_CALL n_procwait(AMX *amx, const cell *params)
{
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    HANDLE hProcess;
    DWORD exitcode;
  #endif

  (void)amx;
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)params[1]);
    if (hProcess != NULL) {
      while (GetExitCodeProcess(hProcess,&exitcode) && exitcode==STILL_ACTIVE)
        Sleep(100);
      CloseHandle(hProcess);
    } /* if */
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    waitpid((pid_t)params[1],NULL,WNOHANG);
  #endif
  return 0;
}


#if defined __cplusplus
  extern "C"
#endif
AMX_NATIVE_INFO ffi_Natives[] = {
  { "libcall",   n_libcall },
  { "libfree",   n_libfree },
  { "procexec",  n_procexec },
  { "procread",  n_procread },
  { "procwrite", n_procwrite },
  { "procwait",  n_procwait },
  { NULL, NULL }        /* terminator */
};

int AMXEXPORT amx_ProcessInit(AMX *amx)
{
  return amx_Register(amx, ffi_Natives, -1);
}

int AMXEXPORT amx_ProcessCleanup(AMX *amx)
{
  freelib(&ModRoot, amx, NULL);
  closepipe();
  return AMX_ERR_NONE;
}
