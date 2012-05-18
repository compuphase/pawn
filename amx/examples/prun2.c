/*  Command-line shell for the "Pawn" Abstract Machine.
 *
 *  Copyright (c) ITB CompuPhase, 2001-2010
 *
 *  This file may be freely used. No warranties of any kind.
 */
#include <stdio.h>
#include <stdlib.h>     /* for exit() */
#include <signal.h>
#include <string.h>     /* for memset() (on some compilers) */
#include "amx.h"
#include "amxaux.c"

void AMXAPI core_Init(void);  /* two functions from AMX_CORE.C */
void AMXAPI core_Exit(void);

static int abortflagged = 0;
void sigabort(int sig)
{
  abortflagged = 1;
  signal(sig, sigabort);    /* re-install the signal handler */
}

int AMXAPI aux_Monitor(AMX *amx)
{
  /* check whether an "abort" was requested */
  return abortflagged ? AMX_ERR_EXIT : AMX_ERR_NONE;
}


void ErrorExit(AMX *amx, int errorcode)
{
  printf("Run time error %d: \"%s\" on address %ld\n",
         errorcode, aux_StrError(errorcode),
         (amx != NULL) ? amx->cip : 0);
  exit(1);
}

void PrintUsage(char *program)
{
  printf("Usage: %s <filename>\n<filename> is a compiled script.\n", program);
  exit(1);
}

int main(int argc,char *argv[])
{
  extern AMX_NATIVE_INFO console_Natives[];
  extern AMX_NATIVE_INFO core_Natives[];

  AMX amx;
  cell ret = 0;
  int err;

  if (argc != 2)
    PrintUsage(argv[0]);

  err = aux_LoadProgram(&amx, argv[1], NULL);
  if (err != AMX_ERR_NONE)
    ErrorExit(&amx, err);

  amx_Register(&amx, console_Natives, -1);
  err = amx_Register(&amx, core_Natives, -1);
  if (err)
    ErrorExit(&amx, err);

  amx_SetDebugHook(&amx, aux_Monitor);
  signal(SIGINT, sigabort);

  err = amx_Exec(&amx, &ret, AMX_EXEC_MAIN);
  if (err)
    ErrorExit(&amx, err);
  printf("%s returns %ld\n", argv[1], (long)ret);

  aux_FreeProgram(&amx);
  return 0;
}
