#pragma once
#include "fixed_types.h"
#include "sim_api.h"

// Indicate the CXL memory regions
typedef uint32_t MEMORY_REGION;
#define DEFAULT LOCAL_SNIPER_MODE
#define WITH_CXL_BNISP (1 << 2)
#define WITH_CXL_MEM (1 << 1)

#define CXL_TRACK_READ (1 << 11)
#define CXL_TRACK_WRITE (1 << 10)
#define CXL_MASK (CXL_TRACK_READ | CXL_TRACK_WRITE | WITH_CXL_MEM | WITH_CXL_BNISP)

#define BELONGS_TO_TYPE3(region) (((region) & WITH_CXL_MEM) && !((region) & WITH_CXL_BNISP))
#define BELONGS_TO_TYPE2(region)  (((region) & WITH_CXL_MEM) && ((region) & WITH_CXL_BNISP))
#define BELONGS_TO_CXL(region) ((region) & WITH_CXL_MEM)
#define IS_TRACKED_WRITE(region) ((region) & CXL_TRACK_WRITE)
#define IS_TRACKED_READ(region) ((region) & CXL_TRACK_READ)