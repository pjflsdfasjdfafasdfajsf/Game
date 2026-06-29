#include "Public/Types.h"
#include "SDL.h"
#include "Zip.h"

Int32 main()
{
    SDL_Log("\n================================\n\n");

    const char *Path = "Build/Test/Data/Archive/Hello.txt";

    Uint8 *ZipBuffer = (Uint8 *)SDL_malloc(Mb(10));
    if (!ZipBuffer)
    {
        return 1;
    }

    MemStream Writer = ZipWriterInit(ZipBuffer, Mb(10));

    Usize FileSize = 0;
    Uint8 *FileData = (Uint8 *)SDL_LoadFile(Path, &FileSize);
    if (!FileData)
    {
        return 1;
    }

    if (!ZipWriterAppend(&Writer, Path, FileData, FileSize))
    {
        return 1;
    }
    SDL_free(FileData);

    Usize ZipSize = ZipWriterFlush(&Writer);
    if (ZipSize == 0 || Writer.HasError)
    {
        return 1;
    }

    if (!SDL_SaveFile("Build/Test/Data/Archive.zip", ZipBuffer, ZipSize))
    {
        LogCritical("%s\n", SDL_GetError());
    }

    ZipArchive Zip = ZipOpen(ZipBuffer, ZipSize);
    if (!Zip.IsValid)
    {
        return 1;
    }

    for (Uint32 I = 0; I < Zip.Count; I++)
    {
        ZipEntry Entry = ZipGetEntByIndex(&Zip, I);
        if (Entry.IsValid)
        {
            SDL_Log("%u: %.*s (%u uncompressed bytes, %u compressed bytes)\n",
                    I, (int)Entry.NameLen, (const char *)Entry.NamePtr, Entry.UncompressedSize, Entry.CompressedSize);
        }
    }

    ZipEntry Entry = ZipGetEntByName(&Zip, Path);
    if (Entry.IsValid)
    {
        Uint8 *Buffer = (Uint8 *)SDL_malloc(Entry.UncompressedSize + 1);
        if (Buffer)
        {
            if (ZipReadEnt(&Zip, &Entry, Buffer, Entry.UncompressedSize))
            {
                Buffer[Entry.UncompressedSize] = '\0';
                SDL_Log("%s", (char *)Buffer);
            }
        }
    }

    return 0;
}
