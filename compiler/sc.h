/*  Pawn compiler
 *
 *  Pawn is a scripting language system consisting of a compiler and an
 *  abstract machine, for building and running programs in the Pawn language.
 *  The Pawn compiler is drafted after the Small-C compiler Version 2.01,
 *  originally created by Ron Cain, july 1980, and enhanced by James E. Hendrix.
 *
 *  This version comes close to a complete rewrite.
 *
 *  Copyright ITB CompuPhase, 1997-2011
 *  Copyright J.E. Hendrix, 1982, 1983
 *  Copyright R. Cain, 1980
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
 *  Version: $Id: sc.h 4611 2011-12-05 17:46:53Z thiadmer $
 */
#ifndef SC_H_INCLUDED
#define SC_H_INCLUDED
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#if defined __BORLANDC__ && defined _Windows && !(defined __32BIT__ || defined __WIN32__)
  /* setjmp() and longjmp() not well supported in 16-bit windows */
  #include <windows.h>
  typedef int jmp_buf[9];
  #define setjmp(b)     Catch(b)
  #define longjmp(b,e)  Throw(b,e)
#else
  #include <setjmp.h>
#endif

#if !defined PAWN_CELL_SIZE
  #define PAWN_CELL_SIZE 64 /* by default, maximum cell size = 64-bit */
#endif
#include "../amx/osdefs.h"
#include "../amx/amx.h"

/* Note: the "cell" and "ucell" types are defined in AMX.H */

#define PUBLIC_CHAR   '@'   /* character that defines a function "public" */
#define CTRL_CHAR     '\\'  /* default control character */
#define sCHARBITS     8     /* size of a packed character */

#define sDIMEN_MAX    3     /* maximum number of array dimensions */
#define sLINEMAX      1023  /* input line length (in characters) */
#define sCOMP_STACK   32    /* maximum nesting of #if .. #endif sections */
#define sDEF_LITMAX   500   /* initial size of the literal pool, in "cells" */
#define sDEF_AMXSTACK 4096  /* default stack size for AMX files */
#define PREPROC_TERM  '\x7f'/* termination character for preprocessor expressions (the "DEL" code) */
#define sDEF_PREFIX   "default.inc" /* default prefix filename */

typedef union {
  void *pv;                 /* e.g. a name */
  int i;
} stkitem;                  /* type of items stored on the compiler stack */

/*  Equate table, tagname table, library table, ... */
typedef struct s_constvalue {
  struct s_constvalue *next;
  char name[sNAMEMAX+1];
  cell value;
  int index;            /* tag for symbolic-array fields; automaton id. for states and automatons */
  char usage;           /* currently only for pseudo-arrays in symbolic-array fields */
} constvalue;

typedef struct s_arginfo {  /* function argument info */
  char name[sNAMEMAX+1];
  char ident;           /* iVARIABLE, iREFERENCE, iREFARRAY or iVARARGS */
  char usage;           /* uCONST, uPACKED */
  /* tags */
  int *tags;            /* argument tag id. list */
  int numtags;          /* number of tags in the tag list */
  /* dimension */
  int dim[sDIMEN_MAX];  /* length of each dimension */
  constvalue *dimnames[sDIMEN_MAX]; /* for symbolic dimensions, the name list */
  int numdim;           /* number of dimensions */
  /* default value */
  unsigned char hasdefault; /* bit0: is there a default value? bit6: "tagof"; bit7: "sizeof" */
  union {
    cell val;           /* default value */
    struct {
      char *symname;    /* name of another symbol */
      short level;      /* indirection level for that symbol */
    } size;             /* used for "sizeof" default value */
    struct {
      cell *data;       /* values of default array */
      int size;         /* complete length of default array */
      int arraysize;    /* size to reserve on the heap */
      cell addr;        /* address of the default array in the data segment */
    } array;
  } defvalue;           /* default value, or pointer to default array */
  int defvalue_tag;     /* tag of the default value */
} arginfo;

typedef struct s_statelist {
  struct s_statelist *next;
  int label;            /* code label (for functions), or overlay index */
  cell addr;            /* code start address (for functions) */
  cell endaddr;         /* code end address (for functions) */
  int id;               /* state-list id */
} statelist;

/*  Symbol table format
 *
 *  The symbol name read from the input file is stored in "name", the
 *  value of "addr" is written to the output file. The address in "addr"
 *  depends on the class of the symbol:
 *      global          offset into the data segment
 *      local           offset relative to the stack frame
 *      label           generated hexadecimal number
 *      function        offset into code segment
 */
typedef struct s_symbol {
  struct s_symbol *next;
  struct s_symbol *parent;  /* hierarchical types (multi-dimensional arrays) */

  char name[sNAMEMAX+1];
  uint32_t hash;        /* value derived from name, for quicker searching */

  cell addr;            /* address or offset (or value for constant) */
  cell codeaddr;        /* address (in the code segment) where the symbol declaration
                         * starts (for a function, the start is in "addr" and the
                         * end is in "codeaddr") */
  int index;            /* overlay index number or index for native function */

  char vclass;          /* sLOCAL if "addr" refers to a local symbol */
  char ident;           /* see below for possible values */
  short usage;          /* see below for possible values */
  char flags;           /* see below for possible values */
  int compound;         /* compound level (braces nesting level) */
  int tag;              /* tagname id */

  union {
    int declared;       /* label: how many local variables are declared */
    constvalue *lib;    /* native function: library it is part of */
    long stacksize;     /* normal/public function: stack requirements */
    int enumlist;       /* enumerated constants: unique sequence number */
  } x;                  /* 'x' for 'extra' */

  union {
    arginfo *arglist;   /* types of all parameters for functions */
    struct {
      cell length;      /* arrays: length (size) of this dimension */
      short level;      /* number of dimensions below this level */
      constvalue *names;/* for symbolic dimensions, the name list */
    } array;
  } dim;                /* for 'dimension', both functions and arrays */

  statelist *states;    /* list of state function/state variable ids + addresses */
  int fvisible;         /* static global variables: file number of the declaration (for visibility test) */
  int fnumber;          /* file number for the declaration */
  int lnumber;          /* line number (in source file "fnumber") for the declaration */

  struct s_symbol **refer;  /* referrer list, functions that "use" this symbol */
  int numrefers;        /* number of entries in the referrer list */

  char *documentation;  /* optional documentation string */
} symbol;


/*  Possible entries for "ident". These are used in the "symbol", "value"
 *  and arginfo structures. Not every constant is valid for every use.
 *  In an argument list, the list is terminated with a "zero" ident; labels
 *  cannot be passed as function arguments, so the value 0 is overloaded.
 */
#define iLABEL      0
#define iVARIABLE   1   /* cell that has an address and that can be fetched directly (lvalue) */
#define iREFERENCE  2   /* iVARIABLE, but must be dereferenced */
#define iARRAY      3
#define iREFARRAY   4   /* an array passed by reference (i.e. a pointer) */
#define iARRAYCELL  5   /* array element, cell that must be fetched indirectly */
#define iARRAYCHAR  6   /* array element, character from cell from array */
#define iEXPRESSION 7   /* expression result, has no address (rvalue) */
#define iCONSTEXPR  8   /* constant expression (or constant symbol) */
#define iFUNCTN     9
#define iREFFUNC    10
#define iVARARGS    11  /* function specified ... as argument(s) */

/*  Possible entries for "usage"
 *
 *  This byte is used as a serie of bits, the syntax is different for
 *  functions and other symbols:
 *
 *  VARIABLE
 *  bits: 0     (uDEFINE) the variable is defined in the source file
 *        1     (uREAD) the variable is "read" (accessed) in the source file
 *        2     (uWRITTEN) the variable is altered (assigned a value)
 *        3     (uCONST) the variable is constant (may not be assigned to)
 *        4     (uPUBLIC) the variable is public
 *        5     (uPACKED) the array variable is defined as packed (only for the last dimension of an array)
 *        6     (uSTOCK) the variable is discardable (without warning)
 *
 *  FUNCTION
 *  bits: 0     (uDEFINE) the function is defined ("implemented") in the source file
 *        1     (uREAD) the function is invoked in the source file
 *        2     (uRETVALUE) the function returns a value (or should return a value)
 *        3     (uPROTOTYPED) the function was prototyped (implicitly via a definition or explicitly)
 *        4     (uPUBLIC) the function is public
 *        5     (uNATIVE) the function is native
 *        6     (uSTOCK) the function is discardable (without warning)
 *        7     (uMISSING) the function is not implemented in this source file
 *        8     (uFORWARD) the function is explicitly forwardly declared
 *
 *  CONSTANT
 *  bits: 0     (uDEFINE) the symbol is defined in the source file
 *        1     (uREAD) the constant is "read" (accessed) in the source file
 *        2     (uWRITTEN) redundant, but may be set for constants passed by reference
 *        3     (uPREDEF) the constant is pre-defined and should be kept between passes
 *        5     (uPACKED) the constant refers to a packed pseudo-array
 */
#define uDEFINE   0x001
#define uREAD     0x002
#define uWRITTEN  0x004
#define uRETVALUE 0x004 /* function returns (or should return) a value */
#define uCONST    0x008
#define uPROTOTYPED 0x008
#define uPREDEF   0x008 /* constant is pre-defined */
#define uPUBLIC   0x010
#define uNATIVE   0x020
#define uPACKED   0x020
#define uSTOCK    0x040
#define uMISSING  0x080
#define uFORWARD  0x100
#define uVISITED  0x200 /* temporary flag, to mark fields as "visited" in recursive loops */
/* uRETNONE is not stored in the "usage" field of a symbol. It is
 * used during parsing a function, to detect a mix of "return;" and
 * "return value;" in a few special cases.
 */
#define uRETNONE  0x10

#define flgDEPRICATED 0x01  /* symbol is deprecated (avoid use) */

#define uTAGOF    0x40  /* set in the "hasdefault" field of the arginfo struct */
#define uSIZEOF   0x80  /* set in the "hasdefault" field of the arginfo struct */

#define uMAINFUNC "main"
#define uENTRYFUNC "entry"
#define uEXITFUNC "exit"

#define sGLOBAL   0     /* global variable/constant class (no states) */
#define sLOCAL    1     /* local variable/constant */
#define sSTATIC   2     /* global life, local scope */

#define sSTATEVAR  3    /* criterion to find variables (sSTATEVAR implies a global variable) */

typedef struct s_value {
  symbol *sym;          /* symbol in symbol table, NULL for (constant) expression */
  cell constval;        /* value of the constant expression (if ident==iCONSTEXPR)
                         * also used for the size of a literal array */
  int tag;              /* tag (of the expression) */
  char ident;           /* iCONSTEXPR, iVARIABLE, iARRAY, iARRAYCELL,
                         * iEXPRESSION or iREFERENCE */
  char boolresult;      /* boolean result for relational operators, also used to flag "packed arrays" for literal arrays */
  cell *arrayidx;       /* last used array indices, for checking self assignment */
} value;

/*  "while" statement queue (also used for "for" and "do - while" loops) */
enum {
  wqBRK,        /* used to restore stack for "break" */
  wqCONT,       /* used to restore stack for "continue" */
  wqLOOP,       /* loop start label number */
  wqEXIT,       /* loop exit label number (jump if false) */
  /* --- */
  wqSIZE        /* "while queue" size */
};
#define wqTABSZ (24*wqSIZE)    /* 24 nested loop statements */

enum {
  statIDLE,     /* not compiling yet */
  statFIRST,    /* first pass */
  statWRITE,    /* writing output */
  statSKIP,     /* skipping output */
};

enum {
  ovlEXIT,      /* fixed, predefined overlay index for the normal exit point */
  ovlNO_STATE,  /* fixed, predefined overlay index for invalid state functions */
  ovlEXITSTATE, /* fixed, predefined overlay index for dummy "exit" state functions */
  /* ----- */
  ovlFIRST,     /* overlay index of the first overlay function */
};

typedef struct s_stringlist {
  struct s_stringlist *next;
  char *line;
} stringlist;

typedef struct s_stringpair {
  struct s_stringpair *next;
  char *first;
  char *second;
  int matchlength;
} stringpair;

typedef struct s_valuepair {
  struct s_valuepair *next;
  long first;
  long second;
} valuepair;

/* macros for code generation */
#define opcodes(n)      ((n)*pc_cellsize)   /* opcode size */
#define opargs(n)       ((n)*pc_cellsize)   /* size of typical argument */

/* general purpose macros */
#if !defined sizearray
  #define sizearray(a)  (sizeof(a) / sizeof((a)[0]))
#endif
#if !defined makelong
  #define makelong(low,high) ((long)(low) | ((long)(high) << (sizeof(long)*4)))
#endif

/*  Tokens recognized by lex()
 *  Some of these constants are assigned as well to the variable "lastst" (see SC1.C)
 */
#define tFIRST      256 /* value of first multi-character operator */
#define tMIDDLE     280 /* value of last multi-character operator */
#define tLAST       321 /* value of last multi-character match-able token */
/* multi-character operators */
#define taMULT      256 /* *= */
#define taDIV       257 /* /= */
#define taMOD       258 /* %= */
#define taADD       259 /* += */
#define taSUB       260 /* -= */
#define taSHL       261 /* <<= */
#define taSHRU      262 /* >>>= */
#define taSHR       263 /* >>= */
#define taAND       264 /* &= */
#define taXOR       265 /* ^= */
#define taOR        266 /* |= */
#define tlOR        267 /* || */
#define tlAND       268 /* && */
#define tlEQ        269 /* == */
#define tlNE        270 /* != */
#define tlLE        271 /* <= */
#define tlGE        272 /* >= */
#define tSHL        273 /* << */
#define tSHRU       274 /* >>> */
#define tSHR        275 /* >> */
#define tINC        276 /* ++ */
#define tDEC        277 /* -- */
#define tELLIPS     278 /* ... */
#define tDBLDOT     279 /* .. */
#define tDBLCOLON   280 /* :: */
/* reserved words (statements) */
#define tASSERT     281
#define tBREAK      282
#define tCASE       283
#define tCONST      284
#define tCONTINUE   285
#define tDEFAULT    286
#define tDEFINED    287
#define tDO         288
#define tELSE       289
#define tEXIT       290
#define tFOR        291
#define tFORWARD    292
#define tGOTO       293
#define tIF         294
#define tNATIVE     295
#define tNEW        296
#define tOPERATOR   297
#define tPUBLIC     298
#define tRETURN     299
#define tSIZEOF     300
#define tSLEEP      301
#define tSTATE      302
#define tSTATIC     303
#define tSTOCK      304
#define tSWITCH     305
#define tTAGOF      306
#define tWHILE      307
/* compiler directives */
#define tpASSERT    308 /* #assert */
#define tpDEFINE    309
#define tpELSE      310 /* #else */
#define tpELSEIF    311 /* #elseif */
#define tpENDIF     312
#define tpENDINPUT  313
#define tpERROR     314
#define tpFILE      315
#define tpIF        316 /* #if */
#define tINCLUDE    317
#define tpLINE      318
#define tpPRAGMA    319
#define tpTRYINCLUDE 320
#define tpUNDEF     321
/* semicolon and comma are special cases, because they can be optional */
#define tTERM       322 /* semicolon or newline */
#define tSEPARATOR  323 /* comma or newline */
#define tENDEXPR    324 /* forced end of expression */
/* other recognized tokens */
#define tNUMBER     325 /* integer number */
#define tRATIONAL   326 /* rational number */
#define tSYMBOL     327
#define tLABEL      328
#define tSYMLABEL   329 /* ".name" syntax for named parameters and symbolic array indices */
#define tSTRING     330
#define tPACKSTRING 331
#define tEXPR       332 /* for assigment to "lastst" only (see SC1.C) */
#define tENDLESS    333 /* endless loop, for assigment to "lastst" only */

/* (reversed) evaluation of staging buffer */
#define sSTARTREORDER 0x01
#define sENDREORDER   0x02
#define sEXPRSTART    0x80      /* top bit set, rest is free */
#define sMAXARGS      127       /* relates to the bit pattern of sEXPRSTART */

#define sDOCSEP       0x01      /* to separate documentation comments between functions */

/* codes for ffabort() */
#define xEXIT           1       /* exit code in PRI */
#define xASSERTION      2       /* abort caused by failing assertion */
#define xSTACKERROR     3       /* stack/heap overflow */
#define xBOUNDSERROR    4       /* array index out of bounds */
#define xMEMACCESS      5       /* data access error */
#define xINVINSTR       6       /* invalid instruction */
#define xSTACKUNDERFLOW 7       /* stack underflow */
#define xHEAPUNDERFLOW  8       /* heap underflow */
#define xCALLBACKERR    9       /* no, or invalid, callback */
#define xSLEEP         12       /* sleep, exit code in PRI, tag in ALT */

/* Miscellaneous  */
#if !defined TRUE
  #define FALSE         0
  #define TRUE          1
#endif
#define sIN_CSEG        1       /* if parsing CODE */
#define sIN_DSEG        2       /* if parsing DATA */
#define sCHKBOUNDS      1       /* bit position in "debug" variable: check bounds */
#define sSYMBOLIC       2       /* bit position in "debug" variable: symbolic info */
#define sRESET          0       /* reset error flag */
#define sFORCESET       1       /* force error flag on */
#define sEXPRMARK       2       /* mark start of expression */
#define sEXPRRELEASE    3       /* mark end of expression */
#define sSETLINE        4       /* set line number for the error */
#define sSETFILE        5       /* set file number for the error */

enum {
  sOPTIMIZE_NONE,               /* no optimization */
  sOPTIMIZE_CORE,               /* optimize using core instruction set only */
  sOPTIMIZE_MACRO,              /* core + supplemental instruction sets */
  sOPTIMIZE_FULL,               /* full optimization, packed instructions */
  /* ----- */
  sOPTIMIZE_NUMBER
};

typedef enum s_regid {
  sPRI,                         /* indicates the primary register */
  sALT,                         /* indicates the secundary register */
} regid;

typedef enum s_optmark {
  sEXPR,                        /* end of expression (for expressions that form a statement) */
  sPARM,                        /* end of parameter (in a function parameter list) */
  sLDECL,                       /* start of local declaration (variable) */
} optmark;

#define suSLEEP_INSTR 0x01      /* the "sleep" instruction was used */

#if INT_MAX<0x8000u
  #define PUBLICTAG   0x8000u
  #define FIXEDTAG    0x4000u
#else
  #define PUBLICTAG   0x80000000Lu
  #define FIXEDTAG    0x40000000Lu
#endif
#define TAGMASK       (~PUBLICTAG)
#define CELL_MAX      (((ucell)1 << (sizeof(cell)*8-1)) - 1)


/* interface functions */
#if defined __cplusplus
  extern "C" {
#endif
#if !defined DLLEXPORT
  #define DLLEXPORT
#endif

/*
 * Functions you call from the "driver" program
 */
DLLEXPORT int pc_compile(int argc, char **argv);
DLLEXPORT int pc_addconstant(const char *name,cell value,int tag);
DLLEXPORT int pc_addtag(const char *name);
DLLEXPORT int pc_enablewarning(int number,int enable);

/*
 * Functions called from the compiler (to be implemented by you)
 */

/* general console output */
int pc_printf(const char *message,...);

/* error report function */
int pc_error(int number,char *message,char *filename,int firstline,int lastline,va_list argptr);

/* input from source file */
void *pc_opensrc(char *filename); /* reading only */
void *pc_createsrc(char *filename);
void pc_closesrc(void *handle);   /* never delete */
char *pc_readsrc(void *handle,unsigned char *target,int maxchars);
int pc_writesrc(void *handle,const unsigned char *source);
void pc_clearpossrc(void);                        /* clear file position marks */
void *pc_getpossrc(void *handle,void *position);  /* mark the current position */
void pc_resetsrc(void *handle,void *position);    /* reset to a position marked earlier */
int  pc_eofsrc(void *handle);

/* output to intermediate (.ASM) file */
void *pc_openasm(char *filename); /* read/write */
void pc_closeasm(void *handle,int deletefile);
void pc_resetasm(void *handle);
int  pc_writeasm(void *handle,const char *str);
char *pc_readasm(void *handle,char *target,int maxchars);

/* output to binary (.AMX) file */
void *pc_openbin(char *filename);
void pc_closebin(void *handle,int deletefile);
void pc_resetbin(void *handle,long offset);
int  pc_writebin(void *handle,const void *buffer,int size);
long pc_lengthbin(void *handle); /* return the length of the file */

#if defined __cplusplus
  }
#endif


/* by default, functions and variables used in throughout the compiler
 * files are "external"
 */
#if !defined SC_FUNC
  #define SC_FUNC
#endif
#if !defined SC_VDECL
  #define SC_VDECL  extern
#endif
#if !defined SC_VDEFINE
  #define SC_VDEFINE
#endif

/* function prototypes in SC1.C */
SC_FUNC void set_extension(char *filename,char *extension,int force);
SC_FUNC symbol *fetchfunc(char *name,int tag);
SC_FUNC char *operator_symname(char *symname,char *opername,int tag1,int tag2,int numtags,int resulttag);
SC_FUNC char *funcdisplayname(char *dest,const char *funcname);
SC_FUNC int constexpr(cell *val,int *tag,symbol **symptr);
SC_FUNC constvalue *append_constval(constvalue *table,const char *name,cell val,int index);
SC_FUNC constvalue *find_constval(constvalue *table,char *name,int index);
SC_FUNC void delete_consttable(constvalue *table);
SC_FUNC int compare_consttable(constvalue *table1, constvalue *table2);
SC_FUNC symbol *add_constant(const char *name,cell val,int vclass,int tag);
SC_FUNC void exporttag(int tag);
SC_FUNC void sc_attachdocumentation(symbol *sym,int onlylastblock);

/* function prototypes in SC2.C */
#define PUSHSTK_P(v)  { stkitem s_; s_.pv=(v); pushstk(s_); }
#define PUSHSTK_I(v)  { stkitem s_; s_.i=(v); pushstk(s_); }
#define POPSTK_P()    (popstk().pv)
#define POPSTK_I()    (popstk().i)
SC_FUNC void pushstk(stkitem val);
SC_FUNC stkitem popstk(void);
SC_FUNC void clearstk(void);
SC_FUNC int plungequalifiedfile(char *name);  /* explicit path included */
SC_FUNC int plungefile(char *name,int try_currentpath,int try_includepaths);   /* search through "include" paths */
SC_FUNC char *strdel(char *str,size_t len);
SC_FUNC char *strins(char *dest,char *src,size_t srclen);
SC_FUNC void preprocess(void);
SC_FUNC void lex_fetchindent(const unsigned char *string,const unsigned char *pos);
SC_FUNC int lex_adjusttabsize(int matchindent);
SC_FUNC int lexinit(int releaseall);
SC_FUNC int lex(cell *lexvalue,char **lexsym);
SC_FUNC void lexpush(void);
SC_FUNC int lexsettoken(int token,char *str);
SC_FUNC void lexclr(int clreol);
SC_FUNC int lexpeek(void);
SC_FUNC int matchtoken(int token);
SC_FUNC int tokeninfo(cell *val,char **str);
SC_FUNC int needtoken(int token);
SC_FUNC void litadd(cell value);
SC_FUNC void litinsert(cell value,int pos);
SC_FUNC int alphanum(unsigned char c);
SC_FUNC int ishex(char c);
SC_FUNC void delete_symbol(symbol *root,symbol *sym);
SC_FUNC void delete_symbols(symbol *root,int level,int del_labels,int delete_functions);
SC_FUNC int refer_symbol(symbol *entry,symbol *bywhom);
SC_FUNC void markusage(symbol *sym,int usage);
SC_FUNC uint32_t namehash(const char *name);
SC_FUNC symbol *findglb(const char *name,int filter);
SC_FUNC symbol *findloc(const char *name);
SC_FUNC symbol *findconst(const char *name);
SC_FUNC symbol *finddepend(const symbol *parent);
SC_FUNC symbol *addsym(const char *name,cell addr,int ident,int vclass,int tag,
                       int usage);
SC_FUNC symbol *addvariable(const char *name,cell addr,int ident,int vclass,int tag,
                            int dim[],constvalue *dimnames[],int numdim,int ispacked);
SC_FUNC int getlabel(void);
SC_FUNC char *itoh(ucell val);

/* function prototypes in SC3.C */
SC_FUNC int check_userop(void (*oper)(void),int tag1,int tag2,int numparam,
                         value *lval,int *resulttag);
SC_FUNC int matchtag(int formaltag,int actualtag,int allowcoerce);
SC_FUNC int expression(cell *val,int *tag,symbol **symptr,int chkfuncresult);
SC_FUNC int sc_getstateid(constvalue **automaton,constvalue **state,char *statename);
SC_FUNC cell array_totalsize(symbol *sym);

/* function prototypes in SC4.C */
SC_FUNC void writeleader(symbol *root,int *lbl_nostate,int *lbl_ignorestate);
SC_FUNC void writetrailer(void);
SC_FUNC void writestatetables(symbol *root,int lbl_nostate,int lbl_ignorestate);
SC_FUNC void begcseg(void);
SC_FUNC void begdseg(void);
SC_FUNC void setline(int chkbounds);
SC_FUNC void setfiledirect(char *name);
SC_FUNC void setlinedirect(int line);
SC_FUNC void setlabel(int index);
SC_FUNC void markexpr(optmark type,const char *name,cell offset);
SC_FUNC void startfunc(const char *fname,int index);
SC_FUNC void endfunc(void);
SC_FUNC void alignframe(int numbytes);
SC_FUNC void rvalue(value *lval);
SC_FUNC void address(symbol *ptr,regid reg);
SC_FUNC void store(value *lval);
SC_FUNC void loadreg(cell address,regid reg);
SC_FUNC void storereg(cell address,regid reg);
SC_FUNC void memcopy(cell size);
SC_FUNC void copyarray2d(int majordim,int minordim);
SC_FUNC void fillarray(symbol *sym,cell size,cell value);
SC_FUNC void ldconst(cell val,regid reg);
SC_FUNC void swapregs(void);
SC_FUNC void pushreg(regid reg);
SC_FUNC void pushreloc(void);
SC_FUNC void pushval(cell val);
SC_FUNC void popreg(regid reg);
SC_FUNC void swap1(void);
SC_FUNC void ffswitch(int label,int iswitch);
SC_FUNC void ffcase(cell value,int label,int newtable,int icase);
SC_FUNC void ffcall(symbol *sym,const char *label,int numargs);
SC_FUNC void ffret(int remparams);
SC_FUNC void ffabort(int reason);
SC_FUNC void ffbounds(cell size);
SC_FUNC void jumplabel(int number);
SC_FUNC void defstorage(void);
SC_FUNC void modstk(int delta);
SC_FUNC void setstk(cell value);
SC_FUNC void modheap(int delta);
SC_FUNC void setheap_pri(void);
SC_FUNC void setheap(cell value);
SC_FUNC void cell2addr(void);
SC_FUNC void cell2addr_alt(void);
SC_FUNC void addr2cell(void);
SC_FUNC void char2addr(void);
SC_FUNC void charalign(void);
SC_FUNC void addconst(cell value);

/*  Code generation functions for arithmetic operators.
 *
 *  Syntax: o[u|s|b]_name
 *          |   |   | +--- name of operator
 *          |   |   +----- underscore
 *          |   +--------- "u"nsigned operator, "s"igned operator or "b"oth
 *          +------------- "o"perator
 */
SC_FUNC void os_mult(void); /* multiplication (signed) */
SC_FUNC void os_div(void);  /* division (signed) */
SC_FUNC void os_mod(void);  /* modulus (signed) */
SC_FUNC void ob_add(void);  /* addition */
SC_FUNC void ob_sub(void);  /* subtraction */
SC_FUNC void ob_sal(void);  /* shift left (arithmetic) */
SC_FUNC void os_sar(void);  /* shift right (arithmetic, signed) */
SC_FUNC void ou_sar(void);  /* shift right (logical, unsigned) */
SC_FUNC void ob_or(void);   /* bitwise or */
SC_FUNC void ob_xor(void);  /* bitwise xor */
SC_FUNC void ob_and(void);  /* bitwise and */
SC_FUNC void ob_eq(void);   /* equality */
SC_FUNC void ob_ne(void);   /* inequality */
SC_FUNC void relop_prefix(void);
SC_FUNC void relop_suffix(void);
SC_FUNC void os_le(void);   /* less or equal (signed) */
SC_FUNC void os_ge(void);   /* greater or equal (signed) */
SC_FUNC void os_lt(void);   /* less (signed) */
SC_FUNC void os_gt(void);   /* greater (signed) */

SC_FUNC void oa_eq(cell size);  /* equal, array operator */
SC_FUNC void oa_ne(cell size);  /* not equal, array operator */

SC_FUNC void lneg(void);
SC_FUNC void neg(void);
SC_FUNC void invert(void);
SC_FUNC void nooperation(void);
SC_FUNC void inc(value *lval);
SC_FUNC void dec(value *lval);
SC_FUNC void jmp_ne0(int number);
SC_FUNC void jmp_eq0(int number);
SC_FUNC void outval(cell val,int fullcell,int newline);

/* function prototypes in SC5.C */
SC_FUNC int error(long number,...);
SC_FUNC int error_suggest(int error,const char *name,int ident);
SC_FUNC int error_suggest_list(int number,const char *name,constvalue *list);
SC_FUNC void errorset(int code,int line);
#define MAX_EDIT_DIST 2 /* allow two mis-typed characters; when there are more,
                         * the names are too different, and no match is returned */
SC_FUNC int levenshtein_distance(const char *s,const char*t);
SC_FUNC symbol *find_closestsymbol(const char *name,int symboltype);

/* function prototypes in SC6.C */
SC_FUNC ucell getparamvalue(const char *s,const char **n);
SC_FUNC int assemble(FILE *fout,FILE *fin);

/* function prototypes in SC7.C */
SC_FUNC ucell hex2ucell(const char *s,const char **n);
#define hex2cell(s,n)  (cell)hex2ucell((s),(n))
SC_FUNC void stgbuffer_cleanup(void);
SC_FUNC void stgmark(char mark);
SC_FUNC void stgwrite(const char *st);
SC_FUNC void stgout(int index);
SC_FUNC void stgdel(int index,cell code_index);
SC_FUNC int stgget(int *index,cell *code_index);
SC_FUNC void stgset(int onoff);
SC_FUNC int phopt_init(void);
SC_FUNC int phopt_cleanup(void);

/* function prototypes in SCLIST.C */
SC_FUNC char* duplicatestring(const char* sourcestring);
SC_FUNC stringpair *insert_alias(char *name,char *alias);
SC_FUNC stringpair *find_alias(char *name);
SC_FUNC int lookup_alias(char *target,char *name);
SC_FUNC void delete_aliastable(void);
SC_FUNC stringlist *insert_path(char *path);
SC_FUNC char *get_path(int index);
SC_FUNC void delete_pathtable(void);
SC_FUNC stringpair *insert_subst(char *pattern,char *substitution,int prefixlen);
SC_FUNC int get_subst(int index,char **pattern,char **substitution);
SC_FUNC stringpair *find_subst(char *name,int length);
SC_FUNC int delete_subst(char *name,int length);
SC_FUNC void delete_substtable(void);
SC_FUNC stringlist *insert_sourcefile(char *string);
SC_FUNC char *get_sourcefile(int index);
SC_FUNC void delete_sourcefiletable(void);
SC_FUNC stringlist *insert_inputfile(char *string);
SC_FUNC char *get_inputfile(int index);
SC_FUNC void delete_inputfiletable(void);
SC_FUNC stringlist *insert_docstring(char *string,int append);
SC_FUNC char *get_docstring(int index);
SC_FUNC void delete_docstring(int index);
SC_FUNC void delete_docstringtable(void);
SC_FUNC stringlist *insert_autolist(char *string);
SC_FUNC char *get_autolist(int index);
SC_FUNC void delete_autolisttable(void);
SC_FUNC valuepair *push_heaplist(long first, long second);
SC_FUNC int popfront_heaplist(long *first, long *second);
SC_FUNC void delete_heaplisttable(void);
SC_FUNC stringlist *insert_dbgfile(const char *filename);
SC_FUNC stringlist *insert_dbgline(int linenr);
SC_FUNC stringlist *insert_dbgsymbol(symbol *sym);
SC_FUNC char *get_dbgstring(int index);
SC_FUNC void delete_dbgstringtable(void);

/* function prototypes in SCMEMFILE.C */
#include "memfile.h"
SC_FUNC memfile_t *mfcreate(const char *filename);
SC_FUNC void mfclose(memfile_t *mf);
SC_FUNC int mfdump(memfile_t *mf);
SC_FUNC size_t mflength(const memfile_t *mf);
SC_FUNC size_t mfseek(memfile_t *mf,long offset,int whence);
SC_FUNC size_t mfwrite(memfile_t *mf,const unsigned char *buffer,size_t size);
SC_FUNC size_t mfread(memfile_t *mf,unsigned char *buffer,size_t size);
SC_FUNC char *mfgets(memfile_t *mf,char *string,size_t size);
SC_FUNC int mfputs(memfile_t *mf,const char *string);

/* function prototypes in SCI18N.C */
#define MAXCODEPAGE 12
SC_FUNC int cp_path(const char *root,const char *directory);
SC_FUNC int cp_set(const char *name);
SC_FUNC cell cp_translate(const unsigned char *string,const unsigned char **endptr);
SC_FUNC cell get_utf8_char(const unsigned char *string,const unsigned char **endptr);
SC_FUNC int scan_utf8(FILE *fp,const char *filename);

/* function prototypes in SCSTATE.C */
SC_FUNC constvalue *automaton_add(const char *name);
SC_FUNC constvalue *automaton_find(const char *name,char *closestmatch);
SC_FUNC constvalue *automaton_findid(int id);
SC_FUNC constvalue *state_add(const char *name,int fsa_id);
SC_FUNC constvalue *state_find(const char *name,int fsa_id,char *closestmatch);
SC_FUNC constvalue *state_findid(int id);
SC_FUNC void state_buildlist(int **list,int *listsize,int *count,int stateid);
SC_FUNC int state_addlist(int *list,int count,int fsa_id);
SC_FUNC void state_deletetable(void);
SC_FUNC int state_getfsa(int listid);
SC_FUNC int state_count(int listid);
SC_FUNC int state_inlist(int listid,int state);
SC_FUNC int state_listitem(int listid,int index);
SC_FUNC void state_conflict(symbol *root);
SC_FUNC int state_conflict_id(int listid1,int listid2);
SC_FUNC statelist *append_statelist(statelist *root,int id,int label,cell address);
SC_FUNC void delete_statelisttable(statelist *root);

/* external variables (defined in scvars.c) */
#if !defined SC_SKIP_VDECL
SC_VDECL symbol loctab;       /* local symbol table */
SC_VDECL symbol glbtab;       /* global symbol table */
SC_VDECL cell *litq;          /* the literal queue */
SC_VDECL unsigned char *srcline;/* the line read from the input file */
SC_VDECL const unsigned char *lptr;/* points to the current position in "srcline" */
SC_VDECL constvalue tagname_tab;/* tagname table */
SC_VDECL constvalue libname_tab;/* library table (#pragma library "..." syntax) */
SC_VDECL constvalue *curlibrary;/* current library */
SC_VDECL int pc_addlibtable;  /* is the library table added to the AMX file? */
SC_VDECL constvalue ntvindex_tab;/* native function index table */
SC_VDECL symbol *curfunc;     /* pointer to current function */
SC_VDECL char *inpfname;      /* name of the file currently read from */
SC_VDECL char outfname[];     /* intermediate (assembler) file name */
SC_VDECL char binfname[];     /* binary file name */
SC_VDECL char errfname[];     /* error file name */
SC_VDECL char sc_ctrlchar;    /* the control character (or escape character) */
SC_VDECL char sc_ctrlchar_org;/* the default control character */
SC_VDECL int litidx;          /* index to literal table */
SC_VDECL int litmax;          /* current size of the literal table */
SC_VDECL int stgidx;          /* index to the staging buffer */
SC_VDECL int sc_labnum;       /* number of (internal) labels */
SC_VDECL int staging;         /* true if staging output */
SC_VDECL cell declared;       /* number of local cells declared */
SC_VDECL cell glb_declared;   /* number of global cells declared */
SC_VDECL cell code_idx;       /* number of bytes with generated code */
SC_VDECL int ntv_funcid;      /* incremental number of native function */
SC_VDECL int errnum;          /* number of errors */
SC_VDECL int warnnum;         /* number of warnings */
SC_VDECL int sc_debug;        /* debug/optimization options (bit field) */
SC_VDECL int sc_asmfile;      /* create .ASM file? */
SC_VDECL int sc_listing;      /* create .LST file? */
SC_VDECL int sc_needsemicolon;/* semicolon required to terminate expressions? */
SC_VDECL int sc_dataalign;    /* data alignment value */
SC_VDECL int sc_alignnext;    /* must frame of the next function be aligned? */
SC_VDECL int pc_docexpr;      /* must expression be attached to documentation comment? */
SC_VDECL int curseg;          /* 1 if currently parsing CODE, 2 if parsing DATA */
SC_VDECL cell pc_stksize;     /* stack size */
SC_VDECL cell pc_amxlimit;    /* abstract machine size limit (code + data, or only code) */
SC_VDECL cell pc_amxram;      /* abstract machine data size limit */
SC_VDECL int freading;        /* is there an input file ready for reading? */
SC_VDECL int fline;           /* the line number in the current file */
SC_VDECL short fnumber;       /* number of files in the input file table */
SC_VDECL short fcurrent;      /* current file being processed */
SC_VDECL short sc_intest;     /* true if inside a test */
SC_VDECL int pc_sideeffect;   /* true if an expression causes a side-effect */
SC_VDECL int pc_stmtindent;   /* current indent of the statement */
SC_VDECL int indent_nowarn;   /* skip warning "217 loose indentation" */
SC_VDECL int pc_tabsize;      /* number of spaces that a TAB represents */
SC_VDECL int pc_matchedtabsize;/* if no tabsize explicitly set, try to detect the tab size */
SC_VDECL short sc_allowtags;  /* allow/detect tagnames in lex() */
SC_VDECL int sc_status;       /* read/write status */
SC_VDECL int sc_rationaltag;  /* tag for rational numbers */
SC_VDECL int rational_digits; /* number of fractional digits */
SC_VDECL int sc_allowproccall;/* allow/detect tagnames in lex() */
SC_VDECL short sc_is_utf8;    /* is this source file in UTF-8 encoding */
SC_VDECL char *pc_deprecate;  /* if non-NULL, mark next declaration as deprecated */
SC_VDECL int sc_curstates;    /* ID of the current state list */
SC_VDECL int pc_optimize;     /* (peephole) optimization level */
SC_VDECL int pc_memflags;     /* special flags for the stack/heap usage */
SC_VDECL int pc_overlays;     /* generate overlay table + instructions? (abstract machine overay size limit) */
SC_VDECL int pc_ovl0size[][2];/* size (in bytes) of the first (special) overlays */
SC_VDECL int pc_cellsize;     /* size (in bytes) of a cell */
SC_VDECL uint64_t pc_cryptkey;/* key for encryption of the generated script */

SC_VDECL constvalue sc_automaton_tab; /* automaton table */
SC_VDECL constvalue sc_state_tab;     /* state table */

SC_VDECL FILE *inpf;          /* file read from (source or include) */
SC_VDECL FILE *inpf_org;      /* main source file */
SC_VDECL FILE *outf;          /* file written to */

SC_VDECL jmp_buf errbuf;      /* target of longjmp() on a fatal error */

#if !defined PAWN_LIGHT
  SC_VDECL int sc_makereport; /* generate a cross-reference report */
#endif

#endif /* SC_SKIP_VDECL */

#endif /* SC_H_INCLUDED */
