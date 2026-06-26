// TODO: Test
#include "KeyValue.h"
#include "Mem.h"
#include "Types.h"

//
// NOTE: Internal
//

static Bool IsWhitespace(char C)
{
    Bool Result = (C == ' ' || C == '\t' || C == '\r' || C == '\n');
    return Result;
}

static Void TrimString(const char *StartInit, const char *EndInit, const char **OutStart, Uint32 *OutLen)
{
    Assert(StartInit);
    Assert(EndInit >= StartInit);

    const char *Start = StartInit;
    const char *End = EndInit;

    while (Start < End && IsWhitespace(*Start))
    {
        Start++;
    }

    while (End > Start && IsWhitespace(*(End - 1)))
    {
        End--;
    }

    *OutStart = Start;
    *OutLen = (Uint32)(End - Start);
}

static Uint32 ParsePass(KeyValuePair *OutPairs, Uint32 MaxCount, Bool *OutHasError, const char *Src, Uint32 SrcLen)
{
    Assert(OutHasError);
    Assert(Src);

    Uint32 Count = 0;
    const char *Cur = Src;
    const char *End = Src + SrcLen;

    while (Cur < End)
    {
        const char *LineStart = Cur;
        const char *LineEnd = MemFindChar(Cur, End, '\n');
        if (!LineEnd)
        {
            LineEnd = End;
        }

        MemAdvanceToNextLine(&Cur, End);

        const char *TrimmedStart;
        Uint32 TrimmedLen;
        TrimString(LineStart, LineEnd, &TrimmedStart, &TrimmedLen);

        if (TrimmedLen == 0 || TrimmedStart[0] == '#')
        {
            continue;
        }

        const char *Eq = MemFindChar(TrimmedStart, TrimmedStart + TrimmedLen, '=');
        if (!Eq)
        {
            *OutHasError = True;
            continue;
        }

        const char *KeyStart;
        Uint32 KeyLen;
        TrimString(TrimmedStart, Eq, &KeyStart, &KeyLen);

        if (KeyLen == 0)
        {
            *OutHasError = True;
            continue;
        }

        if (OutPairs && Count < MaxCount)
        {
            const char *ValueStart;
            Uint32 ValueLen;
            TrimString(Eq + 1, TrimmedStart + TrimmedLen, &ValueStart, &ValueLen);

            KeyValuePair *Pair = &OutPairs[Count];
            Pair->Key = KeyStart;
            Pair->KeyLen = KeyLen;
            Pair->Value = ValueStart;
            Pair->ValueLen = ValueLen;
        }

        Count++;
    }

    return Count;
}

//
// NOTE: Implementation
//

KeyValueList KeyValueParse(MemAlloc *Alloc, const char *Src, Uint32 SrcLen)
{
    KeyValueList Result = {0};

    Assert(Alloc);
    if (!Src || SrcLen == 0)
    {
        Result.HasError = (!Src && SrcLen > 0);
        return Result;
    }

    Bool HasError = False;

    Uint32 PairCount = ParsePass(0, 0, &HasError, Src, SrcLen);

    KeyValuePair *Pairs = 0;
    if (PairCount > 0)
    {
        Pairs = (KeyValuePair *)MemAllocPush(Alloc, PairCount * sizeof(KeyValuePair));
        if (!Pairs)
        {
            Result.HasError = True;
            return Result;
        }

        ParsePass(Pairs, PairCount, &HasError, Src, SrcLen);
    }

    Result.Pairs = Pairs;
    Result.Count = PairCount;
    Result.HasError = HasError;

    return Result;
}

KeyValuePair KeyValueListFind(KeyValueList List, const char *Key, Uint32 KeyLen)
{
    Assert(Key);

    for (Uint32 I = 0; I < List.Count; ++I)
    {
        KeyValuePair Pair = List.Pairs[I];

        if (Pair.KeyLen == KeyLen && MemEql(Pair.Key, Key, KeyLen))
        {
            return Pair;
        }
    }

    return (KeyValuePair){0};
}
