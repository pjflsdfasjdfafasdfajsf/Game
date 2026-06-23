//
// NOTE: Simple uncompressed .mod archiver tool.
//
// TODO: Despite the name as it was already mentioned there is actually no
// compression involved in the format.
//

// NOTE:
// * RelPath is path to the file inside the archive, for example 'mod.wasm'
// * AbsPath is the full OS path, 'Mods/ExampleMod/Image.png'

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "Mem.h"
#include "Types.h"

//
// NOTE: Constants
//

#define MaxFiles 512

#define RelPathMax 128
#define AbsPathMax 512

#define MagicStr "PMOD"

//
// NOTE: Internal
//

#pragma pack(push, 1)

typedef struct
{
    // NOTE: See Magic define
    char Magic[4];
    // NOTE: 1
    Uint32 Version;
    // NOTE: Amount of files in the archive
    Uint32 Count;
} Header;

typedef struct
{
    char RelPath[RelPathMax];
    // NOTE: Byte offset from the start of the archive
    Uint32 Offset;
    Uint32 Size;
} FileEntry;

#pragma pack(pop)

typedef struct
{
    char AbsPath[AbsPathMax];
    char RelPath[RelPathMax];
    Usize Size;
} FileToPack;

typedef struct
{
    FileToPack Files[MaxFiles];
    Uint32 Count;
} PackCtx;

static Bool IterDir(const char *DirName, const char *BaseDir, PackCtx *OutCtx)
{
    Assert(DirName);
    Assert(BaseDir);
    Assert(OutCtx);

    DIR *Dir = opendir(DirName);
    if (!Dir)
    {
        return False;
    }

    struct dirent *DirEnt;
    while ((DirEnt = readdir(Dir)) != 0)
    {
        if (strcmp(DirEnt->d_name, ".") == 0 || strcmp(DirEnt->d_name, "..") == 0)
        {
            continue;
        }

        char AbsPath[AbsPathMax];
        const int AbsPathLen = snprintf(AbsPath, sizeof(AbsPath), "%s/%s", DirName, DirEnt->d_name);
        if (AbsPathLen < 0 || (Usize)AbsPathLen >= sizeof(AbsPath))
        {
            return False;
        }

        struct stat Stat;
        if (stat(AbsPath, &Stat) != 0)
        {
            continue;
        }

        if (S_ISDIR(Stat.st_mode))
        {
            if (!IterDir(AbsPath, BaseDir, OutCtx))
            {
                return False;
            }
        }
        else if (S_ISREG(Stat.st_mode))
        {
            if (OutCtx->Count >= MaxFiles)
            {
                return False;
            }

            FileToPack *File = &OutCtx->Files[OutCtx->Count];
            strncpy(File->AbsPath, AbsPath, sizeof(File->AbsPath));
            MemNullTerminate(File->AbsPath, sizeof(File->AbsPath), (Usize)AbsPathLen);

            // NOTE: Rel path
            const Usize BaseLen = strlen(BaseDir);
            const char *RelPtr = AbsPath + BaseLen;
            if (*RelPtr == '/' || *RelPtr == '\\')
            {
                RelPtr++;
            }

            const Usize RelLen = strlen(RelPtr);
            strncpy(File->RelPath, RelPtr, sizeof(File->RelPath));
            MemNullTerminate(File->RelPath, sizeof(File->RelPath), RelLen);

            File->Size = (Usize)Stat.st_size;
            OutCtx->Count++;
        }
    }

    return True;
}

Int32 main(Int32 Argc, char **Argv)
{
    if (Argc < 3)
    {
        printf("USAGE: %s <input_directory> <output_file_path>", Argv[0]);

        return 1;
    }

    const char *InputDir = Argv[1];
    const char *OutFile = Argv[2];

    PackCtx *Ctx = calloc(1, sizeof(PackCtx));
    if (!Ctx)
    {
        return 1;
    }

    if (!IterDir(InputDir, InputDir, Ctx))
    {
        return 1;
    }

    {
        FILE *Dest = fopen(OutFile, "wb");
        if (!Dest)
        {
            return 1;
        }

        Header Header = {0};
        memcpy(Header.Magic, MagicStr, 4);
        Header.Version = 1;
        Header.Count = Ctx->Count;

        FileEntry *Entries = calloc(Ctx->Count, sizeof(FileEntry));
        if (!Entries)
        {
            return 1;
        }

        Uint32 CurOffset = sizeof(Header) + (Ctx->Count * sizeof(FileEntry));

        for (Uint32 I = 0; I < Ctx->Count; ++I)
        {
            const FileToPack *File = &Ctx->Files[I];
            FileEntry *Entry = &Entries[I];

            strncpy(Entry->RelPath, File->RelPath, sizeof(Entry->RelPath));
            MemNullTerminate(Entry->RelPath, sizeof(Entry->RelPath), strlen(File->RelPath));
            Entry->Offset = CurOffset;
            Entry->Size = (Uint32)File->Size;

            CurOffset += Entry->Size;
        }

        if (fwrite(&Header, sizeof(Header), 1, Dest) != 1)
        {
            return 1;
        }

        if (fwrite(Entries, sizeof(FileEntry), Ctx->Count, Dest) != Ctx->Count)
        {
            return 1;
        }

        for (Uint32 I = 0; I < Ctx->Count; ++I)
        {
            const FileToPack *File = &Ctx->Files[I];
            FILE *Src = fopen(File->AbsPath, "rb");

            if (!Src)
            {
                return 1;
            }

            Uint8 Buf[8192];
            Usize BytesRead = 0;
            while ((BytesRead = fread(Buf, 1, sizeof(Buf), Src)) > 0)
            {
                if (fwrite(Buf, 1, BytesRead, Dest) != BytesRead)
                {
                    return 1;
                }
            }

            fclose(Src);
        }
    }

    return 0;
}
