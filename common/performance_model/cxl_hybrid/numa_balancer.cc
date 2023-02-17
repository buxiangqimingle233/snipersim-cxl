#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "config.hpp"
#include "numa_balancer.h"
#include "simulator.h"
#include "fixed_types.h"
#include "dablooms.h"


NumaAddressBalancer* NumaAddressBalancer::createNumaBalancer(int type, UInt64 local_mem_capacity, UInt64 remote_mem_capacity) {
    if (type == 0) {
        return new NumaAddressBalancer();
    } else if (type == 1) {
        return new InterleaveBalancer(type);
    } else if (type == 2) {
        return new LocalFirstBalancer(type, local_mem_capacity, remote_mem_capacity);
    } else {
        std::cerr << "[SNIPER] " << "Invalid configuration of numa_balancer=" << type << std::endl;
        throw std::bad_cast();
    }
    return NULL;
};


InterleaveBalancer::InterleaveBalancer(int type)
    : NumaAddressBalancer(), CAPACITY(10000), ERROR_RATE(.05), last_decision(NumaAddressBalancer::NUMA_NODE::LOCAL)
{
    const char* bloom_file = Sim()->getCfg()->getString("traceinput/bloom_file").c_str();
    FILE *fp;
    if ((fp = fopen(bloom_file, "r"))) {
        fclose(fp);
        remove(bloom_file);
    }

    if (!(local_mem_recorder = new_scaling_bloom(CAPACITY, ERROR_RATE, bloom_file))) {
        fprintf(stderr, "[SNIPER] ERROR: Could not create local bloom filter\n");
        std::abort();
    }
}

InterleaveBalancer::~InterleaveBalancer() {
    free_scaling_bloom(local_mem_recorder);
}

NumaAddressBalancer::NUMA_NODE InterleaveBalancer::locate_where(UInt64 va, int core_id) {
    UInt64 page_num = va >> va_page_shift;      // page number
    const char* query_key = std::to_string(page_num).c_str();
    if (scaling_bloom_check(local_mem_recorder, query_key, sizeof(query_key))) {
        return NumaAddressBalancer::NUMA_NODE::LOCAL;
    } else if (last_decision == NumaAddressBalancer::NUMA_NODE::REMOTE) {
        scaling_bloom_add(local_mem_recorder, query_key, sizeof(query_key), page_num);
        last_decision = NumaAddressBalancer::NUMA_NODE::LOCAL;
        return NumaAddressBalancer::NUMA_NODE::LOCAL;
    } else if (last_decision == NumaAddressBalancer::NUMA_NODE::LOCAL) {
        return NumaAddressBalancer::NUMA_NODE::REMOTE;
    } else {
        return NumaAddressBalancer::NUMA_NODE::LOCAL;
    }
}

LocalFirstBalancer::LocalFirstBalancer(int type, UInt64 local_mem_capacity, UInt64 remote_mem_capacity)
    : NumaAddressBalancer(), CAPACITY(10000), ERROR_RATE(.05), local_page_limit(0), n_local_allocated_page(0)
{
    const char* bloom_file = Sim()->getCfg()->getString("traceinput/bloom_file").c_str();
    FILE *fp;
    if ((fp = fopen(bloom_file, "r"))) {
        fclose(fp);
        remove(bloom_file);
    }
    local_page_limit = local_mem_capacity >> va_page_shift;
    if (!(local_mem_recorder = new_scaling_bloom(CAPACITY, ERROR_RATE, bloom_file))) {
        fprintf(stderr, "[SNIPER] ERROR: Could not create local bloom filter\n");
        std::abort();
    }
}


LocalFirstBalancer::~LocalFirstBalancer() {
    free_scaling_bloom(local_mem_recorder);
}


NumaAddressBalancer::NUMA_NODE LocalFirstBalancer::locate_where(UInt64 va, int core_id) {

    UInt64 page_num = va >> va_page_shift;      // page number
    const char* query_key = std::to_string(page_num).c_str();

    // If local memory is not full, allocate the page locally, otherwise locate the page to CXL-backed memory
    // TODO: no reclaim nor demote mechanisms now
    if (scaling_bloom_check(local_mem_recorder, query_key, sizeof(query_key))) {
        return NumaAddressBalancer::NUMA_NODE::LOCAL;
    } else if (n_local_allocated_page > local_page_limit) {
        return NumaAddressBalancer::NUMA_NODE::REMOTE;
    } else {
        scaling_bloom_add(local_mem_recorder, query_key, sizeof(query_key), page_num);
        n_local_allocated_page++;
        return NumaAddressBalancer::NUMA_NODE::LOCAL;
    }

}