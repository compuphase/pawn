@   amxexec_cortexm0_gas.s  Abstract Machine for the "Pawn" language
@
@   This file assembles with GNU's AS (GAS). It uses the Cortex M0 subset of
@   the ARM Thumb-2 instructions. It can be assembled for Big Endian
@   environments, by defining the symbol BIG_ENDIAN; the default configuration
@   is Little Endian.
@
@   You will need to compile the standard amx.c file with the macro
@   AMX_ASM defined.
@
@   The calling convention conforms to the ARM Architecture Procedure
@   Call Standard (AAPCS). This applies both to the function amx_exec_run
@   implemented in this file as to the debug hook function, callback hook
@   function and any native functions called directly from the abstract
@   machine.
@
@
@   Copyright (c) CompuPhase, 2015-2024
@
@   Licensed under the Apache License, Version 2.0 (the "License"); you may not
@   use this file except in compliance with the License. You may obtain a copy
@   of the License at
@
@       http://www.apache.org/licenses/LICENSE-2.0
@
@   Unless required by applicable law or agreed to in writing, software
@   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
@   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
@   License for the specific language governing permissions and limitations
@   under the License.
@
@   $Id: amxexec_cortexm0_gas.S 7115 2024-02-26 21:45:03Z thiadmer $

    .file   "amxexec_cortexm0_gas.S"
    .syntax unified
    .cpu cortex-m0
    .thumb


@ Copy GCC preprocessor definitions to assembler equates, so that the assembler
@ file can be built with 'gcc' as well as 'as'.
#if defined AMX_DONT_RELOCATE
  .equ _DONT_RELOCATE, 1
#else
  .ifdef AMX_DONT_RELOCATE
  .equ _DONT_RELOCATE, 1
  .endif
#endif
#if defined AMX_NO_OVERLAY
  .equ _NO_OVERLAY, 1
#else
  .ifdef AMX_NO_OVERLAY
  .equ _NO_OVERLAY, 1
  .endif
#endif
#if defined AMX_NO_MACRO_INSTR
  .equ _NO_MACRO_INSTR, 1
#else
  .ifdef AMX_NO_MACRO_INSTR
  .equ _NO_MACRO_INSTR, 1
  .endif
#endif
#if defined AMX_NO_PACKED_OPC
  .equ _NO_PACKED_OPC, 1
#else
  .ifdef AMX_NO_PACKED_OPC
  .equ _NO_PACKED_OPC, 1
  .endif
#endif
#if defined AMX_TOKENTHREADING
  .equ _TOKENTHREADING, 1
#else
  .ifdef AMX_TOKENTHREADING
  .equ _TOKENTHREADING, 1
  .endif
#endif
#if defined BIG_ENDIAN
  .equ _BIGENDIAN, 1
#endif

.ifndef _NO_PACKED_OPC
  .ifndef _TOKENTHREADING
    .equ _TOKENTHREADING, 1 @ packed opcodes require token threading
  .endif
.endif
.ifdef _DONT_RELOCATE
  .ifndef _TOKENTHREADING
    .equ _TOKENTHREADING, 1 @ when opcode patching is disabled, direct threading is impossible
  .endif
.endif

.equ    AMX_ERR_NONE,       0
.equ    AMX_ERR_EXIT,       1   @ forced exit
.equ    AMX_ERR_ASSERT,     2   @ assertion failed
.equ    AMX_ERR_STACKERR,   3   @ stack/heap collision
.equ    AMX_ERR_BOUNDS,     4   @ index out of bounds
.equ    AMX_ERR_MEMACCESS,  5   @ invalid memory access
.equ    AMX_ERR_INVINSTR,   6   @ invalid instruction
.equ    AMX_ERR_STACKLOW,   7   @ stack underflow
.equ    AMX_ERR_HEAPLOW,    8   @ heap underflow
.equ    AMX_ERR_CALLBACK,   9   @ no callback, or invalid callback
.equ    AMX_ERR_NATIVE,    10   @ native function failed
.equ    AMX_ERR_DIVIDE,    11   @ divide by zero
.equ    AMX_ERR_SLEEP,     12   @ go into sleepmode - code can be restarted
.equ    AMX_ERR_INVSTATE,  13   @ invalid state for this access

.equ    amxBase,        0       @ points to the AMX header, perhaps followed by P-code and data
.equ    amxCode,        4       @ points to P-code block, possibly in ROM or in an overlay pool
.equ    amxData,        8       @ points to separate data+stack+heap, may be NULL
.equ    amxCallback,    12
.equ    amxDebug,       16      @ debug callback
.equ    amxOverlay,     20      @ overlay callback
.equ    amxCIP,         24      @ instruction pointer: relative to base + amxhdr->cod
.equ    amxFRM,         28      @ stack frame base: relative to base + amxhdr->dat
.equ    amxHEA,         32      @ top of the heap: relative to base + amxhdr->dat
.equ    amxHLW,         36      @ bottom of the heap: relative to base + amxhdr->dat
.equ    amxSTK,         40      @ stack pointer: relative to base + amxhdr->dat
.equ    amxSTP,         44      @ top of the stack: relative to base + amxhdr->dat
.equ    amxFlags,       48      @ current status, see amx_Flags()
.equ    amxUserTags,    52      @ user data, AMX_USERNUM fields
.equ    amxUserData,    68      @ user data
.equ    amxError,       84      @ native functions that raise an error
.equ    amxParamCount,  88      @ passing parameters requires a "count" field
.equ    amxPRI,         92      @ the sleep opcode needs to store the full AMX status
.equ    amxALT,         96
.equ    amx_reset_stk,  100
.equ    amx_reset_hea,  104
.equ    amx_sysreq_d,   108     @ relocated address/value for the SYSREQ.D opcode
.equ    amxOvlIndex,    112
.equ    amxCodeSize,    116     @ memory size of the overlay or of the native code
.equ    amx_reloc_size, 120     @ (JIT) required temporary buffer for relocations


    .section    .rodata
    .align  2
    .global amx_opcodelist
    .type   amx_opcodelist, %object
amx_opcodelist:
    @ core set
    .word   .OP_NOP + 1
    .word   .OP_LOAD_PRI + 1
    .word   .OP_LOAD_ALT + 1
    .word   .OP_LOAD_S_PRI + 1
    .word   .OP_LOAD_S_ALT + 1
    .word   .OP_LREF_S_PRI + 1
    .word   .OP_LREF_S_ALT + 1
    .word   .OP_LOAD_I + 1
    .word   .OP_LODB_I + 1
    .word   .OP_CONST_PRI + 1
    .word   .OP_CONST_ALT + 1
    .word   .OP_ADDR_PRI + 1
    .word   .OP_ADDR_ALT + 1
    .word   .OP_STOR + 1
    .word   .OP_STOR_S + 1
    .word   .OP_SREF_S + 1
    .word   .OP_STOR_I + 1
    .word   .OP_STRB_I + 1
    .word   .OP_ALIGN_PRI + 1
    .word   .OP_LCTRL + 1
    .word   .OP_SCTRL + 1
    .word   .OP_XCHG + 1
    .word   .OP_PUSH_PRI + 1
    .word   .OP_PUSH_ALT + 1
    .word   .OP_PUSHR_PRI + 1
    .word   .OP_POP_PRI + 1
    .word   .OP_POP_ALT + 1
    .word   .OP_PICK + 1
    .word   .OP_STACK + 1
    .word   .OP_HEAP + 1
    .word   .OP_PROC + 1
    .word   .OP_RET + 1
    .word   .OP_RETN + 1
    .word   .OP_CALL + 1
    .word   .OP_JUMP + 1
    .word   .OP_JZER + 1
    .word   .OP_JNZ + 1
    .word   .OP_SHL + 1
    .word   .OP_SHR + 1
    .word   .OP_SSHR + 1
    .word   .OP_SHL_C_PRI + 1
    .word   .OP_SHL_C_ALT + 1
    .word   .OP_SMUL + 1
    .word   .OP_SDIV + 1
    .word   .OP_ADD + 1
    .word   .OP_SUB + 1
    .word   .OP_AND + 1
    .word   .OP_OR + 1
    .word   .OP_XOR + 1
    .word   .OP_NOT + 1
    .word   .OP_NEG + 1
    .word   .OP_INVERT + 1
    .word   .OP_EQ + 1
    .word   .OP_NEQ + 1
    .word   .OP_SLESS + 1
    .word   .OP_SLEQ + 1
    .word   .OP_SGRTR + 1
    .word   .OP_SGEQ + 1
    .word   .OP_INC_PRI + 1
    .word   .OP_INC_ALT + 1
    .word   .OP_INC_I + 1
    .word   .OP_DEC_PRI + 1
    .word   .OP_DEC_ALT + 1
    .word   .OP_DEC_I + 1
    .word   .OP_MOVS + 1
    .word   .OP_CMPS + 1
    .word   .OP_FILL + 1
    .word   .OP_HALT + 1
    .word   .OP_BOUNDS + 1
    .word   .OP_SYSREQ + 1
    .word   .OP_SWITCH + 1
    .word   .OP_SWAP_PRI + 1
    .word   .OP_SWAP_ALT + 1
    .word   .OP_BREAK + 1
    .word   .OP_CASETBL + 1
    @ patched instructions
    .word   .OP_SYSREQ_D + 1
    .word   .OP_SYSREQ_ND + 1
    @ overlay instructions
    .word   .OP_CALL_OVL + 1
    .word   .OP_RETN_OVL + 1
    .word   .OP_SWITCH_OVL + 1
    .word   .OP_CASETBL_OVL + 1
    @ supplemental instructions
.ifndef _NO_MACRO_INSTR
    .word   .OP_LIDX + 1
    .word   .OP_LIDX_B + 1
    .word   .OP_IDXADDR + 1
    .word   .OP_IDXADDR_B + 1
    .word   .OP_PUSH_C + 1
    .word   .OP_PUSH + 1
    .word   .OP_PUSH_S + 1
    .word   .OP_PUSH_ADR + 1
    .word   .OP_PUSHR_C + 1
    .word   .OP_PUSHR_S + 1
    .word   .OP_PUSHR_ADR + 1
    .word   .OP_JEQ + 1
    .word   .OP_JNEQ + 1
    .word   .OP_JSLESS + 1
    .word   .OP_JSLEQ + 1
    .word   .OP_JSGRTR + 1
    .word   .OP_JSGEQ + 1
    .word   .OP_SDIV_INV + 1
    .word   .OP_SUB_INV + 1
    .word   .OP_ADD_C + 1
    .word   .OP_SMUL_C + 1
    .word   .OP_ZERO_PRI + 1
    .word   .OP_ZERO_ALT + 1
    .word   .OP_ZERO + 1
    .word   .OP_ZERO_S + 1
    .word   .OP_EQ_C_PRI + 1
    .word   .OP_EQ_C_ALT + 1
    .word   .OP_INC + 1
    .word   .OP_INC_S + 1
    .word   .OP_DEC + 1
    .word   .OP_DEC_S + 1
    .word   .OP_SYSREQ_N + 1
    .word   .OP_PUSHM_C + 1
    .word   .OP_PUSHM + 1
    .word   .OP_PUSHM_S + 1
    .word   .OP_PUSHM_ADR + 1
    .word   .OP_PUSHRM_C + 1
    .word   .OP_PUSHRM_S + 1
    .word   .OP_PUSHRM_ADR + 1
    .word   .OP_LOAD2 + 1
    .word   .OP_LOAD2_S + 1
    .word   .OP_CONST + 1
    .word   .OP_CONST_S + 1
.endif  @ _NO_MACRO_INSTR
    @ packed opcodes
.ifndef _NO_PACKED_OPC
    .word   .OP_LOAD_P_PRI + 1
    .word   .OP_LOAD_P_ALT + 1
    .word   .OP_LOAD_P_S_PRI + 1
    .word   .OP_LOAD_P_S_ALT + 1
    .word   .OP_LREF_P_S_PRI + 1
    .word   .OP_LREF_P_S_ALT + 1
    .word   .OP_LODB_P_I + 1
    .word   .OP_CONST_P_PRI + 1
    .word   .OP_CONST_P_ALT + 1
    .word   .OP_ADDR_P_PRI + 1
    .word   .OP_ADDR_P_ALT + 1
    .word   .OP_STOR_P + 1
    .word   .OP_STOR_P_S + 1
    .word   .OP_SREF_P_S + 1
    .word   .OP_STRB_P_I + 1
    .word   .OP_LIDX_P_B + 1
    .word   .OP_IDXADDR_P_B + 1
    .word   .OP_ALIGN_P_PRI + 1
    .word   .OP_PUSH_P_C + 1
    .word   .OP_PUSH_P + 1
    .word   .OP_PUSH_P_S + 1
    .word   .OP_PUSH_P_ADR + 1
    .word   .OP_PUSHR_P_C + 1
    .word   .OP_PUSHR_P_S + 1
    .word   .OP_PUSHR_P_ADR + 1
    .word   .OP_PUSHM_P_C + 1
    .word   .OP_PUSHM_P + 1
    .word   .OP_PUSHM_P_S + 1
    .word   .OP_PUSHM_P_ADR + 1
    .word   .OP_PUSHRM_P_C + 1
    .word   .OP_PUSHRM_P_S + 1
    .word   .OP_PUSHRM_P_ADR + 1
    .word   .OP_STACK_P + 1
    .word   .OP_HEAP_P + 1
    .word   .OP_SHL_P_C_PRI + 1
    .word   .OP_SHL_P_C_ALT + 1
    .word   .OP_ADD_P_C + 1
    .word   .OP_SMUL_P_C + 1
    .word   .OP_ZERO_P + 1
    .word   .OP_ZERO_P_S + 1
    .word   .OP_EQ_P_C_PRI + 1
    .word   .OP_EQ_P_C_ALT + 1
    .word   .OP_INC_P + 1
    .word   .OP_INC_P_S + 1
    .word   .OP_DEC_P + 1
    .word   .OP_DEC_P_S + 1
    .word   .OP_MOVS_P + 1
    .word   .OP_CMPS_P + 1
    .word   .OP_FILL_P + 1
    .word   .OP_HALT_P + 1
    .word   .OP_BOUNDS_P + 1
.endif  @ _NO_PACKED_OPC
.equ    opcodelist_size, .-amx_opcodelist


.macro NEXT
  .ifdef _TOKENTHREADING
    .ifdef _NO_PACKED_OPC
      ldmia r4!, {r2}           @ get opcode, increment CIP
    .else
      ldmia r4!, {r3}           @ get opcode + parameter (in r3), increment CIP
      uxtb r2, r3               @ keep only the opcode in r2, r3 holds the parameter (plus opcode)
    .endif
    lsls r2, r2, #2             @ multiply by 4 (index into opcode table)
    add r2, r14                 @ add base of opcode table
    ldr r2, [r2]                @ read jump address from opcode table
  .else
    .ifndef _NO_PACKED_OPC
      .err                      @ opcode packing requires token threading
    .endif
    ldmia r4!, {r2}             @ direct threading -> instruction opcodes have been patched to jump addresses
  .endif
  bx r2
.endm

.macro GETPARAM rx
    ldmia r4!, {\rx}            @ \rx = [CIP], CIP += 4
.endm

.macro GETPARAM_P rx            @ the opcode/parameter pack should be in r3
    asrs \rx, r3, #16           @ \rx = r3 >> 16 (signed shift)
.endm

.macro JUMPREL rtmp, cc=al      @ \rtmp = temp register to use, \cc = condition
  .if \cc==al
    ldr \rtmp, [r4]             @ \rtmp = [CIP]
    subs r4, r4, #4             @ CIP -= 4 (restore CIP to start of instruction)
    adds r4, r4, \rtmp          @ CIP += [CIP]
  .else
    b\cc 9f
    adds r4, r4, #4             @ no jump -> skip param (jump address)
    b 8f
  9:
    ldr \rtmp, [r4]             @ \rtmp = [CIP]
    subs r4, r4, #4             @ CIP -= 4 (restore CIP to start of instruction)
    adds r4, r4, \rtmp          @ CIP += [CIP]
  8:
  .endif
.endm

.macro mPUSH rx
    subs r6, r6, #4
    str \rx, [r6]               @ STK -= 4, [STK] = \rx
.endm

.macro mPOP rx
    ldmia r6!, {\rx}            @ \rx = [STK], STK += 4
.endm

.macro CHKMARGIN rtmp           @ \rtmp = temp register to use
    mov \rtmp, r12              @ HEA
    adds \rtmp, \rtmp, #64      @ 64 = 16 cells
    cmp \rtmp, r6               @ HEA + 16*cell <= STK ?
    bls 9f                      @ yes -> done
    movs r2, #AMX_ERR_STACKERR  @ no -> error
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code
  9:
.endm

.macro CHKSTACK rtmp            @ \rtmp = temp register to use
    mov \rtmp, r11
    cmp r6, \rtmp               @ STK <= STP ?
    bls 9f                      @ yes -> done
    movs r2, #AMX_ERR_STACKLOW  @ no -> error
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code
  9:
.endm

.macro CHKHEAP rtmp             @ \rtmp = temp register to use
    mov \rtmp, r10
    ldr \rtmp, [\rtmp, #amxHLW]
    cmp r12, \rtmp              @ HEA >= HLW ?
    bhs 9f                      @ yes -> done
    movs r2, #AMX_ERR_HEAPLOW   @ no -> error
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code
  9:
.endm

.macro VERIFYADDRESS rx
    cmp \rx, r11                @ \rx >= STP ?
    blo 9f                      @ no, proceed with next test
    bl .err_memaccess           @ yes -> error (use BL instruction for longer jump range, r14 is unused by exit code)
  9:
    @ One might want to relax the test and remove the three instructions below.
    @ If a register points into the "free" area between the stack and the heap,
    @ it does not breach the sandbox.
    cmp r12, \rx                @ HEA > \rx ?
    bhi 8f                      @ yes -> no problem
    cmp \rx, r6                 @ no -> then: \rx < STK ?
    bhs 8f                      @ no -> no problem
    bl .err_memaccess           @ yes -> HEA <= \rx && \rx < STK (invalid area) -> error
  8:
.endm


@ ================================================================

    .text

@ ----------------------------------------------------------------
@ cell amx_exec_list(AMX *amx, cell **opcodelist,int *numopcodes)
@                         r0          r1              r2
@ ----------------------------------------------------------------

    .align  2
    .global amx_exec_list
    .thumb_func
    .type   amx_exec_list, %function
amx_exec_list:
    ldr r0, amx_opcodelist_addr     @ r0 = opcode table address
    str r0, [r1]                    @ store in parameter 'opcodelist'
    ldr r0, amx_opcodelist_size
    lsrs r0, r0, #2                 @ number of opcodes, not bytes
    str r0, [r2]                    @ store in parameter 'numopcodes'
    movs r0, #0                     @ no specific return value)
    mov pc, lr
    .size   amx_exec_list, .-amx_exec_list


@ ----------------------------------------------------------------
@ cell amx_exec_run(AMX *amx, cell *retval, char *data)
@                        r0         r1            r2
@ ----------------------------------------------------------------

    .align 2
    .global amx_exec_run
    .thumb_func
    .type   amx_exec_run, %function
amx_exec_run:
    @ save non-scratch registers
    push {r4 - r7, lr}
    mov r3, r8
    mov r4, r9
    mov r5, r10
    mov r6, r11
    mov r7, r12
    push {r3 - r7}

    @ save the register that holds the address for the return value
    @ we only need this at the point of returning, so it would be
    @ a waste to keep it in a register
    sub sp, sp, #8              @ decrement by 8 to keep 8-byte alignment of sp
    str r1, [sp]                @ save pointer to return value

    @ set up the registers
    @ r0  = PRI
    @ r1  = ALT
    @ r2  = scratch, destroyed by NEXT
    @ r3  = scratch, contains packed instruction after NEXT
    @ r4  = CIP
    @ r5  = data section (passed in r2)
    @ r6  = STK, relocated (absolute address)
    @ r7  = FRM, relocated (absolute address)
    @ r8  = code address
    @ r9  = code_size
    @ r10 = amx base (passed in r0)
    @ r11 = STP (stack top), relocated (absolute address)
    @ r12 = HEA, relocated (absolute address)
    @ r13 = sp (not used by AMX)
    @ r14 = opcode list address (for token threading)

    mov r10, r0                 @ r10 = AMX
    mov r5, r2                  @ r5 = data section
    mov r2, r10                 @ copy r10 to r2 (high resgister access)
    ldr r0, [r2, #amxPRI]
    ldr r1, [r2, #amxALT]
    ldr r4, [r2, #amxCIP]
    ldr r6, [r2, #amxSTK]
    ldr r7, [r2, #amxFRM]
    ldr r3, [r2, #amxCode]
    mov r8, r3
    ldr r3, [r2, #amxCodeSize]
    mov r9, r3
    ldr r3, [r2, #amxSTP]
    mov r11, r3
    ldr r3, [r2, #amxHEA]
    mov r12, r3

    add r6, r6, r5              @ relocate STK
    add r7, r7, r5              @ relocate FRM
    add r4, r4, r8              @ relocate CIP
    add r11, r11, r5            @ relocate STP (absolute address)
    add r12, r12, r5            @ relocate HEA

    ldr r2, amx_opcodelist_addr
    mov r14, r2                 @ N.B. r14 is an alias for lr

    @ start running
    NEXT

    @ move pointer to opcode list here (as a kind of literal pool), because LDR needs a positive offset
    .align 2
amx_opcodelist_addr:
    .word   amx_opcodelist
amx_opcodelist_size:
    .word   opcodelist_size

.OP_NOP:
    NEXT

.OP_LOAD_PRI:       @ tested
    GETPARAM r2
    ldr r0, [r5, r2]
    NEXT

.OP_LOAD_ALT:       @ tested
    GETPARAM r2
    ldr r1, [r5, r2]
    NEXT

.OP_LOAD_S_PRI:     @ tested
    GETPARAM r2
    ldr r0, [r7, r2]
    NEXT

.OP_LOAD_S_ALT:     @ tested
    GETPARAM r2
    ldr r1, [r7, r2]
    NEXT

.OP_LREF_S_PRI:     @ tested
    GETPARAM r2
    ldr r2, [r7, r2]
    ldr r0, [r5, r2]
    NEXT

.OP_LREF_S_ALT:     @ tested
    GETPARAM r2
    ldr r2, [r7, r2]
    ldr r1, [r5, r2]
    NEXT

.OP_LOAD_I:         @ tested
    adds r3, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r3
    ldr r0, [r3]
    NEXT

.OP_LODB_I:         @ tested
    adds r3, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r3
    GETPARAM r2
    cmp r2, #1
    bne 1f
    ldrb r0, [r3]
    b 4f
  1:
    cmp r2, #2
    bne 2f
    ldrh r0, [r3]
    b 4f
  2:
    cmp r2, #4
    bne 4f
    ldr r0, [r3]
  4:
    NEXT

.OP_CONST_PRI:      @ tested
    GETPARAM r0
    NEXT

.OP_CONST_ALT:      @ tested
    GETPARAM r1
    NEXT

.OP_ADDR_PRI:       @ tested
    GETPARAM r0
    add r0, r0, r7              @ add FRM
    subs r0, r0, r5             @ reverse relocate
    NEXT

.OP_ADDR_ALT:       @ tested
    GETPARAM r1
    add r1, r1, r7              @ add FRM
    subs r1, r1, r5             @ reverse relocate
    NEXT

.OP_STOR:           @ tested
    GETPARAM r2
    str r0, [r5, r2]
    NEXT

.OP_STOR_S:         @ tested
    GETPARAM r2
    str r0, [r7, r2]
    NEXT

.OP_SREF_S:         @ tested
    GETPARAM r2
    ldr r2, [r7, r2]
    str r0, [r5, r2]
    NEXT

.OP_STOR_I:         @ tested
    adds r3, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r3
    str r0, [r3]
    NEXT

.OP_STRB_I:         @ tested
    adds r3, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r3
    GETPARAM r2
    cmp r2, #1
    bne 1f
    strb r0, [r3]
    b 4f
  1:
    cmp r2, #2
    bne 2f
    strh r0, [r3]
    b 4f
  2:
    cmp r2, #4
    bne 4f
    str r0, [r3]
  4:
    NEXT

.OP_ALIGN_PRI:      @ tested
    GETPARAM r2
.ifndef _BIGENDIAN
    cmp r2, #4                  @ param < cell size ?
    bhi 1f                      @ no -> skip
    movs r3, #4
    subs r2, r3, r2             @ r2 = 4 - param
    eors r0, r0, r2             @ PRI ^= (4 - param), but only if param < 4
  1:
.endif
    NEXT

.OP_LCTRL:
    GETPARAM r2                 @ code of value to get
    cmp r2, #0
    bne 0f
    mov r0, r8                  @ 0 == code base address
  0:
    cmp r2, #1
    bne 1f
    mov r0, r5                  @ 1 == data base address
  1:
    cmp r2, #2
    bne 2f
    mov r2, r12
    subs r0, r2, r5             @ 2 == HEA (reverse relocated)
  2:
    cmp r2, #3
    bne 3f
    mov r2, r11
    subs r0, r2, r5             @ 3 == STP (reverse relocated)
  3:
    cmp r2, #4
    bne 4f
    subs r0, r6, r5             @ 4 == STK (reverse relocated)
  4:
    cmp r2, #5
    bne 5f
    subs r0, r7, r5             @ 5 == FRM (reverse relocated)
  5:
    cmp r2, #6
    bne 6f
    mov r2, r8
    subs r0, r4, r2             @ 6 == CIP (relative to code)
  6:
    NEXT

.OP_SCTRL:
    GETPARAM r2                 @ code of value to get
    cmp r2, #2
    bne 2f
    adds r2, r0, r5             @ 2 == HEA (relocated)
    mov r12, r2
  2:
    cmp r2, #4
    bne 4f
    adds r6, r0, r5             @ 4 == STK (reverse relocated)
  4:
    cmp r2, #5
    bne 5f
    adds r7, r0, r5             @ 5 == FRM (reverse relocated)
  5:
    cmp r2, #6
    bne 6f
    mov r2, r8
    adds r4, r0, r2             @ 6 == CIP (relative to code)
  6:
    NEXT

.OP_XCHG:           @ tested
    mov r2, r0
    mov r0, r1
    mov r1, r2
    NEXT

.OP_PUSH_PRI:       @ tested
    mPUSH r0
    NEXT

.OP_PUSH_ALT:       @ tested
    mPUSH r1
    NEXT

.OP_PUSHR_PRI:
    adds r2, r0, r5             @ relocate PRI to DAT
    mPUSH r2
    NEXT

.OP_POP_PRI:        @ tested
    mPOP r0
    NEXT

.OP_POP_ALT:        @ tested
    mPOP r1
    NEXT

.OP_PICK:
    GETPARAM r2
    ldr r0, [r6, r2]
    NEXT

.OP_STACK:          @ tested
    GETPARAM r2
    add r6, r6, r2              @ STK += param
    subs r1, r6, r5             @ ALT = STK, reverse-relocated
    CHKMARGIN r3
    CHKSTACK r3
    NEXT

.OP_HEAP:           @ tested
    GETPARAM r2
    mov r3, r12
    subs r1, r3, r5             @ ALT = HEA, reverse-relocated
    adds r3, r3, r2             @ add parameter to HEA
    mov r12, r3
    CHKMARGIN r3
    CHKHEAP r3
    NEXT

.OP_PROC:           @ tested
    mPUSH r7
    mov r7, r6                  @ FRM = stk
    CHKMARGIN r3
    NEXT

.OP_RET:
    mPOP r7                     @ pop FRM (relocated)
    mPOP r4                     @ pop CIP (return address, not relocated)
    @ verify return address (avoid stack/buffer overflow)
    cmp r4, r9                  @ return addres < code_end ?
    bhs .err_memaccess          @ no, error
    @ test passed
    add r4, r4, r8              @ relocate
    NEXT

.OP_RETN:           @ tested
    mPOP r7                     @ pop FRM (relocated)
    mPOP r4                     @ pop CIP (return address, not relocated)
    @ verify return address (avoid stack/buffer overflow)
    cmp r4, r9                  @ return addres < code_end ?
    bhs .err_memaccess          @ no, error
    @ all tests passed
    add r4, r4, r8              @ relocate
    mPOP r2                     @ pop # args passed to func
    add r6, r6, r2              @ STK += #args
    NEXT

.err_memaccess:
    movs r2, #AMX_ERR_MEMACCESS
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code

.OP_CALL:           @ tested
    mov r2, r8
    subs r2, r4, r2             @ r2 = r4 - r8 = relocated CIP - codebase -> reverse-relocate CIP
    adds r2, r2, #4             @ r2 = address of next instruction
    mPUSH r2
    JUMPREL r2
    NEXT

.OP_JUMP:           @ tested
    JUMPREL r2
    NEXT

.OP_JZER:           @ tested
    cmp r0, #0
    JUMPREL r2, eq              @ if PRI == 0, jump; otherwise skip param
    NEXT

.OP_JNZ:            @ tested
    cmp r0, #0
    JUMPREL r2, ne              @ if PRI != 0, jump; otherwise skip param
    NEXT

.OP_SHL:            @ tested
    lsls r0, r0, r1             @ PRI = PRI << ALT
    NEXT

.OP_SHR:            @ tested
    lsrs r0, r0, r1             @ PRI = PRI >> ALT (unsigned)
    NEXT

.OP_SSHR:           @ tested
    asrs r0, r0, r1             @ PRI = PRI >> ALT (signed)
    NEXT

.OP_SHL_C_PRI:      @ tested
    GETPARAM r2
    lsls r0, r0, r2             @ PRI = PRI << param
    NEXT

.OP_SHL_C_ALT:
    GETPARAM r2
    lsls r1, r1, r2             @ ALT = ALT << param
    NEXT

.OP_SMUL:           @ tested
    muls r0, r1, r0             @ dest register must also be a source
    NEXT

.OP_SDIV:           @ tested
    cmp r0, #0                  @ verify r0 (divisor)
    bne 1f                      @ r0 != 0 -> continue
    movs r2, #AMX_ERR_DIVIDE    @ r0 == 0 -> set error code & abort
    bl .amx_exit                @ r0 == 0 -> jump to error-exit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    @ save input registers and create absolute values for divisor & dividend
    push {r4 - r6}              @ need two more scratch registers, also need to save lr
    mov r4, r0
    asrs r6, r0, #31            @ r6 = (r0 >= 0) ? 0 : -1
    adds r0, r0, r6             @ "nop" if r6 == 0, "r0 -= 1" if r6 == -1
    eors r0, r0, r6             @ "nop" if r6 == 0, "r0 = ~r0" if r6 == -1
    mov r5, r1
    asrs r6, r1, #31            @ r6 = (r1 >= 0) ? 0 : -1
    adds r1, r1, r6             @ "nop" if r6 == 0, "r1 -= 1" if r6 == -1
    eors r1, r1, r6             @ "nop" if r6 == 0, "r1 = ~r1" if r6 == -1
    mov r6, r14                 @ save lr (because we make a function call)
    @ do the division
    bl  amx_udiv                @ r0 = r1 / r0, r1 = r1 % r0
    mov r14, r6                 @ restore r14
    @ patch signs
    cmp r4, #0                  @ check sign of original value of divisor
    bpl 2f                      @ original r0 >= 0, nothing to do
    rsbs r1, r1, #0             @ sign(remainder) = sign(divisor)
  2:
    mov r6, r4
    eors r6, r6, r5             @ check signs of original dividend and divisor
    bpl 9f                      @ sign(divident) == sign(divisor) -> done
    rsbs r0, r0, #0             @ sign(quotient) = sign(divident) XOR sign(divisor)
    @ so the quotient is negative (or zero); if the remainder is non-zero,
    @ floor the quotient and adjust the remainder
    cmp r1, #0
    beq 9f                      @ remainder == 0 -> done
    subs r0, r0, #1             @ remainder != 0 -> r0 = r0 - 1
    subs r1, r4, r1             @ r1 = original divisor - r1
  9:
    pop {r4 - r6}
    NEXT

.OP_ADD:            @ tested
    adds r0, r0, r1
    NEXT

.OP_SUB:            @ tested
    subs r0, r1, r0
    NEXT

.OP_AND:            @ tested
    ands r0, r0, r1
    NEXT

.OP_OR:             @ tested
    orrs r0, r0, r1
    NEXT

.OP_XOR:            @ tested
    eors r0, r0, r1
    NEXT

.OP_NOT:            @ tested
    @ see Hacker's Delight, ch. 2.12    x == 0 -> ~(x | -x)
    rsbs r2, r0, #0             @ r2 = #0 - PRI
    orrs r0, r0, r2             @ r0 = PRI | -PRI       -- sign bit set if PRI != 0
    mvns r0, r0                 @ r0 = ~(PRI | -PRI)    -- sign bit set if PRI == 0
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_NEG:            @ tested
    rsbs r0, r0, #0             @ PRI = #0 - PRI
    NEXT

.OP_INVERT:         @ tested
    mvns r0, r0                 @ PRI = NOT PRI (all bits inverted)
    NEXT

.OP_EQ:             @ tested
    @ see Hacker's Delight, ch. 2.12    x == y -> ~(x - y | y - x)
    subs r2, r0, r1             @ r2 = PRI - ALT                    -- sign bit set if PRI < ALT
    subs r0, r1, r0             @ r0 = ALT - PRI                    -- sign bit set if PRI > ALT
    orrs r0, r0, r2             @ r0 = (PRI - ALT) | (ALT - PRI)    -- sign bit set if PRI != ALT
    mvns r0, r0                 @ r0 = ~r0                          -- sign bit set if PRI == ALT
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_NEQ:            @ tested
    @ see Hacker's Delight, ch. 2.12    x != y -> x - y | y - x
    subs r2, r0, r1             @ r2 = PRI - ALT                    -- sign bit set if PRI < ALT
    subs r0, r1, r0             @ r0 = ALT - PRI                    -- sign bit set if PRI > ALT
    orrs r0, r0, r2             @ r0 = (PRI - ALT) | (ALT - PRI)    -- sign bit set if PRI != ALT
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_SLESS:          @ tested
    @ see Hacker's Delight, ch. 2.12
    subs r2, r0, r1             @ r2 = PRI - ALT            -- sign bit set if PRI < ALT
    lsrs r0, r2, #31            @ shift sign bit to bit 0
    NEXT

.OP_SLEQ:           @ tested
    @ see Hacker's Delight, ch. 2.12    x <= 0 -> x | (x - 1)
    subs r2, r0, r1             @ r2 = PRI - ALT            -- sign bit set if PRI < ALT
    subs r0, r2, #1             @ r0 = (PRI - ALT) - 1      -- sign bit set if PRI == ALT
    orrs r0, r0, r2
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_SGRTR:          @ tested
    subs r2, r1, r0             @ r2 = ALT - PRI            -- sign bit set if PRI > ALT
    lsrs r0, r2, #31            @ shift sign bit to bit 0
    NEXT

.OP_SGEQ:           @ tested
    subs r2, r1, r0             @ r2 = ALT - PRI            -- sign bit set if PRI > ALT
    subs r0, r2, #1             @ r0 = (ALT - PRI) - 1      -- sign bit set if PRI == ALT
    orrs r0, r0, r2
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_INC_PRI:        @ tested
    adds r0, r0, #1
    NEXT

.OP_INC_ALT:
    adds r1, r1, #1
    NEXT

.OP_INC_I:          @ tested
    ldr r3, [r5, r0]
    adds r3, r3, #1
    str r3, [r5, r0]
    NEXT

.OP_DEC_PRI:        @ tested
    subs r0, r0, #1
    NEXT

.OP_DEC_ALT:
    subs r1, r1, #1
    NEXT

.OP_DEC_I:          @ tested
    ldr r3, [r5, r0]
    subs r3, r3, #1
    str r3, [r5, r0]
    NEXT

.OP_MOVS:           @ tested
    GETPARAM r2
  .movsentry:
    subs r2, r2, #1             @ decrement, for address verification
    adds r3, r0, r5             @ r3 = relocated PRI
    VERIFYADDRESS r3            @ verify PRI (after relocation)
    adds r3, r3, r2
    VERIFYADDRESS r3            @ verify PRI + size - 1
    adds r3, r1, r5             @ r3 = relocated ALT
    VERIFYADDRESS r3            @ verify ALT (after relocation)
    adds r3, r3, r2
    VERIFYADDRESS r3            @ verify ALT + size - 1
    @ dropped through tests
    adds r2, r2, #1             @ restore r2 (# bytes to move)
    push {r0, r1}               @ save PRI and ALT
    adds r0, r0, r5             @ relocate r0/r1
    adds r1, r1, r5
  .movs4loop:
    subs r2, r2, #4             @ pre-decrement, 4 or more bytes to do?
    bmi .movs4end               @ no, exit loop
    ldmia r0!, {r3}
    stmia r1!, {r3}
    b .movs4loop
  .movs4end:
    adds r2, r2, #4             @ restore overrun
  .movs1loop:
    subs r2, r2, #1
    bmi .movsdone               @ count dropped negative -> done
    ldrb r3, [r0]
    strb r3, [r1]
    adds r0, #1
    adds r1, #1
    b .movs1loop
  .movsdone:
    @ restore PRI and ALT
    pop {r0, r1}                @ restore PRI and ALT
    NEXT

.OP_CMPS:           @ tested
    GETPARAM r2
  .cmpsentry:
    subs r2, r2, #1             @ decrement, for address verification
    adds r3, r0, r5             @ r3 = relocated PRI
    VERIFYADDRESS r3            @ verify PRI
    adds r3, r3, r2
    VERIFYADDRESS r3            @ verify PRI + size - 1
    adds r3, r1, r5             @ r3 = relocated ALT
    VERIFYADDRESS r3            @ verify ALT
    adds r3, r3, r2
    VERIFYADDRESS r3            @ verify ALT + size - 1
    @ dropped through tests
    adds r2, r2, #1             @ restore r2
    push {r1, r6}               @ save ALT, need extra scratch register
    add r0, r0, r5              @ relocate r0 and r1
    add r1, r1, r5
    movs r6, #0                 @ preset result, in case r2 == 0
  .cmps4loop:
    subs r2, r2, #4             @ pre-decrement, 4 or more bytes to do?
    bmi .cmps4end               @ no, exit loop
    ldmia r0!, {r6}
    ldmia r1!, {r3}
    subs r6, r6, r3             @ r6 = [PRI] - [ALT]
    beq .cmps4loop              @ ([PRI] - [ALT]) == 0 -> comparison ok -> continue
    b .cmpsdone                 @ when arrived here, comparison failed -> quit
  .cmps4end:
    adds r2, r2, #4             @ restore overrun
  .cmps1loop:
    subs r2, r2, #1
    bmi .cmpsdone               @ count dropped negative -> done
    ldrb r6, [r0]
    ldrb r3, [r1]
    adds r0, #1
    adds r1, #1
    subs r6, r6, r3             @ r14 = [PRI] - [ALT]
    beq .cmps1loop
  .cmpsdone:
    mov r0, r6
    pop {r1, r6}                @ restore ALT, restore scratch register
    NEXT

.OP_FILL:           @ tested
    GETPARAM r2
  .fillentry:
    adds r3, r1, r5             @ r3 = relocated ALT
    VERIFYADDRESS r3            @ verify ALT
    adds r3, r3, r2
    subs r3, r3, #1
    VERIFYADDRESS r3            @ verify ALT + size - 1
    @ dropped through tests
    adds r3, r1, r5             @ r3 = relocated ALT again
  .fill4loop:
    subs r2, r2, #4
    bmi .filldone
    stmia r3!, {r0}
    b .fill4loop
  .filldone:
    NEXT

    @ add packed opcodes for MOVS, CMPS and FILL here, so that the jumps to the shared code is in reach
.ifndef _NO_PACKED_OPC

.OP_MOVS_P:
    GETPARAM_P r2
    b .movsentry

.OP_CMPS_P:
    GETPARAM_P r2
    b .cmpsentry

.OP_FILL_P:
    GETPARAM_P r2
    b .fillentry

.endif

.OP_HALT:           @ tested
    ldr r2, [sp]                @ get "retval" pointer
    cmp r2, #0                  @ pointer == NULL ?
    beq 1f                      @ yes, skip storing it
    str r0, [r2]                @ no, store PRI at pointer address
  1:
    GETPARAM r2                 @ parameter = return code from function
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code

.OP_BOUNDS:
    GETPARAM r2
    cmp r0, r2                  @ r0 > bounds ?
    bls 1f                      @ no, ignore
    movs r2, #AMX_ERR_BOUNDS    @ yes, quit with error
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code
  1:
    NEXT

.OP_SYSREQ:         @ tested
    GETPARAM r0                 @ native function index in r0
    @ store stack and heap state AMX state
    mov r3, r10                 @ copy r10, AMX base
    subs r2, r7, r5             @ reverse-relocate FRM
    str r2, [r3, #amxFRM]
    subs r2, r6, r5             @ reverse-relocate STK
    str r2, [r3, #amxSTK]
    mov r2, r12
    subs r2, r2, r5             @ reverse-relocate HEA
    str r2, [r3, #amxHEA]
    mov r2, r8
    subs r2, r4, r2             @ reverse-relocate CIP
    str r2, [r3, #amxCIP]
    @ invoke callback
    push {r1, r7}               @ save ALT (the callee may trample it), plus extra scratch register
    mov r2, r12                 @ save r12 & r14 (lr) indirectly
    mov r7, r14
    push {r1, r2, r7}           @ r1 is a dummy, to maintain 8-byte stack alignment
    push {r0}                   @ reserve a cell on the stack for the return value
    mov r1, r0                  @ 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 @ 1st arg = AMX base
    mov r2, sp                  @ 3rd arg = address of return value
    mov r3, r6                  @ 4th arg = address in the AMX stack
    ldr r7, [r0, #amxCallback]  @ callback function pointer in r7
    blx r7                      @ call natives callback
    mov r3, r0                  @ get error return in r3
    pop {r0}                    @ get return value, remove from stack
    pop {r1, r2, r7}            @ restore r12 & r14 indirectly (and pop of dummy r1)
    mov r14, r7
    mov r12, r2
    pop {r1, r7}                @ restore saved registers
    cmp r3, #AMX_ERR_NONE       @ callback hook returned error/abort code?
    beq 1f                      @ no
    bl .amx_exit                @ yes -> quit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    NEXT

.OP_SWITCH:         @ tested
    push {r6}                   @ need extra register
    ldr r2, [r4]                @ r2 = [CIP], relative offset to case-table
    adds r2, r2, r4             @ r2 = direct address, OP_CASETBL opcode already skipped
    ldr r3, [r2]                @ r3 = number of case-table records
    ldr r6, [r2, #4]            @ preset CIP to "default" case (none-matched)
    adds r4, r2, r6
  1:
    subs r3, r3, #1             @ decrement number to do; any left?
    bmi 2f                      @ no, quit (CIP already set to the default value)
    adds r2, r2, #8             @ move to next record
    ldr r6, [r2]                @ get the case value
    cmp r0, r6                  @ case value identical to PRI ?
    bne 1b                      @ no, continue
    ldr r6, [r2, #4]            @ yes, load matching CIP and exit loop
    adds r4, r2, r6             @ r4 = address of case record + offset
  2:
    pop {r6}
    NEXT

.OP_SWAP_PRI:       @ tested
    ldr r2, [r6]
    str r0, [r6]
    mov r0, r2
    NEXT

.OP_SWAP_ALT:
    ldr r2, [r6]
    str r1, [r6]
    mov r1, r2
    NEXT

.OP_BREAK:
    @ first test for a debug callback
    mov r3, r10
    ldr r2, [r3, #amxDebug]     @ r2 = debug callback
    cmp r2, #0                  @ debug callback set?
    beq 1f                      @ no, quit
    @ store stack and heap state AMX state
    subs r2, r7, r5             @ reverse-relocate FRM
    str r2, [r3, #amxFRM]
    subs r2, r6, r5             @ reverse-relocate STK
    str r2, [r3, #amxSTK]
    mov r2, r12
    subs r2, r1, r5             @ reverse-relocate HEA
    str r2, [r3, #amxHEA]
    mov r2, r8
    subs r2, r4, r2             @ reverse-relocate CIP
    str r2, [r3, #amxCIP]
    @ invoke debug hook (address still in r3)
    push {r0, r1, r4}           @ save register callee may trample, plus r4 for extra scratch
    mov r2, r12                 @ save r12 & r14 indirectly
    mov r4, r14
    push {r0, r2, r4}           @ push r0 as dummy, to maintain 8-byte stack alignment
    ldr r2, [r3, #amxDebug]     @ r2 = debug callback again
    mov r0, r10                 @ 1st arg = AMX
    blx r2                      @ call debug callback
    mov r3, r0                  @ store exit code in r3 (r0 is restored)
    pop {r0, r2, r4}            @ restore r12 & r14 (indirectly), drop dummy (r0)
    mov r12, r2
    mov r14, r4
    pop {r0, r1, r4}            @ restore other save registers
    cmp r3, #AMX_ERR_NONE       @ debug hook returned error/abort code?
    beq 1f                      @ no
    bl .amx_exit                @ yes -> quit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    NEXT

.OP_CASETBL:
.OP_CASETBL_OVL:
    movs r2, #AMX_ERR_INVINSTR  @ these instructions are no longer supported
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code


    @ patched instructions
.ifndef _DONT_RELOCATE

.OP_SYSREQ_D:
    GETPARAM r0                 @ address of native function in r0 (r0 = scratch, because it is overwritten anyway)
    @ store stack and heap state AMX state
    mov r3, r10                 @ copy r10, AMX base
    subs r2, r7, r5             @ reverse-relocate FRM
    str r2, [r3, #amxFRM]
    subs r2, r6, r5             @ reverse-relocate STK
    str r2, [r3, #amxSTK]
    mov r2, r12
    subs r2, r2, r5             @ reverse-relocate HEA
    str r2, [r3, #amxHEA]
    mov r2, r8
    subs r2, r4, r2             @ reverse-relocate CIP
    str r2, [r3, #amxCIP]
    @ invoke callback
    mov r2, r12                 @ save r12 & r14
    mov r3, r14
    push {r1 - r4}              @ save ALT (the callee may trample it), plus values of r12 & r14 (r4 is dummy to maintain 8-byte alignment)
    mov r3, r0                  @ r3 = native function address (copied from r0; r0 is overwritten)
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r6                  @ 2nd arg = address in the AMX stack
    blx r3                      @ call native function directly
    pop {r1 - r4}               @ restore registers
    mov r12, r2
    mov r14, r3
    mov r2, r10
    ldr r3, [r2, #amxError]     @ get error returned by native function
    cmp r3, #AMX_ERR_NONE       @ callback hook returned error/abort code?
    beq 1f                      @ no
    bl .amx_exit                @ yes -> quit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    NEXT

.OP_SYSREQ_ND:      @ tested
    mov r2, r11                 @ need extra scratch register
    push {r2}
    GETPARAM r0                 @ address of native function in r0 (temporary)
    GETPARAM r2                 @ get # parameters
    mPUSH r2                    @ push second parameter
    mov r11, r2                 @ r11 = # parameters
    @ store stack and heap state AMX state
    mov r3, r10                 @ copy r10, AMX base
    subs r2, r7, r5             @ reverse-relocate FRM
    str r2, [r3, #amxFRM]
    subs r2, r6, r5             @ reverse-relocate STK
    str r2, [r3, #amxSTK]
    mov r2, r12
    subs r2, r2, r5             @ reverse-relocate HEA
    str r2, [r3, #amxHEA]
    mov r2, r8
    subs r2, r4, r2             @ reverse-relocate CIP
    str r2, [r3, #amxCIP]
    @ invoke callback (address still in r0)
    mov r2, r12                 @ save r12 & r14
    mov r3, r14
    push {r1 - r3}              @ save ALT (the callee may trample it), value of r12 & r14
    mov r3, r0                  @ r3 = native function address (copied from r0; r0 is overwritten)
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r6                  @ 2nd arg = address in the AMX stack
    blx r3                      @ call native function directly
    pop {r1 - r3}               @ restore registers
    mov r12, r2
    mov r14, r3
    add r6, r6, r11             @ remove # parameters from the AMX stack
    adds r6, r6, #4             @ also remove the extra cell pushed on the AMX stack
    pop {r2}                    @ restore original value of r11
    mov r11, r2
    mov r2, r10
    ldr r3, [r2, #amxError]     @ get error returned by native function
    cmp r3, #AMX_ERR_NONE       @ callback hook returned error/abort code?
    beq 1f                      @ no
    bl .amx_exit                @ yes -> quit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    NEXT

.else   @ _DONT_RELOCATE

.OP_SYSREQ_D:
.OP_SYSREQ_ND:
    movs r2, #AMX_ERR_INVINSTR  @ relocation disabled -> patched instructions should not be present
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code

.endif  @ _DONT_RELOCATE

    @ overlay instructions
.ifndef _NO_OVERLAY

.OP_CALL_OVL:
    push {r6}                   @ need extra scratch register
    mov r6, r8
    adds r2, r4, #4             @ r2 = address of next instruction (absolute)
    subs r2, r2, r6             @ r2 = relative address (to start of code segment)
    mov r6, r10
    ldr r3, [r6, #amxOvlIndex]  @ r3 = overlay index
    lsls r2, r2, #16            @ r2 = (address << 16)
    adds r2, r2, r3             @ r2 = (address << 16) + ovl_index
    mPUSH r2
    ldr r2, [r4]                @ r2 = [CIP] = param of ICALL = new overlay index
    mov r3, r10
    str r2, [r3, #amxOvlIndex]
    mov r2, r12
    mov r3, r14
    push {r0 - r4}              @ save registers PRI & ALT, plus values of r12 & r14 (r4 is a dummy, to keep sp 8-byte aligned)
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r2                  @ 2nd arg = overlay index
    ldr r2, [r6, #amxOverlay]   @ callback function pointer in r2
    blx r2                      @ call overlay callback
    pop {r0 - r4}               @ restore registers
    mov r12, r2
    mov r14, r3
    pop {r6}
    mov r3, r10
    ldr r4, [r3, #amxCode]      @ CIP = code base
    mov r8, r4                  @ r8 = code pointer (base)
    NEXT

.OP_RETN_OVL:
    mPOP r7                     @ pop FRM
    mPOP r4                     @ pop relative CIP (return address) + overlay index
    mPOP r2                     @ pop # args passed to func
    add r6, r6, r2              @ STK += #args
    mov r2, r12
    mov r3, r14
    push {r0 - r3}              @ save PRI, ALT plus values of r12 & r14
    mov r0, r10                 @ 1st arg = AMX
    movs r1, #0
    subs r1, r1, #1             @ r1 = 0xffffffff
    lsrs r1, r1, #16            @ r1 = 0x0000ffff
    ands r1, r4, r1             @ 2nd arg = overlay index
    str r1, [r0, #amxOvlIndex]  @ store new overlay index too
    ldr r2, [r0, #amxOverlay]   @ callback function pointer in r2
    blx r2                      @ call overlay callback
    pop {r0 - r3}               @ restore registers
    mov r12, r2
    mov r14, r3
    mov r3, r10
    ldr r2, [r3, #amxCode]      @ r2 = code pointer (base)
    mov r8, r2                  @ r8 = base address
    lsrs r4, r4, #16            @ r4 = relative address (shifted out overlay index)
    adds r4, r2, r4             @ r4 = base address + relative address
    NEXT

.OP_SWITCH_OVL:
    push {r5}                   @ need extra register for comparison loop
    ldr r2, [r4]                @ r2 = [CIP], relative offset to case-table
    adds r2, r2, r4             @ r2 = direct address, OP_CASETBL opcode already skipped
    ldr r3, [r2]                @ r3 = number of case-table records
    ldr r4, [r2, #4]            @ preset ovl_index to "default" case (none-matched)
  1:
    subs r3, r3, #1             @ decrement number to do; any left?
    bmi 2f                      @ no, quit (CIP already set to the default value)
    adds r2, r2, #8             @ move to next record
    ldr r5, [r2]                @ get the case value
    cmp r0, r14                 @ case value identical to PRI ?
    bne 1b                      @ no, continue
    ldr r4, [r2, #4]            @ yes, load matching ovl_index and exit loop
  2:
    pop {r5}                    @ restore saved register
    mov r3, r10
    str r4, [r3, #amxOvlIndex]  @ store new overlay index
    mov r2, r12
    mov r3, r14
    push {r0 - r3}              @ save PRI, ALT plus values of r12 & r14
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r4                  @ 2nd arg = overlay index
    ldr r2, [r3, #amxOverlay]   @ callback function pointer in r2
    blx r2                      @ call overlay callback
    pop {r0 - r3}               @ restore registers
    mov r12, r2
    mov r14, r3
    mov r3, r10
    ldr r4, [r3, #amxCode]      @ CIP = code base
    mov r8, r4                  @ r8 = code pointer (base)
    NEXT

.else   @ _NO_OVERLAY

.OP_CALL_OVL:
.OP_RETN_OVL:
.OP_SWITCH_OVL:
    movs r2, #AMX_ERR_INVINSTR  @ overlays disabled -> instructions should not be present
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code

.endif  @ _NO_OVERLAY


    @ supplemental instructions
.ifndef _NO_MACRO_INSTR

.OP_LIDX:           @ tested
    lsls r2, r0, #2             @ r2 = 4*PRI
    adds r3, r1, r2             @ r3 = ALT + 4*PRI
    adds r3, r3, r5             @ relocate to absolute address
    VERIFYADDRESS r3
    ldr r0, [r3]
    NEXT

.OP_LIDX_B:         @ tested
    GETPARAM r2
    mov r3, r0                  @ r3 = PRI
    lsls r3, r3, r2             @ r3 = PRI << param
    adds r3, r3, r1             @ r3 = ALT + (PRI << param)
    adds r3, r3, r5             @ relocate to absolute address
    VERIFYADDRESS r3
    ldr r0, [r3]
    NEXT

.OP_IDXADDR:        @ tested
    lsls r0, r0, #2             @ r0 = 4*PRI
    adds r0, r1, r0             @ PRI = ALT + 4*PRI
    NEXT

.OP_IDXADDR_B:
    GETPARAM r2
    lsls r0, r0, r2             @ r0 = PRI << param
    adds r0, r1, r0             @ PRI = ALT + (PRI << param)
    NEXT

.OP_PUSH_C:         @ tested
    GETPARAM r2
    mPUSH r2
    NEXT

.OP_PUSH:           @ tested
    GETPARAM r2
    ldr r2, [r5, r2]
    mPUSH r2
    NEXT

.OP_PUSH_S:         @ tested
    GETPARAM r2
    ldr r2, [r7, r2]
    mPUSH r2
    NEXT

.OP_PUSH_ADR:       @ tested
    GETPARAM r2
    adds r2, r2, r7             @ relocate to FRM
    subs r2, r2, r5             @ but relative to start of data section
    mPUSH r2
    NEXT

.OP_PUSHR_C:
    GETPARAM r2
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    NEXT

.OP_PUSHR_S:
    GETPARAM r2
    ldr r2, [r7, r2]
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    NEXT

.OP_PUSHR_ADR:
    GETPARAM r2
    adds r2, r2, r7             @ relocate to FRM
    mPUSH r2
    NEXT

.OP_JEQ:            @ tested
    cmp r0, r1
    JUMPREL r2, eq              @ if PRI == ALT, jump; otherwise skip param
    NEXT

.OP_JNEQ:           @ tested
    cmp r0, r1
    JUMPREL r2, ne              @ if PRI != ALT, jump; otherwise skip param
    NEXT

.OP_JSLESS:         @ tested
    cmp r0, r1
    JUMPREL r2, lt              @ if PRI < ALT (signed), jump; otherwise skip param
    NEXT

.OP_JSLEQ:          @ tested
    cmp r0, r1
    JUMPREL r2, le              @ if PRI <= ALT (signed), jump; otherwise skip param
    NEXT

.OP_JSGRTR:         @ tested
    cmp r0, r1
    JUMPREL r2, gt              @ if PRI > ALT (signed), jump; otherwise skip param
    NEXT

.OP_JSGEQ:          @ tested
    cmp r0, r1
    JUMPREL r2, ge              @ if PRI >= ALT (signed), jump; otherwise skip param
    NEXT

.OP_SDIV_INV:
    @ swap r0 and r1, then branch to the normal (signed) division case
    mov r2, r0
    mov r0, r1
    mov r1, r2
    b   .OP_SDIV

.OP_SUB_INV:        # tested
    subs r0, r0, r1
    NEXT

.OP_ADD_C:          @ tested
    GETPARAM r2
    adds r0, r0, r2             @ PRI += param
    NEXT

.OP_SMUL_C:         @ tested
    GETPARAM r2
    muls r0, r0, r2             @ PRI *= param
    NEXT

.OP_ZERO_PRI:       @ tested
    movs r0, #0
    NEXT

.OP_ZERO_ALT:
    movs r1, #0
    NEXT

.OP_ZERO:           @ tested
    GETPARAM r2
    movs r3, #0
    str r3, [r5, r2]
    NEXT

.OP_ZERO_S:         @ tested
    GETPARAM r2
    movs r3, #0
    str r3, [r7, r2]
    NEXT

.OP_EQ_C_PRI:       @ tested
    @ see Hacker's Delight, ch. 2.12    x == y -> ~(x - y | y - x)
    GETPARAM r2
    subs r3, r0, r2             @ r3 = PRI - param                      -- sign bit set if PRI < param
    subs r0, r2, r0             @ r0 = param - PRI                      -- sign bit set if PRI > param
    orrs r0, r0, r3             @ r0 = (PRI - param) | (param - PRI)    -- sign bit set if PRI != param
    mvns r0, r0                 @ r0 = ~r0                              -- sign bit set if PRI == param
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_EQ_C_ALT:
    @ see Hacker's Delight, ch. 2.12    x == y -> ~(x - y | y - x)
    GETPARAM r2
    subs r3, r1, r2             @ r3 = ALT - param                      -- sign bit set if ALT < param
    subs r0, r2, r1             @ r0 = param - ALT                      -- sign bit set if ALT > param
    orrs r0, r0, r3             @ r0 = (ALT - param) | (param - ALT)    -- sign bit set if ALT != param
    mvns r0, r0                 @ r0 = ~r0                              -- sign bit set if ALT == param
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_INC:            @ tested
    GETPARAM r2
    ldr r3, [r5, r2]
    adds r3, r3, #1
    str r3, [r5, r2]
    NEXT

.OP_INC_S:          @ tested
    GETPARAM r2
    ldr r3, [r7, r2]
    adds r3, r3, #1
    str r3, [r7, r2]
    NEXT

.OP_DEC:
    GETPARAM r2
    ldr r3, [r5, r2]
    subs r3, r3, #1
    str r3, [r5, r2]
    NEXT

.OP_DEC_S:
    GETPARAM r2
    ldr r3, [r7, r2]
    subs r3, r3, #1
    str r3, [r7, r2]
    NEXT

.OP_SYSREQ_N:       @ tested
    mov r2, r12                 @ save r12 (need extra scratch)
    push {r2}
    GETPARAM r0                 @ get native function index
    GETPARAM r2                 @ get stack size of parameters (# parameters * cell size)
    mPUSH r2                    @ push parameter stack size
    mov r12, r2                 @ r12 = # parameters
    @ store stack and heap state AMX state
    mov r3, r10                 @ copy r10, AMX base
    subs r2, r7, r5             @ reverse-relocate FRM
    str r2, [r3, #amxFRM]
    subs r2, r6, r5             @ reverse-relocate STK
    str r2, [r3, #amxSTK]
    mov r2, r12
    subs r2, r2, r5             @ reverse-relocate HEA
    str r2, [r3, #amxHEA]
    mov r2, r8
    subs r2, r4, r2             @ reverse-relocate CIP
    str r2, [r3, #amxCIP]
    @ invoke callback
    mov r2, r14
    push {r1, r2, r4, r5}       @ save ALT, values of r14, plus 2 extra scratch (r4, r5); r12 was saved earlier
    push {r0}                   @ dummy, reserve cell for the return value (note: 6 registers pushed in total)
    mov r5, r12                 @ r5 = parameter stack size
    mov r3, r10                 @ r3 = AMX base (again)
    ldr r4, [r3, #amxCallback]  @ callback function pointer in r4
    mov r1, r0                  @ 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 @ 1st arg = AMX
    mov r2, sp                  @ 3rd arg = address of return value
    mov r3, r6                  @ 4th arg = address in the AMX stack
    blx r4                      @ call natives callback
    mov r3, r0                  @ get error return in r3
    add r6, r6, r5              @ remove # parameters from the AMX stack (r5 = parameter stack size)
    adds r6, r6, #4             @ also remove the extra cell pushed
    pop {r0}                    @ return value in r0
    pop {r1, r2, r4, r5}        @ restore registers
    mov r14, r2
    pop {r2}                    @ restore r12
    mov r12, r2
    cmp r3, #AMX_ERR_NONE       @ callback hook returned error/abort code?
    beq 1f                      @ no
    bl .amx_exit                @ yes -> quit (use BL instruction for longer jump range, r14 is unused by exit code)
  1:
    NEXT

.OP_PUSHM_C:        @ tested
    GETPARAM r3                 @ r3 = parameter count
  1:
    GETPARAM r2
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM:          @ tested
    GETPARAM r3                 @ r3 = parameter count
  1:
    GETPARAM r2
    ldr r2, [r5, r2]
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM_S:        @ tested
    GETPARAM r3                 @ r3 = parameter count
  1:
    GETPARAM r2
    ldr r2, [r7, r2]
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM_ADR:      @ tested
    GETPARAM r3                 @ r3 = parameter count
  1:
    GETPARAM r2
    adds r2, r2, r7             @ relocate to FRM
    subs r2, r2, r5             @ but relative to start of data section
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_C:
    GETPARAM r3                @ r3 = parameter count
  1:
    GETPARAM r2
    add r2, r2, r5              @ relocate to DAT
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_S:
    GETPARAM r3                @ r3 = parameter count
  1:
    GETPARAM r2
    ldr r2, [r7, r2]
    add r2, r2, r5              @ relocate to DAT
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_ADR:
    GETPARAM r3                 @ r3 = parameter count
  1:
    GETPARAM r2
    add r2, r2, r7              @ relocate to FRM
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_LOAD2:          @ tested
    GETPARAM r2
    ldr r0, [r5, r2]
    GETPARAM r2
    ldr r1, [r5, r2]
    NEXT

.OP_LOAD2_S:        @ tested
    GETPARAM r2
    ldr r0, [r7, r2]
    GETPARAM r2
    ldr r1, [r7, r2]
    NEXT

.OP_CONST:          @ tested
    GETPARAM r2
    GETPARAM r3
    str r3, [r5, r2]
    NEXT

.OP_CONST_S:        @ tested
    GETPARAM r2
    GETPARAM r3
    str r3, [r7, r2]
    NEXT

.endif  @ _NO_MACRO_INSTR


    @ packed opcodes
.ifndef _NO_PACKED_OPC

.OP_LOAD_P_PRI:     @ tested
    GETPARAM_P r2
    ldr r0, [r5, r2]
    NEXT

.OP_LOAD_P_ALT:
    GETPARAM_P r2
    ldr r1, [r5, r2]
    NEXT

.OP_LOAD_P_S_PRI:   @ tested
    GETPARAM_P r2
    ldr r0, [r7, r2]
    NEXT

.OP_LOAD_P_S_ALT:
    GETPARAM_P r2
    ldr r1, [r7, r2]
    NEXT

.OP_LREF_P_S_PRI:
    GETPARAM_P r2
    ldr r2, [r7, r2]
    ldr r0, [r5, r2]
    NEXT

.OP_LREF_P_S_ALT:
    GETPARAM_P r2
    ldr r2, [r7, r2]
    ldr r1, [r5, r2]
    NEXT

.OP_LODB_P_I:
    GETPARAM_P r2
    adds r3, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r3
    cmp r2, #1
    bne 1f
    ldrb r0, [r3]
    b 4f
  1:
    cmp r2, #2
    bne 2f
    ldrh r0, [r3]
    b 4f
  2:
    cmp r2, #4
    bne 4f
    ldr r0, [r3]
  4:
    NEXT

.OP_CONST_P_PRI:
    GETPARAM_P r0
    NEXT

.OP_CONST_P_ALT:
    GETPARAM_P r1
    NEXT

.OP_ADDR_P_PRI:
    GETPARAM_P r0
    adds r0, r0, r7             @ add FRM
    subs r0, r0, r5             @ reverse relocate
    NEXT

.OP_ADDR_P_ALT:
    GETPARAM_P r1
    adds r1, r1, r7             @ add FRM
    subs r1, r1, r5             @ reverse relocate
    NEXT

.OP_STOR_P:         @ tested
    GETPARAM_P r2
    str r0, [r5, r2]
    NEXT

.OP_STOR_P_S:       @ tested
    GETPARAM_P r2
    str r0, [r7, r2]
    NEXT

.OP_SREF_P_S:
    GETPARAM_P r2
    ldr r2, [r7, r2]
    str r0, [r5, r2]
    NEXT

.OP_STRB_P_I:
    GETPARAM_P r2
    adds r3, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r3
    cmp r2, #1
    bne 1f
    strb r0, [r3]
    b 4f
  1:
    cmp r2, #2
    bne 2f
    strh r0, [r3]
    b 4f
  2:
    cmp r2, #4
    bne 4f
    str r0, [r3]
  4:
    NEXT

.OP_LIDX_P_B:
    GETPARAM_P r2
    mov r3, r0
    lsls r3, r3, r2             @ r3 = PRI << param
    adds r3, r3, r1             @ r3 = ALT + (PRI << param)
    adds r3, r3, r5             @ relocate to absolute address
    VERIFYADDRESS r3
    ldr r0, [r3]
    NEXT

.OP_IDXADDR_P_B:
    GETPARAM_P r2
    lsls r0, r0, r2             @ r0 = PRI << param
    adds r0, r1, r0             @ PRI = ALT + (PRI << param)
    NEXT

.OP_ALIGN_P_PRI:
    GETPARAM_P r2
.ifndef _BIGENDIAN
    cmp r2, #4                  @ param < cell size ?
    bhi 1f                      @ no -> skip
    movs r3, #4
    subs r2, r3, r2             @ r2 = 4 - param
    eors r0, r0, r2             @ PRI ^= (4 - param), but only if param < 4
  1:
.endif
    NEXT

.OP_PUSH_P_C:
    GETPARAM_P r2
    mPUSH r2
    NEXT

.OP_PUSH_P:
    GETPARAM_P r2
    ldr r2, [r5, r2]
    mPUSH r2
    NEXT

.OP_PUSH_P_S:
    GETPARAM_P r2
    ldr r2, [r7, r2]
    mPUSH r2
    NEXT

.OP_PUSH_P_ADR:
    GETPARAM_P r2
    adds r2, r2, r7             @ relocate to FRM
    subs r2, r2, r5             @ but relative to start of data section
    mPUSH r2
    NEXT

.OP_PUSHR_P_C:
    GETPARAM_P r2
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    NEXT

.OP_PUSHR_P_S:
    GETPARAM_P r2
    ldr r2, [r7, r2]
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    NEXT

.OP_PUSHR_P_ADR:
    GETPARAM_P r2
    adds r2, r2, r7             @ relocate to FRM
    mPUSH r2
    NEXT

.OP_PUSHM_P_C:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM_P:
    GETPARAM_P r3               @ r3 = parameter count
 1:
    GETPARAM r2
    ldr r2, [r5, r2]
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM_P_S:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    ldr r2, [r7, r2]
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHM_P_ADR:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    adds r2, r2, r7             @ relocate to FRM
    subs r2, r2, r5             @ but relative to start of data section
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_P_C:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_P_S:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    ldr r2, [r7, r2]
    adds r2, r2, r5             @ relocate to DAT
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_PUSHRM_P_ADR:
    GETPARAM_P r3               @ r3 = parameter count
  1:
    GETPARAM r2
    adds r2, r2, r7             @ relocate to FRM
    mPUSH r2
    subs r3, r3, #1
    bgt 1b
    NEXT

.OP_STACK_P:        @ tested
    GETPARAM_P r2
    adds r6, r6, r2             @ STK += param
    subs r1, r6, r5             @ ALT = STK, reverse-relocated
    CHKMARGIN r3
    CHKSTACK r3
    NEXT

.OP_HEAP_P:
    GETPARAM_P r2
    mov r3, r12
    subs r1, r3, r5             @ ALT = HEA, reverse-relocated
    adds r3, r3, r2
    mov r12, r3
    CHKMARGIN r3
    CHKHEAP r3
    NEXT

.OP_SHL_P_C_PRI:
    GETPARAM_P r2
    lsls r0, r0, r2             @ PRI = PRI << param
    NEXT

.OP_SHL_P_C_ALT:
    GETPARAM_P r2
    lsls r1, r1, r2             @ ALT = ALT << param
    NEXT

.OP_ADD_P_C:        @ tested
    GETPARAM_P r2
    adds r0, r0, r2             @ PRI += param
    NEXT

.OP_SMUL_P_C:
    GETPARAM_P r2
    muls r0, r0, r2             @ PRI *= param
    NEXT

.OP_ZERO_P:
    GETPARAM_P r2
    movs r3, #0
    str r3, [r5, r2]
    NEXT

.OP_ZERO_P_S:
    GETPARAM_P r2
    movs r3, #0
    str r3, [r7, r2]
    NEXT

.OP_EQ_P_C_PRI:     @ tested
    @ see Hacker's Delight, ch. 2.12    x == y -> ~(x - y | y - x)
    GETPARAM_P r2
    subs r3, r0, r2             @ r3 = PRI - param                      -- sign bit set if PRI < param
    subs r0, r2, r0             @ r0 = param - PRI                      -- sign bit set if PRI > param
    orrs r0, r0, r3             @ r0 = (PRI - param) | (param - PRI)    -- sign bit set if PRI != param
    mvns r0, r0                 @ r0 = ~r0                              -- sign bit set if PRI == param
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_EQ_P_C_ALT:
    @ see Hacker's Delight, ch. 2.12    x == y -> ~(x - y | y - x)
    GETPARAM_P r2
    subs r3, r1, r2             @ r3 = ALT - param                      -- sign bit set if ALT < param
    subs r0, r2, r1             @ r0 = param - ALT                      -- sign bit set if ALT > param
    orrs r0, r0, r3             @ r0 = (ALT - param) | (param - ALT)    -- sign bit set if ALT != param
    mvns r0, r0                 @ r0 = ~r0                              -- sign bit set if ALT == param
    lsrs r0, r0, #31            @ shift sign bit to bit 0
    NEXT

.OP_INC_P:
    GETPARAM_P r2
    ldr r3, [r5, r2]
    adds r3, r3, #1
    str r3, [r5, r2]
    NEXT

.OP_INC_P_S:
    GETPARAM_P r2
    ldr r3, [r7, r2]
    adds r3, r3, #1
    str r3, [r7, r2]
    NEXT

.OP_DEC_P:
    GETPARAM_P r2
    ldr r3, [r5, r2]
    subs r3, r3, #1
    str r3, [r5, r2]
    NEXT

.OP_DEC_P_S:
    GETPARAM_P r2
    ldr r3, [r7, r2]
    subs r3, r3, #1
    str r3, [r7, r2]
    NEXT

@.OP_MOVS_P:
@   moved to near .OP_MOVS
@.OP_CMPS_P:
@   moved to near .OP_CMPS
@.OP_FILL_P:
@   moved to near .OP_FILL

.OP_HALT_P:
    ldr r2, [sp]                @ get "retval" pointer
    cmp r2, #0                  @ pointer == NULL ?
    beq 1f                      @ yes, skip storing it
    str r0, [r2]                @ no, store PRI at pointer address
  1:
    GETPARAM_P r2               @ parameter = return code from function
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code

.OP_BOUNDS_P:
    GETPARAM_P r2
    cmp r0, r2                  @ r0 > bounds ?
    bls 1f                      @ no, ignore
    movs r2, #AMX_ERR_BOUNDS    @ yes, quit with error
    bl .amx_exit                @ use BL instruction for longer jump range, r14 is unused by exit code
  1:
    NEXT

.endif  @ _NO_PACKED_OPC


.amx_exit:                      @ assume r2 already set to the exit code
    @ reverse relocate registers
    mov r3, r12                 @ r3 = HEA
    subs r3, r3, r5             @ reverse-relocate HEA
    subs r6, r6, r5             @ reverse-relocate STK
    subs r7, r7, r5             @ reverse-relocate FRM
    mov r5, r8                  @ r5 = code base
    subs r4, r4, r5             @ reverse-relocate CIP

    @ store stack and heap state AMX state
    mov r5, r10
    str r0, [r5, #amxPRI]       @ PRI
    str r1, [r5, #amxALT]       @ ALT
    str r3, [r5, #amxHEA]       @ HEA (copied and reverse relocated from r12)
    str r4, [r5, #amxCIP]       @ CIP
    str r6, [r5, #amxSTK]       @ STK
    str r7, [r5, #amxFRM]       @ FRM

    add sp, sp, #8              @ restore stack (drop space reserved for return value)
    mov r0, r2                  @ put return value in r0
    pop {r3 - r7}               @ restore high registers
    mov r8, r3
    mov r9, r4
    mov r10, r5
    mov r11, r6
    mov r12, r7
    pop {r4 - r7, pc}           @ restore low registers and return

    .size   amx_exec_run, .-amx_exec_run


    .align  2
    .global amx_udiv
    .thumb_func
    .type   amx_udiv, %function
amx_udiv:
    @ expects divident in r1, divisor in r0
    @ on exit quotient is in r0, remainder in r1
    @ unsigned division only; when r1 (divisor) is zero, the function returns
    @ with all registers unchanged
    cmp r0, #0                  @ verify r0
    beq .udiv_quit              @ just for security
    push {r2 - r4}              @ save extra registers needed
    mov r2, r0                  @ save divisor (r0 is changed)
    movs r3, #1
    lsls r3, r3, #31            @ r3 has top bit set
    movs r0, #0                 @ initial quotient
    movs r4, #0                 @ initial remainder
  .udiv1:
    lsls r2, #1                 @ shift divisor left, top bit goes to carry flag
    adcs r4, r4, r4             @ r4 = r4 + r4 + carry => r4 = r4 << 1 + carry
    cmp r4, r1                  @ accumulated remainder >= divident ?
    blo .udiv2                  @ no
    adds r0, r0, r3             @ yes -> set bit in quotient
    subs r4, r4, r1             @ subtract from remainder
  .udiv2:
    lsrs r3, r3, #1             @ prepare for next lower bit
    bne .udiv1                  @ r3 != 0 -> continue
    mov r1, r4                  @ store remainder
    pop {r2 - r4}
  .udiv_quit:
    bx lr

    .size   amx_udiv, .-amx_udiv

    .end
