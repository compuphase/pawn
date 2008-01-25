/*  Support routines for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2003-2008
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
 *  Version: $Id: amxaux.c 3902 2008-01-23 17:40:01Z thiadmer $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amx.h"
#include "amxaux.h"

size_t AMXAPI aux_ProgramSize(char *filename)
{
  FILE *fp;
  AMX_HEADER hdr;

  if ((fp=fopen(filename,"rb")) == NULL)
    return 0;
  fread(&hdr, sizeof hdr, 1, fp);
  fclose(fp);

  amx_Align16(&hdr.magic);
  amx_Align32((uint32_t *)&hdr.stp);
  return (hdr.magic==AMX_MAGIC) ? (size_t)hdr.stp : 0;
}

int AMXAPI aux_LoadProgram(AMX *amx, char *filename, void *memblock)
{
  FILE *fp;
  AMX_HEADER hdr;
  int result, didalloc;

  /* open the file, read and check the header */
  if ((fp = fopen(filename, "rb")) == NULL)
    return AMX_ERR_NOTFOUND;
  fread(&hdr, sizeof hdr, 1, fp);
  amx_Align16(&hdr.magic);
  amx_Align32((uint32_t *)&hdr.size);
  amx_Align32((uint32_t *)&hdr.stp);
  if (hdr.magic != AMX_MAGIC) {
    fclose(fp);
    return AMX_ERR_FORMAT;
  } /* if */

  /* allocate the memblock if it is NULL */
  didalloc = 0;
  if (memblock == NULL) {
    if ((memblock = malloc(hdr.stp)) == NULL) {
      fclose(fp);
      return AMX_ERR_MEMORY;
    } /* if */
    didalloc = 1;
    /* after amx_Init(), amx->base points to the memory block */
  } /* if */

  /* read in the file */
  rewind(fp);
  fread(memblock, 1, (size_t)hdr.size, fp);
  fclose(fp);

  /* initialize the abstract machine */
  memset(amx, 0, sizeof *amx);
  result = amx_Init(amx, memblock);

  /* free the memory block on error, if it was allocated here */
  if (result != AMX_ERR_NONE && didalloc) {
    free(memblock);
    amx->base = NULL;                   /* avoid a double free */
  } /* if */

  return result;
}

int AMXAPI aux_FreeProgram(AMX *amx)
{
  if (amx->base!=NULL) {
    amx_Cleanup(amx);
    free(amx->base);
    memset(amx,0,sizeof(AMX));
  } /* if */
  return AMX_ERR_NONE;
}

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

int AMXAPI aux_GetSection(AMX *amx, int section, cell **start, size_t *size)
{
  AMX_HEADER *hdr;

  if (amx == NULL || start == NULL || size == NULL)
    return AMX_ERR_PARAMS;

  hdr = (AMX_HEADER*)amx->base;
  switch(section) {
  case CODE_SECTION:
    *start = (cell *)(amx->base + hdr->cod);
    *size = hdr->dat - hdr->cod;
    break;
  case DATA_SECTION:
    *start = (cell *)(amx->data);
    *size = hdr->hea - hdr->dat;
    break;
  case HEAP_SECTION:
    *start = (cell *)(amx->data + hdr->hea);
    *size = amx->hea - hdr->hea;
    break;
  case STACK_SECTION:
    *start = (cell *)(amx->data + amx->stk);
    *size = amx->stp - amx->stk;
    break;
  default:
    return AMX_ERR_PARAMS;
  } /* switch */
  return AMX_ERR_NONE;
}
