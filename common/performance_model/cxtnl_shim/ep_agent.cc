#include <stdlib.h>
#include <stdio.h>

#include "ep_agent.h"
#include "murmur.h"
#include "dablooms.hpp"
#include <cmath>

#ifdef UNIT_TEST

// We overlap the definition of DramCntlrInterface to avoid the dependency on the simulator.h and dram_cntlr_interface.h
#define DramCntlrInterface 
typedef enum
{
    READ = 0,
    WRITE,
    NUM_ACCESS_TYPES
} access_t;

#else

#include "dram_cntlr_interface.h"
#include "simulator.h"

#endif

namespace CxTnLMemShim {

// EPAgent::EPAgent(const char* view_bf_file, const char* cache_bf_file, UInt64 view_bf_size, \
//     UInt64 cache_bf_size, UInt64 view_bf_nfunc, UInt64 cache_bf_nfunc, UInt64 view_address_table_size, core_id_t cid)\
//     : m_cnt_raw(0), m_cnt_hash_conflict(0), m_cnt_vbf_hit(0), m_cnt_vbf_false_hit(0), m_cnt_filter1_hit(0), cid(cid), trace_logger(NULL)
// {
//     view_bf = new_counting_bloom_nfunc(view_bf_size, view_bf_nfunc, view_bf_file);
//     cache_bf = new_counting_bloom_nfunc(cache_bf_size, cache_bf_nfunc, cache_bf_file);
//     view_address_table = new CuckooHashMap<UInt64, UInt64>(view_address_table_size, 32);
// }

EPAgent::EPAgent(): m_cnt_raw(0), m_cnt_hash_conflict(0),
    m_cnt_vbf_false_hit(0), m_cnt_filter1_hit(0), m_handled_write(0), m_handled_read(0),
    m_handled_flush(0), m_handled_remote_inv(0), cid(-1), 
    peer_agents(std::vector<EPAgent*>()), work_queue(std::queue<WQE>()),
    pending_remote_inv_req(std::queue<WQE>()),
    m_viewtable_delete(0), m_viewtable_insert(0),
    trace_logger(NULL), counting_bloom_query_latency(0), cuckoo_hashmap_single_hop_latency(0),
    cxl_cache_roundtrip(0), cxl_mem_roundtrip(0)
{

    UInt32 view_bf_size, view_bf_nfunc;
    UInt32 cache_bf_size, cache_bf_nfunc;
    UInt32 view_address_table_size, view_address_table_nway;

// #ifdef UNIT_TEST
//     String view_bf_file = "view_bf_file.bin";
//     String cache_bf_file = "cache_bf_file.bin";
// #else
//     String view_bf_file = Sim()->getConfig()->formatOutputFileName("view_bf_file.bin");
//     String cache_bf_file = Sim()->getConfig()->formatOutputFileName("cache_bf_file.bin");
// #endif

//     FILE *fp;
//     if ((fp = fopen(view_bf_file.c_str(), "r"))) {
//         fclose(fp);
//         remove(view_bf_file.c_str());
//     }
//     if ((fp = fopen(cache_bf_file.c_str(), "r"))) {
//         fclose(fp);
//         remove(cache_bf_file.c_str());
//     }

#ifdef UNIT_TEST
    view_bf_size = 1 << 14;
    view_bf_nfunc = 4;
    cache_bf_size = 1 << 14;
    cache_bf_nfunc = 2;
    view_address_table_size = 1 << 20;
    view_address_table_nway = 2;
    cxl_cache_roundtrip = 785;
    cxl_mem_roundtrip = 300;
#else
    view_bf_size = Sim()->getCfg()->getInt("ctxnl/view_bf_size");
    view_bf_nfunc = Sim()->getCfg()->getInt("ctxnl/view_bf_nfunc");
    cache_bf_size = Sim()->getCfg()->getInt("ctxnl/cache_bf_size");
    cache_bf_nfunc = Sim()->getCfg()->getInt("ctxnl/cache_bf_nfunc");
    view_address_table_size = Sim()->getCfg()->getInt("ctxnl/view_address_table_size");
    view_address_table_nway = Sim()->getCfg()->getInt("ctxnl/view_address_table_nway");
    cxl_cache_roundtrip = Sim()->getCfg()->getInt("perf_model/cxl/cxl_cache_roundtrip");
    cxl_mem_roundtrip = Sim()->getCfg()->getInt("perf_model/cxl/cxl_mem_roundtrip");
    counting_bloom_query_latency = 10;
    cuckoo_hashmap_single_hop_latency = 50;
#endif

    view_bf = new BloomFilter(view_bf_size, view_bf_nfunc);
    view_back_bf = new BloomFilter(cache_bf_size, cache_bf_nfunc);
    
    // TODO: nway should be passed into it
    view_address_table = new CuckooHashMap<UInt64, UInt64>(view_address_table_size, 32);

    work_queue_latch = new pthread_mutex_t;
    pthread_mutex_init(work_queue_latch, NULL);
    inv_que_latch = new pthread_mutex_t;
    pthread_mutex_init(inv_que_latch, NULL);
    vat_latch = new pthread_mutex_t;
    pthread_mutex_init(vat_latch, NULL);

#ifdef TRACK_BUS_THROUGHPUT
    knob_sent_cnt = std::vector<UInt32>(1e4, 0);
    knob_record_interval = 1e4;   // 1ms
    knob_start_record_time = SubsecondTime::MaxTime();
#endif
}


SubsecondTime EPAgent::InvalidLocalCopy(const WQE& wqe) {
    IntPtr base_addr = wqe.addr & cacheline_base_mask;
    SubsecondTime latency = SubsecondTime::Zero();

    for (IntPtr addr = base_addr; addr < wqe.size + wqe.addr; addr += cacheline_size) {
        latency += SubsecondTime::NSfromFloat(counting_bloom_query_latency);
        // Delete the address from host caches
        if (view_back_bf->counting_bloom_check((const char*)&addr, sizeof(addr))) {
            view_back_bf->counting_bloom_remove((const char*)&addr, sizeof(addr));
            latency += SubsecondTime::NSfromFloat(std::max(cxl_mem_roundtrip, counting_bloom_query_latency));   // Bnisp from dev to host
        }
        // Delete the address from view  
        if (view_bf->counting_bloom_check((const char*)&addr, sizeof(addr))) {
            view_bf->counting_bloom_remove((const char*)&addr, sizeof(addr));
            view_address_table->remove(addr);
            latency += SubsecondTime::NSfromFloat(std::max(counting_bloom_query_latency, cuckoo_hashmap_single_hop_latency));
        }
    }
    
    return latency;
}


void EPAgent::AppendPendingRemoteInvReq(WQE wqe) {
    pthread_mutex_lock(inv_que_latch);
    pending_remote_inv_req.push(wqe);
    inv_que_empty = false;
    pthread_mutex_unlock(inv_que_latch);
}


SubsecondTime EPAgent::dequeueWorkQueue() {
    pthread_mutex_lock(work_queue_latch);
    SubsecondTime total_latency = SubsecondTime::Zero();    // Well, this could be optimized
    while (work_queue.size()) {
        WQE wqe = work_queue.front();
        work_queue.pop();
        // Calculate involved cachelines
        // Invalid all remote copies asynchronously
        for (EPAgent* peer: peer_agents) {
            peer->AppendPendingRemoteInvReq(wqe);
        }
        // Also invalid all local copies
        total_latency += InvalidLocalCopy(wqe);
        m_handled_flush++;
    }
    pthread_mutex_unlock(work_queue_latch);
    return total_latency;
}


SubsecondTime EPAgent::AppendWorkQueueElement(WQE wqe) {
// Potential data racing here
    pthread_mutex_lock(work_queue_latch);  // Keep the WQE
    work_queue.push(wqe);
#ifdef RECORD_CXL_TRACE
    IntPtr dummy;
    trace_logger->log({2, wqe.addr, wqe.size});
    // Record(wqe.addr, wqe.requester, 2, dummy);     // Tracing a special event to indicate the end of a flush
#endif
    pthread_mutex_unlock(work_queue_latch);
    return SubsecondTime::Zero();
}


SubsecondTime EPAgent::Translate(IntPtr physical_address, core_id_t requester, int access_type_, IntPtr& dram_address) {
    pthread_mutex_lock(vat_latch);

    DramCntlrInterface::access_t access_type = (DramCntlrInterface::access_t)access_type_;
    IntPtr cacheline_address = physical_address & cacheline_base_mask;
    IntPtr page_address = physical_address & page_base_mask;
    const char* bf_query_key = (const char*)(&cacheline_address);

    SubsecondTime latency = SubsecondTime::Zero();
    if (access_type == DramCntlrInterface::WRITE) {
        // View Bloom Filter Insert
        view_bf->counting_bloom_add(bf_query_key, sizeof(cacheline_address));
        view_back_bf->counting_bloom_remove((const char*)(&page_address), sizeof(cacheline_address));

        // Cuckoo Hash Map Insert
        int n_tries = view_address_table->insert(cacheline_address, cacheline_address);
        
        latency += SubsecondTime::NSfromFloat(counting_bloom_query_latency);
        latency += SubsecondTime::NSfromFloat(n_tries * cuckoo_hashmap_single_hop_latency);
        
        m_cnt_hash_conflict += n_tries - 1;
        m_handled_write++;
        m_viewtable_insert++;
    } else if (access_type == DramCntlrInterface::READ) {
        view_back_bf->counting_bloom_add((const char*)(&page_address), sizeof(cacheline_address));

        latency += SubsecondTime::NSfromFloat(counting_bloom_query_latency);

        if (view_bf->counting_bloom_check(bf_query_key, sizeof(cacheline_address))) {    // Filter 2: View Bloom Filter Check
            if (view_address_table->find(cacheline_address) != NULL) {   // View Address Table Check
                view_bf->counting_bloom_remove(bf_query_key, sizeof(cacheline_address));
                view_address_table->remove(cacheline_address);

                latency += SubsecondTime::NSfromFloat(cuckoo_hashmap_single_hop_latency);
                latency += SubsecondTime::NSfromFloat(counting_bloom_query_latency);
                
                m_cnt_raw++;
                m_viewtable_delete++;
            } else {
                m_cnt_vbf_false_hit++;
            }
        }
        m_handled_read++;
    }

    // Clean up the remote invalidation queue at read time to capture the latency of remote invalidation
    if (access_type == DramCntlrInterface::READ) {
        if (!inv_que_empty) {
            pthread_mutex_lock(inv_que_latch);
            while (pending_remote_inv_req.size()) {
                WQE wqe = pending_remote_inv_req.front();
                pending_remote_inv_req.pop();
                latency += InvalidLocalCopy(wqe);
                m_handled_remote_inv++;
            }
            inv_que_empty = true;
            pthread_mutex_unlock(inv_que_latch);
        }
    }

    pthread_mutex_unlock(vat_latch);
    return latency;
}


void EPAgent::setCoreID(core_id_t core_id) {
    cid = core_id;
    if (trace_logger == NULL) {
#ifdef UNIT_TEST
        String name = String("cxl3_dram_trace_c-1") + String(".log");
#endif

#ifdef RECORD_CXL_TRACE
        assert(cid != -1);
        std::string str = std::to_string(cid);
        String name = Sim()->getConfig()->formatOutputFileName("cxl3_dram_trace_c" + String(str.begin(), str.end()) + ".log");
        trace_logger = new Logger(name.c_str(), 1024);
#endif
    }
}


SubsecondTime EPAgent::Record(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address) {
    pthread_mutex_lock(vat_latch);
    trace_logger->log({access_type, physical_address, 1 << 6});   // 
    pthread_mutex_unlock(vat_latch);
    return SubsecondTime::Zero();
}


EPAgent::~EPAgent() {
    std::cout << std::endl;
    // Cuckoo View Table
    std::cout << "Cuckoo View Table Status: " << std::endl;
    std::cout << "hash conflicts: " << m_cnt_hash_conflict << " inserts: " << m_handled_write << " deletes: " << m_viewtable_delete << std::endl;
    view_address_table->print_states();
    std::cout << std::endl;

    // Bloom Filter
    std::cout << "View Bloom Filter Status: " << std::endl;
    view_bf->print_states();
    std::cout << "False Hit: " << m_cnt_vbf_false_hit << std::endl;
    std::cout << std::endl;
    
    std::cout << "Cache Bloom Filter Status: " << std::endl;
    view_back_bf->print_states();

    // Benchmark 
    std::cout << "Benchmark Status: " << std::endl;
    std::cout << "Read: " << m_handled_read << " Write: " << m_handled_write << " Flush: " << m_handled_flush << " Remove Inv: " << m_handled_remote_inv << " Raw: " << m_cnt_raw << std::endl;
    std::cout << std::endl;

#ifdef TRACK_BUS_THROUGHPUT
    std::cout << "EP-Bus-Record: " << std::endl;
    for (UInt32 i = 0; i < knob_sent_cnt.size(); i++) {
        std::cout << knob_sent_cnt[i] << ",";
    }
    std::cout << std::endl;
#endif

    if (trace_logger != NULL) {
        delete trace_logger;
    }
}

}


