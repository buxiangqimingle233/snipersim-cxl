#pragma once

#include <unordered_map>
#include <vector>
#include <queue>

#include "fixed_types.h"
#include "subsecond_time.h"

#include "dablooms.h"
#include "cuckoo_hashmap.hpp"
#include "trace_logger.hpp"


namespace CxTnLMemShim {

typedef struct {
    std::vector<UInt64> cacheline_to_commit;    // One for each dirty cacheline
    core_id_t requester;
} WQE;

typedef struct {
    bool finish;
    core_id_t receiver;
    WQE wqe;        // OPT: Using shared ptr
} CQE;

class EPAgent {

private:
    const int cacheline_bias = 6;
    core_id_t cid;

    // Fast Path
    counting_bloom_t* view_bf;
    counting_bloom_t* cache_bf;
    CuckooHashMap<UInt64, UInt64>* view_address_table;

    // Commit Path
    std::vector<EPAgent*> peer_agents;
    std::queue<WQE> work_queue;

    // debugging
    Logger* trace_logger;

private:
    bool BFCheck(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);
    void BFInsert(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);
    void BFRemove(UInt64 key, std::vector<bool>& bf, SubsecondTime& latency);

    void BFResizing(std::vector<bool>& bf, std::vector<bool>& new_bf);  // TODO

public: 
    // Stats
    UInt64 m_cnt_raw, m_cnt_hash_conflict;
    UInt64 m_cnt_vbf_hit, m_cnt_vbf_false_hit;
    UInt64 m_cnt_filter1_hit;
    UInt64 m_handled_write, m_handled_read;

public: 
    // EPAgent() { };
    EPAgent();
    ~EPAgent();

    EPAgent(const char* view_bf_file, const char* cache_bf_file, UInt64 view_bf_size, UInt64 cache_bf_size, UInt64 view_bf_nfunc, UInt64 cache_bf_nfunc, UInt64 view_address_table_size, core_id_t cid);    

    // Fast Path
    SubsecondTime Translate(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address);

    SubsecondTime Record(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address);

    // Slow Path
    SubsecondTime AppendWorkQueueElement(WQE wqe);

    SubsecondTime WriteCommit();

    void appendPeerAgent(EPAgent* peer_agent) {
        peer_agents.push_back(peer_agent);
    }

    void setCoreID(core_id_t core_id) {
        cid = core_id;
    }
};

}