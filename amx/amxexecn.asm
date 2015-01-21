;       AMXEXECN.ASM    Abstract Machine for the "Pawn" language
;

;Some notes:
; * This file was adapted from AMXEXEC.ASM (for MASM/TASM/WASM). This
;   version is for NASM (Netwide Assembler). NASM uses Intel syntax for
;   the mnemonics, but it is not compatible with MASM.
; * The "calling convention" is __cdecl for the amx_exec_asm() itself and
;   __cdecl or __stdcall for the native routines (the default is __cdecl,
;   define the macro STDECL to set __stdcall).
; * The borland compiler uses different segment definitions as Microsoft
;   Visual C/C++ and GNU GCC. To assemble the abstract machine with "Borland"
;   segments, add the definition "BORLAND" on the command line.
; * You will need to compile the standard AMX.C file with the macro AMX_ASM
;   defined. On the command line, use:
;       nasmw -Ox -f obj -d BORLAND amxexecn.asm
;       bcc32 -DAMX_ASM srun.c amx.c amxcore.c amxcons.c amxexecn.obj
;   or
;       nasmw -Ox -f win32 amxexecn.asm
;   or
;       nasm -Ox -f elf amxexecn.asm
;       gcc -o srun -DLINUX -DAMX_ASM -I../linux srun.c amx.c amxcore.c amxcons.c amxexecn.o
; * See the notes in AMXEXEC.ASM for more information and a change log).
;
;
;Copyright and license of use, please read
;-----------------------------------------
;This implementation of the abstract machine for NASM is a port of the
;abstract machine core written by Marc Peter.
;
;Copyright 2003-2013 ITB CompuPhase, portions copyright 1998-2000 by Marc Peter.
;
;Licensed under the Apache License, Version 2.0 (the "License"); you may not
;use this file except in compliance with the License. You may obtain a copy
;of the License at
;
;    http://www.apache.org/licenses/LICENSE-2.0
;
;Unless required by applicable law or agreed to in writing, software
;distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
;WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
;License for the specific language governing permissions and limitations
;under the License.
;
;
;History (list of changes)
;-------------------------
;  1 March 2010  by Thiadmer Riemersma
;       The instruction set has been reorganized: a minimal "core" instruction
;       set was picked and three supplemental instruction sets: overlay
;       instructions, macro instructions and packed instructions.
; 14 March 2009  by Thiadmer Riemersma
;       Addition of the relocation instructions (PUSHR_xxx).
; 26 August 2007  by Thiadmer Riemersma
;       Minor clean-up; removed unneeded parameter.
; 31 May 2007  by Thiadmer Riemersma
;       Added packed opcodes
; 30 April 2007  by Thiadmer Riemersma
;       Move to position-independent code (no more relocation needed for
;       branches).
;       Removed cases for obsolete instructions.
; 14 December 2005  by Thiadmer Riemersma
;       Addition of macro instructions, to speed up instruction decoding
; 17 February 2005  by Thiadmer Riemersma
;       Addition of the BREAK opcode, removal of the older debugging opcode table.
;  6 March 2004  by Thiadmer Riemersma
;       Corrected a bug in OP_FILL, where a cell preceding the array would
;       be overwritten (zero'ed out). This bug was brought to my attention
;       by Robert Daniels.
;  2 February 2004  by Thiadmer Riemersma
;       Added checking of the return address in the RET and RETN opcodes.
;       Changed handling of LINE opcode, so that the debugger can force a
;       sleep.
; 22 December 2003  by Thiadmer Riemersma
;       Added support for the SYMTAG and SYSCALL.D opcodes
;  3 October 2003  by Thiadmer Riemersma
;       Added "non-debug" versions of various opcodes, to avoid repetitive
;       checking of the "debug" flag.
; 15 September 2003 by Thiadmer Riemersma
;       Minor corrections, mostly to support older versions of NASM
; 26 January 2003 by Thiadmer Riemersma
;       Port to NASM
;-----

; Macro to begin a code segment
%macro Start_CODE 0
  %ifdef BORLAND
    segment _TEXT public align=1 class=CODE use32
  %else
    segment .text
  %endif
%endmacro

; Macro to begin a data segment
%macro Start_DATA 0
  %ifdef BORLAND
    segment _DATA public align=4 class=DATA use32
  %else
    segment .data
  %endif
%endmacro


%ifdef BORLAND
  Start_CODE
%endif

%include "amxdefn.asm"

%ifndef AMX_NO_PACKED_OPC
  %ifndef AMX_TOKENTHREADING
    %define AMX_TOKENTHREADING 1    ; packed opcodes require token threading
  %endif
%endif


;#define PUSH(v)         ( stk-=sizeof(cell), *(cell *)(data+(int)stk)=v )
%macro  _PUSH   1
        mov     [edi+ecx-4],%1
        sub     ecx,4
%endmacro


;#define POP(v)          ( v=*(cell *)(data+(int)stk), stk+=sizeof(cell) )
%macro  _POP    1
        mov     %1,[edi+ecx]
        add     ecx,4
%endmacro

%macro  NEXT    0
    %ifdef AMX_TOKENTHREADING
        mov     ebp, [esi]
      %ifndef AMX_NO_PACKED_OPC
        and     ebp, 0ffh
      %endif
        jmp     DWORD [opcodelist + 4*ebp]
    %else
      %ifndef AMX_NO_PACKED_OPC
        %error opcode packing requires token threading
      %endif
        ; direct threading
        jmp     DWORD [esi]
    %endif
;       ALIGN   4
%endmacro

%macro  JUMPREL 0
        add     esi,[esi+4]
%endmacro

%macro  GETPARAM_P 1
        ; ??? verify that %1 != esi
        mov     %1,[esi]
        sar     %1,16             ; shift-right, keeping the sign
%endmacro

%macro  _CHKSTACK 0
        cmp     ecx,stp
        jg      near err_stacklow
%endmacro

%macro  _CHKMARGIN 0
        lea     ebp,[ecx-16*4]  ;savety margin = 16 cells
        cmp     hea,ebp
        jg      near err_stack
%endmacro

%macro  _CHKHEAP 0
        mov     ebp,amx
        mov     ebp,[ebp+_hlw]
        cmp     DWORD hea,ebp
        jl      near err_heaplow
%endmacro

%macro  _CHKDIVIDEZERO 0
        or      ebp,ebp         ; check for divide by zero
        jz      near err_divide
%endmacro

%macro  _VERIFYADDRESS 1       ; used in load.i, store.i & lidx
        cmp     %1,stp         ; error if address >= stp
        jae     near err_memaccess
        cmp     %1,hea         ; so address<stp, ok if address<hea
        jb      short %%address_ok
        cmp     %1,ecx         ; so address<stp and address>=hea, ok if address>=stk
        jb      near err_memaccess
    %%address_ok:
%endmacro

%macro  _SAVEREGS 0             ; save the registers (that may not be
        PUSHAD                  ; __stdcall calling conventions)
%endmacro

%macro  _RESTOREREGS 0
        POPAD
%endmacro

%macro  _DROPARGS 1             ; remove function arguments from the stack
    %ifndef STDECL              ; (only for __cdecl calling convention)
        add     esp,%1
    %endif
%endmacro


Start_CODE

;---------------------------------------------------------------------------
;cell   asm_exec_run( AMX *amx, cell *retval, char *data )
;---------------------------------------------------------------------------

        GLOBAL  amx_exec_run
        GLOBAL  _amx_exec_run
amx_exec_run:
_amx_exec_run: ;PROC

        push    ebx
        mov     eax,[esp+08h]
        mov     edx,[esp+0ch]
        mov     ebx,[esp+10h]

        push    edi
        push    esi
        push    ebp

        sub     esp,4*3         ; place for PRI, ALT & STK at SYSREQs

        push    DWORD [eax+_codesize]   ; store code size
        push    DWORD [eax+_codeseg]    ; store pointer to code segment
        push    eax                     ; store pointer to AMX
        push    edx                     ; store address of retval
        push    DWORD [eax+_stp]        ; store STP
        push    DWORD [eax+_hea]        ; store HEA
        push    DWORD [eax+_frm]        ; store FRM

%define stk     [esp+36]        ; define some aliases to registers
%define alt     [esp+32]        ;   that are stored on the stack
%define pri     [esp+28]
%define codesiz [esp+24]
%define code    [esp+20]
%define amx     [esp+16]
%define retval  [esp+ 12]
%define stp     [esp+8]
%define hea     [esp+4]
%define frm     [esp]           ; FRM is NOT stored in ebp, rather FRM+DAT
                                ; is being held in ebx.

        mov     edi,ebx         ; get pointer to data segment
        mov     edx,[eax+_alt]  ; get ALT
        mov     esi,[eax+_cip]  ; get CIP
        mov     ecx,[eax+_stk]  ; get STK
        mov     ebx,[eax+_frm]  ; get FRM
        mov     eax,[eax+_pri]  ; get PRI
        add     ebx,edi         ; relocate frame
        add     esi,code        ; relocate code address

        NEXT                    ; start interpreting


OP_NOP:
        add     esi,4
        NEXT


OP_LOAD_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[edi+eax]
        NEXT


OP_LOAD_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[edi+edx]
        NEXT


OP_LOAD_S_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[ebx+eax]
        NEXT


OP_LOAD_S_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[ebx+edx]
        NEXT


OP_LREF_S_PRI:
        mov     eax,[esi+4]
        add     esi,8
        mov     eax,[ebx+eax]
        mov     eax,[edi+eax]
        NEXT


OP_LREF_S_ALT:
        mov     edx,[esi+4]
        add     esi,8
        mov     edx,[ebx+edx]
        mov     edx,[edi+edx]
        NEXT


OP_ALIGN_PRI:
        mov     ebp,4
        sub     ebp,[esi+4]
        add     esi,8
        xor     eax,ebp
        NEXT


OP_LCTRL:
        mov     ebp,[esi+4]
        add     esi,8
        cmp     ebp,0
        jne     short lctrl_1
        mov     eax,code ; COD
        NEXT
    lctrl_1:
        cmp     ebp,1
        jne     short lctrl_2
        mov     eax,edi  ; DAT
        NEXT
    lctrl_2:
        cmp     ebp,2
        jne     short lctrl_3
        mov     eax,hea  ; 2=HEA
        NEXT
    lctrl_3:
        cmp     ebp,3
        jne     short lctrl_4
        mov     ebp,amx
        mov     eax,stp
        NEXT
    lctrl_4:
        cmp     ebp,4
        jne     short lctrl_5
        mov     eax,ecx  ; 4=STK
        NEXT
    lctrl_5:
        cmp     ebp,5
        jne     short lctrl_6
        mov     eax,frm  ; 5=FRM
        NEXT
    lctrl_6:
        mov     eax,esi  ; 6=CIP
        sub     eax,code
        NEXT


OP_SCTRL:
        mov     ebp,[esi+4]
        add     esi,8
        cmp     ebp,2
        jne     short sctrl_4
        mov     hea,eax  ; 2=HEA
        NEXT
    sctrl_4:
        cmp     ebp,4
        jne     short sctrl_5
        mov     ecx,eax  ; 4=STK
        NEXT
    sctrl_5:
        cmp     ebp,5
        jne     short sctrl_6
        mov     ebx,eax  ; 5=FRM
        mov     frm,eax
        add     ebx,edi  ; relocate FRM
    sctrl_6:
        NEXT


OP_XCHG:
        add     esi,4
        xchg    eax,edx
        NEXT


OP_PUSH_PRI:
        add     esi,4
        _PUSH   eax
        NEXT


OP_PUSH_ALT:
        add     esi,4
        _PUSH   edx
        NEXT


OP_LOAD_I:
        add     esi,4
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        NEXT


OP_LODB_I:
        _VERIFYADDRESS  eax
        mov     ebp,[esi+4]
        mov     eax,[edi+eax]           ;subject to misalignment stalls
        add     esi,8
        and     eax,DWORD [(lodb_and-4)+ebp*4]
        NEXT


OP_CONST_PRI:
        mov     eax,[esi+4]
        add     esi,8
        NEXT


OP_CONST_ALT:
        mov     edx,[esi+4]
        add     esi,8
        NEXT


OP_ADDR_PRI:
        mov     eax,[esi+4]
        add     esi,8
        add     eax,frm
        NEXT


OP_ADDR_ALT:
        mov     edx,[esi+4]
        add     esi,8
        add     edx,frm
        NEXT


OP_STOR:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+edi],eax
        NEXT


OP_STOR_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     [ebp+ebx],eax
        NEXT


OP_SREF_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebx+ebp]
        mov     [edi+ebp],eax
        NEXT


OP_STOR_I:
        add     esi,4
        _VERIFYADDRESS  edx
        mov     [edi+edx],eax
        NEXT


OP_STRB_I:
        mov     ebp,[esi+4]
        add     esi,8
    strb_entry:
        _VERIFYADDRESS  edx
        cmp     ebp,1
        jne     short strb_not1byte
        mov     [edi+edx],al
        NEXT
    strb_not1byte:
        cmp     ebp,4
        je      short strb_4byte
        mov     [edi+edx],ax
        NEXT
    strb_4byte:
        mov     [edi+edx],eax
        NEXT


OP_PUSHR_PRI:
        mov     ebp,eax
        add     esi,4
        add     ebp,edi
        _PUSH   ebp
        NEXT


OP_POP_PRI:
        add     esi,4
        _POP    eax
        NEXT


OP_POP_ALT:
        add     esi,4
        _POP    edx
        NEXT


OP_PICK:
        mov     eax,[esi+4]
        add     esi,8
        add     eax,ecx
        mov     eax,[edi+eax]
        NEXT


OP_STACK:
        mov     edx,ecx
        add     ecx,[esi+4]
        _CHKMARGIN
        _CHKSTACK
        add     esi,8
        NEXT


OP_HEAP:
        mov     ebp,[esi+4]
        mov     edx,hea
        add     esi,8
        add     hea,ebp
        _CHKMARGIN
        _CHKHEAP
        NEXT


OP_PROC:
        mov     ebx,frm
        add     esi,4
        _PUSH   ebx
        mov     ebx,edi
        mov     frm,ecx
        add     ebx,ecx
        _CHKMARGIN
        NEXT


OP_RET:
        _POP    ebx
        _POP    esi
        cmp     esi,codesiz     ; verify ESI<codesiz
        jae     err_memaccess
        mov     frm,ebx
        add     esi,code        ; relocate ESI and EBI
        add     ebx,edi
        NEXT


OP_RETN:
        _POP    ebx
        _POP    esi
        cmp     esi,codesiz     ; verify ESI<codesiz
        jae     err_memaccess
        mov     frm,ebx
        add     esi,code        ; relocate ESI and EBI
        add     ebx,edi
        mov     ebp,[edi+ecx]
        lea     ecx,[ecx+ebp+4]
        NEXT


OP_CALL:
        lea     ebp,[esi+8]
        JUMPREL ; add esi,[esi+4]
        sub     ebp,code        ; reverse relocate (before push)
        _PUSH   ebp
        NEXT


OP_JUMP:
        JUMPREL ; add esi,[esi+4]
        NEXT


OP_JZER:
        or      eax,eax
        jz      short OP_JUMP
        add     esi,8
        NEXT


OP_JNZ:
        or      eax,eax
        jnz     short OP_JUMP
        add     esi,8
        NEXT


OP_SHL:
        push    ecx
        mov     ecx,edx
        add     esi,4
        shl     eax,cl
        pop     ecx
        NEXT


OP_SHR:
        push    ecx
        mov     ecx,edx
        add     esi,4
        shr     eax,cl
        pop     ecx
        NEXT


OP_SSHR:
        push    ecx
        mov     ecx,edx
        add     esi,4
        sar     eax,cl
        pop     ecx
        NEXT


OP_SHL_C_PRI:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     eax,cl
        pop     ecx
        NEXT


OP_SHL_C_ALT:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     edx,cl
        pop     ecx
        NEXT


OP_SMUL:
        add     esi,4
        push    edx
        imul    edx
        pop     edx
        NEXT


OP_SDIV:
        xchg    eax,edx
        ALIGN   4

        ; Although SDIV.INV is a supplemental instruction, it is implemented
        ; as a core instruction here, because SDIV is implemented in terms
        ; of SDIV.INV
OP_SDIV_INV:
        mov     ebp,edx
        xor     edx,eax         ; Check signs of the operands.
        cdq
        js      short sdiv_fiddle ; If the signs of the operands are different
                                ; we'll have to fiddle around to achieve
                                ; proper rounding towards minus infinity.
        _CHKDIVIDEZERO
        add     esi,4           ; default behavior is right in the other cases
        idiv    ebp
        NEXT

    sdiv_fiddle:
        _CHKDIVIDEZERO
        idiv    ebp
        add     esi,4
        or      edx,edx
        jz      short sdiv_goon ; If there's no remainder the result is correct
        add     edx,ebp         ; else fix the result values.
        dec     eax             ; Amazing, how simple this is...
    sdiv_goon:
        NEXT


OP_ADD:
        add     esi,4
        add     eax,edx
        NEXT


OP_SUB:
        neg     eax
        add     esi,4
        add     eax,edx
        NEXT


OP_AND:
        add     esi,4
        and     eax,edx
        NEXT


OP_OR:
        add     esi,4
        or      eax,edx
        NEXT


OP_XOR:
        add     esi,4
        xor     eax,edx
        NEXT


OP_NOT:
        add     esi,4
        neg     eax             ; sets CF iff EAX != 0
        sbb     eax,eax         ; EAX == -1 iff CF set (zero otherwise)
        inc     eax             ; -1 => 0 and 0 => 1
        NEXT


OP_NEG:
        add     esi,4
        neg     eax
        NEXT


OP_INVERT:
        add     esi,4
        not     eax
        NEXT


OP_EQ:
        add     esi,4
        cmp     eax,edx         ; PRI == ALT ?
        mov     eax,0
        sete    al
        NEXT


OP_NEQ:
        add     esi,4
        cmp     eax,edx         ; PRI != ALT ?
        mov     eax,0
        setne   al
        NEXT


OP_SLESS:
        add     esi,4
        cmp     eax,edx         ; PRI < ALT ? (signed)
        mov     eax,0
        setl    al
        NEXT


OP_SLEQ:
        add     esi,4
        cmp     eax,edx         ; PRI <= ALT ? (signed)
        mov     eax,0
        setle   al
        NEXT


OP_SGRTR:
        add     esi,4
        cmp     eax,edx         ; PRI > ALT ? (signed)
        mov     eax,0
        setg    al
        NEXT


OP_SGEQ:
        add     esi,4
        cmp     eax,edx         ; PRI >= ALT ? (signed)
        mov     eax,0
        setge   al
        NEXT


OP_INC_PRI:
        add     esi,4
        inc     eax
        NEXT


OP_INC_ALT:
        add     esi,4
        inc     edx
        NEXT


OP_INC_I:
        add     esi,4
        inc     DWORD [edi+eax]
        NEXT


OP_DEC_PRI:
        add     esi,4
        dec     eax
        NEXT


OP_DEC_ALT:
        add     esi,4
        dec     edx
        NEXT


OP_DEC_I:
        add     esi,4
        sub     DWORD [edi+eax],1
        NEXT


OP_MOVS:
        _VERIFYADDRESS  eax             ; PRI
        _VERIFYADDRESS  edx             ; ALT
        mov     ebp,eax
        add     ebp,[esi+4]
        dec     ebp
        _VERIFYADDRESS  ebp             ; PRI + size - 1
        sub     ebp,eax                 ; EBP = size - 1
        add     ebp,edx
        _VERIFYADDRESS  ebp             ; ALT + size - 1

        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
    movs_entry:
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
        NEXT


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
    cmps_entry:
        push    edi
        push    esi
        lea     esi,[edi+edx]
        lea     edi,[edi+eax]

        xor     eax,eax
        repe cmpsb
        je      short cmps1
        sbb     eax,eax
        sbb     eax,-1
    cmps1:
        pop     esi
        pop     edi
        pop     ecx
        NEXT


OP_FILL:
        mov     ebp,[esi+4]             ; get byte count
        add     esi,8
    fill_entry:
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
        NEXT


OP_HALT:
        cmp     DWORD retval,0
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
        NEXT


OP_SYSREQ:
        mov     eax,[esi+4]     ; get function number
        add     esi,8
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
        call    [ebp+_callback]
        _DROPARGS 10h           ; remove arguments from stack
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
        cmp     eax,AMX_ERR_NONE
        jne     _return         ; return error code, if any

        mov     eax,pri         ; get retval into eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        NEXT


OP_SWITCH:
        push    ecx
        mov     ebp,esi         ; EBP = CIP
        add     ebp,[esi+4]     ; EBP = offset of the casetable
        add     ebp,4           ; skip the "OP_CASETBL" opcode
        mov     ecx,[ebp]       ; ECX = number of records
        mov     esi,ebp         ; ESI = address of first record
        add     esi,[ebp+4]     ; preset ESI to "none-matched" case
    op_switch_loop:
        or      ecx, ecx        ; number of records == 0?
        jz      short op_switch_end ; yes, no more records, exit loop
        add     ebp,8           ; skip previous record
        dec     ecx             ; already decrement cases to do
        cmp     eax,[ebp]       ; PRI == case label?
        jne     short op_switch_loop ; no, continue loop
        mov     esi,ebp         ; yes, get jump address and exit loop
        add     esi,[ebp+4]
    op_switch_end:
        pop     ecx
        NEXT


OP_SWAP_PRI:
        mov     ebp,[edi+ecx]
        add     esi,4
        mov     [edi+ecx],eax
        mov     eax,ebp
        NEXT


OP_SWAP_ALT:
        mov     ebp,[edi+ecx]
        add     esi,4
        mov     [edi+ecx],edx
        mov     edx,ebp
        NEXT


OP_BREAK:
        mov     ebp,amx         ; get amx into ebp
        add     esi,4
        cmp     DWORD [ebp+_debug], 0
        jnz     break_calldebug
        NEXT                    ; debug hook not active, ignore

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
        _SAVEREGS
        push    eax
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
        NEXT


OP_CASETBL:
OP_CASETBL_OVL:
        mov     eax,AMX_ERR_INVINSTR
        jmp     _return


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
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    edx
        push    eax
        call    ebx             ; direct call
        _DROPARGS 8             ; remove arguments from stack
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
        cmp     DWORD [ebp+_error],AMX_ERR_NONE
        jne     _return         ; return error code, if any

        ; function result is in eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        NEXT


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
        ; save a few registers (it is not necessary to save them all
        ; and EAX should *not* be saved because it will hold the return
        ; value)
        push    ebp
        push    esi
        push    edi
        ; push the parameters
        push    edx
        push    eax
        call    ebx             ; direct call
        _DROPARGS 8             ; remove arguments from stack
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
        ; function result is in eax (PRI)
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        add     ecx,[esi-4]     ; remove "number of parameter bytes" from the stack
        add     ecx,4           ; also remove the extra DWORD pushed

        cmp     DWORD [ebp+_error],AMX_ERR_NONE
        jne     _return         ; return error code, if any
        NEXT


        ; overlay instructions
%ifndef AMX_NO_OVERLAY

OP_CALL_OVL:
        mov     alt,edx         ; save ALT
        lea     edx,[esi+8]     ; EDX=address of next instruction
        sub     edx,code        ; EDX=relative address (to start of code segment)
        mov     ebp,amx
        shl     edx,16
        or      edx,[ebp+_ovl_index] ; EDX=(relative address << 16) | ovl_index
        _PUSH   edx
        mov     edx,[esi+4]     ; EDX=ovl_index
        mov     [ebp+_ovl_index],edx
        mov     eax,ebp         ; 1st parm: amx
        _SAVEREGS
        push    edx             ; EDX (2nd parm)=overlay index
        push    eax
        call    [ebp+_overlay]  ; call overlay function
        _DROPARGS 8             ; remove arguments from stack
        _RESTOREREGS
        mov     edx,alt         ; restore ALT
        mov     esi,[ebp+_codeseg] ; get new code base
        mov     code,esi        ; save new code base in local variable
        NEXT


OP_RETN_OVL:
        mov     pri,eax         ; save PRI
        mov     alt,edx         ; save ALT
        _POP    ebx             ; restore FRM
        _POP    esi             ; restore code offset + overlay index
        mov     frm,ebx
        add     ebx,edi
        mov     edx,esi
        mov     ebp,amx
        and     edx,0ffffh      ; EDX=overlay index (popped from stack)
        mov     [ebp+_ovl_index],edx ; store overlay index returning to
        shr     esi,16          ; ESI=offset into overlay
        mov     eax,[edi+ecx]   ; EAX=dataseg[stk]
        lea     ecx,[ecx+eax+4] ; STK=STK+dataseg[stk]+4
        mov     eax,ebp         ; 1st parm: amx
        _SAVEREGS
        push    edx             ; EDX (2nd parm)=overlay index
        push    eax
        call    [ebp+_overlay]  ; call overlay function
        _DROPARGS 8             ; remove arguments from stack
        _RESTOREREGS
        mov     eax,[ebp+_codeseg] ; get new code base
        mov     code,eax        ; save new code base in local variable
        add     esi,eax         ; ESI=code base + offset
        mov     eax,pri         ; restore PRI
        mov     edx,alt         ; restore ALT
        NEXT


OP_SWITCH_OVL:
        push    ecx
        mov     ebp,esi         ; EBP = CIP
        add     ebp,[esi+4]     ; EBP = offset of the icasetable
        add     ebp,4           ; skip the "OP_ICASETBL" opcode
        mov     ecx,[ebp]       ; ECX = number of records
        mov     edx,[ebp+4]     ; preset EDX to "none-matched" case
    op_iswitch_loop:
        or      ecx, ecx        ; number of records == 0?
        jz      short op_iswitch_end ; yes, no more records, exit loop
        add     ebp,8           ; skip previous record
        dec     ecx             ; already decrement cases to do
        cmp     eax,[ebp]       ; PRI == icase label?
        jne     short op_iswitch_loop ; no, continue loop
        mov     edx,[ebp+4]     ; yes, get jump address and exit loop
    op_iswitch_end:
        pop     ecx
        ;load overlay
        mov     eax,amx
        mov     [eax+_ovl_index],edx
        _SAVEREGS
        push    edx             ; EDX (2nd parm)=overlay index
        push    eax             ; EAX (1st parm)=amx structure
        call    [eax+_overlay]  ; call overlay function
        _DROPARGS 8             ; remove arguments from stack
        _RESTOREREGS
        mov     esi,[eax+_codeseg] ; get new code base
        mov     code,esi        ; save new code base in local variable
        NEXT

%endif  ; AMX_NO_OVERLAY


        ; supplemental instructions
%ifndef AMX_NO_MACRO_INSTR

OP_LIDX:
        lea     eax,[edx+4*eax]
        add     esi,4
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        NEXT


OP_LIDX_B:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     eax,cl
        pop     ecx
        add     eax,edx
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        NEXT


OP_IDXADDR:
        add     esi,4
        lea     eax,[edx+4*eax]
        NEXT


OP_IDXADDR_B:
        push    ecx
        mov     ecx,[esi+4]
        add     esi,8
        shl     eax,cl
        pop     ecx
        add     eax,edx
        NEXT


OP_PUSH_C:
        mov     ebp,[esi+4]
        add     esi,8
        _PUSH   ebp
        NEXT


OP_PUSH:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        NEXT


OP_PUSH_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        NEXT


OP_PUSH_ADR:
        mov     ebp,[esi+4]
        add     esi,8
        add     ebp,frm
        _PUSH   ebp
        NEXT


OP_PUSHR_C:
        mov     ebp,[esi+4]
        add     esi,8
        add     ebp,edi
        _PUSH   ebp
        NEXT


OP_PUSHR_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     ebp,[ebp+ebx]
        add     ebp,edi
        _PUSH   ebp
        NEXT


OP_PUSHR_ADR:
        mov     ebp,[esi+4]
        add     esi,8
        add     ebp,ebx         ; EBX = dat+frm
        _PUSH   ebp
        NEXT


OP_JEQ:
        cmp     eax,edx
        je      OP_JUMP
        add     esi,8
        NEXT


OP_JNEQ:
        cmp     eax,edx
        jne     OP_JUMP
        add     esi,8
        NEXT


OP_JSLESS:
        cmp     eax,edx
        jl      OP_JUMP
        add     esi,8
        NEXT


OP_JSLEQ:
        cmp     eax,edx
        jle     OP_JUMP
        add     esi,8
        NEXT


OP_JSGRTR:
        cmp     eax,edx
        jg      OP_JUMP
        add     esi,8
        NEXT


OP_JSGEQ:
        cmp     eax,edx
        jge     OP_JUMP
        add     esi,8
        NEXT


OP_SUB_INV:
        add     esi,4
        sub     eax,edx
        NEXT


OP_ADD_C:
        add     eax,[esi+4]
        add     esi,8
        NEXT


OP_SMUL_C:
        mov     ebp,[esi+4]
        push    edx
        imul    ebp
        pop     edx
        add     esi,8
        NEXT


OP_ZERO_PRI:
        add     esi,4
        sub     eax,eax
        NEXT


OP_ZERO_ALT:
        add     esi,4
        sub     edx,edx
        NEXT


OP_ZERO:
        mov     ebp,[esi+4]
        add     esi,8
        mov     DWORD [edi+ebp],0
        NEXT


OP_ZERO_S:
        mov     ebp,[esi+4]
        add     esi,8
        mov     DWORD [ebx+ebp],0
        NEXT


OP_EQ_C_PRI:
        cmp     eax,[esi+4]     ; PRI == value ?
        lea     esi,[esi+8]
        mov     eax,0
        sete    al
        NEXT


OP_EQ_C_ALT:
        xor     eax,eax
        cmp     edx,[esi+4]     ; ALT == value ?
        lea     esi,[esi+8]
        sete    al
        NEXT


OP_INC:
        mov     ebp,[esi+4]
        add     esi,8
        inc     DWORD [edi+ebp]
        NEXT


OP_INC_S:
        mov     ebp,[esi+4]
        add     esi,8
        inc     DWORD [ebx+ebp]
        NEXT


OP_DEC:
        mov     ebp,[esi+4]
        add     esi,8
        dec     DWORD [edi+ebp]
        NEXT


OP_DEC_S:
        mov     ebp,[esi+4]
        add     esi,8
        dec     DWORD [ebx+ebp]
        NEXT


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
        call    [ebp+_callback]
        _DROPARGS 10h           ; remove arguments from stack
        pop     edi             ; restore saved registers
        pop     esi
        pop     ebp
        mov     edx,alt         ; restore ALT
        mov     ebx,frm
        mov     ecx,stk         ; restore STK
        add     ebx,edi         ; restore FRM
        add     ecx,[esi-4]     ; remove "number of parameter bytes" from the stack
        add     ecx,4           ; also remove the extra DWORD pushed

        cmp     eax,AMX_ERR_NONE
        jne     _return         ; return error code, if any
        mov     eax,pri         ; get retval into eax (PRI)
        NEXT


OP_PUSHM_C:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushm_c_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_c_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushm_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM_S:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushm_s_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_s_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM_ADR:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushm_adr_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,frm
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_adr_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_C:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushrm_c_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,edi
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_c_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_S:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushrm_s_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+ebx]
        add     ebp,edi
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_s_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_ADR:
        mov     pri,eax         ; save PRI
        add     esi,8           ; skip opcode and parameter count
        mov     eax,[esi-4]     ; get parameter count
    op_pushrm_adr_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,ebx         ; EBX = dat+frm
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_adr_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_LOAD2:
        mov     eax,[esi+4]
        mov     edx,[esi+8]
        add     esi,12
        mov     eax,[edi+eax]
        mov     edx,[edi+edx]
        NEXT


OP_LOAD2_S:
        mov     eax,[esi+4]
        mov     edx,[esi+8]
        add     esi,12
        mov     eax,[ebx+eax]
        mov     edx,[ebx+edx]
        NEXT


OP_CONST:
        push    eax
        mov     ebp,[esi+4]
        mov     eax,[esi+8]
        add     esi,12
        mov     [ebp+edi],eax
        pop     eax
        NEXT


OP_CONST_S:
        push    eax
        mov     ebp,[esi+4]
        mov     eax,[esi+8]
        add     esi,12
        mov     [ebp+ebx],eax
        pop     eax
        NEXT

%endif  ; AMX_NO_MACRO_INSTR


        ; packed opcodes
%ifndef AMX_NO_PACKED_OPC

OP_LOAD_P_PRI:
        GETPARAM_P eax
        add     esi,4
        mov     eax,[edi+eax]
        NEXT

OP_LOAD_P_ALT:
        GETPARAM_P edx
        add     esi,4
        mov     edx,[edi+edx]
        NEXT


OP_LOAD_P_S_PRI:
        GETPARAM_P eax
        add     esi,4
        mov     eax,[ebx+eax]
        NEXT


OP_LOAD_P_S_ALT:
        GETPARAM_P edx
        add     esi,4
        mov     edx,[ebx+edx]
        NEXT


OP_LREF_P_S_PRI:
        GETPARAM_P eax
        add     esi,4
        mov     eax,[ebx+eax]
        mov     eax,[edi+eax]
        NEXT


OP_LREF_P_S_ALT:
        GETPARAM_P edx
        add     esi,4
        mov     edx,[ebx+edx]
        mov     edx,[edi+edx]
        NEXT


OP_LODB_P_I:
        _VERIFYADDRESS  eax
        GETPARAM_P ebp
        mov     eax,[edi+eax]           ;subject to misalignment stalls
        add     esi,4
        and     eax,DWORD [(lodb_and-4)+ebp*4]
        NEXT


OP_CONST_P_PRI:
        GETPARAM_P eax
        add     esi,4
        NEXT


OP_CONST_P_ALT:
        GETPARAM_P edx
        add     esi,4
        NEXT


OP_ADDR_P_PRI:
        GETPARAM_P eax
        add     esi,4
        add     eax,frm
        NEXT


OP_ADDR_P_ALT:
        GETPARAM_P edx
        add     esi,4
        add     edx,frm
        NEXT


OP_STOR_P:
        GETPARAM_P ebp
        add     esi,4
        mov     [ebp+edi],eax
        NEXT


OP_STOR_P_S:
        GETPARAM_P ebp
        add     esi,4
        mov     [ebp+ebx],eax
        NEXT


OP_SREF_P_S:
        GETPARAM_P ebp
        add     esi,4
        mov     ebp,[ebx+ebp]
        mov     [edi+ebp],eax
        NEXT


OP_STRB_P_I:
        GETPARAM_P ebp
        add     esi,4
        jmp     strb_entry


OP_LIDX_P_B:
        push    ecx
        GETPARAM_P ecx
        add     esi,4
        shl     eax,cl
        pop     ecx
        add     eax,edx
        _VERIFYADDRESS  eax
        mov     eax,[edi+eax]
        NEXT


OP_IDXADDR_P_B:
        push    ecx
        GETPARAM_P ecx
        add     esi,4
        shl     eax,cl
        pop     ecx
        add     eax,edx
        NEXT


OP_ALIGN_P_PRI:
        GETPARAM_P ebp
        add     esi,4
        neg     ebp             ; ebp = -param
        add     ebp,4           ; ebp = 4 - param
        xor     eax,ebp
        NEXT


OP_PUSH_P_C:
        GETPARAM_P ebp
        add     esi,4
        _PUSH   ebp
        NEXT


OP_PUSH_P:
        GETPARAM_P ebp
        add     esi,4
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        NEXT


OP_PUSH_P_S:
        GETPARAM_P ebp
        add     esi,4
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        NEXT


OP_PUSH_P_ADR:
        GETPARAM_P ebp
        add     esi,4
        add     ebp,frm
        _PUSH   ebp
        NEXT


OP_PUSHR_P_C:
        GETPARAM_P ebp
        add     esi,4
        add     ebp,edi
        _PUSH   ebp
        NEXT


OP_PUSHR_P_S:
        GETPARAM_P ebp
        add     esi,4
        mov     ebp,[ebp+ebx]
        add     ebp,edi
        _PUSH   ebp
        NEXT


OP_PUSHR_P_ADR:
        GETPARAM_P ebp
        add     esi,4
        add     ebp,ebx         ; EBX = dat+frm
        _PUSH   ebp
        NEXT


OP_PUSHM_P_C:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushm_p_c_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_p_c_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM_P:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushm_p_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+edi]
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_p_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM_P_S:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushm_p_s_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+ebx]
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_p_s_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHM_P_ADR:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushm_p_adr_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,frm
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushm_p_adr_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_P_C:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushrm_p_c_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,edi
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_p_c_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_P_S:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushrm_p_s_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        mov     ebp,[ebp+ebx]
        add     ebp,edi
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_p_s_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_PUSHRM_P_ADR:
        mov     pri,eax         ; save PRI
        GETPARAM_P eax          ; get parameter count
        add     esi,4
    op_pushrm_p_adr_loop:
        mov     ebp,[esi]       ; get and push parameter
        add     esi,4
        add     ebp,ebx         ; EBX = dat+frm
        _PUSH   ebp
        dec     eax             ; decrement cound and loop
        jnz     op_pushrm_p_adr_loop
        mov     eax,pri         ; restore PRI
        NEXT


OP_STACK_P:
        GETPARAM_P ebp
        add     esi,4
        mov     edx,ecx
        add     ecx,ebp
        _CHKMARGIN
        _CHKSTACK
        NEXT


OP_HEAP_P:
        GETPARAM_P ebp
        add     esi,4
        mov     edx,hea
        add     hea,ebp
        _CHKMARGIN
        _CHKHEAP
        NEXT


OP_SHL_P_C_PRI:
        push    ecx
        GETPARAM_P ecx
        add     esi,4
        shl     eax,cl
        pop     ecx
        NEXT


OP_SHL_P_C_ALT:
        push    ecx
        GETPARAM_P ecx
        add     esi,4
        shl     edx,cl
        pop     ecx
        NEXT


OP_ADD_P_C:
        GETPARAM_P ebp
        add     esi,4
        add     eax,ebp
        NEXT


OP_SMUL_P_C:
        GETPARAM_P ebp
        add     esi,4
        push    edx
        imul    ebp
        pop     edx
        NEXT


OP_ZERO_P:
        GETPARAM_P ebp
        add     esi,4
        mov     DWORD [edi+ebp],0
        NEXT


OP_ZERO_P_S:
        GETPARAM_P ebp
        add     esi,4
        mov     DWORD [ebx+ebp],0
        NEXT


OP_EQ_P_C_PRI:
        GETPARAM_P ebp
        add     esi,4
        cmp     eax,ebp         ; PRI == value ?
        mov     eax,0
        sete    al
        NEXT


OP_EQ_P_C_ALT:
        GETPARAM_P ebp
        add     esi,4
        xor     eax,eax
        cmp     edx,[esi+4]     ; ALT == value ?
        sete    al
        NEXT


OP_INC_P:
        GETPARAM_P ebp
        add     esi,4
        inc     DWORD [edi+ebp]
        NEXT


OP_INC_P_S:
        GETPARAM_P ebp
        add     esi,4
        inc     DWORD [ebx+ebp]
        NEXT


OP_DEC_P:
        GETPARAM_P ebp
        add     esi,4
        dec     DWORD [edi+ebp]
        NEXT


OP_DEC_P_S:
        GETPARAM_P ebp
        add     esi,4
        dec     DWORD [ebx+ebp]
        NEXT


OP_MOVS_P:
        _VERIFYADDRESS  eax             ; PRI
        _VERIFYADDRESS  edx             ; ALT
        GETPARAM_P ebp
        add     ebp,eax
        dec     ebp
        _VERIFYADDRESS  ebp             ; PRI + size - 1
        sub     ebp,eax                 ; EBP = size - 1
        add     ebp,edx
        _VERIFYADDRESS  ebp             ; ALT + size - 1

        push    ecx                     ; matching POP is in movs_entry
        GETPARAM_P ecx
        add     esi,4
        jmp     movs_entry


OP_CMPS_P:
        _VERIFYADDRESS  eax             ; PRI
        _VERIFYADDRESS  edx             ; ALT
        GETPARAM_P ebp
        add     ebp,eax
        dec     ebp                     ; EBP = PRI + size - 1
        _VERIFYADDRESS  ebp             ; PRI + size - 1
        sub     ebp,eax                 ; EBP = size - 1
        add     ebp,edx                 ; EBP = ALT + size - 1
        _VERIFYADDRESS  ebp             ; ALT + size - 1

        push    ecx                     ; matching pop is in cmps_entry
        GETPARAM_P ecx
        add     esi,4
        jmp     cmps_entry


OP_FILL_P:
        GETPARAM_P ebp                  ; get byte count
        add     esi,4
        jmp     fill_entry


OP_HALT_P:
        cmp     DWORD retval,0
        je      short halt_no_retval_p
        mov     ebp,retval
        mov     [ebp],eax
    halt_no_retval_p:
        ; store the complete status in the AMX
        mov     ebp,amx         ; get amx into ebp
        mov     [ebp+_pri],eax  ; store values in AMX structure (PRI, ALT, STK, HEA, FRM, ...)
        mov     [ebp+_alt],edx
        mov     [ebp+_stk],ecx
        mov     ecx,hea
        mov     ebx,frm
        mov     [ebp+_hea],ecx
        mov     [ebp+_frm],ebx  ; EBX & ECX are invalid by now
        GETPARAM_P ebx          ; EBX=parameter of the HALT opcode
        add     esi,4           ; skip this instruction
        mov     eax,esi         ; EAX=CIP
        sub     eax,code
        mov     [ebp+_cip],eax
        mov     eax,ebx         ; return the parameter of the HALT opcode
        jmp     _return


OP_BOUNDS_P:
        GETPARAM_P ebp
        add     esi,4
        cmp     eax,ebp
        ja      err_bounds      ; use unsigned comparison, so <0 is >bounds
        NEXT

%endif  ; AMX_NO_PACKED_OPC


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
        pop     ebx
        ret



;----------------------------------------------------------------------------
;int amx_exec_list(const AMX *amx,const cell **opcodelist,int *numopcodes)
;----------------------------------------------------------------------------

        GLOBAL  amx_exec_list
        GLOBAL  _amx_exec_list
amx_exec_list:
_amx_exec_list: ;PROC
        push    ebx
        ;mov     eax,[esp+08h]
        mov     edx,[esp+0ch]
        mov     ebx,[esp+10h]

        mov     eax,opcodelist
        mov     [edx],eax
        neg     eax
        add     eax,opcodelist_end   ; EAX = terminator - start
        shr     eax,2
        mov     [ebx],eax       ; store number of opcodes
        xor     eax, eax        ; return value = 0
        pop     ebx
        ret


;============================================================================

Start_DATA
        ALIGN   4       ; This is essential to avoid misalignment stalls.

lodb_and DD     0ffh, 0ffffh, 0, 0ffffffffh

opcodelist:
        ; core set
        DD      OP_NOP
        DD      OP_LOAD_PRI
        DD      OP_LOAD_ALT
        DD      OP_LOAD_S_PRI
        DD      OP_LOAD_S_ALT
        DD      OP_LREF_S_PRI
        DD      OP_LREF_S_ALT
        DD      OP_LOAD_I
        DD      OP_LODB_I
        DD      OP_CONST_PRI
        DD      OP_CONST_ALT
        DD      OP_ADDR_PRI
        DD      OP_ADDR_ALT
        DD      OP_STOR
        DD      OP_STOR_S
        DD      OP_SREF_S
        DD      OP_STOR_I
        DD      OP_STRB_I
        DD      OP_ALIGN_PRI
        DD      OP_LCTRL
        DD      OP_SCTRL
        DD      OP_XCHG
        DD      OP_PUSH_PRI
        DD      OP_PUSH_ALT
        DD      OP_PUSHR_PRI
        DD      OP_POP_PRI
        DD      OP_POP_ALT
        DD      OP_PICK
        DD      OP_STACK
        DD      OP_HEAP
        DD      OP_PROC
        DD      OP_RET
        DD      OP_RETN
        DD      OP_CALL
        DD      OP_JUMP
        DD      OP_JZER
        DD      OP_JNZ
        DD      OP_SHL
        DD      OP_SHR
        DD      OP_SSHR
        DD      OP_SHL_C_PRI
        DD      OP_SHL_C_ALT
        DD      OP_SMUL
        DD      OP_SDIV
        DD      OP_ADD
        DD      OP_SUB
        DD      OP_AND
        DD      OP_OR
        DD      OP_XOR
        DD      OP_NOT
        DD      OP_NEG
        DD      OP_INVERT
        DD      OP_EQ
        DD      OP_NEQ
        DD      OP_SLESS
        DD      OP_SLEQ
        DD      OP_SGRTR
        DD      OP_SGEQ
        DD      OP_INC_PRI
        DD      OP_INC_ALT
        DD      OP_INC_I
        DD      OP_DEC_PRI
        DD      OP_DEC_ALT
        DD      OP_DEC_I
        DD      OP_MOVS
        DD      OP_CMPS
        DD      OP_FILL
        DD      OP_HALT
        DD      OP_BOUNDS
        DD      OP_SYSREQ
        DD      OP_SWITCH
        DD      OP_SWAP_PRI
        DD      OP_SWAP_ALT
        DD      OP_BREAK
        DD      OP_CASETBL
        ; patched instructions
        DD      OP_SYSREQ_D
        DD      OP_SYSREQ_ND
        ; overlay instructions
%ifndef AMX_NO_OVERLAY
        DD      OP_CALL_OVL
        DD      OP_RETN_OVL
        DD      OP_SWITCH_OVL
        DD      OP_CASETBL_OVL
%endif  ; AMX_NO_OVERLAY
        ; supplemental instructions
%ifndef AMX_NO_MACRO_INSTR
        DD      OP_LIDX
        DD      OP_LIDX_B
        DD      OP_IDXADDR
        DD      OP_IDXADDR_B
        DD      OP_PUSH_C
        DD      OP_PUSH
        DD      OP_PUSH_S
        DD      OP_PUSH_ADR
        DD      OP_PUSHR_C
        DD      OP_PUSHR_S
        DD      OP_PUSHR_ADR
        DD      OP_JEQ
        DD      OP_JNEQ
        DD      OP_JSLESS
        DD      OP_JSLEQ
        DD      OP_JSGRTR
        DD      OP_JSGEQ
        DD      OP_SDIV_INV
        DD      OP_SUB_INV
        DD      OP_ADD_C
        DD      OP_SMUL_C
        DD      OP_ZERO_PRI
        DD      OP_ZERO_ALT
        DD      OP_ZERO
        DD      OP_ZERO_S
        DD      OP_EQ_C_PRI
        DD      OP_EQ_C_ALT
        DD      OP_INC
        DD      OP_INC_S
        DD      OP_DEC
        DD      OP_DEC_S
        DD      OP_SYSREQ_N
        DD      OP_PUSHM_C
        DD      OP_PUSHM
        DD      OP_PUSHM_S
        DD      OP_PUSHM_ADR
        DD      OP_PUSHRM_C
        DD      OP_PUSHRM_S
        DD      OP_PUSHRM_ADR
        DD      OP_LOAD2
        DD      OP_LOAD2_S
        DD      OP_CONST
        DD      OP_CONST_S
%endif  ; AMX_NO_MACRO_INSTR
        ; packed opcodes
%ifndef AMX_NO_PACKED_OPC
        DD      OP_LOAD_P_PRI
        DD      OP_LOAD_P_ALT
        DD      OP_LOAD_P_S_PRI
        DD      OP_LOAD_P_S_ALT
        DD      OP_LREF_P_S_PRI
        DD      OP_LREF_P_S_ALT
        DD      OP_LODB_P_I
        DD      OP_CONST_P_PRI
        DD      OP_CONST_P_ALT
        DD      OP_ADDR_P_PRI
        DD      OP_ADDR_P_ALT
        DD      OP_STOR_P
        DD      OP_STOR_P_S
        DD      OP_SREF_P_S
        DD      OP_STRB_P_I
        DD      OP_LIDX_P_B
        DD      OP_IDXADDR_P_B
        DD      OP_ALIGN_P_PRI
        DD      OP_PUSH_P_C
        DD      OP_PUSH_P
        DD      OP_PUSH_P_S
        DD      OP_PUSH_P_ADR
        DD      OP_PUSHR_P_C
        DD      OP_PUSHR_P_S
        DD      OP_PUSHR_P_ADR
        DD      OP_PUSHM_P_C
        DD      OP_PUSHM_P
        DD      OP_PUSHM_P_S
        DD      OP_PUSHM_P_ADR
        DD      OP_PUSHRM_P_C
        DD      OP_PUSHRM_P_S
        DD      OP_PUSHRM_P_ADR
        DD      OP_STACK_P
        DD      OP_HEAP_P
        DD      OP_SHL_P_C_PRI
        DD      OP_SHL_P_C_ALT
        DD      OP_ADD_P_C
        DD      OP_SMUL_P_C
        DD      OP_ZERO_P
        DD      OP_ZERO_P_S
        DD      OP_EQ_P_C_PRI
        DD      OP_EQ_P_C_ALT
        DD      OP_INC_P
        DD      OP_INC_P_S
        DD      OP_DEC_P
        DD      OP_DEC_P_S
        DD      OP_MOVS_P
        DD      OP_CMPS_P
        DD      OP_FILL_P
        DD      OP_HALT_P
        DD      OP_BOUNDS_P
%endif  ; AMX_NO_PACKED_OPC
opcodelist_end:
