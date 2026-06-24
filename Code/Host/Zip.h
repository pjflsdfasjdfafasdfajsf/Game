//
// NOTE: ZIP archive reader. Used for mods.
//
#if !defined(ZIP_H)
#define ZIP_H

#include "Types.h"

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

    Bool IsValid;
} ZipEntry;

typedef struct
{
    const Uint8 *Mem;
    Usize Size;
    // NOTE: Offset to the start of the Central Directory
    Uint32 CdOffset;
    // NOTE: Total bytes in the Central Directory
    Uint32 CdSize;
    // NOTE: Total tnries in the archive
    Uint16 Count;

    Bool IsValid;
} ZipArchive;

ZipArchive ZipOpen(const Uint8 *Mem, Usize Size);

ZipEntry ZipGetEntByIndex(const ZipArchive *Zip, Uint32 Index);
ZipEntry ZipGetEntByName(const ZipArchive *Zip, const char *File);

Bool ZipReadEnt(const ZipArchive *Zip, const ZipEntry *Ent, Uint8 *Buf, Usize BufSize);

#endif
