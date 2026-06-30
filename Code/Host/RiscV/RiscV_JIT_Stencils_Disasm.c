#include "Public/Types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Int32 main(Int32 Argc, char **Argv)
{
    if (Argc < 3)
    {
        fprintf(stderr, "USAGE: %s <compiled file> <source>\n", Argv[0]);
        return 1;
    }

    if (system("objdump --version > /dev/null 2>&1") != 0)
    {
        return 0;
    }

    const char *Bin = Argv[1];
    const char *Source = Argv[2];

    FILE *File = fopen(Source, "rb");
    if (!File)
    {
        return 1;
    }

    fseek(File, 0, SEEK_END);
    long Size = ftell(File);
    fseek(File, 0, SEEK_SET);

    char *Buf = (char *)malloc(Size + 1);
    if (!Buf)
    {
        fclose(File);
        return 1;
    }

    fread(Buf, 1, Size, File);
    Buf[Size] = '\0';
    fclose(File);

    char *Disasm = strstr(Buf, "Disassembly of section");
    if (Disasm)
    {
        while (Disasm > Buf && !(Disasm[0] == '/' && Disasm[1] == '*'))
        {
            Disasm--;
        }
        *Disasm = '\0';
    }

    FILE *Out = fopen(Source, "w");
    if (!Out)
    {
        free(Buf);
        return 1;
    }

    fputs(Buf, Out);
    free(Buf);

    fprintf(Out, "\n/*\n");

    char Objdump[512];
    snprintf(Objdump, sizeof(Objdump), "objdump -d -r %s 2> /dev/null", Bin);
    FILE *Pipe = popen(Objdump, "r");
    if (Pipe)
    {
        char Buf[1024];
        // NOTE: Blank line
        fgets(Buf, sizeof(Buf), Pipe);
        // NOTE: Cache path
        fgets(Buf, sizeof(Buf), Pipe);

        while (fgets(Buf, sizeof(Buf), Pipe))
        {
            fputs(Buf, Out);
        }
        pclose(Pipe);
    }

    fprintf(Out, "\n*/\n");
    fclose(Out);

    return 0;
}
