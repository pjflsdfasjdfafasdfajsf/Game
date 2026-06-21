//
// NOTE: This is more of a build-time utility though not exactly. Cross-platform file watching (used in in Watch target)
//

#if !defined(__linux__)
#error "TODO: Unsuported platform for file watching"
#endif

#include "Types.h"

#include <stdio.h>
#include <stdlib.h>

//
// NOTE: Interface
// 

typedef Void *WatchHandle;

WatchHandle WatchInit(const char *Dir);
Bool WatchWait(WatchHandle Handle);

// NOTE:
// Arguments:
// 1. What directory to watch
// 2. What to do when a file changes in that directory (command)
Int32 main(Int32 Argc, char **Argv)
{
    if (Argc != 3)
    {
        printf("USAGE: %s <directory_to_watch> <command_to_run>\n", Argv[0]);

        return 1;
    }

    const char *Dir = Argv[1];
    const char *Cmd = Argv[2];

    WatchHandle Watch = WatchInit(Dir);
    if (!Watch)
    {
        return 1;
    }

    while (True)
    {
        if (!WatchWait(Watch))
        {
            break;
        }

        system(Cmd);
    }

    return 0;
}

//
// NOTE: Linux
// 

#if defined(__linux__)

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

typedef struct
{
    int Inotify;
    int Watch;
} Context;

WatchHandle WatchInit(const char *Dir)
{
    Assert(Dir);

    Context *Ctx = (Context *)malloc(sizeof(Context));
    if (!Ctx)
    {
        return 0;
    }

    Ctx->Inotify = inotify_init();
    if (Ctx->Inotify < 0)
    {
        return 0;
    }

    Ctx->Watch = inotify_add_watch(Ctx->Inotify, Dir, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (Ctx->Watch < 0)
    {
        return 0;
    }

    return (WatchHandle)Ctx;
}

Bool WatchWait(WatchHandle Handle)
{
    Assert(Handle);
    Context *Ctx = (Context *)Handle;

    const Usize BufSize = 1024 * (sizeof(struct inotify_event) + 16);
    char Buf[BufSize];

    // NOTE: There is a bunch of funny logic for debouncing. Otherwise changing a file would produce two events,
    // probably related to how text editors work.

    struct pollfd Fd;
    Fd.fd = Ctx->Inotify;
    Fd.events = POLLIN;

    Int32 Poll = poll(&Fd, 1, -1);
    if (Poll < 0)
    {
        return False;
    }

    if (read(Ctx->Inotify, Buf, BufSize) < 0)
    {
        return False;
    }

    while (True)
    {
        Poll = poll(&Fd, 1, 100);
        if (Poll > 0)
        {
            if (read(Ctx->Inotify, Buf, BufSize) < 0)
            {
                return False;
            }
        }
        else
        {
            break;
        }
    }

    return True;
}

#endif
