/*  Pawn compiler  - maintenance of various lists
 *
 *  o  Name list (aliases)
 *  o  Include path list
 *  o  Macro definitions (text substitutions)
 *  o  Documentation tags and automatic listings
 *  o  Debug strings
 *
 *  Copyright (c) ITB CompuPhase, 2001-2016
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
 *  Version: $Id: sclist.c 5587 2016-10-25 09:59:46Z  $
 */
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "sc.h"

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

/* a "private" implementation of strdup(), so that porting
 * to other memory allocators becomes easier.
 * By S�ren Hannibal.
 */
SC_FUNC char* duplicatestring(const char* sourcestring)
{
  char* result=(char*)malloc(strlen(sourcestring)+1);
  strcpy(result,sourcestring);
  return result;
}


static stringpair *insert_stringpair(stringpair *root,const char *first,const char *second,int matchlength)
{
  stringpair *cur,*pred;

  assert(root!=NULL);
  assert(first!=NULL);
  assert(second!=NULL);
  /* create a new node, and check whether all is okay */
  if ((cur=(stringpair*)malloc(sizeof(stringpair)))==NULL)
    return NULL;
  cur->first=duplicatestring(first);
  cur->second=duplicatestring(second);
  cur->matchlength=matchlength;
  if (cur->first==NULL || cur->second==NULL) {
    if (cur->first!=NULL)
      free(cur->first);
    if (cur->second!=NULL)
      free(cur->second);
    free(cur);
    return NULL;
  } /* if */
  /* link the node to the tree, find the position */
  for (pred=root; pred->next!=NULL && strcmp(pred->next->first,first)<0; pred=pred->next)
    /* nothing */;
  cur->next=pred->next;
  pred->next=cur;
  return cur;
}

static void delete_stringpairtable(stringpair *root)
{
  stringpair *cur, *next;

  assert(root!=NULL);
  cur=root->next;
  while (cur!=NULL) {
    next=cur->next;
    assert(cur->first!=NULL);
    assert(cur->second!=NULL);
    free(cur->first);
    free(cur->second);
    free(cur);
    cur=next;
  } /* while */
  memset(root,0,sizeof(stringpair));
}

static const stringpair *find_stringpair(const stringpair *cur,const char *first,int matchlength)
{
  int result=0;

  assert(matchlength>0);  /* the function cannot handle zero-length comparison */
  assert(first!=NULL);
  while (cur!=NULL && result<=0) {
    result=(int)*cur->first - (int)*first;
    if (result==0 && matchlength==cur->matchlength) {
      result=strncmp(cur->first,first,matchlength);
      if (result==0)
        return cur;
    } /* if */
    cur=cur->next;
  } /* while */
  return NULL;
}

static int delete_stringpair(stringpair *root,stringpair *item)
{
  stringpair *cur;

  assert(root!=NULL);
  cur=root;
  while (cur->next!=NULL) {
    if (cur->next==item) {
      cur->next=item->next;     /* unlink from list */
      assert(item->first!=NULL);
      assert(item->second!=NULL);
      free(item->first);
      free(item->second);
      free(item);
      return TRUE;
    } /* if */
    cur=cur->next;
  } /* while */
  return FALSE;
}

/* ----- string list functions ----------------------------------- */
static stringlist *insert_string(stringlist *root,const char *string,int append)
{
  stringlist *cur;

  assert(string!=NULL);
  if ((cur=(stringlist*)malloc(sizeof(stringlist)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  if ((cur->text=duplicatestring(string))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  if (append) {
    /* insert as "last" (append mode) */
    assert(root!=NULL);
    while (root->next!=NULL)
      root=root->next;
  } /* if */
  /* if not appending, the string is inserted at the beginning */
  cur->next=root->next;
  root->next=cur;
  return cur;
}

static char *get_string(stringlist *root,int index)
{
  stringlist *cur;

  assert(root!=NULL);
  cur=root->next;
  while (cur!=NULL && index-->0)
    cur=cur->next;
  if (cur!=NULL) {
    assert(cur->text!=NULL);
    return cur->text;
  } /* if */
  return NULL;
}

static stringlist *find_string(stringlist *root,const char *string)
{
  stringlist *cur;

  assert(root!=NULL);
  assert(string!=NULL);
  for (cur=root->next; cur!=NULL; cur=cur->next) {
    if (strcmp(cur->text,string)==0)
      return cur;
  } /* for */
  return NULL;
}

static int delete_string(stringlist *root,int index)
{
  stringlist *cur,*item;

  assert(root!=NULL);
  for (cur=root; cur->next!=NULL && index>0; cur=cur->next,index--)
    /* nothing */;
  if (cur->next!=NULL) {
    item=cur->next;
    cur->next=item->next;       /* unlink from list */
    assert(item->text!=NULL);
    free(item->text);
    free(item);
    return TRUE;
  } /* if */
  return FALSE;
}

SC_FUNC void delete_stringtable(stringlist *root)
{
  stringlist *cur,*next;

  assert(root!=NULL);
  cur=root->next;
  while (cur!=NULL) {
    next=cur->next;
    assert(cur->text!=NULL);
    free(cur->text);
    free(cur);
    cur=next;
  } /* while */
  memset(root,0,sizeof(stringlist));
}


/* ----- alias table --------------------------------------------- */
static stringpair alias_tab = {NULL, NULL, NULL};   /* alias table */

SC_FUNC stringpair *insert_alias(const char *name,const char *alias)
{
  stringpair *cur;

  assert(name!=NULL);
  assert(strlen(name)<=sNAMEMAX);
  assert(alias!=NULL);
  assert(strlen(alias)<=sNAMEMAX);
  if ((cur=insert_stringpair(&alias_tab,name,alias,(int)strlen(name)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  return cur;
}

SC_FUNC int lookup_alias(char *target,const char *name)
{
  const stringpair *cur=find_stringpair(alias_tab.next,name,(int)strlen(name));
  if (cur!=NULL) {
    assert(strlen(cur->second)<=sNAMEMAX);
    strcpy(target,cur->second);
  } /* if */
  return cur!=NULL;
}

SC_FUNC void delete_aliastable(void)
{
  delete_stringpairtable(&alias_tab);
}

/* ----- include paths list -------------------------------------- */
static stringlist includepaths = {NULL, NULL};  /* directory list for include files */

SC_FUNC stringlist *insert_path(const char *path)
{
  return insert_string(&includepaths,path,1);
}

SC_FUNC const char *get_path(int index)
{
  return get_string(&includepaths,index);
}

SC_FUNC void delete_pathtable(void)
{
  delete_stringtable(&includepaths);
  assert(includepaths.next==NULL);
}


/* ----- text substitution patterns ------------------------------ */
#if !defined NO_DEFINE

static stringpair substpair = { NULL, NULL, NULL};  /* list of substitution pairs */

static stringpair *substindex['z'-PUBLIC_CHAR+1]; /* quick index to first character */
static void adjustindex(char c)
{
  stringpair *cur;
  assert(c>='A' && c<='Z' || c>='a' && c<='z' || c=='_' || c==PUBLIC_CHAR);
  assert(PUBLIC_CHAR<'A' && 'A'<'_' && '_'<'z');

  for (cur=substpair.next; cur!=NULL && cur->first[0]!=c; cur=cur->next)
    /* nothing */;
  substindex[(int)c-PUBLIC_CHAR]=cur;
}

SC_FUNC stringpair *insert_subst(const char *pattern,const char *substitution,int prefixlen)
{
  stringpair *cur;

  assert(pattern!=NULL);
  assert(substitution!=NULL);
  if ((cur=insert_stringpair(&substpair,pattern,substitution,prefixlen))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  adjustindex(*pattern);
  return cur;
}

SC_FUNC const stringpair *find_subst(const char *name,int length)
{
  stringpair *item;
  assert(name!=NULL);
  assert(length>0);
  assert(*name>='A' && *name<='Z' || *name>='a' && *name<='z' || *name=='_' || *name==PUBLIC_CHAR);
  item=substindex[(int)*name-PUBLIC_CHAR];
  if (item!=NULL)
    item=(stringpair*)find_stringpair(item,name,length);
  return item;
}

SC_FUNC int delete_subst(const char *name,int length)
{
  stringpair *item;
  assert(name!=NULL);
  assert(length>0);
  assert(*name>='A' && *name<='Z' || *name>='a' && *name<='z' || *name=='_' || *name==PUBLIC_CHAR);
  item=substindex[(int)*name-PUBLIC_CHAR];
  if (item!=NULL)
    item=(stringpair*)find_stringpair(item,name,length);
  if (item==NULL)
    return FALSE;
  delete_stringpair(&substpair,item);
  adjustindex(*name);
  return TRUE;
}

SC_FUNC void delete_substtable(void)
{
  int i;
  delete_stringpairtable(&substpair);
  for (i=0; i<sizeof substindex/sizeof substindex[0]; i++)
    substindex[i]=NULL;
}

#endif /* !defined NO_DEFINE */


/* ----- input file list (explicit files) ------------------------ */
static stringlist sourcefiles = {NULL, NULL};

SC_FUNC stringlist *insert_sourcefile(const char *string)
{
  return insert_string(&sourcefiles,string,1);
}

SC_FUNC const char *get_sourcefile(int index)
{
  return get_string(&sourcefiles,index);
}

SC_FUNC void delete_sourcefiletable(void)
{
  delete_stringtable(&sourcefiles);
  assert(sourcefiles.next==NULL);
}


/* ----- parsed file list (explicit + included files) ------------ */
static stringlist inputfiles = {NULL, NULL};

SC_FUNC stringlist *insert_inputfile(const char *string)
{
  return insert_string(&inputfiles,string,1);
}

SC_FUNC const char *get_inputfile(int index)
{
  return get_string(&inputfiles,index);
}

SC_FUNC void delete_inputfiletable(void)
{
  delete_stringtable(&inputfiles);
  assert(inputfiles.next==NULL);
}


/* ----- symbols that are undefined in #if expressions ----------- */
static stringlist undefsymbols = {NULL, NULL};

SC_FUNC stringlist *insert_undefsymbol(const char *symbolname,int linenr)
{
  stringlist *item;
  char symname[sNAMEMAX+16];
  sprintf(symname,"%x %s",linenr,symbolname);
  item=find_string(&undefsymbols,symname);
  if (item==NULL)
    insert_string(&undefsymbols,symname,0);
  return item;
}

SC_FUNC const char *get_undefsymbol(int index,int *linenr)
{
  const char *ptr=get_string(&undefsymbols,index);
  if (ptr!=NULL) {
    if (linenr!=NULL)
      *linenr=(int)strtol(ptr,NULL,16);
    ptr=strchr(ptr,' ');
    assert(ptr!=NULL);
    ptr++;  /* skip space */
  }
  return ptr;
}

SC_FUNC void delete_undefsymboltable(void)
{
  delete_stringtable(&undefsymbols);
  assert(undefsymbols.next==NULL);
}


/* ----- documentation tags -------------------------------------- */
#if !defined PAWN_LIGHT
static stringlist docstrings = {NULL, NULL};

SC_FUNC stringlist *insert_docstring(const char *string,int append)
{
  return insert_string(&docstrings,string,append);
}

SC_FUNC const char *get_docstring(int index)
{
  return get_string(&docstrings,index);
}

SC_FUNC void delete_docstring(int index)
{
  delete_string(&docstrings,index);
}

SC_FUNC void delete_docstringtable(void)
{
  delete_stringtable(&docstrings);
  assert(docstrings.next==NULL);
}
#endif /* !defined PAWN_LIGHT */


/* ----- autolisting --------------------------------------------- */
static stringlist autolist = {NULL, NULL};

/** insert_autolist() inserts a string from a documentation comment. */
SC_FUNC stringlist *insert_autolist(const char *string)
{
  return insert_string(&autolist,string,1);
}

/** get_autolist() retrieves a string from the collected documentation
 *  comments. */
SC_FUNC const char *get_autolist(int index)
{
  return get_string(&autolist,index);
}

/** delete_autolisttable() clears the list in which documentation strings for
 *  the current function are collected. */
SC_FUNC void delete_autolisttable(void)
{
  delete_stringtable(&autolist);
  assert(autolist.next==NULL);
}


/* ----- heap usage list ----------------------------------------- */
static valuepair heaplist = {NULL, 0, 0};

SC_FUNC valuepair *push_heaplist(long first, long second)
{
  valuepair *cur, *last;
  if ((cur=(valuepair*)malloc(sizeof(valuepair)))==NULL)
    error(103);       /* insufficient memory (fatal error) */

  cur->first=first;
  cur->second=second;
  cur->next=NULL;

  for (last=&heaplist; last->next!=NULL; last=last->next)
    /* nothing */;
  last->next=cur;
  return cur;
}

SC_FUNC int popfront_heaplist(long *first, long *second)
{
  valuepair *front=heaplist.next;
  if (front==NULL)
    return 0;

  /* copy fields */
  *first=front->first;
  *second=front->second;

  /* unlink and free */
  heaplist.next=front->next;
  free(front);
  return 1;
}

SC_FUNC void delete_heaplisttable(void)
{
  valuepair *cur;
  while (heaplist.next!=NULL) {
    cur=heaplist.next;
    heaplist.next=cur->next;
    free(cur);
  } /* while */
}


/* ----- literal string/array list, for merging duplicate strings ----- */
static arraymerge litarray_list = { NULL, 0, 0, NULL };

SC_FUNC void litarray_add(cell baseaddr,cell offset,cell size)
{
  arraymerge *item=(arraymerge*)malloc(sizeof(arraymerge));
  if (item!=NULL) {
    assert(size>0);
    item->data=(cell*)malloc(size*sizeof(cell));
    if (item->data!=NULL) {
      cell idx;
      for (idx=0; idx<size; idx++)
        item->data[idx]=litq[offset+idx];
      item->addr=baseaddr+offset;
      item->size=size;
      item->next=litarray_list.next;
      litarray_list.next=item;
    } else {
      free(item);
    }
  }
}

SC_FUNC cell litarray_find(cell offset,cell size)
{
  arraymerge *item;
  for (item=litarray_list.next; item!=NULL; item=item->next) {
    if (item->size==size) {
      cell idx;
      for (idx=0; idx<size && item->data[idx]==litq[offset+idx]; idx++)
        /* nothing */;
      if (idx==size)
        return item->addr;
    }
  }
  return -1;
}

SC_FUNC void litarray_deleteall(void)
{
  arraymerge *item;
  while (litarray_list.next!=NULL) {
    item=litarray_list.next;
    litarray_list.next=item->next;
    assert(item->data!=NULL);
    free(item->data);
    free(item);
  } /* while */
}


/* ----- debug information --------------------------------------- */

/* These macros are adapted from LibDGG libdgg-int64.h, see
 * http://www.dennougedougakkai-ndd.org/pub/libdgg/
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
  #define __STDC_FORMAT_MACROS
  #define __STDC_CONSTANT_MACROS
  #include <inttypes.h>         /* automatically includes stdint.h */
#elif (defined _MSC_VER || defined __BORLANDC__) && (defined _I64_MAX || defined HAVE_I64)
  #define PRId64    "I64d"
  #define PRIx64    "I64x"
#elif defined __WORDSIZE && __WORDSIZE == 64
  #define PRId64    "ld"
  #define PRIx64    "lx"
#else
  #define PRId64    "lld"
  #define PRIx64    "llx"
#endif
#if PAWN_CELL_SIZE==64
  #define PRIdC         PRId64
  #define PRIxC         PRIx64
  #define CELLCAST(c)   (uint64_t)(c)
#else
  #define PRIdC         "ld"
  #define PRIxC         "lx"
  #define CELLCAST(c)   (unsigned long)(c)
#endif

static stringlist dbgstrings = {NULL, NULL};

SC_FUNC stringlist *insert_dbgfile(const char *filename)
{

  if (sc_status!=statBROWSE && (sc_debug & sSYMBOLIC)!=0) {
    char string[_MAX_PATH+40];
    assert(filename!=NULL);
    assert(strlen(filename)+40<sizeof string);
    sprintf(string,"F:%" PRIxC " %s",CELLCAST(code_idx),filename);
    return insert_string(&dbgstrings,string,1);
  } /* if */
  return NULL;
}

SC_FUNC stringlist *insert_dbgline(int linenr)
{
  if (sc_status==statWRITE && (sc_debug & sSYMBOLIC)!=0) {
    char string[40];
    if (linenr>0)
      linenr--;         /* line numbers are zero-based in the debug information */
    sprintf(string,"L:%" PRIxC " %x",CELLCAST(code_idx),linenr);
    return insert_string(&dbgstrings,string,1);
  } /* if */
  return NULL;
}

SC_FUNC stringlist *insert_dbgsymbol(const symbol *sym)
{
  if (sc_status==statWRITE && (sc_debug & sSYMBOLIC)!=0) {
    char string[2*sNAMEMAX+128];
    char symname[2*sNAMEMAX+16];

    funcdisplayname(symname,sym->name);
    /* address tag:name codestart codeend ident scope [tag:dim ...] */
    if (sym->ident==iFUNCTN) {
      sprintf(string,"S:%" PRIxC " %x:%s %" PRIxC " %" PRIxC " %x %x",
              CELLCAST(sym->addr),sym->tag,symname,CELLCAST(sym->addr),
              CELLCAST(sym->codeaddr),sym->ident,sym->scope);
    } else {
      sprintf(string,"S:%" PRIxC " %x:%s %" PRIxC " %" PRIxC " %x %x",
              CELLCAST(sym->addr),sym->tag,symname,CELLCAST(sym->codeaddr),
              CELLCAST(code_idx),sym->ident,sym->scope);
    } /* if */
    if (sym->ident==iARRAY || sym->ident==iREFARRAY) {
      #if !defined NDEBUG
        int count=sym->dim.array.level;
      #endif
      const symbol *sub;
      strcat(string," [ ");
      for (sub=sym; sub!=NULL; sub=finddepend(sub)) {
        #if !defined NDEBUG
          assert(sub->dim.array.level==count);
          count-=1;
        #endif
        sprintf(string+strlen(string),"%x ",(unsigned)sub->dim.array.length);
        /* ??? also indicate whether the last dimension is packed */
        /* ??? also dump the index field names (but need a dynamically growing string for this) */
      } /* for */
      strcat(string,"]");
    } /* if */

    return insert_string(&dbgstrings,string,1);
  } /* if */
  return NULL;
}

SC_FUNC const char *get_dbgstring(int index)
{
  return get_string(&dbgstrings,index);
}

SC_FUNC void delete_dbgstringtable(void)
{
  delete_stringtable(&dbgstrings);
  assert(dbgstrings.next==NULL);
}
