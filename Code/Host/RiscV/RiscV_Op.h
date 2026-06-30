#if !defined(RISCV_OP_H)
#define RISCV_OP_H

#include "Public/Types.h"

#define RiscV_Opcode(inst) ((inst) & 0x7F)
#define RiscV_Rd(inst) (((inst) >> 7) & 0x1F)
#define RiscV_F3(inst) (((inst) >> 12) & 0x7)
#define RiscV_Rs1(inst) (((inst) >> 15) & 0x1F)
#define RiscV_Rs2(inst) (((inst) >> 20) & 0x1F)
#define RiscV_F7(inst) (((inst) >> 25) & 0x7F)

// NOTE: I-Type
#define RiscV_Imm_I(inst) ((Int32)(inst) >> 20)
// NOTE: S-Type
#define RiscV_Imm_S(inst) (((Int32)((inst) & 0xFE000000) >> 20) | (((inst) >> 7) & 0x1F))
// NOTE: B-Type
#define RiscV_Imm_B(inst) (((Int32)((inst) & 0x80000000) >> 19) | (((inst) & 0x7E000000) >> 20) | (((inst) & 0x00000F00) >> 7) | (((inst) & 0x00000080) << 4))
// NOTE: U-Type
#define RiscV_Imm_U(inst) ((inst) & 0xFFFFF000)
// NOTE: J-Type
#define RiscV_Imm_J(inst) (((Int32)((inst) & 0x80000000) >> 11) | ((inst) & 0x000FF000) | (((inst) & 0x00100000) >> 9) | (((inst) & 0x7FE00000) >> 20))

typedef enum
{
    // NOTE: lb, lh, lw, ld, lbu, lhu, lwu
    RiscV_Op_Load = 0x03,
    // NOTE: fence
    RiscV_Op_Fence = 0x0F,
    // NOTE: addi, slti, sltiu, xori, ori, andi, slli, srli, srai
    RiscV_Op_Imm = 0x13,
    // NOTE: auipc
    RiscV_Op_Auipc = 0x17,
    // NOTE: addiw, slliw, srliw, sraiw (RV64)
    RiscV_Op_Imm_32 = 0x1B,
    // NOTE: sb, sh, sw, sd
    RiscV_Op_Store = 0x23,
    // NOTE: add, sub, sll, slt, sltu, xor, srl, sra, or, and (and M-extension)
    RiscV_Op_Op = 0x33,
    // NOTE: lui
    RiscV_Op_Lui = 0x37,
    // NOTE: addw, subw, sllw, srlw, sraw (RV64)
    RiscV_Op_Op_32 = 0x3B,
    // NOTE: beq, bne, blt, bge, bltu, bgeu
    RiscV_Op_Branch = 0x63,
    // NOTE: jalr
    RiscV_Op_Jalr = 0x67,
    // NOTE: jal
    RiscV_Op_Jal = 0x6F,
    // NOTE: ecall, ebreak
    RiscV_Op_System = 0x73
} RiscV_Opcode;

typedef enum
{
    // NOTE: add, sub, addi, mul
    RiscV_F3_Add_Sub = 0x0,
    // NOTE: sll, slli, mulh
    RiscV_F3_Sll = 0x1,
    // NOTE: slt, slti, mulhsu
    RiscV_F3_Slt = 0x2,
    // NOTE: sltu, sltiu, mulhu
    RiscV_F3_Sltu = 0x3,
    // NOTE: xor, xori, div
    RiscV_F3_Xor = 0x4,
    // NOTE: srl, sra, srli, srai, divu
    RiscV_F3_Sr = 0x5,
    // NOTE: or, ori, rem
    RiscV_F3_Or = 0x6,
    // NOTE: and, andi, remu
    RiscV_F3_And = 0x7
} RiscV_Funct3_ALU;

typedef enum
{
    // NOTE: beq
    RiscV_F3_Beq = 0x0,
    // NOTE: bne
    RiscV_F3_Bne = 0x1,
    // NOTE: blt
    RiscV_F3_Blt = 0x4,
    // NOTE: bge
    RiscV_F3_Bge = 0x5,
    // NOTE: bltu
    RiscV_F3_Bltu = 0x6,
    // NOTE: bgeu
    RiscV_F3_Bgeu = 0x7
} RiscV_Funct3_Branch;

typedef enum
{
    // NOTE: lb
    RiscV_F3_Lb = 0x0,
    // NOTE: lh
    RiscV_F3_Lh = 0x1,
    // NOTE: lw
    RiscV_F3_Lw = 0x2,
    // NOTE: ld (RV64)
    RiscV_F3_Ld = 0x3,
    // NOTE: lbu
    RiscV_F3_Lbu = 0x4,
    // NOTE: lhu
    RiscV_F3_Lhu = 0x5,
    // NOTE: lwu (RV64)
    RiscV_F3_Lwu = 0x6
} RiscV_Funct3_Load;

typedef enum
{
    // NOTE: sb
    RiscV_F3_Sb = 0x0,
    // NOTE: sh
    RiscV_F3_Sh = 0x1,
    // NOTE: sw
    RiscV_F3_Sw = 0x2,
    // NOTE: sd (RV64)
    RiscV_F3_Sd = 0x3
} RiscV_Funct3_Store;

typedef enum
{
    // NOTE: add, sll, srl, xor, etc.
    RiscV_F7_Base = 0x00,
    // NOTE: M-Extension
    RiscV_F7_M_Ext = 0x01,
    // NOTE: sub, sra
    RiscV_F7_Alt = 0x20
} RiscV_Funct7;

static inline Uint64 RiscV_Op_ReadOffset(Uint32 RegIndex)
{
    return (RegIndex == 0) ? (33 * 8) : (RegIndex * 8);
}

static inline Uint64 RiscV_Op_WriteOffset(Uint32 RegIndex)
{
    return (RegIndex == 0) ? (34 * 8) : (RegIndex * 8);
}

#endif
