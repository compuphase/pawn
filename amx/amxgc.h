/*  Simple garbage collector for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2004-2008
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
 *  Version: $Id: amxgc.h 3902 2008-01-23 17:40:01Z thiadmer $
 */

#ifndef AMXGC_H
#define AMXGC_H

typedef void _FAR (* GC_FREE)(cell unreferenced);
enum {
  GC_ERR_NONE,
  GC_ERR_CALLBACK,      /* no callback, or invalid callback */
  GC_ERR_INIT,          /* garbage collector not initialized (no table size) */
  GC_ERR_MEMORY,        /* insufficient memory to set/resize the table */
  GC_ERR_PARAMS,        /* parameter error */
  GC_ERR_TABLEFULL,     /* domain error, expression result does not fit in range */
  GC_ERR_DUPLICATE,     /* item is already in the table */
};

/* flags */
#define GC_AUTOGROW   1 /* gc_mark() may grow the hash table when it fills up */

int gc_setcallback(GC_FREE callback);

int gc_settable(int exponent,int flags);
int gc_tablestat(int *exponent,int *percentage);
        /* Upon return, "exponent" will hold the values passed to gc_settable();
         * "percentage" is the level (in percent) that the hash table is filled
         * up. Either parameter may be set to NULL.
         */

int gc_mark(cell value);
int gc_scan(AMX *amx);
int gc_clean(void);

#endif /* AMXGC_H */
