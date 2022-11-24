#ifndef _CONFIG_H
#define _CONFIG_H

//Configuration starts here
//----------------------------------------------
#define __USE_STDINT
#define __EAGER
//----------------------------------------------
//Configuration ends here

#ifdef __USE_STDINT
#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;
#else
typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;
typedef long long          s64;
typedef int                s32;
typedef short              s16;
typedef char               s8;
#endif

typedef u8 byte;
typedef u16 word;
typedef u32 dword;
typedef u64 qword;

typedef s8 sbyte;
typedef s16 sword;
typedef s32 sdword;
typedef s64 sqword;
#endif