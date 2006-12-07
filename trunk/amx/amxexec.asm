;       AMXEXEC.ASM     Abstract Machine for the "Pawn" language
;

;Some notes:
; * Please also look at the "history" log (below) for more notes.
; * This file assembles with Watcom's WASM, Borland's TASM and Microsoft's
;   MASM.
; * There are two "calling convention" issues:
;   o  The convention with which amx_exec_asm() itself is called. The default
;      calling convention is Watcom's register calling convention. Change
;      this to __cdecl by setting the macro STACKARGS.
;   o  The convention for calling the "hook" functions (native functions and
;      the debugger callback). Again, the default is Watcom's register calling
;      convention. Use the macros CDECL or STDECL for __cdecl and __stdcall
;      respectively. (Since STDCALL is a reserved word on the assembler, I had
;      to choose a different name for the macro, hence STDECL.)
; * You will need to compile the standard AMX.C file with the macro ASM32
;   defined. On the command line, use:
;       wcl386 /l=nt /dASM32 srun.c amx.c amxcore.c amxcons.c amxexec.asm
; * OP_FILE and OP_CASETBL are not implemented, but they should not occur
;   anyway.
; * Not all opcodes are tested. Only those generated when using the demo
;   programs are giving the impression to be working. I've flagged them "good".
;   (I'm pretty sure now that all other codes are working correctly, too.)
; * On my P200 this implementation reaches >30 mill instr/sec - quite good.
;   It's 5 times as fast as the ANSI C version of SRUN.EXE (supplied with the
;   archive). (This was measured with the Hanoi "benchmark" [no output].)
;
;
;Copyright and license of use, please read
;-----------------------------------------
;The assembler implementation of the abstract machine for the Pawn language,
;specifically the file AMXEXEC.ASM, is copyright (c) 1998-2000 by Marc Peter.
;
;Permission is hereby granted, without written agreement and without paid
;license or royalty fees, to use, copy, modify, and distribute this software
;and its documentation for any purpose, subject to the following conditions:
;
;1. The above copyright notice and this permission notice shall appear in all
;   copies or substantial portions of this software.
;
;2. Modifications of this software that do not originate from me (Marc Peter)
;   must be explicitly mentioned in a README file or another appropriate
;   place.
;
;The use of this software as a subsystem of a larger software product is
;explicitly allowed, regardless of whether that larger product is proprietary,
;gratis or commercially available.
;
;I (Marc Peter) specifically disclaim any warranties, including, but not
;limited to, the implied warranties of merchantability and fitness for a
;particular purpose. The software is provided on an "as is" basis,
;and I have no obligation to provide maintenance, support, updates,
;enhancements or modifications.
;
;I cannot be held liable for any damage or loss of profits that results
;from the use of the software (or part thereof), or from the inability to
;use it.
;
;
;History (list of changes)
;-------------------------
; 14 december 2005  by Thiadmer Riemersma (TR)
;       Addition of macro instructions, to speed up instruction decoding
; 17 february 2005  by Thiadmer Riemersma (TR)
;       Addition of the BREAK opcode, removal of the older debugging opcode table.
;  6 march 2004  by Thiadmer Riemersma
;       Corrected a bug in OP_FILL, where a cell preceding the array would
;       be overwritten (zero'ed out). This bug was brought to my attention
;       by Robert Daniels.
;  2 february 2004  by Thiadmer Riemersma (TR)
;       Added checking of the return address in the RET and RETN opcodes.
;       Changed handling of LINE opcode, so that the debugger can force a
;       sleep.
; 22 december 2003  by Thiadmer Riemersma (TR)
;       Added support for the SYMTAG and SYSCALL.D opcodes
;  3 october 2003  by Thiadmer Riemersma (TR)
;       Added "non-debug" versions of various opcodes, to avoid repetitive
;       checking of the "debug" flag.
; 1 July 2002 by Thiadmer Riemersma (TR)
;       Modifications due to added fields in the AMX structure
;       RET and RETN instructions no longer check for a 0 return address
;       Stack top is now relative to the start of the data segment, to allow
;       for "cloned" AMX'es
;       The semantics of _CHKSTACK and _CHKMARGIN were inverted; now corrected
;       New opcodes
;       * OP_CALLN (but commented out, due to problems with the JIT)
;       * OP_NOP
;27 August 2001 by Dario Sacca (DS)
;       Corrected FRM parameter in the debug hook for the OP_LINE opcode.
;25 January 2001 by Thiadmer Riemersma (TR)
;       Implement the "sleep" functionality via the OP_HALT opcode; expand the
;       AMX structure for new required fields.
;21 December 2000 by Thiadmer Riemersma (TR)
;       Let the AMX abort if the debug hook function returns an error code
;       on the DBG_LINE code.
;19 October 2000 by Thiadmer Riemersma (TR)
;       Store return value of a function in the AMX structure when calling the
;       debug hook
; 9 September 2000 by Thiadmer Riemersma (TR)
;       Implemented new opcodes
;       * OP_SWAP_PRI
;       * OP_SWAP_ALT
;       * OP_PUSHADDR
;15 July 2000 By Thiadmer Riemersma (TR)
;       Store address of function being called in a debug parameter
;       Address for OP_CALL_PRI needs to be relocated
;15 November 1999 By Thiadmer Riemersma (TR)
;       Allow the parameter for the return value to be NULL.
;30 September 1999 By Thiadmer Riemersma (TR)
;       Implemented _stdcall calling convention
; 1 August 1999 By Thiadmer Riemersma (TR)
;       Implemented _cdecl calling convention
;15 June 1999   By Thiadmer Riemersma (TR)
;       Implemented new opcodes
;       * OP_SRANGE
;       * OP_JUMP_PRI
;       * OP_SWITCH
;       * OP_CASETBL
;       Referring to the "license of use" of this file (see above), please
;       note that these modifications are NOT by Marc Peter. I have marked
;       the locations of the changes with a comment line starting with "TR".
;28 March 1999
;       Here are some more bug fixes. I (Mark Peter) found these when
;       "designing" the code sequences for the JIT compiler. The Pawn compiler
;       doesn't seem to create these opcodes, or else the bugs would have
;       turned up earlier.
;       * LREF_S_ALT: garbled EAX and didn't work as expected
;       * in LCTRL: In the case of 'lctrl 6' no. 5 would have entered an
;         infinite loop because it jumped back to it's own compare instruction
;         and not to #6.
;       * in SCTRL: FRM was updated, but not EBX
;-----

.386            ; can't use .586p since TASM doesn't recognize it
.MODEL FLAT

IFDEF @Version  ; for Microsoft MASM 6.x
        OPTION  OLDSTRUCTS
        OPTION  M510
ENDIF

INCLUDE amxdef.asm

;#define PUSH(v)         ( stk-=sizeof(cell), *(cell *)(data+(int)stk)=v )
_PUSH   MACRO   v
        mov     [edi+ecx-4],v
        sub     ecx,4
        ENDM


;#define POP(v)          ( v=*(cell *)(data+(int)stk), stk+=sizeof(cell) )
_POP    MACRO   v
        mov     v,[edi+ecx]
        add     ecx,4
        ENDM

GO_ON   MACRO
        jmp     DWORD ptr [esi]
;       ALIGN   4
        ENDM


_CHKSTACK MACRO
        cmp     ecx,stp
        jg      err_stacklow
        ENDM

_CHKMARGIN MACRO
        lea     ebp,[ecx-16*4]  ;savety margin = 16 cells
        cmp     hea,ebp
        jg      err_stack
        ENDM

_CHKHEAP MACRO
        mov     ebp,amx
        mov     ebp,[ebp+_hlw]  ; MASM 6.x needs [ebp+amx_s._hlw]
        cmp     DWORD ptr hea,ebp
        jl      err_heaplow
        ENDM

_CHKDIVIDEZERO MACRO
        or      ebp,ebp         ; check for divide by zero
        jz      err_divide
        ENDM

_VERIFYADDRESS MACRO    adr     ; used in load.i, store.i & lidx
    local l1
        cmp     adr,stp         ; error if adr >= stp
        jae     err_memaccess
        cmp     adr,hea         ; so adr<stp, ok if adr<hea
        jb      short l1
        cmp     adr,ecx         ; so adr<stp and adr>=hea, ok if adr>=stk
        jb      err_memaccess
    l1:
        ENDM


IFDEF CDECL
    CDECL_STDCALL EQU 1
ENDIF
IFDEF STDECL
    CDECL_STDCALL EQU 1
ENDIF

_SAVEREGS MACRO                 ; save the registers (that may not be
    IFDEF CDECL_STDCALL         ; preserved under the rules of __cdecl or
        PUSHAD                  ; __stdcall calling conventions)
    ENDIF
    ENDM

_RESTOREREGS MACRO
    IFDEF CDECL_STDCALL
        POPAD
    ENDIF
    ENDM

_DROPARGS MACRO n               ; remove function arguments from the stack
    IFDEF CDECL                 ; (for __cdecl calling convention)
        add     esp,n
    ENDIF
    ENDM


.CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                               ;
;cell   asm_exec( cell *regs, cell *retval, cell stp, cell hea );
;                       eax         edx          ebx       ecx  ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

IFDEF STACKARGS

        PUBLIC  _amx_exec_asm
        PUBLIC  _amx_exec_asm@16
_amx_exec_asm@16 LABEL BYTE
_amx_exec_asm PROC

        push    ebx
        mov     eax,[esp+08h]
        mov     edx,[esp+0ch]
        mov     ebx,[esp+10h]
        mov     ecx,[esp+14h]

ELSE

        PUBLIC  amx_exec_asm_
amx_exec_asm_ PROC

ENDIF


        push    edi
        push    esi
        push    ebp

        sub     esp,4*3         ; place for PRI, ALT & STK at SYSREQs

        push    DWORD ptr [eax+20h]     ; store code size
        push    DWORD ptr [eax+1ch]     ; store pointer to code segment
        push    DWORD ptr [eax+18h]     ; store pointer to AMX
        push    edx                     ; store address of retval
        push    ebx                     ; store STP
        push    ecx                     ; store HEA
        push    DWORD ptr[eax+14h]      ; store FRM

        stk     EQU [esp+36]    ; define some aliases to registers
        alt     EQU [esp+32]    ;   that are stored on the stack
        pri     EQU [esp+28]
        codesiz EQU [esp+24]
        code    EQU [esp+20]
        amx     EQU [esp+16]
        retval  EQU [esp+12]
        stp     EQU [esp+8]
        hea     EQU [esp+4]
        frm     EQU [esp]       ; FRM is NOT stored in ebp, rather FRM+DAT
                                ; is being held in ebx.

        mov     edx,code        ; change the code size to an...
        add     codesiz,edx     ; ..."end of code" address

        mov     edx,[eax+04h]   ; get ALT
        mov     esi,[eax+08h]   ; get CIP
        mov     edi,[eax+0ch]   ; get pointer to data segment
        mov     ecx,[eax+10h]   ; get STK
        mov     ebx,[eax+14h]   ; get FRM
        mov     eax,[eax]       ; get PRI
        add     ebx,edi         ; relocate frame

        GO_ON                   ; start interpreting


OP_LOAD_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[edi+eax]
        GO_ON

OP_LOAD_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[edi+edx]
        GO_ON

;good
OP_LOAD_S_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[ebx+eax]
        GO_ON

;good
OP_LOAD_S_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[ebx+edx]
        GO_ON

OP_LOAD_I:
        add     esi,4
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        GO_ON

OP_LODB_I:
        _VERIFYADDRESS  eax
        mov     ebp,[esi+4]
        mov     eax,[edi+eax]           ;subject to misalignment stalls
        add     esi,8
        and     eax,DWORD ptr [(lodb_and-4)+ebp*4]
        GO_ON

OP_LREF_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[edi+eax]
        mov     eax,[edi+eax]
        GO_ON

OP_LREF_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[edi+edx]
        mov     edx,[edi+edx]
        GO_ON

OP_LREF_S_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[ebx+eax]
        mov     eax,[edi+eax]
        GO_ON

OP_LREF_S_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[ebx+edx]
        mov     edx,[edi+edx]
        GO_ON

;good
OP_CONST_PRI:
        mov     eax,[esi+4]
        add     esi,8
        GO_ON

;good
OP_CONST_ALT:
        mov     edx,[esi+4]
        add     esi,8
        GO_ON

;good
OP_ADDR_PRI:
        mov     eax,[esi+4]
        add     esi,8
        add     eax,frm
        GO_ON

;good
OP_ADDR_ALT:
        mov     edx,[esi+4]
        add     esi,8
        add     edx,frm
        GO_ON

OP_STOR_PRI:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+edi],eax
        GO_ON

OP_STOR_ALT:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+edi],edx
        GO_ON

;good
OP_STOR_S_PRI:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+ebx],eax
        GO_ON

;good
OP_STOR_S_ALT:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+ebx],edx
        GO_ON

;good
OP_STOR_I:
        add     esi,4
        _VERIFYADDRESS  edx
        mov     [edi+edx],eax
        GO_ON

OP_STRB_I:
        _VERIFYADDRESS  edx
        mov     ebp,[esi+4]
        add     esi,8
        cmp     ebp,1
        jne     short strb_not1byte
        mov     [edi+edx],al
        GO_ON
    strb_not1byte:
        cmp     ebp,4
        je      short strb_4byte
        mov     [edi+edx],ax
        GO_ON
    strb_4byte:
        mov     [edi+edx],eax
        GO_ON

OP_SREF_PRI:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[edi+ebp]
        mov     [edi+ebp],eax
        GO_ON

OP_SREF_ALT:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[edi+ebp]
        mov     [edi+ebp],edx
        GO_ON

OP_SREF_S_PRI:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebx+ebp]
        mov     [edi+ebp],eax
        GO_ON

OP_SREF_S_ALT:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebx+ebp]
        mov     [edi+ebp],edx
        GO_ON

;good
OP_LIDX:
        lea     eax,[edx+4*eax]
        add     esi,4
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        GO_ON

OP_LIDX_B:
        push    ecx
        mov     ecx,[esi+4]
        shl     eax,cl
        add     esi,8
        add     eax,edx
        pop     ecx
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        GO_ON

;good
OP_IDXADDR:
        add     esi,4
        lea     eax,[edx+4*eax]
        GO_ON

OP_IDXADDR_B:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     eax,cl
        pop     ecx
        add     eax,edx
        GO_ON

OP_ALIGN_PRI:
        mov     ebp,4   ; ??? one operation too many?
        sub     ebp,[esi+4]
        add     esi,8
        xor     eax,ebp
        GO_ON

OP_ALIGN_ALT:
        mov     ebp,4
        sub     ebp,[esi+4]
        add     esi,8
        xor     edx,ebp
        GO_ON

OP_LCTRL:
        mov     ebp,[esi+4]
        add     esi,8
        cmp     ebp,0
        jne     short lctrl_1
        mov     eax,code ; COD
        GO_ON
    lctrl_1:
        cmp     ebp,1
        jne     short lctrl_2
        mov     eax,edi  ; DAT
        GO_ON
    lctrl_2:
        cmp     ebp,2
        jne     short lctrl_3
        mov     eax,hea  ; 2=HEA
        GO_ON
    lctrl_3:
        cmp     ebp,3
        jne     short lctrl_4
        mov     ebp,amx
        mov     eax,stp
        GO_ON
    lctrl_4:
        cmp     ebp,4
        jne     short lctrl_5
        mov     eax,ecx  ; 4=STK
        GO_ON
    lctrl_5:
        cmp     ebp,5
        jne     short lctrl_6
        mov     eax,frm  ; 5=FRM
        GO_ON
    lctrl_6:
        mov     eax,esi  ; 6=CIP
        sub     eax,code
        GO_ON

OP_SCTRL:
        mov     ebp,[esi+4]
        add     esi,8
        cmp     ebp,2
        jne     short sctrl_4
        mov     hea,eax  ; 2=HEA
        GO_ON
    sctrl_4:
        cmp     ebp,4
        jne     short sctrl_5
        mov     ecx,eax  ; 4=STK
        GO_ON
    sctrl_5:
        cmp     ebp,5
        jne     short sctrl_6
        mov     ebx,eax  ; 5=FRM
        mov     frm,eax
        add     ebx,edi  ; relocate FRM
    sctrl_6:
        GO_ON

OP_MOVE_PRI:
        add     esi,4
        mov     eax,edx
        GO_ON

;good
OP_MOVE_ALT:
        add     esi,4
        mov     edx,eax
        GO_ON

OP_XCHG:
        add     esi,4
        xchg    eax,edx
        GO_ON

;good
OP_PUSH_PRI:
        add     esi,4
        _PUSH   eax
        GO_ON

;good
OP_PUSH_ALT:
        add     esi,4
        _PUSH   edx
        GO_ON

OP_PUSH_R_PRI:
        mov     ebp,[esi+4]
        add     esi,8
    push_loop:
        _PUSH   eax
        dec     ebp
        jnz     short push_loop
        GO_ON

;good
OP_PUSH_C:
        mov     ebp,[esi+4]
        add     esi,8
        _PUSH   ebp
        GO_ON

OP_PUSH:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        GO_ON

;good
OP_PUSH_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        GO_ON

OP_POP_PRI:
        add     esi,4
        _POP    eax
        GO_ON

;good
OP_POP_ALT:
        add     esi,4
        _POP    edx
        GO_ON

;good
OP_STACK:
        mov     edx,ecx
        add     ecx,[esi+4]
        _CHKMARGIN
        _CHKSTACK
        add     esi,8
        GO_ON

;good
OP_HEAP:
        mov     ebp,[esi+4]
        mov     edx,hea
        add     esi,8
        add     hea,ebp
        _CHKMARGIN
        _CHKHEAP
        GO_ON

;good
OP_PROC:
        mov     ebx,frm
        add     esi,4
        _PUSH   ebx
        mov     ebx,edi
        mov     frm,ecx
        add     ebx,ecx
        _CHKMARGIN
        GO_ON

OP_RET:
        _POP    ebx
        _POP    esi
        cmp     esi,code        ; verify ESI>=code
        jb      err_memaccess
        cmp     esi,codesiz     ; verify ESI<codesiz ("end-of-code" pointer)
        jae     err_memaccess
        mov     frm,ebx
        add     ebx,edi
        GO_ON

;good
OP_RETN:
        _POP    ebx
        _POP    esi
        cmp     esi,code        ; verify ESI>=code
        jb      err_memaccess
        cmp     esi,codesiz     ; verify ESI<codesiz ("end-of-code" pointer)
        jae     err_memaccess
        mov     frm,ebx
        add     ebx,edi
        mov     ebp,[edi+ecx]
        lea     ecx,[ecx+ebp+4]
        GO_ON

OP_CALL:
        lea     ebp,[esi+8]
        mov     esi,[esi+4]
        _PUSH   ebp
        GO_ON

OP_CALL_PRI:
        lea     ebp,[esi+4]
        mov     esi,eax
        add     esi,code        ; cip = PRI + code
        _PUSH   ebp
        GO_ON

;good
OP_JUMP:
        mov     esi,[esi+4]
        GO_ON

OP_JREL:
        add     esi,[esi+4]
        add     esi,8
        GO_ON

;good
OP_JZER:
        or      eax,eax
        jz      short jump_taken
        add     esi,8
        GO_ON

    jump_taken:
        mov     esi,[esi+4]
        GO_ON

;good
OP_JNZ:
        or      eax,eax
        jnz     short jump_taken
        add     esi,8
        GO_ON

;good
OP_JEQ:
        cmp     eax,edx
        je      short jump_taken
        add     esi,8
        GO_ON

OP_JNEQ:
        cmp     eax,edx
        jne     short jump_taken
        add     esi,8
        GO_ON

OP_JLESS:
        cmp     eax,edx
        jb      short jump_taken
        add     esi,8
        GO_ON

OP_JLEQ:
        cmp     eax,edx
        jbe     short jump_taken
        add     esi,8
        GO_ON

OP_JGRTR:
        cmp     eax,edx
        ja      short jump_taken
        add     esi,8
        GO_ON

OP_JGEQ:
        cmp     eax,edx
        jae     short jump_taken    ; unsigned comparison
        add     esi,8
        GO_ON

OP_JSLESS:
        cmp     eax,edx
        jl      short jump_taken
        add     esi,8
        GO_ON

;good
OP_JSLEQ:
        cmp     eax,edx
        jle     short jump_taken
        add     esi,8
        GO_ON

OP_JSGRTR:
        cmp     eax,edx
        jg      short jump_taken
        add     esi,8
        GO_ON

OP_JSGEQ:
        cmp     eax,edx
        jge     short jump_taken    ; signed comparison
        add     esi,8
        GO_ON

OP_SHL:
        push    ecx
        mov     ecx,edx
        add     esi,4
        shl     eax,cl
        pop     ecx
        GO_ON

OP_SHR:
        push    ecx
        mov     ecx,edx
        add     esi,4
        shr     eax,cl
        pop     ecx
        GO_ON

OP_SSHR:
        push    ecx
        mov     ecx,edx
        add     esi,4
        sar     eax,cl
        pop     ecx
        GO_ON

OP_SHL_C_PRI:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     eax,cl
        pop     ecx
        GO_ON

OP_SHL_C_ALT:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     edx,cl
        pop     ecx
        GO_ON

OP_SHR_C_PRI:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shr     eax,cl
        pop     ecx
        GO_ON

OP_SHR_C_ALT:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shr     edx,cl
        pop     ecx
        GO_ON

OP_SMUL:
        add     esi,4
        push    edx
        imul    edx
        pop     edx
        GO_ON

;good
OP_SDIV_ALT:
        xchg    eax,edx
        ALIGN   4

OP_SDIV:
        mov     ebp,edx
        xor     edx,eax         ; Check signs of the operands.
        cdq
        js      short sdiv_fiddle ; If the signs of the operands are different
                                ; we'll have to fiddle around to achieve
                                ; proper rounding towards minus infinity.
        _CHKDIVIDEZERO
        add     esi,4           ; default behavior is right in the other cases
        idiv    ebp
        GO_ON

    sdiv_fiddle:
        _CHKDIVIDEZERO
        idiv    ebp
        add     esi,4
        or      edx,edx
        jz      short sdiv_goon ; If there's no remainder the result is correct
        add     edx,ebp         ; else fix the result values.
        dec     eax             ; Amazing, how simple this is...
    sdiv_goon:
        GO_ON

OP_UMUL:
        add     esi,4
        push    edx
        mul     edx
        pop     edx
        GO_ON

OP_UDIV:
        mov     ebp,edx
        sub     edx,edx
        _CHKDIVIDEZERO
        add     esi,4
        div     ebp
        GO_ON

OP_UDIV_ALT:
        mov     ebp,eax
        mov     eax,edx
        sub     edx,edx
        _CHKDIVIDEZERO
        add     esi,4
        div     ebp
        GO_ON

;good
OP_ADD:
        add     esi,4
        add     eax,edx
        GO_ON

;good
OP_SUB:
        add     esi,4
        sub     eax,edx
        GO_ON

;good
OP_SUB_ALT:
        neg     eax
        add     esi,4
        add     eax,edx
        GO_ON

OP_AND:
        add     esi,4
        and     eax,edx
        GO_ON

OP_OR:
        add     esi,4
        or      eax,edx
        GO_ON

OP_XOR:
        add     esi,4
        xor     eax,edx
        GO_ON

OP_NOT:
        add     esi,4
        neg     eax             ; sets CF iff EAX != 0
        sbb     eax,eax         ; EAX == -1 iff CF set (zero otherwise)
        inc     eax             ; -1 => 0 and 0 => 1
        GO_ON

OP_NEG:
        add     esi,4
        neg     eax
        GO_ON

OP_INVERT:
        add     esi,4
        not     eax
        GO_ON

;good
OP_ADD_C:
        add     eax,[esi+4]
        add     esi,8
        GO_ON

;good
OP_SMUL_C:
        mov     ebp,[esi+4]
        push    edx
        imul    ebp
        pop     edx
        add     esi,8
        GO_ON

;good
OP_ZERO_PRI:
        add     esi,4
        sub     eax,eax
        GO_ON

;good
OP_ZERO_ALT:
        add     esi,4
        sub     edx,edx
        GO_ON

OP_ZERO:
        mov     ebp,[esi+4]
        add     esi,8
        mov     DWORD ptr [edi+ebp],0
        GO_ON

OP_ZERO_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     DWORD ptr [ebx+ebp],0
        GO_ON

OP_SIGN_PRI:
        shl     eax,24
        add     esi,4
        sar     eax,24
        GO_ON

OP_SIGN_ALT:
        shl     edx,24
        add     esi,4
        sar     edx,24
        GO_ON

OP_EQ:
        add     esi,4
        cmp     eax,edx         ; PRI == ALT ?
        mov     eax,0
        sete    al
        GO_ON

OP_NEQ:
        add     esi,4
        cmp     eax,edx         ; PRI != ALT ?
        mov     eax,0
        setne   al
        GO_ON

OP_LESS:
        add     esi,4
        cmp     eax,edx         ; PRI < ALT ? (unsigned)
        mov     eax,0
        setb    al
        GO_ON

OP_LEQ:
        add     esi,4
        cmp     eax,edx         ; PRI <= ALT ? (unsigned)
        mov     eax,0
        setbe   al
        GO_ON

OP_GRTR:
        add     esi,4
        cmp     eax,edx         ; PRI > ALT ? (unsigned)
        mov     eax,0
        seta    al
        GO_ON

OP_GEQ:
        add     esi,4
        cmp     eax,edx         ; PRI >= ALT ? (unsigned)
        mov     eax,0
        setae   al
        GO_ON

;good
OP_SLESS:
        add     esi,4
        cmp     eax,edx         ; PRI < ALT ? (signed)
        mov     eax,0
        setl    al
        GO_ON

OP_SLEQ:
        add     esi,4
        cmp     eax,edx         ; PRI <= ALT ? (signed)
        mov     eax,0
        setle   al
        GO_ON

OP_SGRTR:
        add     esi,4
        cmp     eax,edx         ; PRI > ALT ? (signed)
        mov     eax,0
        setg    al
        GO_ON

OP_SGEQ:
        add     esi,4
        cmp     eax,edx         ; PRI >= ALT ? (signed)
        mov     eax,0
        setge   al
        GO_ON

OP_EQ_C_PRI:
        cmp     eax,[esi+4]     ; PRI == value ?
        lea     esi,[esi+8]
        mov     eax,0
        sete    al
        GO_ON

OP_EQ_C_ALT:
        xor     eax,eax
        cmp     edx,[esi+4]     ; ALT == value ?
        lea     esi,[esi+8]
        sete    al
        GO_ON

OP_INC_PRI:
        add     esi,4
        inc     eax
        GO_ON

OP_INC_ALT:
        add     esi,4
        inc     edx
        GO_ON

OP_INC:
        mov     ebp,[esi+4]
        add     esi,8
        inc     DWORD ptr [edi+ebp]
        GO_ON

;good
OP_INC_S:
        mov     ebp,[esi+4]
        add     esi,8
        inc     DWORD ptr [ebx+ebp]
        GO_ON

OP_INC_I:
        add     esi,4
        inc     DWORD ptr [edi+eax]
        GO_ON

OP_DEC_PRI:
        add     esi,4
        dec     eax
        GO_ON

OP_DEC_ALT:
        add     esi,4
        dec     edx
        GO_ON

OP_DEC:
        mov     ebp,[esi+4]
        add     esi,8
        dec     DWORD ptr [edi+ebp]
        GO_ON

OP_DEC_S:
        mov     ebp,[esi+4]
        add     esi,8
        dec     DWORD ptr [ebx+ebp]
        GO_ON

OP_DEC_I:
        add     esi,4
        sub     DWORD ptr [edi+eax],1
        GO_ON

OP_MOVS:
        _VERIFYADDRESS  eax             ; PRI
        _VERIFYADDRESS  edx             ; ALT
        mov     ebp,eax
        add     ebp,[esi+4]
        dec     ebp
        _VERIFYADDRESS  ebp             ; PRI + size - 1
        mov     ebp,edx
        add     ebp,[esi+4]
        dec     ebp
        _VERIFYADDRESS  ebp             ; ALT + size - 1

        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        push    edi
        push    esi
        lea     esi,[edi+eax]
        lea     edi,[edi+edx]

        push    ecx
        shr     ecx,2
        rep movsd
        pop     ecx
        and     ecx,3
        rep movsb

        pop     esi
        pop     edi
        pop     ecx
        GO_ON

OP_CMPS:
        _VERIFYADDRESS  eax             ; PRI
        _VERIFYADDRESS  edx             ; ALT
        mov     ebp,eax
        add     ebp,[esi+4]             ; size in bytes
        dec     ebp                     ; EBP = PRI + size - 1
        _VERIFYADDRESS  ebp             ; PRI + size - 1
        sub     ebp,eax                 ; EBP = size - 1
        add     ebp,edx                 ; EBP = ALT + size - 1
        _VERIFYADDRESS  ebp             ; ALT + size - 1

        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        push    edi
        push    esi
        lea     esi,[edi+edx]
        lea     edi,[edi+eax]

        xor     eax,eax
        repe cmpsb
        je      short cmps1
        sbb     eax,eax
        sbb     eax,0ffffffffh
    cmps1:
        pop     esi
        pop     edi
        pop     ecx
        GO_ON


OP_FILL:
        mov     ebp,[esi+4]             ; get byte count
        add     esi,8
        and     ebp,0fffffffch          ; align to words
        jz      short fill_ready
        _VERIFYADDRESS  edx             ; ALT
        dec     ebp                     ; EBP = size - 1
        add     ebp,edx                 ; EBP = ALT + size - 1
        _VERIFYADDRESS  ebp             ; ALT + size - 1
        sub     ebp,edx                 ; restore EBP
        inc     ebp

        push    ecx
        push    edi
        mov     ecx,ebp                 ; ECX = count (in bytes)
        lea     edi,[edi+edx]           ; EDI = physical starting address
        shr     ecx,2                   ; ECX = count (in DWORDS)
        rep stosd
        pop     edi
        pop     ecx
    fill_ready:
        GO_ON


OP_HALT:
        cmp     DWORD ptr retval,0
        je      short halt_no_retval
        mov     ebp,retval
        mov     [ebp],eax
    halt_no_retval:
        ; store the complete status in the AMX
        mov     ebp,amx         ; get amx into ebp
        mov     [ebp+_pri],eax  ; store values in AMX structure (PRI, ALT, STK, HEA, FRM, ...)
        mov     [ebp+_alt],edx
        mov     [ebp+_stk],ecx
        mov     ecx,hea
        mov     ebx,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],ebx  ; EBX & ECX are invalid by now
        mov     ebx,[esi+4]     ; EBX=parameter of the HALT opcode
        add     esi,8           ; skip this instruction
        mov     eax,esi         ; EAX=CIP
        sub     eax,code
        mov     [ebp+_cip],eax
        mov     eax,ebx         ; return the parameter of the HALT opcode
        jmp     _return


OP_BOUNDS:
        mov     ebp,[esi+4]
        add     esi,8
        cmp     eax,ebp
        ja      err_bounds      ; use unsigned comparison, so <0 is >bounds
        GO_ON


OP_SYSREQ_C:
        mov     eax,[esi+4]     ; get function number
        add     esi,4


OP_SYSREQ_PRI:
        mov     ebp,amx         ; get amx into ebp
        add     esi,4

        mov     stk,ecx         ; save STK
        mov     alt,edx         ; save ALT

        mov     [ebp+_stk],ecx  ; store values in AMX structure (STK, HEA, FRM)
        mov     ecx,hea
        mov     ebx,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],ebx  ; ebx & ecx are invalid by now

        mov     ebx,esi         ; also store CIP
        sub     ebx,code
        mov     [ebp+_cip],ebx

        mov     edx,eax         ; 2nd param: function number
        mov     eax,ebp         ; 1st param: amx
        mov     ecx,stk
        lea     ebx,pri         ; 3rd param: addr. of retval
        add     ecx,edi         ; 4th param: addr. of function parameters
IFDEF CDECL_STDCALL
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    ecx
        push    ebx
        push    edx
        push    eax
ENDIF
        call    [ebp+_callback]
        _DROPARGS 10h           ; remove arguments from stack
IFDEF CDECL_STDCALL
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
ENDIF
        cmp     eax,AMX_ERR_NONE
        jne     _return         ; return error code, if any

        mov     eax,pri         ; get retval into eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        GO_ON


OP_SYSREQ_N:
        mov     ebp,[esi+8]     ; get # of bytes passed as parameters
        mov     eax,[esi+4]     ; get function number
        _PUSH   ebp             ; push 2nd parameter
        add     esi,12
        mov     ebp,amx         ; get amx into ebp

        mov     stk,ecx         ; save STK
        mov     alt,edx         ; save ALT

        mov     [ebp+_stk],ecx  ; store values in AMX structure (STK, HEA, FRM)
        mov     ecx,hea
        mov     ebx,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],ebx  ; ebx & ecx are invalid by now

        mov     ebx,esi         ; also store CIP
        sub     ebx,code
        mov     [ebp+_cip],ebx

        mov     edx,eax         ; 2nd param: function number
        mov     eax,ebp         ; 1st param: amx
        mov     ecx,stk
        lea     ebx,pri         ; 3rd param: addr. of retval
        add     ecx,edi         ; 4th param: addr. of function parameters
IFDEF CDECL_STDCALL
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    ecx
        push    ebx
        push    edx
        push    eax
ENDIF
        call    [ebp+_callback]
        _DROPARGS 10h           ; remove arguments from stack
IFDEF CDECL_STDCALL
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
ENDIF
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        add     ecx,[esi-4]     ; remove "number of parameter bytes" from the stack
        add     ecx,4           ; also remove the extra DWORD pushed

        cmp     eax,AMX_ERR_NONE
        jne     _return         ; return error code, if any
        mov     eax,pri         ; get retval into eax (PRI)
        GO_ON


OP_SYSREQ_D:
        mov     ebx,[esi+4]     ; get function address
        mov     ebp,amx         ; get amx into ebp
        add     esi,8

        mov     stk,ecx         ; save STK
        mov     alt,edx         ; save ALT

        mov     [ebp+_stk],ecx  ; store values in AMX structure (STK, HEA, FRM)
        mov     ecx,hea
        mov     eax,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],eax  ; eax & ecx are invalid by now

        mov     eax,ebp         ; 1st param: amx
        mov     edx,stk
        add     edx,edi         ; 2nd param: addr. of function parameters
IFDEF CDECL_STDCALL
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    edx
        push    eax
ENDIF
        call    ebx             ; direct call
        _DROPARGS 8             ; remove arguments from stack
IFDEF CDECL_STDCALL
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
ENDIF
        cmp     [ebp+_error],AMX_ERR_NONE
        jne     _return         ; return error code, if any

        ; function result is in eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        GO_ON


OP_SYSREQ_ND:
        mov     ebp,[esi+8]     ; get # of bytes passed as parameters
        mov     ebx,[esi+4]     ; get function number
        _PUSH   ebp             ; push 2nd parameter
        add     esi,12
        mov     ebp,amx         ; get amx into ebp

        mov     stk,ecx         ; save STK
        mov     alt,edx         ; save ALT

        mov     [ebp+_stk],ecx  ; store values in AMX structure (STK, HEA, FRM)
        mov     ecx,hea
        mov     eax,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],eax  ; eax & ecx are invalid by now

        mov     eax,ebp         ; 1st param: amx
        mov     edx,stk
        add     edx,edi         ; 2nd param: addr. of function parameters
IFDEF CDECL_STDCALL
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    edx
        push    eax
ENDIF
        call    ebx             ; direct call
        _DROPARGS 8             ; remove arguments from stack
IFDEF CDECL_STDCALL
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
ENDIF
        ; function result is in eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        add     ecx,[esi-4]     ; remove "number of parameter bytes" from the stack
        add     ecx,4           ; also remove the extra DWORD pushed

        cmp     [ebp+_error],AMX_ERR_NONE
        jne     _return         ; return error code, if any
        GO_ON


OP_FILE:
        jmp     OP_INVALID


OP_LINE:
        add     esi,12
        GO_ON


OP_SYMBOL:
        add     esi,[esi+4]
        add     esi,8           ; skip "fixed" part
        GO_ON


OP_SRANGE:
        add     esi,12
        GO_ON


OP_SYMTAG:
        add     esi,8
        GO_ON


OP_JUMP_PRI:
        mov     esi,eax
        GO_ON


OP_SWITCH:
        push    ecx
        mov     ebp,[esi+4]     ; get offset of the switch table
        add     ebp,4           ; skip the "OP_CASETBL" opcode
        mov     ecx,[ebp]       ; ECX = number of records
        mov     esi,[ebp+4]     ; preset ESI to "none-matched" case
    op_switch_loop:
        or      ecx, ecx        ; number of records == 0?
        jz      short op_switch_end ; yes, no more records, exit loop
        add     ebp,8           ; skip previous record
        dec     ecx             ; already decrement cases to do
        cmp     eax,[ebp]       ; PRI == case label?
        jne     short op_switch_loop ; no, continue loop
        mov     esi,[ebp+4]     ; yes, get jump address and exit loop
    op_switch_end:
        pop     ecx
        GO_ON


OP_CASETBL:
        jmp     OP_INVALID


OP_SWAP_PRI:
        mov     ebp,[edi+ecx]
        add     esi,4
        mov     [edi+ecx],eax
        mov     eax,ebp
        GO_ON


OP_SWAP_ALT:
        mov     ebp,[edi+ecx]
        add     esi,4
        mov     [edi+ecx],edx
        mov     edx,ebp
        GO_ON


OP_PUSH_ADR:
        mov     ebp,[esi+4]
        add     esi,8
        add     ebp,frm
        _PUSH   ebp
        GO_ON


OP_NOP:
        add     esi,4
        GO_ON


OP_BREAK:
        mov     ebp,amx         ; get amx into ebp
        add     esi,4
        cmp     DWORD ptr [ebp+_debug], 0
        jnz     break_calldebug
        GO_ON                   ; debug hook not active, ignore

    break_calldebug:
        ; store the status in the AMX (FRM, STK, HEA, CIP, and PRI + ALT)
        mov     [ebp+_pri],eax
        mov     [ebp+_alt],edx  ; EAX and EDX are now free to use
        mov     eax,frm
        mov     edx,hea
        mov     [ebp+_frm],eax  ; store values in AMX structure (STK, FRM & HEA)
        mov     [ebp+_hea],edx
        mov     [ebp+_stk],ecx
        mov     eax,esi
        sub     eax,code        ; EAX = CIP (relative to start of code segment)
        mov     [ebp+_cip],eax
        ; call the debug hook
        mov     eax,ebp         ; 1st parm: amx
IFDEF CDECL_STDCALL
        _SAVEREGS
        push    eax
ENDIF
        call    [ebp+_debug]    ; call debug function
        _DROPARGS 4             ; remove arguments from stack
        cmp     eax,AMX_ERR_NONE
        je      short break_noabort; continue running
        mov     [ebp+_error],eax   ; save EAX (error code) before restoring all regs
        _RESTOREREGS               ; abort run, but restore stack first
        mov     eax,[ebp+_error]   ; get error code in EAX back again
        jmp     _return         ; return error code
    break_noabort:
        _RESTOREREGS
        mov     eax,[ebp+_pri]  ; restore PRI and ALT
        mov     edx,[ebp+_alt]
        GO_ON


OP_PUSH2_C:
        add     esi,12
        mov     ebp,[esi-8]
        _PUSH   ebp
        mov     ebp,[esi-4]
        _PUSH   ebp
        GO_ON

OP_PUSH2:
        add     esi,12
        mov     ebp,[esi-8]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        GO_ON

OP_PUSH2_S:
        add     esi,12
        mov     ebp,[esi-8]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        GO_ON

OP_PUSH2_ADR:
        add     esi,12
        mov     ebp,[esi-8]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-4]
        add     ebp,frm
        _PUSH   ebp
        GO_ON


OP_PUSH3_C:
        add     esi,16
        mov     ebp,[esi-12]
        _PUSH   ebp
        mov     ebp,[esi-8]
        _PUSH   ebp
        mov     ebp,[esi-4]
        _PUSH   ebp
        GO_ON

OP_PUSH3:
        add     esi,16
        mov     ebp,[esi-12]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        GO_ON

OP_PUSH3_S:
        add     esi,16
        mov     ebp,[esi-12]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        GO_ON

OP_PUSH3_ADR:
        add     esi,16
        mov     ebp,[esi-12]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-8]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-4]
        add     ebp,frm
        _PUSH   ebp
        GO_ON


OP_PUSH4_C:
        add     esi,20
        mov     ebp,[esi-16]
        _PUSH   ebp
        mov     ebp,[esi-12]
        _PUSH   ebp
        mov     ebp,[esi-8]
        _PUSH   ebp
        mov     ebp,[esi-4]
        _PUSH   ebp
        GO_ON

OP_PUSH4:
        add     esi,20
        mov     ebp,[esi-16]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-12]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        GO_ON

OP_PUSH4_S:
        add     esi,20
        mov     ebp,[esi-16]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-12]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        GO_ON

OP_PUSH4_ADR:
        add     esi,20
        mov     ebp,[esi-16]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-12]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-8]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-4]
        add     ebp,frm
        _PUSH   ebp
        GO_ON


OP_PUSH5_C:
        add     esi,24
        mov     ebp,[esi-20]
        _PUSH   ebp
        mov     ebp,[esi-16]
        _PUSH   ebp
        mov     ebp,[esi-12]
        _PUSH   ebp
        mov     ebp,[esi-8]
        _PUSH   ebp
        mov     ebp,[esi-4]
        _PUSH   ebp
        GO_ON

OP_PUSH5:
        add     esi,24
        mov     ebp,[esi-20]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-16]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-12]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        GO_ON

OP_PUSH5_S:
        add     esi,24
        mov     ebp,[esi-20]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-16]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-12]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-8]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        mov     ebp,[esi-4]
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        GO_ON

OP_PUSH5_ADR:
        add     esi,24
        mov     ebp,[esi-20]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-16]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-12]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-8]
        add     ebp,frm
        _PUSH   ebp
        mov     ebp,[esi-4]
        add     ebp,frm
        _PUSH   ebp
        GO_ON


OP_LOAD_BOTH:
        mov     eax,[esi+4]
        mov     edx,[esi+8]
        add     esi,12
        mov     eax,[edi+eax]
        mov     edx,[edi+edx]
        GO_ON


OP_LOAD_S_BOTH:
        mov     eax,[esi+4]
        mov     edx,[esi+8]
        add     esi,12
        mov     eax,[ebx+eax]
        mov     edx,[ebx+edx]
        GO_ON


OP_CONST:
        push    eax
        mov     ebp,[esi+4]
        mov     eax,[esi+8]
        add     esi,12
        mov     [ebp+edi],eax
        pop     eax
        GO_ON


OP_CONST_S:
        push    eax
        mov     ebp,[esi+4]
        mov     eax,[esi+8]
        add     esi,12
        mov     [ebp+ebx],eax
        pop     eax
        GO_ON


OP_INVALID:
        mov     eax,AMX_ERR_INVINSTR
        jmp     _return

err_call:
        mov     eax,AMX_ERR_CALLBACK
        jmp     _return

err_stack:
        mov     eax,AMX_ERR_STACKERR
        jmp     _return

err_stacklow:
        mov     eax,AMX_ERR_STACKLOW
        jmp     _return

err_memaccess:
        mov     eax,AMX_ERR_MEMACCESS
        jmp     _return

err_bounds:
        mov     eax,AMX_ERR_BOUNDS
        jmp     _return

err_heaplow:
        mov     eax,AMX_ERR_HEAPLOW
        jmp     _return

err_divide:
        mov     eax,AMX_ERR_DIVIDE
        jmp     _return


_return:
        ; save a few parameters, mostly for the "sleep"function
        mov     ebp,amx         ; get amx into ebp
        mov     [ebp+_pri],eax  ; store values in AMX structure (PRI, ALT)
        mov     [ebp+_alt],edx  ; store values in AMX structure (PRI, ALT)

        pop     esi             ; remove FRM from stack

        pop     ecx
        pop     ebx
        pop     edx

        pop     esi             ; remove pointer to amx from stack
        pop     esi             ; remove code segment pointer
        pop     esi             ; remove code size

        add     esp,4*3         ; place for PRI, ALT & STK at SYSREQs

        pop     ebp
        pop     esi
        pop     edi

IFDEF STACKARGS
        pop     ebx
ENDIF

        ret

IFDEF STACKARGS

_amx_exec_asm ENDP

ELSE

amx_exec_asm_ ENDP

ENDIF


.DATA
        ALIGN   4       ; This is essential to avoid misalignment stalls.

lodb_and DD     0ffh, 0ffffh, 0, 0ffffffffh

        PUBLIC  _amx_opcodelist
_amx_opcodelist DD OP_INVALID
        DD      OP_LOAD_PRI
        DD      OP_LOAD_ALT
        DD      OP_LOAD_S_PRI
        DD      OP_LOAD_S_ALT
        DD      OP_LREF_PRI
        DD      OP_LREF_ALT
        DD      OP_LREF_S_PRI
        DD      OP_LREF_S_ALT
        DD      OP_LOAD_I
        DD      OP_LODB_I
        DD      OP_CONST_PRI
        DD      OP_CONST_ALT
        DD      OP_ADDR_PRI
        DD      OP_ADDR_ALT
        DD      OP_STOR_PRI
        DD      OP_STOR_ALT
        DD      OP_STOR_S_PRI
        DD      OP_STOR_S_ALT
        DD      OP_SREF_PRI
        DD      OP_SREF_ALT
        DD      OP_SREF_S_PRI
        DD      OP_SREF_S_ALT
        DD      OP_STOR_I
        DD      OP_STRB_I
        DD      OP_LIDX
        DD      OP_LIDX_B
        DD      OP_IDXADDR
        DD      OP_IDXADDR_B
        DD      OP_ALIGN_PRI
        DD      OP_ALIGN_ALT
        DD      OP_LCTRL
        DD      OP_SCTRL
        DD      OP_MOVE_PRI
        DD      OP_MOVE_ALT
        DD      OP_XCHG
        DD      OP_PUSH_PRI
        DD      OP_PUSH_ALT
        DD      OP_PUSH_R_PRI   ; obsolete
        DD      OP_PUSH_C
        DD      OP_PUSH
        DD      OP_PUSH_S
        DD      OP_POP_PRI
        DD      OP_POP_ALT
        DD      OP_STACK
        DD      OP_HEAP
        DD      OP_PROC
        DD      OP_RET
        DD      OP_RETN
        DD      OP_CALL
        DD      OP_CALL_PRI
        DD      OP_JUMP
        DD      OP_JREL
        DD      OP_JZER
        DD      OP_JNZ
        DD      OP_JEQ
        DD      OP_JNEQ
        DD      OP_JLESS
        DD      OP_JLEQ
        DD      OP_JGRTR
        DD      OP_JGEQ
        DD      OP_JSLESS
        DD      OP_JSLEQ
        DD      OP_JSGRTR
        DD      OP_JSGEQ
        DD      OP_SHL
        DD      OP_SHR
        DD      OP_SSHR
        DD      OP_SHL_C_PRI
        DD      OP_SHL_C_ALT
        DD      OP_SHR_C_PRI
        DD      OP_SHR_C_ALT
        DD      OP_SMUL
        DD      OP_SDIV
        DD      OP_SDIV_ALT
        DD      OP_UMUL
        DD      OP_UDIV
        DD      OP_UDIV_ALT
        DD      OP_ADD
        DD      OP_SUB
        DD      OP_SUB_ALT
        DD      OP_AND
        DD      OP_OR
        DD      OP_XOR
        DD      OP_NOT
        DD      OP_NEG
        DD      OP_INVERT
        DD      OP_ADD_C
        DD      OP_SMUL_C
        DD      OP_ZERO_PRI
        DD      OP_ZERO_ALT
        DD      OP_ZERO
        DD      OP_ZERO_S
        DD      OP_SIGN_PRI
        DD      OP_SIGN_ALT
        DD      OP_EQ
        DD      OP_NEQ
        DD      OP_LESS
        DD      OP_LEQ
        DD      OP_GRTR
        DD      OP_GEQ
        DD      OP_SLESS
        DD      OP_SLEQ
        DD      OP_SGRTR
        DD      OP_SGEQ
        DD      OP_EQ_C_PRI
        DD      OP_EQ_C_ALT
        DD      OP_INC_PRI
        DD      OP_INC_ALT
        DD      OP_INC
        DD      OP_INC_S
        DD      OP_INC_I
        DD      OP_DEC_PRI
        DD      OP_DEC_ALT
        DD      OP_DEC
        DD      OP_DEC_S
        DD      OP_DEC_I
        DD      OP_MOVS
        DD      OP_CMPS
        DD      OP_FILL
        DD      OP_HALT
        DD      OP_BOUNDS
        DD      OP_SYSREQ_PRI
        DD      OP_SYSREQ_C
        DD      OP_FILE         ; obsolete
        DD      OP_LINE         ; obsolete
        DD      OP_SYMBOL       ; obsolete
        DD      OP_SRANGE       ; obsolete
        DD      OP_JUMP_PRI
        DD      OP_SWITCH
        DD      OP_CASETBL
        DD      OP_SWAP_PRI
        DD      OP_SWAP_ALT
        DD      OP_PUSH_ADR
        DD      OP_NOP
        DD      OP_SYSREQ_N
        DD      OP_SYMTAG       ; obsolete
        DD      OP_BREAK
        ; macro opcodes
        DD      OP_PUSH2_C
        DD      OP_PUSH2
        DD      OP_PUSH2_S
        DD      OP_PUSH2_ADR
        DD      OP_PUSH3_C
        DD      OP_PUSH3
        DD      OP_PUSH3_S
        DD      OP_PUSH3_ADR
        DD      OP_PUSH4_C
        DD      OP_PUSH4
        DD      OP_PUSH4_S
        DD      OP_PUSH4_ADR
        DD      OP_PUSH5_C
        DD      OP_PUSH5
        DD      OP_PUSH5_S
        DD      OP_PUSH5_ADR
        DD      OP_LOAD_BOTH
        DD      OP_LOAD_S_BOTH
        DD      OP_CONST
        DD      OP_CONST_S
        ; "patch" opcodes
        DD      OP_SYSREQ_D
        DD      OP_SYSREQ_ND

END
