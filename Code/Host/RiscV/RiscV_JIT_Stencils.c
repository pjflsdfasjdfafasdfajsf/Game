#include "Public/Types.h"

//
// NOTE: Not recommended to touch these directly, use the helpers below.
//

__attribute__((visibility("hidden"))) extern Void _Next(Uint64 *Regs);

extern Uint8 _Imm;
extern Uint8 _Rd;
extern Uint8 _Rs1;
extern Uint8 _Rs2;
extern Uint8 _NextPC;

///////////////////////////////////////////////////////////////////////////////

#define Next __attribute__((musttail)) return _Next(Regs)
#define Stencil(name) __attribute__((aligned(1))) void RiscV_JIT_Stencil_##name(Uint64 *Regs)

#define _Hole(name) ((Uint64)(Iptr) & name)
#define _Reg(hole) (*(Uint64 *)((Uint8 *)Regs + _Hole(hole)))

#define Rd _Reg(_Rd)
#define Rs1 _Reg(_Rs1)
#define Rs2 _Reg(_Rs2)
#define PC (*(Uint64 *)((Uint8 *)Regs + (32 * 8)))

#define Imm ((Int64)_Hole(_Imm))
#define NextPC ((Uint64)_Hole(_NextPC))

///////////////////////////////////////////////////////////////////////////////

Stencil(ADDI)
{
    Rd = Rs1 + Imm;
    Next;
}

Stencil(ADDIW)
{
    Rd = (Uint64)(Int64)((Int32)Rs1 + (Int32)Imm);
    Next;
}

Stencil(JALR)
{
    // NOTE: This intermediate variable is important here to avoid weird
    // aliasing stuff
    Uint64 Target = (Rs1 + Imm) & ~1ULL;
    Rd = NextPC;
    PC = Target;
}

Stencil(Exit)
{
    PC = Imm;
}




/*


Disassembly of section .text:

0000000000000000 <RiscV_JIT_Stencil_ADDI>:
   0:	48 c7 c0 00 00 00 00 	mov    $0x0,%rax
			3: R_X86_64_32S	_Imm
   7:	48 03 87 00 00 00 00 	add    0x0(%rdi),%rax
			a: R_X86_64_32S	_Rs1
   e:	48 89 87 00 00 00 00 	mov    %rax,0x0(%rdi)
			11: R_X86_64_32S	_Rd
  15:	e9 00 00 00 00       	jmp    1a <RiscV_JIT_Stencil_ADDIW>
			16: R_X86_64_PLT32	_Next-0x4

000000000000001a <RiscV_JIT_Stencil_ADDIW>:
  1a:	48 c7 c0 00 00 00 00 	mov    $0x0,%rax
			1d: R_X86_64_32S	_Imm
  21:	03 87 00 00 00 00    	add    0x0(%rdi),%eax
			23: R_X86_64_32S	_Rs1
  27:	48 98                	cltq
  29:	48 89 87 00 00 00 00 	mov    %rax,0x0(%rdi)
			2c: R_X86_64_32S	_Rd
  30:	e9 00 00 00 00       	jmp    35 <RiscV_JIT_Stencil_JALR>
			31: R_X86_64_PLT32	_Next-0x4

0000000000000035 <RiscV_JIT_Stencil_JALR>:
  35:	48 c7 c0 00 00 00 00 	mov    $0x0,%rax
			38: R_X86_64_32S	_Imm
  3c:	48 03 87 00 00 00 00 	add    0x0(%rdi),%rax
			3f: R_X86_64_32S	_Rs1
  43:	48 83 e0 fe          	and    $0xfffffffffffffffe,%rax
  47:	48 c7 87 00 00 00 00 	movq   $0x0,0x0(%rdi)
  4e:	00 00 00 00 
			4a: R_X86_64_32S	_Rd
			4e: R_X86_64_32S	_NextPC
  52:	48 89 87 00 01 00 00 	mov    %rax,0x100(%rdi)
  59:	c3                   	ret

000000000000005a <RiscV_JIT_Stencil_Exit>:
  5a:	48 c7 87 00 01 00 00 	movq   $0x0,0x100(%rdi)
  61:	00 00 00 00 
			61: R_X86_64_32S	_Imm
  65:	c3                   	ret

*/
