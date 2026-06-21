#if !defined(HOST_H)
#define HOST_H

#include "wasm3.h"

#include "Types.h"
#include "SDK.h"

typedef struct {
  IM3Environment Env;
  IM3Runtime Runtime;
  IM3Module Module;

  IM3Function UpdateAndRender;
  IM3Function GetExtraMem;
  IM3Function GetState;
  IM3Function GetRenderBuf;

  // NOTE: NOT REAL HOST POINTERS!!!!
  Uint32 State;
  Uint32 ExtraMem;
  Uint32 RenderBuf;

  void *Bytes;

  Bool IsValid;
} Host;

Host HostInit(Void);

Bool HostLoadOne(Host *Host, const char *File);

Bool HostUpdate(Host *Host, State *State, RenderBuf *RenderBuf);

#endif
