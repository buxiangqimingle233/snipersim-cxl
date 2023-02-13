#ifndef __NUMA_BALANCER_H__
#define __NUMA_BALANCER_H__

#include "fixed_types.h"
#include "dablooms.h"
#include "random.h"
#include <iostream>

// class InterleaveBalancer;
class LocalFirstBalancer;

class NumaAddressBalancer {

public:
    enum NUMA_NODE { ANY, LOCAL, REMOTE };

    virtual NUMA_NODE locate_where(UInt64 va, int core_id) {
        return rand() % 16 > 8 ? NUMA_NODE::LOCAL : NUMA_NODE::REMOTE;
    };

    // copied from trace_thread.h:38
    static const UInt64 pa_core_shift = 48;
    static const UInt64 pa_core_size = 16;
    static const UInt64 pa_va_mask = ~(((UInt64(1) << pa_core_size) - 1) << pa_core_shift);
    static const UInt64 va_page_shift = 12;
    static const UInt64 va_page_mask = (UInt64(1) << va_page_shift) - 1;

    NumaAddressBalancer() { };
    virtual ~NumaAddressBalancer() { };

    static NumaAddressBalancer* createNumaBalancer(int type, UInt64 local_mem_capacity, UInt64 remote_mem_capacity);
};


class LocalFirstBalancer: public NumaAddressBalancer {

private:
    scaling_bloom_t* local_mem_bloom;
    const unsigned int CAPACITY = 100000;
    const float ERROR_RATE = .05;
    unsigned int local_page_limit;
    unsigned int n_local_allocated_page;
    // const unsigned int FILTER_CNT_LIMIT = 100000;

public:
    virtual NUMA_NODE locate_where(UInt64 va, int core_id) override;

    LocalFirstBalancer(int type, UInt64 local_mem_capacity, UInt64 remote_mem_capacity);
    ~LocalFirstBalancer();

};

#endif