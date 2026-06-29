#include "Public/Types.h"
#include "SDL.h"
#include "Zip.h"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        SDL_Log("USAGE %s <Output.zip> <InputFile1> [InputFile2] ...\n", argv[0]);
        return 1;
    }

    const char *OutputZip = argv[1];

    Usize TotalSize = 0;
    for (int I = 2; I < argc; ++I)
    {
        SDL_PathInfo Info;
        if (SDL_GetPathInfo(argv[I], &Info))
        {
            TotalSize += (Usize)Info.size;
        }
        else
        {
            LogCritical("Could not get path information for '%s'.\n", argv[I]);
        }
    }

    Usize Cap = TotalSize + (Usize)(argc - 2) * 512 + 65536;

    Uint8 *ZipBuffer = (Uint8 *)SDL_malloc(Cap);
    if (!ZipBuffer)
    {
        return 1;
    }

    MemStream Writer = ZipWriterInit(ZipBuffer, Cap);
    for (int I = 2; I < argc; ++I)
    {
        const char *InputPath = argv[I];

        Usize FileSize = 0;
        Uint8 *FileData = (Uint8 *)SDL_LoadFile(InputPath, &FileSize);
        if (!FileData)
        {
            LogCritical("%s\n", SDL_GetError());
            continue;
        }

        if (!ZipWriterAppend(&Writer, InputPath, FileData, FileSize))
        {
            return 1;
        }

        SDL_free(FileData);
    }

    Usize ZipSize = ZipWriterFlush(&Writer);
    if (ZipSize == 0 || Writer.HasError)
    {
        return 1;
    }

    if (!SDL_SaveFile(OutputZip, ZipBuffer, ZipSize))
    {
        LogCritical("%s\n", SDL_GetError());
        return 1;
    }

    return 0;
}
