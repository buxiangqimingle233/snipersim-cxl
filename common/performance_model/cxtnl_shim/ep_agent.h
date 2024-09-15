#pragma once

#include <unordered_map>
#include <vector>
#include <queue>

#include "fixed_types.h"
#include "subsecond_time.h"

#include "dablooms.hpp"
#include "cuckoo_hashmap.hpp"
#include "trace_logger.hpp"


namespace CxTnLMemShim {

typedef struct {
    IntPtr addr;    // One for each dirty cacheline
    size_t size;
    core_id_t requester;
} WQE;

typedef struct {
    bool finish;
    core_id_t receiver;
    WQE wqe;
} CQE;


class EPAgent {

private:
    // const int cacheline_bias = 6;
    const long long unsigned cacheline_base_mask = ~0x3f;
    const long long unsigned cacheline_size = 0x3f + 1;
    const long long unsigned page_base_mask = ~0xfff;
    core_id_t cid;

    // Fast Path
    BloomFilter* view_bf;
    BloomFilter* view_back_bf;
    CuckooHashMap<UInt64, UInt64>* view_address_table;

    // Commit Path
    std::vector<EPAgent*> peer_agents;
    std::queue<WQE> work_queue;

    pthread_mutex_t* inv_que_latch;
    std::queue<WQE> pending_remote_inv_req;
    volatile bool inv_que_empty;

    // debugging
    Logger* trace_logger;

    // Broadcast bus throughput requirments
    std::vector<UInt32> knob_sent_cnt;
    UInt32 knob_record_interval;
    SubsecondTime knob_start_record_time;

    pthread_mutex_t* work_queue_latch;
    pthread_mutex_t* vat_latch;

private:
    bool BFCheck(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);
    void BFInsert(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);
    void BFRemove(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);

    void BFResizing(std::vector<bool>& bf, std::vector<bool>& new_bf);  // TODO

    SubsecondTime InvalidLocalCopy(const WQE& wqe);

public: 
    // Stats
    UInt64 m_cnt_raw, m_cnt_hash_conflict, m_viewtable_insert, m_viewtable_delete;
    UInt64 m_cnt_vbf_false_hit;
    UInt64 m_cnt_filter1_hit;
    UInt64 m_handled_write, m_handled_read, m_handled_flush, m_handled_remote_inv;

    unsigned int cxl_cache_roundtrip, cxl_mem_roundtrip;
    unsigned int counting_bloom_query_latency, cuckoo_hashmap_single_hop_latency;

public:
    // EPAgent() { };
    EPAgent();
    ~EPAgent();

    // EPAgent(const char* view_bf_file, const char* cache_bf_file, UInt64 view_bf_size, UInt64 cache_bf_size, UInt64 view_bf_nfunc, UInt64 cache_bf_nfunc, UInt64 view_address_table_size, core_id_t cid);    

    // Fast Path
    SubsecondTime Translate(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address);
    
    SubsecondTime Record(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address);

    void RemoveViewBackBF(IntPtr physical_address) {
        IntPtr cacheline_address = physical_address & cacheline_base_mask;
        IntPtr page_address = physical_address & page_base_mask;
        view_back_bf->counting_bloom_remove((const char*)(&page_address), sizeof(cacheline_address));
    }

    // Slow Path
    SubsecondTime AppendWorkQueueElement(WQE wqe);
    SubsecondTime dequeueWorkQueue();
    void AppendPendingRemoteInvReq(WQE wqe);

    void appendPeerAgent(EPAgent* peer_agent) {
        peer_agents.push_back(peer_agent);
    }

    void setCoreID(core_id_t core_id);
    
    bool flag = false;
    void recordBusTraffic(SubsecondTime now, int size) {
#ifdef TRACK_BUS_THROUGHPUT
        if (knob_start_record_time == SubsecondTime::MaxTime()) {
            knob_start_record_time = now;
        }
        UInt64 idx = (now - knob_start_record_time).getNS() / knob_record_interval;
        // printf("ns: %lu, idx: %lu, size: %d\n", now.getNS(), idx, size);
        // if (idx == knob_sent_cnt.size() / 2 && !flag) {
        //     printf("EP-Bus-Record:\n");
        //     for (UInt32 i = 0; i < knob_sent_cnt.size(); i++) {
        //         printf("%u ", knob_sent_cnt[i]);
        //     }
        //     printf("\n");
        //     flag = true;
        // }
        if (idx < knob_sent_cnt.size()) {
            knob_sent_cnt[idx] += size;
        }
#endif
    }
};

}