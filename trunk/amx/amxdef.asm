; Definition of the AMX structure for assembler syntax (MASM/TASM/WASM)

amx_s   STRUC
    _base       DD ?
    _codeseg    DD ?
    _dataseg    DD ?
    _callback   DD ?
    _debug      DD ?
    _overlay    DD ?
    _cip        DD ?
    _frm        DD ?
    _hea        DD ?
    _hlw        DD ?
    _stk        DD ?
    _stp        DD ?
    _flags      DD ?
    _usertags   DD 4 DUP (?)    ; 4 = AMX_USERNUM (#define'd in amx.h)
    _userdata   DD 4 DUP (?)    ; 4 = AMX_USERNUM (#define'd in amx.h)
    _error      DD ?
    _paramcount DD ?
    _pri        DD ?
    _alt        DD ?
    _reset_stk  DD ?
    _reset_hea  DD ?
    _syscall_d  DD ?
    _ovl_index  DD ?
    _codesize   DD ?            ; memory size of the overlay or of the native code
IFDEF JIT
    ; the two fields below are for the JIT; they do not exist in
    ; the non-JIT version of the abstract machine
    _reloc_size DD ?            ; memory block for relocations
ENDIF
amx_s   ENDS

amxhead_s   STRUC
    _size       DD ?  ; size of the "file"
    _magic      DW ?  ; signature
    _file_version DB ? ;file format version
    _amx_version DB ? ; required version of the AMX
    _h_flags    DW ?
    _defsize    DW ?  ; size of one public/native function entry
    _cod        DD ?  ; initial value of COD - code block
    _dat        DD ?  ; initial value of DAT - data block
    _h_hea      DD ?  ; initial value of HEA - start of the heap
    _h_stp      DD ?  ; initial value of STP - stack top
    _h_cip      DD ?  ; initial value of CIP - the instruction pointer
    _publics    DD ?  ; offset to the "public functions" table
    _natives    DD ?  ; offset to the "native functions" table
    _libraries  DD ?  ; offset to the "library" table
    _pubvars    DD ?  ; offset to the "public variables" table
    _tags       DD ?  ; offset to the "public tagnames" table
    _nametable  DD ?  ; offset to the name table, file version 7+ only
    _overlaytbl DD ?  ; offset to the overlay table, file version 10+ only
amxhead_s   ENDS


AMX_ERR_NONE        EQU 0
AMX_ERR_EXIT        EQU 1
AMX_ERR_ASSERT      EQU 2
AMX_ERR_STACKERR    EQU 3
AMX_ERR_BOUNDS      EQU 4
AMX_ERR_MEMACCESS   EQU 5
AMX_ERR_INVINSTR    EQU 6
AMX_ERR_STACKLOW    EQU 7
AMX_ERR_HEAPLOW     EQU 8
AMX_ERR_CALLBACK    EQU 9
AMX_ERR_NATIVE      EQU 10
AMX_ERR_DIVIDE      EQU 11      ; for catching divide errors
AMX_ERR_SLEEP       EQU 12

AMX_ERR_MEMORY      EQU 16
AMX_ERR_FORMAT      EQU 17
AMX_ERR_VERSION     EQU 18
AMX_ERR_NOTFOUND    EQU 19
AMX_ERR_INDEX       EQU 20
AMX_ERR_DEBUG       EQU 21
AMX_ERR_INIT        EQU 22
AMX_ERR_USERDATA    EQU 23
AMX_ERR_INIT_JIT    EQU 24
AMX_ERR_PARAMS      EQU 25
AMX_ERR_DOMAIN      EQU 26
AMX_ERR_GENERAL     EQU 27

AMX_FLAG_DEBUG      EQU 0002h   ; symbolic info. available
AMX_FLAG_COMPACT    EQU 0004h
AMX_FLAG_BYTEOPC    EQU 0008h
AMX_FLAG_NOCHECKS   EQU 0010h
AMX_FLAG_BROWSE     EQU 4000h
AMX_FLAG_RELOC      EQU 8000h   ; jump/call addresses relocated

