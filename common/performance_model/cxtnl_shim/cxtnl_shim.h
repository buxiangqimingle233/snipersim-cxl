#pragma once
#include "fixed_types.h"
#include "sim_api.h"

#define CXL_MASK (CXL_TRACK_READ | CXL_TRACK_WRITE | WITH_CXL_MEM | WITH_CXL_BNISP)

#define BELONGS_TO_TYPE3(region) (((region) & WITH_CXL_MEM) && !((region) & WITH_CXL_BNISP))
#define BELONGS_TO_TYPE2(region)  (((region) & WITH_CXL_MEM) && ((region) & WITH_CXL_BNISP))
#define BELONGS_TO_CXL(region) ((region) & WITH_CXL_MEM)
#define IS_TRACKED_WRITE(region) ((region) & CXL_TRACK_WRITE)
#define IS_TRACKED_READ(region) ((region) & CXL_TRACK_READ)
