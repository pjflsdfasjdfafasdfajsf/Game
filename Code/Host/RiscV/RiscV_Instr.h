#if !defined(RISCV_INSTR_H)
#define RISCV_INSTR_H

#include "Public/Types.h"

typedef struct
{
    Uint64 PC;
    Uint32 Raw;

    Uint32 Opcode;
    Uint32 Rd;
    Uint32 Rs1;
    Uint32 Rs2;
    Uint32 F3;
    Uint32 F7;

    // NOTE: Pre-extracted and sign-extended immediate value.
    Int32 Imm;
} RiscV_Instr;

Void RiscV_Instr_Disasm(Uint32 RawInst, Uint64 PC, RiscV_Instr *OutInst);
Result RiscV_Instr_DisasmBuf(const Uint8 *Mem, Usize Size, Uint64 StartPC, RiscV_Instr *OutInstrs, Usize MaxInstrs, Usize *OutCount);

#endif
