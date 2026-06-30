#include "RiscV_Instr.h"
#include "RiscV_Op.h"

Void RiscV_Instr_Disasm(Uint32 Raw, Uint64 PC, RiscV_Instr *OutInstr)
{
    Assert(OutInstr);

    OutInstr->PC = PC;
    OutInstr->Raw = Raw;

    OutInstr->Opcode = RiscV_Opcode(Raw);
    OutInstr->Rd = RiscV_Rd(Raw);
    OutInstr->F3 = RiscV_F3(Raw);
    OutInstr->Rs1 = RiscV_Rs1(Raw);
    OutInstr->Rs2 = RiscV_Rs2(Raw);
    OutInstr->F7 = RiscV_F7(Raw);

    switch (OutInstr->Opcode)
    {
    case RiscV_Op_Lui:
    case RiscV_Op_Auipc:
        OutInstr->Imm = RiscV_Imm_U(Raw);
        break;

    case RiscV_Op_Jal:
        OutInstr->Imm = RiscV_Imm_J(Raw);
        break;

    case RiscV_Op_Jalr:
    case RiscV_Op_Load:
    case RiscV_Op_Imm:
    case RiscV_Op_Imm_32:
    case RiscV_Op_Fence:
    case RiscV_Op_System:
        OutInstr->Imm = RiscV_Imm_I(Raw);
        break;

    case RiscV_Op_Store:
        OutInstr->Imm = RiscV_Imm_S(Raw);
        break;

    case RiscV_Op_Branch:
        OutInstr->Imm = RiscV_Imm_B(Raw);
        break;

    case RiscV_Op_Op:
    case RiscV_Op_Op_32:
        OutInstr->Imm = 0;
        break;

    default:
        OutInstr->Imm = 0;
        break;
    }
}

Result RiscV_Inst_DisasmBuf(const Uint8 *Mem, Usize Size, Uint64 StartPC, RiscV_Instr *OutInstrs, Usize MaxInstrs, Usize *OutCount)
{
    if (!Mem || !OutInstrs || !OutCount)
    {
        return Result_MakeError("Invalid params");
    }

    Usize Count = 0;
    Usize Offset = 0;

    while ((Offset + 4) <= Size && Count < MaxInstrs)
    {
        Uint32 RawInst = *(const Uint32 *)(Mem + Offset);

        RiscV_Instr_Disasm(RawInst, StartPC + Offset, &OutInstrs[Count]);

        Offset += 4;
        Count++;
    }

    *OutCount = Count;
    return Result_Success();
}
