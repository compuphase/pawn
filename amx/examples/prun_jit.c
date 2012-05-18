/*  Simple "run-time" for the "Pawn" Abstract Machine, using the JIT compiler.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2008
 *  Copyright (c) Mark Peter, 1998-1999
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
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>     /* for memset() (on some compilers) */
#include "osdefs.h"     /* for _MAX_PATH */
#include "amx.h"

#include <time.h>
#if !defined CLOCKS_PER_SEC     /* some (older) compilers do not have it */
  #define CLOCKS_PER_SEC CLK_TCK
#endif


/* These initialization and clean-up functions are part of two "extension
 * modules" (libraries with native functions) that this run-time uses.
 */
extern int amx_ConsoleInit(AMX *amx);
extern int amx_ConsoleCleanup(AMX *amx);
extern int amx_CoreInit(AMX *amx);
extern int amx_CoredCleanp(AMX *amx);


#if defined __WIN32__
  #include <windows.h>

  static void *vmalloc_exec(long size)
  {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  }

  static void vfree(void *ptr)
  {
    VirtualFree(ptr, 0, MEM_RELEASE);
  }
#elif defined LINUX
  #include <sys/mman.h>
  static void *vmalloc_exec(long size)
  {
    void *ptr = malloc(size);
    if (ptr)
      mprotect(ptr, size, PROT_READ | PROT_WRITE | PROT_EXEC);
    return ptr;
  }

  #define vfree(ptr)             free(ptr)
#else
  #define vmalloc_exec(size)     malloc(size)
  #define vfree(ptr)             free(ptr)
#endif


int AMXAPI aux_LoadProgram(AMX *amx, const char *filename)
{
  int error;
  FILE *fp;
  AMX_HEADER hdr;
  void *pcode = NULL;
  void *ncode = NULL;  // ncode = new machine code
  void *rt = NULL;     // rt = relocation table (temporary)

  /* open the file */
  error = AMX_ERR_NOTFOUND;     /* assume "file not found" */
  if ((fp = fopen(filename, "rb")) == NULL)
    goto fail;

  /* read the header */
  error = AMX_ERR_FORMAT;       /* assume "invalid file format" */
  fread(&hdr, sizeof hdr, 1, fp);
  amx_Align16(&hdr.magic);
  if (hdr.magic != AMX_MAGIC)
    goto fail;

  /* allocate memory for the P-code */
  error = AMX_ERR_MEMORY;       /* assume "insufficient memory" */
  amx_Align32((uint32_t *)&hdr.stp);
  amx_Align32((uint32_t *)&hdr.size);
  if ((pcode = malloc((size_t)hdr.stp)) == NULL)
    goto fail;

  /* read the P-code and initialize the abstract machine */
  rewind(fp);
  fread(pcode, 1, (int)hdr.size, fp);
  fclose(fp);
  fp = NULL;
  memset(amx, 0, sizeof *amx);
  amx->flags = AMX_FLAG_JITC;
  if ((error = amx_Init(amx, pcode)) != AMX_ERR_NONE)
    goto fail;

  /* Now we have an initialized abstract machine, which is normally immediately
   * runnable by amx_Exec(). Instead of running the code, we throw it at the
   * JIT compiler...
   */

  /* allocate memory for the compiled instructions and the temporary
   * relocation table
   */
  error = AMX_ERR_MEMORY;       /* assume "insufficient memory" */
  ncode = malloc(amx->codesize);/* amx->code_size is an estimate */
  if (ncode == NULL)
    goto fail;
  if (amx->reloc_size > 0) {
    rt = malloc(amx->reloc_size);
    if (rt == NULL)
      goto fail;
  } /* if */

  /* JIT rulz! (TM) */
  if ((error = amx_InitJIT(amx, rt, ncode)) != AMX_ERR_NONE)
    goto fail;

  /* The compiled code is relocatable, since only relative jumps are
   * used for destinations within the generated code and absoulute
   * addresses for jumps into the runtime, which is fixed in memory.
   */
  error = AMX_ERR_MEMORY;       /* assume "insufficient memory" */
  if ((amx->base = vmalloc_exec(amx->codesize)) == NULL)
    goto fail;
  memcpy(amx->base, ncode, amx->codesize);
  free(pcode);
  free(rt);
  free(ncode);
  return AMX_ERR_NONE;

fail:
  if (fp != NULL)
    fclose(fp);
  if (pcode != NULL)
    free(pcode);
  if (rt != NULL)
    free(rt);
  if (ncode != NULL)
    free(ncode);
  memset(amx, 0, sizeof *amx);
  return error;
}

/* aux_FreeProgram()
 * Clean up a program and free memory.
 * This function is extracted out of AMXAUX.C.
 */
int AMXAPI aux_FreeProgram(AMX *amx)
{
  if (amx->base!=NULL) {
    amx_Cleanup(amx);
    vfree(amx->base);
    memset(amx,0,sizeof(AMX));
  } /* if */
  return AMX_ERR_NONE;
}

/* aux_StrError()
 * Convert an error code to a "readable" string.
 * This function is extracted out of AMXAUX.C.
 */
char * AMXAPI aux_StrError(int errnum)
{
static char *messages[] = {
      /* AMX_ERR_NONE      */ "(none)",
      /* AMX_ERR_EXIT      */ "Forced exit",
      /* AMX_ERR_ASSERT    */ "Assertion failed",
      /* AMX_ERR_STACKERR  */ "Stack/heap collision (insufficient stack size)",
      /* AMX_ERR_BOUNDS    */ "Array index out of bounds",
      /* AMX_ERR_MEMACCESS */ "Invalid memory access",
      /* AMX_ERR_INVINSTR  */ "Invalid instruction",
      /* AMX_ERR_STACKLOW  */ "Stack underflow",
      /* AMX_ERR_HEAPLOW   */ "Heap underflow",
      /* AMX_ERR_CALLBACK  */ "No (valid) native function callback",
      /* AMX_ERR_NATIVE    */ "Native function failed",
      /* AMX_ERR_DIVIDE    */ "Divide by zero",
      /* AMX_ERR_SLEEP     */ "(sleep mode)",
      /* AMX_ERR_INVSTATE  */ "Invalid state",
      /* 14 */                "(reserved)",
      /* 15 */                "(reserved)",
      /* AMX_ERR_MEMORY    */ "Out of memory",
      /* AMX_ERR_FORMAT    */ "Invalid/unsupported P-code file format",
      /* AMX_ERR_VERSION   */ "File is for a newer version of the AMX",
      /* AMX_ERR_NOTFOUND  */ "File or function is not found",
      /* AMX_ERR_INDEX     */ "Invalid index parameter (bad entry point)",
      /* AMX_ERR_DEBUG     */ "Debugger cannot run",
      /* AMX_ERR_INIT      */ "AMX not initialized (or doubly initialized)",
      /* AMX_ERR_USERDATA  */ "Unable to set user data field (table full)",
      /* AMX_ERR_INIT_JIT  */ "Cannot initialize the JIT",
      /* AMX_ERR_PARAMS    */ "Parameter error",
      /* AMX_ERR_DOMAIN    */ "Domain error, expression result does not fit in range",
      /* AMX_ERR_GENERAL   */ "General error (unknown or unspecific error)",
      /* AMX_ERR_OVERLAY   */ "Overlays are unsupported (JIT) or uninitialized",
    };
  if (errnum < 0 || errnum >= sizeof messages / sizeof messages[0])
    return "(unknown)";
  return messages[errnum];
}

void ExitOnError(AMX *amx, int error)
{
  if (error != AMX_ERR_NONE) {
    printf("Run time error %d: \"%s\" on address %ld\n",
           error, aux_StrError(error), (long)amx->cip);
    exit(1);
  } /* if */
}

void PrintUsage(char *program)
{
  printf("Usage: %s <filename>\n", program);
  exit(1);
}


int main(int argc,char *argv[])
{
  AMX amx;
  cell ret = 0;
  int err;
  clock_t start,end;

  if (argc != 2)
    PrintUsage(argv[0]);        /* function "usage" aborts the program */

  if ((err = aux_LoadProgram(&amx, argv[1])) == AMX_ERR_NOTFOUND) {
    /* try adding an extension */
    char filename[_MAX_PATH];
    strcpy(filename, argv[1]);
    strcat(filename, ".amx");
    err = aux_LoadProgram(&amx, filename);
  } /* if */
  ExitOnError(&amx, err);

  amx_ConsoleInit(&amx);
  err = amx_CoreInit(&amx);
  ExitOnError(&amx, err);

  start=clock();
  err = amx_Exec(&amx, &ret, AMX_EXEC_MAIN);
  while (err == AMX_ERR_SLEEP)
    err = amx_Exec(&amx, &ret, AMX_EXEC_CONT);
  end=clock();
  ExitOnError(&amx, err);

  aux_FreeProgram(&amx);

  if (ret!=0)
    printf("\nReturn value: %ld\n", (long)ret);

  printf("\nRun time:     %.2f seconds\n",(double)(end-start)/CLOCKS_PER_SEC);

  return 0;
}
