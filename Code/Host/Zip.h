//
// NOTE: ZIP archive reader. Used for mods.
//
#if !defined(HOST_ZIP_H)
#define HOST_ZIP_H

#include "Public/Mem.h"
#include "Public/Types.h"

typedef struct
{
    // NOTE: Non null terminated pointer inside archive
    const Uint8 *NamePtr;
    Uint16 NameLen;
    Uint16 CompressionMethod;
    Uint32 CompressedSize;
    Uint32 UncompressedSize;
    // NOTE: Offset to the Local File Header
    Uint32 LocalHeaderOffset;
    Uint32 Crc32;
} Zip_Entry;

typedef struct
{
    const Uint8 *Mem;
    Usize Size;
    // NOTE: Offset to the start of the Central Directory
    Uint32 CdOffset;
    // NOTE: Total bytes in the Central Directory
    Uint32 CdSize;
    // NOTE: Total entries in the archive
    Uint16 Count;
} Zip_Archive;

// NOTE: Reader

Result Zip_Open(const Uint8 *Mem, Usize Size, Zip_Archive *OutZip);

Result Zip_Entry_GetByIndex(const Zip_Archive *Zip, Uint32 Index, Zip_Entry *OutEnt);
Result Zip_Entry_GetByName(const Zip_Archive *Zip, const char *File, Zip_Entry *OutEnt);

Result Zip_Entry_Read(const Zip_Archive *Zip, const Zip_Entry *Ent, Uint8 *Buf, Usize BufSize);
// NOTE: Returns true if Ent name ends with Suffix
Bool Zip_Entry_EndsWith(const Zip_Entry *Ent, const char *Suffix);

// NOTE: Writer

Mem_Stream Zip_Writer_Init(Void *Buf, Usize Cap);
Result Zip_Writer_Append(Mem_Stream *S, const char *Name, const Uint8 *Mem, Usize Size);
Result Zip_Writer_Flush(Mem_Stream *S, Usize *OutWritten);

#endif
