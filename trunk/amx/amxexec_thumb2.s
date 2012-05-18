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
;   Call Standard (AAPCS). This applies both to the function amx_exec_run
;   implemented in this file as to the debug hook function, callback hook
;   function and any native functions called directly from the abstract
;   machine.
;
;
;   Copyright (c) ITB CompuPhase, 2006-2011
;
;   Licensed under the Apache License, Version 2.0 (the "License"); you may not
;   use this file except in compliance with the License. You may obtain a copy
;   of the License at
;
;       http://www.apache.org/licenses/LICENSE-2.0
;
;   Unless required by applicable law or agreed to in writing, software
;   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
;   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
;   License for the specific language governing permissions and limitations
;   under the License.
;
;   Version: $Id: amxexec_arm7.s 4125 2009-06-15 16:51:06Z thiadmer $


    AREA    amxexec_data, DATA, READONLY
    ALIGN   2

 IF :LNOT::DEF:AMX_NO_PACKED_OPC
  IF :LNOT::DEF:AMX_TOKENTHREADING
    GBLA AMX_TOKENTHREADING
AMX_TOKENTHREADING SETA 1      ; packed opcodes require token threading
  ENDIF
 ENDIF

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

amxBase           EQU    0     ; points to the AMX header, perhaps followed by P-code and data
amxCode           EQU    4     ; points to P-code block, possibly in ROM or in an overlay pool
amxData           EQU    8     ; points to separate data+stack+heap, may be NULL
amxCallback       EQU    12
amxDebug          EQU    16    ; debug callback
amxOverlay        EQU    20    ; overlay callback
amxCIP            EQU    24    ; instruction pointer: relative to base + amxhdr->cod
amxFRM            EQU    28    ; stack frame base: relative to base + amxhdr->dat
amxHEA            EQU    32    ; top of the heap: relative to base + amxhdr->dat
amxHLW            EQU    36    ; bottom of the heap: relative to base + amxhdr->dat
amxSTK            EQU    40    ; stack pointer: relative to base + amxhdr->dat
amxSTP            EQU    44    ; top of the stack: relative to base + amxhdr->dat
amxFlags          EQU    48    ; current status, see amx_Flags()
amxUserTags       EQU    52    ; user data, AMX_USERNUM fields
amxUserData       EQU    68    ; user data
amxError          EQU    84    ; native functions that raise an error
amxParamCount     EQU    88    ; passing parameters requires a "count" field
amxPRI            EQU    92    ; the sleep opcode needs to store the full AMX status
amxALT            EQU    96
amx_reset_stk     EQU    100
amx_reset_hea     EQU    104
amx_sysreq_d      EQU    108   ; relocated address/value for the SYSREQ.D opcode
amxOvlIndex       EQU    112
amxCodeSize       EQU    116   ; memory size of the overlay or of the native code
amx_reloc_size    EQU    120   ; (JIT) required temporary buffer for relocations


    EXPORT  amx_opcodelist
amx_opcodelist
_amx_opcodelist
    ; core set
    DCD     OP_NOP
    DCD     OP_LOAD_PRI
    DCD     OP_LOAD_ALT
    DCD     OP_LOAD_S_PRI
    DCD     OP_LOAD_S_ALT
    DCD     OP_LREF_S_PRI
    DCD     OP_LREF_S_ALT
    DCD     OP_LOAD_I
    DCD     OP_LODB_I
    DCD     OP_CONST_PRI
    DCD     OP_CONST_ALT
    DCD     OP_ADDR_PRI
    DCD     OP_ADDR_ALT
    DCD     OP_STOR
    DCD     OP_STOR_S
    DCD     OP_SREF_S
    DCD     OP_STOR_I
    DCD     OP_STRB_I
    DCD     OP_ALIGN_PRI
    DCD     OP_LCTRL
    DCD     OP_SCTRL
    DCD     OP_XCHG
    DCD     OP_PUSH_PRI
    DCD     OP_PUSH_ALT
    DCD     OP_PUSHR_PRI
    DCD     OP_POP_PRI
    DCD     OP_POP_ALT
    DCD     OP_PICK
    DCD     OP_STACK
    DCD     OP_HEAP
    DCD     OP_PROC
    DCD     OP_RET
    DCD     OP_RETN
    DCD     OP_CALL
    DCD     OP_JUMP
    DCD     OP_JZER
    DCD     OP_JNZ
    DCD     OP_SHL
    DCD     OP_SHR
    DCD     OP_SSHR
    DCD     OP_SHL_C_PRI
    DCD     OP_SHL_C_ALT
    DCD     OP_SMUL
    DCD     OP_SDIV
    DCD     OP_ADD
    DCD     OP_SUB
    DCD     OP_AND
    DCD     OP_OR
    DCD     OP_XOR
    DCD     OP_NOT
    DCD     OP_NEG
    DCD     OP_INVERT
    DCD     OP_EQ
    DCD     OP_NEQ
    DCD     OP_SLESS
    DCD     OP_SLEQ
    DCD     OP_SGRTR
    DCD     OP_SGEQ
    DCD     OP_INC_PRI
    DCD     OP_INC_ALT
    DCD     OP_INC_I
    DCD     OP_DEC_PRI
    DCD     OP_DEC_ALT
    DCD     OP_DEC_I
    DCD     OP_MOVS
    DCD     OP_CMPS
    DCD     OP_FILL
    DCD     OP_HALT
    DCD     OP_BOUNDS
    DCD     OP_SYSREQ
    DCD     OP_SWITCH
    DCD     OP_SWAP_PRI
    DCD     OP_SWAP_ALT
    DCD     OP_BREAK
    DCD     OP_CASETBL
    ; patched instructions
    DCD     OP_SYSREQ_D
    DCD     OP_SYSREQ_ND
    ; overlay instructions
 IF :LNOT::DEF:AMX_NO_OVERLAY
    DCD     OP_CALL_OVL
    DCD     OP_RETN_OVL
    DCD     OP_SWITCH_OVL
    DCD     OP_CASETBL_OVL
 ENDIF  ; AMX_NO_OVERLAY
    ; supplemental instructions
 IF :LNOT::DEF:AMX_NO_MACRO_INSTR
    DCD     OP_LIDX
    DCD     OP_LIDX_B
    DCD     OP_IDXADDR
    DCD     OP_IDXADDR_B
    DCD     OP_PUSH_C
    DCD     OP_PUSH
    DCD     OP_PUSH_S
    DCD     OP_PUSH_ADR
    DCD     OP_PUSHR_C
    DCD     OP_PUSHR_S
    DCD     OP_PUSHR_ADR
    DCD     OP_JEQ
    DCD     OP_JNEQ
    DCD     OP_JSLESS
    DCD     OP_JSLEQ
    DCD     OP_JSGRTR
    DCD     OP_JSGEQ
    DCD     OP_SDIV_INV
    DCD     OP_SUB_INV
    DCD     OP_ADD_C
    DCD     OP_SMUL_C
    DCD     OP_ZERO_PRI
    DCD     OP_ZERO_ALT
    DCD     OP_ZERO
    DCD     OP_ZERO_S
    DCD     OP_EQ_C_PRI
    DCD     OP_EQ_C_ALT
    DCD     OP_INC
    DCD     OP_INC_S
    DCD     OP_DEC
    DCD     OP_DEC_S
    DCD     OP_SYSREQ_N
    DCD     OP_PUSHM_C
    DCD     OP_PUSHM
    DCD     OP_PUSHM_S
    DCD     OP_PUSHM_ADR
    DCD     OP_PUSHRM_C
    DCD     OP_PUSHRM_S
    DCD     OP_PUSHRM_ADR
    DCD     OP_LOAD2
    DCD     OP_LOAD2_S
    DCD     OP_CONST
    DCD     OP_CONST_S
 ENDIF  ; AMX_NO_MACRO_INSTR
    ; packed opcodes
 IF :LNOT::DEF:AMX_NO_PACKED_OPC
    DCD     OP_LOAD_P_PRI
    DCD     OP_LOAD_P_ALT
    DCD     OP_LOAD_P_S_PRI
    DCD     OP_LOAD_P_S_ALT
    DCD     OP_LREF_P_S_PRI
    DCD     OP_LREF_P_S_ALT
    DCD     OP_LODB_P_I
    DCD     OP_CONST_P_PRI
    DCD     OP_CONST_P_ALT
    DCD     OP_ADDR_P_PRI
    DCD     OP_ADDR_P_ALT
    DCD     OP_STOR_P
    DCD     OP_STOR_P_S
    DCD     OP_SREF_P_S
    DCD     OP_STRB_P_I
    DCD     OP_LIDX_P_B
    DCD     OP_IDXADDR_P_B
    DCD     OP_ALIGN_P_PRI
    DCD     OP_PUSH_P_C
    DCD     OP_PUSH_P
    DCD     OP_PUSH_P_S
    DCD     OP_PUSH_P_ADR
    DCD     OP_PUSHR_P_C
    DCD     OP_PUSHR_P_S
    DCD     OP_PUSHR_P_ADR
    DCD     OP_PUSHM_P_C
    DCD     OP_PUSHM_P
    DCD     OP_PUSHM_P_S
    DCD     OP_PUSHM_P_ADR
    DCD     OP_PUSHRM_P_C
    DCD     OP_PUSHRM_P_S
    DCD     OP_PUSHRM_P_ADR
    DCD     OP_STACK_P
    DCD     OP_HEAP_P
    DCD     OP_SHL_P_C_PRI
    DCD     OP_SHL_P_C_ALT
    DCD     OP_ADD_P_C
    DCD     OP_SMUL_P_C
    DCD     OP_ZERO_P
    DCD     OP_ZERO_P_S
    DCD     OP_EQ_P_C_PRI
    DCD     OP_EQ_P_C_ALT
    DCD     OP_INC_P
    DCD     OP_INC_P_S
    DCD     OP_DEC_P
    DCD     OP_DEC_P_S
    DCD     OP_MOVS_P
    DCD     OP_CMPS_P
    DCD     OP_FILL_P
    DCD     OP_HALT_P
    DCD     OP_BOUNDS_P
 ENDIF  ; AMX_NO_PACKED_OPC
opcodelist_size EQU .-amx_opcodelist


    MACRO
    NEXT
      IF :DEF:AMX_TOKENTHREADING
        IF :DEF:AMX_NO_PACKED_OPC
          ldr r11, [r4], #4     ; get opcode, increment CIP
        ELSE
          ldr r12, [r4], #4     ; get opcode + parameter, increment CIP
          and r11, r12, #0xff   ; keep only the opcode in r11, r12 holds the parameter
        ENDIF
        ldr pc, [r14, r11, LSL #2]
      ELSE
        IF :LNOT::DEF:AMX_NO_PACKED_OPC
          INFO 1, "opcode packing requires token threading"
        ENDIF
        ldr pc, [r4], #4        ; indirect register jump
      ENDIF
    MEND

    MACRO
    GETPARAM $rx
    ldr $rx, [r4], #4           ; $rx = [CIP], CIP += 4
    MEND

    MACRO
    GETPARAM_P $rx              ; the opcode/parameter pack should be in r12
    mov $rx, r12, ASR #16       ; $rx = r12 >> 16 (signed)
    MEND

    MACRO
    JUMPREL $rtmp               ; $rtmp = temp register to use, $cc = condition code
      ldr $rtmp, [r4], #-4      ; $rtmp = [CIP], CIP -= 4 (restore CIP to start of instruction)
      add r4, r4, $rtmp         ; CIP = CIP + [CIP] - 4
    MEND

    MACRO
    JUMPRELCC $rtmp, $cc, $ic   ; $rtmp = temp register to use, $cc = condintion code, $ic = inverted $cc
      itte $cc
      ldr$cc $rtmp, [r4], #-4   ; $rtmp = [CIP], CIP -= 4 (restore CIP to start of instruction)
      add$cc r4, r4, $rtmp      ; CIP = CIP + [CIP] - 4
      add$ic r4, r4, #4         ; otherwise skip param
    MEND

    MACRO
    mPUSH $rx
    str $rx, [r6, #-4]!         ; STK -= 4, [STK] = $rx
    MEND

    MACRO
    mPOP $rx
    ldr $rx, [r6], #4           ; $rx = [STK], STK += 4
    MEND

    MACRO
    CHKMARGIN $rtmp             ; $rtmp = temp register to use
    add $rtmp, r3, #64          ; 64 = 16 cells
    cmp $rtmp, r6               ; compare HEA + 16*cell to STK
    itt hi
    movhi r11, #AMX_ERR_STACKERR
    bhi.w amx_exit
    MEND

    MACRO
    CHKSTACK
    cmp r6, r2                  ; compare STK to STP
    itt hi
    movhi r11, #AMX_ERR_STACKLOW
    bhi.w amx_exit
    MEND

    MACRO
    CHKHEAP $rtmp               ; $rtmp = temp register to use
    ldr $rtmp, [r10, #amxHLW]
    cmp r3, $rtmp               ; compare HEA to HLW
    itt lo
    movlo r11, #AMX_ERR_HEAPLOW
    blo.w amx_exit
    MEND

    MACRO
    VERIFYADDRESS $rx
    cmp $rx, r2                 ; $rx >= STP ?
    bhs.w err_memaccess         ; yes -> error
    ; One might want to relax the test and remove the three instructions below.
    ; If a register points into the "free" area between the stack and the heap,
    ; it does not breach the sandbox.
    cmp r3, $rx                 ; HEA <= $rx ?
    it ls                       ; prepare cmpls below
    cmpls $rx, r6               ; yes, then: $rx < STK ?
    blo.w err_memaccess         ; yes, then HEA <= $rx && $rx < STK (invalid area)
    MEND

    MACRO
    CALL $rx
    IF :DEF:THUMB2
      blx $rx
    ELSE
      mov lr, pc                ; simulate BLX with MOV and BX
      bx $rx
    ENDIF
    MEND

; ================================================================

    AREA    amxexec_code, CODE, READONLY
    IF :DEF:THUMB2
      THUMB
    ELSE
      CODE32
    ENDIF
    ALIGN   2

amx_opcodelist_addr
    DCD   amx_opcodelist


; ----------------------------------------------------------------
; cell amx_exec_list(AMX *amx, cell **opcodelist,int *numopcodes)
;                         r0          r1              r2
; ----------------------------------------------------------------

    EXPORT  amx_exec_list
amx_exec_list
    ldr r0, amx_opcodelist_addr     ; r0 = opcode table address
    str r0, [r1]                    ; store in parameter 'opcodelist'
    mov r0, #opcodelist_size
    mov r0, r0, LSR #2              ; number of opcodes, not bytes
    str r0, [r2]                    ; store in parameter 'numopcodes'
    mov r0, #0                      ; no specific return value)
    bx  lr


; ----------------------------------------------------------------
; cell amx_exec_run(AMX *amx, cell *retval, char *data)
;                        r0         r1            r2
; ----------------------------------------------------------------

    EXPORT  amx_exec_run
amx_exec_run
    ; save non-scratch registers
    stmfd sp!, {r4 - r12, lr}

    ; save the register that holds the address for the return value
    ; we only need this at the point of returning, so it would be
    ; a waste to keep it in a register
    str r1, [sp, #-8]!          ; decrement by 8 to keep 8-byte alignment of sp

    ; set up the registers
    ; r0  = PRI
    ; r1  = ALT
    ; r2  = STP (stack top), relocated (absolute address)
    ; r3  = HEA, relocated (absolute address)
    ; r4  = CIP
    ; r5  = data section (passed in r2)
    ; r6  = STK, relocated (absolute address)
    ; r7  = FRM, relocated (absolute address)
    ; r8  = code
    ; r9  = code_size
    ; r10 = amx base (passed in r0)
    ; r14 = opcode list address (for token threading)
    ; r11 and r12 are scratch; r11 is used in the macro to fetch the next opcode
    ; and r12 is also used there in the case that packed opcodes are supported

    mov r10, r0                 ; r10 = AMX
    mov r5, r2                  ; r5 = data section
    ldr r0, [r10, #amxPRI]
    ldr r1, [r10, #amxALT]
    ldr r2, [r10, #amxSTP]
    ldr r3, [r10, #amxHEA]
    ldr r4, [r10, #amxCIP]
    ldr r6, [r10, #amxSTK]
    ldr r7, [r10, #amxFRM]
    ldr r8, [r10, #amxCode]
    ldr r9, [r10, #amxCodeSize]

    add r2, r2, r5              ; relocate STP (absolute address)
    add r3, r3, r5              ; relocate HEA
    add r6, r6, r5              ; relocate STK
    add r7, r7, r5              ; relocate FRM
    add r4, r4, r8              ; relocate CIP

    ldr r14, amx_opcodelist_addr

    ; start running
    NEXT

OP_NOP
    NEXT

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
    it  eq
    ldrbeq r0, [r12]
    teq r11, #2
    it  eq
    ldrheq r0, [r12]
    teq r11, #4
    it  eq
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

OP_STOR                         ; tested
    GETPARAM r11
    str r0, [r5, r11]
    NEXT

OP_STOR_S                       ; tested
    GETPARAM r11
    str r0, [r7, r11]
    NEXT

OP_SREF_S                       ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    str r0, [r5, r11]
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
    it  eq
    strbeq r0, [r12]
    teq r11, #2
    it  eq
    strheq r0, [r12]
    teq r11, #4
    it  eq
    streq r0, [r12]
    NEXT

OP_ALIGN_PRI                    ; tested
    GETPARAM r11
  IF :LNOT::DEF:BIG_ENDIAN
    rsbs r11, r11, #4           ; rsb = reverse subtract; r11 = #4 - param
    it  hi
    eorhi r0, r0, r11           ; PRI ^= (#4 - param), but only if (#4 - param) > 0
  ENDIF
    NEXT

OP_LCTRL
    GETPARAM r11                ; code of value to get
    teq r11, #0
    it  eq
    moveq r0, r8                ; 0 == code base address
    teq r11, #1
    it  eq
    moveq r0, r5                ; 1 == data base address
    teq r11, #2
    it  eq
    subeq r0, r3, r5            ; 2 == HEA (reverse relocated)
    teq r11, #3
    it  eq
    subeq r0, r2, r5            ; 3 == STP (reverse relocated)
    teq r11, #4
    it  eq
    subeq r0, r6, r5            ; 4 == STK (reverse relocated)
    teq r11, #5
    it  eq
    subeq r0, r7, r5            ; 5 == FRM (reverse relocated)
    teq r11, #6
    it  eq
    subeq r0, r4, r8            ; 6 == CIP (relative to code)
    NEXT

OP_SCTRL
    GETPARAM r11                ; code of value to get
    teq r11, #2
    it  eq
    addeq r3, r0, r5            ; 2 == HEA (relocated)
    teq r11, #4
    it  eq
    addeq r6, r0, r5            ; 4 == STK (reverse relocated)
    teq r11, #5
    it  eq
    addeq r7, r0, r5            ; 5 == FRM (reverse relocated)
    teq r11, #6
    it  eq
    addeq r4, r0, r8            ; 6 == CIP (relative to code)
    NEXT

OP_XCHG                         ; tested
    mov r11, r0
    mov r0, r1
    mov r1, r11
    NEXT

OP_PUSH_PRI                     ; tested
    mPUSH r0
    NEXT

OP_PUSH_ALT                     ; tested
    mPUSH r1
    NEXT

OP_PUSHR_PRI
    add r11, r0, r5             ; relocate PRI to DAT
    mPUSH r11
    NEXT

OP_POP_PRI
    mPOP r0
    NEXT

OP_POP_ALT                      ; tested
    mPOP r1
    NEXT

OP_PICK
    GETPARAM r11
    ldr r0, [r6, r11]
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
    mPUSH r7
    mov r7, r6                  ; FRM = stk
    CHKMARGIN r12
    NEXT

OP_RET
    mPOP r7                     ; pop FRM
    mPOP r4                     ; pop CIP (return address)
    ; verify return address (avoid stack/buffer overflow)
    cmp r4, r9                  ; return addres < code_end ?
    bhs err_memaccess           ; no, error
    ; test passed
    add r4, r4, r8              ; relocate
    NEXT

OP_RETN                         ; tested
    mPOP r7                     ; pop FRM
    mPOP r4                     ; pop CIP (return address)
    ; verify return address (avoid stack/buffer overflow)
    cmp r4, r9                  ; return addres < code_end ?
    bhs err_memaccess           ; no, error
    ; test passed
    add r4, r4, r8              ; relocate
    ldr r11, [r6], #4           ; read value at the stack (#args passed to func), add 4 to STK
    add r6, r6, r11             ; STK += #args (+ 4, added in the LDR instruction)
    NEXT

err_memaccess
    mov r11, #AMX_ERR_MEMACCESS
    b.w amx_exit

OP_CALL                         ; tested
    sub r11, r4, r8             ; reverse-relocate CIP
    add r11, r11, #4            ; r11 = address of next instruction
    mPUSH r11
    JUMPREL r11
    NEXT

OP_JUMP                         ; tested
    JUMPREL r11
    NEXT

OP_JZER                         ; tested
    cmp r0, #0
    JUMPRELCC r11, eq, ne         ; if PRI == 0, jump
    NEXT

OP_JNZ                          ; tested
    cmp r0, #0
    JUMPRELCC r11, ne, eq       ; if PRI != 0, jump
    NEXT

OP_SHL                          ; tested
    mov r0, r0, LSL r1          ; PRI = PRI << ALT
    NEXT

OP_SHR                          ; tested
    cmp r1, #0
    it hi
    movhi r0, r0, LSR r1        ; PRI = PRI >> ALT (but check that ALT > 0)
    NEXT

OP_SSHR                         ; tested
    cmp r1, #0
    it hi
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

OP_SMUL                         ; tested
    mov r11, r0                 ; copy r0
    mul r0, r11, r1             ; dest must be different from source registers
    NEXT

OP_SDIV                         ; tested
    teq r0, #0                  ; verify r0 (divisor)
    itt eq
    moveq r11, #AMX_ERR_DIVIDE  ; r0 == 0 -> set error code
    beq.w amx_exit              ; r0 == 0 -> jump to error-exit
    stmfd sp!, {r2 - r3, lr}    ; need two more scratch registers
    ; save input registers and create absolute values
    movs r2, r0
    it mi
    rsbmi r0, r0, #0            ; if r2 < 0, r0 = #0 - r0
    movs r3, r1
    it mi
    rsbmi r1, r1, #0            ; if r3 < 0, r1 = #0 - r1
    ; do the division
  IF :DEF:THUMB2
    udiv r11, r1, r0            ; r11 = r1 / r0
    mls r1, r0, r11, r1         ; r1 = r1 - (r0 * r11) = r1 % r0
    mov r0, r11                 ; r0 = r1 / r0
  ELSE
    bl  amx_div                 ; r0 = r1 / r0, r1 = r1 % r0
  ENDIF
    ; patch signs
    cmp r2, #0                  ; check sign of original value of divisor
    it mi
    rsbmi r1, r1, #0            ; sign(remainder) = sign(divisor)
    teq r2, r3                  ; check signs of dividend and divisor
    bpl op_div_done             ; sign(divident) == sign(divisor) -> done
    rsb r0, r0, #0              ; sign(quotient) = sign(divident) XOR sign(divisor)
    ; so the quotient is negative (or zero); if the remainder is non-zero,
    ; floor the quotient and adjust the remainder
    cmp r1, #0
    itt ne
    subne r0, r0, #1            ; remainder != 0 -> r0 = r0 - 1
    rsbne r1, r1, r2            ; remainder != 0 -> r1 = divisor - r1
op_div_done
    ldmfd sp!, {r2 - r3, lr}
    NEXT

OP_ADD                          ; tested
    add r0, r0, r1
    NEXT

OP_SUB                          ; tested
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
    ite eq
    moveq r0, #1
    movne r0, #0
    NEXT

OP_NEG                          ; tested
    rsb r0, r0, #0              ; PRI = #0 - PRI
    NEXT

OP_INVERT                       ; tested
    mvn r0, r0                  ; PRI = NOT PRI (all bits inverted)
    NEXT

OP_EQ
    cmp r0, r1
    ite eq
    moveq r0, #1
    movne r0, #0
    NEXT

OP_NEQ
    cmp r0, r1
    ite ne
    movne r0, #1
    moveq r0, #0
    NEXT

OP_SLESS                        ; tested
    cmp r0, r1
    ite lt
    movlt r0, #1
    movge r0, #0
    NEXT

OP_SLEQ                         ; tested
    cmp r0, r1
    ite le
    movle r0, #1
    movgt r0, #0
    NEXT

OP_SGRTR                        ; tested
    cmp r0, r1
    ite gt
    movgt r0, #1
    movle r0, #0
    NEXT

OP_SGEQ                         ; tested
    cmp r0, r1
    ite ge
    movge r0, #1
    movlt r0, #0
    NEXT

OP_INC_PRI
    add r0, r0, #1
    NEXT

OP_INC_ALT
    add r1, r1, #1
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

OP_DEC_I                        ; tested
    ldr r12, [r5, r0]
    sub r12, r12, #1
    str r12, [r5, r0]
    NEXT

OP_MOVS                         ; tested
    GETPARAM r11
movsentry
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
    itt ge
    ldrge r12, [r0], #4
    strge r12, [r1], #4
    bgt movs4loop
movs1loop
    subs r11, r11, #1
    itt ge
    ldrbge r12, [r0], #1
    strbge r12, [r1], #1
    bgt movs1loop
    ; restore PRI and ALT
    ldr r1, [sp], #4            ; restore PRI and ALT
    ldr r0, [sp], #4
    NEXT

OP_CMPS
    GETPARAM r11
cmpsentry
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
    str r14, [sp, #-4]!         ; need extra register
    add r11, r11, #1            ; restore r11
    str r1, [sp, #-4]!          ; save ALT
    add r0, r0, r5              ; relocate r0 and r1
    add r1, r1, r5
    mov r14, #0                 ; preset r14, in case r11 == 0
cmps4loop
    subs r11, r11, #4
    itt mi
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
    ldr r14, [sp], #4           ; restore r14
    NEXT

OP_FILL                         ; tested
    GETPARAM r11
fillentry
    add r12, r1, r5             ; r12 = relocated ALT
    VERIFYADDRESS r12           ; verify ALT
    add r12, r12, r11
    sub r12, r12, #1
    VERIFYADDRESS r12           ; verify ALT + size - 1
    ; dropped through tests
    add r12, r1, r5             ; r12 = relocated ALT again
fill4loop
    subs r11, r11, #4
    it ge
    strge r0, [r12], #4
    bgt fill4loop
    NEXT

OP_HALT                         ; tested
    ldr r11, [sp]               ; get "retval" pointer
    teq r11, #0
    it ne
    strne r0, [r11]             ; store PRI, but only if r11 != 0
    GETPARAM r11                ; parameter = return code from function
    b.w amx_exit

OP_BOUNDS                       ; tested
    GETPARAM r11
    cmp r0, r11
    itt hi
    movhi r11, #AMX_ERR_BOUNDS
    bhi.w amx_exit
    NEXT

OP_SYSREQ                       ; tested
    GETPARAM r0                 ; native function index in r0
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
    stmfd sp!, {r1 - r3, lr}    ; save some extra registers
    sub sp, sp, #8              ; reserve a cell on the stack for the return value, maintaining 8-byte alignment
    mov r1, r0                  ; 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 ; 1st arg = AMX
    mov r2, sp                  ; 3rd arg = address of return value
    mov r3, r6                  ; 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]; callback function pointer in r11
    CALL r11
    mov r11, r0                 ; get error return in r11
    ldr r0, [sp], #8            ; get return value, restore stack
    ldmfd sp!, {r1 - r3, lr}    ; restore registers
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    bne.w amx_exit              ; yes -> quit
    NEXT

OP_SWITCH                       ; tested
    str r14, [sp, #-4]!         ; need extra register
    ldr r11, [r4]               ; r11 = [CIP], relative offset to case-table
    add r11, r11, r4            ; r11 = direct address, OP_CASETBL opcode already skipped
    ldr r12, [r11]              ; r12 = number of case-table records
    ldr r14, [r11, #4]          ; preset CIP to "default" case (none-matched)
    add r4, r11, r14
op_switch_loop
    subs r12, r12, #1           ; decrement number to do; any left?
    bmi op_switch_done          ; no, quit (CIP already set to the default value)
    add r11, r11, #8            ; move to next record
    ldr r14, [r11]              ; get the case value
    cmp r0, r14                 ; case value identical to PRI ?
    bne op_switch_loop          ; no, continue
    ldr r14, [r11, #4]          ; yes, load matching CIP and exit loop
    add r4, r11, r14            ; r4 = address of case record + offset of the record
op_switch_done
    ldr r14, [sp], #4           ; restore r14
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
    stmfd sp!, {r0 - r3, r4, lr}; save some extra registers (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 ; 1st arg = AMX
    CALL r12
    mov r11, r0                 ; store exit code in r11 (r0 is restored)
    ldmfd sp!, {r0 - r3, r4, lr}; restore registers
    teq r11, #0                 ; debug hook returned error/abort code?
    bne.w amx_exit              ; yes -> quit
op_break_quit
    NEXT

OP_CASETBL
OP_CASETBL_OVL
    mov r11, #AMX_ERR_INVINSTR  ; these instructions are no longer supported
    b.w amx_exit

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
    stmfd sp!, {r1 - r3, lr}    ; save some extra registers
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r6                  ; 2nd arg = address in the AMX stack
    CALL r12
    ldmfd sp!, {r1 - r3, lr}    ; restore registers
    ldr r11, [r10, #amxError]   ; get error returned by native function
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    bne.w amx_exit              ; yes -> quit
    NEXT

OP_SYSREQ_ND                    ; tested
    GETPARAM r0                 ; address of native function in r0 (temporary)
    GETPARAM r12                ; get # parameters
    mPUSH r12                   ; push second parameter
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
    stmfd sp!, {r1 - r3, r4, r12, lr} ; save some extra registers (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r6                  ; 2nd arg = address in the AMX stack
    CALL r11
    ldmfd sp!, {r1 - r3, r4, r12, lr} ; restore registers
    add r6, r6, r12             ; remove # parameters from the AMX stack
    add r6, r6, #4              ; also remove the extra cell pushed
    ldr r11, [r10, #amxError]   ; get error returned by native function
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    bne.w amx_exit              ; yes -> quit
    NEXT


    ; overlay instructions
 IF :LNOT::DEF:AMX_NO_OVERLAY

OP_CALL_OVL
    add r11, r4, #4             ; r11 = address of next instruction (absolute)
    sub r11, r11, r8            ; r11 = relative address (to start of code segment)
    ldr r12, [r10, #amxOvlIndex]; r12 = overlay index
    add r11, r12, r11, LSL #16  ; r11 = (address << 16) + ovl_index
    mPUSH r11
    ldr r12, [r4]               ; r12 = [CIP] = new overlay index
    str r12, [r10, #amxOvlIndex]
    stmfd sp!, {r0 - r3, r4, lr}; save some extra registers (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r12                 ; 2nd arg = overlay index
    ldr r11, [r10, #amxOverlay] ; callback function pointer in r11
    CALL r11
    ldmfd sp!, {r0 - r3, r4, lr}; restore registers
    ldr r8, [r10, #amxCode]     ; r8 = code pointer (base)
    mov r4, r8                  ; CIP = code base
    NEXT

OP_RETN_OVL
    mPOP r7                     ; pop FRM
    mPOP r4                     ; pop relative CIP (return address) + overlay index
    ldr r11, [r6], #4           ; read value at the stack (#args passed to func), add 4 to STK
    add r6, r6, r11             ; STK += #args (+ 4, added in the LDR instruction)
    stmfd sp!, {r0 - r3, r4, lr}; save some extra registers (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 ; 1st arg = AMX
    mvn r1, #0                  ; r1 = 0xffffffff
    mov r1, r1, LSR #16         ; r1 = 0x0000ffff
    and r1, r4, r1              ; 2nd arg = overlay index
    str r1, [r10, #amxOvlIndex] ; store new overlay index too
    ldr r11, [r10, #amxOverlay] ; callback function pointer in r11
    CALL r11
    ldmfd sp!, {r0 - r3, r4, lr}; restore registers
    ldr r8, [r10, #amxCode]     ; r8 = code pointer (base)
    add r4, r8, r4, LSR #16     ; r4 = base address + relative address
    NEXT

OP_SWITCH_OVL
    str r14, [sp, #-4]!         ; need extra register
    ldr r11, [r4]               ; r11 = [CIP], relative offset to case-table
    add r11, r11, r4            ; r11 = direct address, OP_CASETBL opcode already skipped
    ldr r12, [r11]              ; r12 = number of case-table records
    ldr r4, [r11, #4]           ; preset ovl_index to "default" case (none-matched)
op_iswitch_loop
    subs r12, r12, #1           ; decrement number to do; any left?
    bmi op_iswitch_done         ; no, quit (CIP already set to the default value)
    add r11, r11, #8            ; move to next record
    ldr r14, [r11]              ; get the case value
    cmp r0, r14                 ; case value identical to PRI ?
    bne op_iswitch_loop         ; no, continue
    ldr r4, [r11, #4]           ; yes, load matching ovl_index and exit loop
op_iswitch_done
    ldr r14, [sp], #4           ; restore r14
    str r4, [r10, #amxOvlIndex] ; store new overlay index
    stmfd sp!, {r0 - r3, r4, lr}; save some extra registers (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 ; 1st arg = AMX
    mov r1, r4                  ; 2nd arg = overlay index
    ldr r11, [r10, #amxOverlay] ; callback function pointer in r11
    CALL r11
    ldmfd sp!, {r0 - r3, r4, lr}; restore registers
    ldr r8, [r10, #amxCode]     ; r8 = code pointer (base)
    mov r4, r8                  ; CIP = code base
    NEXT

 ENDIF  ; AMX_NO_OVERLAY


    ; supplemental instructions
 IF :LNOT::DEF:AMX_NO_MACRO_INSTR

OP_LIDX                         ; tested
    add r12, r1, r0, LSL #2     ; r12 = ALT + 4*PRI
    add r12, r12, r5            ; relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_LIDX_B
    GETPARAM r11
  IF :DEF:THUMB2
    mov r12, r0, LSL r11        ; r12 = PRI << param
    add r12, r12, r1            ; r12 = ALT + (PRI << param)
  ELSE
    add r12, r1, r0, LSL r11    ; r12 = ALT + (PRI << param)
  ENDIF
    add r12, r12, r5            ; relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_IDXADDR                      ; tested
    add r0, r1, r0, LSL #2      ; PRI = ALT + 4*PRI
    NEXT

OP_IDXADDR_B
    GETPARAM r11
  IF :DEF:THUMB2
    mov r0, r0, LSL r11         ; r0 = PRI << param
    add r0, r0, r1              ; PRI = ALT + (PRI << param)
  ELSE
    add r0, r1, r0, LSL r11     ; PRI = ALT + (PRI << param)
  ENDIF
    NEXT

OP_PUSH_C                       ; tested
    GETPARAM r11
    mPUSH r11
    NEXT

OP_PUSH
    GETPARAM r11
    ldr r11, [r5, r11]
    mPUSH r11
    NEXT

OP_PUSH_S                       ; tested
    GETPARAM r11
    ldr r11, [r7, r11]
    mPUSH r11
    NEXT

OP_PUSH_ADR                     ; tested
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    mPUSH r11
    NEXT

OP_PUSHR_C
    GETPARAM r11
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    NEXT

OP_PUSHR_S
    GETPARAM r11
    ldr r11, [r7, r11]
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    NEXT

OP_PUSHR_ADR
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    mPUSH r11
    NEXT

OP_JEQ                          ; tested
    cmp r0, r1
    JUMPRELCC r11, eq, ne       ; if PRI == ALT, jump
    NEXT

OP_JNEQ                         ; tested
    cmp r0, r1
    JUMPRELCC r11, ne, eq       ; if PRI != ALT, jump
    NEXT

OP_JSLESS                       ; tested
    cmp r0, r1
    JUMPRELCC r11, lt, ge       ; if PRI < ALT (signed), jump
    NEXT

OP_JSLEQ                        ; tested
    cmp r0, r1
    JUMPRELCC r11, le, gt       ; if PRI <= ALT (signed), jump
    NEXT

OP_JSGRTR                       ; tested
    cmp r0, r1
    JUMPRELCC r11, gt, le       ; if PRI > ALT (signed), jump
    NEXT

OP_JSGEQ                        ; tested
    cmp r0, r1
    JUMPRELCC r11, ge, lt       ; if PRI >= ALT (signed), jump
    NEXT

OP_SDIV_INV
    ; swap r0 and r1, then branch to the normal (signed) division case
    mov r11, r0
    mov r0, r1
    mov r1, r11
    b   OP_SDIV

OP_SUB_INV
    sub r0, r0, r1
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

OP_EQ_C_PRI                     ; tested
    GETPARAM r11
    cmp r0, r11
    ite eq
    moveq r0, #1
    movne r0, #0
    NEXT

OP_EQ_C_ALT
    GETPARAM r11
    cmp r1, r11
    ite eq
    moveq r0, #1
    movne r0, #0
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

OP_SYSREQ_N                     ; tested
    GETPARAM r0                 ; get native function index
    GETPARAM r12                ; get # parameters
    mPUSH r12                   ; push second parameter
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
    stmfd sp!, {r1 - r3, r12, lr} ; save some extra registers
    sub sp, sp, #4              ; reserve a cell on the stack for the return value
    mov r1, r0                  ; 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 ; 1st arg = AMX
    mov r2, sp                  ; 3rd arg = address of return value
    mov r3, r6                  ; 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]; callback function pointer in r11
    CALL r11
    mov r11, r0                 ; get error return in r11
    ldr r0, [sp], #4            ; get return value, restore ARM stack
    ldmfd sp!, {r1 - r3, r12, lr} ; restore registers
    add r6, r6, r12             ; remove # parameters from the AMX stack
    add r6, r6, #4              ; also remove the extra cell pushed
    teq r11, #AMX_ERR_NONE      ; callback hook returned error/abort code?
    bne.w amx_exit              ; yes -> quit
    NEXT

OP_PUSHM_C
    GETPARAM r12                ; r12 = parameter count
op_pushm_c_loop
    GETPARAM r11
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_c_loop
    NEXT

OP_PUSHM
    GETPARAM r12                ; r12 = parameter count
op_pushm_loop
    GETPARAM r11
    ldr r11, [r5, r11]
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_loop
    NEXT

OP_PUSHM_S
    GETPARAM r12                ; r12 = parameter count
op_pushm_s_loop
    GETPARAM r11
    ldr r11, [r7, r11]
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_s_loop
    NEXT

OP_PUSHM_ADR
    GETPARAM r12                ; r12 = parameter count
op_pushm_adr_loop
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_adr_loop
    NEXT

OP_PUSHRM_C
    GETPARAM r12                ; r12 = parameter count
op_pushrm_c_loop
    GETPARAM r11
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_c_loop
    NEXT

OP_PUSHRM_S
    GETPARAM r12                ; r12 = parameter count
op_pushrm_s_loop
    GETPARAM r11
    ldr r11, [r7, r11]
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_s_loop
    NEXT

OP_PUSHRM_ADR
    GETPARAM r12                ; r12 = parameter count
op_pushrm_adr_loop
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_adr_loop
    NEXT

OP_LOAD2                        ; tested
    GETPARAM r11
    ldr r0, [r5, r11]
    GETPARAM r11
    ldr r1, [r5, r11]
    NEXT

OP_LOAD2_S                      ; tested
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

 ENDIF  ; AMX_NO_MACRO_INSTR


    ; packed opcodes
 IF :LNOT::DEF:AMX_NO_PACKED_OPC

OP_LOAD_P_PRI
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r0, [r5, r11]
  ELSE
    ldr r0, [r5, r12, ASR #16]
  ENDIF
    NEXT

OP_LOAD_P_ALT
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r1, [r5, r11]
  ELSE
    ldr r1, [r5, r12, ASR #16]
  ENDIF
    NEXT

OP_LOAD_P_S_PRI
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r0, [r7, r11]
  ELSE
    ldr r0, [r7, r12, ASR #16]
  ENDIF
    NEXT

OP_LOAD_P_S_ALT
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r1, [r7, r11]
  ELSE
    ldr r1, [r7, r12, ASR #16]
  ENDIF
    NEXT

OP_LREF_P_S_PRI
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r11, [r7, r11]
  ELSE
    ldr r11, [r7, r12, ASR #16]
  ENDIF
    ldr r0, [r5, r11]
    NEXT

OP_LREF_P_S_ALT
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r11, [r7, r11]
  ELSE
    ldr r11, [r7, r12, ASR #16]
  ENDIF
    ldr r1, [r5, r11]
    NEXT

OP_LODB_P_I
    GETPARAM_P r11
    add r12, r0, r5             ; relocate PRI to absolute address
    VERIFYADDRESS r12
    teq r11, #1
    it eq
    ldrbeq r0, [r12]
    teq r11, #2
    it eq
    ldrheq r0, [r12]
    teq r11, #4
    it eq
    ldreq r0, [r12]
    NEXT

OP_CONST_P_PRI
    GETPARAM_P r0
    NEXT

OP_CONST_P_ALT
    GETPARAM_P r1
    NEXT

OP_ADDR_P_PRI
    GETPARAM_P r0
    add r0, r0, r7              ; add FRM
    sub r0, r0, r5              ; reverse relocate
    NEXT

OP_ADDR_P_ALT
    GETPARAM_P r1
    add r1, r1, r7              ; add FRM
    sub r1, r1, r5              ; reverse relocate
    NEXT

OP_STOR_P
  IF :DEF:THUMB2
    GETPARAM_P r11
    str r0, [r5, r11]
  ELSE
    str r0, [r5, r12, ASR #16]
  ENDIF
    NEXT

OP_STOR_P_S
  IF :DEF:THUMB2
    GETPARAM_P r11
    str r0, [r7, r11]
  ELSE
    str r0, [r7, r12, ASR #16]
  ENDIF
    NEXT

OP_SREF_P_S
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r11, [r7, r11]
  ELSE
    ldr r11, [r7, r12, ASR #16]
  ENDIF
    str r0, [r5, r11]
    NEXT

OP_STRB_P_I
    GETPARAM_P r11
    add r12, r1, r5             ; relocate ALT to absolute address
    VERIFYADDRESS r12
    teq r11, #1
    it eq
    strbeq r0, [r12]
    teq r11, #2
    it eq
    strheq r0, [r12]
    teq r11, #4
    it eq
    streq r0, [r12]
    NEXT

OP_LIDX_P_B
    GETPARAM_P r11
  IF :DEF:THUMB2
    mov r12, r0, LSL r11        ; r12 = PRI << param
    add r12, r12, r1            ; r12 = ALT + (PRI << param)
  ELSE
    add r12, r1, r0, LSL r11    ; r12 = ALT + (PRI << param)
  ENDIF
    add r12, r12, r5            ; relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

OP_IDXADDR_P_B
    GETPARAM_P r11
  IF :DEF:THUMB2
    mov r0, r0, LSL r11        ; r0 = PRI << param
    add r0, r0, r1             ; PRI = ALT + (PRI << param)
  ELSE
    add r0, r1, r0, LSL r11     ; PRI = ALT + (PRI << param)
  ENDIF
    NEXT

OP_ALIGN_P_PRI
    GETPARAM_P r11
  IF :LNOT::DEF:BIG_ENDIAN
    rsbs r11, r11, #4           ; rsb = reverse subtract; r11 = #4 - param
    it hi
    eorhi r0, r0, r11           ; PRI ^= (#4 - param), but only if (#4 - param) > 0
  ENDIF
    NEXT

OP_PUSH_P_C
    GETPARAM_P r11
    mPUSH r11
    NEXT

OP_PUSH_P
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r11, [r5, r11]
  ELSE
    ldr r11, [r5, r12, ASR #16]
  ENDIF
    mPUSH r11
    NEXT

OP_PUSH_P_S
  IF :DEF:THUMB2
    GETPARAM_P r11
    ldr r11, [r7, r11]
  ELSE
    ldr r11, [r7, r12, ASR #16]
  ENDIF
    mPUSH r11
    NEXT

OP_PUSH_P_ADR
    GETPARAM_P r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    mPUSH r11
    NEXT

OP_PUSHR_P_C
    GETPARAM_P r11
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    NEXT

OP_PUSHR_P_S
    GETPARAM_P r11
    ldr r11, [r7, r11]
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    NEXT

OP_PUSHR_P_ADR
    GETPARAM_P r11
    add r11, r11, r7            ; relocate to FRM
    mPUSH r11
    NEXT

OP_PUSHM_P_C
    GETPARAM_P r12              ; r12 = parameter count
op_pushm_p_c_loop
    GETPARAM r11
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_p_c_loop
    NEXT

OP_PUSHM_P
    GETPARAM_P r12              ; r12 = parameter count
op_pushm_p_loop
    GETPARAM r11
    ldr r11, [r5, r11]
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_p_loop
    NEXT

OP_PUSHM_P_S
    GETPARAM_P r12              ; r12 = parameter count
op_pushm_p_s_loop
    GETPARAM r11
    ldr r11, [r7, r11]
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_p_s_loop
    NEXT

OP_PUSHM_P_ADR
    GETPARAM_P r12              ; r12 = parameter count
op_pushm_p_adr_loop
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    sub r11, r11, r5            ; but relative to start of data section
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushm_p_adr_loop
    NEXT

OP_PUSHRM_P_C
    GETPARAM_P r12              ; r12 = parameter count
op_pushrm_p_c_loop
    GETPARAM r11
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_p_c_loop
    NEXT

OP_PUSHRM_P_S
    GETPARAM_P r12              ; r12 = parameter count
op_pushrm_p_s_loop
    GETPARAM r11
    ldr r11, [r7, r11]
    add r11, r11, r5            ; relocate to DAT
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_p_s_loop
    NEXT

OP_PUSHRM_P_ADR
    GETPARAM_P r12              ; r12 = parameter count
op_pushrm_p_adr_loop
    GETPARAM r11
    add r11, r11, r7            ; relocate to FRM
    mPUSH r11
    subs r12, r12, #1
    bgt op_pushrm_p_adr_loop
    NEXT

OP_STACK_P
    GETPARAM_P r11
    sub r1, r6, r5              ; ALT = STK, reverse-relocated
    add r6, r6, r11             ; STK += param
    CHKMARGIN r12
    CHKSTACK
    NEXT

OP_HEAP_P
    GETPARAM_P r11
    sub r1, r3, r5              ; ALT = HEA, reverse-relocated
    add r3, r3, r11
    CHKMARGIN r12
    CHKHEAP r12
    NEXT

OP_SHL_P_C_PRI
    GETPARAM_P r11
    mov r0, r0, LSL r11         ; PRI = PRI << param
    NEXT

OP_SHL_P_C_ALT
    GETPARAM_P r11
    mov r1, r1, LSL r11         ; ALT = ALT << param
    NEXT

OP_ADD_P_C
    GETPARAM_P r11
    add r0, r0, r11             ; PRI += param
    NEXT

OP_SMUL_P_C
    GETPARAM_P r11
    mov r12, r0
    mul r0, r11, r12            ; PRI *= param
    NEXT

OP_ZERO_P
    GETPARAM_P r11
    mov r12, #0
    str r12, [r5, r11]
    NEXT

OP_ZERO_P_S
    GETPARAM_P r11
    mov r12, #0
    str r12, [r7, r11]
    NEXT

OP_EQ_P_C_PRI
    GETPARAM_P r11
    cmp r0, r11
    ite eq
    moveq r0, #1
    movne r0, #0
    NEXT

OP_EQ_P_C_ALT
    GETPARAM_P r11
    cmp r1, r11
    ite eq
    moveq r0, #1
    movne r0, #0
    NEXT

OP_INC_P
    GETPARAM_P r11
    ldr r12, [r5, r11]
    add r12, r12, #1
    str r12, [r5, r11]
    NEXT

OP_INC_P_S
    GETPARAM_P r11
    ldr r12, [r7, r11]
    add r12, r12, #1
    str r12, [r7, r11]
    NEXT

OP_DEC_P
    GETPARAM_P r11
    ldr r12, [r5, r11]
    sub r12, r12, #1
    str r12, [r5, r11]
    NEXT

OP_DEC_P_S
    GETPARAM_P r11
    ldr r12, [r7, r11]
    sub r12, r12, #1
    str r12, [r7, r11]
    NEXT

OP_MOVS_P
    GETPARAM_P r11
    b movsentry

OP_CMPS_P
    GETPARAM_P r11
    b cmpsentry

OP_FILL_P
    GETPARAM_P r11
    b fillentry

OP_HALT_P
    ldr r11, [sp]               ; get "retval" pointer
    teq r11, #0
    it ne
    strne r0, [r11]             ; store PRI, but only if r11 != 0
    GETPARAM_P r11              ; parameter = return code from function
    b   amx_exit

OP_BOUNDS_P
    GETPARAM_P r11
    cmp r0, r11
    itt hi
    movhi r11, #AMX_ERR_BOUNDS
    bhi amx_exit
    NEXT

 ENDIF  ; AMX_NO_PACKED_OPC


amx_exit                        ; assume r11 already set (to exit code)
    ; reverse relocate registers
    sub r3, r3, r5              ; reverse-relocate HEA
    sub r6, r6, r5              ; reverse-relocate STK
    sub r7, r7, r5              ; reverse-relocate FRM
    sub r4, r4, r8              ; reverse-relocate CIP

    ; store stack and heap state AMX state
    str r0, [r10, #amxPRI]      ; PRI
    str r1, [r10, #amxALT]      ; ALT
    str r3, [r10, #amxHEA]      ; HEA
    str r4, [r10, #amxCIP]      ; CIP
    str r6, [r10, #amxSTK]      ; STK
    str r7, [r10, #amxFRM]      ; FRM

    mov r0, r11                 ; put return value in r0
    add sp, sp, #8              ; drop register for the return value
    ldmfd sp!, {r4 - r12, lr}
    bx  lr
; amx_exec_run

 IF :LNOT::DEF:THUMB2
    ALIGN   2
    EXPORT  amx_div
amx_div
    ; expects divident in r1, divisor in r0
    ; on exit quotient is in r0, remainder in r1
    ; r11 and r12 are scratch; r12 is temporary result
    ; unsigned division only; when r0 (divisor) is zero, the function returns
    ; with all registers unchanged
    teq r0, #0                  ; verify r0
    moveq pc, lr                ; just for security
    ; drop-through (divisor is not zero)
    mov r11, #1
amx_div1
    cmp r0, #0x80000000         ; shift divisor left until top bit set
    cmpcc r0, r1                ; ...or divisor>divident
    movcc r0, r0, LSL #1        ; shift divisor left if required
    movcc r11, r11, LSL #1      ; shift r11 left if required
    bcc amx_div1                ; repeat whilst more shifting required
    mov r12, #0                 ; used to store result (temporary)
amx_div2
    cmp r1, r0                  ; test for possible subtraction
    subcs r1, r1, r0            ; subtract if divident>divisor
    addcs r12, r12, r11         ; put relevant bit into result
    movs  r11, r11, LSR #1      ; shift control bit
    movne r0, r0, LSR #1        ; halve unless finished
    bne amx_div2                ; loop if there is more to do
    ; remainder r1, result in r12
    mov r0, r12                 ; quotient now in r0
    mov pc, lr
; amx_div

 ENDIF ; THUMB2
    END
