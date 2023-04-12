#include <vector>

#include "dram_perf_model_hybrid.h"
#include "simulator.h"
#include "config.hpp"
#include "stats.h"
#include "shmem_perf.h"

DramPerfModelHybrid::DramPerfModelHybrid(core_id_t core_id, 
    UInt32 cache_block_size):
    DramPerfModel(core_id, cache_block_size),
    core_id(core_id),
    m_queue_model(NULL),
    m_dram_bandwidth(8 * Sim()->getCfg()->getFloat("perf_model/dram/per_controller_bandwidth")),
    m_total_queueing_delay(SubsecondTime::Zero()),
    m_total_access_latency(SubsecondTime::Zero()),
    m_node(DramPerfModelHybrid::MEMORY_NODE::LOCAL)
    // numa_balancer(NULL)
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

    // int type = Sim()->getCfg()->getInt("traceinput/numa_balance_strategy");
    unsigned int local_mem_capacity = Sim()->getCfg()->getInt("perf_model/dram/local_capacity");
    unsigned int remote_mem_capacity = Sim()->getCfg()->getInt("perf_model/dram/remote_capacity");
    // numa_balancer = NumaAddressBalancer::createNumaBalancer(type, local_mem_capacity, remote_mem_capacity);     // Some magic number for primary testing (x)
}


DramPerfModelHybrid::~DramPerfModelHybrid() {
    if (m_queue_model) {
        delete m_queue_model;
        m_queue_model = NULL;
    } 
    // if (numa_balancer) {
    //     delete numa_balancer;
    //     numa_balancer = NULL;
    // }
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

    SubsecondTime processing_time = m_dram_bandwidth.getRoundedLatency(8 * pkt_size); // bytes to bits
    // Compute Queue Delay
    SubsecondTime queue_delay;
    if (m_queue_model)
    {
        queue_delay = m_queue_model->computeQueueDelay(pkt_time, processing_time, requester);
    }
    else
    {
        queue_delay = SubsecondTime::Zero();
    }

    SubsecondTime m_dram_access_cost;
    // NumaAddressBalancer::NUMA_NODE where = numa_balancer->locate_where(address, core_id);

    if (m_node == MEMORY_NODE::LOCAL) {
        m_dram_access_cost = m_dram_access_cost_local;
    } else {
        m_dram_access_cost = m_dram_access_cost_remote;
    } 
    SubsecondTime access_latency = processing_time + queue_delay + m_dram_access_cost;

    perf->updateTime(pkt_time);
    perf->updateTime(pkt_time + queue_delay, ShmemPerf::DRAM_QUEUE);
    perf->updateTime(pkt_time + queue_delay + processing_time, ShmemPerf::DRAM_BUS);
    perf->updateTime(pkt_time + queue_delay + processing_time + m_dram_access_cost, ShmemPerf::DRAM_DEVICE);

    m_num_accesses ++;
    m_total_access_latency += access_latency;
    m_total_queueing_delay += access_latency;

    return access_latency;
}