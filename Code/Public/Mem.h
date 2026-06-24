//
// NOTE: Memory utilities.
//
#if !defined(MEM_H)
#define MEM_H

#include "Types.h"

// NOTE: This is a simple bump allocator.
typedef struct
{
    Uint8 *Base;
    Uint32 Cap;
    Uint32 Used;
} MemAlloc;

MemAlloc MemAllocInit(void *Mem, Uint32 Cap);

Void *MemAllocPush(MemAlloc *MemAlloc, Uint32 Bytes, Uint32 Align);
Void MemAllocClear(MemAlloc *MemAlloc);


//
// NOTE: String utilities
//

Uint32 CStrLen(const char *CStr);

//
// NOTE: Memory utilities
//

Void MemCopy(Void *DestInit, const Void *SrcInit, Usize Size);
Void MemNullTerminate(char *Buf, Usize Cap, Usize Len);

Uint32 MemReadUint(const char **CurInit);

Void MemAdvanceToNextLine(const char **CurInit, const char *End);


//
// NOTE: Misc
//

// NOTE: From a path 'Foo/Bar/Baz.png' returns 'Baz.png'.
const char *GetBaseName(const char *Path, Uint32 Len);

#endif
