#ifndef __NUMA_BALANCER_H__
#define __NUMA_BALANCER_H__

#include "fixed_types.h"

class NumaAddressBalancer {

public:
    virtual UInt64 va2pa(UInt64 va, int core_id) {
        return va;
    };

    NumaAddressBalancer() { };
    ~NumaAddressBalancer() { };

    static NumaAddressBalancer* getNumaBalancer(int type) {
        return new NumaAddressBalancer();
    };
};

#endif
