/*  Pawn compiler
 *
 *  Pawn is a scripting language system consisting of a compiler and an
 *  abstract machine, for building and running programs in the Pawn language.
 *
 *  The Pawn compiler has its origins in the Small-C compiler Version 2.01,
 *  originally created by Ron Cain, july 1980, and enhanced by James E. Hendrix.
 *  The modifications in Pawn come close to a complete rewrite, though.
 *
 *  Copyright ITB CompuPhase, 1997-2012
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
 *  Version: $Id: sc1.c 4769 2012-08-31 12:21:02Z  $
 */
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined __WIN32__ || defined _WIN32 || defined __MSDOS__
  #include <conio.h>
  #include <io.h>
#endif

#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

#if defined __BORLANDC__ || defined __WATCOMC__
  #include <dos.h>
  static unsigned total_drives; /* dummy variable */
  #define dos_setdrive(i)       _dos_setdrive(i,&total_drives)
#elif defined _MSC_VER && defined _WIN32
  #include <direct.h>           /* for _chdrive() */
  #define dos_setdrive(i)       _chdrive(i)
#endif
#if defined __BORLANDC__
  #include <dir.h>              /* for chdir() */
#elif defined __WATCOMC__
  #include <direct.h>           /* for chdir() */
#endif
#if defined __WIN32__ || defined _WIN32 || defined _Windows
  #include <windows.h>
#endif

#if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined __NT__
  #define DLLEXPORT __declspec (dllexport)
#endif

#include "lstring.h"
#include "sc.h"
#if defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
  #include <sclinux.h>
  #include <binreloc.h> /* from BinReloc, see www.autopackage.org */
#endif

#include "svnrev.h"
#define VERSION_STR "4.0." SVN_REVSTR
#define VERSION_INT 0x0400


static void resetglobals(void);
static void initglobals(void);
static char *get_extension(char *filename);
static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *rname,char *codepage);
static void setconfig(char *root);
static void setcaption(void);
static void about(void);
static void setconstants(void);
static void plungeprefix(char *prefixname);
static void parse(void);
static void dumplits(void);
static void dumpzero(int count);
static void declfuncvar(int fpublic,int fstatic,int fstock,int fconst);
static void declglb(char *firstname,int firsttag,int fpublic,int fstatic,
                    int stock,int fconst);
static int declloc(int fstatic);
static void decl_const(int table);
static cell needsub(char match,constvalue **namelist);
static void verify_array_namelist(constvalue *namelist[], int numdim);
static void initials(int ident,int usage,int tag,cell *size,int dim[],int numdim,
                     constvalue *namelist[]);
static cell initarray(int ident,int usage,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue *lastdim,
                      constvalue *namelist[],int *errorfound);
static cell initvector(int ident,int usage,int tag,cell size,int fillzero,
                       constvalue *namelist,int *errorfound);
static cell init(int ident,int usage,int *tag,int *errorfound,int *packcount,cell *packitem);
static int getstates(const char *funcname);
static statelist *attachstatelist(symbol *sym,int state_id);
static void funcstub(int fnative);
static int newfunc(char *firstname,int firsttag,int fpublic,int fstatic,int stock);
static int declargs(symbol *sym,int chkshadow);
static void doarg(char *name,int ident,int offset,int tags[],int numtags,
                  int fpublic,int fconst,int chkshadow,arginfo *arg);
static void make_report(symbol *root,FILE *log,char *sourcefile);
static void reduce_referrers(symbol *root);
static void gen_ovlinfo(symbol *root);
static long max_stacksize(symbol *root,int *recursion);
static long max_overlaysize(symbol *root,char **funcname);
static int checkundefined(symbol *root);
static int testsymbols(symbol *root,int level,int testlabs,int testconst);
static void destructsymbols(symbol *root,int level);
static constvalue *find_constval_byval(constvalue *table,cell val);
static constvalue *clone_consttable(const constvalue *table);
static symbol *fetchlab(char *name);
static void statement(int *lastindent,int allow_decl);
static void compound(int stmt_sameline);
static int test(int label,int parens,int invert);
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult);
static void doassert(void);
static void doexit(void);
static int doif(void);
static int dowhile(void);
static int dodo(void);
static int dofor(void);
static void doswitch(void);
static void dogoto(void);
static void dolabel(void);
static void doreturn(void);
static void dobreak(void);
static void docont(void);
static void dosleep(void);
static void dostate(void);
static void addwhile(int *ptr);
static void delwhile(void);
static int *readwhile(void);

static int lastst     =0;       /* last executed statement type */
static int nestlevel  =0;       /* number of active (open) compound statements */
static int endlessloop=0;       /* nesting level of endless loop */
static int rettype    =0;       /* the type that a "return" expression should have */
static int skipinput  =0;       /* number of lines to skip from the first input file */
static int optproccall=TRUE;    /* support "procedure call" */
static int verbosity  =1;       /* verbosity level, 0=quiet, 1=normal, 2=verbose */
static int sc_reparse =0;       /* needs 3th parse because of changed prototypes? */
static int sc_parsenum=0;       /* number of the extra parses */
static int undefined_vars=FALSE;/* if TRUE, undefined symbols were found */
static int pc_enumsequence=0;   /* sequence number of enumerated constant lists, for reporting these lists */
static int wq[wqTABSZ];         /* "while queue", internal stack for nested loops */
static int *wqptr;              /* pointer to next entry */
#if !defined PAWN_LIGHT
  static char sc_rootpath[_MAX_PATH]; /* base path of the installation */
  static char sc_binpath[_MAX_PATH];  /* path for the binaries, often sc_rootpath + /bin */
  static char *pc_globaldoc=NULL;/* main documentation */
  static char *pc_recentdoc=NULL;/* documentation from the most recent comment block */
  int pc_docstring_suspended=FALSE;
#endif

#if !defined NO_MAIN

#if defined __TURBOC__ && !defined __32BIT__
  extern unsigned int _stklen = 0x2000;
#endif

int main(int argc, char *argv[])
{
  return pc_compile(argc,argv);
}

/* pc_printf()
 * Called for general purpose "console" output. This function prints general
 * purpose messages; errors go through pc_error(). The function is modelled
 * after printf().
 */
int pc_printf(const char *message,...)
{
  int ret;
  va_list argptr;

  va_start(argptr,message);
  ret=vprintf(message,argptr);
  va_end(argptr);
  fflush(stdout);

  return ret;
}

/* pc_error()
 * Called for producing error output.
 *    number      the error number (as documented in the manual)
 *    message     a string describing the error with embedded %d and %s tokens
 *    filename    the name of the file currently being parsed
 *    firstline   the line number at which the expression started on which
 *                the error was found, or -1 if there is no "starting line"
 *    lastline    the line number at which the error was detected
 *    argptr      a pointer to the first of a series of arguments (for macro
 *                "va_arg")
 * Return:
 *    If the function returns 0, the parser attempts to continue compilation.
 *    On a non-zero return value, the parser aborts.
 */
int pc_error(int number,char *message,char *filename,int firstline,int lastline,va_list argptr)
{
static char *prefix[3]={ "error", "fatal error", "warning" };

  if (number!=0) {
    char *pre;

    pre=prefix[number/100];
    if (firstline>=0)
      fprintf(stderr,"%s(%d -- %d) : %s %03d: ",filename,firstline,lastline,pre,number);
    else
      fprintf(stderr,"%s(%d) : %s %03d: ",filename,lastline,pre,number);
  } /* if */
  vfprintf(stderr,message,argptr);
  fflush(stderr);
  return 0;
}

/* pc_opensrc()
 * Opens a source file (or include file) for reading. The "file" does not have
 * to be a physical file, one might compile from memory.
 *    filename    the name of the "file" to read from
 * Return:
 *    The function must return a pointer, which is used as a "magic cookie" to
 *    all I/O functions. When failing to open the file for reading, the
 *    function must return NULL.
 * Note:
 *    Several "source files" may be open at the same time. Specifically, one
 *    file can be open for reading and another for writing.
 */
void *pc_opensrc(char *filename)
{
  return fopen(filename,"r");
}

/* pc_createsrc()
 * Creates/overwrites a source file for writing. The "file" does not have
 * to be a physical file, one might compile from memory.
 *    filename    the name of the "file" to create
 * Return:
 *    The function must return a pointer, which is used as a "magic cookie" to
 *    all I/O functions. When failing to open the file for reading, the
 *    function must return NULL.
 * Note:
 *    Several "source files" may be open at the same time. Specifically, one
 *    file can be open for reading and another for writing.
 */
void *pc_createsrc(char *filename)
{
  return fopen(filename,"w");
}

/* pc_closesrc()
 * Closes a source file (or include file). The "handle" parameter has the
 * value that pc_opensrc() returned in an earlier call.
 */
void pc_closesrc(void *handle)
{
  assert(handle!=NULL);
  fclose((FILE*)handle);
}

/* pc_readsrc()
 * Reads a single line from the source file (or up to a maximum number of
 * characters if the line in the input file is too long).
 */
char *pc_readsrc(void *handle,unsigned char *target,int maxchars)
{
  return fgets((char*)target,maxchars,(FILE*)handle);
}

/* pc_writesrc()
 * Writes to to the source file. There is no automatic line ending; to end a
 * line, write a "\n".
 */
int pc_writesrc(void *handle,const unsigned char *source)
{
  return fputs((char*)source,(FILE*)handle) >= 0;
}

#define MAXPOSITIONS  4
static fpos_t srcpositions[MAXPOSITIONS];
static unsigned char srcposalloc[MAXPOSITIONS];

void pc_clearpossrc(void)
{
  memset(srcpositions,0,sizeof srcpositions);
  memset(srcposalloc,0,sizeof srcposalloc);
}

void *pc_getpossrc(void *handle,void *position)
{
  if (position==NULL) {
    /* allocate a new slot */
    int i;
    for (i=0; i<MAXPOSITIONS && srcposalloc[i]!=0; i++)
      /* nothing */;
    assert(i<MAXPOSITIONS); /* if not, there is a queue overrun */
    if (i>=MAXPOSITIONS)
      return NULL;
    position=&srcpositions[i];
    srcposalloc[i]=1;
  } else {
    /* use the gived slot */
    assert(position>=(void*)srcpositions && position<(void*)((char*)srcpositions+sizeof(srcpositions)));
  } /* if */
  fgetpos((FILE*)handle,(fpos_t*)position);
  return position;
}

/* pc_resetsrc()
 * "position" may only hold a pointer that was previously obtained from
 * pc_getpossrc()
 */
void pc_resetsrc(void *handle,void *position)
{
  assert(handle!=NULL);
  assert(position!=NULL);
  fsetpos((FILE*)handle,(fpos_t*)position);
  /* note: the item is not cleared from the pool */
}

int pc_eofsrc(void *handle)
{
  return feof((FILE*)handle);
}

/* should return a pointer, which is used as a "magic cookie" to all I/O
 * functions; return NULL for failure
 */
void *pc_openasm(char *filename)
{
  #if defined __MSDOS__ || defined PAWN_LIGHT
    return fopen(filename,"w+");
  #else
    return mfcreate(filename);
  #endif
}

void pc_closeasm(void *handle, int deletefile)
{
  #if defined __MSDOS__ || defined PAWN_LIGHT
    if (handle!=NULL)
      fclose((FILE*)handle);
    if (deletefile)
      remove(outfname);
  #else
    if (handle!=NULL) {
      if (!deletefile)
        mfdump((memfile_t*)handle);
      mfclose((memfile_t*)handle);
    } /* if */
  #endif
}

void pc_resetasm(void *handle)
{
  assert(handle!=NULL);
  #if defined __MSDOS__ || defined PAWN_LIGHT
    fflush((FILE*)handle);
    fseek((FILE*)handle,0,SEEK_SET);
  #else
    mfseek((memfile_t*)handle,0,SEEK_SET);
  #endif
}

int pc_writeasm(void *handle,const char *string)
{
  #if defined __MSDOS__ || defined PAWN_LIGHT
    return fputs(string,(FILE*)handle) >= 0;
  #else
    return mfputs((memfile_t*)handle,string);
  #endif
}

char *pc_readasm(void *handle, char *string, int maxchars)
{
  #if defined __MSDOS__ || defined PAWN_LIGHT
    return fgets(string,maxchars,(FILE*)handle);
  #else
    return mfgets((memfile_t*)handle,string,maxchars);
  #endif
}

/* Should return a pointer, which is used as a "magic cookie" to all I/O
 * functions; return NULL for failure.
 */
void *pc_openbin(char *filename)
{
  return fopen(filename,"wb");
}

void pc_closebin(void *handle,int deletefile)
{
  fclose((FILE*)handle);
  if (deletefile)
    remove(binfname);
}

/* pc_resetbin()
 * Can seek to any location in the file.
 * The offset is always from the start of the file.
 */
void pc_resetbin(void *handle,long offset)
{
  fflush((FILE*)handle);
  fseek((FILE*)handle,offset,SEEK_SET);
}

int pc_writebin(void *handle,const void *buffer,int size)
{
  return (int)fwrite(buffer,1,size,(FILE*)handle) == size;
}

long pc_lengthbin(void *handle)
{
  return ftell((FILE*)handle);
}

#endif  /* !defined NO_MAIN */


/*  "main" of the compiler
 */
#if defined __cplusplus
  extern "C"
#endif
DLLEXPORT
int pc_compile(int argc, char *argv[])
{
  int entry,i,jmpcode;
  int retcode,lbl_nostate,lbl_exitstate;
  char incfname[_MAX_PATH];
  char reportname[_MAX_PATH];
  char codepage[MAXCODEPAGE+1];
  char *tmpname;  /* temporary input file name */
  FILE *binf;
  void *inpfmark;
  int lcl_needsemicolon,lcl_tabsize;
  #if !defined PAWN_LIGHT
    int hdrsize=0;
  #endif
  char *ptr;

  /* set global variables to their initial value */
  binf=NULL;
  tmpname=NULL;
  initglobals();
  errorset(sRESET,0);
  errorset(sEXPRRELEASE,0);
  if (!lexinit(FALSE))
    error(103);         /* insufficient memory */
  pc_clearpossrc();

  /* make sure that we clean up on a fatal error; do this before the first
   * call to error(). */
  if ((jmpcode=setjmp(errbuf))!=0)
    goto cleanup;

  /* allocate memory for fixed tables */
  inpfname=(char*)malloc(_MAX_PATH*sizeof(char));
  if (inpfname==NULL)
    error(103);         /* insufficient memory */
  *inpfname='\0';
  litq=(cell*)malloc(litmax*sizeof(cell));
  if (litq==NULL)
    error(103);         /* insufficient memory */
  if (!phopt_init())
    error(103);         /* insufficient memory */

  setconfig(argv[0]);   /* the path to the include and codepage files, plus the root path */
  setopt(argc,argv,outfname,errfname,incfname,reportname,codepage);
  strcpy(binfname,outfname);
  ptr=get_extension(binfname);
  if (ptr!=NULL && stricmp(ptr,".asm")==0)
    set_extension(binfname,".amx",TRUE);
  else
    set_extension(binfname,".amx",FALSE);
  /* set output names that depend on the input name */
  if (sc_listing)
    set_extension(outfname,".lst",TRUE);
  else
    set_extension(outfname,".asm",TRUE);
  if (strlen(errfname)!=0)
    remove(errfname);   /* delete file on startup */
  else if (verbosity>0)
    setcaption();
  sc_ctrlchar_org=sc_ctrlchar;
  lcl_needsemicolon=sc_needsemicolon;
  lcl_tabsize=pc_tabsize;
  #if !defined PAWN_NO_CODEPAGE
    if (!cp_set(codepage))      /* set codepage */
      error(108);               /* codepage mapping file not found */
  #endif
  /* optionally create a temporary input file that is a collection of all
   * input files
   */
  assert(get_sourcefile(0)!=NULL);  /* there must be at least one source file */
  if (get_sourcefile(1)!=NULL) {
    /* there are at least two or more source files */
    char *sname;
    FILE *ftmp,*fsrc;
    int fidx;
    #if defined __WIN32__ || defined _WIN32
      tmpname=_tempnam(NULL,"pawn");
    #elif defined __MSDOS__ || defined _Windows
      tmpname=tempnam(NULL,"pawn");
    #elif defined(MACOS) && !defined(__MACH__)
      /* tempnam is not supported for the Macintosh CFM build. */
      error(104,get_sourcefile(1));
      tmpname=NULL;
      sname=NULL;
    #else
      tmpname=tempnam(NULL,"pawn");
    #endif
    ftmp=(FILE*)pc_createsrc(tmpname);
    for (fidx=0; (sname=get_sourcefile(fidx))!=NULL; fidx++) {
      unsigned char tstring[128];
      fsrc=(FILE*)pc_opensrc(sname);
      if (fsrc==NULL) {
        pc_closesrc(ftmp);
        remove(tmpname);
        strcpy(inpfname,sname); /* avoid invalid filename */
        error(100,sname);       /* fatal error, aborts program */
      } /* if */
      pc_writesrc(ftmp,(unsigned char*)"#file \"");
      pc_writesrc(ftmp,(unsigned char*)sname);
      pc_writesrc(ftmp,(unsigned char*)"\"\n#line 0\n");
      while (pc_readsrc(fsrc,tstring,sizeof tstring))
        pc_writesrc(ftmp,tstring);
      pc_closesrc(fsrc);
    } /* for */
    pc_closesrc(ftmp);
    strcpy(inpfname,tmpname);
  } else {
    assert(tmpname==NULL);
    strcpy(inpfname,get_sourcefile(0));
  } /* if */
  inpf_org=(FILE*)pc_opensrc(inpfname);
  if (inpf_org==NULL)
    error(100,inpfname);
  freading=TRUE;
  outf=(FILE*)pc_openasm(outfname); /* first write to assembler file (may be temporary) */
  if (outf==NULL)
    error(101,outfname);
  /* immediately open the binary file, for other programs to check */
  if (sc_asmfile || sc_listing) {
    binf=NULL;
  } else {
    binf=(FILE*)pc_openbin(binfname);
    if (binf==NULL)
      error(101,binfname);
  } /* if */
  setconstants();               /* set predefined constants and tagnames */
  for (i=0; i<skipinput; i++)   /* skip lines in the input file */
    if (pc_readsrc(inpf_org,srcline,sLINEMAX)!=NULL)
      fline++;                  /* keep line number up to date */
  skipinput=fline;
  sc_status=statFIRST;
  /* write starting options (from the command line or the configuration file) */
  if (sc_listing) {
    char string[150];
    sprintf(string,"#pragma ctrlchar 0x%02x\n"
                   "#pragma semicolon %s\n",
            sc_ctrlchar,
            sc_needsemicolon ? "true" : "false");
    if (pc_matchedtabsize>1 && pc_tabsize>1)
      sprintf(string,"#pragma tabsize %d\n",pc_tabsize);
    pc_writeasm(outf,string);
    setfiledirect(inpfname);
  } /* if */
  /* do the first pass through the file (or possibly two or more "first passes") */
  sc_parsenum=0;
  inpfmark=pc_getpossrc(inpf_org,NULL);
  do {
    /* reset "defined" flag of all functions and global variables */
    reduce_referrers(&glbtab);
    delete_symbols(&glbtab,0,TRUE,FALSE);
    delete_heaplisttable();
    #if !defined NO_DEFINE
      delete_substtable();
    #endif
    delete_inputfiletable();
    resetglobals();
    sc_ctrlchar=sc_ctrlchar_org;
    sc_needsemicolon=lcl_needsemicolon;
    pc_tabsize=(pc_matchedtabsize<=1) ? lcl_tabsize : pc_matchedtabsize;
    errorset(sRESET,0);
    /* reset the source file */
    inpf=inpf_org;
    freading=TRUE;
    pc_resetsrc(inpf,inpfmark); /* reset file position */
    fline=skipinput;            /* reset line number */
    sc_reparse=FALSE;           /* assume no extra passes */
    sc_status=statFIRST;        /* resetglobals() resets it to IDLE */

    insert_inputfile(inpfname); /* save for the error system and the report mechanism */
    plungeprefix(incfname);     /* jump into "default.inc" or alternative prefix file */
    preprocess();               /* fetch first line */
    parse();                    /* process all input */
    sc_parsenum++;
  } while (sc_reparse);

  /* after the "discovery" passes, test the global symbol table for undefined
   * symbols
   */
  undefined_vars=checkundefined(&glbtab);

  /* second (or third) pass */
  sc_status=statWRITE;          /* set, to enable warnings */
  state_conflict(&glbtab);

  /* write a report, if requested */
  #if !defined PAWN_LIGHT
    if (sc_makereport) {
      if (strlen(reportname)>0) {
        FILE *frep=fopen(reportname,"wb");  /* avoid translation of \n to \r\n in DOS/Windows */
        if (frep!=NULL) {
          make_report(&glbtab,frep,get_sourcefile(0));
          fclose(frep);
        } /* if */
      } /* if */
      if (pc_globaldoc!=NULL) {
        free(pc_globaldoc);
        pc_globaldoc=NULL;
      } /* if */
      assert(pc_recentdoc==NULL);
    } /* if */
  #endif
  if (sc_listing)
    goto cleanup;

  #if !defined NO_DEFINE
    delete_substtable();
  #endif
  delete_inputfiletable();
  resetglobals();
  sc_ctrlchar=sc_ctrlchar_org;
  sc_needsemicolon=lcl_needsemicolon;
  pc_tabsize=(pc_matchedtabsize<=1) ? lcl_tabsize : pc_matchedtabsize;
  errorset(sRESET,0);
  /* reset the source file */
  inpf=inpf_org;
  freading=TRUE;
  pc_resetsrc(inpf,inpfmark);   /* reset file position */
  fline=skipinput;              /* reset line number */
  if (!lexinit(FALSE))          /* clear internal flags of lex() */
    error(103);                 /* insufficient memory */
  sc_status=statWRITE;          /* allow to write --this variable was reset by resetglobals() */
  writeleader(&glbtab,&lbl_nostate,&lbl_exitstate);
  reduce_referrers(&glbtab);    /* test for unused functions */
  gen_ovlinfo(&glbtab);         /* generate overlay information */
  writestatetables(&glbtab,lbl_nostate,lbl_exitstate);  /* create state tables and additional overlay information */
  /* reset "defined" flag of all functions and global variables */
  delete_symbols(&glbtab,0,TRUE,FALSE);
  insert_dbgfile(inpfname);     /* attach to debug information */
  insert_inputfile(inpfname);   /* save for the error system */
  plungeprefix(incfname);       /* jump into "default.inc" or alternative prefix file */
  preprocess();                 /* fetch first line */
  parse();                      /* process all input */
  /* inpf is already closed when readline() attempts to pop of a file */
  writetrailer();               /* write remaining stuff */

  entry=testsymbols(&glbtab,0,TRUE,FALSE);  /* test for unused or undefined
                                             * functions and variables */
  if (!entry)
    error(13);                  /* no entry point (no public functions) */

cleanup:
  if (inpf!=NULL)               /* main source file is not closed, do it now */
    pc_closesrc(inpf);
  /* write the binary file (the file is already open) */
  if (!(sc_asmfile || sc_listing) && errnum==0 && jmpcode==0) {
    assert(binf!=NULL);
    pc_resetasm(outf);          /* flush and loop back, for reading */
    #if !defined PAWN_LIGHT
      hdrsize=
    #endif
    assemble(binf,outf);        /* assembler file is now input */
  } /* if */
  if (outf!=NULL) {
    pc_closeasm(outf,!(sc_asmfile || sc_listing));
    outf=NULL;
  } /* if */
  if (binf!=NULL) {
    pc_closebin(binf,errnum!=0);
    binf=NULL;
  } /* if */

  #if !defined PAWN_LIGHT
    if (errnum==0 && strlen(errfname)==0) {
      int recursion;
      int flag_exceed=0;
      long stacksize=max_stacksize(&glbtab,&recursion);
      char *max_ovlname=NULL;
      long max_ovlsize=(pc_overlays>0) ? max_overlaysize(&glbtab,&max_ovlname) : 0;
      long totalsize=hdrsize;
      if (pc_overlays==0)
        totalsize+=(long)code_idx;
      else if (pc_overlays==1)
        totalsize+=max_ovlsize;
      else
        totalsize+=pc_overlays;
      if (pc_amxram==0)
        totalsize+=(long)(glb_declared+pc_stksize)*pc_cellsize;
      if (pc_amxlimit>0 && totalsize>=pc_amxlimit)
        flag_exceed=1;
      if (pc_amxram>0 && (glb_declared+pc_stksize)*pc_cellsize>=(unsigned long)pc_amxram)
        flag_exceed=1;
      if ((sc_debug & sSYMBOLIC)!=0 || verbosity>=2 || stacksize+32>=(long)pc_stksize || flag_exceed) {
        if (errnum>0 || warnnum>0)
          pc_printf("\n");
        pc_printf("Header size:       %8lu bytes\n",(long)hdrsize);
        pc_printf("Code size:         %8lu bytes\n",(long)code_idx);
        if (pc_overlays>0) {
          if (pc_overlays>1)
            pc_printf("Max. overlay size: %8lu bytes; largest overlay=%ld bytes\n",(long)pc_overlays,(long)max_ovlsize);
          else
            pc_printf("Largest overlay:   %8lu bytes\n",(long)max_ovlsize);
        } /* if */
        pc_printf("Data size:         %8lu bytes\n",(long)glb_declared*pc_cellsize);
        pc_printf("Stack/heap size:   %8lu bytes; ",(long)pc_stksize*pc_cellsize);
        pc_printf("estimated max. use");
        if (recursion)
          pc_printf(": unknown, due to recursion\n");
        else if ((pc_memflags & suSLEEP_INSTR)!=0)
          pc_printf(": unknown, due to \"sleep\" instruction\n");
        else
          pc_printf("=%lu cells (%lu bytes)\n",stacksize,stacksize*pc_cellsize);
        pc_printf("Total requirements:%8lu bytes",(long)totalsize);
        if (pc_amxram>0)
          pc_printf(" plus %lu bytes for data/stack",(long)(glb_declared+pc_stksize)*pc_cellsize);
        pc_printf("\n");
      } /* if */
      if (pc_overlays>1 && max_ovlsize>pc_overlays) {
        char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
        assert(max_ovlname!=NULL);
        funcdisplayname(symname,max_ovlname);
        error(112,symname,(max_ovlsize-pc_overlays)*pc_cellsize);
      } /* if */
      if (flag_exceed)
        error(106,pc_amxlimit+pc_amxram); /* this causes a jump back to label "cleanup" */
    } /* if */
  #endif

  if (tmpname!=NULL) {
    remove(tmpname);
    #if defined FORTIFY
      /* Fortify does not know about the memory allocated by tempnam(), so
       * it should not check the "free()" call for that memory
       */
      Fortify_Enable(FORTIFY_OFF);
    #endif
    free(tmpname);
    #if defined FORTIFY
      Fortify_Enable(FORTIFY_ENABLED);
    #endif
  } /* if */
  if (inpfname!=NULL)
    free(inpfname);
  if (litq!=NULL)
    free(litq);
  /* when aborting inside an include file, pop off and erase all names on the stack */
  while ((i=POPSTK_I())!=-1) {
    (void)POPSTK_I();   /* fcurrent */
    (void)POPSTK_I();   /* icomment */
    (void)POPSTK_I();   /* sc_is_utf8 */
    (void)POPSTK_I();   /* iflevel */
    (void)POPSTK_P();   /* curlibrary */
    inpfname=(char *)POPSTK_P();
    inpf=(FILE *)POPSTK_P();
    assert(inpfname!=NULL && inpfname!=(char*)-1);
    free(inpfname);
    assert(inpf!=NULL && inpf!=(FILE*)-1);
    fclose(inpf);
  } /* if */
  lexinit(TRUE);                          /* reset and release buffers */
  phopt_cleanup();
  stgbuffer_cleanup();
  clearstk();
  assert(jmpcode!=0 || loctab.next==NULL);/* on normal flow, local symbols
                                           * should already have been deleted */
  delete_symbols(&loctab,0,TRUE,TRUE);    /* delete local variables if not yet
                                           * done (i.e. on a fatal error) */
  delete_symbols(&glbtab,0,TRUE,TRUE);
  delete_consttable(&tagname_tab);
  delete_consttable(&libname_tab);
  delete_consttable(&ntvindex_tab);
  delete_consttable(&sc_automaton_tab);
  delete_consttable(&sc_state_tab);
  state_deletetable();
  delete_aliastable();
  delete_pathtable();
  delete_sourcefiletable();
  delete_inputfiletable();
  delete_dbgstringtable();
  #if !defined NO_DEFINE
    delete_substtable();
  #endif
  #if !defined PAWN_LIGHT
    delete_docstringtable();
    if (pc_globaldoc!=NULL)
      free(pc_globaldoc);
    if (pc_recentdoc!=NULL)
      free(pc_recentdoc);
  #endif
  delete_autolisttable();
  delete_heaplisttable();
  if (errnum!=0) {
    if (strlen(errfname)==0)
      pc_printf("\n%d Error%s.\n",errnum,(errnum>1) ? "s" : "");
    retcode=1;
  } else if (warnnum!=0){
    if (strlen(errfname)==0)
      pc_printf("\n%d Warning%s.\n",warnnum,(warnnum>1) ? "s" : "");
    retcode=0;          /* use "0", so that MAKE and similar tools continue */
  } else {
    retcode=jmpcode;
  } /* if */
  #if defined FORTIFY
    Fortify_ListAllMemory();
  #endif
  return retcode;
}

#if defined __cplusplus
  extern "C"
#endif
DLLEXPORT
int pc_addconstant(const char *name,cell value,int tag)
{
  errorset(sFORCESET,0);        /* make sure error engine is silenced */
  sc_status=statIDLE;
  add_constant(name,value,sGLOBAL,tag);
  return 1;
}

#if defined __cplusplus
  extern "C"
#endif
DLLEXPORT
int pc_addtag(const char *name)
{
  cell val;
  constvalue *ptr;
  int last,tag;

  if (name==NULL) {
    /* no tagname was given, check for one */
    if (lex(&val,(char**)&name)!=tLABEL) {
      lexpush();
      return 0;         /* untagged */
    } /* if */
  } /* if */

  assert(strchr(name,':')==NULL); /* colon should already have been stripped */
  last=0;
  ptr=tagname_tab.next;
  while (ptr!=NULL) {
    tag=(int)(ptr->value & TAGMASK);
    if (strcmp(name,ptr->name)==0)
      return tag;       /* tagname is known, return its sequence number */
    tag &= (int)~FIXEDTAG;
    if (tag>last)
      last=tag;
    ptr=ptr->next;
  } /* while */

  /* tagname currently unknown, add it */
  tag=last+1;           /* guaranteed not to exist already */
  if (isupper(*name))
    tag |= (int)FIXEDTAG;
  append_constval(&tagname_tab,name,(cell)tag,0);
  return tag;
}

static void resetglobals(void)
{
  /* reset the subset of global variables that is modified by the first pass */
  curfunc=NULL;         /* pointer to current function */
  lastst=0;             /* last executed statement type */
  nestlevel=0;          /* number of active (open) compound statements */
  rettype=0;            /* the type that a "return" expression should have */
  litidx=0;             /* index to literal table */
  stgidx=0;             /* index to the staging buffer */
  sc_labnum=0;          /* top value of (internal) labels */
  staging=0;            /* true if staging output */
  declared=0;           /* number of local cells declared */
  glb_declared=0;       /* number of global cells declared */
  code_idx=0;           /* number of bytes with generated code */
  ntv_funcid=0;         /* incremental number of native function */
  curseg=0;             /* 1 if currently parsing CODE, 2 if parsing DATA */
  freading=FALSE;       /* no input file ready yet */
  fline=0;              /* the line number in the current file */
  fnumber=0;            /* the file number in the file table (debugging) */
  fcurrent=0;           /* current file being processed (debugging) */
  sc_intest=FALSE;      /* true if inside a test */
  pc_sideeffect=0;      /* true if an expression causes a side-effect */
  pc_stmtindent=0;      /* current indent of the statement */
  indent_nowarn=FALSE;  /* do not skip warning "217 loose indentation" */
  sc_allowtags=TRUE;    /* allow/detect tagnames */
  sc_status=statIDLE;
  sc_allowproccall=FALSE;
  sc_alignnext=FALSE;
  pc_docexpr=FALSE;
  pc_deprecate=NULL;
  sc_curstates=0;
  pc_memflags=0;
  pc_enumsequence=0;
}

static void initglobals(void)
{
  int i;

  resetglobals();

  sc_asmfile=FALSE;     /* do not create .ASM file */
  sc_listing=FALSE;     /* do not create .LST file */
  skipinput=0;          /* number of lines to skip from the first input file */
  sc_ctrlchar=CTRL_CHAR;/* the escape character */
  litmax=sDEF_LITMAX;   /* current size of the literal table */
  errnum=0;             /* number of errors */
  warnnum=0;            /* number of warnings */
  optproccall=TRUE;     /* support "procedure call" */
  verbosity=1;          /* verbosity level, no copyright banner */
  sc_debug=sCHKBOUNDS;  /* by default: bounds checking+assertions */
  pc_optimize=sOPTIMIZE_CORE;
  sc_needsemicolon=FALSE;/* semicolon required to terminate expressions? */
  pc_cellsize=4;        /* default cell size = 4 bytes, 32-bits */
  sc_dataalign=pc_cellsize;
  pc_stksize=sDEF_AMXSTACK;/* default stack size */
  pc_addlibtable=TRUE;  /* by default, add a "library table" to the output file */
  pc_amxlimit=0;        /* no limit on size of the abstract machine */
  pc_amxram=0;          /* no limit on data size of the abstract machine */
  pc_tabsize=8;         /* assume a TAB is 8 spaces */
  pc_matchedtabsize=0;  /* allow auto-adjust of TAB size (no space indents detected yet) */
  sc_rationaltag=0;     /* assume no support for rational numbers */
  rational_digits=0;    /* number of fractional digits */
  undefined_vars=FALSE; /* if TRUE, undefined symbols were found */
  pc_overlays=0;        /* do not generate for overlays */
  for (i=0; i<ovlFIRST; i++)
    pc_ovl0size[i][0]=pc_ovl0size[i][1]=0;
  pc_cryptkey=0;

  outfname[0]='\0';     /* output file name */
  errfname[0]='\0';     /* error file name */
  inpf=NULL;            /* file read from */
  inpfname=NULL;        /* pointer to name of the file currently read from */
  outf=NULL;            /* file written to */
  litq=NULL;            /* the literal queue */
  glbtab.next=NULL;     /* clear global variables/constants table */
  loctab.next=NULL;     /*   "   local      "    /    "       "   */
  tagname_tab.next=NULL;/* tagname table */
  libname_tab.next=NULL;/* library table (#pragma library "..." syntax) */
  ntvindex_tab.next=NULL;

  lptr=NULL;            /* points to the current position in "srcline" */
  curlibrary=NULL;      /* current library */
  inpf_org=NULL;        /* main source file */

  wqptr=wq;             /* initialize while queue pointer */

#if !defined PAWN_LIGHT
  pc_globaldoc=NULL;
  pc_recentdoc=NULL;
  sc_makereport=FALSE;  /* do not generate a cross-reference report */
  pc_docstring_suspended=FALSE;
#endif
}

static char *get_extension(char *filename)
{
  char *ptr;

  assert(filename!=NULL);
  ptr=strrchr(filename,'.');
  if (ptr!=NULL) {
    /* ignore extension on a directory or at the start of the filename */
    if (strchr(ptr,DIRSEP_CHAR)!=NULL || ptr==filename || *(ptr-1)==DIRSEP_CHAR)
      ptr=NULL;
  } /* if */
  return ptr;
}

/* set_extension
 * Set the default extension, or force an extension. To erase the
 * extension of a filename, set "extension" to an empty string.
 */
SC_FUNC void set_extension(char *filename,char *extension,int force)
{
  char *ptr;

  assert(extension!=NULL && (*extension=='\0' || *extension=='.'));
  assert(filename!=NULL);
  ptr=get_extension(filename);
  if (force && ptr!=NULL)
    *ptr='\0';          /* set zero terminator at the position of the period */
  if (force || ptr==NULL)
    strcat(filename,extension);
}

static const char *option_value(const char *optptr)
{
  return (*(optptr+1)=='=' || *(optptr+1)==':') ? optptr+2 : optptr+1;
}

static int toggle_option(const char *optptr, int option)
{
  switch (*option_value(optptr)) {
  case '\0':
    option=!option;
    break;
  case '-':
    option=FALSE;
    break;
  case '+':
    option=TRUE;
    break;
  default:
    about();
  } /* switch */
  return option;
}

/* Parsing command line options is indirectly recursive: parseoptions()
 * calls parserespf() to handle options in a a response file and
 * parserespf() calls parseoptions() at its turn after having created
 * an "option list" from the contents of the file.
 */
#if !defined PAWN_LIGHT
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *rname, char *codepage);
#endif

static void parseoptions(int argc,char **argv,char *oname,char *ename,char *pname,
                         char *rname, char *codepage)
{
  char str[_MAX_PATH];
  const char *ptr,*optionptr;
  int arg,i,isoption;

  for (arg=1; arg<argc; arg++) {
    #if DIRSEP_CHAR=='/'
      isoption= argv[arg][0]=='-';
    #else
      isoption= argv[arg][0]=='/' || argv[arg][0]=='-';
    #endif
    if (isoption) {
      ptr=&argv[arg][1];
      switch (*ptr) {
      case 'A':
        i=atoi(option_value(ptr));
        if (i>0 && (i % pc_cellsize)==0)
          sc_dataalign=i;
        else
          about();
        break;
      case 'a':
        if (*(ptr+1)!='\0')
          about();
        sc_asmfile=TRUE;        /* skip last pass of making binary file */
        if (verbosity>1)
          verbosity=1;
        break;
      case 'C':
        i=atoi(option_value(ptr));
        if ((i==16 || i==32 || i==64) && i <= PAWN_CELL_SIZE)
          pc_cellsize=i/8;
        else
          about();
        /* change default data-align too */
        if (sc_dataalign<pc_cellsize || sc_dataalign==4)
          sc_dataalign=pc_cellsize;
        break;
      case 'c':
        strlcpy(codepage,option_value(ptr),MAXCODEPAGE);  /* set name of codepage */
        break;
#if defined dos_setdrive
      case 'D':                 /* set active directory */
        ptr=option_value(ptr);
        if (ptr[1]==':')
          dos_setdrive(toupper(*ptr)-'A'+1);    /* set active drive */
        chdir(ptr);
        break;
#endif
      case 'd':
        switch (*option_value(ptr)) {
        case '0':
          sc_debug=0;
          break;
        case '1':
          sc_debug=sCHKBOUNDS;  /* assertions and bounds checking */
          break;
        case '2':
          sc_debug=sCHKBOUNDS | sSYMBOLIC;  /* also symbolic info */
          break;
        case '3':
          sc_debug=sCHKBOUNDS | sSYMBOLIC;
          pc_optimize=sOPTIMIZE_NONE;
          /* also avoid peephole optimization */
          break;
        default:
          about();
        } /* switch */
        break;
      case 'e':
        strlcpy(ename,option_value(ptr),_MAX_PATH); /* set name of error file */
        break;
      case 'i':
        strlcpy(str,option_value(ptr),sizeof str);  /* set name of include directory */
        i=(int)strlen(str);
        if (i>0) {
          if (str[i-1]!=DIRSEP_CHAR) {
            str[i]=DIRSEP_CHAR;
            str[i+1]='\0';
          } /* if */
          insert_path(str);
        } /* if */
        break;
      case 'k':
        ptr=option_value(ptr);
        while (*ptr!='\0') {
          uint64_t h;
          if ('0'<=*ptr && *ptr<='9')
            h=*ptr-'0';
          else if ('A'<=*ptr && *ptr<='F')
            h=*ptr-'A'+10;
          else if ('a'<=*ptr && *ptr<='f')
            h=*ptr-'a'+10;
          else
            h=*ptr & 0x0f;
          pc_cryptkey=(pc_cryptkey << 4) | h;
          ptr++;
        } /* while */
        break;
      case 'l':
        if (*(ptr+1)!='\0')
          about();
        sc_listing=TRUE;        /* skip second pass & code generation */
        break;
      case 'N':
        strlcpy(str,option_value(ptr),sizeof str);
        if ((ptr=strchr(str,'='))!=NULL && (i=atoi(ptr+1))<0) {
          *(char*)ptr='\0';
          append_constval(&ntvindex_tab,str,i,0);
          pc_addlibtable=FALSE; /* no library table, for hard-coded natives */
        } /* if */
        break;
      case 'o':
        strlcpy(oname,option_value(ptr),_MAX_PATH); /* set name of (binary) output file */
        break;
      case 'O':
        pc_optimize=*option_value(ptr) - '0';
        if (pc_optimize<sOPTIMIZE_NONE || pc_optimize>=sOPTIMIZE_NUMBER)
          about();
        break;
      case 'p':
        strlcpy(pname,option_value(ptr),_MAX_PATH); /* set name of implicit include file */
        break;
#if !defined PAWN_LIGHT
      case 'r':
        strlcpy(rname,option_value(ptr),_MAX_PATH); /* set name of report file */
        sc_makereport=TRUE;
        if (strlen(rname)>0)
          set_extension(rname,".xml",FALSE);
        break;
#endif
      case 'S':
        i=atoi(option_value(ptr));
        if (i>32)
          pc_stksize=(cell)i;   /* stack size has minimum size */
        else
          about();
        break;
      case 's':
        skipinput=atoi(option_value(ptr));
        break;
      case 'T':
        /* this option was already handled on an initial scan, see setopt() */
        break;
      case 't':
        i=atoi(option_value(ptr));
        if (2<=i && i<=8)
          pc_tabsize=pc_matchedtabsize=i;
        break;
      case 'V':
        /* allow -V+, -V- and -V1234, where 1234 is the code pool size;
         * with -V+, this pool is set to 1 byte, which means "unlimited"
         */
        optionptr=option_value(ptr);
        if (isdigit(*optionptr))
          pc_overlays=atoi(optionptr);
        else
          pc_overlays=toggle_option(ptr,pc_overlays);
        /* there is a technical limit on the size of an overlay of the max. number
         * that fits in half a cell
         */
        if (pc_overlays>=((cell)1<<4*pc_cellsize))
          pc_overlays=(int)((cell)1<<4*pc_cellsize);
        break;
      case 'v':
        optionptr=option_value(ptr);
        verbosity= isdigit(*optionptr) ? atoi(optionptr) : 2;
        if (sc_asmfile && verbosity>1)
          verbosity=1;
        break;
      case 'w':
        i=(int)strtol(option_value(ptr),(char **)&ptr,10);
        if (*ptr=='-')
          pc_enablewarning(i,0);
        else if (*ptr=='+')
          pc_enablewarning(i,1);
        else if (*ptr=='\0')
          pc_enablewarning(i,2);
        break;
      case 'X':
        if (*(ptr+1)=='D') {
          i=atoi(option_value(ptr+1));
          if (i>64)
            pc_amxram=(cell)i;  /* abstract machine data/stack has minimum size */
          else
            about();
        } else {
          i=atoi(option_value(ptr));
          if (i>64)
            pc_amxlimit=(cell)i;/* abstract machine has minimum size */
          else
            about();
        } /* if */
        break;
      case '\\':                /* use \ instead for escape characters */
        sc_ctrlchar='\\';
        break;
      case '^':                 /* use ^ instead for escape characters */
        sc_ctrlchar='^';
        break;
      case ';':
        sc_needsemicolon=toggle_option(ptr,sc_needsemicolon);
        break;
      case '(':
        optproccall=!toggle_option(ptr,!optproccall);
        break;
      default:                  /* wrong option */
        about();
      } /* switch */
    } else if (argv[arg][0]=='@') {
      #if !defined PAWN_LIGHT
        parserespf(&argv[arg][1],oname,ename,pname,rname,codepage);
      #endif
    } else if ((ptr=strchr(argv[arg],'='))!=NULL) {
      i=(int)(ptr-argv[arg]);
      if (i>sNAMEMAX) {
        i=sNAMEMAX;
        error(200,argv[arg],sNAMEMAX);  /* symbol too long, truncated to sNAMEMAX chars */
      } /* if */
      strlcpy(str,argv[arg],i+1);       /* str holds symbol name */
      i=atoi(ptr+1);
      add_constant(str,i,sGLOBAL,0);
    } else {
      strlcpy(str,argv[arg],sizeof(str)-2); /* -2 because default extension is ".p" */
      set_extension(str,".p",FALSE);
      insert_sourcefile(str);
      /* The output name is the first input name with a different extension,
       * but it is stored in a different directory
       */
      if (strlen(oname)==0) {
        if ((ptr=strrchr(str,DIRSEP_CHAR))!=NULL)
          ptr++;          /* strip path */
        else
          ptr=str;
        assert(strlen(ptr)<_MAX_PATH);
        strcpy(oname,ptr);
      } /* if */
      set_extension(oname,".asm",TRUE);
    } /* if */
  } /* for */
#if !defined PAWN_LIGHT
  if (sc_makereport && strlen(rname)==0) {
    if (strlen(oname)==0) {
      /* not output filename is set, use the name of the first source file,
       * without the path
       */
      char *name;
      if ((name=get_sourcefile(0))!=NULL) {
        assert(strlen(name)<_MAX_PATH);
        if ((ptr=strrchr(name,DIRSEP_CHAR))!=NULL)
          ptr++;          /* strip path */
        else
          ptr=name;
        assert(strlen(ptr)<_MAX_PATH);
        strcpy(rname,ptr);
      } /* if */
    } else {
      /* a specific output filename is set, use it for the report name too */
      strcpy(rname,oname);
    } /* if */
    set_extension(rname,".xml",TRUE);
  } /* if */
#endif
}

#if !defined PAWN_LIGHT
static void parserespf(char *filename,char *oname,char *ename,char *pname,
                       char *rname,char *codepage)
{
#define MAX_OPTIONS     200
  FILE *fp;
  char *string, *ptr;
  char **argv;
  int argc,maxoptions;
  long size;

  if ((fp=fopen(filename,"r"))==NULL)
    error(100,filename);        /* error reading input file */
  /* load the complete file into memory */
  fseek(fp,0L,SEEK_END);
  size=ftell(fp);
  fseek(fp,0L,SEEK_SET);
  assert(size<INT_MAX);
  if ((string=(char *)malloc(((int)size+1)*sizeof(char)))==NULL)
    error(103);                 /* insufficient memory */
  /* fill with zeros; in MS-DOS, fread() may collapse CR/LF pairs to
   * a single '\n', so the string size may be smaller than the file
   * size. */
  memset(string,0,(int)size+1);
  fread(string,1,(int)size,fp);
  fclose(fp);
  /* remove comments in the response file */
  while ((ptr=strchr(string,'#'))!=NULL) {
    *ptr=' ';                   /* pad with spaces up to \n */
    while (*++ptr!='\n')
      *ptr=' ';
  } /* while */
  /* allocate an initial table for option pointers */
  maxoptions=2;
  if ((argv=(char**)malloc(maxoptions*sizeof(char*)))==NULL)
    error(103);                 /* insufficient memory */
  /* fill the options table */
  ptr=strtok(string," \t\r\n");
  argc=1; /* note: the routine skips argv[0], for compatibility with main() */
  while (ptr!=NULL) {
    if (argc>=maxoptions) {
      maxoptions*=2;
      if ((argv=(char**)realloc(argv,maxoptions*sizeof(char*)))==NULL)
        error(103);             /* insufficient memory */
    } /* if */
    argv[argc++]=ptr;
    ptr=strtok(NULL," \t\r\n");
  } /* while */
  if (ptr!=NULL)
    error(102,"option table");   /* table overflow */
  /* parse the option table */
  parseoptions(argc,argv,oname,ename,pname,rname,codepage);
  /* free allocated memory */
  free(argv);
  free(string);
}
#endif

static void setopt(int argc,char **argv,char *oname,char *ename,char *pname,
                   char *rname,char *codepage)
{
  delete_sourcefiletable(); /* make sure it is empty */
  *oname='\0';
  *ename='\0';
  *pname='\0';
  *rname='\0';
  *codepage='\0';
  strcpy(pname,sDEF_PREFIX);

  #if !defined PAWN_LIGHT
    /* first parse a "config" file with default options */
    if (sc_binpath[0]!='\0') {
      char cfgfile[_MAX_PATH];
      const char *ptr;
      char *base;
      int i,isoption,found;
      /* copy the default config file name, but keep a pointer to the location
       * of the base name
       */
      assert(strlen(sc_binpath)<sizeof cfgfile);
      strcpy(cfgfile,sc_binpath);
      base=strchr(cfgfile,'\0');
      assert(base!=NULL);
      strcpy(base,"pawn.cfg");
      /* run through the argument list to see whether a -T option is present */
      found=0;
      for (i=1; i<argc; i++) {
        #if DIRSEP_CHAR=='/'
          isoption= argv[i][0]=='-';
        #else
          isoption= argv[i][0]=='/' || argv[i][0]=='-';
        #endif
        if (isoption && argv[i][1]=='T') {
          found=1;
          ptr=option_value(&argv[i][1]);
          if (strchr(ptr,DIRSEP_CHAR)!=NULL)
            strlcpy(cfgfile,ptr,_MAX_PATH); /* assume full path */
          else
            strlcpy(base,ptr,_MAX_PATH);    /* no path */
          ptr=strrchr(cfgfile,'.');
          if (ptr==NULL || strchr(ptr,DIRSEP_CHAR)!=NULL)
            strlcat(cfgfile,".cfg",_MAX_PATH);
        } /* if */
      } /* for */
      if (access(cfgfile,4)==0)
        parserespf(cfgfile,oname,ename,pname,rname,codepage);
      else if (found)
        error(100,cfgfile);  /* config. file was explicitly specified, but cannot be read */
    } /* if */
  #endif
  parseoptions(argc,argv,oname,ename,pname,rname,codepage);
  if (get_sourcefile(0)==NULL)
    about();
  if (pc_cryptkey!=0 && pc_cellsize<4)
    error(104,"-k","cell size < 32-bits");
}

#if defined __BORLANDC__ || defined __WATCOMC__
  #pragma argsused
#endif
static void setconfig(char *root)
{
  char path[_MAX_PATH]="";
  char *ptr,*base;
  size_t len;

  #if defined macintosh || defined __APPLE__
    /* OS X makes the directory of the binary "current" when it is launched */
    getcwd(path,sizeof path);
  #elif defined __WIN32__ || defined _WIN32
    GetModuleFileName(NULL,path,_MAX_PATH);
  #elif defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
    /* see www.autopackage.org for the BinReloc module */
    br_init_lib(NULL);
    ptr=br_find_exe("/opt/Pawn/bin/pawncc");
    strlcpy(path,ptr,sizeof path);
    free(ptr);
  #else
    if (root!=NULL) {
      strlcpy(path,root,sizeof path); /* path + filename (hopefully) */
    } else {
      getcwd(path,sizeof path);
      /* add a final \ or / to the path (which is stripped of later) */
      ptr=strchr(path,'\0');
      assert(ptr!=NULL);
      assert((ptr-path)<sizeof path-2);
      *ptr=DIRSEP_CHAR;
      *(ptr+1)='\0';
    } /* if */
  #endif
  #if defined __MSDOS__
    /* strip the options (appended to the path + filename) */
    if ((ptr=strpbrk(path," \t/"))!=NULL)
      *ptr='\0';
  #endif
  #if defined macintosh || defined __APPLE__
    /* add a final ':' to the path */
    ptr=strchr(path,'\0');
    assert(ptr!=NULL);
    assert((ptr-path)<sizeof path-1);
    *ptr=DIRSEP_CHAR;
    *(ptr+1)='\0';
  #else
    /* "path" contains path + filename, terminate just behind last \, / or : */
    if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL || (ptr=strchr(path,':'))!=NULL) {
      /* If there is no "\" or ":", the string probably does not contain the
       * path; so we just don't add it to the list in that case
       */
      *(ptr+1)='\0';
    } /* if */
  #endif

  if (strlen(path)>0) {
    #if !defined PAWN_LIGHT
      assert(sizeof sc_binpath==sizeof path);
      strcpy(sc_binpath,path);
    #endif
    base=strchr(path,'\0')-1; /* base points to last character in the string (probably a directory separator) */
    strcat(path,"include");
    len=strlen(path);
    path[len]=DIRSEP_CHAR;
    path[len+1]='\0';
    /* see if it exists */
    if (access(path,0)!=0 && *base==DIRSEP_CHAR) {
      /* There is no "include" directory below the directory where the compiler
       * is found. This typically means that the compiler is in a "bin" sub-directory
       * and the "include" is below the *parent*. So find the parent...
       */
      *base='\0';
      if ((ptr=strrchr(path,DIRSEP_CHAR))!=NULL) {
        *(ptr+1)='\0';
        strcat(path,"include");
        len=strlen(path);
        path[len]=DIRSEP_CHAR;
        path[len+1]='\0';
      } else {
        /* no parent, restore path */
        *base=DIRSEP_CHAR;
      } /* if */
    } /* if */
    insert_path(path);
    /* same for the codepage root */
    #if !defined PAWN_NO_CODEPAGE
      *ptr='\0';
      if (!cp_path(path,"codepage"))
        error(109,path);        /* codepage path */
    #endif
    /* also copy the root path (for the XML documentation) */
    #if !defined PAWN_LIGHT
      *ptr='\0';
      strlcpy(sc_rootpath,path,sizeof sc_rootpath);
    #endif
  } /* if */
}

static void setcaption(void)
{
  pc_printf("Pawn compiler %-25s Copyright (c) 1997-2012, ITB CompuPhase\n\n",VERSION_STR);
}

static void about(void)
{
  if (strlen(errfname)==0) {
    setcaption();
    pc_printf("Usage:   pawncc <filename> [filename...] [options]\n\n");
    pc_printf("Options:\n");
    pc_printf("         -A<num>  alignment in bytes of the data segment and the stack\n");
    pc_printf("         -a       output assembler code\n");
    pc_printf("         -C<num>  cell size in bits (default=-C%d)\n",pc_cellsize*8);
    pc_printf("         -c<name> codepage name or number; e.g. 1252 for Windows Latin-1\n");
#if defined dos_setdrive
    pc_printf("         -Dpath   active directory path\n");
#endif
    pc_printf("         -d<num>  debugging level (default=-d%d)\n",sc_debug);
    pc_printf("             0    no symbolic information, no run-time checks\n");
    pc_printf("             1    run-time checks, no symbolic information\n");
    pc_printf("             2    full debug information and dynamic checking\n");
    pc_printf("             3    same as -d2, but implies -O0\n");
    pc_printf("         -e<name> set name of error file (quiet compile)\n");
    pc_printf("         -i<name> path for include files\n");
    pc_printf("         -k<hex>  key for encrypted scripts\n");
    pc_printf("         -l       create list file (preprocess only)\n");
    pc_printf("         -o<name> set base name of (P-code) output file\n");
    pc_printf("         -O<num>  optimization level (default=-O%d)\n",pc_optimize);
    pc_printf("             0    no optimization\n");
    pc_printf("             1    core instruction set (JIT-compatible)\n");
    pc_printf("             2    supplemental instruction set\n");
    pc_printf("             3    full instruction set (packed opcodes)\n");
    pc_printf("         -p<name> set name of the \"prefix\" file\n");
#if !defined PAWN_LIGHT
    pc_printf("         -r[name] write cross reference report to console or to specified file\n");
#endif
    pc_printf("         -S<num>  stack/heap size in cells (default=%d)\n",(int)pc_stksize);
    pc_printf("         -s<num>  skip lines from the input file\n");
    pc_printf("         -t<num>  TAB indent size (in character positions, default=%d)\n",pc_tabsize);
    pc_printf("         -T<name> set name of the configuration file to use\n");
    pc_printf("         -V<num>  generate overlay code and instructions; set buffer size\n");
    pc_printf("         -v<num>  verbosity level; 0=quiet, 1=normal, 2=verbose (default=%d)\n",verbosity);
    pc_printf("         -w<num>  disable a specific warning by its number\n");
    pc_printf("         -X<num>  abstract machine size limit in bytes\n");
    pc_printf("         -XD<num> abstract machine data/stack size limit in bytes\n");
    pc_printf("         -\\       use '\\' for escape characters\n");
    pc_printf("         -^       use '^' for escape characters\n");
    pc_printf("         -;[+/-]  require a semicolon to end each statement (default=%c)\n", sc_needsemicolon ? '+' : '-');
    pc_printf("         -([+/-]  require parantheses for function invocation (default=%c)\n", optproccall ? '-' : '+');
    pc_printf("         sym=val  define constant \"sym\" with value \"val\"\n");
    pc_printf("         sym=     define constant \"sym\" with value 0\n");
#if defined __WIN32__ || defined _WIN32 || defined _Windows || defined __MSDOS__
    pc_printf("\nOptions may start with a dash or a slash; the options \"-d0\" and \"/d0\" are\n");
    pc_printf("equivalent.\n");
#endif
    pc_printf("\nOptions with a value may optionally separate the value from the option letter\n");
    pc_printf("with a colon (\":\") or an equal sign (\"=\"). That is, the options \"-d0\", \"-d=0\"\n");
    pc_printf("and \"-d:0\" are all equivalent.\n");
  } /* if */
  longjmp(errbuf,3);        /* user abort */
}

static void setconstants(void)
{
  int debug;

  assert(sc_status==statIDLE);
  append_constval(&tagname_tab,"_",0,0);  /* "untagged" */
  append_constval(&tagname_tab,"bool",1,0);

  add_constant("true",1,sGLOBAL,1); /* boolean flags */
  add_constant("false",0,sGLOBAL,1);
  add_constant("EOS",0,sGLOBAL,0);  /* End Of String, or '\0' */
  add_constant("cellbits",pc_cellsize*8,sGLOBAL,0);
  add_constant("cellmax",(cell)(((ucell)1<<(8*pc_cellsize-1))-1),sGLOBAL,0);
  add_constant("cellmin",(cell)((ucell)-1<<(8*pc_cellsize-1)),sGLOBAL,0);
  add_constant("charbits",sCHARBITS,sGLOBAL,0);
  add_constant("charmin",0,sGLOBAL,0);
  add_constant("charmax",~(-1 << sCHARBITS),sGLOBAL,0);
  add_constant("ucharmax",((cell)1 << (pc_cellsize-1)*8)-1,sGLOBAL,0);

  add_constant("__Pawn",VERSION_INT,sGLOBAL,0);
  add_constant("__line",0,sGLOBAL,0);

  debug=0;
  if ((sc_debug & (sCHKBOUNDS | sSYMBOLIC))==(sCHKBOUNDS | sSYMBOLIC))
    debug=2;
  else if ((sc_debug & sCHKBOUNDS)==sCHKBOUNDS)
    debug=1;
  add_constant("debug",debug,sGLOBAL,0);

  add_constant("overlaysize",pc_overlays,sGLOBAL,0);
  append_constval(&sc_automaton_tab,"",0,0);    /* anonymous automaton */
}

#if !defined PAWN_LIGHT
/* sc_attachdocumentation()
 * appends documentation comments to the passed-in symbol, or to a global
 * string if "sym" is NULL.
 */
void sc_attachdocumentation(symbol *sym,int onlylastblock)
{
  int line,wrap;
  size_t length;
  char *str,*doc,*end;

  if (!sc_makereport || sc_status!=statFIRST || sc_parsenum>0) {
    /* just clear the entire table */
    delete_docstringtable();
    return;
  } /* if */
  /* in the case of state functions, multiple documentation sections may
   * appear; we should concatenate these
   * (with forward declarations, this is also already the case, so the assertion
   * below is invalid)
   */
  // assert(sym==NULL || sym->documentation==NULL || sym->states!=NULL);

  if (pc_recentdoc!=NULL) {
    /* either copy the most recent comment block to the current symbol, or
     * append it to the global documentation
     */
    length=strlen(pc_recentdoc);
    wrap=0;
    if (onlylastblock) {
      assert(sym!=NULL);
      str=sym->documentation;
    } else {
      str=pc_globaldoc;
    } /* if */
    /* set the documentation, or append it to it */
    if (str!=NULL)
      length+=strlen(str)+1+4;   /* plus 4 for "<p/>" */
    doc=(char*)malloc((length+1)*sizeof(char));
    if (doc!=NULL) {
      if (str!=NULL) {
        strcpy(doc,str);
        free(str);
        strcat(doc,"<p/>");
      } else {
        *doc='\0';
      } /* if */
      end=strchr(doc,'\0');
      assert(end!=NULL);
      strcat(doc,pc_recentdoc);
      if (wrap) {
        strins(end,"<summary>",9);
        end+=9;         /* skip "<summary>" */
        while (*end!='\0' && *end!='.' && *end!='<')
          end++;
        if (*end=='.')
          end++;        /* end the summary behind the period */
        strins(end,"</summary>",10);
      } /* if */
      if (onlylastblock) {
        sym->documentation=doc;
        /* force adding the new strings (if any) to the global data */
        sym=NULL;
      } else {
        pc_globaldoc=doc;
      } /* if */
    } /* if */
    /* remove the "recent block" string */
    free(pc_recentdoc);
    pc_recentdoc=NULL;
  } /* if */

  if (onlylastblock)
    pc_docstring_suspended=FALSE;
  else if (pc_docstring_suspended)
    return;

  /* first check the size */
  length=0;
  for (line=0; (str=get_docstring(line))!=NULL && *str!=sDOCSEP; line++) {
    if (length>0)
      length++;   /* count 1 extra for a separating space */
    length+=strlen(str);
  } /* for */

  if (length>0) {
    /* allocate memory for the documentation */
    if (sym!=NULL && sym->documentation!=NULL)
      length+=strlen(sym->documentation)+1+4;   /* plus 4 for "<p/>" */
    doc=(char*)malloc((length+1)*sizeof(char));
    if (doc!=NULL) {
      /* initialize string or concatenate */
      if (sym!=NULL && sym->documentation!=NULL) {
        strcpy(doc,sym->documentation);
        strcat(doc,"<p/>");
        free(sym->documentation);
        sym->documentation=NULL;
      } else {
        doc[0]='\0';
      } /* if */
      /* collect all documentation */
      while ((str=get_docstring(0))!=NULL && *str!=sDOCSEP) {
        if (doc[0]!='\0')
          strcat(doc," ");
        strcat(doc,str);
        delete_docstring(0);
      } /* while */
      if (str!=NULL) {
        /* also delete the separator */
        assert(*str==sDOCSEP);
        delete_docstring(0);
      } /* if */
      if (sym!=NULL) {
        assert(sym->documentation==NULL);
        sym->documentation=doc;
      } else {
        assert(pc_recentdoc==NULL);
        pc_recentdoc=doc;
      } /* if */
    } /* if */
  } else {
    /* delete an empty separator, if present */
    if ((str=get_docstring(0))!=NULL && *str==sDOCSEP)
      delete_docstring(0);
  } /* if */
}

static void insert_docstring_separator(void)
{
  char sep[2]={sDOCSEP,'\0'};
  insert_docstring(sep,1);
}
#else
  #define sc_attachdocumentation(s,f)    (void)(s)
  #define insert_docstring_separator()
#endif

static void plungeprefix(char *prefixname)
{

  if (strlen(prefixname)>0) {
    int ok;
    if (strchr(prefixname,DIRSEP_CHAR)==NULL) {
      /* no path, search the prefix file in the include directory */
      ok=plungefile(prefixname,FALSE,TRUE);
    } else {
      /* when a path is given for the prefix file, it must be absolute */
      ok=plungequalifiedfile(prefixname);
    } /* if */
    /* silently ignore a "default.inc" file that cannot be read, but give
     * an error on explicit files
     */
    if (!ok && strcmp(prefixname,sDEF_PREFIX)!=0)
      error(100,prefixname);    /* cannot read from ... (fatal error) */
  } /* if */
}

static int getclassspec(int initialtok,int *fpublic,int *fstatic,int *fstock,int *fconst)
{
  int tok,err;
  cell val;
  char *str;

  assert(fconst!=NULL);
  assert(fstock!=NULL);
  assert(fstatic!=NULL);
  assert(fpublic!=NULL);
  *fconst=FALSE;
  *fstock=FALSE;
  *fstatic=FALSE;
  *fpublic=FALSE;
  switch (initialtok) {
  case tCONST:
    *fconst=TRUE;
    break;
  case tSTOCK:
    *fstock=TRUE;
    break;
  case tSTATIC:
    *fstatic=TRUE;
    break;
  case tPUBLIC:
    *fpublic=TRUE;
    break;
  } /* switch */

  err=0;
  do {
    tok=lex(&val,&str);  /* read in (new) token */
    switch (tok) {
    case tCONST:
      if (*fconst)
        err=42;          /* invalid combination of class specifiers */
      *fconst=TRUE;
      break;
    case tSTOCK:
      if (*fstock)
        err=42;          /* invalid combination of class specifiers */
      *fstock=TRUE;
      break;
    case tSTATIC:
      if (*fstatic)
        err=42;          /* invalid combination of class specifiers */
      *fstatic=TRUE;
      break;
    case tPUBLIC:
      if (*fpublic)
        err=42;          /* invalid combination of class specifiers */
      *fpublic=TRUE;
      break;
    default:
      lexpush();
      tok=0;             /* force break out of loop */
    } /* switch */
  } while (tok && err==0);

  /* extra checks */
  if (*fstatic && *fpublic) {
    err=42;              /* invalid combination of class specifiers */
    *fstatic=*fpublic=FALSE;
  } /* if */

  if (err)
    error(err);
  return err==0;
}

/*  parse       - process all input text
 *
 *  At this level, only static declarations and function definitions are legal.
 */
static void parse(void)
{
  int tok,fconst,fstock,fstatic,fpublic;
  cell val;
  char *str;

  while (freading){
    /* first try whether a declaration possibly is native or public */
    tok=lex(&val,&str);  /* read in (new) token */
    switch (tok) {
    case 0:
      /* ignore zero's */
      break;
    case tNEW:
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst))
        declglb(NULL,0,fpublic,fstatic,fstock,fconst);
      break;
    case tSTATIC:
      /* This can be a static function or a static global variable; we know
       * which of the two as soon as we have parsed up to the point where an
       * opening paranthesis of a function would be expected. To back out after
       * deciding it was a declaration of a static variable after all, we have
       * to store the symbol name and tag.
       */
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
        assert(!fpublic);
        declfuncvar(fpublic,fstatic,fstock,fconst);
      } /* if */
      break;
    case tCONST:
      decl_const(sGLOBAL);
      break;
    case tPUBLIC:
      /* This can be a public function or a public variable; see the comment
       * above (for static functions/variables) for details.
       */
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
        assert(!fstatic);
        declfuncvar(fpublic,fstatic,fstock,fconst);
      } /* if */
      break;
    case tSTOCK:
      /* This can be a stock function or a stock *global*) variable; see the
       * comment above (for static functions/variables) for details.
       */
      if (getclassspec(tok,&fpublic,&fstatic,&fstock,&fconst)) {
        assert(fstock);
        declfuncvar(fpublic,fstatic,fstock,fconst);
      } /* if */
      break;
    case tEXIT:
    case tLABEL:
    case tSYMBOL:
    case tOPERATOR:
      /* at this level, if the token is tEXIT, it is probably the declaration
       * of a state exist function, not the "exit" statement
       */
      if (tok==tEXIT)
        tok=lexsettoken(tSYMBOL,uEXITFUNC);
      lexpush();
      if (!newfunc(NULL,-1,FALSE,FALSE,FALSE)) {
        error(10);              /* illegal function or declaration */
        lexclr(TRUE);           /* drop the rest of the line */
        litidx=0;               /* drop the literal queue too */
      } /* if */
      break;
    case tNATIVE:
      funcstub(TRUE);           /* create a dummy function */
      break;
    case tFORWARD:
      funcstub(FALSE);
      break;
    case '}':
      error(54);                /* unmatched closing brace */
      break;
    case '{':
      error(55);                /* start of function body without function header */
      break;
    default:
      if (freading) {
        error(10);              /* illegal function or declaration */
        lexclr(TRUE);           /* drop the rest of the line */
        litidx=0;               /* drop any literal arrays (strings) */
      } /* if */
    } /* switch */
  } /* while */
  sc_attachdocumentation(NULL,FALSE);  /* attach any trailing comment to the main documentation */
}

/*  dumplits
 *
 *  Dump the literal pool (strings etc.)
 *
 *  Global references: litidx (referred to only)
 */
static void dumplits(void)
{
  int j,k;

  if (sc_status==statSKIP)
    return;

  k=0;
  while (k<litidx){
    /* should be in the data segment */
    assert(curseg==2);
    defstorage();
    j=32/pc_cellsize; /* 16 values per line for 2-byte cells, 8 for 4-byte cells, etc. */
    while (j && k<litidx){
      outval(litq[k],TRUE,FALSE);
      stgwrite(" ");
      k++;
      j--;
      if (j==0 || k>=litidx)
        stgwrite("\n");         /* force a newline after 10 dumps */
      /* Note: stgwrite() buffers a line until it is complete. It recognizes
       * the end of line as a sequence of "\n\0", so something like "\n\t"
       * so should not be passed to stgwrite().
       */
    } /* while */
  } /* while */
}

/*  dumpzero
 *  Dump zero's for default initial values of elements of global arrays.
 *  Elements of local arrays do not need to be dumped, because they are
 *  allocated on the stack (and the stack is cleared when creating a new
 *  variable).
 */
static void dumpzero(int count)
{
  int i,cellsperline;

  if (sc_status==statSKIP || count<=0)
    return;
  assert(curseg==2);
  cellsperline=32/pc_cellsize;
  defstorage();
  i=0;
  while (count-- > 0) {
    outval(0,TRUE,FALSE);
    i=(i+1) % cellsperline;
    stgwrite((i==0 || count==0) ? "\n" : " ");
    if (i==0 && count>0)
      defstorage();
  } /* while */
}

static void aligndata(int numbytes)
{
  assert(numbytes % pc_cellsize == 0);  /* alignment must be a multiple of
                                         * the cell size */
  assert(numbytes!=0);

  if ((((glb_declared+litidx)*pc_cellsize) % numbytes)!=0) {
    while ((((glb_declared+litidx)*pc_cellsize) % numbytes)!=0)
      litadd(0);
  } /* if */

}

static void declfuncvar(int fpublic,int fstatic,int fstock,int fconst)
{
  char name[sNAMEMAX+11];
  int tok,tag;
  char *str;
  cell val;
  int invalidfunc;

  tag=pc_addtag(NULL);
  tok=lex(&val,&str);
  /* if we arrived here, this may not be a declaration of a native function
   * or variable
   */
  if (tok==tNATIVE) {
    error(42);          /* invalid combination of class specifiers */
    return;
  } /* if */

  /* at this level, if the token is tEXIT, it is probably the declaration
   * of a state exist function, not the "exit" statement
   */
  if (tok==tEXIT)
    tok=lexsettoken(tSYMBOL,uEXITFUNC);
  if (tok!=tSYMBOL && tok!=tOPERATOR) {
    lexpush();
    needtoken(tSYMBOL);
    lexclr(TRUE);       /* drop the rest of the line */
    litidx=0;           /* drop the literal queue too */
    return;
  } /* if */
  if (tok==tOPERATOR) {
    lexpush();          /* push "operator" keyword back (for later analysis) */
    if (!newfunc(NULL,tag,fpublic,fstatic,fstock)) {
      error(10);        /* illegal function or declaration */
      lexclr(TRUE);     /* drop the rest of the line */
      litidx=0;         /* drop the literal queue too */
    } /* if */
  } else {
    /* so tok is tSYMBOL */
    assert(strlen(str)<=sNAMEMAX);
    strcpy(name,str);
    /* only variables can be "const" or both "public" and "stock" */
    invalidfunc= fconst || (fpublic && fstock);
    if (invalidfunc || !newfunc(name,tag,fpublic,fstatic,fstock)) {
      /* if not a function, try a global variable */
      declglb(name,tag,fpublic,fstatic,fstock,fconst);
    } /* if */
  } /* if */
}

/*  declglb     - declare global symbols
 *
 *  Declare a static (global) variable. Global variables are stored in
 *  the DATA segment.
 *
 *  global references: glb_declared     (altered)
 */
static void declglb(char *firstname,int firsttag,int fpublic,int fstatic,int fstock,int fconst)
{
  int ident,tag,ispublic,usage;
  char name[sNAMEMAX+1];
  cell val,size,cidx;
  ucell address;
  int glb_incr;
  char *str;
  int dim[sDIMEN_MAX];
  int numdim;
  constvalue *dimnames[sDIMEN_MAX];
  short filenum;
  symbol *sym;
  #if !defined NDEBUG
    cell glbdecl=0;
  #endif

  assert(!fpublic || !fstatic);         /* may not both be set */
  insert_docstring_separator();         /* see comment in newfunc() */
  filenum=fcurrent;                     /* save file number at the start of the declaration */
  do {
    #if !defined PAWN_LIGHT
      pc_docstring_suspended=TRUE;      /* suspend attaching documentation to global/recent blocks */
    #endif
    size=1;                             /* single size (no array) */
    numdim=0;                           /* no dimensions */
    ident=iVARIABLE;
    if (firstname!=NULL) {
      assert(strlen(firstname)<=sNAMEMAX);
      strcpy(name,firstname);           /* save symbol name */
      tag=firsttag;
      firstname=NULL;
    } else {
      tag=pc_addtag(NULL);
      if (lex(&val,&str)!=tSYMBOL) {    /* read in (new) token */
        error(20,str);                  /* invalid symbol name */
        litidx=0;                       /* clear literal pool (in case there is a string constant instead of a symbol name) */
      } /* if */
      assert(strlen(str)<=sNAMEMAX);
      strcpy(name,str);                 /* save symbol name */
    } /* if */
    ispublic=fpublic;
    if (name[0]==PUBLIC_CHAR) {
      ispublic=TRUE;                    /* implicitly public variable */
      assert(!fstatic);
    } /* if */
    while (matchtoken('[')) {
      ident=iARRAY;
      if (numdim==sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return;
      } /* if */
      size=needsub(']',&dimnames[numdim]); /* get size; size==0 for "var[]" */
      #if INT_MAX < LONG_MAX
        if (size>INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      dim[numdim++]=(int)size;
    } /* while */
    usage=0;
    if (matchtoken('{')) {              /* last dimension may be specified as {} */
      ident=iARRAY;
      usage=uPACKED;
      if (numdim==sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return;
      } /* if */
      size=needsub('}',&dimnames[numdim]);/* get size; size==0 for "var{}" */
      size=((size*sCHARBITS/8)+pc_cellsize-1) / pc_cellsize;
      #if INT_MAX < LONG_MAX
        if (size>INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      dim[numdim++]=(int)size;
    } /* if */
    if (matchtoken('['))
      error(51);                        /* {} must be last */
    verify_array_namelist(dimnames,numdim);
    if (ispublic && ident==iARRAY)
      error(56,name);                   /* arrays cannot be public */
    assert(sc_curstates==0);
    sc_curstates=getstates(name);
    if (sc_curstates<0) {
      error(85,name);           /* empty state list on declaration */
      sc_curstates=0;
    } else if (sc_curstates>0 && ispublic) {
      error(88,name);           /* public variables may not have states */
      sc_curstates=0;
    } /* if */
    sym=findconst(name);
    if (sym==NULL) {
      sym=findglb(name,sSTATEVAR);
      /* if a global variable without states is found and this declaration has
       * states, the declaration is okay
       */
      if (sym!=NULL && sym->states==NULL && sc_curstates>0)
        sym=NULL;               /* set to NULL, we found the global variable */
      if (sc_curstates>0 && findglb(name,sGLOBAL)!=NULL)
        error(233,name);        /* state variable shadows a global variable */
    } /* if */
    /* we have either:
     * a) not found a matching variable (or rejected it, because it was a shadow)
     * b) found a global variable and we were looking for that global variable
     * c) found a state variable in the automaton that we were looking for
     */
    assert(sym==NULL
           || sym->states==NULL && sc_curstates==0
           || sym->states!=NULL && sym->next!=NULL && sym->states->next->id==sc_curstates);
    /* a state variable may only have a single id in its list (so either this
     * variable has no states, or it has a single list)
     */
    assert(sym==NULL || sym->states==NULL || sym->states->next->next==NULL);
    /* it is okay for the (global) variable to exist, as long as it belongs to
     * a different automaton
     */
    if (sym!=NULL && (sym->usage & uDEFINE)!=0)
      error(21,name);                   /* symbol already defined */
    /* if this variable is never used (which can be detected only in the
     * second stage), shut off code generation
     */
    cidx=0;             /* only to avoid a compiler warning */
    if (sc_status==statWRITE && sym!=NULL && (sym->usage & (uREAD | uWRITTEN))==0) {
      sc_status=statSKIP;
      cidx=code_idx;
      #if !defined NDEBUG
        glbdecl=glb_declared;
      #endif
    } /* if */
    begdseg();          /* real (initialized) data in data segment */
    assert(litidx==0);  /* literal queue should be empty */
    if (sc_alignnext) {
      litidx=0;
      aligndata(sc_dataalign);
      dumplits();       /* dump the literal queue */
      sc_alignnext=FALSE;
      litidx=0;         /* global initial data is dumped, so restart at zero */
    } /* if */
    assert(litidx==0);  /* literal queue should be empty (again) */
    initials(ident,usage,tag,&size,dim,numdim,dimnames);/* stores values in the literal queue */
    assert(size>=litidx);
    if (numdim==1)
      dim[0]=(int)size;
    /* before dumping the initial values (or zeros) check whether this variable
     * overlaps another
     */
    if (sc_curstates>0) {
      unsigned char *map;

      if (litidx!=0)
        error(89,name); /* state variables may not be initialized */
      /* find an appropriate address for the state variable */
      /* assume that it cannot be found */
      address=pc_cellsize*glb_declared;
      glb_incr=(int)size;
      /* use a memory map in which every cell occupies one bit */
      if (glb_declared>0 && (map=(unsigned char*)malloc((size_t)(glb_declared+7)/8))!=NULL) {
        int fsa=state_getfsa(sc_curstates);
        symbol *sweep;
        cell sweepsize,addr;
        memset(map,0,(size_t)(glb_declared+7)/8);
        assert(fsa>=0);
        /* fill in all variables belonging to this automaton */
        for (sweep=glbtab.next; sweep!=NULL; sweep=sweep->next) {
          if (sweep->parent!=NULL || sweep->states==NULL || sweep==sym)
            continue;   /* hierarchical type, or no states, or same as this variable */
          if (sweep->ident!=iVARIABLE && sweep->ident!=iARRAY)
            continue;   /* a function or a constant */
          if ((sweep->usage & uDEFINE)==0)
            continue;   /* undefined variable, ignore */
          if (fsa!=state_getfsa(sweep->states->next->id))
            continue;   /* wrong automaton */
          /* when arrived here, this is a global variable, with states and
           * belonging to the same automaton as the variable we are declaring
           */
          sweepsize=(sweep->ident==iVARIABLE) ? 1 : array_totalsize(sweep);
          assert(sweep->addr % pc_cellsize == 0);
          addr=sweep->addr/pc_cellsize;
          /* mark this address range */
          while (sweepsize-->0) {
            map[addr/8] |= (unsigned char)(1 << (addr % 8));
            addr++;
          } /* while */
        } /* for */
        /* go over it again, clearing any ranges that have conflicts */
        for (sweep=glbtab.next; sweep!=NULL; sweep=sweep->next) {
          if (sweep->parent!=NULL || sweep->states==NULL || sweep==sym)
            continue;   /* hierarchical type, or no states, or same as this variable */
          if (sweep->ident!=iVARIABLE && sweep->ident!=iARRAY)
            continue;   /* a function or a constant */
          if ((sweep->usage & uDEFINE)==0)
            continue;   /* undefined variable, ignore */
          if (fsa!=state_getfsa(sweep->states->next->id))
            continue;   /* wrong automaton */
          /* when arrived here, this is a global variable, with states and
           * belonging to the same automaton as the variable we are declaring
           */
          /* if the lists of states of the existing variable and the new
           * variable have a non-empty intersection, this is not a suitable
           * overlap point -> wipe the address range
           */
          if (state_conflict_id(sc_curstates,sweep->states->next->id)) {
            sweepsize=(sweep->ident==iVARIABLE) ? 1 : array_totalsize(sweep);
            assert(sweep->addr % pc_cellsize == 0);
            addr=sweep->addr/pc_cellsize;
            /* mark this address range */
            while (sweepsize-->0) {
              map[addr/8] &= (unsigned char)(~(1 << (addr % 8)));
              addr++;
            } /* while */
          } /* if */
        } /* for */
        /* now walk through the map and find a starting point that is big enough */
        sweepsize=0;
        for (addr=0; addr<glb_declared; addr++) {
          if ((map[addr/8] & (1 << (addr % 8)))==0)
            continue;
          for (sweepsize=addr+1; sweepsize<glb_declared; sweepsize++) {
            if ((map[sweepsize/8] & (1 << (sweepsize % 8)))==0)
              break;    /* zero bit found, skip this range */
            if (sweepsize-addr>=size)
              break;    /* fitting range found, no need to search further */
          } /* for */
          if (sweepsize-addr>=size)
            break;      /* fitting range found, no need to search further */
          addr=sweepsize;
        } /* for */
        free(map);
        if (sweepsize-addr>=size) {
          address=pc_cellsize*addr;     /* fitting range found, set it */
          glb_incr=0;
        } /* if */
      } /* if */
    } else {
      address=pc_cellsize*glb_declared;
      glb_incr=(int)size;
    } /* if */
    if (address==(ucell)pc_cellsize*glb_declared) {
      dumplits();       /* dump the literal queue */
      dumpzero((int)size-litidx);
    } /* if */
    litidx=0;
    if (strlen(name)==0)
      continue;
    if (sym==NULL) {    /* define only if not yet defined */
      sym=addvariable(name,address,ident,sGLOBAL,tag,dim,dimnames,numdim,usage);
      sc_attachdocumentation(sym,TRUE);/* attach any documenation to the variable */
      if (sc_curstates>0)
        attachstatelist(sym,sc_curstates);
    } else {            /* if declared but not yet defined, adjust the variable's address */
      int idx;
      assert(sym->states==NULL && sc_curstates==0
             || sym->states->next!=NULL && sym->states->next->id==sc_curstates && sym->states->next->next==NULL);
      sym->addr=address;
      sym->codeaddr=code_idx;
      sym->usage|=uDEFINE;
      for (idx=0; idx<numdim; idx++) {
        if (dimnames[idx]!=NULL) {
          delete_consttable(dimnames[idx]);
          free(dimnames[idx]);
        } /* if */
      } /* for */
    } /* if */
    assert(sym!=NULL);
    sc_curstates=0;
    if (ispublic)
      sym->usage|=uPUBLIC;
    if (fconst)
      sym->usage|=uCONST;
    if (fstock)
      sym->usage|=uSTOCK;
    if (fstatic)
      sym->fvisible=filenum;
    if (sc_status==statSKIP) {
      sc_status=statWRITE;
      code_idx=cidx;
      assert(glb_declared==glbdecl);
    } else {
      glb_declared+=glb_incr;   /* add total number of cells (if added to the end) */
    } /* if */
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);    /* if not comma, must be semicolon */
}

/*  declloc     - declare local symbols
 *
 *  Declare local (automatic) variables. Since these variables are relative
 *  to the STACK, there is no switch to the DATA segment. These variables
 *  cannot be initialized either.
 *
 *  global references: declared   (altered)
 *                     funcstatus (referred to only)
 */
static int declloc(int fstatic)
{
  int ident,tag,usage;
  char name[sNAMEMAX+1];
  symbol *sym;
  cell val,size;
  char *str;
  value lval = {0};
  int cur_lit=0;
  int dim[sDIMEN_MAX];
  int numdim;
  constvalue *dimnames[sDIMEN_MAX];
  int fconst;
  int staging_start;

  fconst=matchtoken(tCONST);
  do {
    ident=iVARIABLE;
    size=1;
    numdim=0;                           /* no dimensions */
    tag=pc_addtag(NULL);
    if (!needtoken(tSYMBOL)) {
      lexclr(TRUE);                     /* drop the rest of the line... */
      litidx=0;                         /* ...clear literal pool... */
      return 0;                         /* ...and quit */
    } /* if */
    tokeninfo(&val,&str);
    assert(strlen(str)<=sNAMEMAX);
    strcpy(name,str);                   /* save symbol name */
    if (name[0]==PUBLIC_CHAR)
      error(56,name);                   /* local variables cannot be public */
    /* Note: block locals may be named identical to locals at higher
     * compound blocks (as with standard C); so we must check (and add)
     * the "nesting level" of local variables to verify the
     * multi-definition of symbols.
     */
    if ((sym=findloc(name))!=NULL && sym->compound==nestlevel)
      error(21,name);                   /* symbol already defined */
    /* Although valid, a local variable whose name is equal to that
     * of a global variable or to that of a local variable at a lower
     * level might indicate a bug.
     */
    if (((sym=findloc(name))!=NULL && sym->compound!=nestlevel || findglb(name,sGLOBAL)!=NULL) && !undefined_vars)
      error(219,name);                  /* variable shadows another symbol (only give the warning when there are no errors) */
    while (matchtoken('[')) {
      ident=iARRAY;
      if (numdim == sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return ident;
      } /* if */
      size=needsub(']',&dimnames[numdim]);/* get size; size==0 for "var[]" */
      #if INT_MAX < LONG_MAX
        if (size > INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      dim[numdim++]=(int)size;
    } /* while */
    usage=0;
    if (matchtoken('{')) {              /* final dimension may also be specified with {} */
      ident=iARRAY;
      usage=uPACKED;
      if (numdim == sDIMEN_MAX) {
        error(53);                      /* exceeding maximum number of dimensions */
        return ident;
      } /* if */
      size=needsub('}',&dimnames[numdim]);/* get size; size==0 for "var{}" */
      size=((size*sCHARBITS/8)+pc_cellsize-1) / pc_cellsize;
      #if INT_MAX < LONG_MAX
        if (size > INT_MAX)
          error(105);                   /* overflow, exceeding capacity */
      #endif
      dim[numdim++]=(int)size;
    } /* if */
    if (matchtoken('['))
      error(51);                /* {} must be last */
    verify_array_namelist(dimnames,numdim);
    if (getstates(name))
      error(88,name);           /* local variables may not have states */
    if (ident==iARRAY || fstatic) {
      if (sc_alignnext) {
        aligndata(sc_dataalign);
        sc_alignnext=FALSE;
      } /* if */
      cur_lit=litidx;           /* save current index in the literal table */
      initials(ident,usage,tag,&size,dim,numdim,dimnames);
      if (size==0)
        return ident;           /* error message already given */
      if (numdim==1)
        dim[0]=(int)size;
    } /* if */
    /* reserve memory (on the stack) for the variable */
    if (fstatic) {
      /* write zeros for uninitialized fields */
      while (litidx<cur_lit+size)
        litadd(0);
      sym=addvariable(name,(cur_lit+glb_declared)*pc_cellsize,ident,sSTATIC,
                      tag,dim,dimnames,numdim,usage);
    } else {
      declared+=(int)size;      /* variables are put on stack, adjust "declared" */
      sym=addvariable(name,-declared*pc_cellsize,ident,sLOCAL,tag,
                      dim,dimnames,numdim,usage);
      if (ident==iVARIABLE) {
        assert(!staging);
        stgset(TRUE);           /* start stage-buffering */
        assert(stgidx==0);
        staging_start=stgidx;
      } /* if */
      markexpr(sLDECL,name,-declared*pc_cellsize); /* mark for better optimization */
      modstk(-(int)size*pc_cellsize);
      assert(curfunc!=NULL);
      assert((curfunc->usage & uNATIVE)==0);
      if (curfunc->x.stacksize<declared+1)
        curfunc->x.stacksize=(long)declared+1;  /* +1 for PROC opcode */
    } /* if */
    /* now that we have reserved memory for the variable, we can proceed
     * to initialize it */
    assert(sym!=NULL);          /* we declared it, it must be there */
    sym->compound=nestlevel;    /* for multiple declaration/shadowing check */
    if (fconst)
      sym->usage|=uCONST;
    if (!fstatic) {             /* static variables already initialized */
      if (ident==iVARIABLE) {
        /* simple variable, also supports initialization */
        int ctag = tag;         /* set to "tag" by default */
        int explicit_init=FALSE;/* is the variable explicitly initialized? */
        if (matchtoken('=')) {
          doexpr(FALSE,FALSE,FALSE,FALSE,&ctag,NULL,TRUE);
          explicit_init=TRUE;
        } else {
          ldconst(0,sPRI);      /* uninitialized variable, set to zero */
        } /* if */
        /* now try to save the value (still in PRI) in the variable */
        lval.sym=sym;
        lval.ident=iVARIABLE;
        lval.constval=0;
        lval.tag=tag;
        check_userop(NULL,ctag,lval.tag,2,NULL,&ctag);
        store(&lval);
        markexpr(sEXPR,NULL,0); /* full expression ends after the store */
        assert(staging);        /* end staging phase (optimize expression) */
        stgout(staging_start);
        stgset(FALSE);
        if (!matchtag(tag,ctag,TRUE))
          error(213);           /* tag mismatch */
        /* if the variable was not explicitly initialized, reset the
         * "uWRITTEN" flag that store() set */
        if (!explicit_init)
          sym->usage &= ~uWRITTEN;
      } else {
        /* an array */
        assert(cur_lit>=0 && cur_lit<=litidx && litidx<=litmax);
        assert(size>0 && size>=sym->dim.array.length);
        assert(numdim>1 || size==sym->dim.array.length);
        /* final literal values that are zero make no sense to put in the literal
         * pool, because values get zero-initialized anyway; we check for this,
         * because users often explicitly initialize strings to ""
         */
        while (litidx>cur_lit && litq[litidx-1]==0)
          litidx--;
        /* if the array is not completely filled, set all values to zero first */
        if (litidx-cur_lit<size && (ucell)size<CELL_MAX)
          fillarray(sym,size*pc_cellsize,0);
        if (cur_lit<litidx) {
          /* check whether the complete array is set to a single value; if
           * it is, more compact code can be generated */
          cell first=litq[cur_lit];
          int i;
          for (i=cur_lit; i<litidx && litq[i]==first; i++)
            /* nothing */;
          if (i==litidx) {
            /* all values are the same */
            fillarray(sym,(litidx-cur_lit)*pc_cellsize,first);
            litidx=cur_lit;     /* reset literal table */
          } else {
            /* copy the literals to the array */
            ldconst((cur_lit+glb_declared)*pc_cellsize,sPRI);  /* PRI = source */
            address(sym,sALT);                                  /* ALT = dest */
            sym->usage &= ~uREAD; /* clear this flag that address() implicitly sets */
            memcopy((litidx-cur_lit)*pc_cellsize);
          } /* if */
          markusage(sym,uWRITTEN);
        } /* if */
      } /* if */
    } /* if */
  } while (matchtoken(',')); /* enddo */   /* more? */
  needtoken(tTERM);    /* if not comma, must be semicolon */
  return ident;
}

/* this function returns the maximum value for a cell in case of an error
 * (invalid dimension).
 */
static cell calc_arraysize(int dim[],int numdim,int cur)
{
  #if UINT_MAX > 0xffffffffLu
    #define ARRAY_MAX 0xffffffffLu  /* array is max. 4 GiB */
  #else
    #define ARRAY_MAX UINT_MAX
  #endif
  cell subsize;
  ucell newsize;

  /* the return value is in cells, not bytes */
  assert(cur>=0 && cur<=numdim);
  if (cur==numdim)
    return 0;
  subsize=calc_arraysize(dim,numdim,cur+1);
  newsize=dim[cur]+dim[cur]*subsize;
  if (newsize==0)
    return 0;
  if ((ucell)subsize>=ARRAY_MAX || newsize>=ARRAY_MAX || newsize<(ucell)subsize
      || newsize*pc_cellsize>=ARRAY_MAX)
    return CELL_MAX;
  return newsize;
  #undef ARRAY_MAX
}

static cell adjust_indirectiontables(int dim[],int numdim,int cur,cell increment,
                                     int startlit,constvalue *lastdim,int *skipdim)
{
static int base;
  int d;
  cell accum;

  assert(cur>=0 && cur<numdim);
  assert(increment>=0);
  assert(cur>0 && startlit==-1 || startlit>=0 && startlit<=litidx);
  if (cur==0)
    base=startlit;
  if (cur==numdim-1)
    return 0;
  /* 2 or more dimensions left, fill in an indirection vector */
  assert(dim[cur]>0);
  if (dim[cur+1]>0) {
    for (d=0; d<dim[cur]; d++)
      litq[base++]=(dim[cur]+d*(dim[cur+1]-1)+increment)*pc_cellsize;
    accum=dim[cur]*(dim[cur+1]-1);
  } else {
    /* final dimension is variable length */
    constvalue *ld;
    assert(dim[cur+1]==0);
    assert(lastdim!=NULL);
    assert(skipdim!=NULL);
    accum=0;
    /* skip the final dimension sizes for all earlier major dimensions */
    for (d=0,ld=lastdim->next; d<*skipdim; d++,ld=ld->next) {
      assert(ld!=NULL);
    } /* for */
    for (d=0; d<dim[cur]; d++) {
      assert(ld!=NULL);
      assert(strtol(ld->name,NULL,16)==d);
      litq[base++]=(dim[cur]+accum+increment)*pc_cellsize;
      accum+=ld->value-1;
      *skipdim+=1;
      ld=ld->next;
    } /* for */
  } /* if */
  /* create the indirection tables for the lower level */
  if (cur+2<numdim) {   /* are there at least 2 dimensions below this one? */
    increment+=(dim[cur]-1)*dim[cur+1]; /* this many indirection tables follow */
    for (d=0; d<dim[cur]; d++)
      increment+=adjust_indirectiontables(dim,numdim,cur+1,increment,-1,lastdim,skipdim);
  } /* if */
  return accum;
}

/*  initials
 *
 *  Initialize global objects and local arrays.
 *
 *  ident   a combination of the kind of variable (array, simple variable)
 *  usage   flags (only the uPACKED flag is used)
 *  size    array cells (count), if 0 on input, the routine counts the number of elements
 *  tag     required tagname for the initial value
 *
 *  Global references: litidx (altered)
 */
static void initials(int ident,int usage,int tag,cell *size,int dim[],int numdim,
                     constvalue *dimnames[])
{
  int ctag;
  cell tablesize;
  int curlit=litidx;
  int err=0;

  if (!matchtoken('=')) {
    assert(ident!=iARRAY || numdim>0);
    if (ident==iARRAY && dim[numdim-1]==0) {
      /* declared as "myvar[];" which is senseless (note: this *does* make
       * sense in the case of a iREFARRAY, which is a function parameter)
       */
      error(9);         /* array has zero length -> invalid size */
    } /* if */
    if (ident==iARRAY) {
      assert(numdim>0 && numdim<=sDIMEN_MAX);
      *size=calc_arraysize(dim,numdim,0);
      if (*size==(cell)CELL_MAX) {
        error(9);       /* array is too big -> invalid size */
        return;
      } /* if */
      /* first reserve space for the indirection vectors of the array, then
       * adjust it to contain the proper values
       * (do not use dumpzero(), as it bypasses the literal queue)
       */
      for (tablesize=calc_arraysize(dim,numdim-1,0); tablesize>0; tablesize--)
        litadd(0);
      if (dim[numdim-1]!=0)     /* error 9 has already been given */
        adjust_indirectiontables(dim,numdim,0,0,curlit,NULL,NULL);
    } /* if */
    return;
  } /* if */

  if (ident==iVARIABLE) {
    assert(*size==1);
    init(ident,usage,&ctag,NULL,NULL,NULL);
    if (!matchtag(tag,ctag,TRUE))
      error(213);       /* tag mismatch */
  } else {
    assert(numdim>0);
    if (numdim==1) {
      *size=initvector(ident,usage,tag,dim[0],FALSE,dimnames[0],NULL);
    } else {
      int errorfound=FALSE;
      int counteddim[sDIMEN_MAX];
      int idx;
      constvalue lastdim={NULL,"",0,0};     /* sizes of the final dimension */
      int skipdim=0;

      if (dim[numdim-1]!=0)
        *size=calc_arraysize(dim,numdim,0); /* calc. full size, if known */
      /* initialize the sub-arrays */
      memset(counteddim,0,sizeof counteddim);
      initarray(ident,usage,tag,dim,numdim,0,curlit,counteddim,&lastdim,dimnames,&errorfound);
      /* check the specified array dimensions with the initialler counts */
      for (idx=0; idx<numdim-1; idx++) {
        if (dim[idx]==0) {
          dim[idx]=counteddim[idx];
        } else if (counteddim[idx]<dim[idx]) {
          error(52);            /* array is not fully initialized */
          err++;
        } else if (counteddim[idx]>dim[idx]) {
          error(18);            /* initialization data exceeds declared size */
          err++;
        } /* if */
      } /* for */
      if (numdim>1 && dim[numdim-1]==0 && !errorfound && err==0) {
        /* also look whether, by any chance, all "counted" final dimensions are
         * the same value; if so, we can store this
         */
        constvalue *ld=lastdim.next;
        int d,match;
        for (d=0; d<dim[numdim-2]; d++) {
          assert(ld!=NULL);
          assert(strtol(ld->name,NULL,16)==d);
          if (d==0)
            match=(int)ld->value;
          else if (match!=ld->value)
            break;
          ld=ld->next;
        } /* for */
        if (d>0 && d==dim[numdim-2])
          dim[numdim-1]=match;
      } /* if */
      /* after all arrays have been initalized, we know the (major) dimensions
       * of the array and we can properly adjust the indirection vectors
       */
      if (err==0)
        adjust_indirectiontables(dim,numdim,0,0,curlit,&lastdim,&skipdim);
      delete_consttable(&lastdim);  /* clear list of minor dimension sizes */
    } /* if */
  } /* if */

  if (*size==0)
    *size=litidx-curlit;        /* number of elements defined */
}

static cell initarray(int ident,int usage,int tag,int dim[],int numdim,int cur,
                      int startlit,int counteddim[],constvalue *lastdim,
                      constvalue *dimnames[],int *errorfound)
{
  cell dsize,totalsize;
  int idx,abortparse;

  assert(cur>=0 && cur<numdim);
  assert(startlit>=0);
  assert(cur+2<=numdim);/* there must be 2 dimensions or more to do */
  assert(errorfound!=NULL && *errorfound==FALSE);
  /* check for a quick exit */
  if (matchtoken(']')) {
    lexpush();
    return 0;
  } /* if */
  totalsize=0;
  needtoken('[');
  for (idx=0,abortparse=FALSE; !abortparse; idx++) {
    /* We need to store the offset to the newly detected sub-array into the
     * indirection table; i.e. this table needs to be expanded and updated.
     * In the current design, the indirection vectors for a multi-dimensional
     * array are adjusted after parsing all initiallers. Hence, it is only
     * necessary at this point to reserve space for an extra cell in the
     * indirection vector.
     */
    litinsert(0,startlit);
    if (cur+2<numdim) {
      dsize=initarray(ident,usage,tag,dim,numdim,cur+1,startlit,counteddim,
                      lastdim,dimnames,errorfound);
    } else {
      dsize=initvector(ident,usage,tag,dim[cur+1],TRUE,dimnames[cur+1],errorfound);
      /* The final dimension may be variable length. We need to save the
       * lengths of the final dimensions in order to set the indirection
       * vectors for the next-to-last dimension.
       */
      append_constval(lastdim,itoh(idx),dsize,0);
    } /* if */
    /* In a declaration like:
     *    new a[2][2] = [
     *                    [1, 2],
     *                    [3, 4],
     *                  ]
     * the final trailing comma (after the "4}") makes this loop attempt to
     * parse one more initialization vector, but initvector() (and a recursive
     * call to initarray()) quits with a size of zero while not setting
     * "errorfound". Then, we must exit this loop without incrementing the
     * dimension count.
     */
    if (dsize==0 && !*errorfound)
      break;
    if (dim[cur]!=0 && idx>=dim[cur]) {
      assert(dsize>0 || *errorfound);
      error(18);            /* initialization data exceeds array size */
      break;                /* avoid incrementing "idx" */
    } /* if */
    totalsize+=dsize;
    if (*errorfound || !matchtoken(','))
      abortparse=TRUE;
  } /* for */
  needtoken(']');
  assert(counteddim!=NULL);
  if (counteddim[cur]>0) {
    if (idx<counteddim[cur])
      error(52);                /* array is not fully initialized */
    else if (idx>counteddim[cur])
      error(18);                /* initialization data exceeds declared size */
  } /* if */
  counteddim[cur]=idx;

  return totalsize+dim[cur];    /* size of sub-arrays + indirection vector */
}

/*  initvector
 *  Initialize a single dimensional array
 */
static cell initvector(int ident,int usage,int tag,cell size,int fillzero,
                       constvalue *dimnames,int *errorfound)
{
  cell prev1=0,prev2=0;
  int ellips=FALSE;
  int curlit=litidx;
  int ctag,ftag;
  int match=0;
  int packcount=-1;
  cell packitem=0;

  assert(ident==iARRAY || ident==iREFARRAY);
  if (matchtoken('[')) {
    match=']';
    if (usage & uPACKED)
      error(229);
  } else if (matchtoken('{')) {
    match='}';
    packcount=0;        /* this enables item packing */
    if ((usage & uPACKED)==0)
      error(229);
  } /* if */
  if (match!=0) {
    constvalue *field=(dimnames!=NULL) ? dimnames->next : NULL;
    if (match=='}' && field!=NULL)
      error(51);        /* error (only unpacked arrays can use symbolic indices) */
    do {
      int fieldlit=litidx;
      int matchbrace,fieldusage,i;
      if (matchtoken(match)) {  /* to allow for trailing ',' after the initialization */
        lexpush();
        break;
      } /* if */
      if ((ellips=matchtoken(tELLIPS))!=0)
        break;
      /* for symbolic subscripts, allow another level of braces ("[...]" or "{...}") */
      matchbrace=0;             /* preset */
      fieldusage=usage;
      ellips=0;
      if (field!=NULL) {
        if (matchtoken('['))
          matchbrace=']';
        else if (matchtoken('{'))
          matchbrace='}';
        fieldusage=field->usage;
      } /* if */
      ftag=-1;
      for ( ;; ) {
        prev2=prev1;
        prev1=init(ident,fieldusage,&ctag,errorfound,&packcount,&packitem);
        if (ftag!=-1 && ftag!=ctag)
          error(213);     /* tag mismatch */
        ftag=ctag;
        if (!matchbrace)
          break;
        if ((ellips=matchtoken(tELLIPS))!=0)
          break;
        if (!matchtoken(',')) {
          needtoken(matchbrace);
          break;
        } /* for */
      } /* for */
      /* if this array array field is a pseudo-array, fill the "field" up with
       * zeros, and toggle the tag
       */
      if (dimnames!=NULL && (field==NULL || field->name[0]=='\0'))
        error(227);             /* more initiallers than array fields */
      if (field!=NULL && field->name[0]!='\0') {
        cell step,val,size;
        assert(field->next!=NULL);
        size=(field->next->value - field->value);
        assert(fieldlit<litidx || packcount>0);
        if (litidx-fieldlit>size)
          error(228);           /* length of initialler exceeds size of the array field */
        if (ellips) {
          val=prev1;
          step=prev1-prev2;
        } else {
          step=0;
          val=0;                /* fill up with zeros */
        } /* if */
        for (i=litidx-fieldlit; i<size; i++) {
          if (packcount>=0) {
            assert(packcount<pc_cellsize);
            do {
              val+=step;
              packcount+=1;
              packitem|=(val & 0xff) << ((pc_cellsize-packcount)*sCHARBITS);
            } while (packcount<pc_cellsize);
            litadd(packitem);    /* store collected values in literal table */
            packitem=0;
            packcount=0;
          } else {
            val+=step;
            litadd(val);
          } /* if */
        } /* for */
        ftag=field->index;
        field=field->next;
      } else {
        ftag=tag;
      } /* if */
      if (!matchtag(ftag,ctag,TRUE))
        error(213);             /* tag mismatch */
    } while (matchtoken(','));
    if (packcount!=0 && match=='}' && !ellips)
      litadd(packitem);         /* store final collected values */
    needtoken(match);
  } else if (matchtoken(']')) {
    /* this may be caused by a trailing comma in a declaration of a
     * multi-dimensional array
     */
    lexpush();                  /* push back for later analysis */
    size=0;                     /* avoid zero filling */
    assert(!ellips);
  } else {
    init(ident,usage,&ctag,errorfound,NULL,NULL);
    if (!matchtag(tag,ctag,TRUE))
      error(213);               /* tagname mismatch */
  } /* if */
  /* fill up the literal queue with a series */
  if (ellips) {
    cell step=((litidx-curlit)==1) ? (cell)0 : prev1-prev2;
    if (size==0 || (litidx-curlit)==0 && packcount<=0)
      error(41);                /* invalid ellipsis, array size unknown */
    else if ((litidx-curlit)==(int)size)
      error(18);                /* initialisation data exceeds declared size */
    while ((litidx-curlit)<(int)size) {
      if (packcount>=0) {
        assert(packcount<pc_cellsize);
        do {
          prev1+=step;
          packcount+=1;
          packitem|=(prev1 & 0xff) << ((pc_cellsize-packcount)*sCHARBITS);
        } while (packcount<pc_cellsize);
        litadd(packitem);    /* store collected values in literal table */
        packitem=0;
        packcount=0;
      } else {
        prev1+=step;
        litadd(prev1);
      } /* if */
    } /* while */
  } /* if */
  if (fillzero && size>0) {
    while ((litidx-curlit)<(int)size)
      litadd(0);
  } /* if */
  if (size==0) {
    size=litidx-curlit;                 /* number of elements defined */
  } else if (litidx-curlit>(int)size) { /* e.g. "myvar[3]={1,2,3,4};" */
    error(18);                  /* initialisation data exceeds declared size */
    litidx=(int)size+curlit;    /* avoid overflow in memory moves */
  } /* if */
  return size;
}

/*  init
 *
 *  Evaluate one initializer.
 */
static cell init(int ident,int usage,int *tag,int *errorfound,int *packcount,cell *packitem)
{
  cell val;
  char *str;
  int tok;
  int curlit=litidx;
  cell i = 0;

  tok=lex(&val,&str);
  if (tok!=tSTRING && tok!=tPACKSTRING)
    lexpush();
  if (tok==tSTRING || tok==tPACKSTRING) {
    /* lex() automatically stores strings in the literal table (and
     * increases "litidx")
     */
    if (ident==iVARIABLE) {
      error(6);         /* must be assigned to an array */
      litidx=curlit+1;  /* reset literal queue */
    } /* if */
    if (tok==tSTRING && (usage & uPACKED)!=0 || tok==tPACKSTRING && (usage & uPACKED)==0)
      error(229);
    *tag=0;
  } else if (constexpr(&i,tag,NULL)) {
    if (packcount!=NULL && *packcount>=0) {
      assert(packitem!=NULL);
      assert(*packcount<pc_cellsize);
      if ((ucell)i>=(ucell)(1 << sCHARBITS))
        error(43,(long)i);    /* constant exceeds range */
      *packcount+=1;
      *packitem|=(i & 0xff) << ((pc_cellsize-*packcount)*sCHARBITS);
      if (*packcount==pc_cellsize) {
        litadd(*packitem);    /* store collected values in literal table */
        *packitem=0;
        *packcount=0;
      } /* if */
    } else {
      litadd(i);        /* store expression result in literal table */
    } /* if */
  } else {
    if (errorfound!=NULL)
      *errorfound=TRUE;
  } /* if */
  return i;
}

/*  needsub
 *
 *  Get required array size
 */
static cell needsub(char match,constvalue **namelist)
{
  cell val;
  int tag;

  if (namelist!=NULL)
    *namelist=NULL;
  if (matchtoken(match))    /* we have already seen "[" (or "{") */
    return 0;               /* zero size (like "char msg[]") */
  tag=pc_addtag(NULL);
  if (namelist!=NULL && matchtoken(tSYMLABEL)) {
    /* this is a name list */
    char *str;
    constvalue *field;
    cell index=0;
    cell size;
    if (match!=']') {
      error(51);  /* named array fields only allowed for non-packed arrays */
      /* ignore everything until a matching token is found */
      do {
        lex(&val,&str);
      } while (freading && !matchtoken(match));
      return 0;
    } /* if */
    if ((*namelist=(constvalue*)malloc(sizeof(constvalue)))==NULL)
      error(103);
    memset(*namelist,0,sizeof(constvalue));
    for ( ;; ) {
      /* add the name that was already parsed */
      tokeninfo(&val,&str);
      assert(strlen(str)>0 && alphanum(str[0]));
      field=append_constval(*namelist,str,index,tag);
      /* get the size (parse optional '[' and '{') */
      if (matchtoken('[')) {
        size=needsub(']',NULL); /* get size; size==0 for "var[]" */
      } else if (matchtoken('{')) {
        field->usage=uPACKED;
        size=needsub('}',NULL); /* get size; size==0 for "var{}" */
        size=((size*sCHARBITS/8)+pc_cellsize-1) / pc_cellsize;
      } else {
        size=1;
      } /* if */
      #if INT_MAX < LONG_MAX
        if (size>INT_MAX)
          error(105);           /* overflow, exceeding capacity */
      #endif
      if (matchtoken('[') || matchtoken('{'))
        error(53);              /* exceeding maximum number of dimensions */
      if (size<=0)
        error(9);               /* invalid array size */
      index+=size;
      /* see whether more names follow */
      if (!matchtoken(','))
        break;
      if (matchtoken(match)) {
        /* closing brace/bracket following the comma, push back but no error */
        lexpush();
        break;
      } /* if */
      tag=pc_addtag(NULL);
      needtoken(tSYMLABEL);
    } /* for */
    /* add a sentinel, so that we know the size of the last symbol */
    field=append_constval(*namelist,"",index,0);
    val=index;
  } else {
    symbol *sym;
    int exprtag;
    constexpr(&val,&exprtag,&sym);/* get value (must be constant expression) */
    if (tag==0)
      tag=exprtag;            /* copy expression tag */
    if (val<0 || sc_rationaltag!=0 && tag==sc_rationaltag) {
      error(9);               /* negative array size is invalid; so is rational value */
      val=1;                  /* assume a valid array size */
    } /* if */
  } /* if */
  needtoken(match);

  return val;               /* return array size */
}

static void verify_array_namelist(constvalue *namelist[], int numdim)
{
  int dim;
  cell size;
  constvalue *field;

  /* only in the final dimension, an array may have fields with a size */
  for (dim=0; dim<numdim-1; dim++) {
    if (namelist[dim]==NULL)
      continue;
    assert(namelist[dim]->next!=NULL);
    for (field=namelist[dim]->next; field!=NULL && strlen(field->name)>0; field=field->next) {
      assert(field->next!=NULL);  /* there must be a sentinel */
      size=(field->next->value - field->value);
      if (size!=1)
        error(93);  /* array fields with a size may only appear in the final dimension */
    } /* for */
  } /* for */
}

/*  decl_const  - declare a single constant, or a list of enumerated
 *  constants
 */
static void decl_const(int vclass)
{
  char constname[sNAMEMAX+1];
  cell val,defaultval;
  char *str;
  int tag,defaulttag,exprtag;
  int symbolline;
  int enumerate,first;
  symbol *sym;

  insert_docstring_separator();         /* see comment in newfunc() */
  do {
    #if !defined PAWN_LIGHT
      pc_docstring_suspended=TRUE;      /* suspend attaching documentation to global/recent blocks */
    #endif
    defaulttag=pc_addtag(NULL);
    enumerate=matchtoken('{') ? ++pc_enumsequence : 0;
    first=1;
    defaultval=0;
    do {
      if (enumerate!=0 && matchtoken('}')) { /* quick exit if '}' follows ',' */
        lexpush();
        break;
      } /* if */
      if (enumerate==0 || (tag=pc_addtag(NULL))==0)
        tag=defaulttag;
      if (lex(&val,&str)!=tSYMBOL)      /* read in (new) token */
        error(20,str);                  /* invalid symbol name */
      symbolline=fline;                 /* save line where symbol was found */
      strcpy(constname,str);            /* save symbol name */
      val=defaultval+1;                 /* set default value and tag for next enumeration field */
      exprtag=tag;
      assert(enumerate!=0 || first);    /* when not enumerating, each constant is the "first" of the list */
      if (matchtoken('='))
        constexpr(&val,&exprtag,NULL);  /* get optional value */
      else if (first)
        error(91,str);                  /* first constant in a list must be initialized */
      first=0;
      defaultval=val;
      if (exprtag!=0 && !matchtag(tag,exprtag,FALSE)) {
        /* temporarily reset the line number to where the symbol was defined */
        int orgfline=fline;
        fline=symbolline;
        error(213);                     /* tagname mismatch */
        fline=orgfline;
      } /* if */
      /* add_constant() checks for duplicate definitions */
      sym=add_constant(constname,val,vclass,tag);
      if (sym!=NULL) {
        sc_attachdocumentation(sym,TRUE); /* attach any documenation to the constant */
        sym->x.enumlist=enumerate;
      } /* if */
    } while (enumerate!=0 && matchtoken(tSEPARATOR));
    if (enumerate!=0)
      needtoken('}');                   /* terminates the constant list */
  } while (matchtoken(','));            /* more? */
  needtoken(tTERM);
  matchtoken(';');                      /* eat an optional ; */
}

static int getstates(const char *funcname)
{
  char fsaname[sNAMEMAX+1],statename[sNAMEMAX+1];
  cell val;
  char *str;
  constvalue *automaton;
  constvalue *state;
  int fsa,islabel;
  int *list;
  int count,listsize,state_id;

  if (!matchtoken('<'))
    return 0;
  if (matchtoken('>'))
    return -1;          /* special construct: all other states (fall-back) */

  count=0;
  listsize=0;
  list=NULL;
  automaton=NULL;
  fsa=-1;

  do {
    if ((islabel=matchtoken(tLABEL))==0 && !needtoken(tSYMBOL))
      break;
    tokeninfo(&val,&str);
    assert(strlen(str)<sizeof fsaname);
    strcpy(fsaname,str);  /* assume this is the name of the automaton */
    if (islabel || matchtoken(':')) {
      /* token is an automaton name, add the name and get a new token */
      if (!needtoken(tSYMBOL))
        break;
      tokeninfo(&val,&str);
      assert(strlen(str)<sizeof statename);
      strcpy(statename,str);
    } else {
      /* the token was the state name (part of an anynymous automaton) */
      assert(strlen(fsaname)<sizeof statename);
      strcpy(statename,fsaname);
      fsaname[0]='\0';
    } /* if */
    if (fsa<0 || fsaname[0]!='\0') {
      automaton=automaton_add(fsaname);
      assert(automaton!=NULL);
      if (fsa>=0 && automaton->index!=fsa)
        error(83,funcname); /* multiple automatons for a single function/variable */
      fsa=automaton->index;
    } /* if */
    state=state_add(statename,fsa);
    /* add this state to the state combination list (it will be attached to the
     * automaton later) */
    state_buildlist(&list,&listsize,&count,(int)state->value);
  } while (matchtoken(','));
  needtoken('>');

  if (count>0) {
    assert(automaton!=NULL);
    assert(fsa>=0);
    state_id=state_addlist(list,count,fsa);
    assert(state_id>0);
  } else {
    /* error is already given */
    state_id=0;
  } /* if */
  if (list!=NULL)
    free(list);

  return state_id;
}

static statelist *attachstatelist(symbol *sym,int state_id)
{
  assert(sym!=NULL);
  if ((sym->usage & uDEFINE)!=0 && (sym->states==NULL || state_id==0))
    error(21,sym->name); /* function already defined, either without states or the current definition has no states */

  if (state_id!=0) {
    /* add the state list id */
    statelist *stateptr;
    if (sym->states==NULL) {
      if ((sym->states=(statelist*)malloc(sizeof(statelist)))==NULL)
        error(103);             /* insufficient memory (fatal error) */
      memset(sym->states,0,sizeof(statelist));
    } /* if */
    /* see whether the id already exists (add new state only if it does not
     * yet exist
     */
    assert(sym->states!=NULL);
    for (stateptr=sym->states->next; stateptr!=NULL && stateptr->id!=state_id; stateptr=stateptr->next)
      /* nothing */;
    assert(state_id<=SHRT_MAX);
    if (stateptr==NULL)
      stateptr=append_statelist(sym->states,state_id,0,code_idx);
    else if (stateptr->addr==0)
      stateptr->addr=code_idx;
    else
      error(84,sym->name);      /* state conflict */
    /* also check for another conflicting situation: a fallback function
     * without any states
     */
    if (state_id==-1 && sc_status!=statFIRST) {
      /* in the second round, all states should have been accumulated */
      statelist *stlist;
      assert(sym->states!=NULL);
      for (stlist=sym->states->next; stlist!=NULL && stlist->id==-1; stlist=stlist->next)
        /* nothing */;
      if (stlist==NULL)
        error(85,sym->name);    /* no states are defined for this function */
    } /* if */
    return stateptr;
  } /* if */

  return NULL;
}

/*
 *  Finds a function in the global symbol table or creates a new entry.
 *  It does some basic processing and error checking.
 */
SC_FUNC symbol *fetchfunc(char *name,int tag)
{
  symbol *sym;

  if ((sym=findglb(name,sGLOBAL))!=0) {   /* already in symbol table? */
    if (sym->ident!=iFUNCTN) {
      error(21,name);                     /* yes, but not as a function */
      return NULL;                        /* make sure the old symbol is not damaged */
    } else if ((sym->usage & uNATIVE)!=0) {
      error(21,name);                     /* yes, and it is a native */
    } /* if */
    assert(sym->vclass==sGLOBAL);
    if ((sym->usage & uPROTOTYPED)!=0 && sym->tag!=tag)
      error(25);                          /* mismatch from earlier prototype */
    if ((sym->usage & uDEFINE)==0) {
      /* as long as the function stays undefined, update the address and the tag */
      if (sym->states==NULL)
        sym->addr=code_idx;
      sym->tag=tag;
    } /* if */
  } else {
    /* don't set the "uDEFINE" flag; it may be a prototype */
    sym=addsym(name,code_idx,iFUNCTN,sGLOBAL,tag,0);
    assert(sym!=NULL);          /* fatal error 103 must be given on error */
    /* assume no arguments */
    sym->dim.arglist=(arginfo*)malloc(1*sizeof(arginfo));
    sym->dim.arglist[0].ident=0;
    /* set library ID to NULL (only for native functions) */
    sym->x.lib=NULL;
    /* set the required stack size to zero (only for non-native functions) */
    sym->x.stacksize=1;         /* 1 for PROC opcode */
  } /* if */
  if (pc_deprecate!=NULL) {
    assert(sym!=NULL);
    sym->flags|=flgDEPRICATED;
    if (sc_status==statWRITE) {
      if (sym->documentation!=NULL) {
        free(sym->documentation);
        sym->documentation=NULL;
      } /* if */
      sym->documentation=pc_deprecate;
    } else {
      free(pc_deprecate);
    } /* if */
    pc_deprecate=NULL;
  } /* if */

  return sym;
}

/* This routine adds symbolic information for each argument.
 */
static void define_args(void)
{
  symbol *sym;

  /* At this point, no local variables have been declared. All
   * local symbols are function arguments.
   */
  sym=loctab.next;
  while (sym!=NULL) {
    assert(sym->ident!=iLABEL);
    assert(sym->vclass==sLOCAL);
    markexpr(sLDECL,sym->name,sym->addr); /* mark for better optimization */
    sym=sym->next;
  } /* while */
}

static int operatorname(char *name)
{
  int opertok;
  char *str;
  cell val;

  assert(name!=NULL);

  /* check the operator */
  opertok=lex(&val,&str);
  switch (opertok) {
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
  case '>':
  case '<':
  case '!':
  case '~':
  case '=':
    name[0]=(char)opertok;
    name[1]='\0';
    break;
  case tINC:
    strcpy(name,"++");
    break;
  case tDEC:
    strcpy(name,"--");
    break;
  case tlEQ:
    strcpy(name,"==");
    break;
  case tlNE:
    strcpy(name,"!=");
    break;
  case tlLE:
    strcpy(name,"<=");
    break;
  case tlGE:
    strcpy(name,">=");
    break;
  default:
    name[0]='\0';
    error(7);           /* operator cannot be redefined (or bad operator name) */
    return 0;
  } /* switch */

  return opertok;
}

static int operatoradjust(int opertok,symbol *sym,char *opername,int resulttag)
{
  int tags[2]={0,0};
  int count=0;
  arginfo *arg;
  char tmpname[sNAMEMAX+1];
  symbol *oldsym;

  if (opertok==0)
    return TRUE;

  assert(sym!=NULL && sym->ident==iFUNCTN && sym->dim.arglist!=NULL);
  /* count arguments and save (first two) tags */
  while (arg=&sym->dim.arglist[count], arg->ident!=0) {
    if (count<2) {
      if (arg->numtags>1)
        error(65,count+1);  /* function argument may only have a single tag */
      else if (arg->numtags==1)
        tags[count]=arg->tags[0];
    } /* if */
    if (opertok=='~' && count==0) {
      if (arg->ident!=iREFARRAY)
        error(73,arg->name);/* must be an array argument */
    } else {
      if (arg->ident!=iVARIABLE)
        error(66,arg->name);/* must be non-reference argument */
    } /* if */
    if (arg->hasdefault)
      error(59,arg->name);  /* arguments of an operator may not have a default value */
    count++;
  } /* while */

  /* for '!', '++' and '--', count must be 1
   * for '-', count may be 1 or 2
   * for '=', count must be 1, and the resulttag is also important
   * for all other (binary) operators and the special '~' operator, count must be 2
   */
  switch (opertok) {
  case '!':
  case '=':
  case tINC:
  case tDEC:
    if (count!=1)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  case '-':
    if (count!=1 && count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
    break;
  default:
    if (count!=2)
      error(62);      /* number or placement of the operands does not fit the operator */
  } /* switch */

  if (tags[0]==0 && (opertok!='=' && tags[1]==0 || opertok=='=' && resulttag==0))
    error(64);        /* cannot change predefined operators */

  /* change the operator name */
  assert(strlen(opername)>0);
  operator_symname(tmpname,opername,tags[0],tags[1],count,resulttag);
  if ((oldsym=findglb(tmpname,sGLOBAL))!=NULL) {
    int i;
    if ((oldsym->usage & uDEFINE)!=0) {
      char errname[2*sNAMEMAX+16];
      funcdisplayname(errname,tmpname);
      error(21,errname);        /* symbol already defined */
    } /* if */
    sym->usage|=oldsym->usage;  /* copy flags from the previous definition */
    sym->index=oldsym->index;   /* copy overlay index */
    for (i=0; i<oldsym->numrefers; i++)
      if (oldsym->refer[i]!=NULL)
        refer_symbol(sym,oldsym->refer[i]);
    delete_symbol(&glbtab,oldsym);
  } /* if */
  strcpy(sym->name,tmpname);
  sym->hash=namehash(sym->name);/* calculate new hash */

  /* operators should return a value, except the '~' operator */
  if (opertok!='~')
    sym->usage |= uRETVALUE;

  return TRUE;
}

static int check_operatortag(int opertok,int resulttag,char *opername)
{
  assert(opername!=NULL && strlen(opername)>0);
  switch (opertok) {
  case '!':
  case '<':
  case '>':
  case tlEQ:
  case tlNE:
  case tlLE:
  case tlGE:
    if (resulttag!=pc_addtag("bool")) {
      error(63,opername,"bool:"); /* operator X requires a "bool:" result tag */
      return FALSE;
    } /* if */
    break;
  case '~':
    if (resulttag!=0) {
      error(63,opername,"_:");    /* operator "~" requires a "_:" result tag */
      return FALSE;
    } /* if */
    break;
  } /* switch */
  return TRUE;
}

static char *tag2str(char *dest,int tag)
{
  tag &= TAGMASK;
  assert(tag>=0);
  sprintf(dest,"0%x",tag);
  return isdigit(dest[1]) ? &dest[1] : dest;
}

SC_FUNC char *operator_symname(char *symname,char *opername,int tag1,int tag2,int numtags,int resulttag)
{
  char tagstr1[10], tagstr2[10];
  int opertok;

  assert(numtags>=1 && numtags<=2);
  opertok= (opername[1]=='\0') ? opername[0] : 0;
  if (opertok=='=')
    sprintf(symname,"%s%s%s",tag2str(tagstr1,resulttag),opername,tag2str(tagstr2,tag1));
  else if (numtags==1 || opertok=='~')
    sprintf(symname,"%s%s",opername,tag2str(tagstr1,tag1));
  else
    sprintf(symname,"%s%s%s",tag2str(tagstr1,tag1),opername,tag2str(tagstr2,tag2));
  return symname;
}

static int parse_funcname(const char *fname,int *tag1,int *tag2,char *opname)
{
  const char *ptr;
  char *name;
  int unary;

  /* tags are only positive, so if the function name starts with a '-',
   * the operator is an unary '-' or '--' operator.
   */
  if (*fname=='-') {
    *tag1=0;
    unary=TRUE;
    ptr=fname;
  } else {
    *tag1=(int)strtol(fname,(char**)&ptr,16);
    unary= ptr==fname;  /* unary operator if it doesn't start with a tag name */
  } /* if */
  assert(!unary || *tag1==0);
  assert(*ptr!='\0');
  for (name=opname; !isdigit(*ptr); )
    *name++ = *ptr++;
  *name='\0';
  *tag2=(int)strtol(ptr,NULL,16);
  return unary;
}

static constvalue *find_tag_byval(int tag)
{
  constvalue *tagsym;
  tagsym=find_constval_byval(&tagname_tab,tag & ~PUBLICTAG);
  if (tagsym==NULL)
    tagsym=find_constval_byval(&tagname_tab,tag | PUBLICTAG);
  return tagsym;
}

SC_FUNC char *funcdisplayname(char *dest,const char *funcname)
{
  int tags[2];
  char opname[10];
  constvalue *tagsym[2];
  int unary;

  if (isalpha(*funcname) || *funcname=='_' || *funcname==PUBLIC_CHAR || *funcname=='\0') {
    if (dest!=funcname)
      strcpy(dest,funcname);
    return dest;
  } /* if */

  unary=parse_funcname(funcname,&tags[0],&tags[1],opname);
  tagsym[1]=find_tag_byval(tags[1]);
  assert(tagsym[1]!=NULL);
  if (unary) {
    sprintf(dest,"operator%s(%s:)",opname,tagsym[1]->name);
  } else {
    tagsym[0]=find_tag_byval(tags[0]);
    assert(tagsym[0]!=NULL);
    /* special case: the assignment operator has the return value as the 2nd tag */
    if (opname[0]=='=' && opname[1]=='\0')
      sprintf(dest,"%s:operator%s(%s:)",tagsym[0]->name,opname,tagsym[1]->name);
    else
      sprintf(dest,"operator%s(%s:,%s:)",opname,tagsym[0]->name,tagsym[1]->name);
  } /* if */
  return dest;
}

static void funcstub(int fnative)
{
  int tok,tag,fpublic,usage;
  char *str;
  cell val,size;
  char symbolname[sNAMEMAX+1];
  int dim[sDIMEN_MAX];
  int numdim;
  constvalue *dimnames[sDIMEN_MAX];
  symbol *sym,*sub;
  int opertok;

  opertok=0;
  lastst=0;
  litidx=0;                     /* clear the literal pool */
  assert(loctab.next==NULL);    /* local symbol table should be empty */

  tag=pc_addtag(NULL);          /* get the tag of the return value */
  tok=lex(&val,&str);
  fpublic=(tok==tPUBLIC) || (tok==tSYMBOL && str[0]==PUBLIC_CHAR);
  if (fnative) {
    if (fpublic || tok==tSTOCK || tok==tSTATIC || tok==tSYMBOL && *str==PUBLIC_CHAR)
      error(42);                /* invalid combination of class specifiers */
  } else {
    if (tok==tPUBLIC || tok==tSTOCK || tok==tSTATIC)
      tok=lex(&val,&str);
  } /* if */

  if (tok==tOPERATOR) {
    opertok=operatorname(symbolname);
    if (opertok==0)
      return;                   /* error message already given */
    check_operatortag(opertok,tag,symbolname);
  } else {
    if (tok!=tSYMBOL && freading) {
      error(10);                /* illegal function or declaration */
      return;
    } /* if */
    strcpy(symbolname,str);
  } /* if */

  numdim=0;
  while (matchtoken('[')) {
    /* the function returns an array, get this tag for the index and the array
     * dimensions
     */
    if (numdim==sDIMEN_MAX) {
      error(53);                /* exceeding maximum number of dimensions */
      return;
    } /* if */
    size=needsub(']',&dimnames[numdim]);/* get size; size==0 for "var[]" */
    if (size==0)
      error(9);                 /* invalid array size */
    #if INT_MAX < LONG_MAX
      if (size>INT_MAX)
        error(105);             /* overflow, exceeding capacity */
    #endif
    dim[numdim++]=(int)size;
  } /* while */
  usage=0;
  if (matchtoken('{')) {        /* handle {} for the last dimension, too */
    if (numdim==sDIMEN_MAX) {
      error(53);                /* exceeding maximum number of dimensions */
      return;
    } /* if */
    usage=uPACKED;
    size=needsub('}',&dimnames[numdim]);/* get size; size==0 for "var{}" */
    if (size==0)
      error(9);                 /* invalid array size */
    size=((size*sCHARBITS/8)+pc_cellsize-1) / pc_cellsize;
    #if INT_MAX < LONG_MAX
      if (size>INT_MAX)
        error(105);             /* overflow, exceeding capacity */
    #endif
    dim[numdim++]=(int)size;
  } /* if */
  if (matchtoken('['))
    error(51);                  /* {} must be last */
  verify_array_namelist(dimnames,numdim);

  needtoken('(');               /* only functions may be native/forward */
  sym=fetchfunc(symbolname,tag);/* get a pointer to the function entry */
  if (sym==NULL)
    return;
  if (fnative) {
    sym->usage=(char)(uNATIVE | uRETVALUE | uDEFINE | (sym->usage & uPROTOTYPED));
    sym->x.lib=curlibrary;
  } else {
    if (fpublic)
      sym->usage|=uPUBLIC;
    else
      sym->usage&=~uPUBLIC;     /* forward declaration is decisive */
  } /* if */
  sym->usage|=uFORWARD;

  declargs(sym,FALSE);
  /* "declargs()" found the ")" */
  sc_attachdocumentation(sym,TRUE);  /* attach any documenation to the function */
  if (!operatoradjust(opertok,sym,symbolname,tag))
    sym->usage &= ~uDEFINE;

  if (getstates(symbolname)!=0) {
    if (fnative || opertok!=0)
      error(82);                /* native functions and operators may not have states */
    else
      error(231);               /* ignoring state specifications on forward declarations */
  } /* if */

  /* for a native operator, also need to specify an "exported" function name;
   * for a native function, this is optional
   */
  if (fnative) {
    constvalue *ntvidx;
    if (opertok!=0) {
      needtoken('=');
      lexpush();        /* push back, for matchtoken() to retrieve again */
    } /* if */
    if (matchtoken('=')) {
      /* allow number or symbol */
      if (matchtoken(tSYMBOL)) {
        tokeninfo(&val,&str);
        insert_alias(sym->name,str);
        /* see whether the aliased name has a preset index */
        if ((ntvidx=find_constval(&ntvindex_tab,str,-1))!=NULL) {
          assert(ntvidx->value<0);
          sym->index=(int)ntvidx->value;
        } /* if */
      } else {
        constexpr(&val,NULL,NULL);
        sym->index=(int)val;
        /* At the moment, I have assumed that this syntax is only valid if
         * val < 0. To properly mix "normal" native functions and indexed
         * native functions, one should use negative indices anyway.
         * Special code for a negative index in sym->addr exists in SC4.C
         * (ffcall()) and in SC6.C (the loops for counting the number of native
         * variables and for writing them).
         */
      } /* if */
    } else {
      /* no explicit name/index for this native function, see whether one
       * was preset
       */
      if ((ntvidx=find_constval(&ntvindex_tab,sym->name,-1))!=NULL) {
        assert(ntvidx->value<0);
        sym->index=(int)ntvidx->value;
      } else if (ntvindex_tab.next!=NULL) {
        /* no predefined index for this native function, but the native index
         * table is not empty: mixing predefined and automatic indiced -> give
         * a warning
         */
        error(232,sym->name);
      } /* if */
    } /* if */
  } /* if */
  needtoken(tTERM);

  /* attach the array to the function symbol */
  if (numdim>0) {
    assert(sym!=NULL);
    sub=addvariable(symbolname,0,iREFARRAY,sGLOBAL,tag,dim,dimnames,numdim,usage);
    sub->parent=sym;
  } /* if */

  litidx=0;                     /* clear the literal pool */
  delete_symbols(&loctab,0,TRUE,TRUE);/* clear local variables queue */
}

/*  newfunc    - begin a function
 *
 *  This routine is called from "parse" and tries to make a function
 *  out of the following text
 *
 *  Global references: funcstatus,lastst,litidx
 *                     rettype  (altered)
 *                     curfunc  (altered)
 *                     declared (altered)
 *                     glb_declared (altered)
 *                     sc_alignnext (altered)
 */
static int newfunc(char *firstname,int firsttag,int fpublic,int fstatic,int stock)
{
  symbol *sym;
  int argcnt,tok,tag,funcline;
  int opertok,opererror;
  char symbolname[sNAMEMAX+1];
  char *str;
  cell val,cidx,glbdecl;
  short filenum;
  int state_id,ovl_index;
  statelist *stlist;

  assert(litidx==0);    /* literal queue should be empty */
  litidx=0;             /* clear the literal pool (should already be empty) */
  opertok=0;
  lastst=0;             /* no statement yet */
  cidx=0;               /* just to avoid compiler warnings */
  glbdecl=0;
  assert(loctab.next==NULL);    /* local symbol table should be empty */
  filenum=fcurrent;     /* save file number at the start of the declaration */

  if (firstname!=NULL) {
    assert(strlen(firstname)<=sNAMEMAX);
    strcpy(symbolname,firstname);       /* save symbol name */
    tag=firsttag;
  } else {
    tag= (firsttag>=0) ? firsttag : pc_addtag(NULL);
    tok=lex(&val,&str);
    assert(!fpublic);
    if (tok==tNATIVE || tok==tPUBLIC && stock)
      error(42);                /* invalid combination of class specifiers */
    if (tok==tOPERATOR) {
      opertok=operatorname(symbolname);
      if (opertok==0)
        return TRUE;            /* error message already given */
      check_operatortag(opertok,tag,symbolname);
    } else {
      if (tok!=tSYMBOL && freading) {
        error_suggest(20,str,iFUNCTN);  /* invalid symbol name */
        return FALSE;
      } /* if */
      assert(strlen(str)<=sNAMEMAX);
      strcpy(symbolname,str);
    } /* if */
  } /* if */
  /* check whether this is a function or a variable declaration */
  if (!matchtoken('('))
    return FALSE;
  /* so it is a function, proceed */
  funcline=fline;               /* save line at which the function is defined */
  if (symbolname[0]==PUBLIC_CHAR) {
    fpublic=TRUE;               /* implicitly public function */
    if (stock)
      error(42);                /* invalid combination of class specifiers */
  } /* if */
  sym=fetchfunc(symbolname,tag);/* get a pointer to the function entry */
  if (sym==NULL || (sym->usage & uNATIVE)!=0)
    return TRUE;                /* it was recognized as a function declaration, but not as a valid one */
  sym->lnumber=fline;
  /* attach a preceding comment to the function's documentation */
  sc_attachdocumentation(sym,TRUE);
  if ((sym->usage & uPUBLIC)!=0 && !fpublic) {
    fpublic=TRUE;               /* earlier (forward) declaration said this is a public function */
    error(25);                  /* function definition does not match prototype */
  } /* if */
  if (fpublic)
    sym->usage|=uPUBLIC;
  if (fstatic)
    sym->fvisible=filenum;
  /* if the function was used before being declared, and it has a tag for the
   * result, add a third pass (as second "skimming" parse) because the function
   * result may have been used with user-defined operators, which have now
   * been incorrectly flagged (as the return tag was unknown at the time of
   * the call)
   */
  if ((sym->usage & (uPROTOTYPED | uREAD))==uREAD && sym->tag!=0) {
    int curstatus=sc_status;
    sc_status=statWRITE;  /* temporarily set status to WRITE, so the warning isn't blocked */
    error(208);
    sc_status=curstatus;
    sc_reparse=TRUE;      /* must add another pass to "initial scan" phase */
  } /* if */
  /* we want public functions to be explicitly prototyped, as they are called
   * from the outside (the exception is main(), which is implicitly forward
   * declared)
   */
  if (fpublic && (sym->usage & uFORWARD)==0 && strcmp(symbolname,uMAINFUNC)!=0)
    error(235,symbolname);
  /* declare all arguments */
  argcnt=declargs(sym,TRUE);
  opererror=!operatoradjust(opertok,sym,symbolname,tag);
  if (strcmp(symbolname,uMAINFUNC)==0 || strcmp(symbolname,uENTRYFUNC)==0
      || strcmp(symbolname,uEXITFUNC)==0)
  {
    if (argcnt>0)
      error(5);         /* main(), entry() and exit() functions may not have any arguments */
    sym->usage|=uREAD;  /* main() is the program's entry point: always used */
  } /* if */
  state_id=getstates(symbolname);
  if (state_id>0 && (opertok!=0 || strcmp(symbolname,uMAINFUNC)==0))
    error(82);          /* operators may not have states, main() may neither */
  stlist=attachstatelist(sym,state_id);
  /* "declargs()" found the ")"; if a ";" appears after this, it was a
   * prototype */
  if (matchtoken(';')) {
    sym->usage|=uFORWARD;
    if (!sc_needsemicolon)
      error(218);       /* old style prototypes used with optional semicolons */
    delete_symbols(&loctab,0,TRUE,TRUE);  /* prototype is done; forget everything */
    return TRUE;
  } /* if */
  /* so it is not a prototype, proceed */
  /* if this is a function that is not referred to (this can only be detected
   * in the second stage), shut code generation off */
  if (sc_status==statWRITE && (sym->usage & uREAD)==0 && !fpublic) {
    sc_status=statSKIP;
    cidx=code_idx;
    glbdecl=glb_declared;
  } /* if */
  if ((sym->flags & flgDEPRICATED)!=0) {
    char *ptr= (sym->documentation!=NULL) ? sym->documentation : "";
    error(234,symbolname,ptr);  /* deprecated (probably a public function) */
  } /* if */
  begcseg();
  sym->usage|=uDEFINE;  /* set the definition flag */
  if (stock)
    sym->usage|=uSTOCK;
  if (opertok!=0 && opererror)
    sym->usage &= ~uDEFINE;
  /* if the function has states, dump the label to the start of the function */
  ovl_index=sym->index;
  if (state_id!=0) {
    statelist *ptr=sym->states->next;
    while (ptr!=NULL) {
      assert(sc_status!=statWRITE || ptr->label>0);
      if (ptr->id==state_id) {
        if (pc_overlays==0)
          setlabel(ptr->label);
        ovl_index=ptr->label;
        break;          /* no need to search further */
      } /* if */
      ptr=ptr->next;
    } /* while */
  } /* if */
  startfunc(sym->name,ovl_index); /* creates stack frame */
  insert_dbgline(funcline);
  setline(FALSE);
  if (sc_alignnext) {
    alignframe(sc_dataalign);
    sc_alignnext=FALSE;
  } /* if */
  declared=0;           /* number of local cells */
  rettype=(sym->usage & uRETVALUE);      /* set "return type" variable */
  curfunc=sym;
  define_args();        /* add the symbolic info for the function arguments */
  #if !defined PAWN_LIGHT
    if (matchtoken('{')) {
      lexpush();
    } else {
      /* Insert a separator so that comments following the statement will not
       * be attached to this function; they should be attached to the next
       * function. This is not a problem for functions having a compound block,
       * because the closing brace is an explicit "end token" for the function.
       * With single statement functions, the preprocessor may overread the
       * source code before the parser determines an "end of statement".
       */
      insert_docstring_separator();
    } /* if */
  #endif
  sc_curstates=state_id;/* set state id, for accessing global state variables */
  statement(NULL,FALSE);
  sc_curstates=0;
  if ((rettype & uRETVALUE)!=0)
    sym->usage|=uRETVALUE;
  if (declared!=0) {
    /* This happens only in a very special (and useless) case, where a function
     * has only a single statement in its body (no compound block) and that
     * statement declares a new variable
     */
    modstk((int)declared*pc_cellsize);  /* remove all local variables */
    declared=0;
  } /* if */
  if ((lastst!=tRETURN) && (lastst!=tGOTO)){
    int remparams=(strcmp(sym->name,uENTRYFUNC)!=0 && strcmp(sym->name,uEXITFUNC)!=0);
    ldconst(0,sPRI);
    ffret(remparams);
    if ((sym->usage & uRETVALUE)!=0 && lastst!=tENDLESS) {
      char symname[2*sNAMEMAX+16];  /* allow space for user defined operators */
      funcdisplayname(symname,sym->name);
      error(209,symname);       /* function should return a value */
    } /* if */
  } /* if */
  endfunc();
  /* for normal functions, set the end address of the function symbol; for
   * for functions with states, adjust the endaddr field for the particular
   * state (these fields are needed for overlays)
   */
  if (stlist==NULL)
    sym->codeaddr=code_idx;
  else
    stlist->endaddr=code_idx;
  sc_attachdocumentation(sym,FALSE);  /* attach collected documentation to the function */
  if (litidx) {                 /* if there are literals defined */
    glb_declared+=litidx;
    begdseg();                  /* flip to DATA segment */
    dumplits();                 /* dump literal strings */
    litidx=0;
  } /* if */
  testsymbols(&loctab,0,TRUE,TRUE);     /* test for unused arguments and labels */
  delete_symbols(&loctab,0,TRUE,TRUE);  /* clear local variables queue */
  assert(loctab.next==NULL);
  curfunc=NULL;
  if (sc_status==statSKIP) {
    sc_status=statWRITE;
    code_idx=cidx;
    glb_declared=glbdecl;
  } /* if */
  return TRUE;
}

static int argcompare(arginfo *a1,arginfo *a2)
{
  int result,level,i;

  result= strcmp(a1->name,a2->name)==0;     /* name */
  if (result)
    result= a1->ident==a2->ident;           /* type/class */
  if (result)
    result= a1->usage==a2->usage;           /* "const" flag */
  if (result)
    result= a1->numtags==a2->numtags;       /* tags (number and names) */
  for (i=0; result && i<a1->numtags; i++)
    result= a1->tags[i]==a2->tags[i];
  if (result)
    result= a1->numdim==a2->numdim;         /* array dimensions & index tags */
  for (level=0; result && level<a1->numdim; level++)
    result= a1->dim[level]==a2->dim[level];
  for (level=0; result && level<a1->numdim; level++)
    result=compare_consttable(a1->dimnames[level],a2->dimnames[level]);
  if (result)
    result= a1->hasdefault==a2->hasdefault; /* availability of default value */
  if (a1->hasdefault) {
    if (a1->ident==iREFARRAY) {
      if (result)
        result= a1->defvalue.array.size==a2->defvalue.array.size;
      if (result)
        result= a1->defvalue.array.arraysize==a2->defvalue.array.arraysize;
      /* ??? should also check contents of the default array (these troubles
       * go away in a 2-pass compiler that forbids double declarations but
       * Pawn currently does not forbid them) */
    } else {
      if (result) {
        if ((a1->hasdefault & uSIZEOF)!=0 || (a1->hasdefault & uTAGOF)!=0)
          result= a1->hasdefault==a2->hasdefault
                  && strcmp(a1->defvalue.size.symname,a2->defvalue.size.symname)==0
                  && a1->defvalue.size.level==a2->defvalue.size.level;
        else
          result= a1->defvalue.val==a2->defvalue.val;
      } /* if */
    } /* if */
    if (result)
      result= a1->defvalue_tag==a2->defvalue_tag;
  } /* if */
  return result;
}

/*  declargs()
 *
 *  This routine adds an entry in the local symbol table for each argument
 *  found in the argument list. It returns the number of arguments.
 */
static int declargs(symbol *sym,int chkshadow)
{
  #define MAXTAGS 16
  char *ptr;
  int argcnt,oldargcnt,tok,numtags;
  int tags[MAXTAGS];
  cell val;
  arginfo arg;
  arginfo *arglist;
  char name[sNAMEMAX+1];
  int ident,fpublic,fconst;
  int idx;

  /* if the function is already defined earlier, get the number of arguments
   * of the existing definition
   */
  oldargcnt=0;
  if ((sym->usage & uPROTOTYPED)!=0)
    while (sym->dim.arglist[oldargcnt].ident!=0)
      oldargcnt++;
  argcnt=0;                             /* zero aruments up to now */
  ident=iVARIABLE;
  numtags=0;
  fconst=FALSE;
  fpublic= (sym->usage & uPUBLIC)!=0;
  assert(litidx==0);                    /* literal queue should be empty */
  /* the '(' parantheses has already been parsed */
  if (!matchtoken(')')){
    do {                                /* there are arguments; process them */
      /* any legal name increases argument count (and stack offset) */
      tok=lex(&val,&ptr);
      switch (tok) {
      case 0:
        /* nothing */
        break;
      case '&':
        if (ident!=iVARIABLE || numtags>0)
          error(1,"-identifier-","&");
        ident=iREFERENCE;
        break;
      case tCONST:
        if (ident!=iVARIABLE || numtags>0)
          error(1,"-identifier-","const");
        fconst=TRUE;
        break;
      case tLABEL:
        if (numtags>0)
          error(1,"-identifier-","-tagname-");
        tags[0]=pc_addtag(ptr);
        numtags=1;
        break;
      case '{':
        if (numtags>0)
          error(1,"-identifier-","-tagname-");
        numtags=0;
        while (numtags<MAXTAGS) {
          if (!matchtoken('_') && !needtoken(tSYMBOL))
            break;
          tokeninfo(&val,&ptr);
          tags[numtags++]=pc_addtag(ptr);
          if (matchtoken('}'))
            break;
          needtoken(',');
        } /* while */
        needtoken(':');
        tok=tLABEL;     /* for outer loop: flag that we have seen a tagname */
        break;
      case tSYMBOL:
        if (argcnt>=sMAXARGS)
          error(45);                    /* too many function arguments */
        strcpy(name,ptr);               /* save symbol name */
        if (name[0]==PUBLIC_CHAR)
          error(56,name);               /* function arguments cannot be public */
        if (numtags==0)
          tags[numtags++]=0;            /* default tag */
        /* Stack layout:
         *   base + 0*pc_cellsize  == previous "base"
         *   base + 1*pc_cellsize  == function return address
         *   base + 2*pc_cellsize  == number of arguments
         *   base + 3*pc_cellsize  == first argument of the function
         * So the offset of each argument is "(argcnt+3) * pc_cellsize".
         */
        doarg(name,ident,(argcnt+3)*pc_cellsize,tags,numtags,fpublic,fconst,chkshadow,&arg);
        if (fpublic && arg.hasdefault)
          error(59,name);       /* arguments of a public function may not have a default value */
        if ((sym->usage & uPROTOTYPED)==0) {
          /* redimension the argument list, add the entry */
          sym->dim.arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
          if (sym->dim.arglist==0)
            error(103);                 /* insufficient memory */
          memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
          /* clone the named indices (because the generated list is inherited
           * by the local symbol
           */
          for (idx=0; idx<arg.numdim; idx++) {
            if (arg.dimnames[idx]!=NULL)
              arg.dimnames[idx]=clone_consttable(arg.dimnames[idx]);
          } /* for */
          sym->dim.arglist[argcnt]=arg;
        } else {
          /* check the argument with the earlier definition */
          if (argcnt>oldargcnt || !argcompare(&sym->dim.arglist[argcnt],&arg))
            error(25);          /* function definition does not match prototype */
          /* may need to free default array argument */
          if (arg.ident==iREFARRAY && arg.hasdefault)
            free(arg.defvalue.array.data);
          else if (arg.ident==iVARIABLE
                   && ((arg.hasdefault & uSIZEOF)!=0 || (arg.hasdefault & uTAGOF)!=0))
            free(arg.defvalue.size.symname);
          /* free the tag list */
          free(arg.tags);
          /* do NOT free the named indices list, because a local symbol with this list
           * was created, and this symbol "inherited" the list
           */
        } /* if */
        argcnt++;
        ident=iVARIABLE;
        numtags=0;
        fconst=FALSE;
        break;
      case tELLIPS:
        if (ident!=iVARIABLE)
          error(10);                    /* illegal function or declaration */
        if (numtags==0)
          tags[numtags++]=0;            /* default tag */
        if ((sym->usage & uPROTOTYPED)==0) {
          /* redimension the argument list, add the entry iVARARGS */
          sym->dim.arglist=(arginfo*)realloc(sym->dim.arglist,(argcnt+2)*sizeof(arginfo));
          if (sym->dim.arglist==0)
            error(103);                 /* insufficient memory */
          memset(&sym->dim.arglist[argcnt+1],0,sizeof(arginfo));  /* keep the list terminated */
          sym->dim.arglist[argcnt].ident=iVARARGS;
          sym->dim.arglist[argcnt].hasdefault=FALSE;
          sym->dim.arglist[argcnt].defvalue.val=0;
          sym->dim.arglist[argcnt].defvalue_tag=0;
          sym->dim.arglist[argcnt].numtags=numtags;
          sym->dim.arglist[argcnt].tags=(int*)malloc(numtags*sizeof tags[0]);
          if (sym->dim.arglist[argcnt].tags==NULL)
            error(103);                 /* insufficient memory */
          memcpy(sym->dim.arglist[argcnt].tags,tags,numtags*sizeof tags[0]);
        } else {
          if (argcnt>oldargcnt || sym->dim.arglist[argcnt].ident!=iVARARGS)
            error(25);          /* function definition does not match prototype */
        } /* if */
        argcnt++;
        break;
      default:
        error(10);              /* illegal function or declaration */
        litidx=0;               /* make sure literals get dropped (ignored) */
      } /* switch */
    } while (tok=='&' || tok==tLABEL || tok==tCONST
             || tok!=tELLIPS && matchtoken(',')); /* more? */
    /* if the next token is not ",", it should be ")" */
    needtoken(')');
  } /* if */
  if ((sym->usage & uPROTOTYPED)!=0 && argcnt!=oldargcnt)
    error(25);                  /* function definition does not match prototype */

  /* resolve any "sizeof" arguments (now that all arguments are known) */
  assert(sym->dim.arglist!=NULL);
  arglist=sym->dim.arglist;
  for (idx=0; idx<argcnt && arglist[idx].ident!=0; idx++) {
    if ((arglist[idx].hasdefault & uSIZEOF)!=0 || (arglist[idx].hasdefault & uTAGOF)!=0) {
      int altidx;
      int dist,closestdist=INT_MAX;
      int closestidx=-1;
      /* Find the argument with the name mentioned after the "sizeof" or "tagof".
       * Note that we cannot use findloc here because we need the arginfo struct,
       * not the symbol. (While searching, also keep an eye for the "closest match".)
       */
      ptr=arglist[idx].defvalue.size.symname;
      assert(ptr!=NULL);
      for (altidx=0; altidx<argcnt; altidx++) {
        if (strcmp(ptr,arglist[altidx].name)==0)
          break;
        dist=levenshtein_distance(arglist[altidx].name,ptr);
        if (dist<closestdist && dist<=MAX_EDIT_DIST) {
          closestidx=altidx;
          closestdist=dist;
        } /* if */
      } /* for */
      if (altidx>=argcnt) {
        /* no match found */
        if (closestidx>=0)
          error(makelong(17,1),ptr,arglist[closestidx].name);
        else
          error(17,ptr);  /* undefined symbol (meaning "undefined argument") */
      } else {
        assert(arglist[idx].defvalue.size.symname!=NULL);
        /* check the level against the number of dimensions */
        if (arglist[idx].defvalue.size.level>0
            && arglist[idx].defvalue.size.level>=arglist[altidx].numdim)
          error(28,arglist[idx].name);  /* invalid subscript */
        /* check the type of the argument whose size to take; for a iVARIABLE
         * or a iREFERENCE, this is always 1 (so the code is redundant)
         */
        assert(arglist[altidx].ident!=iVARARGS);
        if (arglist[altidx].ident!=iREFARRAY && (arglist[idx].hasdefault & uSIZEOF)!=0) {
          assert(arglist[altidx].ident==iVARIABLE || arglist[altidx].ident==iREFERENCE);
          error(223,ptr);               /* redundant sizeof */
        } /* if */
      } /* if */
    } /* if */
  } /* for */

  sym->usage|=uPROTOTYPED;
  errorset(sRESET,0);           /* reset error flag (clear the "panic mode")*/
  return argcnt;
}

/*  doarg       - declare one argument type
 *
 *  this routine is called from "declargs()" and adds an entry in the local
 *  symbol table for one argument.
 *
 *  "fpublic" indicates whether the function for this argument list is public.
 *  The arguments themselves are never public.
 */
static void doarg(char *name,int ident,int offset,int tags[],int numtags,
                  int fpublic,int fconst,int chkshadow,arginfo *arg)
{
  symbol *argsym;
  cell size;
  int usage,matchbrace;

  strcpy(arg->name,name);
  arg->hasdefault=FALSE;        /* preset (most common case) */
  arg->defvalue.val=0;          /* clear */
  arg->defvalue_tag=0;
  arg->numdim=0;
  arg->usage=0;
  matchbrace=0;
  usage=0;
  if (matchtoken('['))
    matchbrace=']';
  else if (matchtoken('{'))
    matchbrace='}';
  if (matchbrace!=0) {
    if (ident==iREFERENCE)
      error(67,name);           /* illegal declaration ("&name[]" is unsupported) */
    if (matchbrace==']') {
      do {
        if (arg->numdim==sDIMEN_MAX) {
          error(53);              /* exceeding maximum number of dimensions */
          return;
        } /* if */
        size=needsub(']',&arg->dimnames[arg->numdim]);/* may be zero here, it is a pointer anyway */
        #if INT_MAX < LONG_MAX
          if (size>INT_MAX)
            error(105);           /* overflow, exceeding capacity */
        #endif
        arg->dim[arg->numdim]=(int)size;
        arg->numdim+=1;
      } while (matchtoken('['));
      if (matchtoken('{'))
        matchbrace='}';
    } /* if */
    if (matchbrace=='}') {
      if (arg->numdim==sDIMEN_MAX) {
        error(53);              /* exceeding maximum number of dimensions */
        return;
      } /* if */
      usage=uPACKED;
      size=needsub('}',&arg->dimnames[arg->numdim]);/* may be zero here, it is a pointer anyway */
      size=((size*sCHARBITS/8)+pc_cellsize-1) / pc_cellsize;
      #if INT_MAX < LONG_MAX
        if (size>INT_MAX)
          error(105);           /* overflow, exceeding capacity */
      #endif
      arg->dim[arg->numdim]=(int)size;
      arg->numdim+=1;
    } /* if */
    if (matchtoken('['))
      error(51);                /* {} must be last */
    verify_array_namelist(arg->dimnames,arg->numdim);
    ident=iREFARRAY;            /* "reference to array" (is a pointer) */
    if (matchtoken('=')) {
      lexpush();                /* initials() needs the "=" token again */
      assert(litidx==0);        /* at the start of a function, this is reset */
      assert(numtags>0);
      initials(ident,usage,tags[0],&size,arg->dim,arg->numdim,arg->dimnames);
      assert(size>=litidx);
      /* allocate memory to hold the initial values */
      arg->defvalue.array.data=(cell *)malloc(litidx*sizeof(cell));
      if (arg->defvalue.array.data!=NULL) {
        int i;
        memcpy(arg->defvalue.array.data,litq,litidx*sizeof(cell));
        arg->hasdefault=TRUE;   /* argument has default value */
        arg->defvalue.array.size=litidx;
        arg->defvalue.array.addr=-1;
        /* calulate size to reserve on the heap */
        arg->defvalue.array.arraysize=1;
        for (i=0; i<arg->numdim; i++)
          arg->defvalue.array.arraysize*=arg->dim[i];
        if (arg->defvalue.array.arraysize < arg->defvalue.array.size)
          arg->defvalue.array.arraysize = arg->defvalue.array.size;
      } /* if */
      litidx=0;                 /* reset */
    } /* if */
  } else {
    if (matchtoken('=')) {
      unsigned char size_tag_token;
      assert(ident==iVARIABLE || ident==iREFERENCE);
      arg->hasdefault=TRUE;     /* argument has a default value */
      size_tag_token=(unsigned char)(matchtoken(tSIZEOF) ? uSIZEOF : 0);
      if (size_tag_token==0)
        size_tag_token=(unsigned char)(matchtoken(tTAGOF) ? uTAGOF : 0);
      if (size_tag_token!=0) {
        int paranthese;
        if (ident==iREFERENCE)
          error(66,name);       /* argument may not be a reference */
        paranthese=0;
        while (matchtoken('('))
          paranthese++;
        if (needtoken(tSYMBOL)) {
          /* save the name of the argument whose size id to take */
          char *name;
          cell val;
          tokeninfo(&val,&name);
          if ((arg->defvalue.size.symname=duplicatestring(name)) == NULL)
            error(103);         /* insufficient memory */
          arg->defvalue.size.level=0;
          if (size_tag_token==uSIZEOF) {
            while (matchtoken('[')) {
              arg->defvalue.size.level+=(short)1;
              needtoken(']');
            } /* while */
          } /* if */
          if (ident==iVARIABLE) /* make sure we set this only if not a reference */
            arg->hasdefault |= size_tag_token;  /* uSIZEOF or uTAGOF */
        } /* if */
        while (paranthese--)
          needtoken(')');
      } else {
        constexpr(&arg->defvalue.val,&arg->defvalue_tag,NULL);
        assert(numtags>0);
        if (!matchtag(tags[0],arg->defvalue_tag,TRUE))
          error(213);           /* tagname mismatch */
      } /* if */
    } /* if */
  } /* if */
  arg->ident=(char)ident;
  arg->usage=(char)(fconst ? uCONST : 0);
  arg->usage|=usage;
  arg->numtags=numtags;
  arg->tags=(int*)malloc(numtags*sizeof tags[0]);
  if (arg->tags==NULL)
    error(103);                 /* insufficient memory */
  memcpy(arg->tags,tags,numtags*sizeof tags[0]);
  argsym=findloc(name);
  if (argsym!=NULL) {
    error(21,name);             /* symbol already defined */
  } else {
    if (chkshadow && (argsym=findglb(name,sSTATEVAR))!=NULL && argsym->ident!=iFUNCTN && !undefined_vars)
      error(219,name);          /* variable shadows another symbol (only give the warning when there are no errors) */
    /* add details of type and address */
    assert(numtags>0);
    argsym=addvariable(name,offset,ident,sLOCAL,tags[0],
                       arg->dim,arg->dimnames,arg->numdim,usage);
    argsym->compound=0;
    if (ident==iREFERENCE)
      argsym->usage|=uREAD;     /* because references are passed back */
    if (fpublic)
      argsym->usage|=uREAD;     /* arguments of public functions are always "used" */
    if (fconst)
      argsym->usage|=uCONST;
  } /* if */
}

static int count_referrers(symbol *entry)
{
  int i,count;

  count=0;
  for (i=0; i<entry->numrefers; i++)
    if (entry->refer[i]!=NULL)
      count++;
  return count;
}

#if !defined PAWN_LIGHT
static int find_xmltag(char *source,char *xmltag,char *xmlparam,char *xmlvalue,
                       char **outer_start,int *outer_length,
                       char **inner_start,int *inner_length)
{
  char *ptr,*inner_end;
  size_t xmltag_len,xmlparam_len,xmlvalue_len;
  int match;

  assert(source!=NULL);
  assert(xmltag!=NULL);
  assert(outer_start!=NULL);
  assert(outer_length!=NULL);
  assert(inner_start!=NULL);
  assert(inner_length!=NULL);

  /* both NULL or both non-NULL */
  assert(xmlvalue!=NULL && xmlparam!=NULL || xmlvalue==NULL && xmlparam==NULL);

  xmltag_len=strlen(xmltag);
  xmlparam_len= (xmlparam!=NULL) ? strlen(xmlparam) : 0;
  xmlvalue_len= (xmlvalue!=NULL) ? strlen(xmlvalue) : 0;
  ptr=source;
  /* find an opening '<' */
  while ((ptr=strchr(ptr,'<'))!=NULL) {
    *outer_start=ptr;           /* be optimistic... */
    match=FALSE;                /* ...and pessimistic at the same time */
    ptr++;                      /* skip '<' */
    while (*ptr!='\0' && *ptr<=' ')
      ptr++;                    /* skip white space */
    if (strncmp(ptr,xmltag,xmltag_len)==0 && (*(ptr+xmltag_len)<=' ' || *(ptr+xmltag_len)=='>')) {
      /* xml tag found, optionally check the parameter */
      ptr+=xmltag_len;
      while (*ptr!='\0' && *ptr<=' ')
        ptr++;                  /* skip white space */
      if (xmlparam!=NULL) {
        if (strncmp(ptr,xmlparam,xmlparam_len)==0 && (*(ptr+xmlparam_len)<=' ' || *(ptr+xmlparam_len)=='=')) {
          ptr+=xmlparam_len;
          while (*ptr!='\0' && *ptr<=' ')
            ptr++;              /* skip white space */
          if (*ptr=='=') {
            ptr++;              /* skip '=' */
            while (*ptr!='\0' && *ptr<=' ')
              ptr++;            /* skip white space */
            if (*ptr=='"' || *ptr=='\'')
              ptr++;            /* skip " or ' */
            assert(xmlvalue!=NULL);
            if (strncmp(ptr,xmlvalue,xmlvalue_len)==0
                && (*(ptr+xmlvalue_len)<=' '
                    || *(ptr+xmlvalue_len)=='>'
                    || *(ptr+xmlvalue_len)=='"'
                    || *(ptr+xmlvalue_len)=='\''))
              match=TRUE;       /* found it */
          } /* if */
        } /* if */
      } else {
        match=TRUE;             /* don't check the parameter */
      } /* if */
    } /* if */
    if (match) {
      /* now find the end of the opening tag */
      while (*ptr!='\0' && *ptr!='>')
        ptr++;
      if (*ptr=='>')
        ptr++;
      while (*ptr!='\0' && *ptr<=' ')
        ptr++;                  /* skip white space */
      *inner_start=ptr;
      /* find the start of the closing tag (assume no nesting) */
      while ((ptr=strchr(ptr,'<'))!=NULL) {
        inner_end=ptr;
        ptr++;                  /* skip '<' */
        while (*ptr!='\0' && *ptr<=' ')
          ptr++;                /* skip white space */
        if (*ptr=='/') {
          ptr++;                /* skip / */
          while (*ptr!='\0' && *ptr<=' ')
            ptr++;              /* skip white space */
          if (strncmp(ptr,xmltag,xmltag_len)==0 && (*(ptr+xmltag_len)<=' ' || *(ptr+xmltag_len)=='>')) {
            /* find the end of the closing tag */
            while (*ptr!='\0' && *ptr!='>')
              ptr++;
            if (*ptr=='>')
              ptr++;
            /* set the lengths of the inner and outer segment */
            assert(*inner_start!=NULL);
            *inner_length=(int)(inner_end-*inner_start);
            assert(*outer_start!=NULL);
            *outer_length=(int)(ptr-*outer_start);
            break;              /* break out of the loop */
          } /* if */
        } /* if */
      } /* while */
      return TRUE;
    } /* if */
  } /* while */
  return FALSE; /* not found */
}

static char *xmlencode(char *dest,char *source)
{
  char temp[2*sNAMEMAX+20];
  char *ptr;

  /* replace < by &lt; and such; normally, such a symbol occurs at most once in
   * a symbol name (e.g. "operator<")
   */
  ptr=temp;
  while (*source!='\0') {
    switch (*source) {
    case '<':
      strcpy(ptr,"&lt;");
      ptr+=4;
      break;
    case '>':
      strcpy(ptr,"&gt;");
      ptr+=4;
      break;
    case '&':
      strcpy(ptr,"&amp;");
      ptr+=5;
      break;
    default:
      *ptr++=*source;
    } /* switch */
    source++;
  } /* while */
  *ptr='\0';
  strcpy(dest,temp);
  return dest;
}

static void write_docstring(FILE *log,const char *string)
{
  int len;
  const char *ptr;

  if (string==NULL)
    return;
  while (*string<=' ' && *string!='\0')
    string++;                 /* skip white space */
  if (*string=='\0')
    return;

  assert(strchr(string,sDOCSEP)==NULL);
  /* optionally wrap in "<summary>...</summary>", check whether this must happen */
  if (*string!='<') {       /* wrap in "summary" */
    len=0;
    for (ptr=string; *ptr!='\0' && *ptr!='<' && *ptr!='.'; ptr++)
      len++;
    if (*ptr=='.')
      len++;
    assert(len>0);
    fprintf(log,"\t\t\t<summary>%.*s</summary>\n",len,string);
    string+=len;
    while (*string<=' ' && *string!='\0')
      string++;             /* skip white space */
  } else {
    len=0;
  } /* if */

  if (*string!='\0')
    fprintf(log,"\t\t\t%s\n",string);
}

static void make_report(symbol *root,FILE *log,char *sourcefile)
{
  char symname[_MAX_PATH];
  int i,arg,dim,count,tag;
  symbol *sym,*ref;
  constvalue *tagsym;
  char *ptr;

  /* adapt the installation directory */
  strcpy(symname,sc_rootpath);
  #if DIRSEP_CHAR=='\\'
    while ((ptr=strchr(symname,':'))!=NULL)
      *ptr='|';
    while ((ptr=strchr(symname,DIRSEP_CHAR))!=NULL)
      *ptr='/';
  #endif

  /* the XML header */
  fprintf(log,"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
  fprintf(log,"<?xml-stylesheet href=\"file:///%s/xml/pawndoc.xsl\" type=\"text/xsl\"?>\n",symname);
  fprintf(log,"<doc source=\"%s\">\n",sourcefile);
  ptr=strrchr(sourcefile,DIRSEP_CHAR);
  if (ptr!=NULL)
    ptr++;
  else
    ptr=sourcefile;
  fprintf(log,"\t<assembly>\n\t\t<name>%s</name>\n\t</assembly>\n",ptr);

  /* attach the global documentation, if any */
  if (pc_globaldoc!=NULL) {
    fprintf(log,"\n\t<!-- general -->\n");
    fprintf(log,"\t<general>\n\t\t");
    while ((ptr=strchr(pc_globaldoc,sDOCSEP))!=NULL)
      *ptr=' ';
    fputs(pc_globaldoc,log);
    fprintf(log,"\n\t</general>\n\n");
  } /* if */

  /* use multiple passes to print constants variables and functions in
   * separate sections
   */
  fprintf(log,"\t<members>\n");

  fprintf(log,"\n\t\t<!-- enumerated constants -->\n");
  for (arg=1; ; arg++) {
    count=0;
    dim=0;
    tag=0;
    /* count how many constants there are in this list, and count how many are used */
    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->ident!=iCONSTEXPR || sym->x.enumlist!=arg)
        continue;
      if (count==0)
        tag=sym->tag;
      else if (tag!=sym->tag)
        tag=0;
      count++;
      if ((sym->usage & uREAD)!=0)
        dim++;
    } /* for */
    if (count==0)
      break;            /* this list is unknown, don't look further */
    if (dim==0)
      continue;         /* none of the constants in this list is used, skip documenting it */
    /* document the list */
    if (tag!=0) {
      tagsym=find_tag_byval(tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t<member name=\"T:%s\">\n",tagsym->name);
    } else {
      fprintf(log,"\t\t<member name=\"T:anonymous\">\n");
    } /* if */
    count=0;
    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->ident!=iCONSTEXPR || sym->x.enumlist!=arg)
        continue;
      if (count==0)
        fprintf(log,"\t\t\t<location file=\"%s\" line=\"%ld\"/>\n",get_inputfile(sym->fnumber),(long)sym->lnumber);
      count++;
      fprintf(log,"\t\t\t<member name=\"C:%s\" value=\"%ld\">\n",funcdisplayname(symname,sym->name),(long)sym->addr);
      if (sym->tag!=0 && sym->tag!=tag) {
        tagsym=find_tag_byval(sym->tag);
        assert(tagsym!=NULL);
        fprintf(log,"\t\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
      } /* if */
      assert(sym->refer!=NULL);
      for (i=0; i<sym->numrefers; i++) {
        if ((ref=sym->refer[i])!=NULL)
          fprintf(log,"\t\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
      } /* for */
      write_docstring(log,sym->documentation);
      fprintf(log,"\t\t\t</member>\n");
    } /* for */
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- constants -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->ident!=iCONSTEXPR || sym->x.enumlist!=0)
      continue;
    if ((sym->usage & uREAD)==0)
      continue;
    /* list only constants with referrers */
    count=0;
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++)
      if ((ref=sym->refer[i])!=NULL)
        count++;
    if (count==0)
      continue;
    fprintf(log,"\t\t<member name=\"C:%s\" value=\"%ld\">\n",funcdisplayname(symname,sym->name),(long)sym->addr);
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    if (sym->fnumber>0 || sym->lnumber>0) {
      /* predefined constants do not have a "location" */
      fprintf(log,"\t\t\t<location file=\"%s\" line=\"%ld\"/>\n",get_inputfile(sym->fnumber),(long)sym->lnumber);
    } /* if */
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++) {
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    write_docstring(log,sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- variables -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    if (sym->ident!=iVARIABLE && sym->ident!=iARRAY)
      continue;
    funcdisplayname(symname,sym->name);
    fprintf(log,"\t\t<member name=\"F:%s\" syntax=\"%s",symname,symname);
    if (sym->ident==iARRAY) {
      for (ref=sym; ref!=NULL; ref=finddepend(ref)) {
        if (ref->dim.array.names!=NULL) {
          constvalue *field=ref->dim.array.names->next;
          fprintf(log,"[");
          while (field!=NULL && strlen(field->name)>0) {
            fprintf(log,".%s",field->name);
            if (field->next!=NULL && (field->next->value - field->value) > 1)
              fprintf(log,"[%ld]",(long)(field->next->value - field->value));
            field=field->next;
            if (field!=NULL && strlen(field->name)>0)
              fprintf(log,", ");
          } /* if */
          fprintf(log,"]");
        } else {
          fprintf(log,"[%ld]",(long)ref->dim.array.length);
        } /* if */
      } /* for */
    } /* if */
    fprintf(log,"\">\n",funcdisplayname(symname,sym->name));
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    fprintf(log,"\t\t\t<location file=\"%s\" line=\"%ld\"/>\n",get_inputfile(sym->fnumber),(long)sym->lnumber);
    assert(sym->refer!=NULL);
    if ((sym->usage & uPUBLIC)!=0)
      fprintf(log,"\t\t\t<attribute name=\"public\"/>\n");
    for (i=0; i<sym->numrefers; i++) {
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    write_docstring(log,sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t\t<!-- functions -->\n");
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->parent!=NULL)
      continue;                 /* hierarchical data type */
    if (sym->ident!=iFUNCTN)
      continue;
    if ((sym->usage & (uREAD | uNATIVE))==uNATIVE)
      continue;                 /* unused native function */
    if ((sym->usage & (uPUBLIC | uDEFINE))==uPUBLIC)
      continue;
    funcdisplayname(symname,sym->name);
    xmlencode(symname,symname);
    fprintf(log,"\t\t<member name=\"M:%s\" syntax=\"%s(",symname,symname);
    /* print only the names of the parameters between the parentheses */
    assert(sym->dim.arglist!=NULL);
    for (arg=0; sym->dim.arglist[arg].ident!=0; arg++) {
      if (arg>0)
        fprintf(log,", ");
      switch (sym->dim.arglist[arg].ident) {
      case iVARIABLE:
        fprintf(log,"%s",sym->dim.arglist[arg].name);
        break;
      case iREFERENCE:
        fprintf(log,"&amp;%s",sym->dim.arglist[arg].name);
        break;
      case iREFARRAY:
        fprintf(log,"%s",sym->dim.arglist[arg].name);
        for (dim=0; dim<sym->dim.arglist[arg].numdim; dim++)
          fprintf(log,"[]");
        break;
      case iVARARGS:
        fprintf(log,"...");
        break;
      } /* switch */
    } /* for */
    fprintf(log,")\">\n");
    if (sym->tag!=0) {
      tagsym=find_tag_byval(sym->tag);
      assert(tagsym!=NULL);
      fprintf(log,"\t\t\t<tagname value=\"%s\"/>\n",tagsym->name);
    } /* if */
    /* check whether this function is called from the outside */
    if ((sym->usage & uNATIVE)!=0)
      fprintf(log,"\t\t\t<attribute name=\"native\"/>\n");
    if ((sym->usage & uPUBLIC)!=0)
      fprintf(log,"\t\t\t<attribute name=\"public\"/>\n");
    if (strcmp(sym->name,uMAINFUNC)==0)
      fprintf(log,"\t\t\t<attribute name=\"init\"/>\n");
    else if (strcmp(sym->name,uENTRYFUNC)==0)
      fprintf(log,"\t\t\t<attribute name=\"entry\"/>\n");
    else if (strcmp(sym->name,uEXITFUNC)==0)
      fprintf(log,"\t\t\t<attribute name=\"exit\"/>\n");
    if ((sym->usage & uNATIVE)==0)
      fprintf(log,"\t\t\t<stacksize value=\"%ld\"/>\n",(long)sym->x.stacksize);
    if (sym->states!=NULL) {
      statelist *stlist=sym->states->next;
      constvalue *fsa;
      assert(stlist!=NULL);     /* there should be at least one state item */
      while (stlist!=NULL && stlist->id==-1)
        stlist=stlist->next;
      assert(stlist!=NULL);     /* state id should be found */
      i=state_getfsa(stlist->id);
      assert(i>=0);             /* automaton 0 exists */
      fsa=automaton_findid(i);
      assert(fsa!=NULL);        /* automaton should be found */
      fprintf(log,"\t\t\t<automaton name=\"%s\"/>\n", strlen(fsa->name)>0 ? fsa->name : "(anonymous)");
      //??? dump state decision table
    } /* if */
    fprintf(log,"\t\t\t<location file=\"%s\" line=\"%ld\"/>\n",get_inputfile(sym->fnumber),(long)sym->lnumber);
    assert(sym->refer!=NULL);
    for (i=0; i<sym->numrefers; i++)
      if ((ref=sym->refer[i])!=NULL)
        fprintf(log,"\t\t\t<referrer name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    /* print all symbols that are required for this function to compile */
    for (ref=root->next; ref!=NULL; ref=ref->next) {
      if (ref==sym)
        continue;
      for (i=0; i<ref->numrefers; i++)
        if (ref->refer[i]==sym)
          fprintf(log,"\t\t\t<dependency name=\"%s\"/>\n",xmlencode(symname,funcdisplayname(symname,ref->name)));
    } /* for */
    /* print parameter list, with tag & const information, plus descriptions */
    assert(sym->dim.arglist!=NULL);
    for (arg=0; sym->dim.arglist[arg].ident!=0; arg++) {
      int dim,paraminfo;
      char *outer_start,*inner_start;
      int outer_length,inner_length;
      if (sym->dim.arglist[arg].ident==iVARARGS)
        fprintf(log,"\t\t\t<param name=\"...\">\n");
      else
        fprintf(log,"\t\t\t<param name=\"%s\">\n",sym->dim.arglist[arg].name);
      /* print the tag name(s) for each parameter */
      assert(sym->dim.arglist[arg].numtags>0);
      assert(sym->dim.arglist[arg].tags!=NULL);
      paraminfo=(sym->dim.arglist[arg].numtags>1 || sym->dim.arglist[arg].tags[0]!=0)
                || sym->dim.arglist[arg].ident==iREFERENCE
                || sym->dim.arglist[arg].ident==iREFARRAY;
      if (paraminfo)
        fprintf(log,"\t\t\t\t<paraminfo>");
      if (sym->dim.arglist[arg].numtags>1 || sym->dim.arglist[arg].tags[0]!=0) {
        assert(paraminfo);
        if (sym->dim.arglist[arg].numtags>1)
          fprintf(log," {");
        for (i=0; i<sym->dim.arglist[arg].numtags; i++) {
          if (i>0)
            fprintf(log,",");
          tagsym=find_tag_byval(sym->dim.arglist[arg].tags[i]);
          assert(tagsym!=NULL);
          fprintf(log,"%s",tagsym->name);
        } /* for */
        if (sym->dim.arglist[arg].numtags>1)
          fprintf(log,"}");
      } /* if */
      switch (sym->dim.arglist[arg].ident) {
      case iREFERENCE:
        fprintf(log," &amp;");
        break;
      case iREFARRAY:
        fprintf(log," ");
        for (dim=0; dim<sym->dim.arglist[arg].numdim; dim++) {
          if (sym->dim.arglist[arg].dim[dim]==0) {
            fprintf(log,"[]");
          } else if (sym->dim.arglist[arg].dimnames[dim]!=NULL) {
            constvalue *field=sym->dim.arglist[arg].dimnames[dim]->next;
            fprintf(log,"[");
            while (field!=NULL && strlen(field->name)>0) {
              fprintf(log,".%s",field->name);
              if (field->next!=NULL && (field->next->value - field->value) > 1)
                fprintf(log,"[%ld]",(long)(field->next->value - field->value));
              field=field->next;
              if (field!=NULL && strlen(field->name)>0)
                fprintf(log,", ");
            } /* if */
            fprintf(log,"]");
          } else {
            fprintf(log,"[%d]",sym->dim.arglist[arg].dim[dim]);
          } /* if */
        } /* for */
        break;
      } /* switch */
      if (paraminfo)
        fprintf(log," </paraminfo>\n");
      /* print the user description of the parameter (parse through
       * sym->documentation)
       */
      if (sym->documentation!=NULL
          && find_xmltag(sym->documentation, "param", "name", sym->dim.arglist[arg].name,
                         &outer_start, &outer_length, &inner_start, &inner_length))
      {
        char *tail;
        fprintf(log,"\t\t\t\t%.*s\n",inner_length,inner_start);
        /* delete from documentation string */
        tail=outer_start+outer_length;
        memmove(outer_start,tail,strlen(tail)+1);
      } /* if */
      fprintf(log,"\t\t\t</param>\n");
    } /* for */
    write_docstring(log,sym->documentation);
    fprintf(log,"\t\t</member>\n");
  } /* for */

  fprintf(log,"\n\t</members>\n");
  fprintf(log,"</doc>\n");
}
#endif

/* Every symbol has a referrer list, that contains the functions that use
 * the symbol. Now, if function "apple" is accessed by functions "banana" and
 * "citron", but neither function "banana" nor "citron" are used by anyone
 * else, then, by inference, function "apple" is not used either.
 */
static void reduce_referrers(symbol *root)
{
  int i,restart;
  symbol *sym,*ref;

  do {
    restart=0;
    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->parent!=NULL)
        continue;                 /* hierarchical data type */
      if (sym->ident==iFUNCTN
          && (sym->usage & uNATIVE)==0
          && (sym->usage & uPUBLIC)==0
          && strcmp(sym->name,uMAINFUNC)!=0 && strcmp(sym->name,uENTRYFUNC)!=0 && strcmp(sym->name,uEXITFUNC)!=0
          && count_referrers(sym)==0)
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
        /* find all symbols that are referred by this symbol */
        for (ref=root->next; ref!=NULL; ref=ref->next) {
          if (ref->parent!=NULL)
            continue;             /* hierarchical data type */
          assert(ref->refer!=NULL);
          for (i=0; i<ref->numrefers && ref->refer[i]!=sym; i++)
            /* nothing */;
          if (i<ref->numrefers) {
            assert(ref->refer[i]==sym);
            ref->refer[i]=NULL;
            restart++;
          } /* if */
        } /* for */
      } else if ((sym->ident==iVARIABLE || sym->ident==iARRAY)
                 && (sym->usage & uPUBLIC)==0
                 && sym->parent==NULL
                 && count_referrers(sym)==0)
      {
        sym->usage&=~(uREAD | uWRITTEN);  /* erase usage bits if there is no referrer */
      } /* if */
    } /* for */
    /* after removing a symbol, check whether more can be removed */
  } while (restart>0);
}

/* Generate the overlay information; this can be done once the first passes
 * have completed, and we know which functions are actually called. The overlay
 * information is always generated; it depends on the compiler options and
 * configuration whether it is actually used.
 * This function should be called after the pre-amble is generated, because
 * the pre-amble contains "static" overlays.
 */
static void gen_ovlinfo(symbol *root)
{
  int idx=0;
  symbol *sym;
  int i;

  if (pc_overlays>0) {
    assert(pc_ovl0size[ovlEXIT][1]!=0); /* if this fails, writeleader() was not called */
    for (i=0; i<ovlFIRST; i++)
      if (pc_ovl0size[i][1]!=0)
          idx++;

    for (sym=root->next; sym!=NULL; sym=sym->next) {
      if (sym->ident==iFUNCTN && sym->parent==NULL
          && (sym->usage & uNATIVE)==0 && (sym->usage & (uREAD | uPUBLIC))!=0
          && (sym->usage & uDEFINE)!=0)
      {
        /* state entry functions are called directly for the states, but there
         * is no function stub (for the jump table) -> no overlay index should
         * be assigned for this stub
         */
        if (strcmp(sym->name,uENTRYFUNC)!=0)
          sym->index=idx++;
        if (sym->states!=NULL) {
          /* for functions with states, allocate an index for every implementation */
          statelist *stateptr;
          for (stateptr=sym->states->next; stateptr!=NULL; stateptr=stateptr->next)
            stateptr->label=idx++;
        } /* if */
      } /* if */
    } /* for */
  } /* if */
}

#if !defined PAWN_LIGHT
static long max_stacksize_recurse(symbol **sourcesym,symbol *sym,long basesize,int *pubfuncparams,int *recursion)
{
  long size,maxsize;
  int i,stkpos;

  assert(sourcesym!=NULL);
  assert(sym!=NULL);
  assert(sym->ident==iFUNCTN);
  assert((sym->usage & uNATIVE)==0);
  assert(recursion!=NULL);

  maxsize=sym->x.stacksize;
  for (i=0; i<sym->numrefers; i++) {
    if (sym->refer[i]!=NULL) {
      assert(sym->refer[i]->ident==iFUNCTN);
      assert((sym->refer[i]->usage & uNATIVE)==0); /* a native function cannot refer to a user-function */
      for (stkpos=0; sourcesym[stkpos]!=NULL; stkpos++) {
        if (sym->refer[i]==sourcesym[stkpos]) {   /* recursion detection */
          if ((sc_debug & sSYMBOLIC)!=0 || verbosity>=2) {
            char symname[2*sNAMEMAX+16];/* allow space for user defined operators */
            funcdisplayname(symname,sym->name);
            errorset(sSETFILE,sym->fnumber);
            errorset(sSETLINE,sym->lnumber);
            error(237,symname);         /* recursive function */
          } /* if */
          *recursion=1;
          goto break_recursion;         /* recursion was detected, quit loop */
        } /* if */
      } /* for */
      /* add this symbol to the stack */
      sourcesym[stkpos]=sym;
      sourcesym[stkpos+1]=NULL;
      /* check size of callee */
      size=max_stacksize_recurse(sourcesym,sym->refer[i],sym->x.stacksize,pubfuncparams,recursion);
      if (maxsize<size)
        maxsize=size;
      /* remove this symbol from the stack */
      sourcesym[stkpos]=NULL;
    } /* if */
  } /* for */
  break_recursion:

  if ((sym->usage & uPUBLIC)!=0) {
    /* Find out how many parameters a public function has, then see if this
     * is bigger than some maximum
     */
    arginfo *arg=sym->dim.arglist;
    int count=0;
    assert(arg!=0);
    while (arg->ident!=0) {
      count++;
      arg++;
    } /* while */
    assert(pubfuncparams!=0);
    if (count>*pubfuncparams)
      *pubfuncparams=count;
  } /* if */

  errorset(sEXPRRELEASE,0); /* clear error data */
  errorset(sRESET,0);

  return maxsize+basesize;
}

static long max_stacksize(symbol *root,int *recursion)
{
  /* Loop over all non-native functions. For each function, loop
   * over all of its referrers, accumulating the stack requirements.
   * Detect (indirect) recursion with a "mark-and-sweep" algorithm.
   * I (mis-)use the "compound" field of the symbol structure for
   * the marker, as this field is unused for functions.
   *
   * Note that the stack is shared with the heap. A host application
   * may "eat" cells from the heap as well, through amx_Allot(). The
   * stack requirements are thus only an estimate.
   */
  long size,maxsize;
  int maxparams,numfunctions;
  symbol *sym;
  symbol **symstack;

  assert(root!=NULL);
  assert(recursion!=NULL);
  /* count number of functions (for allocating the stack for recursion detection) */
  numfunctions=0;
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iFUNCTN) {
      assert(sym->compound==0);
      if ((sym->usage & uNATIVE)==0)
        numfunctions++;
    } /* if */
  } /* if */
  /* allocate function symbol stack */
  symstack=(symbol **)malloc((numfunctions+1)*sizeof(symbol*));
  if (symstack==NULL)
    error(103);         /* insufficient memory (fatal error) */
  memset(symstack,0,(numfunctions+1)*sizeof(symbol*));

  maxsize=0;
  maxparams=0;
  *recursion=0;         /* assume no recursion */
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    /* drop out if this is not a user-implemented function */
    if (sym->ident!=iFUNCTN || (sym->usage & uNATIVE)!=0)
      continue;
    /* accumulate stack size for this symbol */
    symstack[0]=sym;
    assert(symstack[1]==NULL);
    size=max_stacksize_recurse(symstack,sym,0L,&maxparams,recursion);
    assert(size>=0);
    if (maxsize<size)
      maxsize=size;
  } /* for */

  free((void*)symstack);
  maxsize++;                  /* +1 because a zero cell is always pushed on top
                               * of the stack to catch stack overwrites */
  return maxsize+(maxparams+1);/* +1 because # of parameters is always pushed on entry */
}

static long max_overlaysize(symbol *root,char **funcname)
{
  symbol *sym;
  cell max=0;

  assert(root!=NULL);
  assert(pc_overlays>0);
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iFUNCTN && (sym->usage & uNATIVE)==0 && (sym->usage & uREAD)!=0) {
      if (max<(sym->codeaddr - sym->addr)) {
        max=sym->codeaddr - sym->addr;
        if (funcname!=NULL)
          *funcname=sym->name;
      } /* if */
    } /* if */
  } /* if */
  return (long)max;
}
#endif

static int checkundefined(symbol *root)
{
  int count=0;
  symbol *sym=root->next;
  while (sym!=NULL) {
    switch (sym->ident) {
    case iVARIABLE:
    case iREFERENCE:
    case iARRAY:
    case iREFARRAY:
      if ((sym->usage & uDEFINE)==0 && (sym->usage & (uREAD | uWRITTEN))!=0)
        count++;
      break;
    case iFUNCTN:
      if ((sym->usage & uPROTOTYPED)==0)
        count++;
      break;
    } /* if */
    sym=sym->next;
  } /* while */
  return (count>0);
}

/*  testsymbols - test for unused local or global variables
 *
 *  "Public" functions are excluded from the check, since these
 *  may be exported to other object modules.
 *  Labels are excluded from the check if the argument 'testlabs'
 *  is 0. Thus, labels are not tested until the end of the function.
 *  Constants may also be excluded (convenient for global constants).
 *
 *  When the nesting level drops below "level", the check stops.
 *
 *  The function returns whether there is an "entry" point for the file.
 *  This flag will only be 1 when browsing the global symbol table.
 */
static int testsymbols(symbol *root,int level,int testlabs,int testconst)
{
  char symname[2*sNAMEMAX+16];
  int entry=FALSE;

  symbol *sym=root->next;
  while (sym!=NULL && sym->compound>=level) {
    switch (sym->ident) {
    case iLABEL:
      if (testlabs) {
        if ((sym->usage & uDEFINE)==0) {
          error_suggest(19,sym->name,iLABEL);  /* not a label: ... */
        } else if ((sym->usage & uREAD)==0) {
          errorset(sSETFILE,sym->fnumber);
          errorset(sSETLINE,sym->lnumber);
          error(203,sym->name);     /* symbol isn't used: ... */
        } /* if */
      } /* if */
      break;
    case iFUNCTN:
      if ((sym->usage & (uDEFINE | uREAD | uNATIVE | uSTOCK | uPUBLIC))==uDEFINE) {
        funcdisplayname(symname,sym->name);
        if (strlen(symname)>0) {
          errorset(sSETFILE,sym->fnumber);
          errorset(sSETLINE,sym->lnumber);
          error(203,symname);       /* symbol isn't used ... (and not public/native/stock) */
        } /* if */
      } /* if */
      if ((sym->usage & uPUBLIC)!=0 || strcmp(sym->name,uMAINFUNC)==0)
        entry=TRUE;                 /* there is an entry point */
      /* also mark the function to the debug information */
      if (((sym->usage & uREAD)!=0 || (sym->usage & uPUBLIC)!=0 && (sym->usage & uDEFINE)!=0)
          && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
      break;
    case iCONSTEXPR:
      if (testconst && (sym->usage & uREAD)==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(203,sym->name);       /* symbol isn't used: ... */
      } /* if */
      break;
    default:
      /* a variable */
      if (sym->parent!=NULL)
        break;                      /* hierarchical data type */
      if ((sym->usage & (uWRITTEN | uREAD | uSTOCK))==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(203,sym->name,sym->lnumber);  /* symbol isn't used (and not stock) */
      } else if ((sym->usage & (uREAD | uSTOCK | uPUBLIC))==0) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(204,sym->name);       /* value assigned to symbol is never used */
#if 0 // ??? not sure whether it is a good idea to force people use "const"
      } else if ((sym->usage & (uWRITTEN | uPUBLIC | uCONST))==0 && sym->ident==iREFARRAY) {
        errorset(sSETFILE,sym->fnumber);
        errorset(sSETLINE,sym->lnumber);
        error(214,sym->name);       /* make array argument "const" */
#endif
      } /* if */
      /* also mark the variable (local or global) to the debug information */
      if ((sym->usage & (uWRITTEN | uREAD))!=0 && (sym->usage & uNATIVE)==0)
        insert_dbgsymbol(sym);
    } /* if */
    sym=sym->next;
  } /* while */

  errorset(sEXPRRELEASE,0); /* clear error data */
  errorset(sRESET,0);
  return entry;
}

static cell calc_array_datasize(symbol *sym, cell *offset)
{
  cell length;

  assert(sym!=NULL);
  assert(sym->ident==iARRAY || sym->ident==iREFARRAY);
  length=sym->dim.array.length;
  if (sym->dim.array.level > 0) {
    cell sublength=calc_array_datasize(finddepend(sym),offset);
    if (offset!=NULL)
      *offset=length*(*offset+pc_cellsize);
    if (sublength>0)
      length*=length*sublength;
    else
      length=0;
  } else {
    if (offset!=NULL)
      *offset=0;
  } /* if */
  return length;
}

static void destructsymbols(symbol *root,int level)
{
  cell offset=0;
  int savepri=FALSE;
  symbol *sym=root->next;
  while (sym!=NULL && sym->compound>=level) {
    if (sym->ident==iVARIABLE || sym->ident==iARRAY) {
      char symbolname[16];
      symbol *opsym;
      cell elements;
      /* check that the '~' operator is defined for this tag */
      operator_symname(symbolname,"~",sym->tag,0,1,0);
      if ((opsym=findglb(symbolname,sGLOBAL))!=NULL) {
        /* save PRI, in case of a return statment */
        if (!savepri) {
          pushreg(sPRI);        /* right-hand operand is in PRI */
          savepri=TRUE;
        } /* if */
        /* if the variable is an array, get the number of elements */
        if (sym->ident==iARRAY) {
          elements=calc_array_datasize(sym,&offset);
          /* "elements" can be zero when the variable is declared like
           *    new mytag: myvar[2][] = { {1, 2}, {3, 4} }
           * one should declare all dimensions!
           */
          if (elements==0)
            error(46,sym->name);        /* array size is unknown */
        } else {
          elements=1;
          offset=0;
        } /* if */
        pushval(elements);
        /* call the '~' operator */
        address(sym,sPRI);
        addconst(offset);       /* add offset to array data to the address */
        pushreg(sPRI);
        pushval(2*pc_cellsize); /* 2 parameters */
        assert(opsym->ident==iFUNCTN);
        ffcall(opsym,NULL,1);
        if (sc_status!=statSKIP)
          markusage(opsym,uREAD);   /* do not mark as "used" when this call itself is skipped */
        if ((opsym->usage & uNATIVE)!=0 && opsym->x.lib!=NULL)
          opsym->x.lib->value += 1; /* increment "usage count" of the library */
      } /* if */
    } /* if */
    sym=sym->next;
  } /* while */
  /* restore PRI, if it was saved */
  if (savepri)
    popreg(sPRI);
}

static constvalue *insert_constval(constvalue *prev,constvalue *next,const char *name,cell val,
                                   int index)
{
  constvalue *cur;

  if ((cur=(constvalue*)malloc(sizeof(constvalue)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  memset(cur,0,sizeof(constvalue));
  if (name!=NULL) {
    assert(strlen(name)<=sNAMEMAX);
    strcpy(cur->name,name);
  } /* if */
  cur->value=val;
  cur->index=index;
  cur->usage=0;
  cur->next=next;
  prev->next=cur;
  return cur;
}

SC_FUNC constvalue *append_constval(constvalue *table,const char *name,cell val,int index)
{
  constvalue *cur,*prev;

  /* find the end of the constant table */
  for (prev=table, cur=table->next; cur!=NULL; prev=cur, cur=cur->next)
    /* nothing */;
  return insert_constval(prev,NULL,name,val,index);
}

SC_FUNC constvalue *find_constval(constvalue *table,char *name,int index)
{
  constvalue *ptr = table->next;

  while (ptr!=NULL) {
    if (strcmp(name,ptr->name)==0 && (index<0 || ptr->index==index))
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

static constvalue *find_constval_byval(constvalue *table,cell val)
{
  constvalue *ptr = table->next;

  while (ptr!=NULL) {
    if (ptr->value==val)
      return ptr;
    ptr=ptr->next;
  } /* while */
  return NULL;
}

#if 0   /* never used */
static int delete_constval(constvalue *table,char *name)
{
  constvalue *prev = table;
  constvalue *cur = prev->next;

  while (cur!=NULL) {
    if (strcmp(name,cur->name)==0) {
      prev->next=cur->next;
      free(cur);
      return TRUE;
    } /* if */
    prev=cur;
    cur=cur->next;
  } /* while */
  return FALSE;
}
#endif

static constvalue *clone_consttable(const constvalue *table)
{
  constvalue *cur,*root;

  if (table==NULL)
    return NULL;

  /* create new root */
  if ((root=(constvalue*)malloc(sizeof(constvalue)))==NULL)
    error(103);       /* insufficient memory (fatal error) */
  memset(root,0,sizeof(constvalue));

  /* walk through the source, copy fields */
  cur=table->next;
  while (cur!=NULL) {
    append_constval(root,cur->name,cur->value,cur->index);
    cur=cur->next;
  } /* while */
  return root;
}

SC_FUNC void delete_consttable(constvalue *table)
{
  constvalue *cur=table->next, *next;

  while (cur!=NULL) {
    next=cur->next;
    free(cur);
    cur=next;
  } /* while */
  memset(table,0,sizeof(constvalue));
}

SC_FUNC int compare_consttable(constvalue *table1,constvalue *table2)
{
  constvalue *cur1,*cur2;

  if (table1==NULL && table2==NULL)
    return TRUE;
  else if (table1==NULL || table2==NULL)
    return FALSE;

  assert(table1!=NULL && table2!=NULL);
  cur1=table1->next;
  cur2=table2->next;
  while (cur1!=NULL && cur2!=NULL) {
    if (cur1->index!=cur2->index || cur1->value!=cur2->value)
      return FALSE;
    if (strcmp(cur1->name,cur2->name)!=0)
      return FALSE;
    cur1=cur1->next;
    cur2=cur2->next;
  } /* while */

  return TRUE;
}

/*  add_constant
 *
 *  Adds a symbol to the symbol table. Returns NULL on failure.
 */
SC_FUNC symbol *add_constant(const char *name,cell val,int vclass,int tag)
{
  symbol *sym;

  /* Test whether a global or local symbol with the same name exists. Since
   * constants are stored in the symbols table, this also finds previously
   * defind constants. */
  sym=findglb(name,sSTATEVAR);
  if (!sym)
    sym=findloc(name);
  if (sym) {
    if (sym->ident!=iCONSTEXPR || sym->tag!=tag) {
      /* redefinition a function/variable to a constant is not allowed;
       * redefinition of a constant to a different tag is not allowed
       */
      error(21,name);           /* symbol already defined */
      return NULL;
    } else if (sym->addr!=val) {
      error(201,name);          /* redefinition of constant (different value) */
      sym->addr=val;            /* set new value */
    } /* if */
    /* silently ignore redefinitions of constants with the same value & tag */
    return sym;
  } /* if */

  /* constant doesn't exist yet */
  sym=addsym(name,val,iCONSTEXPR,vclass,tag,uDEFINE);
  assert(sym!=NULL);            /* fatal error 103 must be given on error */
  if (sc_status == statIDLE)
    sym->usage |= uPREDEF;
  return sym;
}

/*  statement           - The Statement Parser
 *
 *  This routine is called whenever the parser needs to know what statement
 *  it encounters (i.e. whenever program syntax requires a statement).
 */
static void statement(int *lastindent,int allow_decl)
{
  int tok,save;
  cell val;
  char *st;

  if (!freading) {
    error(36);                  /* empty statement */
    return;
  } /* if */
  errorset(sRESET,0);

  tok=lex(&val,&st);
  if (tok!='{') {
    insert_dbgline(fline);
    setline(TRUE);
  } /* if */
  /* lex() has set pc_stmtindent */
  if (lastindent!=NULL && tok!=tLABEL) {
    if (*lastindent>=0 && *lastindent!=pc_stmtindent && !indent_nowarn) {
      if (pc_matchedtabsize>1 || !lex_adjusttabsize(*lastindent))
        error(217);             /* loose indentation */
    } /* if */
    *lastindent=pc_stmtindent;
    indent_nowarn=FALSE;        /* if warning was blocked, re-enable it */
  } /* if */
  switch (tok) {
  case 0:
    /* nothing */
    break;
  case tNEW:
    if (allow_decl) {
      declloc(FALSE);
      lastst=tNEW;
    } else {
      error(3);                 /* declaration only valid in a block */
    } /* if */
    break;
  case tSTATIC:
    if (allow_decl) {
      declloc(TRUE);
      lastst=tNEW;
    } else {
      error(3);                 /* declaration only valid in a block */
    } /* if */
    break;
  case '{':
    save=fline;
    if (!matchtoken('}'))       /* {} is the empty statement */
      compound(save==fline);
    /* lastst (for "last statement") does not change */
    break;
  case ';':
    error(36);                  /* empty statement */
    break;
  case tIF:
    lastst=doif();
    break;
  case tWHILE:
    lastst=dowhile();
    break;
  case tDO:
    lastst=dodo();
    break;
  case tFOR:
    lastst=dofor();
    break;
  case tSWITCH:
    doswitch();
    lastst=tSWITCH;
    break;
  case tCASE:
  case tDEFAULT:
    error(14);     /* not in switch */
    break;
  case tGOTO:
    dogoto();
    lastst=tGOTO;
    break;
  case tLABEL:
    dolabel();
    lastst=tLABEL;
    break;
  case tRETURN:
    doreturn();
    lastst=tRETURN;
    break;
  case tBREAK:
    dobreak();
    lastst=tBREAK;
    break;
  case tCONTINUE:
    docont();
    lastst=tCONTINUE;
    break;
  case tEXIT:
    doexit();
    lastst=tEXIT;
    break;
  case tASSERT:
    doassert();
    lastst=tASSERT;
    break;
  case tSLEEP:
    dosleep();
    lastst=tSLEEP;
    break;
  case tSTATE:
    dostate();
    lastst=tSTATE;
    break;
  case tCONST:
    decl_const(sLOCAL);
    break;
  default:          /* non-empty expression */
    sc_allowproccall=optproccall;
    lexpush();      /* analyze token later */
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);
    needtoken(tTERM);
    lastst=tEXPR;
    sc_allowproccall=FALSE;
  } /* switch */
}

static void compound(int stmt_sameline)
{
  int indent=-1;
  cell save_decl=declared;
  int count_stmt=0;
  int block_start=fline;  /* save line where the compound block started */

  /* if there is more text on this line, we should adjust the statement indent */
  if (stmt_sameline) {
    const unsigned char *p=lptr-1;
    /* go back to the opening brace */
    while (*p!='{') {
      assert(p>srcline);
      p--;
    } /* while */
    assert(*p=='{');  /* it should be found */
    /* go forward, skipping white-space */
    p++;
    while (*p<=' ' && *p!='\0')
      p++;
    assert(*p!='\0'); /* a token should be found */
    lex_fetchindent(srcline,p);
  } /* if */

  nestlevel+=1;                 /* increase compound statement level */
  while (!matchtoken('}')){     /* repeat until compound statement is closed */
    if (!freading){
      error(30,block_start);    /* compound block not closed at end of file */
      break;
    } else {
      if (count_stmt>0 && (lastst==tRETURN || lastst==tBREAK || lastst==tCONTINUE || lastst==tENDLESS))
        error(225);             /* unreachable code */
      statement(&indent,TRUE);  /* do a statement */
      count_stmt++;
    } /* if */
  } /* while */
  if (lastst!=tRETURN)
    destructsymbols(&loctab,nestlevel);
  if (lastst!=tRETURN && lastst!=tGOTO)
    modstk((int)(declared-save_decl)*pc_cellsize);  /* delete local variable space */
  testsymbols(&loctab,nestlevel,FALSE,TRUE);        /* look for unused block locals */
  declared=save_decl;
  delete_symbols(&loctab,nestlevel,FALSE,TRUE);     /* erase local symbols, but
                                                     * retain block local labels
                                                     * (within the function) */
  nestlevel-=1;                 /* decrease compound statement level */
}

/*  doexpr
 *
 *  Global references: stgidx   (referred to only)
 */
static int doexpr(int comma,int chkeffect,int allowarray,int mark_endexpr,
                  int *tag,symbol **symptr,int chkfuncresult)
{
  int index,ident;
  int localstaging=FALSE;
  cell val;

  if (!staging) {
    stgset(TRUE);               /* start stage-buffering */
    localstaging=TRUE;
    assert(stgidx==0);
  } /* if */
  index=stgidx;
  errorset(sEXPRMARK,0);
  do {
    /* on second round through, mark the end of the previous expression */
    if (index!=stgidx)
      markexpr(sEXPR,NULL,0);
    pc_sideeffect=FALSE;
    ident=expression(&val,tag,symptr,chkfuncresult);
    if (!allowarray && (ident==iARRAY || ident==iREFARRAY))
      error(33,"-unknown-");    /* array must be indexed */
    if (chkeffect && !pc_sideeffect)
      error(215);               /* expression has no effect */
    sc_allowproccall=FALSE;     /* cannot use "procedure call" syntax anymore */
  } while (comma && matchtoken(',')); /* more? */
  if (mark_endexpr)
    markexpr(sEXPR,NULL,0);     /* optionally, mark the end of the expression */
  errorset(sEXPRRELEASE,0);
  if (localstaging) {
    stgout(index);
    stgset(FALSE);              /* stop staging */
  } /* if */
  return ident;
}

/*  constexpr
 */
SC_FUNC int constexpr(cell *val,int *tag,symbol **symptr)
{
  int ident,index;
  cell cidx;

  stgset(TRUE);         /* start stage-buffering */
  stgget(&index,&cidx); /* mark position in code generator */
  errorset(sEXPRMARK,0);
  ident=expression(val,tag,symptr,FALSE);
  stgdel(index,cidx);   /* scratch generated code */
  stgset(FALSE);        /* stop stage-buffering */
  if (ident!=iCONSTEXPR) {
    error(8);           /* must be constant expression */
    if (val!=NULL)
      *val=0;
    if (tag!=NULL)
      *tag=0;
    if (symptr!=NULL)
      *symptr=NULL;
  } /* if */
  errorset(sEXPRRELEASE,0);
  return (ident==iCONSTEXPR);
}

/*  test
 *
 *  In the case a "simple assignment" operator ("=") is used within a test,
 *  the warning "possibly unintended assignment" is displayed. This routine
 *  sets the global variable "sc_intest" to true, it is restored upon termination.
 *  In the case the assignment was intended, use parantheses around the
 *  expression to avoid the warning; primary() sets "sc_intest" to 0.
 *
 *  Global references: sc_intest (altered, but restored upon termination)
 */
static int test(int label,int parens,int invert)
{
  int index,tok;
  cell cidx;
  int ident,tag;
  cell constval;
  symbol *sym;
  int localstaging=FALSE;
  short local_intest;

  if (!staging) {
    stgset(TRUE);               /* start staging */
    localstaging=TRUE;
    #if !defined NDEBUG
      stgget(&index,&cidx);     /* should start at zero if started locally */
      assert(index==0);
    #endif
  } /* if */

  local_intest=sc_intest;
  sc_intest=TRUE;
  if (parens)
    needtoken('(');
  do {
    stgget(&index,&cidx);       /* mark position (of last expression) in
                                 * code generator */
    ident=expression(&constval,&tag,&sym,TRUE);
    tok=matchtoken(',');
    if (tok)
      markexpr(sEXPR,NULL,0);
  } while (tok); /* do */
  if (parens)
    needtoken(')');
  if (ident==iARRAY || ident==iREFARRAY) {
    char *ptr=(sym->name!=NULL) ? sym->name : "-unknown-";
    error(33,ptr);              /* array must be indexed */
  } /* if */
  if (ident==iCONSTEXPR) {      /* constant expression */
    int testtype=0;
    sc_intest=local_intest;     /* restore state */
    stgdel(index,cidx);
    if (constval) {             /* code always executed */
      error(206);               /* redundant test: always non-zero */
      testtype=tENDLESS;
    } else {
      error(205);               /* redundant code: never executed */
      jumplabel(label);
    } /* if */
    if (localstaging) {
      stgout(0);                /* write "jumplabel" code */
      stgset(FALSE);            /* stop staging */
    } /* if */
    return testtype;
  } /* if */
  if (tag!=0 && tag!=pc_addtag("bool"))
    if (check_userop(lneg,tag,0,1,NULL,&tag))
      invert= !invert;          /* user-defined ! operator inverted result */
  if (invert)
    jmp_ne0(label);             /* jump to label if true (different from 0) */
  else
    jmp_eq0(label);             /* jump to label if false (equal to 0) */
  markexpr(sEXPR,NULL,0);       /* end expression (give optimizer a chance) */
  sc_intest=local_intest;       /* restore state */
  if (localstaging) {
    stgout(0);                  /* output queue from the very beginning (see
                                 * assert() when localstaging is set to TRUE) */
    stgset(FALSE);              /* stop staging */
  } /* if */
  return 0;
}

static int doif(void)
{
  int flab1,flab2;
  int ifindent;
  int lastst_true;

  ifindent=pc_stmtindent;       /* save the indent of the "if" instruction */
  flab1=getlabel();             /* get label number for false branch */
  test(flab1,TRUE,FALSE);       /* get expression, branch to flab1 if false */
  statement(NULL,FALSE);        /* if true, do a statement */
  if (!matchtoken(tELSE)) {     /* if...else ? */
    setlabel(flab1);            /* no, simple if..., print false label */
  } else {
    lastst_true=lastst;         /* save last statement of the "true" branch */
    /* to avoid the "dangling else" error, we want a warning if the "else"
     * has a lower indent than the matching "if" */
    if (pc_stmtindent<ifindent)
      error(217);               /* loose indentation */
    flab2=getlabel();
    if ((lastst!=tRETURN) && (lastst!=tGOTO))
      jumplabel(flab2);         /* "true" branch jumps around "else" clause, unless the "true" branch statement already jumped */
    setlabel(flab1);            /* print false label */
    statement(NULL,FALSE);      /* do "else" clause */
    setlabel(flab2);            /* print true label */
    /* if both the "true" branch and the "false" branch ended with the same
     * kind of statement, set the last statement id to that kind, rather than
     * to the generic tIF; this allows for better "unreachable code" checking
     */
    if (lastst==lastst_true)
      return lastst;
  } /* if */
  return tIF;
}

static int dowhile(void)
{
  int wq[wqSIZE];               /* allocate local queue */
  int save_endlessloop,retcode;

  save_endlessloop=endlessloop;
  addwhile(wq);                 /* add entry to queue for "break" */
  setlabel(wq[wqLOOP]);         /* loop label */
  /* The debugger uses the "break" opcode to be able to "break" out of
   * a loop. To make sure that each loop has a break opcode, even for the
   * tiniest loop, set it below the top of the loop
   */
  setline(TRUE);
  endlessloop=test(wq[wqEXIT],TRUE,FALSE);/* branch to wq[wqEXIT] if false */
  statement(NULL,FALSE);        /* if so, do a statement */
  jumplabel(wq[wqLOOP]);        /* and loop to "while" start */
  setlabel(wq[wqEXIT]);         /* exit label */
  delwhile();                   /* delete queue entry */

  retcode=endlessloop ? tENDLESS : tWHILE;
  endlessloop=save_endlessloop;
  return retcode;
}

/*
 *  Note that "continue" will in this case not jump to the top of the loop, but
 *  to the end: just before the TRUE-or-FALSE testing code.
 */
static int dodo(void)
{
  int wq[wqSIZE];
  int save_endlessloop,retcode,top;

  save_endlessloop=endlessloop;
  addwhile(wq);           /* see "dowhile" for more info */
  top=getlabel();         /* make a label first */
  setlabel(top);          /* loop label */
  statement(NULL,FALSE);
  needtoken(tWHILE);
  setlabel(wq[wqLOOP]);   /* "continue" always jumps to WQLOOP. */
  setline(TRUE);
  endlessloop=test(wq[wqEXIT],TRUE,FALSE);
  jumplabel(top);
  setlabel(wq[wqEXIT]);
  delwhile();
  needtoken(tTERM);

  retcode=endlessloop ? tENDLESS : tDO;
  endlessloop=save_endlessloop;
  return retcode;
}

static int dofor(void)
{
  int wq[wqSIZE];
  cell save_decl;
  int save_nestlevel,save_endlessloop,skiplab;
  int index,endtok;
  int *ptr;

  save_decl=declared;
  save_nestlevel=nestlevel;
  save_endlessloop=endlessloop;

  addwhile(wq);
  skiplab=getlabel();
  endtok= matchtoken('(') ? ')' : tDO;
  if (matchtoken(';')==0) {
    /* new variable declarations are allowed here */
    if (matchtoken(tNEW)) {
      /* The variable in expr1 of the for loop is at a
       * 'compound statement' level of it own.
       */
      nestlevel++;
      declloc(FALSE); /* declare local variable */
    } else {
      doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);  /* expression 1 */
      needtoken(';');
    } /* if */
  } /* if */
  /* Adjust the "declared" field in the "while queue", in case that
   * local variables were declared in the first expression of the
   * "for" loop. These are deleted in separately, so a "break" or a "continue"
   * must ignore these fields.
   */
  ptr=readwhile();
  assert(ptr!=NULL);
  ptr[wqBRK]=(int)declared;
  ptr[wqCONT]=(int)declared;
  jumplabel(skiplab);               /* skip expression 3 1st time */
  setlabel(wq[wqLOOP]);             /* "continue" goes to this label: expr3 */
  setline(TRUE);
  /* Expressions 2 and 3 are reversed in the generated code: expression 3
   * precedes expression 2. When parsing, the code is buffered and marks for
   * the start of each expression are insterted in the buffer.
   */
  assert(!staging);
  stgset(TRUE);                     /* start staging */
  assert(stgidx==0);
  index=stgidx;
  stgmark(sSTARTREORDER);
  stgmark((unsigned char)(sEXPRSTART+0));    /* mark start of 2nd expression in stage */
  setlabel(skiplab);                /* jump to this point after 1st expression */
  if (matchtoken(';')) {
    endlessloop=1;
  } else {
    endlessloop=test(wq[wqEXIT],FALSE,FALSE);/* expression 2 (jump to wq[wqEXIT] if false) */
    needtoken(';');
  } /* if */
  stgmark((unsigned char)(sEXPRSTART+1));    /* mark start of 3th expression in stage */
  if (!matchtoken(endtok)) {
    doexpr(TRUE,TRUE,TRUE,TRUE,NULL,NULL,FALSE);    /* expression 3 */
    needtoken(endtok);
  } /* if */
  stgmark(sENDREORDER);             /* mark end of reversed evaluation */
  stgout(index);
  stgset(FALSE);                    /* stop staging */
  statement(NULL,FALSE);
  jumplabel(wq[wqLOOP]);
  setlabel(wq[wqEXIT]);
  delwhile();

  assert(nestlevel>=save_nestlevel);
  if (nestlevel>save_nestlevel) {
    /* Clean up the space and the symbol table for the local
     * variable in "expr1".
     */
    destructsymbols(&loctab,nestlevel);
    modstk((int)(declared-save_decl)*pc_cellsize);
    testsymbols(&loctab,nestlevel,FALSE,TRUE);  /* look for unused block locals */
    declared=save_decl;
    delete_symbols(&loctab,nestlevel,FALSE,TRUE);
    nestlevel=save_nestlevel;     /* reset 'compound statement' nesting level */
  } /* if */

  index=endlessloop ? tENDLESS : tFOR;
  endlessloop=save_endlessloop;
  return index;
}

/* The switch statement is incompatible with its C sibling:
 * 1. the cases are not drop through
 * 2. only one instruction may appear below each case, use a compound
 *    instruction to execute multiple instructions
 * 3. the "case" keyword accepts a comma separated list of values to
 *    match, it also accepts a range using the syntax "1 .. 4"
 *
 * SWITCH param
 *   PRI = expression result
 *   param = table offset (code segment)
 *
 */
static void doswitch(void)
{
  int lbl_table,lbl_exit,lbl_case;
  int swdefault,casecount;
  int tok;
  cell val;
  char *str;
  constvalue caselist = { NULL, "", 0, 0};   /* case list starts empty */
  constvalue *cse,*csp;
  int label;

  needtoken('(');
  doexpr(TRUE,FALSE,FALSE,FALSE,NULL,NULL,TRUE);/* evaluate switch expression */
  needtoken(')');
  /* generate the code for the switch statement, the label is the address
   * of the case table (to be generated later).
   */
  lbl_table=getlabel();
  lbl_case=0;                   /* just to avoid a compiler warning */
  ffswitch(lbl_table,FALSE);

  needtoken('{');
  lbl_exit=getlabel();          /* get label number for jumping out of switch */
  swdefault=FALSE;
  casecount=0;
  do {
    tok=lex(&val,&str);         /* read in (new) token */
    switch (tok) {
    case tCASE:
      if (swdefault!=FALSE)
        error(15);        /* "default" case must be last in switch statement */
      lbl_case=getlabel();
      PUSHSTK_I(sc_allowtags);
      sc_allowtags=FALSE; /* do not allow tagnames here */
      do {
        casecount++;

        /* ??? enforce/document that, in a switch, a statement cannot start
         *     with a label. Then, you can search for:
         *     * the first semicolon (marks the end of a statement)
         *     * an opening brace (marks the start of a compound statement)
         *     and search for the right-most colon before that statement
         *     Now, by replacing the ':' by a special COLON token, you can
         *     parse all expressions until that special token.
         */

        constexpr(&val,NULL,NULL);
        /* Search the insertion point (the table is kept in sorted order, so
         * that advanced abstract machines can sift the case table with a
         * binary search). Check for duplicate case values at the same time.
         */
        for (csp=&caselist, cse=caselist.next;
             cse!=NULL && cse->value<val;
             csp=cse, cse=cse->next)
          /* nothing */;
        if (cse!=NULL && cse->value==val)
          error(40,val);                /* duplicate "case" label */
        /* Since the label is stored as a string in the "constvalue", the
         * size of an identifier must be at least 1/4th of a cell size in bits
         * (there are 8 hexadecimal digits in a 32-bit number).
         */
        #if sNAMEMAX < PAWN_CELL_SIZE / 4
          #error Length of identifier (sNAMEMAX) too small.
        #endif
        assert(csp!=NULL);
        assert(csp->next==cse);
        insert_constval(csp,cse,itoh(lbl_case),val,0);
        if (matchtoken(tDBLDOT)) {
          cell end;
          constexpr(&end,NULL,NULL);
          if (end<=val)
            error(50);                  /* invalid range */
          while (++val<=end) {
            casecount++;
            /* find the new insertion point */
            for (csp=&caselist, cse=caselist.next;
                 cse!=NULL && cse->value<val;
                 csp=cse, cse=cse->next)
              /* nothing */;
            if (cse!=NULL && cse->value==val)
              error(40,val);            /* duplicate "case" label */
            assert(csp!=NULL);
            assert(csp->next==cse);
            insert_constval(csp,cse,itoh(lbl_case),val,0);
          } /* if */
        } /* if */
      } while (matchtoken(','));
      needtoken(':');                   /* ':' ends the case */
      sc_allowtags=(short)POPSTK_I();   /* reset */
      setlabel(lbl_case);
      statement(NULL,FALSE);
      jumplabel(lbl_exit);
      break;
    case tDEFAULT:
      if (swdefault!=FALSE)
        error(16);         /* multiple defaults in switch */
      lbl_case=getlabel();
      setlabel(lbl_case);
      needtoken(':');
      swdefault=TRUE;
      statement(NULL,FALSE);
      /* Jump to lbl_exit, even thouh this is the last clause in the
       * switch, because the jump table is generated between the last
       * clause of the switch and the exit label.
       */
      jumplabel(lbl_exit);
      break;
    default:
      if (tok!='}') {
        error(2);
        indent_nowarn=TRUE; /* disable this check */
        tok='}';     /* break out of the loop after an error */
      } /* if */
    } /* switch */
  } while (tok!='}');

  #if !defined NDEBUG
    /* verify that the case table is sorted (unfortunatly, duplicates can
     * occur; there really shouldn't be duplicate cases, but the compiler
     * may not crash or drop into an assertion for a user error). */
    for (cse=caselist.next; cse!=NULL && cse->next!=NULL; cse=cse->next)
      assert(cse->value <= cse->next->value);
  #endif
  /* generate the table here, before lbl_exit (general jump target) */
  setlabel(lbl_table);
  assert(swdefault==FALSE || swdefault==TRUE);
  if (swdefault==FALSE) {
    /* store lbl_exit as the "none-matched" label in the switch table */
    label=lbl_exit;
  } else {
    /* lbl_case holds the label of the "default" clause */
    label=lbl_case;
  } /* if */
  ffcase(casecount,label,TRUE,FALSE);
  /* generate the rest of the table */
  for (cse=caselist.next; cse!=NULL; cse=cse->next)
    ffcase(cse->value,strtol(cse->name,NULL,16),FALSE,FALSE);

  setlabel(lbl_exit);
  delete_consttable(&caselist); /* clear list of case labels */
}

static void doassert(void)
{
  int flab1,index;
  cell cidx;

  if ((sc_debug & sCHKBOUNDS)!=0) {
    flab1=getlabel();           /* get label number for "OK" branch */
    test(flab1,FALSE,TRUE);/* get expression and branch to flab1 if true */
    insert_dbgline(fline);      /* make sure we can find the correct line number */
    ffabort(xASSERTION);
    setlabel(flab1);
  } else {
    stgset(TRUE);               /* start staging */
    stgget(&index,&cidx);       /* mark position in code generator */
    do {
      expression(NULL,NULL,NULL,FALSE);
      stgdel(index,cidx);       /* just scrap the code */
    } while (matchtoken(','));
    stgset(FALSE);              /* stop staging */
  } /* if */
  needtoken(tTERM);
}

static void dogoto(void)
{
  char *st;
  cell val;
  symbol *sym;

  /* if we were inside an endless loop, assume that we jump out of it */
  endlessloop=0;

  if (lex(&val,&st)==tSYMBOL) {
    sym=fetchlab(st);
    jumplabel((int)sym->addr);
    sym->usage|=uREAD;          /* set "uREAD" bit */
    /* for a backward goto, destruct the symbols; for a forward goto, the
     * compound nesting level of a symbol is unknown and therefore it cannot
     * be destructed; the trouble is: by the time we get to the label, the
     * symbol may no longer be in the symbol table
     */
    if ((sym->usage & uDEFINE)!=0) {
      if (sym->compound<nestlevel)
        destructsymbols(&loctab,sym->compound+1);
    } else {
      //??? test for symbols with sym->compound==nestlevel, test whether these
      // have a destruction operator on their tag, issue a warning if so
    } /* if */
  } else {
    error_suggest(20,st,iLABEL);/* illegal symbol name */
  } /* if */
  needtoken(tTERM);
}

static void dolabel(void)
{
  char *st;
  cell val;
  symbol *sym;

  tokeninfo(&val,&st);  /* retrieve label name again */
  if (find_constval(&tagname_tab,st,-1)!=NULL)
    error(221,st);      /* label name shadows tagname */
  sym=fetchlab(st);
  setlabel((int)sym->addr);
  /* since one can jump around variable declarations or out of compound
   * blocks, the stack must be manually adjusted
   */
  setstk(-declared*pc_cellsize);
  sym->usage|=uDEFINE;  /* label is now defined */
}

/*  fetchlab
 *
 *  Finds a label from the (local) symbol table or adds one to it.
 *  Labels are local in scope.
 *
 *  Note: The "_usage" bit is set to zero. The routines that call "fetchlab()"
 *        must set this bit accordingly.
 */
static symbol *fetchlab(char *name)
{
  symbol *sym;

  sym=findloc(name);            /* labels are local in scope */
  if (sym) {
    if (sym->ident!=iLABEL)
      error_suggest(19,sym->name,iLABEL);  /* not a label: ... */
    //??? if this is a label definition, update sym->nestlevel and sym->x.declared
  } else {
    sym=addsym(name,getlabel(),iLABEL,sLOCAL,0,0);
    assert(sym!=NULL);          /* fatal error 103 must be given on error */
    sym->x.declared=(int)declared;
    sym->compound=nestlevel;
  } /* if */
  return sym;
}

/*  doreturn
 *
 *  Global references: rettype  (altered)
 */
static void doreturn(void)
{
  int tag,ident;
  int level,remparams;
  symbol *sym,*sub;

  if (!matchtoken(tTERM)) {
    /* "return <value>" */
    if ((rettype & uRETNONE)!=0)
      error(78);                        /* mix "return;" and "return value;" */
    ident=doexpr(TRUE,FALSE,TRUE,FALSE,&tag,&sym,TRUE);
    needtoken(tTERM);
    if (ident==iARRAY && sym==NULL) {
      /* returning a literal string is not supported (it must be a variable) */
      error(39);
      ident=iCONSTEXPR;                 /* avoid handling an "array" case */
    } /* if */
    /* see if this function already has a sub type (an array attached) */
    sub=finddepend(curfunc);
    assert(sub==NULL || sub->ident==iREFARRAY);
    if ((rettype & uRETVALUE)!=0) {
      int retarray=(ident==iARRAY || ident==iREFARRAY);
      /* there was an earlier "return" statement in this function */
      if (sub==NULL && retarray || sub!=NULL && !retarray)
        error(79);                      /* mixing "return array;" and "return value;" */
      if (retarray && (curfunc->usage & uPUBLIC)!=0)
        error(90,curfunc->name);        /* public function may not return array */
    } /* if */
    rettype|=uRETVALUE;                 /* function returns a value */
    /* check tagname with function tagname */
    assert(curfunc!=NULL);
    if (!matchtag(curfunc->tag,tag,TRUE))
      error(213);                       /* tagname mismatch */
    if (ident==iARRAY || ident==iREFARRAY) {
      int dim[sDIMEN_MAX];
      constvalue *dimnames[sDIMEN_MAX];
      int numdim=0;
      cell arraysize;
      assert(sym!=NULL);
      if (sub!=NULL) {
        assert(sub->ident==iREFARRAY);
        /* this function has an array attached already; check that the current
         * "return" statement returns exactly the same array
         */
        level=sym->dim.array.level;
        if (sub->dim.array.level!=level) {
          error(48);                    /* array dimensions must match */
        } else {
          for (numdim=0; numdim<=level; numdim++) {
            dim[numdim]=(int)sub->dim.array.length;
            dimnames[numdim]=sub->dim.array.names;
            if (sym->dim.array.length!=dim[numdim])
              error(47);    /* array sizes must match */
            if (!compare_consttable(sym->dim.array.names,dimnames[numdim]))
              error(47);    /* array definitions must match */
            if (numdim<level) {
              sym=finddepend(sym);
              sub=finddepend(sub);
              assert(sym!=NULL && sub!=NULL);
              /* ^^^ both arrays have the same dimensions (this was checked
               *     earlier) so the dependend should always be found
               */
            } /* if */
          } /* for */
        } /* if */
      } else {
        int argcount,usage;
        /* this function does not yet have an array attached; clone the
         * returned symbol beneath the current function
         */
        sub=sym;
        assert(sub!=NULL);
        level=sub->dim.array.level;
        for (numdim=0; numdim<=level; numdim++) {
          dim[numdim]=(int)sub->dim.array.length;
          dimnames[numdim]=clone_consttable(sub->dim.array.names);
          usage=(sub->usage & uPACKED);
          if (numdim<level) {
            sub=finddepend(sub);
            assert(sub!=NULL);
          } /* if */
          /* check that all dimensions are known */
          if (dim[numdim]<=0)
            error(46,sym->name);
        } /* for */
        /* the address of the array is stored in a hidden parameter; the address
         * of this parameter is 1 + the number of parameters (times the size of
         * a cell) + the size of the stack frame and the return address
         *   base + 0*pc_cellsize         == previous "base"
         *   base + 1*pc_cellsize         == function return address
         *   base + 2*pc_cellsize         == number of arguments
         *   base + 3*pc_cellsize         == first argument of the function
         *   ...
         *   base + ((n-1)+3)*pc_cellsize == last argument of the function
         *   base + (n+3)*pc_cellsize     == hidden parameter with array address
         */
        assert(curfunc!=NULL);
        assert(curfunc->dim.arglist!=NULL);
        for (argcount=0; curfunc->dim.arglist[argcount].ident!=0; argcount++)
          /* nothing */;
        sub=addvariable(curfunc->name,(argcount+3)*pc_cellsize,iREFARRAY,
                        sGLOBAL,curfunc->tag,dim,dimnames,numdim,usage);
        sub->parent=curfunc;
      } /* if */
      /* get the hidden parameter, copy the array (the array is on the heap;
       * it stays on the heap for the moment, and it is removed -usually- at
       * the end of the expression/statement, see expression() in SC3.C)
       */
      address(sub,sALT);                /* ALT = destination */
      arraysize=calc_arraysize(dim,numdim,0);
      memcopy(arraysize*pc_cellsize);   /* source already in PRI */
      /* moveto1(); is not necessary, callfunction() does a popreg() */
    } /* if */
  } else {
    /* this return statement contains no expression */
    ldconst(0,sPRI);
    if ((rettype & uRETVALUE)!=0) {
      char symname[2*sNAMEMAX+16];      /* allow space for user defined operators */
      assert(curfunc!=NULL);
      funcdisplayname(symname,curfunc->name);
      error(209,symname);               /* function should return a value */
    } /* if */
    rettype|=uRETNONE;                  /* function does not return anything */
  } /* if */
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  modstk((int)declared*pc_cellsize);    /* end of function, remove *all*
                                         * local variables */
  assert(curfunc!=NULL);
  remparams=(strcmp(curfunc->name,uENTRYFUNC)!=0 && strcmp(curfunc->name,uEXITFUNC)!=0);
  ffret(remparams);
}

static void dobreak(void)
{
  int *ptr;

  endlessloop=0;      /* if we were inside an endless loop, we just jumped out */
  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,nestlevel);
  modstk(((int)declared-ptr[wqBRK])*pc_cellsize);
  jumplabel(ptr[wqEXIT]);
}

static void docont(void)
{
  int *ptr;

  ptr=readwhile();      /* readwhile() gives an error if not in loop */
  needtoken(tTERM);
  if (ptr==NULL)
    return;
  destructsymbols(&loctab,nestlevel);
  modstk(((int)declared-ptr[wqCONT])*pc_cellsize);
  jumplabel(ptr[wqLOOP]);
}

SC_FUNC void exporttag(int tag)
{
  /* find the tag by value in the table, then set the top bit to mark it
   * "public"
   */
  if (tag!=0 && (tag & PUBLICTAG)==0) {
    constvalue *ptr;
    for (ptr=tagname_tab.next; ptr!=NULL && tag!=(int)(ptr->value & TAGMASK); ptr=ptr->next)
      /* nothing */;
    if (ptr!=NULL)
      ptr->value |= PUBLICTAG;
  } /* if */
}

static void doexit(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,FALSE,&tag,NULL,TRUE);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  exporttag(tag);
  destructsymbols(&loctab,0);           /* call destructor for *all* locals */
  ffabort(xEXIT);
}

static void dosleep(void)
{
  int tag=0;

  if (matchtoken(tTERM)==0){
    doexpr(TRUE,FALSE,FALSE,FALSE,&tag,NULL,TRUE);
    needtoken(tTERM);
  } else {
    ldconst(0,sPRI);
  } /* if */
  ldconst(tag,sALT);
  exporttag(tag);
  ffabort(xSLEEP);

  /* for stack usage checking, mark the use of the sleep instruction */
  pc_memflags |= suSLEEP_INSTR;
}

static void dostate(void)
{
  char name[sNAMEMAX+1];
  constvalue *automaton;
  constvalue *state;
  statelist *stlist;
  int flabel,result;
  symbol *sym;
  constvalue dummyautomaton,dummystate;
  #if !defined PAWN_LIGHT
    size_t length;
	int index,listid,listindex,stateindex;
    char *doc;
  #endif

  /* check for an optional condition */
  if (matchtoken('(')) {
    flabel=getlabel();          /* get label number for "false" branch */
    pc_docexpr=TRUE;            /* attach expression as a documentation string */
    test(flabel,FALSE,FALSE);/* get expression, branch to flabel if false */
    pc_docexpr=FALSE;
    needtoken(')');
  } else {
    flabel=-1;
  } /* if */

  result=sc_getstateid(&automaton,&state,name);
  needtoken(tTERM);
  if (!result) {
    /* create a dummy state, so that the transition table can be built in the
     * report
     */
    if (sc_status==statFIRST) {
      memset(&dummyautomaton,0,sizeof dummyautomaton);
      automaton=&dummyautomaton;
      memset(&dummystate,0,sizeof dummystate);
      assert(strlen(name)<=sNAMEMAX);
      strcpy(dummystate.name,name);
      state=&dummystate;
    } else {
      delete_autolisttable();
      return;
    } /* if */
  } /* if */

  /* see if the current automaton has exit functions for the states, if so,
   * call it through the function stub
   */
  if ((sym=findglb(uEXITFUNC,sSTATEVAR))!=NULL) {
    /* push 0 for number of arguments in case of overlays (the state exit
     * function has a RET for normal mode, but an IRETN for overlay mode;
     * this IRETN clears the arguments from the stack)
     */
    if (pc_overlays>0)
      pushval(0);
    ffcall(sym,NULL,0);
  } /* if */

  /* store the new state id */
  assert(state!=NULL);
  ldconst(state->value,sPRI);
  assert(automaton!=NULL);
  assert(automaton->index==0 && automaton->name[0]=='\0' || automaton->index>0);
  storereg(automaton->value,sPRI);

  /* find the optional entry() function for the state */
  sym=findglb(uENTRYFUNC,sGLOBAL);
  if (sc_status==statWRITE && sym!=NULL && sym->ident==iFUNCTN && sym->states!=NULL) {
    for (stlist=sym->states->next; stlist!=NULL; stlist=stlist->next) {
      assert(stlist->label>0);
      if (state_getfsa(stlist->id)==automaton->index && state_inlist(stlist->id,(int)state->value))
        break;      /* found! */
    } /* for */
    assert(stlist==NULL || state_inlist(stlist->id,(int)state->value));
    if (stlist!=NULL) {
      /* when generating overlays, make sure to push in a zero for the number
       * of arguments (when not using overlays, the compiler generates a RET
       * for the entry function return, but for overlays, there only exists an
       * IRETN instruction, which clears the arguments from the stack)
       */
      if (pc_overlays>0)
        pushval(0);
      /* the label to jump to is in stlist->name */
      ffcall(sym,itoh(stlist->label),0);
    } /* if */
  } /* if */

  if (flabel>=0)
    setlabel(flabel);           /* condition was false, jump around the state switch */

  #if !defined PAWN_LIGHT
    /* mark for documentation */
    if (sc_status==statFIRST) {
      char *str;
      /* get the last list id attached to the function, this contains the source states */
      assert(curfunc!=NULL);
      if (curfunc->states!=NULL) {
        stlist=curfunc->states->next;
        assert(stlist!=NULL);
        while (stlist->next!=NULL)
          stlist=stlist->next;
        listid=stlist->id;
      } else {
        listid=-1;
      } /* if */
      assert(state!=NULL && state->name!=NULL && strlen(state->name)>0 && strlen(state->name)<=sNAMEMAX);
      strcpy(name,state->name);
      length=strlen(name)+70; /* +70 for the fixed part "<transition ... />\n" */
      /* see if there are any condition strings to attach */
      for (index=0; (str=get_autolist(index))!=NULL; index++)
        length+=strlen(str);
      listindex=0;
      if ((doc=(char*)malloc(length*sizeof(char)))!=NULL) {
        do {
          sprintf(doc,"<transition target=\"%s\"",name);
          if (listid>=0) {
            /* get the source state */
            stateindex=state_listitem(listid,listindex);
            state=state_findid(stateindex);
            assert(state!=NULL);
            sprintf(doc+strlen(doc)," source=\"%s\"",state->name);
          } /* if */
          if (get_autolist(0)!=NULL) {
            /* add the condition */
            strcat(doc," condition=\"");
            for (index=0; (str=get_autolist(index))!=NULL; index++) {
              /* remove the ')' token that may be appended before detecting that the expression has ended */
              if (*str!=')' || *(str+1)!='\0' || get_autolist(index+1)!=NULL)
                strcat(doc,str);
            } /* for */
            strcat(doc,"\"");
          } /* if */
          strcat(doc,"/>\n");
          insert_docstring(doc,0);  /* prefix the state information */
        } while (listid>=0 && ++listindex<state_count(listid));
        free(doc);
      } /* if */
    } /* if */
  #endif
  delete_autolisttable();
}


static void addwhile(int *ptr)
{
  int k;

  ptr[wqBRK]=(int)declared;     /* stack pointer (for "break") */
  ptr[wqCONT]=(int)declared;    /* for "continue", possibly adjusted later */
  ptr[wqLOOP]=getlabel();
  ptr[wqEXIT]=getlabel();
  if (wqptr>=(wq+wqTABSZ-wqSIZE))
    error(102,"loop table");    /* loop table overflow (too many active loops)*/
  k=0;
  while (k<wqSIZE){     /* copy "ptr" to while queue table */
    *wqptr=*ptr;
    wqptr+=1;
    ptr+=1;
    k+=1;
  } /* while */
}

static void delwhile(void)
{
  if (wqptr>wq)
    wqptr-=wqSIZE;
}

static int *readwhile(void)
{
  if (wqptr<=wq){
    error(24);          /* out of context */
    return NULL;
  } else {
    return (wqptr-wqSIZE);
  } /* if */
}

