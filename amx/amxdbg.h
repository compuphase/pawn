/*  Abstract Machine for the Pawn compiler, debugger support
 *
 *  This file contains extra definitions that are convenient for debugger
 *  support.
 *
 *  Copyright (c) ITB CompuPhase, 2005-2013
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
 *  Version: $Id: amxdbg.h 5148 2014-11-14 13:14:56Z  $
 */

#ifndef AMXDBG_H_INCLUDED
#define AMXDBG_H_INCLUDED

#ifndef AMX_H_INCLUDED
  #include "amx.h"
#endif

#ifdef  __cplusplus
extern  "C" {
#endif

/* Some compilers do not support the #pragma align, which should be fine. Some
 * compilers give a warning on unknown #pragmas, which is not so fine...
 */
#if defined SN_TARGET_PS2 || defined __GNUC__
  #define AMX_NO_ALIGN
#endif

#if defined __GNUC__
  #define PACKED        __attribute__((packed))
#else
  #define PACKED
#endif

#if !defined AMX_NO_ALIGN
  #if defined __LINUX__ || defined __FreeBSD__ || defined __APPLE__
    #pragma pack(1)         /* structures must be packed (byte-aligned) */
  #elif defined MACOS && defined __MWERKS__
	#pragma options align=mac68k
  #else
    #pragma pack(push)
    #pragma pack(1)         /* structures must be packed (byte-aligned) */
    #if defined __TURBOC__
      #pragma option -a-    /* "pack" pragma for older Borland compilers */
    #endif
  #endif
#endif

typedef struct tagAMX_DBG_HDR {
  int32_t  size;            /* size of the debug information chunk */
  uint16_t magic;           /* signature, must be 0xf1ef */
  char     file_version;    /* file format version */
  char     amx_version;     /* required version of the AMX */
  int16_t  flags;           /* currently unused */
  int16_t  files;           /* number of entries in the "file" table */
  int16_t  lines;           /* number of entries in the "line" table */
  int16_t  symbols;         /* number of entries in the "symbol" table */
  int16_t  tags;            /* number of entries in the "tag" table */
  int16_t  automatons;      /* number of entries in the "automaton" table */
  int16_t  states;          /* number of entries in the "state" table */
} PACKED AMX_DBG_HDR;
#define AMX_DBG_MAGIC   0xf1ef

typedef struct tagAMX_DBG_FILE {
  uint32_t address;         /* address in the code segment where generated code (for this file) starts */
  char     name[1];         /* ASCII string, zero-terminated */
} PACKED AMX_DBG_FILE;

typedef struct tagAMX_DBG_LINE {
  uint32_t address;         /* address in the code segment where generated code (for this line) starts */
  int32_t  line;            /* line number */
} PACKED AMX_DBG_LINE;

typedef struct tagAMX_DBG_SYMBOL {
  uint32_t address;         /* address in the data segment or relative to the frame */
  int16_t  tag;             /* tag for the symbol */
  uint32_t codestart;       /* address in the code segment from which this symbol is valid (in scope) */
  uint32_t codeend;         /* address in the code segment until which this symbol is valid (in scope) */
  char     ident;           /* kind of symbol (function/variable) */
  char     vclass;          /* class of symbol (global/local) */
  int16_t  dim;             /* number of dimensions */
  char     name[1];         /* ASCII string, zero-terminated */
} PACKED AMX_DBG_SYMBOL;

typedef struct tagAMX_DBG_SYMDIM {
  int16_t  tag;             /* tag for the array dimension */
  uint32_t size;            /* size of the array dimension */
} PACKED AMX_DBG_SYMDIM;

typedef struct tagAMX_DBG_TAG {
  int16_t  tag;             /* tag id */
  char     name[1];         /* ASCII string, zero-terminated */
} PACKED AMX_DBG_TAG;

typedef struct tagAMX_DBG_MACHINE {
  int16_t  automaton;       /* automaton id */
  uint32_t address;         /* address of state variable */
  char     name[1];         /* ASCII string, zero-terminated */
} PACKED AMX_DBG_MACHINE;

typedef struct tagAMX_DBG_STATE {
  int16_t  state;           /* state id */
  int16_t  automaton;       /* automaton id */
  char     name[1];         /* ASCII string, zero-terminated */
} PACKED AMX_DBG_STATE;

typedef struct tagAMX_DBG {
  AMX_DBG_HDR     *hdr;     /* points to the AMX_DBG header */
  AMX_DBG_FILE    **filetbl;
  AMX_DBG_LINE    *linetbl;
  AMX_DBG_SYMBOL  **symboltbl;
  AMX_DBG_TAG     **tagtbl;
  AMX_DBG_MACHINE **automatontbl;
  AMX_DBG_STATE   **statetbl;
} PACKED AMX_DBG;

#if !defined iVARIABLE
  #define iVARIABLE  1  /* cell that has an address and that can be fetched directly (lvalue) */
  #define iREFERENCE 2  /* iVARIABLE, but must be dereferenced */
  #define iARRAY     3
  #define iREFARRAY  4  /* an array passed by reference (i.e. a pointer) */
  #define iFUNCTN    9
#endif


#if !defined AMXDBG_STRUCTONLY

int AMXAPI dbg_FreeInfo(AMX_DBG *amxdbg);
int AMXAPI dbg_LoadInfo(AMX_DBG *amxdbg, FILE *fp);

int AMXAPI dbg_LinearAddress(AMX *amx, ucell relative_addr, ucell *linear_addr);
int AMXAPI dbg_LookupFile(AMX_DBG *amxdbg, ucell address, const char **filename);
int AMXAPI dbg_LookupFunction(AMX_DBG *amxdbg, ucell address, const char **funcname);
int AMXAPI dbg_LookupLine(AMX_DBG *amxdbg, ucell address, long *line);

int AMXAPI dbg_GetFunctionAddress(AMX_DBG *amxdbg, const char *funcname, const char *filename, ucell *address);
int AMXAPI dbg_GetLineAddress(AMX_DBG *amxdbg, long line, const char *filename, ucell *address);
int AMXAPI dbg_GetAutomatonName(AMX_DBG *amxdbg, int automaton, const char **name);
int AMXAPI dbg_GetStateName(AMX_DBG *amxdbg, int state, const char **name);
int AMXAPI dbg_GetTagName(AMX_DBG *amxdbg, int tag, const char **name);
int AMXAPI dbg_GetVariable(AMX_DBG *amxdbg, const char *symname, ucell scopeaddr, const AMX_DBG_SYMBOL **sym);
int AMXAPI dbg_GetArrayDim(AMX_DBG *amxdbg, const AMX_DBG_SYMBOL *sym, const AMX_DBG_SYMDIM **symdim);

#endif /* AMXDBG_STRUCTONLY */


#if !defined AMX_NO_ALIGN
  #if defined __LINUX__ || defined __FreeBSD__ || defined __APPLE__
    #pragma pack()    /* reset default packing */
  #elif defined MACOS && defined __MWERKS__
    #pragma options align=reset
  #else
    #pragma pack(pop) /* reset previous packing */
  #endif
#endif

#ifdef  __cplusplus
}
#endif

#endif /* AMXDBG_H_INCLUDED */
