/* Pawn state-graph creator, creates a "dot" file from the XML report; this dot
 * file can be used by the GraphViz programs for further progessing (and to
 * create an image).
 *
 *  Copyright (c) CompuPhase, 2016-2017
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
 *  Version: $Id: stategraph.c 6131 2020-04-29 19:47:15Z thiadmer $
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezxml.h"

typedef struct tagNODE {
  struct tagNODE *next;
  char *name;
  char *automaton;
  char *events; /* events handled internally (which do not cause a state switch) */
  int startstate;
} NODE;

typedef struct tagEVENT {
  struct tagEVENT *next;
  char *name;
  NODE *node1,*node2;
} EVENT;

static NODE noderoot = { NULL };
static EVENT eventroot = { NULL };

static NODE *node_find(const char *name,const char *automaton)
{
  NODE *node;

  assert(name!=NULL && strlen(name)>0);
  assert(automaton==NULL || strlen(automaton)>0);
  for (node=noderoot.next; node!=NULL; node=node->next) {
    assert(node->name!=NULL);
    if (strcmp(node->name,name)==0
        && (node->automaton==NULL && automaton==NULL
            || node->automaton!=NULL && automaton!=NULL && strcmp(node->automaton,automaton)==0))
      return node;
  }
  return NULL;
}

static NODE *node_add(const char *name,const char *automaton,int startstate)
{
  NODE *node,*link;

  assert(name!=NULL && strlen(name)>0);
  assert(automaton==NULL || strlen(automaton)>0);
  /* check whether it already exists */
  node=node_find(name,automaton);
  if (node!=NULL)
    return node;

  /* no matching node found */
  node=malloc(sizeof(NODE));
  memset(node,0,sizeof(NODE));
  node->name=strdup(name);
  if (node->name==NULL) {
    free(node);
    return NULL;
  }
  if (automaton!=NULL && strlen(automaton)>0)
    node->automaton=strdup(automaton);
  node->startstate=startstate;
  /* insert sorten on automaton/startstate/name */
  link=&noderoot;
  while (link->next!=NULL) {
    if (automaton==NULL)
      break;  /* unnamed automaton is always at the root */
    if (link->next->automaton!=NULL && strcmp(automaton,link->next->automaton)<=0)
      break;
    link=link->next;
  }
  if (automaton==NULL && !startstate) {
    while (link->next!=NULL) {
      if (link->next->automaton!=NULL || link->next->startstate==0)
        break;
      link=link->next;
    }
  }
  while (link->next!=NULL) {
    if (automaton==NULL && link->next->automaton!=NULL
        || automaton!=NULL && link->next->automaton!=NULL && strcmp(automaton,link->next->automaton)!=0)
      break;
    if (link->next->startstate!=startstate)
      break;
    if (strcmp(name,link->next->name)<=0)
      break;
    link=link->next;
  }
  node->next=link->next;
  link->next=node;
  return node;
}

static void node_addevent(const char *name,const char *automaton,const char *event)
{
  NODE *node;

  assert(name!=NULL && strlen(name)>0);
  assert(automaton==NULL || strlen(automaton)>0);
  assert(event!=NULL);
  node=node_find(name,automaton);
  if (node!=NULL) {
    if (node->events==NULL) {
      node->events=strdup(event);
    } else {
      node->events=realloc(node->events,strlen(node->events)+strlen(event)+3); /* +2 for "\\n", +1 for terminating '\0' */
      if (node->events!=NULL) {
        strcat(node->events,"\\n");
        strcat(node->events,event);
      }
    }
  }
}

static void node_deleteall(void)
{
  NODE *node;

  while (noderoot.next!=NULL) {
    node=noderoot.next;
    noderoot.next=node->next;
    assert(node->name!=NULL);
    free(node->name);
    if (node->automaton!=NULL)
      free(node->automaton);
    free(node);
  }
}

#if (defined _MSC_VER || defined __GNUC__ || defined __clang__) && !defined __APPLE__
/* Copy src to string dst of size siz.
 * At most siz-1 characters * will be copied. Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred                        .
 *                                                                                   .
 *  Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>, MIT license.                                                                                  .
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif

int main(int argc,char *argv[])
{
  ezxml_t rpt,list,member,transition,automaton;
  const char *func,*fsaname,*source,*target,*ptr;
  char targetfsa[100];
  NODE *node;
  FILE *dot;
  int first_fsa;

  if (argc!=3) {
    printf("Pawn stategraph utility, to create a state graph of a source file.\n\n"
           "Usage: stategraph <input> <output>\n\n"
           "where <input> is the XML report and <output> is in Graphviz \"dot\" format.\n");
    return 1;
  }

  rpt=ezxml_parse_file(argv[1]);
  if (rpt==NULL) {
    printf("Error: failure parsing the input file \"%s\".\n",argv[1]);
    return 1;
  }
  dot=fopen(argv[2],"wt");
  if (dot==NULL) {
    printf("Error: failure to create the output file \"%s\".\n",argv[2]);
    ezxml_free(rpt);
    return 1;
  }

  list=ezxml_child(rpt,"members");
  if (list==NULL) {
    printf("Error: \"members\" list not present.\n");
    ezxml_free(rpt);
    fclose(dot);
    return 1;
  }
  fprintf(dot,"digraph StateDiagram {\n"
              "\tgraph [overlap=prism];\n"
              "\tnode [nodesep=2.0];\n");
  for (member=ezxml_child(list,"member"); member!=NULL; member=member->next) {
    func=ezxml_attr(member,"name");
    if (func[0]!='M' || func[1]!=':')
      continue;
    func+=2;  /* skip prefix */
    fsaname=NULL;
    automaton=ezxml_child(member,"automaton");
    if (automaton!=NULL)
      fsaname=ezxml_attr(automaton,"name");
    for (transition=ezxml_child(member,"transition"); transition!=NULL; transition=transition->next) {
      source=ezxml_attr(transition,"source");
      if (source!=NULL) {
        /* the source state is always in the current automaton */
        node_add(source,fsaname,0);
      } else {
        assert(fsaname==NULL);
        node_add(func,NULL,1);
      }
      target=ezxml_attr(transition,"target");
      if (target!=NULL) {
        /* the target state may be in a different automaton, though */
        if ((ptr=strchr(target,':'))!=NULL) {
          strlcpy(targetfsa,target,sizeof targetfsa);
          targetfsa[ptr-target]='\0';
          target=ptr+1;
        } else {
          strcpy(targetfsa,fsaname);
        }
        node_add(target,targetfsa,0);
      }
      if (target!=NULL && source!=NULL) {
        /* standard transition */
        fprintf(dot,"\t%s_%s -> %s_%s [label=\"%s\"];\n",fsaname,source,targetfsa,target,func);
      } else if (target==NULL && source!=NULL) {
        /* event handled internally, add event name to description */
        node_addevent(source,fsaname,func);
      } else if (target!=NULL && source==NULL) {
        /* event coming from a "start" event */
        assert(fsaname==NULL);
        fprintf(dot,"\t_%s -> %s_%s;\n",func,targetfsa,target);
      }
    }
  }
  /* dump nodes, start with the list of "start" nodes */
  node=noderoot.next;
  while (node!=NULL && node->startstate) {
    /* start nodes are (by definition) not part of an automaton and they can
       (therefore) not handle internal events */
    assert(node->automaton==NULL);
    assert(node->events==NULL);
    fprintf(dot,"\t_%s [shape=cds,label=\"%s\"];\n",node->name,node->name);
    node=node->next;
  }
  /* dump the state nodes */
  first_fsa=1;
  fsaname=NULL;
  while (node!=NULL) {
    if (first_fsa
        || fsaname==NULL && node->automaton!=NULL
        || fsaname!=NULL && node->automaton!=NULL && strcmp(fsaname,node->automaton)!=0)
    {
      if (!first_fsa)
        fprintf(dot,"\t}\n");
      first_fsa=0;
      fsaname=node->automaton;
      if (fsaname==NULL)
        fprintf(dot,"\tsubgraph cluster_0 {\n");
      else
        fprintf(dot,"\tsubgraph cluster_%s {\n\t\tlabel=\"%s\"; labeljust=left; labelloc=t;\n",fsaname,fsaname);
    }
    if (node->automaton==NULL)
      fprintf(dot,"\t\t_%s [shape=Mrecord,label=\"{%s|%s}\"];\n",node->name,node->name,node->events);
    else
      fprintf(dot,"\t\t%s_%s [shape=Mrecord,label=\"{%s|%s}\"];\n",node->automaton,node->name,node->name,node->events);
    node=node->next;
  }
  if (noderoot.next!=NULL)
    fprintf(dot,"\t}\n");
  fprintf(dot,"}\n");
  /* clean up */
  ezxml_free(rpt);
  fclose(dot);
  node_deleteall();
  return 0;
}

