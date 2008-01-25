/*  Pawn compiler - Error message system
 *  In fact a very simple system, using only 'panic mode'.
 *
 *  Copyright (c) ITB CompuPhase, 1997-2008
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
 *  Version: $Id: sc5.c 3902 2008-01-23 17:40:01Z thiadmer $
 */
#include <assert.h>
#if defined	__WIN32__ || defined _WIN32 || defined __MSDOS__
  #include <io.h>
#endif
#if defined __LINUX__ || defined __GNUC__
  #include <unistd.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>     /* ANSI standardized variable argument list functions */
#include <string.h>
#if defined FORTIFY
  #include <alloc/fortify.h>
#endif

#if defined __WIN32__ || defined _WIN32 || defined WIN32 || defined __NT__
  #define DLLEXPORT __declspec (dllexport)
#endif
#include "sc.h"

#if defined _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4125)  /* decimal digit terminates octal escape sequence */
#endif

#include "sc5.scp"

#if defined _MSC_VER
  #pragma warning(pop)
#endif

#define NUM_WARNINGS    (sizeof warnmsg / sizeof warnmsg[0])
static unsigned char warndisable[(NUM_WARNINGS + 7) / 8]; /* 8 flags in a char */

static int errflag;
static int errfile;
static int errstart;    /* line number at which the instruction started */
static int errline;     /* forced line number for the error message */

/*  error
 *
 *  Outputs an error message (note: msg is passed optionally).
 *  If an error is found, the variable "errflag" is set and subsequent
 *  errors are ignored until lex() finds a semicolon or a keyword
 *  (lex() resets "errflag" in that case).
 *
 *  Global references: inpfname   (reffered to only)
 *                     fline      (reffered to only)
 *                     fcurrent   (reffered to only)
 *                     errflag    (altered)
 */
SC_FUNC int error(long number,...)
{
static char *prefix[3]={ "error", "fatal error", "warning" };
static int lastline,errorcount;
static short lastfile;
  char *msg,*pre,*filename;
  va_list argptr;
  char string[256];
  int notice;

  /* split the error field between the real error/warning number and an optional
   * "notice" number
   */
  notice=number >> (sizeof(long)*4);
  number&=((unsigned long)~0) >> (sizeof(long)*4);
  assert(number>0 && number<300);

  /* errflag is reset on each semicolon.
   * In a two-pass compiler, an error should not be reported twice. Therefore
   * the error reporting is enabled only in the second pass (and only when
   * actually producing output). Fatal errors may never be ignored.
   */
  if ((errflag || sc_status!=statWRITE) && (number<100 || number>=200))
    return 0;

  /* also check for disabled warnings */
  if (number>=200) {
    int index=(number-200)/8;
    int mask=1 << ((number-200)%8);
    if ((warndisable[index] & mask)!=0) {
      errline=-1;
      errfile=-1;
      return 0;
    } /* if */
  } /* if */

  if (number<100){
    assert(number>0 && number<sizearray(errmsg));
    msg=errmsg[number];
    pre=prefix[0];
    errflag=TRUE;       /* set errflag (skip rest of erroneous expression) */
    errnum++;
  } else if (number<200) {
    assert((number-100)>=0 && (number-100)<sizearray(fatalmsg));
    msg=fatalmsg[number-100];
    pre=prefix[1];
    errnum++;           /* a fatal error also counts as an error */
  } else {
    assert((number-200)>=0 && (number-200)<sizearray(warnmsg));
    msg=warnmsg[number-200];
    pre=prefix[2];
    warnnum++;
  } /* if */

  strexpand(string,(unsigned char *)msg,sizeof string-2,SCPACK_TABLE);
  if (notice>0) {
    int len;
    assert(notice<sizearray(noticemsg));
    strcat(string,"; ");
    len=strlen(string);
    strexpand(string+len,(unsigned char *)noticemsg[notice],sizeof string-len-1,SCPACK_TABLE);
  } /* if */
  strcat(string,"\n");

  if (errline>0)
    errstart=errline;           /* forced error position, set single line destination */
  else
    errline=fline;              /* normal error, errstart may (or may not) have been marked, endpoint is current line */
  if (errstart>errline)
    errstart=errline;           /* special case: error found at end of included file */
  if (errfile>=0)
    filename=get_inputfile(errfile);/* forced filename */
  else
    filename=inpfname;          /* current file */
  assert(filename!=NULL);

  va_start(argptr,number);
  if (strlen(errfname)==0) {
    int start= (errstart==errline) ? -1 : errstart;
    if (pc_error((int)number,string,filename,start,errline,argptr)) {
      if (outf!=NULL) {
        pc_closeasm(outf,TRUE);
        outf=NULL;
      } /* if */
      longjmp(errbuf,3);        /* user abort */
    } /* if */
  } else {
    FILE *fp=fopen(errfname,"a");
    if (fp!=NULL) {
      if (errstart>=0 && errstart!=errline)
        fprintf(fp,"%s(%d -- %d) : %s %03d: ",filename,errstart,errline,pre,number);
      else
        fprintf(fp,"%s(%d) : %s %03d: ",filename,errline,pre,number);
      vfprintf(fp,string,argptr);
      fclose(fp);
    } /* if */
  } /* if */
  va_end(argptr);

  if (number>=100 && number<200 || errnum>25){
    if (strlen(errfname)==0) {
      va_start(argptr,number);
      pc_error(0,"\nCompilation aborted.",NULL,0,0,argptr);
      va_end(argptr);
    } /* if */
    if (outf!=NULL) {
      pc_closeasm(outf,TRUE);
      outf=NULL;
    } /* if */
    longjmp(errbuf,2);          /* fatal error, quit */
  } /* if */

  errline=-1;
  errfile=-1;
  /* check whether we are seeing many errors on the same line */
  if ((errstart<0 && lastline!=fline) || lastline<errstart || lastline>fline || fcurrent!=lastfile)
    errorcount=0;
  lastline=fline;
  lastfile=fcurrent;
  if (number<200)
    errorcount++;
  if (errorcount>=3)
    error(107);         /* too many error/warning messages on one line */

  return 0;
}

SC_FUNC int error_suggest(int number,const char *name,int ident)
{
  symbol *closestsym=find_closestsymbol(name,ident);
  if (closestsym!=NULL && strcmp(name,closestsym->name)!=0)
    error(makelong(number,1),name,closestsym->name);
  else
    error(number,name);
  return 0;
}

SC_FUNC void errorset(int code,int line)
{
  switch (code) {
  case sRESET:
    errflag=FALSE;      /* start reporting errors */
    break;
  case sFORCESET:
    errflag=TRUE;       /* stop reporting errors */
    break;
  case sEXPRMARK:
    errstart=fline;     /* save start line number */
    break;
  case sEXPRRELEASE:
    errstart=-1;        /* forget start line number */
    errline=-1;
    errfile=-1;
    break;
  case sSETLINE:
    errstart=-1;        /* force error line number, forget start line */
    errline=line;
    break;
  case sSETFILE:
    errfile=line;
    break;
  } /* switch */
}

/* sc_enablewarning()
 * Enables or disables a warning (errors cannot be disabled).
 * Initially all warnings are enabled. The compiler does this by setting bits
 * for the *disabled* warnings and relying on the array to be zero-initialized.
 *
 * Parameter enable can be:
 *  o  0 for disable
 *  o  1 for enable
 *  o  2 for toggle
 */
DLLEXPORT
#if defined __cplusplus
  extern "C"
#endif
int pc_enablewarning(int number,int enable)
{
  int index;
  unsigned char mask;

  if (number<200)
    return FALSE;       /* errors and fatal errors cannot be disabled */
  number -= 200;
  if (number>=NUM_WARNINGS)
    return FALSE;

  index=number/8;
  mask=(unsigned char)(1 << (number%8));
  switch (enable) {
  case 0:
    warndisable[index] |= mask;
    break;
  case 1:
    warndisable[index] &= (unsigned char)~mask;
    break;
  case 2:
    warndisable[index] ^= mask;
    break;
  } /* switch */

  return TRUE;
}

/* Implementation of Levenshtein distance, by Lorenzo Seidenari
 */
static int minimum(int a,int b,int c)
{
  int min=a;
  if(b<min)
    min=b;
  if(c<min)
    min=c;
  return min;
}

SC_FUNC int levenshtein_distance(const char *s,const char*t)
{
  //Step 1
  int k,i,j,cost,*d,distance;
  int n=strlen(s);
  int m=strlen(t);
  assert(n>0 && m>0);
  d=(int*)malloc((sizeof(int))*(m+1)*(n+1));
  m++;
  n++;
  //Step 2
  for (k=0;k<n;k++)
    d[k]=k;
  for (k=0;k<m;k++)
    d[k*n]=k;
  //Step 3 and 4
  for (i=1;i<n;i++) {
    for (j=1;j<m;j++) {
      //Step 5
      cost= (tolower(s[i-1])!=tolower(t[j-1]));
      //Step 6
      d[j*n+i]=minimum(d[(j-1)*n+i]+1,d[j*n+i-1]+1,d[(j-1)*n+i-1]+cost);
    } /* for */
  } /* for */
  distance=d[n*m-1];
  free(d);
  return distance;
}

static int find_closestsymbol_table(const char *name,const symbol *root,int symboltype,symbol **closestsym)
{
  int dist,closestdist=INT_MAX;
  char symname[2*sNAMEMAX+16];
  symbol *sym=root->next;
  int ident,critdist;

  assert(closestsym!=NULL);
  *closestsym=NULL;
  assert(name!=NULL);
  critdist=strlen(name)/2;  /* for short names, allow only a single edit */
  if (critdist>MAX_EDIT_DIST)
    critdist=MAX_EDIT_DIST;
  while (sym!=NULL) {
    funcdisplayname(symname,sym->name);
    ident=sym->ident;
    if (sym->ident==iCONSTEXPR || sym->ident==iREFERENCE || sym->ident==iARRAY || sym->ident==iREFARRAY)
      ident=iVARIABLE;
    if (symboltype==ident || (symboltype==iVARIABLE && ident==iFUNCTN)) {
      dist=levenshtein_distance(name,symname);
      if (dist<closestdist && dist<=critdist) {
        *closestsym=sym;
        closestdist=dist;
      } /* if */
    } /* if */
    sym=sym->next;
  } /* while */
  return closestdist;
}

SC_FUNC symbol *find_closestsymbol(const char *name,int symboltype)
{
  symbol *symloc,*symglb;
  int distloc,distglb;

  if (sc_status==statFIRST)
    return NULL;
  assert(name!=NULL);
  if (strlen(name)==0)
    return NULL;
  distloc=find_closestsymbol_table(name,&loctab,symboltype,&symloc);
  distglb=find_closestsymbol_table(name,&glbtab,symboltype,&symglb);
  return (distglb<distloc) ? symglb : symloc;
}

#undef SCPACK_TABLE
