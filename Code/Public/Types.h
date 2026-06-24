//
// NOTE: Shared types.
//
#if !defined(TYPES_H)
#define TYPES_H

#include <stddef.h>
#include <stdint.h>

// NOTE: Singed integer types.
typedef int32_t Int32;
typedef int64_t Int64;
// NOTE: Unsigned integer types.
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
// NOTE: Other.
typedef Int32 Bool;
typedef void Void;
typedef float Float32;
typedef double Float64;
typedef size_t Usize;

#define True 1
#define False 0

//
// NOTE: Compilerz.
//

#if defined(_MSC_VER)
#define MSVC
#elif defined(__GNUC__)
#define GCC
#elif defined(__clang__)
#define CLANG
#endif

//
// NOTE: Platforms.
//

#if defined(__wasm__)
#define WASM
#endif

//
// NOTE: Macros.
//

#define Kb(X) ((X) * 1024)
#define Mb(X) ((Kb(X)) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))
#define StaticAssert(Cond) typedef char static_assertion_at_line_##__LINE__[(Cond) ? 1 : -1]

#if defined(MSVC)

#define Assert(Expression) \
    if (!(Expression))     \
    {                      \
        __debugbreak();    \
    }

#elif defined(GCC) || defined(CLANG)

#define Assert(Expression) \
    if (!(Expression))     \
    {                      \
        __builtin_trap();  \
    }

#else

#define Assert(Expression)      \
    if (!(Expression))          \
    {                           \
        *(volatile int *)0 = 0; \
    }

#endif

#define IsPow2(X) (((X) != 0) && (((X) & ((X) - 1)) == 0))
//  `Align` must be a power of 2.
#define AlignUp(Val, Align) (((Val) + ((Align) - 1)) & ~((Align) - 1))

#endif
