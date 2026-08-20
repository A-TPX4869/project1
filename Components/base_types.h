#ifndef __BASE_TYPES_H__
#define __BASE_TYPES_H__


#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "py32f0xx_hal.h"


typedef unsigned char           word8;
typedef unsigned short          word16;
typedef unsigned long           word32;

typedef signed char             int8_t;
typedef signed short int        int16_t;
typedef int                     int32_t;
typedef long long               int64_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long      uint64_t;
typedef uint8_t                 boolean_t;

typedef int8_t                  s8;
typedef int16_t                 s16;
typedef int32_t                 s32;
typedef int64_t                 s64;
typedef uint8_t                 u8;
typedef uint16_t                u16;
typedef uint32_t                u32;
typedef uint64_t                u64;

#define ATTRIBUTE_RAM_CODE      static inline
typedef boolean_t               boolean;

#define true                    ((boolean) 1u)
#define false                   ((boolean) 0u)

#endif /* __BASE_TYPES_H__ */
