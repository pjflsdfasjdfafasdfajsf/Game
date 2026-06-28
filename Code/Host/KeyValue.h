//
// NOTE: Parser for a simple format:
//
//     # This is a comment.
//     Key = Value
//
#if !defined(HOST_KEY_VALUE_H)
#define HOST_KEY_VALUE_H

#include "Public/Mem.h"
#include "Public/Types.h"

typedef struct
{
    const char *Key;
    Uint32 KeyLen;

    const char *Value;
    Uint32 ValueLen;
} KeyValuePair;

typedef struct
{
    KeyValuePair *Pairs;
    Uint32 Count;

    Bool HasError;
} KeyValueList;

KeyValueList KeyValueParse(MemAlloc *Alloc, const char *Src, Uint32 SrcLen);
KeyValuePair KeyValueListFind(KeyValueList List, const char *Key, Uint32 KeyLen);

#endif
