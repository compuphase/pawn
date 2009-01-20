/*  Support routines for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2003-2009
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
 *  Version: $Id: amxaux.h 4057 2009-01-15 08:21:31Z thiadmer $
 */
#ifndef AMXAUX_H_INCLUDED
#define AMXAUX_H_INCLUDED

#include "amx.h"

#ifdef  __cplusplus
extern  "C" {
#endif

/* loading and freeing programs */
size_t AMXAPI aux_ProgramSize(char *filename);
int AMXAPI aux_LoadProgram(AMX *amx, char *filename, void *memblock);
int AMXAPI aux_FreeProgram(AMX *amx);

/* a readable error message from an error code */
char * AMXAPI aux_StrError(int errnum);

enum {
  CODE_SECTION,
  DATA_SECTION,
  HEAP_SECTION,
  STACK_SECTION,
  /* ----- */
  NUM_SECTIONS
};
int AMXAPI aux_GetSection(AMX *amx, int section, cell **start, size_t *size);

#ifdef  __cplusplus
}
#endif

#endif /* AMXAUX_H_INCLUDED */
