#if !defined(INPUTMAP_H)
#define INPUTMAP_H

#include "SDK.h"

namespace inputmap {

struct Binding {
  char key[64];
  char action[64];
};

struct Bindings {
  Binding *items;
  Uint32 count;
  Uint32 capacity;

  static Bindings Initialize(Uint32 capacity);

  void Read(const char *file_content);

  void AddOrUpdate(const char *action, const char *key);
};

struct String {
  const char *text;
  Uint32 length;

  bool IsEmpty() const;

  bool ContainsSpace() const;

  void CopyToBuffer(char *buffer, Uint32 size) const;

  String TrimWhitespace();
};



} // namespace inputmap

#endif
