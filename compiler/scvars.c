/*  Pawn compiler
 *
 *  Global (cross-module) variables.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2013
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
 *  Version: $Id: scvars.c 4983 2013-10-21 07:32:57Z  $
 */
#include <stdio.h>
#include <stdlib.h>     /* for _MAX_PATH */
#include "sc.h"

/*  global variables
 *
 *  All global variables that are shared amongst the compiler files are
 *  declared here.
 */
SC_VDEFINE symbol loctab;                   /* local symbol table */
SC_VDEFINE symbol glbtab;                   /* global symbol table */
SC_VDEFINE cell *litq;                      /* the literal queue */
SC_VDEFINE unsigned char *srcline=NULL;     /* the line read from the input file */
SC_VDEFINE const unsigned char *lptr;       /* points to the current position in "srcline" */
SC_VDEFINE constvalue tagname_tab = { NULL, "", 0, 0};  /* tagname table */
SC_VDEFINE constvalue libname_tab = { NULL, "", 0, 0};  /* library table (#pragma library "..." syntax) */
SC_VDEFINE constvalue *curlibrary = NULL;   /* current library */
SC_VDEFINE int pc_addlibtable = TRUE;       /* is the library table added to the AMX file? */
SC_VDEFINE constvalue ntvindex_tab = { NULL, "", 0, 0}; /* native function index table */
SC_VDEFINE symbol *curfunc;                 /* pointer to current function */
SC_VDEFINE char *inpfname;                  /* pointer to name of the file currently read from */
SC_VDEFINE char outfname[_MAX_PATH];        /* intermediate (assembler) file name */
SC_VDEFINE char binfname[_MAX_PATH];        /* binary file name */
SC_VDEFINE char errfname[_MAX_PATH];        /* error file name */
SC_VDEFINE char sc_ctrlchar = CTRL_CHAR;    /* the control character (or escape character)*/
SC_VDEFINE char sc_ctrlchar_org = CTRL_CHAR;/* the default control character */
SC_VDEFINE int litidx    = 0;               /* index to literal table */
SC_VDEFINE int litmax    = sDEF_LITMAX;     /* current size of the literal table */
SC_VDEFINE int stgidx    = 0;      /* index to the staging buffer */
SC_VDEFINE int sc_labnum = 0;      /* number of (internal) labels */
SC_VDEFINE int staging   = 0;      /* true if staging output */
SC_VDEFINE cell declared = 0;      /* number of local cells declared */
SC_VDEFINE cell glb_declared=0;    /* number of global cells declared */
SC_VDEFINE cell code_idx = 0;      /* number of bytes with generated code */
SC_VDEFINE int ntv_funcid= 0;      /* incremental number of native function */
SC_VDEFINE int errnum    = 0;      /* number of errors */
SC_VDEFINE int warnnum   = 0;      /* number of warnings */
SC_VDEFINE int sc_debug  = sCHKBOUNDS; /* by default: bounds checking+assertions */
SC_VDEFINE int sc_asmfile= FALSE;  /* create .ASM file? */
SC_VDEFINE int sc_listing= FALSE;  /* create .LST file? */
SC_VDEFINE int sc_needsemicolon=TRUE;/* semicolon required to terminate expressions? */
SC_VDEFINE int pc_cellsize=4;      /* size (in bytes) of a cell */
SC_VDEFINE int sc_dataalign=4;     /* data alignment value (same as a cell) */
SC_VDEFINE int sc_alignnext=FALSE; /* must frame of the next function be aligned? */
SC_VDEFINE int pc_docexpr=FALSE;   /* must expression be attached to documentation comment? */
SC_VDEFINE int curseg    = 0;      /* 1 if currently parsing CODE, 2 if parsing DATA */
SC_VDEFINE cell pc_stksize=sDEF_AMXSTACK;/* default stack size */
SC_VDEFINE cell pc_amxlimit=0;     /* default abstract machine size limit = none */
SC_VDEFINE cell pc_amxram=0;       /* default abstract machine data size limit = none */
SC_VDEFINE int freading  = FALSE;  /* Is there an input file ready for reading? */
SC_VDEFINE int fline     = 0;      /* the line number in the current file */
SC_VDEFINE short fnumber = 0;      /* the file number in the file table (debugging) */
SC_VDEFINE short fcurrent= 0;      /* current file being processed (debugging) */
SC_VDEFINE short sc_intest=FALSE;  /* true if inside a test */
SC_VDEFINE int pc_sideeffect=0;    /* true if an expression causes a side-effect */
SC_VDEFINE int pc_stmtindent=0;    /* current indent of the statement */
SC_VDEFINE int indent_nowarn=FALSE;/* skip warning "217 loose indentation" */
SC_VDEFINE int pc_tabsize=8;       /* number of spaces that a TAB represents */
SC_VDEFINE int pc_matchedtabsize=0;/* if no tabsize explicitly set, try to detect the tab size */
SC_VDEFINE short sc_allowtags=TRUE;/* allow/detect tagnames in lex() */
SC_VDEFINE int sc_status;          /* read/write status */
SC_VDEFINE int sc_rationaltag=0;   /* tag for rational numbers */
SC_VDEFINE int rational_digits=0;  /* number of fractional digits */
SC_VDEFINE int sc_allowproccall=0; /* allow/detect tagnames in lex() */
SC_VDEFINE short sc_is_utf8=FALSE; /* is this source file in UTF-8 encoding */
SC_VDEFINE char *pc_deprecate=NULL;/* if non-null, mark next declaration as deprecated */
SC_VDEFINE int sc_curstates=0;     /* ID of the current state list */
SC_VDEFINE int pc_optimize=sOPTIMIZE_CORE; /* (peephole) optimization level */
SC_VDEFINE int pc_memflags=0;      /* special flags for the stack/heap usage */
SC_VDEFINE int pc_overlays=0;      /* generate overlay table + instructions? */
SC_VDEFINE int pc_ovl0size[ovlFIRST][2];/* offset & size (in bytes) of the first (special) overlays */
SC_VDEFINE uint64_t pc_cryptkey=0; /* key for encryption of the generated script */

SC_VDEFINE constvalue sc_automaton_tab = { NULL, "", 0, 0}; /* automaton table */
SC_VDEFINE constvalue sc_state_tab = { NULL, "", 0, 0};   /* state table */

SC_VDEFINE FILE *inpf    = NULL;   /* file read from (source or include) */
SC_VDEFINE FILE *inpf_org= NULL;   /* main source file */
SC_VDEFINE FILE *outf    = NULL;   /* (intermediate) text file written to */

SC_VDEFINE jmp_buf errbuf;

#if !defined PAWN_LIGHT
  SC_VDEFINE int sc_makereport=FALSE; /* generate a cross-reference report */
#endif
