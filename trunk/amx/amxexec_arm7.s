;   amxexec_arm7.s      Abstract Machine for the "Pawn" language
;
;   This file uses the ARM assembler syntax. It uses ARM Architecture v4T
;   instructions. It can be assembled for Big Endian environments, by
;   defining the symbol BIG_ENDIAN; the default configuration is
;   Little Endian.
;
;   You will need to compile the standard AMX.C file with the macro
;   ASM32 defined.
;
;   The calling convention conforms to the ARM Architecture Procedure
;   Call Standard (AAPCS). This applies both to the function amx_exec_asm
;   implemented in this file as to the debug hook function, callback hook
;   function and any native functions called directly from the abstract
;   machine.
;
;
;   Copyright (c) ITB CompuPhase, 2006
;
;   This software is provided "as-is", without any express or implied warranty.
;   In no event will the authors be held liable for any damages arising from
;   the use of this software.
;
;   Permission is granted to anyone to use this software for any purpose,
;   including commercial applications, and to alter it and redistribute it
;   freely, subject to the following restrictions:
;
;   1.  The origin of this software must not be misrepresented; you must not
;       claim that you wrote the original software. If you use this software in
;       a product, an acknowledgment in the product documentation would be
;       appreciated but is not required.
;   2.  Altered source versions must be plainly marked as such, and must not be
;       misrepresented as being the original software.
;   3.  This notice may not be removed or altered from any source distribution.
;
;   Version: $Id: amxexec_arm7.s 3701 2007-01-21 17:01:21Z thiadmer $


AMX_ERR_NONE      EQU      0
AMX_ERR_EXIT      EQU      1   ; forced exit
AMX_ERR_ASSERT    EQU      2   ; assertion failed
AMX_ERR_STACKERR  EQU      3   ; stack/heap collision
AMX_ERR_BOUNDS    EQU      4   ; index out of bounds
AMX_ERR_MEMACCESS EQU      5   ; invalid memory access
AMX_ERR_INVINSTR  EQU      6   ; invalid instruction
AMX_ERR_STACKLOW  EQU      7   ; stack underflow
AMX_ERR_HEAPLOW   EQU      8   ; heap underflow
AMX_ERR_CALLBACK  EQU      9   ; no callback, or invalid callback
AMX_ERR_NATIVE    EQU     10   ; native function failed
AMX_ERR_DIVIDE    EQU     11   ; divide by zero
AMX_ERR_SLEEP     EQU     12   ; go into sleepmode - code can be restarted
AMX_ERR_INVSTATE  EQU     13   ; invalid state for this access

amxBase           EQU    0
amxData           EQU    4     ; points to separate data+stack+heap, may be NULL
amxCallback       EQU    8
amxDebug          EQU    12    ; debug callback
amxCIP            EQU    16    ; instruction pointer: relative to base + amxhdr->cod
amxFRM            EQU    20    ; stack frame base: relative to base + amxhdr->dat
amxHEA            EQU    24    ; top of the heap: relative to base + amxhdr->dat
amxHLW            EQU    28    ; bottom of the heap: relative to base + amxhdr->dat
amxSTK            EQU    32    ; stack pointer: relative to base + amxhdr->dat
amxSTP            EQU    36    ; top of the stack: relative to base + amxhdr->dat
amxFlags          EQU    40    ; current status, see amx_Flags()
amxUserTags       EQU    44    ; user data, AMX_USERNUM fields
amxUserData       EQU    60    ; user data
amxError          EQU    76    ; native functions that raise an error
amxParamCount     EQU    80    ; passing parameters requires a "count" field
amxPRI            EQU    84    ; the sleep opcode needs to store the full AMX status
amxALT            EQU    88
amx_reset_stk     EQU    92
amx_reset_hea     EQU    96
amx_sysreq_d      EQU    100   ; relocated address/value for the SYSREQ.D opcode
amx_reloc_size    EQU    104   ; (JIT) required temporary buffer for relocations
amx_code_size     EQU    108   ; estimated memory footprint of the native code


    AREA    amxexec_data, DATA, READONLY
    ALIGN   2
    EXPORT  amx_opcodelist
amx_opcodelist
    DCD     OP_INVALID
    DCD     OP_LOAD_PRI
    DCD     OP_LOAD_ALT
    DCD     OP_LOAD_S_PRI
    DCD     OP_LOAD_S_ALT
    DCD     OP_LREF_PRI
    DCD     OP_LREF_ALT
    DCD     OP_LREF_S_PRI
    DCD     OP_LREF_S_ALT
    DCD     OP_LOAD_I
    DCD     OP_LODB_I
    DCD     OP_CONST_PRI
    DCD     OP_CONST_ALT
    DCD     OP_ADDR_PRI
    DCD     OP_ADDR_ALT
    DCD     OP_STOR_PRI
    DCD     OP_STOR_ALT
    DCD     OP_STOR_S_PRI
    DCD     OP_STOR_S_ALT
    DCD     OP_SREF_PRI
    DCD     OP_SREF_ALT
    DCD     OP_SREF_S_PRI
    DCD     OP_SREF_S_ALT
    DCD     OP_STOR_I
    DCD     OP_STRB_I
    DCD     OP_LIDX
    DCD     OP_LIDX_B
    DCD     OP_IDXADDR
    DCD     OP_IDXADDR_B
    DCD     OP_ALIGN_PRI
    DCD     OP_ALIGN_ALT
    DCD     OP_LCTRL
    DCD     OP_SCTRL
    DCD     OP_MOVE_PRI
    DCD     OP_MOVE_ALT
    DCD     OP_XCHG
    DCD     OP_PUSH_PRI
    DCD     OP_PUSH_ALT
    DCD     OP_PUSH_R_PRI       ; obsolete
    DCD     OP_PUSH_C
    DCD     OP_PUSH
    DCD     OP_PUSH_S
    DCD     OP_POP_PRI
    DCD     OP_POP_ALT
    DCD     OP_STACK
    DCD     OP_HEAP
    DCD     OP_PROC
    DCD     OP_RET
    DCD     OP_RETN
    DCD     OP_CALL
    DCD     OP_CALL_PRI
    DCD     OP_JUMP
    DCD     OP_JREL
    DCD     OP_JZER
    DCD     OP_JNZ
    DCD     OP_JEQ
    DCD     OP_JNEQ
    DCD     OP_JLESS
    DCD     OP_JLEQ
    DCD     OP_JGRTR
    DCD     OP_JGEQ
    DCD     OP_JSLESS
    DCD     OP_JSLEQ
    DCD     OP_JSGRTR
    DCD     OP_JSGEQ
    DCD     OP_SHL
    DCD     OP_SHR
    DCD     OP_SSHR
    DCD     OP_SHL_C_PRI
    DCD     OP_SHL_C_ALT
    DCD     OP_SHR_C_PRI
    DCD     OP_SHR_C_ALT
    DCD     OP_SMUL
    DCD     OP_SDIV
    DCD     OP_SDIV_ALT
    DCD     OP_UMUL
    DCD     OP_UDIV
    DCD     OP_UDIV_ALT
    DCD     OP_ADD
    DCD     OP_SUB
    DCD     OP_SUB_ALT
    DCD     OP_AND
    DCD     OP_OR
    DCD     OP_XOR
    DCD     OP_NOT
    DCD     OP_NEG
    DCD     OP_INVERT
    DCD     OP_ADD_C
    DCD     OP_SMUL_C
    DCD     OP_ZERO_PRI
    DCD     OP_ZERO_ALT
    DCD     OP_ZERO
    DCD     OP_ZERO_S
    DCD     OP_SIGN_PRI
    DCD     OP_SIGN_ALT
    DCD     OP_EQ
    DCD     OP_NEQ
    DCD     OP_LESS
    DCD     OP_LEQ
    DCD     OP_GRTR
    DCD     OP_GEQ
    DCD     OP_SLESS
    DCD     OP_SLEQ
    DCD     OP_SGRTR
    DCD     OP_SGEQ
    DCD     OP_EQ_C_PRI
    DCD     OP_EQ_C_ALT
    DCD     OP_INC_PRI
    DCD     OP_INC_ALT
    DCD     OP_INC
    DCD     OP_INC_S
    DCD     OP_INC_I
    DCD     OP_DEC_PRI
    DCD     OP_DEC_ALT
    DCD     OP_DEC
    DCD     OP_DEC_S
    DCD     OP_DEC_I
    DCD     OP_MOVS
    DCD     OP_CMPS
    DCD     OP_FILL
    DCD     OP_HALT
    DCD     OP_BOUNDS
    DCD     OP_SYSREQ_PRI
    DCD     OP_SYSREQ_C
    DCD     OP_FILE            ; obsolete
    DCD     OP_LINE            ; obsolete
    DCD     OP_SYMBOL          ; obsolete
    DCD     OP_SRANGE          ; obsolete
    DCD     OP_JUMP_PRI
    DCD     OP_SWITCH
    DCD     OP_CASETBL
    DCD     OP_SWAP_PRI
    DCD     OP_SWAP_ALT
    DCD     OP_PUSH_ADR
    DCD     OP_NOP
    DCD     OP_SYSREQ_N
    DCD     OP_SYMTAG          ; obsolete
    DCD     OP_BREAK
    DCD     OP_PUSH2_C
    DCD     OP_PUSH2
    DCD     OP_PUSH2_S
    DCD     OP_PUSH2_ADR
    DCD     OP_PUSH3_C
    DCD     OP_PUSH3
    DCD     OP_PUSH3_S
    DCD     OP_PUSH3_ADR
    DCD     OP_PUSH4_C
    DCD     OP_PUSH4
    DCD     OP_PUSH4_S
    DCD     OP_PUSH4_ADR
    DCD     OP_PUSH5_C
    DCD     OP_PUSH5
    DCD     OP_PUSH5_S
    DCD     OP_PUSH5_ADR
    DCD     OP_LOAD_BOTH
    DCD     OP_LOAD_S_BOTH
    DCD     OP_CONST
    DCD     OP_CONST_S
    DCD     OP_SYSREQ_D
    DCD     OP_SYSREQ_ND


    MACRO
    NEXT
    ldr pc, [r4], #4            ; indirect register jump
    MEND

    MACRO
    GETPARAM $rx
    ldr $rx, [r4], #4           ; $rx = [CIP], CIP += 4
    MEND

    MACRO
    VPUSH $rx
    str $rx, [r6, #-4]!         ; STK -= 4, [STK] = $rx
    MEND

    MACRO
    VPOP $rx
    ldr $rx, [r6], #4           ; $rx = [STK], STK += 4
    MEND

    MACRO
    CHKMARGIN $rtmp             ; $rtmp = temp register to use
    add $rtmp, r3, #64          ; 64 = 16 cells
    cmp $rtmp, r6               ; compare HEA + 16*cell to STK
    movhi r0, #AMX_ERR_STACKERR
    bhi amx_exit
    MEND

    MACRO
    CHKSTACK
    cmp r6, r2                  ; compare STK to STP
    movhi r0, #AMX_ERR_STACKLOW
    bhi amx_exit
    MEND

    MACRO
    CHKHEAP $rtmp               ; $rtmp = temp register to use
    ldr $rtmp, [r10, #amxHLW]
    cmp r3, $rtmp               ; compare HEA to HLW
    movlo r0, #AMX_ERR_HEAPLOW
    blo amx_exit
    MEND

    MACRO
    VERIFYADDRESS $rx
    cmp $rx, r2                 ; $rx >= STP ?
    bhs err_memaccess           ; yes -> error
    ; One might want to relax the test and remove the three instructions below.
    ; If a register points into the "free" area between the stack and the heap,
    ; it does not breach the sandbox.
    cmp r3, $rx                 ; HEA <= $rx ?
    cmpls $rx, r6               ; yes, then: $rx < STK ?
    blo err_memaccess           ; yes, then HEA <= $rx && $rx < STK (invalid area)
    MEND


; ----------------------------------------------------------------
; cell amx_exec_asm(cell parms[9],cell *retval,cell stp,cell hea)
;                   r0            r1           r2       r3
; ----------------------------------------------------------------

    AREA    amxexec_code, CODE, READONLY
    CODE32
    ALIGN   2
    EXPORT  amx_exec_asm
amx_exec_asm
    ; save non-scratch registers
    stmfd sp!, {r4 - r12, lr}

    ; save register that holds the address for the return value
    ; we only need this at the point of returning, so it would be
    ; a waste to keep it in a register
    str r1, [sp, #-4]!

    ; set up the registers
    ; r0  = PRI  = parms[0]
    ; r1  = ALT  = parms[1]
    ; r2  = STP (stack top), relocated (absolute address)
    ; r3  = HEA, relocated (absolute address)
    ; r4  = CIP  = (cell*)parms[2]
    ; r5  = data section = (unsigned char*)parms[3]
    ; r6  = STK = parms[4], relocated (absolute address)
    ; r7  = FRM  = parms[5], relocated (absolute address)
    ; r8  = code_start = (unsigned char*)parms[7]
    ; r9  = code_end = code_start + (ucell)parms[8]
    ; r10 = amx base = (AMX*)parms[6]
    ; r11, r12 and r14 are scratch

    ldr r1, [r0, #4]
    ldr r4, [r0, #8]
    ldr r5, [r0, #12]
    ldr r6, [r0, #16]
    ldr r7, [r0, #20]
    ldr r10, [r0, #24]
    ldr r8, [r0, #28]
    ldr r9, [r0, #32]
    ldr r0, [r0, #0]            ; this one last

    add r2, r2, r5              ; relocate STP (absolute address)
    add r3, r3, r5              ; relocate HEA
    add r6, r6, r5              ; relocate STK
    add r7, r7, r5              ; relocate FRM
    add r9, r9, r8              ; make r9 point behind last valid code address

    ; start running
    NEXT

OP_INVALID
    mov r0, #AMX_ERR_INVINSTR
    b   amx_exit

OP_LOAD_PRI                     ; tested
    GETPARAM r11
    ldr r0, [r5, r11]
    NEXT

OP_LOAD_ALT
    GETPARAM r11
    ldr r1, [r5, r11]
    NEXT

OP_LOAD_S_PRI                   ; tested
    GETPARAM r11
    ldr r0, [r7, r11]
    NEXT

OP_LOAD_S_ALT                   ; tested
    GETPARAM r11
    ldr r1, [r7, r11]
    NEXT

OP_LREF_PRI                     ; not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    ldr r0, [r5, r11]
    NEXT

OP_LREF_ALT                     ; not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    ldr r1, [r5, r11]
    NEXT

OP_LREF_S_PRI                   ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    ldr r0, [r5, r11]
    NEXT

OP_LREF_S_ALT
    GETPARAM r11
    ldr r11, [r7, r11]
    ldr r1, [r5, r11]
    NEXT

OP_LOAD_I                       ; tested
    add r12, r0, r5             ; relocate PRI to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_LODB_I                       ; tested
    add r12, r0, r5             ; relocate PRI to absolute address
    VERIFYADDRESS r12
    GETPARAM r11
    teq r11, #1
    ldreqb r0, [r12]
    teq r11, #2
    ldreqh r0, [r12]
    teq r11, #4
    ldreq r0, [r12]
    NEXT

OP_CONST_PRI                    ; tested
    GETPARAM r0
    NEXT

OP_CONST_ALT                    ; tested
    GETPARAM r1
    NEXT

OP_ADDR_PRI                     ; tested
    GETPARAM r0
    add r0, r0, r7              ; add FRM
    sub r0, r0, r5              ; reverse relocate
    NEXT

OP_ADDR_ALT                     ; tested
    GETPARAM r1
    add r1, r1, r7              ; add FRM
    sub r1, r1, r5              ; reverse relocate
    NEXT

OP_STOR_PRI                     ; tested
    GETPARAM r11
    str r0, [r5, r11]
    NEXT

OP_STOR_ALT
    GETPARAM r11
    str r1, [r5, r11]
    NEXT

OP_STOR_S_PRI                   ; tested
    GETPARAM r11
    str r0, [r7, r11]
    NEXT

OP_STOR_S_ALT                   ; not generated by pawncc
    GETPARAM r11
    str r1, [r7, r11]
    NEXT

OP_SREF_PRI                     ; not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    str r0, [r5, r11]
    NEXT

OP_SREF_ALT                     ; not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    str r1, [r5, r11]
    NEXT

OP_SREF_S_PRI                   ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    str r0, [r5, r11]
    NEXT

OP_SREF_S_ALT                   ; not generated by pawncc
    GETPARAM r11
    ldr r11, [r7, r11]
    str r1, [r5, r11]
    NEXT

OP_STOR_I                       ; tested
    add r12, r1, r5             ; relocate ALT to absolute address
    VERIFYADDRESS r12
    str r0, [r12]
    NEXT

OP_STRB_I                       ; tested
    add r12, r1, r5             ; relocate ALT to absolute address
    VERIFYADDRESS r12
    GETPARAM r11
    teq r11, #1
    streqb r0, [r12]
    teq r11, #2
    streqh r0, [r12]
    teq r11, #4
    streq r0, [r12]
    NEXT

OP_LIDX                         ; tested
    add r12, r1, r0, LSL #2     ; r12 = ALT + 4*PRI
    add r12, r12, r5            ; relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_LIDX_B
    GETPARAM r11
    add r12, r1, r0, LSL r11    ; r12 = ALT + (PRI << param)
    add r12, r12, r5            ; relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_IDXADDR                      ; tested
    add r0, r1, r0, LSL #2      ; PRI = ALT + 4*PRI
    NEXT

OP_IDXADDR_B
    GETPARAM r11
    add r0, r1, r0, LSL r11     ; PRI = ALT + (PRI << param)
    NEXT

OP_ALIGN_PRI                    ; tested
    GETPARAM r11
  IF :DEF:BIG_ENDIAN
  ELSE
    rsbs r11, r11, #4           ; rsb = reverse subtract; r11 = #4 - param
    eorhi r0, r0, r11           ; PRI ^= (#4 - param), but only if (#4 - param) > 0
  ENDIF
    NEXT

OP_ALIGN_ALT
    GETPARAM r11
  IF :DEF:BIG_ENDIAN
  ELSE
    rsbs r11, r11, #4           ; rsb = reverse subtract; r11 = #4 - param
    eorhi r1, r1, r11           ; ALT ^= (#4 - param), but only if (#4 - param) > 0
  ENDIF
    NEXT

OP_LCTRL
    GETPARAM r11                ; code of value to get
    teq r11, #0
    moveq r0, r8                ; 0 == code base address
    teq r11, #1
    moveq r0, r5                ; 1 == data base address
    teq r11, #2
    subeq r0, r3, r5            ; 2 == HEA (reverse relocated)
    teq r11, #3
    subeq r0, r2, r5            ; 3 == STP (reverse relocated)
    teq r11, #4
    subeq r0, r6, r5            ; 4 == STK (reverse relocated)
    teq r11, #5
    subeq r0, r7, r5            ; 5 == FRM (reverse relocated)
    teq r11, #6
    subeq r0, r4, r8            ; 6 == CIP (relative to code)
    NEXT

OP_SCTRL
    GETPARAM r11                ; code of value to get
    teq r11, #2
    addeq r3, r0, r5            ; 2 == HEA (relocated)
    teq r11, #4
    addeq r6, r0, r5            ; 4 == STK (reverse relocated)
    teq r11, #5
    addeq r7, r0, r5            ; 5 == FRM (reverse relocated)
    teq r11, #6
    subeq r4, r0, r8            ; 6 == CIP (relative to code)
    NEXT

OP_MOVE_PRI                     ; tested
    mov r0, r1
    NEXT

OP_MOVE_ALT                     ; tested
    mov r1, r0
    NEXT

OP_XCHG                         ; tested
    mov r11, r0
    mov r0, r1
    mov r1, r11
    NEXT

OP_PUSH_PRI                     ; tested
    VPUSH r0
    NEXT

OP_PUSH_ALT                     ; tested
    VPUSH r1
    NEXT

OP_PUSH_C                       ; tested
    GETPARAM r11
    VPUSH r11
    NEXT

OP_PUSH
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    NEXT

OP_PUSH_S                       ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    NEXT

OP_POP_PRI
    VPOP r0
    NEXT

OP_POP_ALT                      ; tested
    VPOP r1
    NEXT

OP_STACK                        ; tested
    GETPARAM r11
    sub r1, r6, r5              ; ALT = STK, reverse-relocated
    add r6, r6, r11             ; STK += param
    CHKMARGIN r12
    CHKSTACK
    NEXT

OP_HEAP                         ; tested
    GETPARAM r11
    sub r1, r3, r5              ; ALT = HEA, reverse-relocated
    add r3, r3, r11
    CHKMARGIN r12
    CHKHEAP r12
    NEXT

OP_PROC                         ; tested
    VPUSH r7
    mov r7, r6                  ; FRM = stk
    CHKMARGIN r12
    NEXT

OP_RET
    VPOP r7                     ; pop FRM
    VPOP r4                     ; pop CIP (return address)
    ; verify return address (avoid stack/buffer overflow)
    cmp r4, r8                  ; return address >= code_start ?
    blo err_memaccess           ; no, error
    cmp r4, r9                  ; return addres < code_end ?
    bhs err_memaccess           ; no, error
    ; all tests passed
    NEXT

OP_RETN                         ; tested
    VPOP r7                     ; pop FRM
    VPOP r4                     ; pop CIP (return address)
    ; verify return address (avoid stack/buffer overflow)
    cmp r4, r8                  ; return address >= code_start ?
    blo err_memaccess           ; no, error
    cmp r4, r9                  ; return addres < code_end ?
    bhs err_memaccess           ; no, error
    ; all tests passed
    ldr r11, [r6], #4           ; read value at the stack (#args passed to func), add 4 to STK
    add r6, r6, r11             ; STK += #args (+ 4, added in the LDR instruction)
    NEXT

err_memaccess
    mov r0, #AMX_ERR_MEMACCESS
    b   amx_exit

OP_CALL                         ; tested
    GETPARAM r11                ; get call address
    VPUSH r4                    ; r4 points to the next instruction
    mov r4, r11
    NEXT

OP_CALL_PRI                     ; not generated by pawncc
    VPUSH r4                    ; r4 points to the next instruction
    add r4, r8, r0              ; relocate call address (in PRI)
    NEXT

OP_JUMP                         ; tested
    ldr r4, [r4]                ; CIP = [CIP]
    NEXT

OP_JREL                         ; not generated by pawncc
    GETPARAM r11                ; get offset, increment CIP by 4
    add r4, r4, r11             ; add offset too
    NEXT

OP_JZER                         ; tested
    cmp r0, #0
    ldreq r4, [r4]              ; if PRI == 0, CIP = [CIP]
    addne r4, r4, #4            ; otherwise skip param
    NEXT

OP_JNZ                          ; tested
    cmp r0, #0
    ldrne r4, [r4]              ; if PRI != 0, CIP = [CIP]
    addeq r4, r4, #4            ; otherwise skip param
    NEXT

OP_JEQ                          ; tested
    cmp r0, r1
    ldreq r4, [r4]              ; if PRI == ALT, CIP = [CIP]
    addne r4, r4, #4            ; otherwise skip param
    NEXT

OP_JNEQ                         ; tested
    cmp r0, r1
    ldrne r4, [r4]              ; if PRI != ALT, CIP = [CIP]
    addeq r4, r4, #4            ; otherwise skip param
    NEXT

OP_JLESS                        ; not generated by pawncc
    cmp r0, r1
    ldrlo r4, [r4]              ; if PRI < ALT (unsigned), CIP = [CIP]
    addhs r4, r4, #4            ; otherwise skip param
    NEXT

OP_JLEQ                         ; not generated by pawncc
    cmp r0, r1
    ldrls r4, [r4]              ; if PRI <= ALT (unsigned), CIP = [CIP]
    addhi r4, r4, #4            ; otherwise skip param
    NEXT

OP_JGRTR                        ; not generated by pawncc
    cmp r0, r1
    ldrhi r4, [r4]              ; if PRI > ALT (unsigned), CIP = [CIP]
    addls r4, r4, #4            ; otherwise skip param
    NEXT

OP_JGEQ                         ; not generated by pawncc
    cmp r0, r1
    ldrhs r4, [r4]              ; if PRI >= ALT (unsigned), CIP = [CIP]
    addlo r4, r4, #4            ; otherwise skip param
    NEXT

OP_JSLESS                       ; tested
    cmp r0, r1
    ldrlt r4, [r4]              ; if PRI < ALT (signed), CIP = [CIP]
    addge r4, r4, #4            ; otherwise skip param
    NEXT

OP_JSLEQ                        ; tested
    cmp r0, r1
    ldrle r4, [r4]              ; if PRI <= ALT (signed), CIP = [CIP]
    addgt r4, r4, #4            ; otherwise skip param
    NEXT

OP_JSGRTR                       ; tested
    cmp r0, r1
    ldrgt r4, [r4]              ; if PRI > ALT (signed), CIP = [CIP]
    addle r4, r4, #4            ; otherwise skip param
    NEXT

OP_JSGEQ                        ; tested
    cmp r0, r1
    ldrge r4, [r4]              ; if PRI >= ALT (signed), CIP = [CIP]
    addlt r4, r4, #4            ; otherwise skip param
    NEXT

OP_SHL                          ; tested
    mov r0, r0, LSL r1          ; PRI = PRI << ALT
    NEXT

OP_SHR                          ; tested
    cmp r1, #0
    movhi r0, r0, LSR r1        ; PRI = PRI >> ALT (but check that ALT > 0)
    NEXT

OP_SSHR                         ; tested
    cmp r1, #0
    movhi r0, r0, ASR r1        ; PRI = PRI >> ALT (but check that ALT > 0)
    NEXT

OP_SHL_C_PRI
    GETPARAM r11
    mov r0, r0, LSL r11         ; PRI = PRI << param
    NEXT

OP_SHL_C_ALT
    GETPARAM r11
    mov r1, r1, LSL r11         ; ALT = ALT << param
    NEXT

OP_SHR_C_PRI
    GETPARAM r11
    cmp r11, #0
    movhi r0, r0, LSR r11       ; PRI = PRI >> param (but check that param > 0)
    NEXT

OP_SHR_C_ALT
    GETPARAM r11
    cmp r11, #0
    movhi r1, r1, LSR r11       ; ALT = ALT >> param (but check that param > 0)
    NEXT

OP_SMUL                         ; tested
OP_UMUL                         ; tested (not generated by pawncc)
    mov r11, r0                 ; copy r0
    mul r0, r11, r1             ; dest must be different from source registers
    NEXT

OP_SDIV_ALT                     ; tested
    ; swap r0 and r1, then drop into normal (signed) division case
    mov r11, r0
    mov r0, r1
    mov r1, r11
OP_SDIV                         ; tested (not generated by pawncc)
    teq r1, #0                  ; verify r1 (divisor)
    moveq r0, #AMX_ERR_DIVIDE   ; r1 == 0 -> set error code
    beq amx_exit                ; r1 == 0 -> jump to error-exit
    stmfd sp!, {r2 - r3}        ; need two more scratch registers
    ; save input registers and create absolute values
    movs r2, r0
    rsbmi r0, r0, #0            ; if r2 < 0, r0 = #0 - r0
    movs r3, r1
    rsbmi r1, r1, #0            ; if r3 < 0, r1 = #0 - r1
    ; do the division
    bl  amx_div                 ; r0 = abs(PRI) / abs(ALT), r1 = abs(PRI) % abs(ALT)
    ; patch signs
    cmp r3, #0                  ; check sign of original value of ALT (divisor)
    rsbmi r1, r1, #0            ; sign(remainder) = sign(divisor)
    teq r2, r3                  ; check signs of dividend and divisor
    bpl op_div_done             ; sign(divident) == sign(divisor) -> done
    rsb r0, r0, #0              ; sign(quotient) = sign(divident) XOR sign(divisor)
    ; so the quotient is negative (or zero); if the remainder is non-zero,
    ; floor the quotient and adjust the remainder
    cmp r1, #0
    subne r0, r0, #1            ; remainder != 0 -> r0 = r0 - 1
    rsbne r1, r1, r3            ; remainder != 0 -> r1 = divisor - r1
op_div_done
    ldmfd sp!, {r2 - r3}
    NEXT

OP_UDIV_ALT                     ; not generated by pawncc
    ; swap r0 and r1, then drop into normal (unsigned) division case
    mov r11, r0
    mov r0, r1
    mov r1, r11
OP_UDIV                         ; not generated by pawncc
    teq r1, #0                  ; verify r1 (divisor)
    moveq r0, #AMX_ERR_DIVIDE   ; r1 == 0 -> set error code
    beq amx_exit                ; r1 == 0 -> jump to error-exit
    bl  amx_div                 ; PRI = PRI / ALT, ALT = PRI % ALT
    NEXT

OP_ADD                          ; tested
    add r0, r0, r1
    NEXT

OP_SUB
    sub r0, r0, r1
    NEXT

OP_SUB_ALT                      ; tested
    sub r0, r1, r0
    NEXT

OP_AND                          ; tested
    and r0, r0, r1
    NEXT

OP_OR                           ; tested
    orr r0, r0, r1
    NEXT

OP_XOR                          ; tested
    eor r0, r0, r1
    NEXT

OP_NOT                          ; tested
    teq r0, #0
    moveq r0, #1
    movne r0, #0
    NEXT

OP_NEG                          ; tested
    rsb r0, r0, #0              ; PRI = #0 - PRI
    NEXT

OP_INVERT                       ; tested
    mvn r0, r0                  ; PRI = NOT PRI (all bits inverted)
    NEXT

OP_ADD_C                        ; tested
    GETPARAM r11
    add r0, r0, r11             ; PRI += param
    NEXT

OP_SMUL_C                       ; tested
    GETPARAM r11
    mov r12, r0
    mul r0, r11, r12            ; PRI *= param
    NEXT

OP_ZERO_PRI                     ; tested
    mov r0, #0
    NEXT

OP_ZERO_ALT
    mov r1, #0
    NEXT

OP_ZERO                         ; tested
    GETPARAM r11
    mov r12, #0
    str r12, [r5, r11]
    NEXT

OP_ZERO_S                       ; tested
    GETPARAM r11
    mov r12, #0
    str r12, [r7, r11]
    NEXT

OP_SIGN_PRI                     ; not generated by pawncc
    tst r0, #0x80               ; bit 7 set?
    mvnne r11, #0xff            ; sets 0xffffff00
    orrne r0, r0, r11           ; yes, sign-extend to the top 24 bits
    NEXT

OP_SIGN_ALT                     ; not generated by pawncc
    tst r1, #0x80               ; bit 7 set?
    mvnne r11, #0xff            ; sets 0xffffff00
    orrne r1, r1, r11           ; yes, sign-extend to the top 24 bits
    NEXT

OP_EQ
    cmp r0, r1
    moveq r0, #1
    movne r0, #0
    NEXT

OP_NEQ
    cmp r0, r1
    movne r0, #1
    moveq r0, #0
    NEXT

OP_LESS                         ; not generated by pawncc
    cmp r0, r1
    movlo r0, #1
    movhs r0, #0
    NEXT

OP_LEQ                          ; not generated by pawncc
    cmp r0, r1
    movls r0, #1
    movhi r0, #0
    NEXT

OP_GRTR                         ; not generated by pawncc
    cmp r0, r1
    movhi r0, #1
    movls r0, #0
    NEXT

OP_GEQ                          ; not generated by pawncc
    cmp r0, r1
    movhs r0, #1
    movlo r0, #0
    NEXT

OP_SLESS                        ; tested
    cmp r0, r1
    movlt r0, #1
    movge r0, #0
    NEXT

OP_SLEQ                         ; tested
    cmp r0, r1
    movle r0, #1
    movgt r0, #0
    NEXT

OP_SGRTR                        ; tested
    cmp r0, r1
    movgt r0, #1
    movle r0, #0
    NEXT

OP_SGEQ                         ; tested
    cmp r0, r1
    movge r0, #1
    movlt r0, #0
    NEXT

OP_EQ_C_PRI                     ; tested
    GETPARAM r11
    cmp r0, r11
    moveq r0, #1
    movne r0, #0
    NEXT

OP_EQ_C_ALT
    GETPARAM r11
    cmp r1, r11
    moveq r0, #1
    movne r0, #0
    NEXT

OP_INC_PRI
    add r0, r0, #1
    NEXT

OP_INC_ALT
    add r1, r1, #1
    NEXT

OP_INC                          ; tested
    GETPARAM r11
    ldr r12, [r5, r11]
    add r12, r12, #1
    str r12, [r5, r11]
    NEXT

OP_INC_S                        ; tested
    GETPARAM r11
    ldr r12, [r7, r11]
    add r12, r12, #1
    str r12, [r7, r11]
    NEXT

OP_INC_I                        ; tested
    ldr r12, [r5, r0]
    add r12, r12, #1
    str r12, [r5, r0]
    NEXT

OP_DEC_PRI
    sub r0, r0, #1
    NEXT

OP_DEC_ALT
    sub r1, r1, #1
    NEXT

OP_DEC                          ; tested
    GETPARAM r11
    ldr r12, [r5, r11]
    sub r12, r12, #1
    str r12, [r5, r11]
    NEXT

OP_DEC_S                        ; tested
    GETPARAM r11
    ldr r12, [r7, r11]
    sub r12, r12, #1
    str r12, [r7, r11]
    NEXT

OP_DEC_I                        ; tested
    ldr r12, [r5, r0]
    sub r12, r12, #1
    str r12, [r5, r0]
    NEXT

OP_MOVS                         ; tested
    GETPARAM r11
    sub r11, r11, #1            ; decrement, for address verification
    add r12, r0, r5             ; r12 = relocated PRI
    VERIFYADDRESS r12           ; verify PRI (after relocation)
    add r12, r12, r11
    VERIFYADDRESS r12           ; verify PRI + size - 1
    add r12, r1, r5             ; r12 = relocated ALT
    VERIFYADDRESS r12           ; verify ALT (after relocation)
    add r12, r12, r11
    VERIFYADDRESS r12           ; verify ALT + size - 1
    ; dropped through tests
    add r11, r11, #1            ; restore r11 (# bytes to move)
    str r0, [sp, #-4]!          ; save PRI and ALT
    str r1, [sp, #-4]!
    add r0, r0, r5              ; relocate r0/r1
    add r1, r1, r5
movs4loop
    subs r11, r11, #4
    ldrge r12, [r0], #4
    strge r12, [r1], #4
    bgt movs4loop
movs1loop
    subs r11, r11, #1
    ldrgeb r12, [r0], #1
    strgeb r12, [r1], #1
    bgt movs1loop
    ; restore PRI and ALT
    ldr r1, [sp], #4            ; restore PRI and ALT
    ldr r0, [sp], #4
    NEXT

OP_CMPS 			; not generated by  pawncc ???
    GETPARAM r11
    sub r11, r11, #1            ; decrement, for address verification
    add r12, r0, r5             ; r12 = relocated PRI
    VERIFYADDRESS r12           ; verify PRI
    add r12, r12, r11
    VERIFYADDRESS r12           ; verify PRI + size - 1
    add r12, r1, r5             ; r12 = relocated ALT
    VERIFYADDRESS r12           ; verify ALT
    add r12, r12, r11
    VERIFYADDRESS r12           ; verify ALT + size - 1
    ; dropped through tests
    add r11, r11, #1            ; restore r11
    str r1, [sp, #-4]!          ; save ALT
    add r0, r0, r5              ; relocate r0 and r1
    add r1, r1, r5
    mov r14, #0                 ; preset r14, in case r11 == 0
cmps4loop
    subs r11, r11, #4
    addmi r11, r11, #4
    bmi cmps1loop
    ldr r14, [r0], #4
    ldr r12, [r1], #4
    subs r14, r14, r12          ; r14 = [PRI] - [ALT]
    bne cmpsdone
    b cmps4loop
cmps1loop
    subs r11, r11, #1
    bmi cmpsdone
    ldrb r14, [r0], #1
    ldrb r12, [r1], #1
    subs r14, r14, r12          ; r14 = [PRI] - [ALT]
    beq cmps1loop
cmpsdone
    ldr r1, [sp], #4            ; restore ALT (PRI is changed by this routine)
    mov r0, r14
    NEXT

OP_FILL                         ; tested
    GETPARAM r11
    add r12, r1, r5             ; r12 = relocated ALT
    VERIFYADDRESS r12           ; verify ALT
    add r12, r12, r11
    sub r12, r12, #1
    VERIFYADDRESS r12           ; verify ALT + size - 1
    ; dropped through tests
    add r12, r1, r5             ; r12 = relocated ALT again
fill4loop
    subs r11, r11, #4
    strge r0, [r12], #4
    bgt fill4loop
    NEXT

OP_HALT                         ; tested
    ldr r11, [sp]               ; get "retval" pointer
    teq r11, #0
    strne r0, [r11]             ; store PRI, but only if r11 != 0
    GETPARAM r0                 ; parameter = return code from function
    b   amx_exit

OP_BOUNDS                       ; tested
    GETPARAM r11
    cmp r0, r11
    movhi r0, #AMX_ERR_BOUNDS
    bhi amx_exit
    NEXT

OP_SYSREQ_C                     ; tested
    GETPARAM r0                 ; native function index in r0
OP_SYSREQ_PRI                   ; tested (not generated by pawncc)
    ; store stack and heap state AMX state
    sub r11, r7, r5             ; reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             ; reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             ; reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             ; reverse-relocate CIP
    str r11, [r10, #amxCIP]
    ; invoke callback
    stmfd sp!, {r1 - r3}        ; save some extra registers
    sub sp, sp, #4              ; reserve a cell on the stack for the return value
    mov r1, r0                  ; 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 ; 1st arg = AMX
    mov r2, sp                  ; 3rd arg = address of return value
    mov r3, r6                  ; 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]; callback function pointer in r11
    mov lr, pc                  ; simulate BLX with MOV and BX
    bx  r11
    ldr r11, [sp], #4           ; get return value (in r11), restore stack
    ldmfd sp!, {r1 - r3}        ; restore registers
    teq r0, #AMX_ERR_NONE       ; callback hook returned error/abort code?
    bne amx_exit                ; yes -> quit
    mov r0, r11                 ; no, set PRI to the return value
    NEXT

OP_SYSREQ_N                     ; tested
    GETPARAM r0                 ; get native function index
    GETPARAM r12                ; get # parameters
    VPUSH r12                   ; push second parameter
    ; store stack and heap state AMX state
    sub r11, r7, r5             ; reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             ; reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             ; reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             ; reverse-relocate CIP
    str r11, [r10, #amxCIP]
    ; invoke callback
    stmfd sp!, {r1 - r3, r12}   ; save some extra registers
    sub sp, sp, #4              ; reserve a cell on the stack for the return value
    mov r1, r0                  ; 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 ; 1st arg = AMX
    mov r2, sp                  ; 3rd arg = address of return value
    mov r3, r6                  ; 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]; callback function pointer in r11
    mov lr, pc                  ; simulate BLX with MOV and BX
    bx  r11
    ldr r11, [sp], #4           ; get return value (in r11), restore ARM stack
    ldmfd sp!, {r1 - r3, r12}   ; restore registers
    add r6, r6, r12             ; remove # parameters from the AMX stack
    add r6, r6, #4              ; also remove the extra cell pushed
    teq r0, #AMX_ERR_NONE       ; callback hook returned error/abort code?
    bne amx_exit                ; yes -> quit
    mov r0, r11                 ; no, set PRI to the return value
    NEXT

OP_SYSREQ_D                     ; tested
    GETPARAM r12                ; address of native function in r12
    ; store stack and heap state AMX state
    sub r11, r7, r5             ; reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             ; reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             ; reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             ; reverse-relocate CIP
    str r11, [r10, #amxCIP]
    ; invoke callback (address still in r12)
    stmfd sp!, {r1 - r3}        ; save some extra registers
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r6                  ; 2nd arg = address in the AMX stack
    mov lr, pc                  ; simulate BLX with MOV and BX
    bx  r12
    ldmfd sp!, {r1 - r3}        ; restore registers
    ldr r11, [r10, #amxError]   ; get error returned by native function
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    movne r0, r11               ; yes -> set error in r0 (return value)
    bne amx_exit                ; yes -> quit
    NEXT

OP_SYSREQ_ND                    ; tested
    GETPARAM r0                 ; address of native function in r0 (temporary)
    GETPARAM r12                ; get # parameters
    VPUSH r12                   ; push second parameter
    ; store stack and heap state AMX state
    sub r11, r7, r5             ; reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             ; reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             ; reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             ; reverse-relocate CIP
    str r11, [r10, #amxCIP]
    ; invoke callback (address still in r0)
    mov r11, r0                 ; get address to call in r11 (r0 is overwritten)
    stmfd sp!, {r1 - r3, r12}   ; save some extra registers
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r6                  ; 2nd arg = address in the AMX stack
    mov lr, pc                  ; simulate BLX with MOV and BX
    bx  r11
    ldmfd sp!, {r1 - r3, r12}   ; restore registers
    add r6, r6, r12             ; remove # parameters from the AMX stack
    add r6, r6, #4              ; also remove the extra cell pushed
    ldr r11, [r10, #amxError]   ; get error returned by native function
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    movne r0, r11               ; yes -> set error in r0 (return value)
    bne amx_exit                ; yes -> quit
    NEXT

OP_PUSH_R_PRI
OP_FILE
OP_LINE
OP_SYMBOL
OP_SRANGE
OP_SYMTAG
OP_CASETBL
    mov r0, #AMX_ERR_INVINSTR   ; these instructions are no longer supported
    b   amx_exit

OP_JUMP_PRI                     ; not generated by pawncc
    add r4, r0, r8              ; CIP = PRI + code_start
    NEXT

OP_SWITCH                       ; tested
    ldr r11, [r4]               ; r11 = [CIP], offset to case-table
    ldr r12, [r11, #4]!         ; r12 = number of case-table records, skip OP_CASETBL opcode
    ldr r4, [r11, #4]           ; preset CIP to "default" case (none-matched)
op_switch_loop
    subs r12, r12, #1           ; decrement number to do; any left?
    bmi op_switch_done          ; no, quit (CIP already set to the default value)
    add r11, r11, #8            ; move to next record
    ldr r14, [r11]              ; get the case value
    cmp r0, r14                 ; case value identical to PRI ?
    bne op_switch_loop          ; no, continue
    ldr r4, [r11, #4]           ; yes, load matching CIP and exit loop
op_switch_done
    NEXT

OP_SWAP_PRI                     ; tested
    ldr r11, [r6]
    str r0, [r6]
    mov r0, r11
    NEXT

OP_SWAP_ALT
    ldr r11, [r6]
    str r1, [r6]
    mov r1, r11
    NEXT

OP_PUSH_ADR                     ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    VPUSH r11
    NEXT

OP_NOP
    NEXT

OP_BREAK                        ; tested
    ldr r12, [r10, #amxDebug]
    teq r12, #0
    beq op_break_quit
    ; store stack and heap state AMX state
    sub r11, r7, r5             ; reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             ; reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             ; reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             ; reverse-relocate CIP
    str r11, [r10, #amxCIP]
    ; invoke debug hook (address still in r12)
    stmfd   sp!, {r1 - r3}      ; save some extra registers
    mov r0, r10                 ; 1st arg = AMX
    mov lr, pc                  ; simulate BLX with MOV and BX
    bx  r12
    ldmfd sp!, {r1 - r3}        ; restore registers
    teq r0, #0                  ; debug hook returned error/abort code?
    bne amx_exit                ; yes -> quit
op_break_quit
    NEXT

OP_PUSH5                        ; tested
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    ; drop through
OP_PUSH4                        ; tested
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    ; drop through
OP_PUSH3                        ; tested
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    ; drop through
OP_PUSH2                        ; tested
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    GETPARAM r11
    ldr r11, [r5, r11]
    VPUSH r11
    NEXT

OP_PUSH5_S                      ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    ; drop through
OP_PUSH4_S                      ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    ; drop through
OP_PUSH3_S                      ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    ; drop through
OP_PUSH2_S                      ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    GETPARAM r11
    ldr r11, [r7, r11]
    VPUSH r11
    NEXT

OP_PUSH5_C
    GETPARAM r11
    VPUSH r11
    ; drop through
OP_PUSH4_C                      ; tested
    GETPARAM r11
    VPUSH r11
    ; drop through
OP_PUSH3_C                      ; tested
    GETPARAM r11
    VPUSH r11
    ; drop through
OP_PUSH2_C                      ; tested
    GETPARAM r11
    VPUSH r11
    GETPARAM r11
    VPUSH r11
    NEXT

OP_PUSH5_ADR                    ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    VPUSH r11
    ; drop through
OP_PUSH4_ADR                    ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    VPUSH r11
    ; drop through
OP_PUSH3_ADR                    ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    VPUSH r11
    ; drop through
OP_PUSH2_ADR                    ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    VPUSH r11
    GETPARAM r11
    add r11, r11, r7
    sub r11, r11, r5
    VPUSH r11
    NEXT

OP_LOAD_BOTH                    ; tested
    GETPARAM r11
    ldr r0, [r5, r11]
    GETPARAM r11
    ldr r1, [r5, r11]
    NEXT

OP_LOAD_S_BOTH                  ; tested
    GETPARAM r11
    ldr r0, [r7, r11]
    GETPARAM r11
    ldr r1, [r7, r11]
    NEXT

OP_CONST                        ; tested
    GETPARAM r11
    GETPARAM r12
    str r12, [r5, r11]
    NEXT

OP_CONST_S                      ; tested
    GETPARAM r11
    GETPARAM r12
    str r12, [r7, r11]
    NEXT

amx_exit                        ; assume r0 already set
    ; reverse relocate registers
    sub r3, r3, r5              ; reverse-relocate HEA
    sub r6, r6, r5              ; reverse-relocate STK
    sub r7, r7, r5              ; reverse-relocate FRM

    ; store stack and heap state AMX state
    str r0, [r10, #amxPRI]      ; PRI (redundant store)
    str r1, [r10, #amxALT]      ; ALT
    str r3, [r10, #amxHEA]      ; HEA
    str r4, [r10, #amxCIP]      ; CIP
    str r6, [r10, #amxSTK]      ; STK
    str r7, [r10, #amxFRM]      ; FRM

    add sp, sp, #4              ; drop register for the return value
    ldmfd sp!, {r4 - r12, lr}
    bx  lr
; amx_exec_asm


    ALIGN   2
    EXPORT  amx_div
amx_div
    ; expects divident in r0, divisor in r1
    ; on exit quotient is in r0, remainder in r1
    ; r11 and r12 are scratch; r12 is temporary result
    ; unsigned division only; when r1 (divisor) is zero, the function returns
    ; with all registers unchanged
    teq r1, #0                  ; verify r1
    moveq pc, lr                ; just for security
    ; drop-through (divisor is not zero)
    mov r11, #1
amx_div1
    cmp r1, #0x80000000         ; shift divisor left until top bit set
    cmpcc r1, r0                ; ...or divisor>divident
    movcc r1, r1, LSL #1        ; shift divisor left if required
    movcc r11, r11, LSL #1      ; shift r11 left if required
    bcc amx_div1                ; repeat whilst more shifting required
    mov r12, #0                 ; used to store result (temporary)
amx_div2
    cmp r0, r1                  ; test for possible subtraction
    subcs r0, r0, r1            ; subtract if divident>divisor
    addcs r12, r12, r11         ; put relevant bit into result
    movs  r11, r11, LSR #1      ; shift control bit
    movne r1, r1, LSR #1        ; halve unless finished
    bne amx_div2                ; loop if there is more to do
    ; remainder r0, result in r12
    mov r1, r0                  ; remainder now in r1
    mov r0, r12                 ; quotient now in r0
    mov pc, lr
; amx_div

    END
