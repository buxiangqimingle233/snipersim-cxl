#include <stdlib.h>
#include <stdio.h>

#include "ep_agent.h"
#include "murmur.h"

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

EPAgent::EPAgent(const char* view_bf_file, const char* cache_bf_file, UInt64 view_bf_size, \
    UInt64 cache_bf_size, UInt64 view_bf_nfunc, UInt64 cache_bf_nfunc, UInt64 view_address_table_size, core_id_t cid)\
    : m_cnt_raw(0), m_cnt_hash_conflict(0), m_cnt_vbf_hit(0), m_cnt_vbf_false_hit(0), m_cnt_filter1_hit(0), cid(cid), trace_logger(NULL)
{
    view_bf = new_counting_bloom_nfunc(view_bf_size, view_bf_nfunc, view_bf_file);
    cache_bf = new_counting_bloom_nfunc(cache_bf_size, cache_bf_nfunc, cache_bf_file);
    view_address_table = new CuckooHashMap<UInt64, UInt64>(view_address_table_size, 32);
}

EPAgent::EPAgent(): m_cnt_raw(0), m_cnt_hash_conflict(0), m_cnt_vbf_hit(0), 
    m_cnt_vbf_false_hit(0), m_cnt_filter1_hit(0), m_handled_write(0), m_handled_read(0),
    cid(-1), trace_logger(NULL)
{
    m_cnt_raw = m_cnt_hash_conflict = 0;

    UInt32 view_bf_size, view_bf_nfunc;
    UInt32 cache_bf_size, cache_bf_nfunc;
    UInt32 view_address_table_size, view_address_table_nway;

#ifdef UNIT_TEST
    String view_bf_file = "view_bf_file.bin";
    String cache_bf_file = "cache_bf_file.bin";
#else
    String view_bf_file = Sim()->getConfig()->formatOutputFileName("view_bf_file.bin");
    String cache_bf_file = Sim()->getConfig()->formatOutputFileName("cache_bf_file.bin");
#endif

    FILE *fp;
    if ((fp = fopen(view_bf_file.c_str(), "r"))) {
        fclose(fp);
        remove(view_bf_file.c_str());
    }
    if ((fp = fopen(cache_bf_file.c_str(), "r"))) {
        fclose(fp);
        remove(cache_bf_file.c_str());
    }

#ifdef UNIT_TEST
    view_bf_size = 1 << 20;
    view_bf_nfunc = 4;
    cache_bf_size = 1024;
    cache_bf_nfunc = 2;
    view_address_table_size = 1 << 20;
    view_address_table_nway = 2;
#else
    view_bf_size = Sim()->getCfg()->getInt("ctxnl/view_bf_size");
    view_bf_nfunc = Sim()->getCfg()->getInt("ctxnl/view_bf_nfunc");
    cache_bf_size = Sim()->getCfg()->getInt("ctxnl/cache_bf_size");
    cache_bf_nfunc = Sim()->getCfg()->getInt("ctxnl/cache_bf_nfunc");
    view_address_table_size = Sim()->getCfg()->getInt("ctxnl/view_address_table_size");
    view_address_table_nway = Sim()->getCfg()->getInt("ctxnl/view_address_table_nway");
#endif

    view_bf = new_counting_bloom(view_bf_size, 0.05, view_bf_file.c_str());
    // view_bf = new_counting_bloom_nfunc(view_bf_size, view_bf_nfunc, view_bf_file.c_str());
    cache_bf = new_counting_bloom_nfunc(cache_bf_size, cache_bf_nfunc, cache_bf_file.c_str());
    // TODO: nway should be passed into it
    view_address_table = new CuckooHashMap<UInt64, UInt64>(view_address_table_size, 32);
}


SubsecondTime EPAgent::AppendWorkQueueElement(WQE wqe) {
    work_queue.push(wqe);
    return SubsecondTime::Zero();
}


SubsecondTime EPAgent::Translate(IntPtr physical_address, core_id_t requester, int access_type_, IntPtr& dram_address) {
    DramCntlrInterface::access_t access_type = (DramCntlrInterface::access_t)access_type_;
    IntPtr cacheline_address = physical_address >> cacheline_bias;
    const char* bf_query_key = std::to_string(cacheline_address).c_str();

 
    SubsecondTime latency = SubsecondTime::Zero();
    if (access_type == DramCntlrInterface::WRITE) {
        // View Bloom Filter Insert
        counting_bloom_add(view_bf, bf_query_key, sizeof(bf_query_key));
        // Cuckoo Hash Map Insert
        int n_tries = view_address_table->insert(cacheline_address, cacheline_address);
        m_cnt_hash_conflict += n_tries - 1;
        m_handled_write++;
    } else if (access_type == DramCntlrInterface::READ) {
        if (view_bf->header->count > 0) {       // Filter 1
            if (counting_bloom_check(view_bf, bf_query_key, sizeof(bf_query_key))) {    // Filter 2: View Bloom Filter Check
                m_cnt_vbf_hit++;
                if (view_address_table->find(cacheline_address) != NULL) {   // View Address Table Check
                    m_cnt_raw++;
                    counting_bloom_remove(view_bf, bf_query_key, sizeof(bf_query_key));
                    view_address_table->remove(cacheline_address);
                } else {
                    m_cnt_vbf_false_hit++;
                }
            }
        } else {
            m_cnt_filter1_hit++;
        }
        m_handled_read++;
    }

    return latency;
}


SubsecondTime EPAgent::Record(IntPtr physical_address, core_id_t requester, int access_type, IntPtr& dram_address) {
    if (trace_logger == NULL) {
#ifdef UNIT_TEST
        String name = String("cxl3_dram_trace_c-1") + String(".log");
#else
        assert(cid != -1);
        std::string str = std::to_string(cid);
        String name = Sim()->getConfig()->formatOutputFileName("cxl3_dram_trace_c" + String(str.begin(), str.end()) + ".log");
#endif
        trace_logger = new Logger(name.c_str(), 1024);
    }
    trace_logger->log(physical_address, access_type);
    return SubsecondTime::Zero();
}


EPAgent::~EPAgent() {
    std::cout << "Raw: " << m_cnt_raw << std::endl;
    std::cout << "m_cnt_hash_conflict: " << m_cnt_hash_conflict << std::endl;
    std::cout << "m_cnt_vbf_hit: " << m_cnt_vbf_hit << std::endl;
    std::cout << "m_cnt_vbf_false_hit: " << m_cnt_vbf_false_hit << std::endl;
    std::cout << "m_handled_write: " << m_handled_write << std::endl;  
    std::cout << "m_handled_read: " << m_handled_read << std::endl;
    view_address_table->print_fill_ratio();
    print_counting_bloom_fill_rate(view_bf);

    // Check 
    if (trace_logger != NULL) {
        delete trace_logger;
    }
}

}


