#include "dram_perf_model_hybrid.h"
#include "simulator.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

DramPerfModelHybrid::DramPerfModelHybrid(core_id_t core_id, 
    UInt32 cache_block_size):
    DramPerfModel(core_id, cache_block_size),
    m_queue_model(NULL),
    m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")),
    m_total_queueing_delay(SubsecondTime::Zero()),
    m_total_access_latency(SubsecondTime::Zero())
{
    
    m_dram_access_cost_local = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/local_latency")));
    m_dram_access_cost_remote = SubsecondTime::FS() * static_cast<uint64_t>(TimeConverter<float>::NStoFS(Sim()->getCfg()->getFloat("perf_model/dram/remote_latency")));

   if (Sim()->getCfg()->getBool("perf_model/dram/queue_model/enabled"))
   {
      m_queue_model = QueueModel::create("dram-queue", core_id, Sim()->getCfg()->getString("perf_model/dram/queue_model/type"),
                                         m_dram_bandwidth.getRoundedLatency(8 * cache_block_size)); // bytes to bits
   }

    registerStatsMetric("dram", core_id, "total-access-latency", &m_total_access_latency);
    registerStatsMetric("dram", core_id, "total-queueing-delay", &m_total_queueing_delay);
}


DramPerfModelHybrid::~DramPerfModelHybrid() {
    if (m_queue_model) {
        delete m_queue_model;
        m_queue_model = NULL;
    }
}
   

SubsecondTime
DramPerfModelHybrid::getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
    // pkt_size is in 'Bytes'
   // m_dram_bandwidth is in 'Bits per clock cycle'
    if ((!m_enabled) ||
        (requester >= (core_id_t) Config::getSingleton()->getApplicationCores()))
    {
       return SubsecondTime::Zero();
    }

    SubsecondTime access_latency = m_dram_access_cost_local + m_dram_access_cost_remote;

    perf->updateTime(pkt_time);
    perf->updateTime(pkt_time + access_latency, ShmemPerf::DRAM_DEVICE);

    m_num_accesses ++;
    m_total_access_latency += access_latency;
    m_total_queueing_delay += access_latency;

    return access_latency;
}