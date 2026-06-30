#if !defined(RISCV_JIT_H)
#define RISCV_JIT_H

#include "Public/Types.h"
#include <elf.h>

#define RiscV_BlockSize 100
// NOTE: Gets assigned to ra and then when this is encountered the function is done
#define RiscV_MagicRet 0xFFFFFFFFFFFFFFFEULL

//
// NOTE: Callbacks
//

typedef struct RiscV_JIT RiscV_JIT;

// NOTE: When the mod executes ecall.
typedef Void (*RiscV_JIT_EcallCb)(RiscV_JIT *State);
typedef void (*RiscV_JIT_Func)(Uint64 *);

//
// NOTE: Structures
//

typedef struct
{
    const Uint8 *Mem;
    Usize Size;

    const Elf64_Ehdr *Ehdr;

    const Elf64_Shdr *ShdrTable;
    Usize ShdrCount;

    const Elf64_Sym *SymTable;
    Usize SymCount;

    const char *StrTable;
    Usize StrTableSize;

    const Elf64_Shdr *TextShdr;

    const Elf64_Rela *RelaTable;
    Usize RelaCount;
} RiscV_JIT_Elf;

struct RiscV_JIT
{
    // NOTE:
    // [0-31] = standard x0-x31
    // [32]   = PC
    // [33]   = 0
    // [34]   = junk
    Uint64 Regs[35];

    Void *UserData;
    RiscV_JIT_EcallCb EcallCb;
};

typedef struct
{
    Bool IsContinuation;
    // NOTE: Local offset within the stencil's byte array
    Usize Offset;
    // NOTE: Relocation addend
    Uint64 Addend;
    // NOTE: Relocation type (R_X86_64_32, R_X86_64_PC32, etc.)
    Usize RelType;
    // NOTE: Dynamic name of placeholder
    char Name[64];
} RiscV_JIT_Hole;

typedef struct
{
    const Uint8 *Bytes;
    Usize Size;

    // NOTE: Original st_value within .text
    Uint64 SymbolVal;

    RiscV_JIT_Hole Holes[16];
    Usize HoleCount;
} RiscV_JIT_Stencil;

//
// NOTE: Stencils
//

#define RiscV_Stencils(X) \
    X(ADDI)               \
    X(ADDIW)              \
    X(JALR)               \
    X(Exit)

typedef struct
{
#define X(Name) RiscV_JIT_Stencil Name;
    RiscV_Stencils(X)
#undef X
} RiscV_JIT_Stencils;

//
// NOTE: Funcs
//

Result RiscV_JIT_Elf_Parse(const Uint8 *Mem, Usize Size, RiscV_JIT_Elf *OutElf);
// NOTE: Module here is the ELF with stencils.
Result RiscV_JIT_Stencils_Load(const RiscV_JIT_Elf *Module, RiscV_JIT_Stencils *OutStencils);

Result RiscV_JIT_FindExport(const RiscV_JIT_Elf *Module, const char *FuncName, Uint64 *OutPC);
Result RiscV_JIT_Call(RiscV_JIT *State, const RiscV_JIT_Stencils *Stencils, const RiscV_JIT_Elf *Module, Uint64 PC);

#endif
