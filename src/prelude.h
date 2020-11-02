#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

// Due to C/C++ being silly and allowing one word to mean three things
// we've created these aliases to allow for more expressive code
// and more importantly searchable. By using these macros we can
// now quickly search for truly 'global' variables that we need
// to be careful with for thread safety.
#define global static
#define internal static
#define local_persist static
