/*  Simple allocation from a memory pool, with automatic release of
 *  least-recently used blocks (LRU blocks).
 *
 *  Copyright (c) ITB CompuPhase, 2007-2013
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
 *  Version: $Id$
 */
#ifndef AMXPOOL_H_INCLUDED
#define AMXPOOL_H_INCLUDED

void  amx_poolinit(void *pool, unsigned size);
void *amx_poolalloc(unsigned size, int index);
void  amx_poolfree(void *block);
void *amx_poolfind(int index);
int   amx_poolprotect(int index);


#endif /* AMXPOOL_H_INCLUDED */
