#pragma once
#include "fixed_types.h"

// Indicate the CXL memory regions
typedef uint32_t MEMORY_REGION;
#define DEFAULT (0)
#define WITH_CXL_BNISP (1 << 2)
#define WITH_CXL_MEM (1 << 1)
#define PREFIX (1 << 30)