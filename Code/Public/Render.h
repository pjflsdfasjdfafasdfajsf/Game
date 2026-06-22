#if !defined(RENDER_H)
#define RENDER_H

#include "Mem.h"
#include "Types.h"

#define BUF_ALIGN 4
StaticAssert(IsPow2(BUF_ALIGN));

#define BufAlign(Value) AlignUp(Value, BUF_ALIGN)

// NOTE:
// 1. All positions are in pixels, with origin for everyone being the top-left.
// 2. Intenally renderer uses letterboxing with the resolution of 1280x720
// TODO: Probably will have more to say later

typedef Uint32 TexHandle;
#define TexHandleInvalid 0xFFFFFFFF

typedef enum
{
    RenderCommand_None,
    RenderCommand_Clear,
    RenderCommand_DrawDebugText,
    RenderCommand_DrawRect,
} RenderCommand;

// NOTE: Every command must start with this header for iterators sake.
typedef struct
{
    RenderCommand Type;
    // NOTE: Must store aligned size here!!!
    Uint32 Size;
} RenderHeader;

// NOTE: Clears the entire screen with the specified `Color`.
typedef struct
{
    RenderHeader Header;

    Color Color;
} RenderClear;

// NOTE: There's a reason why this command is called 'draw DEBUG' text and
// that reason is that the implementation of it just uses SDL built-in
// SDL_RenderDebugText function, which provides just simple bitmap
// font rendering intended for, as you probably already guessed,
// debugging.
typedef struct
{
    RenderHeader Header;

    V2 Pos;
    Color Color;
    // NOTE: Scale multiplier.
    V2 Scale;

    Uint32 StrLen;
    // NOTE: The final newline is not appended!
    char Str[];
} RenderDrawDebugText;

typedef struct
{
    RenderHeader Header;

    // NOTE: This can be TexHandleInvalid for a regular untextured rectangle.
    TexHandle Tex;

    // NOTE: Where on screen to draw the rectangle.
    Rect Dst;
    // NOTE: Which part of the texture draw. If Src.Pos is 0,0, then the entire
    // image is drawn.
    // This is fully ignored if Tex is TexHandleInvalid.
    Rect Src;

    // NOTE: If Tex is TexHandleInvalid then it's the color of the rectangle.
    // Otherwise acts as a tint. For unmodified keep it as White.
    Color Color;
} RenderDrawRect;

typedef struct
{
    Uint8 *Mem;
    Uint32 Size;
    Uint32 Cap;

    Bool IsValid;
} RenderBuf;

static inline Void *RenderBufPush(RenderBuf *RenderBuf, Uint32 Bytes)
{
    Assert(RenderBuf);
    Assert(Bytes >= sizeof(RenderHeader)); // NOTE: Always must be room for the header.

    Uint32 Aligned = BufAlign(Bytes);

    Bool HasSpace = (RenderBuf->Size + Aligned) <= RenderBuf->Cap;
    Assert(HasSpace);

    if (HasSpace)
    {
        Uint32 Offset = BufAlign(RenderBuf->Size);
        Void *Result = RenderBuf->Mem + Offset;
        RenderBuf->Size = Offset + Aligned;

        return Result;
    }

    return 0;
}

static inline Void RenderBufClear(RenderBuf *RenderBuf, Color Color)
{
    RenderClear *Cmd = (RenderClear *)RenderBufPush(RenderBuf, sizeof(RenderClear));

    if (Cmd)
    {
        Cmd->Header.Type = RenderCommand_Clear;
        Cmd->Header.Size = BufAlign(sizeof(RenderClear));

        Cmd->Color = Color;
    }
}

static inline Void RenderBufDrawDebugText(RenderBuf *RenderBuf, Color Color, V2 Pos, V2 Scale, const char *Str, Uint32 StrLen)
{
    if (!Str || StrLen == 0)
    {
        return;
    }

    Uint32 StrCap = StrLen + 1;
    Uint32 Size = sizeof(RenderDrawDebugText) + StrCap;
    RenderDrawDebugText *Cmd = (RenderDrawDebugText *)RenderBufPush(RenderBuf, Size);

    if (Cmd)
    {
        Cmd->Header.Type = RenderCommand_DrawDebugText;
        Cmd->Header.Size = BufAlign(Size);

        Cmd->Pos = Pos;
        Cmd->Color = Color;
        Cmd->Scale = Scale;

        MemCopy(Cmd->Str, Str, StrLen);
        MemNullTerminate(Cmd->Str, StrCap, StrLen);
    }
}

static inline Void RenderBufDrawCStr(RenderBuf *RenderBuf, Color Color, V2 Pos, V2 Scale, const char *Str)
{
    RenderBufDrawDebugText(RenderBuf, Color, Pos, Scale, Str, CStrLen(Str));
}

// NOTE: If you're thinking 'It is too inconvinient to pass Dst and Src
// everytime!' then you're probably doing something wrong. Standard untextured
// rectangles shouldn't be a thing you do very often and if you're actually
// using spritesheets like intended it should not seem as inconvinient.
static inline Void RenderBufDrawRect(RenderBuf *RenderBuf, TexHandle Tex, Rect Dst, Rect Src, Color Color)
{
    RenderDrawRect *Cmd = (RenderDrawRect *)RenderBufPush(RenderBuf, sizeof(RenderDrawRect));

    if (Cmd)
    {
        Cmd->Header.Type = RenderCommand_DrawRect;
        Cmd->Header.Size = BufAlign(sizeof(RenderDrawRect));

        Cmd->Tex = Tex;
        Cmd->Dst = Dst;
        Cmd->Src = Src;
        Cmd->Color = Color;
    }
}

static inline Void RenderBufReset(RenderBuf *RenderBuf)
{
    RenderBuf->Size = 0;
}

static inline RenderBuf RenderBufInit(MemAlloc *MemAlloc, Uint32 Cap)
{
    RenderBuf Result = {};

    Assert(MemAlloc);
    Assert(Cap > 0);

    Result.Mem = (Uint8 *)MemAllocPush(MemAlloc, Cap, BUF_ALIGN);
    if (Result.Mem)
    {
        Result.Size = 0;
        Result.Cap = Cap;
        Result.IsValid = True;

        return Result;
    }

    return Result;
}

// NOTE: This is used only internally. You do not need to port this.
#define RenderBufIterate(BufPtr, CmdPtrName)                       \
    for (RenderHeader *CmdPtrName = (RenderHeader *)(BufPtr)->Mem; \
         (Uint8 *)CmdPtrName < (BufPtr)->Mem + (BufPtr)->Size;     \
         CmdPtrName = (RenderHeader *)((Uint8 *)CmdPtrName + CmdPtrName->Size))

//
// NOTE: Atlas utilities.
//

#define ATLAS_MAX_NAME 64

typedef struct
{
    char Name[ATLAS_MAX_NAME];
    Rect Src;
} AtlasEntry;

typedef struct
{
    TexHandle Tex;

    AtlasEntry *Entries;
    Uint32 Count;
    Uint32 Cap;

    Bool IsValid;
} Atlas;

// NOTE: Meta is expected to use the same format generated by AtlasPack:
//
// <image_path> <x_position> <y_position> <width> <height>
static inline Atlas AtlasInit(MemAlloc *Alloc, TexHandle Tex, const char *Meta, Uint32 MetaLen)
{
    Atlas Result = {0};
    Assert(Alloc);

    if (!Meta || MetaLen == 0)
    {
        return Result;
    }

    Result.Tex = Tex;

    Uint32 LineCount = 0;
    for (Uint32 I = 0; I < MetaLen; ++I)
    {
        if (Meta[I] == '\n')
        {
            LineCount++;
        }
    }

    if (Meta[MetaLen - 1] != '\n')
    {
        LineCount++;
    }

    if (LineCount == 0)
    {
        return Result;
    }

    Result.Entries = (AtlasEntry *)MemAllocPush(Alloc, sizeof(AtlasEntry) * LineCount, 4);
    if (!Result.Entries)
    {
        return Result;
    }
    Result.Cap = LineCount;

    const char *Cur = Meta;
    const char *End = Meta + MetaLen;

    while (Cur < End && Result.Count < Result.Cap)
    {
        const char *Start = Cur;

        while (Cur < End && *Cur != ' ' && *Cur != '\n')
        {
            Cur++;
        }

        if (Cur >= End || *Cur == '\n')
        {
            MemAdvanceToNextLine(&Cur, End);
            continue;
        }

        Uint32 PathLen = (Uint32)(Cur - Start);
        const char *BaseName = GetBaseName(Start, PathLen);
        Uint32 BaseLen = (Uint32)(Cur - BaseName);

        AtlasEntry *Entry = &Result.Entries[Result.Count++];

        Uint32 CopyLen = BaseLen < (ATLAS_MAX_NAME - 1) ? BaseLen : (ATLAS_MAX_NAME - 1);
        MemCopy(Entry->Name, BaseName, CopyLen);
        MemNullTerminate(Entry->Name, ATLAS_MAX_NAME, CopyLen);

        Entry->Src.Pos.X = (Float32)MemReadUint(&Cur);
        Entry->Src.Pos.Y = (Float32)MemReadUint(&Cur);
        Entry->Src.Size.W = (Float32)MemReadUint(&Cur);
        Entry->Src.Size.H = (Float32)MemReadUint(&Cur);

        MemAdvanceToNextLine(&Cur, End);
    }

    Result.IsValid = True;
    return Result;
}

static inline Rect AtlasGetRect(Atlas *Atlas, const char *Name)
{
    Rect Result = {0};

    Assert(Atlas);
    Assert(Name);

    if (!Atlas->IsValid)
    {
        return Result;
    }

    Uint32 NameLen = CStrLen(Name);
    for (Uint32 I = 0; I < Atlas->Count; ++I)
    {
        AtlasEntry *Entry = &Atlas->Entries[I];

        Bool Match = True;
        for (Uint32 C = 0; C <= NameLen; ++C)
        {
            if (Entry->Name[C] != Name[C])
            {
                Match = False;
                break;
            }
        }

        if (Match)
        {
            return Entry->Src;
        }
    }

    return Result;
}

#endif
