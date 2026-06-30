//
// NOTE: Copy-and-Patch JIT.
// TODO:
// * Map registers directly to hardware registers, I think it should bring
// around 40% performance improvement
//
#include "Public/Mem.h"
#include "Public/Types.h"

#include "RiscV_Instr.h"
#include "RiscV_JIT.h"
#include "RiscV_Op.h"

#include <elf.h>
#include <stdio.h>
#include <sys/mman.h>

//
// NOTE: Internal
//

typedef struct
{
    const RiscV_JIT_Stencil *Template;
    Uint64 HoleValues[16];
} RiscV_JIT_StencilInst;

typedef struct
{
    Void *Mem;
    Usize Size;
} RiscV_JIT_ExecCode;

static Result MapValue(RiscV_JIT_StencilInst *Inst, const char *Name, Uint64 Value)
{
    Assert(Inst);
    Assert(Name);

    for (Usize i = 0; i < Inst->Template->HoleCount; ++i)
    {
        if (Mem_Eql(Inst->Template->Holes[i].Name, Name, Mem_CStrLen(Name)))
        {
            Inst->HoleValues[i] = Value;
        }
    }
    return Result_Success();
}

static void BindStandard(RiscV_JIT_StencilInst *Inst, const RiscV_JIT_Stencil *Template, const RiscV_Instr *Instr, Uint64 PC)
{
    Inst->Template = Template;

    MapValue(Inst, "_Rd", RiscV_Op_WriteOffset(Instr->Rd));
    MapValue(Inst, "_Rs1", RiscV_Op_ReadOffset(Instr->Rs1));
    MapValue(Inst, "_Rs2", RiscV_Op_ReadOffset(Instr->Rs2));
    MapValue(Inst, "_Imm", (Uint64)(Int64)Instr->Imm);
    MapValue(Inst, "_NextPC", PC + 4);
    MapValue(Inst, "_PC", PC);
}

static Result Stich(const RiscV_JIT_StencilInst *Instances, Usize Count, RiscV_JIT_ExecCode *OutExec)
{
    Assert(OutExec);
    if (!Instances || Count == 0)
    {
        return Result_MakeError("Invalid params");
    }

    Usize TotalSize = 0;
    for (Usize i = 0; i < Count; ++i)
    {
        TotalSize += Instances[i].Template->Size;
    }

    Usize PageSize = Kb(4);
    Usize AllocatedSize = AlignUp(TotalSize + 1, PageSize);

    Void *ExecMem = mmap(0, AllocatedSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ExecMem == MAP_FAILED)
    {
        return Result_MakeError("mmap");
    }

    Usize CurrentOffset = 0;
    Usize BaseOffsets[Count];

    for (Usize i = 0; i < Count; ++i)
    {
        BaseOffsets[i] = CurrentOffset;
        Mem_Copy((Uint8 *)ExecMem + CurrentOffset, Instances[i].Template->Bytes, Instances[i].Template->Size);

        CurrentOffset += Instances[i].Template->Size;
    }

    Usize RetOff = CurrentOffset;
    *((Uint8 *)ExecMem + RetOff) = 0xC3;

    for (Usize i = 0; i < Count; ++i)
    {
        const RiscV_JIT_StencilInst *Inst = &Instances[i];
        Usize StencilBase = BaseOffsets[i];
        Usize NextAddr = (i + 1 < Count) ? ((Usize)ExecMem + BaseOffsets[i + 1]) : ((Usize)ExecMem + RetOff);

        for (Usize h = 0; h < Inst->Template->HoleCount; ++h)
        {
            const RiscV_JIT_Hole *Hole = &Inst->Template->Holes[h];
            Uint8 *PatchAddr = (Uint8 *)ExecMem + StencilBase + Hole->Offset;

            if (Hole->IsContinuation)
            {
                Uint32 Val = (Uint32)(NextAddr + Hole->Addend - (Usize)PatchAddr);
                Mem_Copy(PatchAddr, &Val, sizeof(Val));
            }
            else
            {
                Uint64 Val = Inst->HoleValues[h] + Hole->Addend;
                if (Hole->RelType == R_X86_64_32 || Hole->RelType == R_X86_64_32S)
                {
                    Uint32 Val32 = (Uint32)Val;
                    Mem_Copy(PatchAddr, &Val32, sizeof(Val32));
                }
                else if (Hole->RelType == R_X86_64_64)
                {
                    Mem_Copy(PatchAddr, &Val, sizeof(Val));
                }
            }
        }
    }

    if (mprotect(ExecMem, AllocatedSize, PROT_READ | PROT_EXEC))
    {
        return Result_MakeError("mprotect");
    }

    OutExec->Mem = ExecMem;
    OutExec->Size = AllocatedSize;
    return Result_Success();
}

#define SimpleBlockSize 100

static Result CompileSimpleBlock(const RiscV_JIT_Stencils *Stencils, const Uint8 *Mem, Usize Size, Uint64 PC, RiscV_JIT_ExecCode *OutBlock)
{
    RiscV_JIT_StencilInst Block[SimpleBlockSize];
    Usize InstCount = 0;
    Uint64 CurrentPC = PC;

    while (CurrentPC < Size && InstCount < ArrayCount(Block) - 1)
    {
        Uint32 RawInst = *(const Uint32 *)(Mem + CurrentPC);

        RiscV_Instr Instr;
        RiscV_Instr_Disasm(RawInst, CurrentPC, &Instr);

        Bool IsRecognized = True;

        //
        // NOTE: Opcode handling
        //

        switch (Instr.Opcode)
        {
        case RiscV_Op_Imm:
        {
            switch (Instr.F3)
            {
            case RiscV_F3_Add_Sub:
            {
                BindStandard(&Block[InstCount], &Stencils->ADDI, &Instr, CurrentPC);
            }
            break;
            default:
                IsRecognized = False;
                break;
            }
            break;
        }

        case RiscV_Op_Imm_32:
        {
            switch (Instr.F3)
            {
            case RiscV_F3_Add_Sub:
            {
                BindStandard(&Block[InstCount], &Stencils->ADDIW, &Instr, CurrentPC);
            }
            break;
            default:
                IsRecognized = False;
                break;
            }
            break;
        }

        case RiscV_Op_Jalr:
        {
            switch (Instr.F3)
            {
            case 0:
            {
                BindStandard(&Block[InstCount], &Stencils->JALR, &Instr, CurrentPC);
                InstCount++;

                return Stich(Block, InstCount, OutBlock);
            }

            default:
                IsRecognized = False;
                break;
            }
            break;
        }

        case RiscV_Op_System:
        {
            switch (Instr.F3)
            {
            case 0:
            {
                if (Instr.Imm == 0 || Instr.Imm == 1)
                {
                    BindStandard(&Block[InstCount], &Stencils->Exit, &Instr, CurrentPC);
                    InstCount++;

                    return Stich(Block, InstCount, OutBlock);
                }
                else
                {
                    IsRecognized = False;
                }
                break;
            }
            default:
                IsRecognized = False;
                break;
            }
            break;
        }

        default:
        {
            printf("Not recognized: 0x%x\n", Instr.Opcode);
            IsRecognized = False;
            break;
        }
        }

        if (!IsRecognized)
        {
            if (InstCount == 0)
            {
                return Result_MakeError("Unrecognized instruction encountered");
            }
            break;
        }

        InstCount++;
        CurrentPC += 4;
    }

    Block[InstCount].Template = &Stencils->Exit;
    MapValue(&Block[InstCount], "_Imm", CurrentPC);
    InstCount++;

    return Stich(Block, InstCount, OutBlock);
}

static Result GetStencil(const RiscV_JIT_Elf *Elf, const char *SymbolName, RiscV_JIT_Stencil *OutStencil)
{
    Assert(Elf);
    Assert(SymbolName);
    Assert(OutStencil);

    Usize TargetLen = Mem_CStrLen(SymbolName);
    const Elf64_Sym *FoundSym = 0;

    for (Usize i = 0; i < Elf->SymCount; ++i)
    {
        const Elf64_Sym *Sym = &Elf->SymTable[i];
        if (Sym->st_name >= Elf->StrTableSize)
        {
            continue;
        }

        const char *Name = Elf->StrTable + Sym->st_name;
        Usize NameLen = Mem_CStrLen(Name);

        if (NameLen == TargetLen && Mem_Eql(Name, SymbolName, TargetLen))
        {
            FoundSym = Sym;
            break;
        }
    }

    if (!FoundSym)
    {
        return Result_MakeError("Stencil symbol not found in ELF");
    }

    Usize SectionOffset = FoundSym->st_value;
    Usize SymbolSize = FoundSym->st_size;

    if (SectionOffset + SymbolSize > Elf->TextShdr->sh_size)
    {
        return Result_MakeError("Symbol exceeds text section boundaries");
    }

    Usize FileCodeOffset = Elf->TextShdr->sh_offset + SectionOffset;
    if (FileCodeOffset + SymbolSize > Elf->Size)
    {
        return Result_MakeError("Symbol data exceeds file bounds");
    }

    OutStencil->Bytes = Elf->Mem + FileCodeOffset;
    OutStencil->Size = SymbolSize;
    OutStencil->SymbolVal = SectionOffset;
    OutStencil->HoleCount = 0;

    if (!Elf->RelaTable)
    {
        return Result_Success();
    }

    const char *ContName = "_Next";
    Usize ContLen = Mem_CStrLen(ContName);

    for (Usize i = 0; i < Elf->RelaCount; ++i)
    {
        const Elf64_Rela *Rel = &Elf->RelaTable[i];

        if (Rel->r_offset < SectionOffset || Rel->r_offset >= SectionOffset + SymbolSize)
        {
            continue;
        }

        if (OutStencil->HoleCount >= ArrayCount(OutStencil->Holes))
        {
            return Result_MakeError("Maximum number of holes exceeded");
        }

        Usize SymIdx = ELF64_R_SYM(Rel->r_info);
        if (SymIdx >= Elf->SymCount)
        {
            return Result_MakeError("Relocation symbol index out of bounds");
        }

        const Elf64_Sym *RelSym = &Elf->SymTable[SymIdx];
        if (RelSym->st_name >= Elf->StrTableSize)
        {
            return Result_MakeError("Relocation symbol name out of bounds");
        }

        const char *RelSymName = Elf->StrTable + RelSym->st_name;
        Usize RelSymNameLen = Mem_CStrLen(RelSymName);

        RiscV_JIT_Hole *Hole = &OutStencil->Holes[OutStencil->HoleCount];
        Hole->Offset = (Usize)(Rel->r_offset - SectionOffset);
        Hole->Addend = Rel->r_addend;
        Hole->RelType = ELF64_R_TYPE(Rel->r_info);

        Usize CopyLen = Min(RelSymNameLen, ArrayCount(Hole->Name) - 1);
        Mem_Copy(Hole->Name, RelSymName, CopyLen);
        Mem_NullTerminate(Hole->Name, ArrayCount(Hole->Name), CopyLen);

        Hole->IsContinuation = (RelSymNameLen == ContLen && Mem_Eql(RelSymName, ContName, ContLen));

        OutStencil->HoleCount++;
    }

    return Result_Success();
}

//
// NOTE: Implementation
//

Result RiscV_JIT_Elf_Parse(const Uint8 *Mem, Usize Size, RiscV_JIT_Elf *OutElf)
{
    Assert(OutElf);

    if (!Mem || Size == 0)
    {
        return Result_MakeError("Invalid params");
    }

    Mem_Stream S = Mem_Stream_Init(Mem, Size);

    const Elf64_Ehdr *Ehdr = (const Elf64_Ehdr *)Mem_Stream_ReadBytes(&S, sizeof(Elf64_Ehdr));
    if (S.HasError || !Ehdr)
    {
        return Result_MakeError("Failed to read ELF header");
    }

    if (Ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        Ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        Ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        Ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        Ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        return Result_MakeError("Invalid ELF file magic or class");
    }

    OutElf->Mem = Mem;
    OutElf->Size = Size;
    OutElf->Ehdr = Ehdr;

    Usize ShdrOffset = Ehdr->e_shoff;
    Usize ShdrCount = Ehdr->e_shnum;
    Usize ShdrTotalSize = ShdrCount * sizeof(Elf64_Shdr);

    Mem_Stream_Seek(&S, ShdrOffset);
    const Elf64_Shdr *ShdrTable = (const Elf64_Shdr *)Mem_Stream_ReadBytes(&S, ShdrTotalSize);
    if (S.HasError || !ShdrTable)
    {
        return Result_MakeError("Failed to read ELF section header table");
    }

    OutElf->ShdrTable = ShdrTable;
    OutElf->ShdrCount = ShdrCount;

    OutElf->SymTable = 0;
    OutElf->SymCount = 0;
    OutElf->StrTable = 0;
    OutElf->StrTableSize = 0;
    OutElf->TextShdr = 0;
    OutElf->RelaTable = 0;
    OutElf->RelaCount = 0;

    for (Usize i = 0; i < ShdrCount; ++i)
    {
        const Elf64_Shdr *Shdr = &OutElf->ShdrTable[i];

        if (Shdr->sh_type == SHT_SYMTAB)
        {
            Mem_Stream_Seek(&S, Shdr->sh_offset);
            const Elf64_Sym *SymTable = (const Elf64_Sym *)Mem_Stream_ReadBytes(&S, Shdr->sh_size);
            if (S.HasError || !SymTable)
            {
                return Result_MakeError("Failed to read symbol table section");
            }
            OutElf->SymTable = SymTable;
            OutElf->SymCount = Shdr->sh_size / sizeof(Elf64_Sym);

            Usize LinkIdx = Shdr->sh_link;
            if (LinkIdx >= ShdrCount)
            {
                continue;
            }

            const Elf64_Shdr *StrShdr = &OutElf->ShdrTable[LinkIdx];
            Mem_Stream_Seek(&S, StrShdr->sh_offset);
            const char *StrTable = (const char *)Mem_Stream_ReadBytes(&S, StrShdr->sh_size);

            if (S.HasError || !StrTable)
            {
                return Result_MakeError("Failed to read string table section linked to symtab");
            }

            OutElf->StrTable = StrTable;
            OutElf->StrTableSize = StrShdr->sh_size;
            continue;
        }

        if (Shdr->sh_type == SHT_PROGBITS && (Shdr->sh_flags & SHF_EXECINSTR))
        {
            Mem_Stream_Seek(&S, Shdr->sh_offset);
            Mem_Stream_ReadBytes(&S, Shdr->sh_size);
            if (S.HasError)
            {
                return Result_MakeError("Executable text section is out of file bounds");
            }
            OutElf->TextShdr = Shdr;
            continue;
        }

        if (Shdr->sh_type == SHT_RELA)
        {
            Usize TargetIdx = Shdr->sh_info;
            if (TargetIdx >= ShdrCount)
            {
                continue;
            }

            const Elf64_Shdr *TargetShdr = &OutElf->ShdrTable[TargetIdx];
            if (TargetShdr->sh_type != SHT_PROGBITS || !(TargetShdr->sh_flags & SHF_EXECINSTR))
            {
                continue;
            }

            Mem_Stream_Seek(&S, Shdr->sh_offset);
            const Elf64_Rela *RelaTable = (const Elf64_Rela *)Mem_Stream_ReadBytes(&S, Shdr->sh_size);
            if (S.HasError || !RelaTable)
            {
                return Result_MakeError("Failed to read relocation table section");
            }
            OutElf->RelaTable = RelaTable;
            OutElf->RelaCount = Shdr->sh_size / sizeof(Elf64_Rela);
            continue;
        }
    }

    if (!OutElf->SymTable || !OutElf->StrTable || !OutElf->TextShdr)
    {
        return Result_MakeError("Required ELF sections are missing");
    }

    return Result_Success();
}

Result RiscV_JIT_FindExport(const RiscV_JIT_Elf *ModElf, const char *FuncName, Uint64 *OutPC)
{
    Assert(ModElf);
    Assert(FuncName);
    Assert(OutPC);

    Usize TargetLen = Mem_CStrLen(FuncName);

    for (Usize i = 0; i < ModElf->SymCount; ++i)
    {
        const Elf64_Sym *Sym = &ModElf->SymTable[i];
        if (Sym->st_name >= ModElf->StrTableSize)
            continue;

        const char *Name = ModElf->StrTable + Sym->st_name;
        if (Mem_CStrLen(Name) == TargetLen && Mem_Eql(Name, FuncName, TargetLen))
        {
            *OutPC = Sym->st_value;
            return Result_Success();
        }
    }
    return Result_MakeError("Export not found in Mod ELF");
}

Result RiscV_JIT_Stencils_Load(const RiscV_JIT_Elf *StencilsElf, RiscV_JIT_Stencils *OutStencils)
{
    if (!StencilsElf || !OutStencils)
    {
        return Result_MakeError("Invalid params");
    }

#define X(Name)                                                                    \
    Result Result = RiscV_JIT_Stencil_Get(StencilsElf, #Name, &OutStencils->Name); \
    if (Result.Error)                                                              \
    {                                                                              \
        return Result;                                                             \
    }                                                                              \
    RiscV_Stencils(X)
#undef X

    return Result_Success();
}

Result RiscV_JIT_Call(RiscV_JIT *State, const RiscV_JIT_Stencils *Stencils, const RiscV_JIT_Elf *ModElf, Uint64 PC)
{
    Assert(State);
    Assert(Stencils);
    Assert(ModElf);

    const Uint8 *Mem = ModElf->Mem + ModElf->TextShdr->sh_offset;
    Usize Size = ModElf->TextShdr->sh_size;

    if (PC >= Size)
    {
        return Result_MakeError("Invalid starting PC");
    }

    State->Regs[1] = RiscV_MagicRet;
    State->Regs[32] = PC;

    while (State->Regs[32] < Size)
    {
        RiscV_JIT_ExecCode Exec = {0};

        Result Result = CompileSimpleBlock(Stencils, Mem, Size, State->Regs[32], &Exec);
        if (Result.Error)
        {
            return Result;
        }

        RiscV_JIT_Func Func = (RiscV_JIT_Func)Exec.Mem;
        Func(State->Regs);

        munmap(Exec.Mem, Exec.Size);

        if (State->Regs[32] == RiscV_MagicRet)
        {
            break;
        }

        Uint32 LastInst = *(const Uint32 *)(Mem + State->Regs[32]);
        if (LastInst == 0x00000073)
        {
            if (State->EcallCb)
            {
                State->EcallCb(State);
            }
            State->Regs[32] += 4;
        }
    }

    return Result_Success();
}
