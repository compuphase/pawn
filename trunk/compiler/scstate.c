/*  Pawn compiler
 *
 *  Machine and state maintenance.
 *
 *  Three lists are maintained here:
 *  - A list of automatons (state machines): these hold a name, a unique id
 *    (in the "index" field) and the memory address of a cell that holds the
 *    current state of the automaton (in the "value" field).
 *  - A list of states for each automaton: a name, an automaton id (in the
 *    "index" field) and a unique id for the state (unique in the automaton;
 *    states belonging to different automatons may have the same id).
 *  - A list of state combinations. Each function may belong to a set of states.
 *    This list assigns a unique id to the combination of the automaton and all
 *    states.
 *
 *  A fourth list is partially maintained here: it is attached to the "symbol"
 *  structure of a function/variable that has states. This list contains the
 *  code label, the id of the state combinations (the state list id) and the
 *  code addresses at which the function starts and ends. These addresses are
 *  currently only used for state functions in an overlay system.
 *
 *  At the start of the compiled code, a set of stub functions is generated.
 *  Each stub function looks up the value of the "state selector" value for the
 *  automaton, and goes with a "switch" instruction to the start address of the
 *  function. This happens in SC4.C.
 *
 *
 *  Copyright (c) ITB CompuPhase, 2005-2015
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
 *  Version: $Id: scstate.c 5181 2015-01-21 09:44:28Z thiadmer $
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sc.h"
#if defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__ || defined __APPLE__
  #include <sclinux.h>
#endif

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

typedef struct s_statepool {
  struct s_statepool *next;
  int *states;          /* list of states in this combination */
  int numstates;        /* number of items in the above list */
  int fsa;              /* automaton id */
  int listid;           /* unique id for this combination list */
} statepool;

static statepool statepool_tab = { NULL, NULL, 0, 0, 0};   /* state combinations table */


static constvalue *find_automaton(const char *name,int *last,char *closestmatch)
{
  constvalue *ptr;
  int dist,closestdist=INT_MAX;

  assert(last!=NULL);
  *last=0;
  if (closestmatch!=NULL)
    *closestmatch='\0';
  ptr=sc_automaton_tab.next;
  while (ptr!=NULL) {
    if (strcmp(name,ptr->name)==0)
      return ptr;
    if (closestmatch!=NULL && strlen(ptr->name)>0) {
      dist=levenshtein_distance(name,ptr->name);
      if (dist<closestdist && dist<=MAX_EDIT_DIST) {
        strcpy(closestmatch,ptr->name);
        closestdist=dist;
      } /* if */
    } /* if */
    if (ptr->index>*last)
      *last=ptr->index;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

SC_FUNC constvalue *automaton_add(const char *name)
{
  constvalue *ptr;
  int last;

  assert(strlen(name)<sizeof(ptr->name));
  ptr=find_automaton(name,&last,NULL);
  if (ptr==NULL) {
    assert(last+1 <= SHRT_MAX);
    ptr=append_constval(&sc_automaton_tab,name,(cell)0,(short)(last+1));
  } /* if */
  return ptr;
}

SC_FUNC constvalue *automaton_find(const char *name,char *closestmatch)
{
  int last; /* dummy, never used */
  return find_automaton(name,&last,closestmatch);
}

SC_FUNC constvalue *automaton_findid(int id)
{
  constvalue *ptr;
  for (ptr=sc_automaton_tab.next; ptr!=NULL && ptr->index!=id; ptr=ptr->next)
    /* nothing */;
  return ptr;
}


static constvalue *find_state(const char *name,int fsa,int *last,char *closestmatch)
{
  constvalue *ptr;
  int dist,closestdist=INT_MAX;

  assert(last!=NULL);
  *last=0;
  if (closestmatch!=NULL)
    *closestmatch='\0';
  ptr=sc_state_tab.next;
  while (ptr!=NULL) {
    if (ptr->index==fsa) {
      if (strcmp(name,ptr->name)==0)
        return ptr;
      if (closestmatch!=NULL && strlen(ptr->name)>0) {
        dist=levenshtein_distance(name,ptr->name);
        if (dist<closestdist && dist<=MAX_EDIT_DIST) {
          strcpy(closestmatch,ptr->name);
          closestdist=dist;
        } /* if */
      } /* if */
      if ((int)ptr->value>*last)
        *last=(int)ptr->value;
    } /* if */
    ptr=ptr->next;
  } /* while */
  return NULL;
}

SC_FUNC constvalue *state_add(const char *name,int fsa)
{
  constvalue *ptr;
  int last;

  assert(strlen(name)<sizeof(ptr->name));
  ptr=find_state(name,fsa,&last,NULL);
  if (ptr==NULL) {
    assert(fsa <= SHRT_MAX);
    ptr=append_constval(&sc_state_tab,name,(cell)(last+1),(short)fsa);
  } /* if */
  return ptr;
}

SC_FUNC constvalue *state_find(const char *name,int fsa_id,char *closestmatch)
{
  int last;     /* dummy */
  return find_state(name,fsa_id,&last,closestmatch);
}

SC_FUNC constvalue *state_findid(int id)
{
  constvalue *ptr;
  for (ptr=sc_state_tab.next; ptr!=NULL && ptr->value!=id; ptr=ptr->next)
    /* nothing */;
  return ptr;
}

SC_FUNC void state_buildlist(int **list,int *listsize,int *count,int stateid)
{
  int idx;

  assert(list!=NULL);
  assert(listsize!=NULL);
  assert(*listsize>=0);
  assert(count!=NULL);
  assert(*count>=0);
  assert(*count<=*listsize);

  if (*count==*listsize) {
    /* To avoid constantly calling malloc(), the list is grown by 4 states at
     * a time.
     */
    *listsize+=4;
    *list=(int*)realloc(*list,*listsize*sizeof(int));
    if (*list==NULL)
      error(103);               /* insufficient memory */
  } /* if */

  /* find the insertion point (the list has to stay sorted) */
  for (idx=0; idx<*count && (*list)[idx]<stateid; idx++)
    /* nothing */;
  if (idx<*count)
    memmove(&(*list)[idx+1],&(*list)[idx],(int)((*count-idx+1)*sizeof(int)));
  (*list)[idx]=stateid;
  *count+=1;
}

static statepool *state_findlist(int *list,int count,int fsa,int *last)
{
  statepool *ptr;
  int i;

  assert(count>0);
  assert(last!=NULL);
  *last=0;
  ptr=statepool_tab.next;
  while (ptr!=NULL) {
    if (ptr->listid>*last)
      *last=ptr->listid;
    if (ptr->fsa==fsa && ptr->numstates==count) {
      /* compare all states */
      for (i=0; i<count && ptr->states[i]==list[i]; i++)
        /* nothing */;
      if (i==count)
        return ptr;
    } /* if */
    ptr=ptr->next;
  } /* while */
  return NULL;
}

static statepool *state_getlist_ptr(int listid)
{
  statepool *ptr;

  assert(listid>0);
  for (ptr=statepool_tab.next; ptr!=NULL && ptr->listid!=listid; ptr=ptr->next)
    /* nothing */;
  return ptr;
}

SC_FUNC int state_addlist(int *list,int count,int fsa)
{
  statepool *ptr;
  int last;

  assert(list!=NULL);
  assert(count>0);
  ptr=state_findlist(list,count,fsa,&last);
  if (ptr==NULL) {
    if ((ptr=(statepool*)malloc(sizeof(statepool)))==NULL)
      error(103);       /* insufficient memory */
    if ((ptr->states=(int*)malloc(count*sizeof(int)))==NULL) {
      free(ptr);
      error(103);       /* insufficient memory */
    } /* if */
    memcpy(ptr->states,list,count*sizeof(int));
    ptr->numstates=count;
    ptr->fsa=fsa;
    ptr->listid=last+1;
    ptr->next=statepool_tab.next;
    statepool_tab.next=ptr;
  } /* if */
  assert(ptr!=NULL);
  return ptr->listid;
}

SC_FUNC void state_deletetable(void)
{
  statepool *ptr;

  while (statepool_tab.next!=NULL) {
    ptr=statepool_tab.next;
    /* unlink first */
    statepool_tab.next=ptr->next;
    /* then delete */
    assert(ptr->states!=NULL);
    free(ptr->states);
    free(ptr);
  } /* while */
}

SC_FUNC int state_getfsa(int listid)
{
  statepool *ptr;

  assert(listid>=0);
  if (listid==0)
    return -1;

  ptr=state_getlist_ptr(listid);
  return (ptr!=NULL) ? ptr->fsa : -1; /* fsa 0 exists */
}

SC_FUNC int state_count(int listid)
{
  statepool *ptr=state_getlist_ptr(listid);
  if (ptr==NULL)
    return 0;           /* unknown list, no states in it */
  return ptr->numstates;
}

SC_FUNC int state_inlist(int listid,int state)
{
  statepool *ptr;
  int i;

  ptr=state_getlist_ptr(listid);
  if (ptr==NULL)
    return FALSE;       /* unknown list, state not in it */
  for (i=0; i<ptr->numstates; i++)
    if (ptr->states[i]==state)
      return TRUE;
  return FALSE;
}

SC_FUNC int state_listitem(int listid,int index)
{
  statepool *ptr;

  ptr=state_getlist_ptr(listid);
  assert(ptr!=NULL);
  assert(index>=0 && index<ptr->numstates);
  return ptr->states[index];
}

static int checkconflict(statepool *psrc,statepool *ptgt)
{
  int s,t;

  assert(psrc!=NULL);
  assert(ptgt!=NULL);
  for (s=0; s<psrc->numstates; s++)
    for (t=0; t<ptgt->numstates; t++)
      if (psrc->states[s]==ptgt->states[t])
        return 1;       /* state conflict */
  return 0;
}

/* This function searches whether one of the states in the list of statepool id's
 * of a symbol exists in any other statepool id's of the same function; it also
 * verifies that all definitions of the symbol are in the same automaton.
 */
SC_FUNC void state_conflict(symbol *root)
{
  statepool *psrc,*ptgt;
  statelist *srcptr,*tgtptr;
  symbol *sym;

  assert(root!=NULL);
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL || sym->ident!=iFUNCTN)
      continue;                 /* hierarchical data type or no function */
    if (sym->states==NULL)
      continue;                 /* this function has no states */
    for (srcptr=sym->states->next; srcptr!=NULL; srcptr=srcptr->next) {
      if (srcptr->id==-1)
        continue;               /* state list id -1 is a special case */
      psrc=state_getlist_ptr(srcptr->id);
      assert(psrc!=NULL);
      for (tgtptr=srcptr->next; tgtptr!=NULL; tgtptr=tgtptr->next) {
        if (tgtptr->id==-1)
          continue;             /* state list id -1 is a special case */
        ptgt=state_getlist_ptr(tgtptr->id);
        assert(ptgt!=NULL);
        if (psrc->fsa!=ptgt->fsa && strcmp(sym->name,_ENTRYFUNC)!=0)
          error(83,sym->name);  /* this function is part of another machine */
        if (checkconflict(psrc,ptgt))
          error(84,sym->name);  /* state conflict */
      } /* for (tgtptr) */
    } /* for (srcptr) */
  } /* for (sym) */
}

/* check whether the two state lists (whose ids are passed in) share any
 * states
 */
SC_FUNC int state_conflict_id(int listid1,int listid2)
{
  statepool *psrc,*ptgt;

  psrc=state_getlist_ptr(listid1);
  assert(psrc!=NULL);
  ptgt=state_getlist_ptr(listid2);
  assert(ptgt!=NULL);
  return checkconflict(psrc,ptgt);
}


SC_FUNC statelist *append_statelist(statelist *root,int id,int label,cell address)
{
  statelist *cur;

  if ((cur=(statelist*)malloc(sizeof(statelist)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  cur->next=NULL;
  cur->id=id;
  cur->label=label;
  cur->addr=address;
  cur->endaddr=0;

  /* insert as "last" (append mode) */
  assert(root!=NULL);
  while (root->next!=NULL)
    root=root->next;
  root->next=cur;

  return cur;
}

SC_FUNC void delete_statelisttable(statelist *root)
{
  statelist *cur,*next;

  assert(root!=NULL);
  cur=root->next;
  while (cur!=NULL) {
    next=cur->next;
    free(cur);
    cur=next;
  } /* while */
  memset(root,0,sizeof(statelist));
}
