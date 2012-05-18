/*  Simple garbage collector for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2004-2011
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
 *  Version: $Id: amxgc.h 4523 2011-06-21 15:03:47Z thiadmer $
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
