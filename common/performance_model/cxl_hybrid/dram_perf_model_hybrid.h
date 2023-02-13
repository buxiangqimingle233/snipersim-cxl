#ifndef __DRAM_PERF_MODEL_HYBRID_H__
#define __DRAM_PERF_MODEL_HYBRID_H__

#include "dram_perf_model.h"
#include "queue_model.h"
#include "fixed_types.h"
#include "subsecond_time.h"
#include "dram_cntlr_interface.h"
#include "numa_balancer.h"

class DramPerfModelHybrid : public DramPerfModel
{
    private:
        core_id_t core_id;
        QueueModel* m_queue_model;
        SubsecondTime m_dram_access_cost_local, m_dram_access_cost_remote;
        ComponentBandwidth m_dram_bandwidth;

        SubsecondTime m_total_queueing_delay;
        SubsecondTime m_total_access_latency;

        NumaAddressBalancer* numa_balancer;

    public:
        DramPerfModelHybrid(core_id_t core_id, UInt32 cache_block_size);

        ~DramPerfModelHybrid();

        SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester, \
            IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf);
};


#endif
