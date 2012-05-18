/*  Command-line shell for the "Pawn" Abstract Machine.
 *
 *  Copyright (c) ITB CompuPhase, 2001-2010
 *
 *  This file may be freely used. No warranties of any kind.
 */
#include <stdio.h>
#include <stdlib.h>     /* for exit() */
#include <string.h>     /* for memset() (on some compilers) */
#include "amx.h"
#include "amxaux.c"


void ErrorExit(AMX *amx, int errorcode)
{
  printf("Run time error %d: \"%s\" on address %ld\n",
         errorcode, aux_StrError(errorcode),
         (amx != NULL) ? amx->cip : 0);
  exit(1);
}

void PrintUsage(char *program)
{
  printf("Usage: %s <filename>\n", program);
  exit(1);
}

int main(int argc, char *argv[])
{
  extern int AMXAPI amx_ConsoleInit(AMX *amx);
  extern int AMXAPI amx_ConsoleCleanup(AMX *amx);
  extern int AMXAPI amx_CoreInit(AMX *amx);
  extern int AMXAPI amx_CoreCleanup(AMX *amx);

  size_t memsize;
  void *program;
  AMX amx;
  int index, err;
  cell *address;
  char output[128];

  if (argc != 4)
    PrintUsage(argv[0]);
  if ((memsize = aux_ProgramSize(argv[1])) == 0)
    PrintUsage(argv[0]);

  program = malloc(memsize);
  if (program == NULL)
    ErrorExit(NULL, AMX_ERR_MEMORY);

  err = aux_LoadProgram(&amx, argv[1], program);
  if (err)
    ErrorExit(&amx, err);

  amx_ConsoleInit(&amx);
  err = amx_CoreInit(&amx);
  if (err)
    ErrorExit(&amx, err);

  err = amx_FindPublic(&amx, argv[2], &index);
  if (err)
    ErrorExit(&amx, err);

  err = amx_PushString(&amx, &address, argv[3], 0, 0);
  if (err)
    ErrorExit(&amx, err);

  err = amx_Exec(&amx, NULL, index);
  if (err)
    ErrorExit(&amx, err);

  amx_GetString(output, address, 0, sizeof output);
  amx_Release(&amx, address);
  printf("%s returns %s\n", argv[1], output);

  amx_ConsoleCleanup(&amx);
  amx_CoreCleanup(&amx);
  amx_Cleanup(&amx);
  free(program);
  return 0;
}
