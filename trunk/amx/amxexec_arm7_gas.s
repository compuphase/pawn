@   amxexec_arm7_gas.s  Abstract Machine for the "Pawn" language
@
@   This file assembles with GUN's AS (GAS). It uses ARM Architecture v4T
@   instructions. It can be assembled for Big Endian environments, by
@   defining the symbol BIG_ENDIAN; the default configuration is
@   Little Endian.
@
@   You will need to compile the standard AMX.C file with the macro
@   ASM32 defined.
@
@   The calling convention conforms to the ARM Architecture Procedure
@   Call Standard (AAPCS). This applies both to the function amx_exec_asm
@   implemented in this file as to the debug hook function, callback hook
@   function and any native functions called directly from the abstract
@   machine.
@
@
@   Copyright (c) ITB CompuPhase, 2006-2008
@
@   This software is provided "as-is", without any express or implied warranty.
@   In no event will the authors be held liable for any damages arising from
@   the use of this software.
@
@   Permission is granted to anyone to use this software for any purpose,
@   including commercial applications, and to alter it and redistribute it
@   freely, subject to the following restrictions:
@
@   1.  The origin of this software must not be misrepresented; you must not
@       claim that you wrote the original software. If you use this software in
@       a product, an acknowledgment in the product documentation would be
@       appreciated but is not required.
@   2.  Altered source versions must be plainly marked as such, and must not be
@       misrepresented as being the original software.
@   3.  This notice may not be removed or altered from any source distribution.
@
@   Version: $Id: $

    .file   "amxexec_arm7_gas.s"
.ifndef AMX_NO_PACKED_OPC
  .ifndef AMX_TOKENTHREADING
    .equ AMX_TOKENTHREADING, 1 @ packed opcodes require token threading
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
    .size   amx_opcodelist, 640
amx_opcodelist:
    .word   .OP_INVALID
    .word   .OP_LOAD_PRI
    .word   .OP_LOAD_ALT
    .word   .OP_LOAD_S_PRI
    .word   .OP_LOAD_S_ALT
    .word   .OP_LREF_PRI
    .word   .OP_LREF_ALT
    .word   .OP_LREF_S_PRI
    .word   .OP_LREF_S_ALT
    .word   .OP_LOAD_I
    .word   .OP_LODB_I
    .word   .OP_CONST_PRI
    .word   .OP_CONST_ALT
    .word   .OP_ADDR_PRI
    .word   .OP_ADDR_ALT
    .word   .OP_STOR_PRI
    .word   .OP_STOR_ALT
    .word   .OP_STOR_S_PRI
    .word   .OP_STOR_S_ALT
    .word   .OP_SREF_PRI
    .word   .OP_SREF_ALT
    .word   .OP_SREF_S_PRI
    .word   .OP_SREF_S_ALT
    .word   .OP_STOR_I
    .word   .OP_STRB_I
    .word   .OP_LIDX
    .word   .OP_LIDX_B
    .word   .OP_IDXADDR
    .word   .OP_IDXADDR_B
    .word   .OP_ALIGN_PRI
    .word   .OP_ALIGN_ALT
    .word   .OP_LCTRL
    .word   .OP_SCTRL
    .word   .OP_MOVE_PRI
    .word   .OP_MOVE_ALT
    .word   .OP_XCHG
    .word   .OP_PUSH_PRI
    .word   .OP_PUSH_ALT
    .word   .OP_PICK
    .word   .OP_PUSH_C
    .word   .OP_PUSH
    .word   .OP_PUSH_S
    .word   .OP_POP_PRI
    .word   .OP_POP_ALT
    .word   .OP_STACK
    .word   .OP_HEAP
    .word   .OP_PROC
    .word   .OP_RET
    .word   .OP_RETN
    .word   .OP_CALL
    .word   .OP_CALL_PRI        @ obsolete
    .word   .OP_JUMP
    .word   .OP_JREL            @ obsolete
    .word   .OP_JZER
    .word   .OP_JNZ
    .word   .OP_JEQ
    .word   .OP_JNEQ
    .word   .OP_JLESS
    .word   .OP_JLEQ
    .word   .OP_JGRTR
    .word   .OP_JGEQ
    .word   .OP_JSLESS
    .word   .OP_JSLEQ
    .word   .OP_JSGRTR
    .word   .OP_JSGEQ
    .word   .OP_SHL
    .word   .OP_SHR
    .word   .OP_SSHR
    .word   .OP_SHL_C_PRI
    .word   .OP_SHL_C_ALT
    .word   .OP_SHR_C_PRI
    .word   .OP_SHR_C_ALT
    .word   .OP_SMUL
    .word   .OP_SDIV
    .word   .OP_SDIV_ALT
    .word   .OP_UMUL
    .word   .OP_UDIV
    .word   .OP_UDIV_ALT
    .word   .OP_ADD
    .word   .OP_SUB
    .word   .OP_SUB_ALT
    .word   .OP_AND
    .word   .OP_OR
    .word   .OP_XOR
    .word   .OP_NOT
    .word   .OP_NEG
    .word   .OP_INVERT
    .word   .OP_ADD_C
    .word   .OP_SMUL_C
    .word   .OP_ZERO_PRI
    .word   .OP_ZERO_ALT
    .word   .OP_ZERO
    .word   .OP_ZERO_S
    .word   .OP_SIGN_PRI
    .word   .OP_SIGN_ALT
    .word   .OP_EQ
    .word   .OP_NEQ
    .word   .OP_LESS
    .word   .OP_LEQ
    .word   .OP_GRTR
    .word   .OP_GEQ
    .word   .OP_SLESS
    .word   .OP_SLEQ
    .word   .OP_SGRTR
    .word   .OP_SGEQ
    .word   .OP_EQ_C_PRI
    .word   .OP_EQ_C_ALT
    .word   .OP_INC_PRI
    .word   .OP_INC_ALT
    .word   .OP_INC
    .word   .OP_INC_S
    .word   .OP_INC_I
    .word   .OP_DEC_PRI
    .word   .OP_DEC_ALT
    .word   .OP_DEC
    .word   .OP_DEC_S
    .word   .OP_DEC_I
    .word   .OP_MOVS
    .word   .OP_CMPS
    .word   .OP_FILL
    .word   .OP_HALT
    .word   .OP_BOUNDS
    .word   .OP_SYSREQ_PRI
    .word   .OP_SYSREQ_C
    .word   .OP_FILE            @ obsolete
    .word   .OP_LINE            @ obsolete
    .word   .OP_SYMBOL          @ obsolete
    .word   .OP_SRANGE          @ obsolete
    .word   .OP_JUMP_PRI        @ obsolete
    .word   .OP_SWITCH
    .word   .OP_CASETBL
    .word   .OP_SWAP_PRI
    .word   .OP_SWAP_ALT
    .word   .OP_PUSH_ADR
    .word   .OP_NOP
    .word   .OP_SYSREQ_N
    .word   .OP_SYMTAG          @ obsolete
    .word   .OP_BREAK
    @ macro opcodes
    .word   .OP_PUSH2_C
    .word   .OP_PUSH2
    .word   .OP_PUSH2_S
    .word   .OP_PUSH2_ADR
    .word   .OP_PUSH3_C
    .word   .OP_PUSH3
    .word   .OP_PUSH3_S
    .word   .OP_PUSH3_ADR
    .word   .OP_PUSH4_C
    .word   .OP_PUSH4
    .word   .OP_PUSH4_S
    .word   .OP_PUSH4_ADR
    .word   .OP_PUSH5_C
    .word   .OP_PUSH5
    .word   .OP_PUSH5_S
    .word   .OP_PUSH5_ADR
    .word   .OP_LOAD_BOTH
    .word   .OP_LOAD_S_BOTH
    .word   .OP_CONST
    .word   .OP_CONST_S
    @ overlay opcodes
    .word   .OP_ICALL
    .word   .OP_IRETN
    .word   .OP_ISWITCH
    .word   .OP_ICASETBL
    @ packed opcodes
.ifndef AMX_NO_PACKED_OPC
    .word   .OP_LOAD_P_PRI
    .word   .OP_LOAD_P_ALT
    .word   .OP_LOAD_P_S_PRI
    .word   .OP_LOAD_P_S_ALT
    .word   .OP_LREF_P_PRI
    .word   .OP_LREF_P_ALT
    .word   .OP_LREF_P_S_PRI
    .word   .OP_LREF_P_S_ALT
    .word   .OP_LODB_P_I
    .word   .OP_CONST_P_PRI
    .word   .OP_CONST_P_ALT
    .word   .OP_ADDR_P_PRI
    .word   .OP_ADDR_P_ALT
    .word   .OP_STOR_P_PRI
    .word   .OP_STOR_P_ALT
    .word   .OP_STOR_P_S_PRI
    .word   .OP_STOR_P_S_ALT
    .word   .OP_SREF_P_PRI
    .word   .OP_SREF_P_ALT
    .word   .OP_SREF_P_S_PRI
    .word   .OP_SREF_P_S_ALT
    .word   .OP_STRB_P_I
    .word   .OP_LIDX_P_B
    .word   .OP_IDXADDR_P_B
    .word   .OP_ALIGN_P_PRI
    .word   .OP_ALIGN_P_ALT
    .word   .OP_PUSH_P_C
    .word   .OP_PUSH_P
    .word   .OP_PUSH_P_S
    .word   .OP_STACK_P
    .word   .OP_HEAP_P
    .word   .OP_SHL_P_C_PRI
    .word   .OP_SHL_P_C_ALT
    .word   .OP_SHR_P_C_PRI
    .word   .OP_SHR_P_C_ALT
    .word   .OP_ADD_P_C
    .word   .OP_SMUL_P_C
    .word   .OP_ZERO_P
    .word   .OP_ZERO_P_S
    .word   .OP_EQ_P_C_PRI
    .word   .OP_EQ_P_C_ALT
    .word   .OP_INC_P
    .word   .OP_INC_P_S
    .word   .OP_DEC_P
    .word   .OP_DEC_P_S
    .word   .OP_MOVS_P
    .word   .OP_CMPS_P
    .word   .OP_FILL_P
    .word   .OP_HALT_P
    .word   .OP_BOUNDS_P
    .word   .OP_PUSH_P_ADR
.endif  @ AMX_NO_PACKED_OPC
    @ patch opcodes
    .word   .OP_SYSREQ_D
    .word   .OP_SYSREQ_ND


.macro NEXT
  .ifdef AMX_TOKENTHREADING
    .ifdef AMX_NO_PACKED_OPC
      ldr r11, [r4], #4         @ get opcode, increment CIP
    .else
      ldr r12, [r4], #4         @ get opcode + parameter, increment CIP
      and r11, r12, #0xff       @ keep only the opcode in r11, r12 holds the parameter
    .endif
    ldr pc, [r14, r11, LSL #2]
  .else
    .ifndef AMX_NO_PACKED_OPC
      .err                      @ opcode packing requires token threading
    .endif
    ldr pc, [r4], #4            @ indirect register jump
  .endif
.endm

.macro GETPARAM rx
    ldr \rx, [r4], #4           @ \rx = [CIP], CIP += 4
.endm

.macro GETPARAM_P rx            @ the opcode/parameter pack should be in r12
    mov \rx, r12, ASR #16       @ \rx = r12 >> 16 (signed)
.endm

.macro JUMPREL cc, rtmp         @ \cc = condition code, \rtmp = temp register to use
    ldr\cc \rtmp, [r4], #-4     @ $rtmp = [CIP], CIP -= 4 (restore CIP to start of instruction)
    add\cc r4, r4, \rtmp        @ CIP = CIP + [CIP] - 4
.endm

.macro PUSH rx
    str \rx, [r6, #-4]!         @ STK -= 4, [STK] = \rx
.endm

.macro POP rx
    ldr \rx, [r6], #4           @ \rx = [STK], STK += 4
.endm

.macro CHKMARGIN rtmp           @ \rtmp = temp register to use
    add \rtmp, r3, #64          @ 64 = 16 cells
    cmp \rtmp, r6               @ compare HEA + 16*cell to STK
    movhi r11, #AMX_ERR_STACKERR
    bhi .amx_exit
.endm

.macro CHKSTACK
    cmp r6, r2                  @ compare STK to STP
    movhi r11, #AMX_ERR_STACKLOW
    bhi .amx_exit
.endm

.macro CHKHEAP rtmp             @ \rtmp = temp register to use
    ldr \rtmp, [r10, #amxHLW]
    cmp r3, \rtmp               @ compare HEA to HLW
    movlo r11, #AMX_ERR_HEAPLOW
    blo .amx_exit
.endm

.macro VERIFYADDRESS rx
    cmp \rx, r2                 @ \rx >= STP ?
    bhs .err_memaccess          @ yes -> error
    @ One might want to relax the test and remove the three instructions below.
    @ If a register points into the "free" area between the stack and the heap,
    @ it does not breach the sandbox.
    cmp r3, \rx                 @ HEA <= \rx ?
    cmpls \rx, r6               @ yes, then: \rx < STK ?
    blo .err_memaccess          @ yes, then HEA <= \rx && \rx < STK (invalid area)
.endm


@ ----------------------------------------------------------------
@ cell amx_exec_asm(AMX *amx, cell *retval, char *data)
@                        r0         r1            r2
@ ----------------------------------------------------------------

    .text
    .align  2

amx_opcodelist_addr:
    .word   amx_opcodelist

    .global amx_exec_asm
    .type   amx_exec_asm, %function
amx_exec_asm:
    @ save non-scratch registers
    stmfd sp!, {r4 - r12, lr}

    @ save register that holds the address for the return value
    @ we only need this at the point of returning, so it would be
    @ a waste to keep it in a register
    str r1, [sp, #-4]!

    @ set up the registers
    @ r0  = PRI
    @ r1  = ALT
    @ r2  = STP (stack top), relocated (absolute address)
    @ r3  = HEA, relocated (absolute address)
    @ r4  = CIP
    @ r5  = data section (passed in r2)
    @ r6  = STK, relocated (absolute address)
    @ r7  = FRM, relocated (absolute address)
    @ r8  = code
    @ r9  = code_end = code + code_size
    @ r10 = amx base (passed in r0)
    @ r14 = opcode list address (for token threading)
    @ r11 and r12 are scratch; r11 is used in the macro to fetch the next opcode
    @ and r12 is also used there in the case that packed opcodes are supported

    mov r10, r0                 @ r10 = AMX
    mov r5, r2                  @ r5 = data section
    ldr r0, [r10, #amxPRI]
    ldr r1, [r10, #amxALT]
    ldr r2, [r10, #amxSTP]
    ldr r3, [r10, #amxHEA]
    ldr r4, [r10, #amxCIP]
    ldr r6, [r10, #amxSTK]
    ldr r7, [r10, #amxFRM]
    ldr r8, [r10, #amxCode]
    ldr r9, [r10, #amxCodeSize]

    add r2, r2, r5              @ relocate STP (absolute address)
    add r3, r3, r5              @ relocate HEA
    add r6, r6, r5              @ relocate STK
    add r7, r7, r5              @ relocate FRM
    add r4, r4, r8              @ relocate CIP
    add r9, r9, r8              @ make r9 point behind last valid code address

    ldr r14, amx_opcodelist_addr @ N.B. r14 is an alias for lr

    @ start running
    NEXT

.OP_INVALID:
    mov r11, #AMX_ERR_INVINSTR
    b   .amx_exit

.OP_LOAD_PRI:                   @ tested
    GETPARAM r11
    ldr r0, [r5, r11]
    NEXT

.OP_LOAD_ALT:
    GETPARAM r11
    ldr r1, [r5, r11]
    NEXT

.OP_LOAD_S_PRI:                 @ tested
    GETPARAM r11
    ldr r0, [r7, r11]
    NEXT

.OP_LOAD_S_ALT:                 @ tested
    GETPARAM r11
    ldr r1, [r7, r11]
    NEXT

.OP_LREF_PRI:                   @ not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    ldr r0, [r5, r11]
    NEXT

.OP_LREF_ALT:                   @ not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    ldr r1, [r5, r11]
    NEXT

.OP_LREF_S_PRI:                 @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    ldr r0, [r5, r11]
    NEXT

.OP_LREF_S_ALT:
    GETPARAM r11
    ldr r11, [r7, r11]
    ldr r1, [r5, r11]
    NEXT

.OP_LOAD_I:                     @ tested
    add r12, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

.OP_LODB_I:                     @ tested
    add r12, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r12
    GETPARAM r11
    teq r11, #1
    ldreqb r0, [r12]
    teq r11, #2
    ldreqh r0, [r12]
    teq r11, #4
    ldreq r0, [r12]
    NEXT

.OP_CONST_PRI:                  @ tested
    GETPARAM r0
    NEXT

.OP_CONST_ALT:                  @ tested
    GETPARAM r1
    NEXT

.OP_ADDR_PRI:                   @ tested
    GETPARAM r0
    add r0, r0, r7              @ add FRM
    sub r0, r0, r5              @ reverse relocate
    NEXT

.OP_ADDR_ALT:                   @ tested
    GETPARAM r1
    add r1, r1, r7              @ add FRM
    sub r1, r1, r5              @ reverse relocate
    NEXT

.OP_STOR_PRI:                   @ tested
    GETPARAM r11
    str r0, [r5, r11]
    NEXT

.OP_STOR_ALT:
    GETPARAM r11
    str r1, [r5, r11]
    NEXT

.OP_STOR_S_PRI:                 @ tested
    GETPARAM r11
    str r0, [r7, r11]
    NEXT

.OP_STOR_S_ALT:                 @ not generated by pawncc
    GETPARAM r11
    str r1, [r7, r11]
    NEXT

.OP_SREF_PRI:                   @ not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    str r0, [r5, r11]
    NEXT

.OP_SREF_ALT:                   @ not generated by pawncc
    GETPARAM r11
    ldr r11, [r5, r11]
    str r1, [r5, r11]
    NEXT

.OP_SREF_S_PRI:                 @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    str r0, [r5, r11]
    NEXT

.OP_SREF_S_ALT:                 @ not generated by pawncc
    GETPARAM r11
    ldr r11, [r7, r11]
    str r1, [r5, r11]
    NEXT

.OP_STOR_I:                     @ tested
    add r12, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r12
    str r0, [r12]
    NEXT

.OP_STRB_I:                     @ tested
    add r12, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r12
    GETPARAM r11
    teq r11, #1
    streqb r0, [r12]
    teq r11, #2
    streqh r0, [r12]
    teq r11, #4
    streq r0, [r12]
    NEXT

.OP_LIDX:                       @ tested
    add r12, r1, r0, LSL #2     @ r12 = ALT + 4*PRI
    add r12, r12, r5            @ relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

.OP_LIDX_B:
    GETPARAM r11
    add r12, r1, r0, LSL r11    @ r12 = ALT + (PRI << param)
    add r12, r12, r5            @ relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

.OP_IDXADDR:                    @ tested
    add r0, r1, r0, LSL #2      @ PRI = ALT + 4*PRI
    NEXT

.OP_IDXADDR_B:
    GETPARAM r11
    add r0, r1, r0, LSL r11     @ PRI = ALT + (PRI << param)
    NEXT

.OP_ALIGN_PRI:                  @ tested
    GETPARAM r11
.ifndef BIG_ENDIAN
    rsbs r11, r11, #4           @ rsb = reverse subtract; r11 = #4 - param
    eorhi r0, r0, r11           @ PRI ^= (#4 - param), but only if (#4 - param) > 0
.endif
    NEXT

.OP_ALIGN_ALT:
    GETPARAM r11
.ifndef BIG_ENDIAN
    rsbs r11, r11, #4           @ rsb = reverse subtract; r11 = #4 - param
    eorhi r1, r1, r11           @ ALT ^= (#4 - param), but only if (#4 - param) > 0
.endif
    NEXT

.OP_LCTRL:
    GETPARAM r11                @ code of value to get
    teq r11, #0
    moveq r0, r8                @ 0 == code base address
    teq r11, #1
    moveq r0, r5                @ 1 == data base address
    teq r11, #2
    subeq r0, r3, r5            @ 2 == HEA (reverse relocated)
    teq r11, #3
    subeq r0, r2, r5            @ 3 == STP (reverse relocated)
    teq r11, #4
    subeq r0, r6, r5            @ 4 == STK (reverse relocated)
    teq r11, #5
    subeq r0, r7, r5            @ 5 == FRM (reverse relocated)
    teq r11, #6
    subeq r0, r4, r8            @ 6 == CIP (relative to code)
    NEXT

.OP_SCTRL:
    GETPARAM r11                @ code of value to get
    teq r11, #2
    addeq r3, r0, r5            @ 2 == HEA (relocated)
    teq r11, #4
    addeq r6, r0, r5            @ 4 == STK (reverse relocated)
    teq r11, #5
    addeq r7, r0, r5            @ 5 == FRM (reverse relocated)
    teq r11, #6
    subeq r4, r0, r8            @ 6 == CIP (relative to code)
    NEXT

.OP_MOVE_PRI:                   @ tested
    mov r0, r1
    NEXT

.OP_MOVE_ALT:                   @ tested
    mov r1, r0
    NEXT

.OP_XCHG:                       @ tested
    mov r11, r0
    mov r0, r1
    mov r1, r11
    NEXT

.OP_PUSH_PRI:                   @ tested
    PUSH r0
    NEXT

.OP_PUSH_ALT:                   @ tested
    PUSH r1
    NEXT

.OP_PUSH_C:                     @ tested
    GETPARAM r11
    PUSH r11
    NEXT

.OP_PUSH:
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    NEXT

.OP_PUSH_S:                     @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    NEXT

.OP_POP_PRI:
    POP r0
    NEXT

.OP_POP_ALT:                    @ tested
    POP r1
    NEXT

.OP_STACK:                      @ tested
    GETPARAM r11
    sub r1, r6, r5              @ ALT = STK, reverse-relocated
    add r6, r6, r11             @ STK += param
    CHKMARGIN r12
    CHKSTACK
    NEXT

.OP_HEAP:                       @ tested
    GETPARAM r11
    sub r1, r3, r5              @ ALT = HEA, reverse-relocated
    add r3, r3, r11
    CHKMARGIN r12
    CHKHEAP r12
    NEXT

.OP_PROC:                       @ tested
    PUSH r7
    mov r7, r6                  @ FRM = stk
    CHKMARGIN r12
    NEXT

.OP_RET:
    POP r7                      @ pop FRM
    POP r4                      @ pop CIP (return address)
    @ verify return address (avoid stack/buffer overflow)
    cmp r4, r8                  @ return address >= code_start ?
    blo .err_memaccess          @ no, error
    cmp r4, r9                  @ return addres < code_end ?
    bhs .err_memaccess          @ no, error
    @ all tests passed
    NEXT

.OP_RETN:                       @ tested
    POP r7                      @ pop FRM
    POP r4                      @ pop CIP (return address)
    @ verify return address (avoid stack/buffer overflow)
    cmp r4, r8                  @ return address >= code_start ?
    blo .err_memaccess          @ no, error
    cmp r4, r9                  @ return addres < code_end ?
    bhs .err_memaccess          @ no, error
    @ all tests passed
    ldr r11, [r6], #4           @ read value at the stack (#args passed to func), add 4 to STK
    add r6, r6, r11             @ STK += #args (+ 4, added in the LDR instruction)
    NEXT

.err_memaccess:
    mov r11, #AMX_ERR_MEMACCESS
    b   .amx_exit

.OP_CALL:                       @ tested
    add r11, r4, #4             @ r11 = address of next instruction
    PUSH r11
    JUMPREL al, r11             @ cc = always
    NEXT

.OP_ICALL:
    add r11, r4, #4             @ r11 = address of next instruction (absolute)
    sub r11, r11, r8            @ r11 = relative address (to start of code segment)
    ldr r12, [r10, #amxOvlIndex]@ r12 = overlay index
    add r11, r12, r11, LSL #16  @ r11 = (address << 16) + ovl_index
    PUSH r11
    ldr r12, [r4]               @ r12 = [CIP] = param of ICALL = new overlay index
    str r12, [r10, #amxOvlIndex]
    stmfd sp!, {r0 - r3, lr}    @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r12                 @ 2nd arg = overlay index
    ldr r11, [r10, #amxOverlay] @ callback function pointer in r11
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    ldmfd sp!, {r0 - r3, lr}    @ restore registers
    ldr r8, [r10, #amxCode]     @ r8 = code pointer (base)
    mov r4, r8                  @ CIP = code base
    NEXT

.OP_IRETN:
    POP r7                      @ pop FRM
    POP r4                      @ pop relative CIP (return address) + overlay index
    ldr r11, [r6], #4           @ read value at the stack (#args passed to func), add 4 to STK
    add r6, r6, r11             @ STK += #args (+ 4, added in the LDR instruction)
    stmfd sp!, {r0 - r3, lr}    @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mvn r1, #0                  @ r1 = 0xffffffff
    mov r1, r1, LSR #16         @ r1 = 0x0000ffff
    and r1, r4, r1              @ 2nd arg = overlay index
    str r1, [r10, #amxOvlIndex] @ store new overlay index too
    ldr r11, [r10, #amxOverlay] @ callback function pointer in r11
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    ldmfd sp!, {r0 - r3, lr}    @ restore registers
    ldr r8, [r10, #amxCode]     @ r8 = code pointer (base)
    add r4, r8, r4, LSR #16     @ r4 = base address + relative address
    NEXT

.OP_JUMP:                       @ tested
    JUMPREL al, r11             @ cc = always
    NEXT

.OP_JZER:                       @ tested
    cmp r0, #0
    JUMPREL eq, r11             @ if PRI == 0, jump
    addne r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JNZ:                        @ tested
    cmp r0, #0
    JUMPREL ne, r11             @ if PRI != 0, jump
    addeq r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JEQ:                        @ tested
    cmp r0, r1
    JUMPREL eq, r11             @ if PRI == ALT, jump
    addne r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JNEQ:                       @ tested
    cmp r0, r1
    JUMPREL ne, r11             @ if PRI != ALT, jump
    addeq r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JLESS:                      @ not generated by pawncc
    cmp r0, r1
    JUMPREL lo, r11             @ if PRI < ALT (unsigned), jump
    addhs r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JLEQ:                       @ not generated by pawncc
    cmp r0, r1
    JUMPREL ls, r11             @ if PRI <= ALT (unsigned), jump
    addhi r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JGRTR:                      @ not generated by pawncc
    cmp r0, r1
    JUMPREL hi, r11             @ if PRI > ALT (unsigned), jump
    addls r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JGEQ:                       @ not generated by pawncc
    cmp r0, r1
    JUMPREL hs, r11             @ if PRI >= ALT (unsigned), jump
    addlo r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JSLESS:                     @ tested
    cmp r0, r1
    JUMPREL lt, r11             @ if PRI < ALT (signed), jump
    addge r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JSLEQ:                      @ tested
    cmp r0, r1
    JUMPREL le, r11             @ if PRI <= ALT (signed), jump
    addgt r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JSGRTR:                     @ tested
    cmp r0, r1
    JUMPREL gt, r11             @ if PRI > ALT (signed), jump
    addle r4, r4, #4            @ otherwise skip param
    NEXT

.OP_JSGEQ:                      @ tested
    cmp r0, r1
    JUMPREL ge, r11             @ if PRI >= ALT (signed), jump
    addlt r4, r4, #4            @ otherwise skip param
    NEXT

.OP_SHL:                        @ tested
    mov r0, r0, LSL r1          @ PRI = PRI << ALT
    NEXT

.OP_SHR:                        @ tested
    cmp r1, #0
    movhi r0, r0, LSR r1        @ PRI = PRI >> ALT (but check that ALT > 0)
    NEXT

.OP_SSHR:                       @ tested
    cmp r1, #0
    movhi r0, r0, ASR r1        @ PRI = PRI >> ALT (but check that ALT > 0)
    NEXT

.OP_SHL_C_PRI:
    GETPARAM r11
    mov r0, r0, LSL r11         @ PRI = PRI << param
    NEXT

.OP_SHL_C_ALT:
    GETPARAM r11
    mov r1, r1, LSL r11         @ ALT = ALT << param
    NEXT

.OP_SHR_C_PRI:
    GETPARAM r11
    cmp r11, #0
    movhi r0, r0, LSR r11       @ PRI = PRI >> param (but check that param > 0)
    NEXT

.OP_SHR_C_ALT:
    GETPARAM r11
    cmp r11, #0
    movhi r1, r1, LSR r11       @ ALT = ALT >> param (but check that param > 0)
    NEXT

.OP_SMUL:                       @ tested
.OP_UMUL:                       @ tested (not generated by pawncc)
    mov r11, r0                 @ copy r0
    mul r0, r11, r1             @ dest must be different from source registers
    NEXT

.OP_SDIV_ALT:                   @ tested
    @ swap r0 and r1, then drop into normal (signed) division case
    mov r11, r0
    mov r0, r1
    mov r1, r11
.OP_SDIV:                       @ tested (not generated by pawncc)
    teq r1, #0                  @ verify r1 (divisor)
    moveq r11, #AMX_ERR_DIVIDE  @ r1 == 0 -> set error code
    beq .amx_exit               @ r1 == 0 -> jump to error-exit
    stmfd sp!, {r2 - r3, lr}    @ need two more scratch registers
    @ save input registers and create absolute values
    movs r2, r0
    rsbmi r0, r0, #0            @ if r2 < 0, r0 = #0 - r0
    movs r3, r1
    rsbmi r1, r1, #0            @ if r3 < 0, r1 = #0 - r1
    @ do the division
    bl  amx_div                 @ r0 = abs(PRI) / abs(ALT), r1 = abs(PRI) % abs(ALT)
    @ patch signs
    cmp r3, #0                  @ check sign of original value of ALT (divisor)
    rsbmi r1, r1, #0            @ sign(remainder) = sign(divisor)
    teq r2, r3                  @ check signs of dividend and divisor
    bpl .op_div_done            @ sign(divident) == sign(divisor) -> done
    rsb r0, r0, #0              @ sign(quotient) = sign(divident) XOR sign(divisor)
    @ so the quotient is negative (or zero); if the remainder is non-zero,
    @ floor the quotient and adjust the remainder
    cmp r1, #0
    subne r0, r0, #1            @ remainder != 0 -> r0 = r0 - 1
    rsbne r1, r1, r3            @ remainder != 0 -> r1 = divisor - r1
.op_div_done:
    ldmfd sp!, {r2 - r3, lr}
    NEXT

.OP_UDIV_ALT:                   @ not generated by pawncc
    @ swap r0 and r1, then drop into normal (unsigned) division case
    mov r11, r0
    mov r0, r1
    mov r1, r11
.OP_UDIV:                       @ not generated by pawncc
    teq r1, #0                  @ verify r1 (divisor)
    moveq r11, #AMX_ERR_DIVIDE  @ r1 == 0 -> set error code
    beq .amx_exit               @ r1 == 0 -> jump to error-exit
    str lr, [sp, #-4]!          @ save lr (r14)
    bl  amx_div                 @ PRI = PRI / ALT, ALT = PRI % ALT
    ldr lr, [sp], #4            @ restore lr
    NEXT

.OP_ADD:                        @ tested
    add r0, r0, r1
    NEXT

.OP_SUB:
    sub r0, r0, r1
    NEXT

.OP_SUB_ALT:                    @ tested
    sub r0, r1, r0
    NEXT

.OP_AND:                        @ tested
    and r0, r0, r1
    NEXT

.OP_OR:                         @ tested
    orr r0, r0, r1
    NEXT

.OP_XOR:                        @ tested
    eor r0, r0, r1
    NEXT

.OP_NOT:                        @ tested
    teq r0, #0
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_NEG:                        @ tested
    rsb r0, r0, #0              @ PRI = #0 - PRI
    NEXT

.OP_INVERT:                     @ tested
    mvn r0, r0                  @ PRI = NOT PRI (all bits inverted)
    NEXT

.OP_ADD_C:                      @ tested
    GETPARAM r11
    add r0, r0, r11             @ PRI += param
    NEXT

.OP_SMUL_C:                     @ tested
    GETPARAM r11
    mov r12, r0
    mul r0, r11, r12            @ PRI *= param
    NEXT

.OP_ZERO_PRI:                   @ tested
    mov r0, #0
    NEXT

.OP_ZERO_ALT:
    mov r1, #0
    NEXT

.OP_ZERO:                       @ tested
    GETPARAM r11
    mov r12, #0
    str r12, [r5, r11]
    NEXT

.OP_ZERO_S:                     @ tested
    GETPARAM r11
    mov r12, #0
    str r12, [r7, r11]
    NEXT

.OP_SIGN_PRI:                   @ not generated by pawncc
    tst r0, #0x80               @ bit 7 set?
    mvnne r11, #0xff            @ sets 0xffffff00
    orrne r0, r0, r11           @ yes, sign-extend to the top 24 bits
    NEXT

.OP_SIGN_ALT:                   @ not generated by pawncc
    tst r1, #0x80               @ bit 7 set?
    mvnne r11, #0xff            @ sets 0xffffff00
    orrne r1, r1, r11           @ yes, sign-extend to the top 24 bits
    NEXT

.OP_EQ:
    cmp r0, r1
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_NEQ:
    cmp r0, r1
    movne r0, #1
    moveq r0, #0
    NEXT

.OP_LESS:                       @ not generated by pawncc
    cmp r0, r1
    movlo r0, #1
    movhs r0, #0
    NEXT

.OP_LEQ:                        @ not generated by pawncc
    cmp r0, r1
    movls r0, #1
    movhi r0, #0
    NEXT

.OP_GRTR:                       @ not generated by pawncc
    cmp r0, r1
    movhi r0, #1
    movls r0, #0
    NEXT

.OP_GEQ:                        @ not generated by pawncc
    cmp r0, r1
    movhs r0, #1
    movlo r0, #0
    NEXT

.OP_SLESS:                      @ tested
    cmp r0, r1
    movlt r0, #1
    movge r0, #0
    NEXT

.OP_SLEQ:                       @ tested
    cmp r0, r1
    movle r0, #1
    movgt r0, #0
    NEXT

.OP_SGRTR:                      @ tested
    cmp r0, r1
    movgt r0, #1
    movle r0, #0
    NEXT

.OP_SGEQ:                       @ tested
    cmp r0, r1
    movge r0, #1
    movlt r0, #0
    NEXT

.OP_EQ_C_PRI:                   @ tested
    GETPARAM r11
    cmp r0, r11
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_EQ_C_ALT:
    GETPARAM r11
    cmp r1, r11
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_INC_PRI:
    add r0, r0, #1
    NEXT

.OP_INC_ALT:
    add r1, r1, #1
    NEXT

.OP_INC:                        @ tested
    GETPARAM r11
    ldr r12, [r5, r11]
    add r12, r12, #1
    str r12, [r5, r11]
    NEXT

.OP_INC_S:                      @ tested
    GETPARAM r11
    ldr r12, [r7, r11]
    add r12, r12, #1
    str r12, [r7, r11]
    NEXT

.OP_INC_I:                      @ tested
    ldr r12, [r5, r0]
    add r12, r12, #1
    str r12, [r5, r0]
    NEXT

.OP_DEC_PRI:
    sub r0, r0, #1
    NEXT

.OP_DEC_ALT:
    sub r1, r1, #1
    NEXT

.OP_DEC:                        @ tested
    GETPARAM r11
    ldr r12, [r5, r11]
    sub r12, r12, #1
    str r12, [r5, r11]
    NEXT

.OP_DEC_S:                      @ tested
    GETPARAM r11
    ldr r12, [r7, r11]
    sub r12, r12, #1
    str r12, [r7, r11]
    NEXT

.OP_DEC_I:                      @ tested
    ldr r12, [r5, r0]
    sub r12, r12, #1
    str r12, [r5, r0]
    NEXT

.OP_MOVS:                       @ tested
    GETPARAM r11
.movsentry:
    sub r11, r11, #1            @ decrement, for address verification
    add r12, r0, r5             @ r12 = relocated PRI
    VERIFYADDRESS r12           @ verify PRI (after relocation)
    add r12, r12, r11
    VERIFYADDRESS r12           @ verify PRI + size - 1
    add r12, r1, r5             @ r12 = relocated ALT
    VERIFYADDRESS r12           @ verify ALT (after relocation)
    add r12, r12, r11
    VERIFYADDRESS r12           @ verify ALT + size - 1
    @ dropped through tests
    add r11, r11, #1            @ restore r11 (# bytes to move)
    str r0, [sp, #-4]!          @ save PRI and ALT
    str r1, [sp, #-4]!
    add r0, r0, r5              @ relocate r0/r1
    add r1, r1, r5
.movs4loop:
    subs r11, r11, #4
    ldrge r12, [r0], #4
    strge r12, [r1], #4
    bgt .movs4loop
.movs1loop:
    subs r11, r11, #1
    ldrgeb r12, [r0], #1
    strgeb r12, [r1], #1
    bgt .movs1loop
    @ restore PRI and ALT
    ldr r1, [sp], #4            @ restore PRI and ALT
    ldr r0, [sp], #4
    NEXT

.OP_CMPS:
    GETPARAM r11
.cmpsentry:
    sub r11, r11, #1            @ decrement, for address verification
    add r12, r0, r5             @ r12 = relocated PRI
    VERIFYADDRESS r12           @ verify PRI
    add r12, r12, r11
    VERIFYADDRESS r12           @ verify PRI + size - 1
    add r12, r1, r5             @ r12 = relocated ALT
    VERIFYADDRESS r12           @ verify ALT
    add r12, r12, r11
    VERIFYADDRESS r12           @ verify ALT + size - 1
    @ dropped through tests
    str r14, [sp, #-4]!         @ need extra register
    add r11, r11, #1            @ restore r11
    str r1, [sp, #-4]!          @ save ALT
    add r0, r0, r5              @ relocate r0 and r1
    add r1, r1, r5
    mov r14, #0                 @ preset r14, in case r11 == 0
.cmps4loop:
    subs r11, r11, #4
    addmi r11, r11, #4
    bmi .cmps1loop
    ldr r14, [r0], #4
    ldr r12, [r1], #4
    subs r14, r14, r12          @ r14 = [PRI] - [ALT]
    bne .cmpsdone
    b .cmps4loop
.cmps1loop:
    subs r11, r11, #1
    bmi .cmpsdone
    ldrb r14, [r0], #1
    ldrb r12, [r1], #1
    subs r14, r14, r12          @ r14 = [PRI] - [ALT]
    beq .cmps1loop
.cmpsdone:
    ldr r1, [sp], #4            @ restore ALT (PRI is changed by this routine)
    mov r0, r14
    ldr r14, [sp], #4           @ restore r14
    NEXT

.OP_FILL:                       @ tested
    GETPARAM r11
.fillentry:
    add r12, r1, r5             @ r12 = relocated ALT
    VERIFYADDRESS r12           @ verify ALT
    add r12, r12, r11
    sub r12, r12, #1
    VERIFYADDRESS r12           @ verify ALT + size - 1
    @ dropped through tests
    add r12, r1, r5             @ r12 = relocated ALT again
.fill4loop:
    subs r11, r11, #4
    strge r0, [r12], #4
    bgt .fill4loop
    NEXT

.OP_HALT:                       @ tested
    ldr r11, [sp]               @ get "retval" pointer
    teq r11, #0
    strne r0, [r11]             @ store PRI, but only if r11 != 0
    GETPARAM r11                @ parameter = return code from function
    b   .amx_exit

.OP_BOUNDS:                     @ tested
    GETPARAM r11
    cmp r0, r11
    movhi r11, #AMX_ERR_BOUNDS
    bhi .amx_exit
    NEXT

.OP_SYSREQ_C:                   @ tested
    GETPARAM r0                 @ native function index in r0
.OP_SYSREQ_PRI:                 @ tested (not generated by pawncc)
    @ store stack and heap state AMX state
    sub r11, r7, r5             @ reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             @ reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             @ reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             @ reverse-relocate CIP
    str r11, [r10, #amxCIP]
    @ invoke callback
    stmfd sp!, {r1 - r3, lr}    @ save some extra registers
    sub sp, sp, #4              @ reserve a cell on the stack for the return value
    mov r1, r0                  @ 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 @ 1st arg = AMX
    mov r2, sp                  @ 3rd arg = address of return value
    mov r3, r6                  @ 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]@ callback function pointer in r11
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    mov	r11, r0                 @ get error return in r11
    ldr r0, [sp], #4            @ get return value, restore stack
    ldmfd sp!, {r1 - r3, lr}    @ restore registers
    teq r11, #AMX_ERR_NONE      @ callback hook returned error/abort code?
    bne .amx_exit               @ yes -> quit
    NEXT

.OP_SYSREQ_N:                   @ tested
    GETPARAM r0                 @ get native function index
    GETPARAM r12                @ get # parameters
    PUSH r12                    @ push second parameter
    @ store stack and heap state AMX state
    sub r11, r7, r5             @ reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             @ reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             @ reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             @ reverse-relocate CIP
    str r11, [r10, #amxCIP]
    @ invoke callback
    stmfd sp!, {r1 - r3, r12, lr} @ save some extra registers
    sub sp, sp, #4              @ reserve a cell on the stack for the return value
    mov r1, r0                  @ 2nd arg = index (in r0, so do this one first)
    mov r0, r10                 @ 1st arg = AMX
    mov r2, sp                  @ 3rd arg = address of return value
    mov r3, r6                  @ 4th arg = address in the AMX stack
    ldr r11, [r10, #amxCallback]@ callback function pointer in r11
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    mov	r11, r0                 @ get error return in r11
    ldr r0, [sp], #4            @ get return value, restore ARM stack
    ldmfd sp!, {r1 - r3, r12, lr} @ restore registers
    add r6, r6, r12             @ remove # parameters from the AMX stack
    add r6, r6, #4              @ also remove the extra cell pushed
    teq r11, #AMX_ERR_NONE      @ callback hook returned error/abort code?
    bne .amx_exit               @ yes -> quit
    NEXT

.OP_SYSREQ_D:                   @ tested
    GETPARAM r12                @ address of native function in r12
    @ store stack and heap state AMX state
    sub r11, r7, r5             @ reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             @ reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             @ reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             @ reverse-relocate CIP
    str r11, [r10, #amxCIP]
    @ invoke callback (address still in r12)
    stmfd sp!, {r1 - r3, lr}    @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r6                  @ 2nd arg = address in the AMX stack
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r12
    ldmfd sp!, {r1 - r3, lr}    @ restore registers
    ldr r11, [r10, #amxError]   @ get error returned by native function
    teq r11, #AMX_ERR_NONE      @ callback hook returned error/abort code?
    bne .amx_exit               @ yes -> quit
    NEXT

.OP_SYSREQ_ND:                  @ tested
    GETPARAM r0                 @ address of native function in r0 (temporary)
    GETPARAM r12                @ get # parameters
    PUSH r12                    @ push second parameter
    @ store stack and heap state AMX state
    sub r11, r7, r5             @ reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             @ reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             @ reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             @ reverse-relocate CIP
    str r11, [r10, #amxCIP]
    @ invoke callback (address still in r0)
    mov r11, r0                 @ get address to call in r11 (r0 is overwritten)
    stmfd sp!, {r1 - r3, r12, lr} @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r6                  @ 2nd arg = address in the AMX stack
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    ldmfd sp!, {r1 - r3, r12, lr} @ restore registers
    add r6, r6, r12             @ remove # parameters from the AMX stack
    add r6, r6, #4              @ also remove the extra cell pushed
    ldr r11, [r10, #amxError]   @ get error returned by native function
    teq r11, #AMX_ERR_NONE      @ callback hook returned error/abort code?
    bne .amx_exit               @ yes -> quit
    NEXT

.OP_PICK:
    GETPARAM r11
    ldr r0, [r6, r11]
    NEXT

.OP_JREL:
.OP_FILE:
.OP_LINE:
.OP_SYMBOL:
.OP_SRANGE:
.OP_SYMTAG:
.OP_JUMP_PRI:
.OP_CALL_PRI:
.OP_CASETBL:
.OP_ICASETBL:
    mov r11, #AMX_ERR_INVINSTR  @ these instructions are no longer supported
    b   .amx_exit

.OP_SWITCH:                     @ tested
    str r14, [sp, #-4]!         @ need extra register
    ldr r11, [r4]               @ r11 = [CIP], relative offset to case-table
    add r11, r11, r4            @ r11 = direct address, OP_CASETBL opcode already skipped
    ldr r12, [r11]              @ r12 = number of case-table records
    ldr r14, [r11, #4]          @ preset CIP to "default" case (none-matched)
    add r4, r11, r14
.op_switch_loop:
    subs r12, r12, #1           @ decrement number to do; any left?
    bmi .op_switch_done         @ no, quit (CIP already set to the default value)
    add r11, r11, #8            @ move to next record
    ldr r14, [r11]              @ get the case value
    cmp r0, r14                 @ case value identical to PRI ?
    bne .op_switch_loop         @ no, continue
    ldr r14, [r11, #4]          @ yes, load matching CIP and exit loop
    add r4, r11, r14            @ r4 = address of case record + offset
.op_switch_done:
    ldr r14, [sp], #4           @ restore r14
    NEXT

.OP_ISWITCH:
    str r14, [sp, #-4]!         @ need extra register
    ldr r11, [r4]               @ r11 = [CIP], relative offset to case-table
    add r11, r11, r4            @ r11 = direct address, OP_CASETBL opcode already skipped
    ldr r12, [r11]              @ r12 = number of case-table records
    ldr r4, [r11, #4]           @ preset ovl_index to "default" case (none-matched)
.op_iswitch_loop:
    subs r12, r12, #1           @ decrement number to do; any left?
    bmi .op_iswitch_done        @ no, quit (CIP already set to the default value)
    add r11, r11, #8            @ move to next record
    ldr r14, [r11]              @ get the case value
    cmp r0, r14                 @ case value identical to PRI ?
    bne .op_iswitch_loop        @ no, continue
    ldr r4, [r11, #4]           @ yes, load matching ovl_index and exit loop
.op_iswitch_done:
    ldr r14, [sp], #4           @ restore r14
    str r4, [r10, #amxOvlIndex] @ store new overlay index
    stmfd sp!, {r0 - r3, lr}    @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mov r1, r4                  @ 2nd arg = overlay index
    ldr r11, [r10, #amxOverlay] @ callback function pointer in r11
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r11
    ldmfd sp!, {r0 - r3, lr}    @ restore registers
    ldr r8, [r10, #amxCode]     @ r8 = code pointer (base)
    mov r4, r8                  @ CIP = code base
    NEXT

.OP_SWAP_PRI:                   @ tested
    ldr r11, [r6]
    str r0, [r6]
    mov r0, r11
    NEXT

.OP_SWAP_ALT:
    ldr r11, [r6]
    str r1, [r6]
    mov r1, r11
    NEXT

.OP_PUSH_ADR:                   @ tested
    GETPARAM r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    NEXT

.OP_NOP:
    NEXT

.OP_BREAK:                      @ tested
    ldr r12, [r10, #amxDebug]
    teq r12, #0
    beq .op_break_quit
    @ store stack and heap state AMX state
    sub r11, r7, r5             @ reverse-relocate FRM
    str r11, [r10, #amxFRM]
    sub r11, r6, r5             @ reverse-relocate STK
    str r11, [r10, #amxSTK]
    sub r11, r3, r5             @ reverse-relocate HEA
    str r11, [r10, #amxHEA]
    sub r11, r4, r8             @ reverse-relocate CIP
    str r11, [r10, #amxCIP]
    @ invoke debug hook (address still in r12)
    stmfd   sp!, {r0 - r3, lr}  @ save some extra registers
    mov r0, r10                 @ 1st arg = AMX
    mov lr, pc                  @ simulate BLX with MOV and BX
    bx  r12
    mov r11, r0                 @ store exit code in r11 (r0 is restored)
    ldmfd sp!, {r0 - r3, lr}    @ restore registers
    teq r11, #0                 @ debug hook returned error/abort code?
    bne .amx_exit               @ yes -> quit
.op_break_quit:
    NEXT

.OP_PUSH5:                      @ tested
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    @ drop through
.OP_PUSH4:                      @ tested
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    @ drop through
.OP_PUSH3:                      @ tested
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    @ drop through
.OP_PUSH2:                      @ tested
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    GETPARAM r11
    ldr r11, [r5, r11]
    PUSH r11
    NEXT

.OP_PUSH5_S:                    @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    @ drop through
.OP_PUSH4_S:                    @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    @ drop through
.OP_PUSH3_S:                    @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    @ drop through
.OP_PUSH2_S:                    @ tested
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    GETPARAM r11
    ldr r11, [r7, r11]
    PUSH r11
    NEXT

.OP_PUSH5_C:
    GETPARAM r11
    PUSH r11
    @ drop through
.OP_PUSH4_C:                    @ tested
    GETPARAM r11
    PUSH r11
    @ drop through
.OP_PUSH3_C:                    @ tested
    GETPARAM r11
    PUSH r11
    @ drop through
.OP_PUSH2_C:                    @ tested
    GETPARAM r11
    PUSH r11
    GETPARAM r11
    PUSH r11
    NEXT

.OP_PUSH5_ADR:                  @ tested
    GETPARAM r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    @ drop through
.OP_PUSH4_ADR:                  @ tested
    GETPARAM r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    @ drop through
.OP_PUSH3_ADR:                  @ tested
    GETPARAM r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    @ drop through
.OP_PUSH2_ADR:                  @ tested
    GETPARAM r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    GETPARAM r11
    add r11, r11, r7
    sub r11, r11, r5
    PUSH r11
    NEXT

.OP_LOAD_BOTH:                  @ tested
    GETPARAM r11
    ldr r0, [r5, r11]
    GETPARAM r11
    ldr r1, [r5, r11]
    NEXT

.OP_LOAD_S_BOTH:                @ tested
    GETPARAM r11
    ldr r0, [r7, r11]
    GETPARAM r11
    ldr r1, [r7, r11]
    NEXT

.OP_CONST:                      @ tested
    GETPARAM r11
    GETPARAM r12
    str r12, [r5, r11]
    NEXT

.OP_CONST_S:                    @ tested
    GETPARAM r11
    GETPARAM r12
    str r12, [r7, r11]
    NEXT

@ --------------------------------------------------------
@ packed opcodes
@ --------------------------------------------------------
.ifndef AMX_NO_PACKED_OPC

.OP_LOAD_P_PRI:
    ldr r0, [r5, r12, ASR #16]
    NEXT

.OP_LOAD_P_ALT:
    ldr r1, [r5, r12, ASR #16]
    NEXT

.OP_LOAD_P_S_PRI:
    ldr r0, [r7, r12, ASR #16]
    NEXT

.OP_LOAD_P_S_ALT:
    ldr r1, [r7, r12, ASR #16]
    NEXT

.OP_LREF_P_PRI:
    ldr r11, [r5, r12, ASR #16]
    ldr r0, [r5, r11]
    NEXT

.OP_LREF_P_ALT:
    ldr r11, [r5, r12, ASR #16]
    ldr r1, [r5, r11]
    NEXT

.OP_LREF_P_S_PRI:
    ldr r11, [r7, r12, ASR #16]
    ldr r0, [r5, r11]
    NEXT

.OP_LREF_P_S_ALT:
    ldr r11, [r7, r12, ASR #16]
    ldr r1, [r5, r11]
    NEXT

.OP_LODB_P_I:
    GETPARAM_P r11
    add r12, r0, r5             @ relocate PRI to absolute address
    VERIFYADDRESS r12
    teq r11, #1
    ldreqb r0, [r12]
    teq r11, #2
    ldreqh r0, [r12]
    teq r11, #4
    ldreq r0, [r12]
    NEXT

.OP_CONST_P_PRI:
    GETPARAM_P r0
    NEXT

.OP_CONST_P_ALT:
    GETPARAM_P r1
    NEXT

.OP_ADDR_P_PRI:
    GETPARAM_P r0
    add r0, r0, r7              @ add FRM
    sub r0, r0, r5              @ reverse relocate
    NEXT

.OP_ADDR_P_ALT:
    GETPARAM_P r1
    add r1, r1, r7              @ add FRM
    sub r1, r1, r5              @ reverse relocate
    NEXT

.OP_STOR_P_PRI:
    str r0, [r5, r12, ASR #16]
    NEXT

.OP_STOR_P_ALT:
    str r1, [r5, r12, ASR #16]
    NEXT

.OP_STOR_P_S_PRI:
    str r0, [r7, r12, ASR #16]
    NEXT

.OP_STOR_P_S_ALT:
    str r1, [r7, r12, ASR #16]
    NEXT

.OP_SREF_P_PRI:
    ldr r11, [r5, r12, ASR #16]
    str r0, [r5, r11]
    NEXT

.OP_SREF_P_ALT:
    ldr r11, [r5, r12, ASR #16]
    str r1, [r5, r11]
    NEXT

.OP_SREF_P_S_PRI:
    ldr r11, [r7, r12, ASR #16]
    str r0, [r5, r11]
    NEXT

.OP_SREF_P_S_ALT:
    ldr r11, [r7, r12, ASR #16]
    str r1, [r5, r11]
    NEXT

.OP_STRB_P_I:
    GETPARAM_P r11
    add r12, r1, r5             @ relocate ALT to absolute address
    VERIFYADDRESS r12
    teq r11, #1
    streqb r0, [r12]
    teq r11, #2
    streqh r0, [r12]
    teq r11, #4
    streq r0, [r12]
    NEXT

.OP_LIDX_P_B:
    GETPARAM_P r11
    add r12, r1, r0, LSL r11    @ r12 = ALT + (PRI << param)
    add r12, r12, r5            @ relocate to absolute address
    VERIFYADDRESS r12
    ldr r0, [r12]
    NEXT

.OP_IDXADDR_P_B:
    GETPARAM_P r11
    add r0, r1, r0, LSL r11     @ PRI = ALT + (PRI << param)
    NEXT

.OP_ALIGN_P_PRI:
    GETPARAM_P r11
.ifndef BIG_ENDIAN
    rsbs r11, r11, #4           @ rsb = reverse subtract; r11 = #4 - param
    eorhi r0, r0, r11           @ PRI ^= (#4 - param), but only if (#4 - param) > 0
.endif
    NEXT

.OP_ALIGN_P_ALT:
    GETPARAM_P r11
.ifndef BIG_ENDIAN
    rsbs r11, r11, #4           @ rsb = reverse subtract; r11 = #4 - param
    eorhi r1, r1, r11           @ ALT ^= (#4 - param), but only if (#4 - param) > 0
.endif
    NEXT

.OP_PUSH_P_C:
    GETPARAM_P r11
    PUSH r11
    NEXT

.OP_PUSH_P:
    ldr r11, [r5, r12, ASR #16]
    PUSH r11
    NEXT

.OP_PUSH_P_S:
    ldr r11, [r7, r12, ASR #16]
    PUSH r11
    NEXT

.OP_STACK_P:
    GETPARAM_P r11
    sub r1, r6, r5              @ ALT = STK, reverse-relocated
    add r6, r6, r11             @ STK += param
    CHKMARGIN r12
    CHKSTACK
    NEXT

.OP_HEAP_P:
    GETPARAM_P r11
    sub r1, r3, r5              @ ALT = HEA, reverse-relocated
    add r3, r3, r11
    CHKMARGIN r12
    CHKHEAP r12
    NEXT

.OP_SHL_P_C_PRI:
    GETPARAM_P r11
    mov r0, r0, LSL r11         @ PRI = PRI << param
    NEXT

.OP_SHL_P_C_ALT:
    GETPARAM_P r11
    mov r1, r1, LSL r11         @ ALT = ALT << param
    NEXT

.OP_SHR_P_C_PRI:
    GETPARAM_P r11
    cmp r11, #0
    movhi r0, r0, LSR r11       @ PRI = PRI >> param (but check that param > 0)
    NEXT

.OP_SHR_P_C_ALT:
    GETPARAM_P r11
    cmp r11, #0
    movhi r1, r1, LSR r11       @ ALT = ALT >> param (but check that param > 0)
    NEXT

.OP_ADD_P_C:
    GETPARAM_P r11
    add r0, r0, r11             @ PRI += param
    NEXT

.OP_SMUL_P_C:
    GETPARAM_P r11
    mov r12, r0
    mul r0, r11, r12            @ PRI *= param
    NEXT

.OP_ZERO_P:
    GETPARAM_P r11
    mov r12, #0
    str r12, [r5, r11]
    NEXT

.OP_ZERO_P_S:
    GETPARAM_P r11
    mov r12, #0
    str r12, [r7, r11]
    NEXT

.OP_EQ_P_C_PRI:
    GETPARAM_P r11
    cmp r0, r11
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_EQ_P_C_ALT:
    GETPARAM_P r11
    cmp r1, r11
    moveq r0, #1
    movne r0, #0
    NEXT

.OP_INC_P:
    GETPARAM_P r11
    ldr r12, [r5, r11]
    add r12, r12, #1
    str r12, [r5, r11]
    NEXT

.OP_INC_P_S:
    GETPARAM_P r11
    ldr r12, [r7, r11]
    add r12, r12, #1
    str r12, [r7, r11]
    NEXT

.OP_DEC_P:
    GETPARAM_P r11
    ldr r12, [r5, r11]
    sub r12, r12, #1
    str r12, [r5, r11]
    NEXT

.OP_DEC_P_S:
    GETPARAM_P r11
    ldr r12, [r7, r11]
    sub r12, r12, #1
    str r12, [r7, r11]
    NEXT

.OP_MOVS_P:
    GETPARAM_P r11
    b .movsentry

.OP_CMPS_P:
    GETPARAM_P r11
    b .cmpsentry

.OP_FILL_P:
    GETPARAM_P r11
    b .fillentry

.OP_HALT_P:
    ldr r11, [sp]               @ get "retval" pointer
    teq r11, #0
    strne r0, [r11]             @ store PRI, but only if r11 != 0
    GETPARAM_P r11              @ parameter = return code from function
    b   .amx_exit

.OP_BOUNDS_P:
    GETPARAM_P r11
    cmp r0, r11
    movhi r11, #AMX_ERR_BOUNDS
    bhi .amx_exit
    NEXT

.OP_PUSH_P_ADR:
    GETPARAM_P r11
    add r11, r11, r7            @ relocate to FRM
    sub r11, r11, r5            @ but relative to start of data section
    PUSH r11
    NEXT

.endif  @ AMX_NO_PACKED_OPC


.amx_exit:                      @ assume r11 already set to the exit code
    @ reverse relocate registers
    sub r3, r3, r5              @ reverse-relocate HEA
    sub r6, r6, r5              @ reverse-relocate STK
    sub r7, r7, r5              @ reverse-relocate FRM
    sub r4, r4, r8              @ reverse-relocate CIP

    @ store stack and heap state AMX state
    str r0, [r10, #amxPRI]      @ PRI
    str r1, [r10, #amxALT]      @ ALT
    str r3, [r10, #amxHEA]      @ HEA
    str r4, [r10, #amxCIP]      @ CIP
    str r6, [r10, #amxSTK]      @ STK
    str r7, [r10, #amxFRM]      @ FRM

    mov r0, r11                 @ put return value in r0
    add sp, sp, #4              @ drop register for the return value
    ldmfd sp!, {r4 - r12, lr}
    bx  lr

    .size   amx_exec_asm, .-amx_exec_asm


    .align  2
    .global amx_div
    .type   amx_div, %function
amx_div:
    @ expects divident in r0, divisor in r1
    @ on exit quotient is in r0, remainder in r1
    @ r11 and r12 are scratch; r12 is temporary result
    @ unsigned division only; when r1 (divisor) is zero, the function returns
    @ with all registers unchanged
    teq r1, #0                  @ verify r1
    moveq pc, lr                @ just for security
    @ drop-through (divisor is not zero)
    mov r11, #1
.amx_div1:
    cmp r1, #0x80000000         @ shift divisor left until top bit set
    cmpcc r1, r0                @ ...or divisor>divident
    movcc r1, r1, LSL #1        @ shift divisor left if required
    movcc r11, r11, LSL #1      @ shift r11 left if required
    bcc .amx_div1               @ repeat whilst more shifting required
    mov r12, #0                 @ used to store result (temporary)
.amx_div2:
    cmp r0, r1                  @ test for possible subtraction
    subcs r0, r0, r1            @ subtract if divident>divisor
    addcs r12, r12, r11         @ put relevant bit into result
    movs  r11, r11, LSR #1      @ shift control bit
    movne r1, r1, LSR #1        @ halve unless finished
    bne .amx_div2               @ loop if there is more to do
    @ remainder r0, result in r12
    mov r1, r0                  @ remainder now in r1
    mov r0, r12                 @ quotient now in r0
    mov pc, lr

    .size   amx_div, .-amx_div

    .end
