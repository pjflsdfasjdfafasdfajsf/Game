#include "Types.h"
#include "Zip.h"

#include <stdio.h>
#include <stdlib.h>

static const Uint8 Mem[] = {
#embed "Archive.zip"
};

Int32 main()
{
    ZipArchive Zip = ZipOpen(Mem, sizeof(Mem));
    if (!Zip.IsValid)
    {
        printf("ERROR: Invalid test data.\n");

        return 1;
    }

    for (Uint32 I = 0; I < Zip.Count; I++)
    {
        ZipEntry Entry = ZipGetEntByIndex(&Zip, I);
        if (Entry.IsValid)
        {
            printf("%u: %.*s (%u bytes)\n",
                   I, (int)Entry.NameLen, (const char *)Entry.NamePtr, Entry.UncompressedSize);
        }
    }

    ZipEntry Entry = ZipGetEntByName(&Zip, "Build/Test/Data/Archive/Hello.txt");
    if (Entry.IsValid)
    {
        Uint8 *Buffer = malloc(Entry.UncompressedSize + 1);
        if (Buffer)
        {
            if (ZipReadEnt(&Zip, &Entry, Buffer, Entry.UncompressedSize))
            {
                Buffer[Entry.UncompressedSize] = '\0';
                printf("%s", (char *)Buffer);
            }
            else
            {
                printf("ERROR: Extraction failed.\n");
                return 1;
            }
        }
        else
        {
            return 1;
        }
    }
    else
    {
        printf("ERROR: Invalid test data.\n");
    }

    return 0;
}
