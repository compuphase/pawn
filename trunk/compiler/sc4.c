/*  Pawn compiler - code generation (unoptimized "assembler" code)
 *
 *  Copyright (c) ITB CompuPhase, 1997-2012
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
 *  Version: $Id: sc4.c 4733 2012-06-22 08:39:46Z thiadmer $
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>     /* for _MAX_PATH */
#include <string.h>
#if defined FORTIFY
  #include <alloc/fortify.h>
#endif
#include "sc.h"

static int fcurseg;     /* the file number (fcurrent) for the active segment */


/* When a subroutine returns to address 0, the AMX must halt. In earlier
 * releases, the RET and RETN opcodes checked for the special case 0 address.
 * Today, the compiler simply generates a HALT instruction at address 0. So
 * a subroutine can savely return to 0, and then encounter a HALT.
 */
SC_FUNC void writeleader(symbol *root,int *lbl_nostate,int *lbl_ignorestate)
{
  symbol *sym;

  assert(code_idx==0);

  assert(lbl_nostate!=NULL);
  assert(lbl_ignorestate!=NULL);
  *lbl_nostate=0;
  *lbl_ignorestate=0;

  begcseg();
  pc_ovl0size[ovlEXIT][0]=(int)code_idx;  /* store offset to the special overlay */
  if (pc_optimize>=sOPTIMIZE_FULL) {
    stgwrite("\thalt.p 0\t;program exit point\n\n");
    code_idx+=opcodes(1);
  } else {
    stgwrite("\thalt 0\t;program exit point\n\n");
    code_idx+=opcodes(1)+opargs(1);     /* calculate code length */
  } /* if */
  pc_ovl0size[ovlEXIT][1]=(int)(code_idx-pc_ovl0size[ovlEXIT][0]); /* store overlay code size */

  /* check whether there are any functions that have states */
  for (sym=root->next; sym!=NULL; sym=sym->next)
    if (sym->ident==iFUNCTN && (sym->usage & (uPUBLIC | uREAD))!=0 && sym->states!=NULL)
      break;
  if (sym==NULL)
    return;             /* no function has states, nothing to do next */

  /* generate an error function that is called for an undefined state */
  pc_ovl0size[ovlNO_STATE][0]=(int)code_idx;
  stgwrite(";exit point for functions called from the wrong state\n");
  assert(lbl_nostate!=NULL);
  *lbl_nostate=getlabel();
  setlabel(*lbl_nostate);
  if (pc_optimize>=sOPTIMIZE_FULL) {
    assert(AMX_ERR_INVSTATE<((cell)1<<pc_cellsize*4));
    stgwrite("\thalt.p ");
    outval(AMX_ERR_INVSTATE,TRUE,TRUE);
    code_idx+=opcodes(1);
  } else {
    stgwrite("\thalt ");
    outval(AMX_ERR_INVSTATE,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);     /* calculate code length */
  } /* if */
  stgwrite("\n");
  pc_ovl0size[ovlNO_STATE][1]=(int)(code_idx-pc_ovl0size[ovlNO_STATE][0]);

  /* check whether there are "exit state" functions */
  for (sym=root->next; sym!=NULL; sym=sym->next)
    if (strcmp(sym->name,uEXITFUNC)==0)
      break;
  if (sym!=NULL) {
    /* generate a stub function that is called for an undefined exit state and
     * that returns immediately to the caller (no error)
     */
    pc_ovl0size[ovlEXITSTATE][0]=(int)code_idx;
    stgwrite(";catch-all for undefined exit states\n");
    assert(lbl_ignorestate!=NULL);
    *lbl_ignorestate=getlabel();
    setlabel(*lbl_ignorestate);
    /* the RET and IRETN instructions pop off the FRM register from the stack,
     * because they assume that a stack frame was set up; for this catch-all
     * routine (for exit states) we therefore need to set up a stack frame
     */
    stgwrite("\tproc\n");
    if (pc_overlays>0)
      stgwrite("\tretn.ovl\n");
    else
      stgwrite("\tret\n");
    code_idx+=opcodes(2);
    pc_ovl0size[ovlEXITSTATE][1]=(int)(code_idx-pc_ovl0size[ovlEXITSTATE][0]);
  } /* if */
}

/*  writetrailer
 *  Not much left of this once important function.
 *
 *  Global references: pc_stksize       (referred to only)
 *                     sc_dataalign     (referred to only)
 *                     code_idx         (altered)
 *                     glb_declared     (altered)
 */
SC_FUNC void writetrailer(void)
{
  assert(sc_dataalign % opcodes(1) == 0);   /* alignment must be a multiple of
                                             * the opcode size */
  assert(sc_dataalign!=0);

  /* pad code to align data segment */
  if ((code_idx % sc_dataalign)!=0) {
    begcseg();
    while ((code_idx % sc_dataalign)!=0)
      nooperation();
  } /* if */

  /* pad data segment to align the stack and the heap */
  assert(litidx==0);            /* literal queue should have been emptied */
  assert(sc_dataalign % pc_cellsize == 0);
  if (((glb_declared*pc_cellsize) % sc_dataalign)!=0) {
    begdseg();
    defstorage();
    while (((glb_declared*pc_cellsize) % sc_dataalign)!=0) {
      stgwrite("0 ");
      glb_declared++;
    } /* while */
  } /* if */

  stgwrite("\nSTKSIZE ");       /* write stack size (align stack top) */
  outval(pc_stksize - (pc_stksize % sc_dataalign),TRUE,TRUE);
}

/* writestatetables
 * Creates and dumps the state tables. Every function with states has a state
 * table that contains jump addresses (or overlay indices) the branch to the
 * appropriate function using the (hidden) state variable as the criterion.
 * Technically, this happens in a "switch" (or an "iswitch") instruction.
 * This function also creates the hidden state variables (one for each
 * automaton) in the data segment.
 */
SC_FUNC void writestatetables(symbol *root,int lbl_nostate,int lbl_ignorestate)
{
  int lbl_default,lbl_table,lbl_defnostate;
  int statecount;
  symbol *sym;
  constvalue *fsa, *state;
  statelist *stlist;
  int fsa_id,listid;

  assert(code_idx>0);   /* leader must already have been written */

  /* check whether there are any functions that have states */
  for (sym=root->next; sym!=NULL; sym=sym->next)
    if (sym->ident==iFUNCTN && (sym->usage & (uPUBLIC | uREAD))!=0 && sym->states!=NULL)
      break;
  if (sym==NULL)
    return;             /* no function has states, nothing to do next */

  assert(pc_ovl0size[ovlNO_STATE][0]>0); /* state exit point must already have been created */
  assert(pc_ovl0size[ovlNO_STATE][1]>0);

  /* write the "state-selectors" table with all automatons (update the
   * automatons structure too, as we are now assigning the address to
   * each automaton state-selector variable)
   */
  assert(glb_declared==0);
  begdseg();
  for (fsa=sc_automaton_tab.next; fsa!=NULL; fsa=fsa->next) {
    defstorage();
    stgwrite("0\t; automaton ");
    if (strlen(fsa->name)==0)
      stgwrite("(anonymous)");
    else
      stgwrite(fsa->name);
    stgwrite("\n");
    fsa->value=glb_declared*pc_cellsize;
    glb_declared++;
  } /* for */

  /* write stubs and jump tables for all state functions */
  begcseg();
  for (sym=root->next; sym!=NULL; sym=sym->next) {
    if (sym->ident==iFUNCTN && (sym->usage & (uPUBLIC | uREAD))!=0 && sym->states!=NULL) {
      stlist=sym->states->next;
      assert(stlist!=NULL);     /* there should be at least one state item */
      listid=stlist->id;
      assert(listid==-1 || listid>0);
      if (listid==-1 && stlist->next!=NULL) {
        /* first index is the "fallback", take the next one (if available) */
        stlist=stlist->next;
        listid=stlist->id;
      } /* if */
      if (listid==-1) {
        /* first index is the fallback, there is no second... */
        stlist->label=0;        /* insert dummy label number */
        /* this is an error, but we postpone adding the error message until the
         * function definition
         */
        continue;
      } /* if */
      /* generate label numbers for all statelist ids */
      for (stlist=sym->states->next; stlist!=NULL; stlist=stlist->next) {
        if (pc_overlays>0) {
          /* code overlay indices should already be set, see gen_ovlinfo() */
          assert(stlist->label>0);
        } else {
          assert(stlist->label==0);
          stlist->label=getlabel();
        } /* if */
      } /* for */
      if (strcmp(sym->name,uENTRYFUNC)==0)
        continue;               /* do not generate stubs for this special function */
      sym->addr=code_idx;       /* fix the function address now */
      /* get automaton id for this function */
      assert(listid>0);
      fsa_id=state_getfsa(listid);
      assert(fsa_id>=0);        /* automaton 0 exists */
      fsa=automaton_findid(fsa_id);
      /* count the number of states actually used; at the same time, check
       * whether there is a default (i.e. "fallback") state function
       */
      statecount=0;
      if (strcmp(sym->name,uEXITFUNC)==0) {
        lbl_default= (pc_overlays>0) ? ovlEXITSTATE : lbl_ignorestate;
      } else {
        lbl_defnostate= (pc_overlays>0) ? ovlNO_STATE : lbl_nostate;
        lbl_default=lbl_defnostate;
      } /* if */
      for (stlist=sym->states->next; stlist!=NULL; stlist=stlist->next) {
        if (stlist->id==-1) {
          lbl_default=stlist->label;
        } else {
          statecount+=state_count(stlist->id);
        } /* if */
      } /* for */
      /* generate a stub entry for the functions */
      stgwrite("\tload.pri ");
      outval(fsa->value,TRUE,FALSE);
      stgwrite("\t; ");
      stgwrite(sym->name);
      if (pc_overlays>0) {
        /* add overlay index */
        stgwrite("/");
        outval(sym->index,FALSE,FALSE);
      } /* if */
      stgwrite("\n");
      code_idx+=opcodes(1)+opargs(1);   /* calculate code length */
      lbl_table=getlabel();
      ffswitch(lbl_table,(pc_overlays>0));
      /* generate the jump table */
      setlabel(lbl_table);
      ffcase(statecount,lbl_default,TRUE,(pc_overlays>0));
      for (state=sc_state_tab.next; state!=NULL; state=state->next) {
        if (state->index==fsa_id) {
          /* find the label for this list id */
          for (stlist=sym->states->next; stlist!=NULL; stlist=stlist->next) {
            if (stlist->id!=-1 && state_inlist(stlist->id,(int)state->value)) {
              /* when overlays are used, the jump-label for the case statement
               * are overlay indices instead of code labels
               */
              ffcase(state->value,stlist->label,FALSE,(pc_overlays>0));
              break;
            } /* if */
          } /* for */
          if (stlist==NULL && lbl_default==lbl_defnostate)
            error(230,state->name,sym->name);  /* unimplemented state, no fallback */
        } /* if (state belongs to automaton of function) */
      } /* for (state) */
      stgwrite("\n");
      /* the jump table gets its own overlay index, and the size of the jump
       * table must therefore be known (i.e. update the codeaddr field of the
       * function with the address where the jump table ends)
       */
      sym->codeaddr=code_idx;
    } /* if (is function, used & having states) */
  } /* for (sym) */
}

/*
 *  Start (or restart) the CODE segment.
 *
 *  In fact, the code and data segment specifiers are purely informational;
 *  the "DUMP" instruction itself already specifies that the following values
 *  should go to the data segment. All other instructions go to the code
 *  segment.
 *
 *  Global references: curseg
 *                     fcurrent
 */
SC_FUNC void begcseg(void)
{
  if (sc_status!=statSKIP && (curseg!=sIN_CSEG || fcurrent!=fcurseg)) {
    stgwrite("\n");
    stgwrite("CODE ");
    outval(fcurrent,FALSE,FALSE);
    stgwrite("\t; ");
    outval(code_idx,TRUE,TRUE);
    curseg=sIN_CSEG;
    fcurseg=fcurrent;
  } /* endif */
}

/*
 *  Start (or restart) the DATA segment.
 *
 *  Global references: curseg
 */
SC_FUNC void begdseg(void)
{
  if (sc_status!=statSKIP && (curseg!=sIN_DSEG || fcurrent!=fcurseg)) {
    stgwrite("\n");
    stgwrite("DATA ");
    outval(fcurrent,FALSE,FALSE);
    stgwrite("\t; ");
    outval((glb_declared-litidx)*pc_cellsize,TRUE,TRUE);
    curseg=sIN_DSEG;
    fcurseg=fcurrent;
  } /* if */
}

SC_FUNC void setline(int chkbounds)
{
  if (sc_asmfile) {
    stgwrite("\t; line ");
    outval(fline,TRUE,TRUE);
  } /* if */
  if ((sc_debug & sSYMBOLIC)!=0 || chkbounds && (sc_debug & sCHKBOUNDS)!=0) {
    /* generate a "break" (start statement) opcode rather than a "line" opcode
     * because earlier versions of Small/Pawn have an incompatible version of the
     * line opcode
     */
    stgwrite("\tbreak\t; ");
    outval(code_idx,TRUE,TRUE);
    code_idx+=opcodes(1);
  } /* if */
}

SC_FUNC void setfiledirect(char *name)
{
  if (sc_status==statFIRST && sc_listing) {
    assert(name!=NULL);
    pc_writeasm(outf,"#file ");
    pc_writeasm(outf,name);
    pc_writeasm(outf,"\n");
  } /* if */
}

SC_FUNC void setlinedirect(int line)
{
  if (sc_status==statFIRST && sc_listing) {
    char string[40];
    sprintf(string,"#line %d\n",line);
    pc_writeasm(outf,string);
  } /* if */
}

/*  setlabel
 *
 *  Post a code label (specified as a number), on a new line.
 */
SC_FUNC void setlabel(int number)
{
  assert(number>=0);
  stgwrite("l.");
  stgwrite((char *)itoh(number));
  /* To assist verification of the assembled code, put the address of the
   * label as a comment. However, labels that occur inside an expression
   * may move (through optimization or through re-ordering). So write the
   * address only if it is known to accurate.
   */
  if (!staging) {
    stgwrite("\t\t; ");
    outval(code_idx,TRUE,FALSE);
  } /* if */
  stgwrite("\n");
}

/* Write a token that signifies the start or end of an expression or special
 * statement. This allows several simple optimizations by the peephole
 * optimizer.
 */
SC_FUNC void markexpr(optmark type,const char *name,cell offset)
{
  switch (type) {
  case sEXPR:
    stgwrite("\t;$exp\n");
    break;
  case sPARM:
    stgwrite("\t;$par\n");
    break;
  case sLDECL:
    assert(name!=NULL);
    stgwrite("\t;$lcl ");
    stgwrite(name);
    stgwrite(" ");
    outval(offset,TRUE,TRUE);
    break;
  default:
    assert(0);
  } /* switch */
}

/*  startfunc   - declare a CODE entry point (function start)
 *
 *  Global references: funcstatus  (referred to only)
 */
SC_FUNC void startfunc(const char *fname,int index)
{
  stgwrite("\tproc");
  if (sc_asmfile) {
    char symname[2*sNAMEMAX+16];
    funcdisplayname(symname,fname);
    stgwrite("\t; ");
    stgwrite(symname);
    if (pc_overlays>0) {
      /* add overlay index */
      stgwrite("/");
      outval(index,FALSE,FALSE);
    } /* if */
  } /* if */
  stgwrite("\n");
  code_idx+=opcodes(1);
}

/*  endfunc
 *
 *  Declare a CODE ending point (function end)
 */
SC_FUNC void endfunc(void)
{
  stgwrite("\n");       /* skip a line */
}

/*  alignframe
 *
 *  Aligns the frame (and the stack) of the current function to a multiple
 *  of the specified byte count. Two caveats: the alignment ("numbytes") should
 *  be a power of 2, and this alignment must be done right after the frame
 *  is set up (before the first variable is declared)
 */
SC_FUNC void alignframe(int numbytes)
{
  #if !defined NDEBUG
    /* "numbytes" should be a power of 2 for this code to work */
    int i,count=0;
    for (i=0; i<sizeof numbytes*8; i++)
      if (numbytes & (1 << i))
        count++;
    assert(count==1);
  #endif

  stgwrite("\tlctrl 4\n");      /* get STK in PRI */
  stgwrite("\tconst.alt ");     /* get ~(numbytes-1) in ALT */
  outval(~(numbytes-1),TRUE,TRUE);
  stgwrite("\tand\n");          /* PRI = STK "and" ~(numbytes-1) */
  stgwrite("\tsctrl 4\n");      /* set the new value of STK ... */
  stgwrite("\tsctrl 5\n");      /* ... and FRM */
  code_idx+=opcodes(5)+opargs(4);
}

/*  rvalue
 *
 *  Generate code to get the value of a symbol into "primary".
 */
SC_FUNC void rvalue(value *lval)
{
  symbol *sym;

  sym=lval->sym;
  if (lval->ident==iARRAYCELL) {
    /* indirect fetch, address already in PRI */
    stgwrite("\tload.i\n");
    code_idx+=opcodes(1);
  } else if (lval->ident==iARRAYCHAR) {
    /* indirect fetch of a character from a pack, address already in PRI */
    stgwrite("\tlodb.i ");
    outval(sCHARBITS/8,TRUE,TRUE);   /* read one or two bytes */
    code_idx+=opcodes(1)+opargs(1);
  } else if (lval->ident==iREFERENCE) {
    /* indirect fetch, but address not yet in PRI */
    assert(sym!=NULL);
    assert(sym->vclass==sLOCAL);/* global references don't exist in Pawn */
    if (sym->vclass==sLOCAL)
      stgwrite("\tlref.s.pri ");
    else
      stgwrite("\tlref.pri ");
    outval(sym->addr,TRUE,TRUE);
    markusage(sym,uREAD);
    code_idx+=opcodes(1)+opargs(1);
  } else {
    /* direct or stack relative fetch */
    assert(sym!=NULL);
    if (sym->vclass==sLOCAL)
      stgwrite("\tload.s.pri ");
    else
      stgwrite("\tload.pri ");
    outval(sym->addr,TRUE,TRUE);
    markusage(sym,uREAD);
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
}

/* Get the address of a symbol into the primary or alternate register (used
 * for arrays, and for passing arguments by reference).
 */
SC_FUNC void address(symbol *sym,regid reg)
{
  assert(sym!=NULL);
  assert(reg==sPRI || reg==sALT);
  /* the symbol can be a local array, a global array, or an array
   * that is passed by reference.
   */
  if (sym->ident==iREFARRAY || sym->ident==iREFERENCE) {
    /* reference to a variable or to an array; this is always a local variable */
    switch (reg) {
    case sPRI:
      stgwrite("\tload.s.pri ");
      break;
    case sALT:
      stgwrite("\tload.s.alt ");
      break;
    } /* switch */
  } else {
    /* a local array or local variable */
    switch (reg) {
    case sPRI:
      if (sym->vclass==sLOCAL)
        stgwrite("\taddr.pri ");
      else
        stgwrite("\tconst.pri ");
      break;
    case sALT:
      if (sym->vclass==sLOCAL)
        stgwrite("\taddr.alt ");
      else
        stgwrite("\tconst.alt ");
      break;
    } /* switch */
  } /* if */
  outval(sym->addr,TRUE,TRUE);
  markusage(sym,uREAD);
  code_idx+=opcodes(1)+opargs(1);
}

/*  store
 *
 *  Saves the contents of "primary" into a memory cell, either directly
 *  or indirectly (at the address given in the alternate register).
 */
SC_FUNC void store(value *lval)
{
  symbol *sym;

  sym=lval->sym;
  if (lval->ident==iARRAYCELL) {
    /* store at address in ALT */
    stgwrite("\tstor.i\n");
    code_idx+=opcodes(1);
  } else if (lval->ident==iARRAYCHAR) {
    /* store at address in ALT */
    stgwrite("\tstrb.i ");
    outval(sCHARBITS/8,TRUE,TRUE);   /* write one or two bytes */
    code_idx+=opcodes(1)+opargs(1);
  } else if (lval->ident==iREFERENCE) {
    assert(sym!=NULL);
    assert(sym->vclass==sLOCAL);
    stgwrite("\tsref.s ");
    outval(sym->addr,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } else {
    assert(sym!=NULL);
    markusage(sym,uWRITTEN);
    if (sym->vclass==sLOCAL)
      stgwrite("\tstor.s ");
    else
      stgwrite("\tstor ");
    outval(sym->addr,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
}

/* Get a cell from a fixed address in memory */
SC_FUNC void loadreg(cell address,regid reg)
{
  assert(reg==sPRI || reg==sALT);
  if (reg==sPRI)
    stgwrite("\tload.pri ");
  else
    stgwrite("\tload.alt ");
  outval(address,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* Store a cell into a fixed address in memory */
SC_FUNC void storereg(cell address,regid reg)
{
  assert(reg==sPRI);
  stgwrite("\tstor ");
  outval(address,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* Source must be in PRI, destination address in ALT. The "size"
 * parameter is in bytes, not cells.
 */
SC_FUNC void memcopy(cell size)
{
  stgwrite("\tmovs ");
  outval(size,TRUE,TRUE);

  code_idx+=opcodes(1)+opargs(1);
}

/* Address of the source must already have been loaded in PRI, the
 * destination address in ALT.
 * This routine makes a loop that copies the minor dimension vector
 * by vector.
 */
SC_FUNC void copyarray2d(int majordim,int minordim)
{
  int cellshift=(pc_cellsize==8) ? 3 : (pc_cellsize==4) ? 2 : 1;
  int looplbl=getlabel();

  stgwrite("\tpush.alt\n");
  stgwrite("\tpush.pri\n");
  code_idx+=opcodes(2);
  if (pc_optimize>=sOPTIMIZE_MACRO) {
    stgwrite("\tzero.alt\n");/* ALT = index = 0 */
    code_idx+=opcodes(1);
  } else {
    stgwrite("\tconst.alt 0\n");/* ALT = index = 0 */
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
  setlabel(looplbl);
  stgwrite("\tpush.alt\n"); /* save index */
  stgwrite("\tpick 8\n");   /* PRI = dest */
  code_idx+=opcodes(2)+opargs(1);
  if (pc_optimize>=sOPTIMIZE_MACRO) {
    stgwrite("\txchg\n");   /* ALT = dest, PRI = index */
    stgwrite("\tidxaddr\n");/* PRI = dest + index * sizeof(cell) */
    code_idx+=opcodes(2);
  } else {
    stgwrite("\tshl.c.alt "); outval(cellshift,TRUE,TRUE); /* ALT = index * sizeof(cell) */
    stgwrite("\tadd\n");    /* PRI = dest + index * sizeof(cell) */
    code_idx+=opcodes(2)+opargs(1);
  } /* if */
  stgwrite("\tpush.pri\n");
  stgwrite("\tload.i\n");   /* PRI = dest[index * sizeof(cell)] */
  stgwrite("\tpop.alt\n");  /* ALT = dest + index * sizeof(cell) */
  stgwrite("\tadd\n");      /* PRI = dest + index * sizeof(cell) + dest[index * sizeof(cell)] */
  stgwrite("\tpush.pri\n");
  code_idx+=opcodes(5);
  if (pc_optimize>=sOPTIMIZE_MACRO) {
    stgwrite("\tpick 8\n"); /* PRI = source */
    stgwrite("\txchg\n");   /* ALT = source */
    stgwrite("\tpick 4\n"); /* PRI = index */
    stgwrite("\tidxaddr\n");/* PRI = source + index * sizeof(cell) */
    code_idx+=opcodes(4)+opargs(2);
  } else {
    stgwrite("\tpick 4\n"); /* PRI = index */
    stgwrite("\txchg\n");   /* ALT = index */
    stgwrite("\tshl.c.alt "); outval(cellshift,TRUE,TRUE); /* ALT = index * sizeof(cell) */
    stgwrite("\tpick 8\n"); /* PRI = source */
    stgwrite("\tadd\n");    /* PRI = source + index * sizeof(cell) */
    code_idx+=opcodes(5)+opargs(3);
  } /* if */
  stgwrite("\tpush.pri\n");
  stgwrite("\tload.i\n");   /* PRI = source[index * sizeof(cell)] */
  stgwrite("\tpop.alt\n");  /* ALT = source + index * sizeof(cell) */
  stgwrite("\tadd\n");      /* PRI = source + index * sizeof(cell) + source[index * sizeof(cell)] */
  stgwrite("\tpop.alt\n");  /* ALT = dest + index * sizeof(cell) + dest[index * sizeof(cell)] */
  stgwrite("\tmovs "); outval(minordim*pc_cellsize,TRUE,TRUE);
  stgwrite("\tpop.alt\n");  /* ALT = saved index */
  stgwrite("\tinc.alt\n");  /* ALT = index + 1 */
  code_idx+=opcodes(8)+opargs(1);
  if (pc_optimize>=sOPTIMIZE_MACRO) {
    stgwrite("\teq.c.alt "); outval(majordim,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } else {
    stgwrite("\tconst.pri "); outval(majordim,TRUE,TRUE);
    stgwrite("\teq\n");     /* compare ALT with majordim */
    code_idx+=opcodes(2)+opargs(1);
  } /* if */
  stgwrite("\tjzer "); outval(looplbl,TRUE,TRUE);
  stgwrite("\tpop.pri\n");  /* restore stack & registers */
  stgwrite("\tpop.alt\n");
  code_idx+=opcodes(3)+opargs(1);
}

SC_FUNC void fillarray(symbol *sym,cell size,cell value)
{
  ldconst(value,sPRI);  /* load value in PRI */

  assert(sym!=NULL);
  /* the symbol can be a local array, a global array, or an array
   * that is passed by reference.
   */
  if (sym->ident==iREFARRAY) {
    /* reference to an array; currently this is always a local variable */
    assert(sym->vclass==sLOCAL);        /* symbol must be stack relative */
    stgwrite("\tload.s.alt ");
  } else {
    /* a local or global array */
    if (sym->vclass==sLOCAL)
      stgwrite("\taddr.alt ");
    else
      stgwrite("\tconst.alt ");
  } /* if */
  outval(sym->addr,TRUE,TRUE);

  assert(size>0);
  stgwrite("\tfill ");
  outval(size,TRUE,TRUE);

  code_idx+=opcodes(2)+opargs(2);
}

/* Instruction to get an immediate value into the primary or the alternate
 * register
 */
SC_FUNC void ldconst(cell val,regid reg)
{
  assert(reg==sPRI || reg==sALT);
  switch (reg) {
  case sPRI:
    if (val==0 && pc_optimize>=sOPTIMIZE_MACRO) {
      stgwrite("\tzero.pri\n");
      code_idx+=opcodes(1);
    } else {
      stgwrite("\tconst.pri ");
      outval(val,TRUE,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
    break;
  case sALT:
    if (val==0 && pc_optimize>=sOPTIMIZE_MACRO) {
      stgwrite("\tzero.alt\n");
      code_idx+=opcodes(1);
    } else {
      stgwrite("\tconst.alt ");
      outval(val,TRUE,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
    break;
  } /* switch */
}

/* Copy value in alternate register to the primary register */
SC_FUNC void swapregs(void)
{
  stgwrite("\txchg\n");
  code_idx+=opcodes(1)+opargs(0);
}

/* Push primary or the alternate register onto the stack
 */
SC_FUNC void pushreg(regid reg)
{
  assert(reg==sPRI || reg==sALT);
  switch (reg) {
  case sPRI:
    stgwrite("\tpush.pri\n");
    break;
  case sALT:
    stgwrite("\tpush.alt\n");
    break;
  } /* switch */
  code_idx+=opcodes(1);
}

/* Push primary register onto the stack, mark for run-time relocation
 */
SC_FUNC void pushreloc(void)
{
  stgwrite("\tpushr.pri\n");
  code_idx+=opcodes(1);
}

/*
 *  Push a constant value onto the stack
 */
SC_FUNC void pushval(cell val)
{
  if (pc_optimize>=sOPTIMIZE_MACRO) {
    stgwrite("\tpush.c ");
    outval(val,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } else {
    stgwrite("\tconst.pri ");
    outval(val,TRUE,TRUE);
    stgwrite("\tpush.pri\n");
    code_idx+=opcodes(2)+opargs(1);
  } /* if */
}

/* Pop stack into the primary or the alternate register
 */
SC_FUNC void popreg(regid reg)
{
  assert(reg==sPRI || reg==sALT);
  switch (reg) {
  case sPRI:
    stgwrite("\tpop.pri\n");
    break;
  case sALT:
    stgwrite("\tpop.alt\n");
    break;
  } /* switch */
  code_idx+=opcodes(1);
}

/*
 *  swap the top-of-stack with the value in primary register
 */
SC_FUNC void swap1(void)
{
  stgwrite("\tswap.pri\n");
  code_idx+=opcodes(1);
}

/* Switch statements
 * The "switch" statement generates a "case" table using the "CASE" opcode.
 * The case table contains a list of records, each record holds a comparison
 * value and a label to branch to on a match. The very first record is an
 * exception: it holds the size of the table (excluding the first record) and
 * the label to branch to when none of the values in the case table match.
 * The case table is sorted on the comparison value. This allows more advanced
 * abstract machines to sift the case table with a binary search.
 * The iswitch statement uses an icase table. The parameter of an iswitch is
 * still a (relative) code address.
 */
SC_FUNC void ffswitch(int label,int iswitch)
{
  if (iswitch)
    stgwrite("\tswitch.ovl ");
  else
    stgwrite("\tswitch ");
  outval(label,TRUE,TRUE);      /* the label is the address of the case table */
  code_idx+=opcodes(1)+opargs(1);
}

SC_FUNC void ffcase(cell value,int label,int newtable,int icase)
{
  if (newtable) {
    if (icase)
      stgwrite("\tcasetbl.ovl\n");
    else
      stgwrite("\tcasetbl\n");
    code_idx+=opcodes(1);
  } /* if */
  if (icase)
    stgwrite("\tcase.ovl ");
  else
    stgwrite("\tcase ");
  outval(value,TRUE,FALSE);
  stgwrite(" ");
  outval(label,TRUE,TRUE);
  code_idx+=opcodes(0)+opargs(2);
}

/*
 *  Call specified function
 */
SC_FUNC void ffcall(symbol *sym,const char *label,int numargs)
{
  char symname[2*sNAMEMAX+16];

  assert(sym!=NULL);
  assert(sym->ident==iFUNCTN);
  if (sc_asmfile)
    funcdisplayname(symname,sym->name);
  if ((sym->usage & uNATIVE)!=0) {
    /* reserve a SYSREQ id if called for the first time */
    assert(label==NULL);
    if (sc_status==statWRITE && (sym->usage & uREAD)==0 && sym->index>=0)
      sym->index=ntv_funcid++;
    stgwrite("\tsysreq ");
    outval(sym->index,TRUE,FALSE);
    if (sc_asmfile) {
      stgwrite("\t; ");
      stgwrite(symname);
    } /* if */
    stgwrite("\n"); /* write on a separate line, to mark a sequence point for the peephole optimizer */
    stgwrite("\tstack ");
    outval((numargs+1)*pc_cellsize,TRUE,TRUE);
    code_idx+=opcodes(2)+opargs(2);
  } else {
    /* normal function */
    if (pc_overlays>0)
      stgwrite("\tcall.ovl ");
    else
      stgwrite("\tcall ");
    if (pc_overlays>0) {
      if (label!=NULL)
        stgwrite(label);
      else
        outval(sym->index,TRUE,FALSE);
    } else {
      if (label!=NULL) {
        stgwrite("l.");
        stgwrite(label);
      } else {
        stgwrite(sym->name);
      } /* if */
    } /* if */
    if (sc_asmfile
        && (label!=NULL || pc_overlays>0
            || !isalpha(sym->name[0]) && sym->name[0]!='_'  && sym->name[0]!=sc_ctrlchar))
    {
      stgwrite("\t; ");
      stgwrite(symname);
    } /* if */
    stgwrite("\n");
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
}

/*  Return from function
 *
 *  Global references: funcstatus  (referred to only)
 */
SC_FUNC void ffret(int remparams)
{
  if (pc_overlays>0)
    stgwrite("\tretn.ovl\n");
  else if (remparams)
    stgwrite("\tretn\n");
  else
    stgwrite("\tret\n");
  code_idx+=opcodes(1);
}

SC_FUNC void ffabort(int reason)
{
  stgwrite("\thalt ");
  outval(reason,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

SC_FUNC void ffbounds(cell size)
{
  if ((sc_debug & sCHKBOUNDS)!=0) {
    stgwrite("\tbounds ");
    outval(size,TRUE,TRUE);
    code_idx+=opcodes(1)+opargs(1);
  } /* if */
}

/*
 *  Jump to local label number (the number is converted to a name)
 */
SC_FUNC void jumplabel(int number)
{
  stgwrite("\tjump ");
  outval(number,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/*
 *   Define storage (global and static variables)
 */
SC_FUNC void defstorage(void)
{
  stgwrite("dump ");
}

/*
 *  Inclrement/decrement stack pointer. Note that this routine does
 *  nothing if the delta is zero.
 */
SC_FUNC void modstk(int delta)
{
  if (delta) {
    cell crit=((cell)1<<pc_cellsize*4);
    if (!staging && pc_optimize>=sOPTIMIZE_FULL && delta>=-crit && delta<crit) {
      stgwrite("\tstack.p ");
      outval(delta,FALSE,TRUE);
      code_idx+=opcodes(1);
    } else {
      stgwrite("\tstack ");
      outval(delta,TRUE,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
  } /* if */
}

/* set the stack to a hard offset from the frame */
SC_FUNC void setstk(cell value)
{
  stgwrite("\tlctrl 5\n");      /* get FRM in PRI */
  assert(value<=0);             /* STK should always become <= FRM */
  if (value<0) {
    if (pc_optimize>=sOPTIMIZE_MACRO) {
      stgwrite("\tadd.c ");
      outval(value,TRUE,TRUE);  /* add (negative) offset */
      code_idx+=opcodes(1)+opargs(1);
    } else {
      stgwrite("\tconst.alt ");
      outval(value,TRUE,TRUE);
      stgwrite("\tadd\n");      /* add (negative) offset */
      code_idx+=opcodes(2)+opargs(1);
    } /* if */
  } /* if */
  stgwrite("\tsctrl 4\n");      /* store in STK */
  code_idx+=opcodes(2)+opargs(2);
}

SC_FUNC void modheap(int delta)
{
  if (delta) {
    cell crit=((cell)1<<pc_cellsize*4);
    if (!staging && pc_optimize>=sOPTIMIZE_FULL && delta>=-crit && delta<crit) {
      stgwrite("\theap.p ");
      outval(delta,FALSE,TRUE);
      code_idx+=opcodes(1);
    } else {
      stgwrite("\theap ");
      outval(delta,TRUE,TRUE);
      code_idx+=opcodes(1)+opargs(1);
    } /* if */
  } /* if */
}

SC_FUNC void setheap_pri(void)
{
  if (!staging && pc_optimize>=sOPTIMIZE_FULL) {
    stgwrite("\theap.p ");
    outval(pc_cellsize,FALSE,TRUE);
    code_idx+=opcodes(3);       /* the other 2 opcodes follow below */
  } else {
    stgwrite("\theap ");        /* ALT = HEA++ */
    outval(pc_cellsize,TRUE,TRUE);
    code_idx+=opcodes(3)+opargs(1); /* the other 2 opcodes follow below */
  } /* if */
  stgwrite("\tstor.i\n");       /* store PRI (default value) at address ALT */
  stgwrite("\txchg\n");         /* move ALT to PRI: PRI contains the address */
}

SC_FUNC void setheap(cell value)
{
  stgwrite("\tconst.pri ");     /* load default value in PRI */
  outval(value,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
  setheap_pri();
}

/*
 *  Convert a cell number to a "byte" address; i.e. double or quadruple
 *  the primary register.
 */
SC_FUNC void cell2addr(void)
{
  stgwrite("\tshl.c.pri ");
  if (pc_cellsize==2) {
    outval(1,TRUE,TRUE);
  } else if (pc_cellsize==4) {
    outval(2,TRUE,TRUE);
  } else {
    assert(pc_cellsize==8);
    outval(3,TRUE,TRUE);
  } /* if */
  code_idx+=opcodes(1)+opargs(1);
}

/*
 *  Double or quadruple the alternate register.
 */
SC_FUNC void cell2addr_alt(void)
{
  stgwrite("\tshl.c.alt ");
  if (pc_cellsize==2) {
    outval(1,TRUE,TRUE);
  } else if (pc_cellsize==4) {
    outval(2,TRUE,TRUE);
  } else {
    assert(pc_cellsize==8);
    outval(3,TRUE,TRUE);
  } /* if */
  code_idx+=opcodes(1)+opargs(1);
}

/* Convert "distance of addresses" to "number of cells" in between.
 * Or convert a number of packed characters to the number of cells (with
 * truncation).
 * The ALT register is be used as scratch.
 */
SC_FUNC void addr2cell(void)
{
  stgwrite("\tconst.alt ");
  if (pc_cellsize==2) {
    outval(1,TRUE,TRUE);
  } else if (pc_cellsize==4) {
    outval(2,TRUE,TRUE);
  } else {
    assert(pc_cellsize==8);
    outval(3,TRUE,TRUE);
  } /* if */
  stgwrite("\tshr\n");
  code_idx+=opcodes(2)+opargs(1);
}

/* Convert from character index to byte address. This routine does
 * nothing if a character has the size of a byte.
 */
SC_FUNC void char2addr(void)
{
  #if sCHARBITS==16
    stgwrite("\tshl.c.pri 1\n");
    code_idx+=opcodes(1)+opargs(1);
  #endif
}

/* Align PRI (which should hold a character index) to an address.
 * The first character in a "pack" occupies the highest bits of
 * the cell. This is at the lower memory address on Big Endian
 * computers and on the higher address on Little Endian computers.
 * The ALIGN.pri/alt instructions must solve this machine dependence;
 * that is, on Big Endian computers, ALIGN.pri/alt shuold do nothing
 * and on Little Endian computers they should toggle the address.
 */
SC_FUNC void charalign(void)
{
  stgwrite("\talign.pri ");
  outval(sCHARBITS/8,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* Add a constant to the primary register; the secondary register may be
 * used as scratch.
 */
SC_FUNC void addconst(cell value)
{
  if (value!=0) {
    if (pc_optimize>=sOPTIMIZE_MACRO) {
      cell crit=((cell)1<<pc_cellsize*4);
      if (pc_optimize>=sOPTIMIZE_FULL && value>=-crit && value<crit) {
        stgwrite("\tadd.p.c ");
        outval(value,TRUE,TRUE);
        code_idx+=opcodes(1);
      } else {
        stgwrite("\tadd.c ");
        outval(value,TRUE,TRUE);
        code_idx+=opcodes(1)+opargs(1);
      } /* if */
    } else {
      stgwrite("\tconst.alt ");
      outval(value,TRUE,TRUE);
      stgwrite("\tadd\n");
      code_idx+=opcodes(2)+opargs(1);
    } /* if */
  } /* if */
}

/*
 *  signed multiply of primary and secundairy registers (result in primary)
 */
SC_FUNC void os_mult(void)
{
  stgwrite("\tsmul\n");
  code_idx+=opcodes(1);
}

/*
 *  signed divide of alternate register by primary register (quotient in
 *  primary; remainder in alternate)
 */
SC_FUNC void os_div(void)
{
  stgwrite("\tsdiv\n");
  code_idx+=opcodes(1);
}

/*
 *  modulus of (alternate % primary), result in primary (signed)
 */
SC_FUNC void os_mod(void)
{
  stgwrite("\tsdiv\n");
  stgwrite("\txchg\n");         /* move ALT to PRI */
  code_idx+=opcodes(2);
}

/*
 *  Add primary and alternate registers (result in primary).
 */
SC_FUNC void ob_add(void)
{
  stgwrite("\tadd\n");
  code_idx+=opcodes(1);
}

/*
 *  subtract primary register from alternate register (result in primary)
 */
SC_FUNC void ob_sub(void)
{
  stgwrite("\tsub\n");
  code_idx+=opcodes(1);
}

/*
 *  arithmic shift left alternate register the number of bits
 *  given in the primary register (result in primary).
 *  There is no need for a "logical shift left" routine, since
 *  logical shift left is identical to arithmic shift left.
 */
SC_FUNC void ob_sal(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tshl\n");
  code_idx+=opcodes(2);
}

/*
 *  arithmic shift right alternate register the number of bits
 *  given in the primary register (result in primary).
 */
SC_FUNC void os_sar(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tsshr\n");
  code_idx+=opcodes(2);
}

/*
 *  logical (unsigned) shift right of the alternate register by the
 *  number of bits given in the primary register (result in primary).
 */
SC_FUNC void ou_sar(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tshr\n");
  code_idx+=opcodes(2);
}

/*
 *  inclusive "or" of primary and alternate registers (result in primary)
 */
SC_FUNC void ob_or(void)
{
  stgwrite("\tor\n");
  code_idx+=opcodes(1);
}

/*
 *  "exclusive or" of primary and alternate registers (result in primary)
 */
SC_FUNC void ob_xor(void)
{
  stgwrite("\txor\n");
  code_idx+=opcodes(1);
}

/*
 *  "and" of primary and secundairy registers (result in primary)
 */
SC_FUNC void ob_and(void)
{
  stgwrite("\tand\n");
  code_idx+=opcodes(1);
}

/*
 *  test ALT==PRI; result in primary register (1 or 0).
 */
SC_FUNC void ob_eq(void)
{
  stgwrite("\teq\n");
  code_idx+=opcodes(1);
}

SC_FUNC void oa_eq(cell size)
{
  stgwrite("\tcmps ");
  outval(size,TRUE,TRUE);
  stgwrite("\tnot\n");  /* CMPS results in zero if both arrays match, change it to 1 */
  code_idx+=opcodes(2)+opargs(1);
}

/*
 *  test ALT!=PRI
 */
SC_FUNC void ob_ne(void)
{
  stgwrite("\tneq\n");
  code_idx+=opcodes(1);
}

SC_FUNC void oa_ne(cell size)
{
  stgwrite("\tcmps ");
  outval(size,TRUE,TRUE); /* this leaves PRI == 0 when the arrays are equal */
  stgwrite("\tnot\n");    /* PRI == 1 if equal, 0 if different */
  stgwrite("\tnot\n");    /* PRI == 0 if equal, 1 = different */
  code_idx+=opcodes(3)+opargs(1);
}

/* The abstract machine defines the relational instructions so that PRI is
 * on the left side and ALT on the right side of the operator. For example,
 * SLESS sets PRI to either 1 or 0 depending on whether the expression
 * "PRI < ALT" is true.
 *
 * The compiler generates comparisons with ALT on the left side of the
 * relational operator and PRI on the right side. The XCHG instruction
 * prefixing the relational operators resets this. We leave it to the
 * peephole optimizer to choose more compact instructions where possible.
 */

/* Relational operator prefix for chained relational expressions. The
 * "suffix" code restores the stack.
 * For chained relational operators, the goal is to keep the comparison
 * result "so far" in PRI and the value of the most recent operand in
 * ALT, ready for a next comparison.
 * The "prefix" instruction pushed the comparison result (PRI) onto the
 * stack and moves the value of ALT into PRI. If there is a next comparison,
 * PRI can now serve as the "left" operand of the relational operator.
 */
SC_FUNC void relop_prefix(void)
{
  stgwrite("\tpush.pri\n");
  stgwrite("\txchg\n");
  code_idx+=opcodes(2);
}

SC_FUNC void relop_suffix(void)
{
  stgwrite("\tswap.alt\n");
  stgwrite("\tand\n");
  stgwrite("\tpop.alt\n");
  code_idx+=opcodes(3);
}

/*
 *  test ALT<PRI (signed)
 */
SC_FUNC void os_lt(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tsless\n");
  code_idx+=opcodes(2);
}

/*
 *  test ALT<=PRI (signed)
 */
SC_FUNC void os_le(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tsleq\n");
  code_idx+=opcodes(2);
}

/*
 *  test ALT>PRI (signed)
 */
SC_FUNC void os_gt(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tsgrtr\n");
  code_idx+=opcodes(2);
}

/*
 *  test ALT>=PRI (signed)
 */
SC_FUNC void os_ge(void)
{
  stgwrite("\txchg\n");
  stgwrite("\tsgeq\n");
  code_idx+=opcodes(2);
}

/*
 *  logical negation of primary register
 */
SC_FUNC void lneg(void)
{
  stgwrite("\tnot\n");
  code_idx+=opcodes(1);
}

/*
 *  two's complement primary register
 */
SC_FUNC void neg(void)
{
  stgwrite("\tneg\n");
  code_idx+=opcodes(1);
}

/*
 *  one's complement of primary register
 */
SC_FUNC void invert(void)
{
  stgwrite("\tinvert\n");
  code_idx+=opcodes(1);
}

/*
 *  nop
 */
SC_FUNC void nooperation(void)
{
  stgwrite("\tnop\n");
  code_idx+=opcodes(1);
}


/*  increment symbol
 */
SC_FUNC void inc(value *lval)
{
  symbol *sym;

  sym=lval->sym;
  if (lval->ident==iARRAYCELL) {
    /* indirect increment, address already in PRI */
    stgwrite("\tinc.i\n");
    code_idx+=opcodes(1);
  } else if (lval->ident==iARRAYCHAR) {
    /* indirect increment of single character, address already in PRI */
    stgwrite("\tpush.alt\n");
    stgwrite("\txchg\n");         /* ALT = address */
    stgwrite("\tlodb.i ");        /* read from ALT into PRI */
    outval(sCHARBITS/8,TRUE,TRUE);/* read one or two bytes */
    stgwrite("\tinc.pri\n");
    stgwrite("\tstrb.i ");        /* write PRI to ALT */
    outval(sCHARBITS/8,TRUE,TRUE);/* write one or two bytes */
    stgwrite("\txchg\n");         /* PRI = address (restored PRI) */
    stgwrite("\tpop.alt\n");
    code_idx+=opcodes(7)+opargs(2);
  } else if (lval->ident==iREFERENCE) {
    /* indirect increment, but address not yet in PRI */
    assert(sym!=NULL);
    stgwrite("\tpush.pri\n");
    assert(sym->vclass==sLOCAL);  /* global references don't exist in Pawn */
    stgwrite("\tload.s.pri ");
    outval(sym->addr,TRUE,TRUE);
    stgwrite("\tinc.i\n");
    stgwrite("\tpop.pri\n");
    code_idx+=opcodes(4)+opargs(1);
  } else {
    /* local or global variable */
    assert(sym!=NULL);
    stgwrite("\tpush.pri\n");
    if (sym->vclass==sLOCAL)
      stgwrite("\taddr.pri ");
    else
      stgwrite("\tconst.pri ");
    outval(sym->addr,TRUE,TRUE);
    stgwrite("\tinc.i\n");
    stgwrite("\tpop.pri\n");
    code_idx+=opcodes(4)+opargs(1);
  } /* if */
}

/*  decrement symbol
 *
 *  in case of an integer pointer, the symbol must be incremented by 2.
 */
SC_FUNC void dec(value *lval)
{
  symbol *sym;

  sym=lval->sym;
  if (lval->ident==iARRAYCELL) {
    /* indirect decrement, address already in PRI */
    stgwrite("\tdec.i\n");
    code_idx+=opcodes(1);
  } else if (lval->ident==iARRAYCHAR) {
    /* indirect decrement of single character, address already in PRI */
    stgwrite("\tpush.alt\n");
    stgwrite("\txchg\n");         /* ALT = address */
    stgwrite("\tlodb.i ");        /* read from ALT into PRI */
    outval(sCHARBITS/8,TRUE,TRUE);/* read one or two bytes */
    stgwrite("\tdec.pri\n");
    stgwrite("\tstrb.i ");        /* write PRI to ALT */
    outval(sCHARBITS/8,TRUE,TRUE);/* write one or two bytes */
    stgwrite("\txchg\n");         /* PRI = address (restored PRI) */
    stgwrite("\tpop.pri\n");
    code_idx+=opcodes(7)+opargs(2);
  } else if (lval->ident==iREFERENCE) {
    assert(sym!=NULL);
    stgwrite("\tpush.pri\n");
    assert(sym->vclass==sLOCAL);    /* global references don't exist in Pawn */
    stgwrite("\tload.s.pri ");
    outval(sym->addr,TRUE,TRUE);
    stgwrite("\tdec.i\n");
    stgwrite("\tpop.pri\n");
    code_idx+=opcodes(4)+opargs(1);
  } else {
    /* local or global variable */
    assert(sym!=NULL);
    stgwrite("\tpush.pri\n");
    if (sym->vclass==sLOCAL)
      stgwrite("\taddr.pri ");
    else
      stgwrite("\tconst.pri ");
    outval(sym->addr,TRUE,TRUE);
    stgwrite("\tdec.i\n");
    stgwrite("\tpop.pri\n");
    code_idx+=opcodes(4)+opargs(1);
  } /* if */
}

/*
 *  Jumps to "label" if PRI != 0
 */
SC_FUNC void jmp_ne0(int number)
{
  stgwrite("\tjnz ");
  outval(number,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/*
 *  Jumps to "label" if PRI == 0
 */
SC_FUNC void jmp_eq0(int number)
{
  stgwrite("\tjzer ");
  outval(number,TRUE,TRUE);
  code_idx+=opcodes(1)+opargs(1);
}

/* write a value in hexadecimal, either as a full cell or a half cell, and
 * optionally add a newline
 */
SC_FUNC void outval(cell val,int fullcell,int newline)
{
  char *str=itoh(val);
  #if !defined AMX_NO_PACKED_OPC
    if (!fullcell) {
      #if !defined NDEBUG
        assert(strlen(str)==2*pc_cellsize);
        assert((str[0]=='0' || str[0]=='f') && (str[1]=='0' || str[1]=='f'));
        if (pc_cellsize>=4)
          assert((str[2]=='0' || str[2]=='f') && (str[3]=='0' || str[3]=='f'));
        if (pc_cellsize>=8) {
          assert((str[4]=='0' || str[4]=='f') && (str[5]=='0' || str[5]=='f'));
          assert((str[6]=='0' || str[6]=='f') && (str[7]=='0' || str[7]=='f'));
        } /* if */
      #endif
      str+=pc_cellsize;
    } /* if */
  #else
    (void)fullcell;
  #endif
  stgwrite(str);
  if (newline)
    stgwrite("\n");
}

