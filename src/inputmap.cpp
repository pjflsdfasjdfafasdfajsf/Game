#include "inputmap.h"
#include "SDK.h"

#include <SDL3/SDL.h>

namespace inputmap {

//
// NOTE: Private.
// 

bool String::IsEmpty() const { return this->length == 0; }

bool String::ContainsSpace() const {
  for (Uint32 i = 0; i < this->length; ++i) {
    if (SDL_isspace((unsigned char)this->text[i])) {
      return true;
    }
  }

  return false;
}

String String::TrimWhitespace() {
  while (this->length > 0 && SDL_isspace((unsigned char)this->text[0])) {
    this->text++;
    this->length--;
  }
  while (this->length > 0 &&
         SDL_isspace((unsigned char)this->text[this->length - 1])) {
    this->length--;
  }
  return *this;
}

void String::CopyToBuffer(char *buffer, Uint32 size) const {
  Assert(buffer);
  Assert(size > 0);

  SDL_memset(buffer, 0, size);
  Uint32 length = this->length;

  if (length >= size) {
    // NOTE: Reserve some space for our boy null terminator.
    length = size - 1;
  }

  SDL_memcpy(buffer, this->text, length);
}

void Bindings::AddOrUpdate(const char *action, const char *key) {
  Assert(action);
  Assert(key);

  if (!this->items) {
    return;
  }

  for (Uint32 i = 0; i < this->count; ++i) {
    if (SDL_strcmp(this->items[i].key, key) == 0) {
      SDL_strlcpy(this->items[i].action, action, sizeof(this->items[i].action));
      return;
    }
  }

  if (this->count >= this->capacity) {
    Uint32 capacity = this->capacity * 2;
    Binding *items =
        (Binding *)SDL_realloc(this->items, capacity * sizeof(Binding));

    if (!items) {
      SDL_Log("out of memory\n");
      return;
    }

    this->items = items;
    this->capacity = capacity;
  }

  SDL_strlcpy(this->items[this->count].action, action,
              sizeof(this->items[this->count].action));
  SDL_strlcpy(this->items[this->count].key, key,
              sizeof(this->items[this->count].key));
  this->count++;
}

void ReadKeys(String action, String keys, Bindings &bindings) {
  const char *current = keys.text;
  const char *end = keys.text + keys.length;

  char action_buffer[64];
  action.CopyToBuffer(action_buffer, sizeof(action_buffer));

  while (current < end) {
    const char *comma = current;
    while (comma < end && *comma != ',') {
      comma++;
    }

    String key;
    key.text = current;
    key.length = (Uint32)(comma - current);
    key = key.TrimWhitespace();

    if (key.IsEmpty()) {
      SDL_Log("this key segment will be skipped as there is an empty key or a "
              "trailing comma\n");
    } else {
      char key_buffer[64];
      key.CopyToBuffer(key_buffer, sizeof(key_buffer));
      bindings.AddOrUpdate(action_buffer, key_buffer);
    }

    current = comma + 1;
  }
}

void ReadLine(String line, Bindings &bindings) {
  for (Uint32 i = 0; i < line.length; ++i) {
    if (line.text[i] == '#') {
      line.length = i;
      break;
    }
  }

  line = line.TrimWhitespace();
  if (line.IsEmpty()) {
    return;
  }

  const char *equals = 0;
  for (Uint32 i = 0; i < line.length; ++i) {
    if (line.text[i] == '=') {
      equals = line.text + i;
      break;
    }
  }

  if (!equals) {
    SDL_Log("line missing '='\n");
    return;
  }

  // NOTE: Left of the equals.
  String action;
  action.text = line.text;
  action.length = (Uint32)(equals - line.text);
  action = action.TrimWhitespace();

  if (action.IsEmpty()) {
    SDL_Log("empty action name\n");
    return;
  }

  if (action.ContainsSpace()) {
    SDL_Log("action name contains spaces\n");
    return;
  }

  // NOTE: Right of the equals.
  String keys;
  keys.text = equals + 1;
  keys.length = (Uint32)((line.text + line.length) - keys.text);

  ReadKeys(action, keys, bindings);
}

//
// NOTE: Implementation.
// 

Bindings Bindings::Initialize(Uint32 capacity) {
  Assert(capacity > 0);

  Bindings bindings = {};
  bindings.items = (Binding *)SDL_calloc(capacity, sizeof(Binding));
  bindings.capacity = capacity;
  bindings.count = 0;

  return bindings;
}

void Bindings::Read(const char *file_content) {
  Assert(file_content);
  // NOTE: For Release.
  if (!file_content) {
    return;
  }

  const char *current = file_content;

  while (*current != '\0') {
    const char *end = current;

    while (*end != '\0' && *end != '\n' && *end != '\r') {
      end++;
    }

    String line;
    line.text = current;
    line.length = (Uint32)(end - current);

    ReadLine(line, *this);

    if (*end == '\r' && *(end + 1) == '\n') {
      current = end + 2;
    } else if (*end != '\0') {
      current = end + 1;
    } else {
      current = end;
    }
  }
}

}; // namespace inputmap
