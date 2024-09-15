#pragma once
// #ifndef __DRAM_CNTLR_INTERFACE_H
// #define __DRAM_CNTLR_INTERFACE_H

#include "fixed_types.h"
#include "subsecond_time.h"
#include "hit_where.h"
#include "simulator.h"
#include "config.hpp"
#include "shmem_msg.h"
#include "memory_manager.h"
#include "ep_agent.h"
#include <vector>
#include "murmur.h"

#include "boost/tuple/tuple.hpp"

class MemoryManagerBase;
class ShmemPerfModel;
class ShmemPerf;

class DramCntlrInterface
{
   protected:
      MemoryManagerBase* m_memory_manager;
      ShmemPerfModel* m_shmem_perf_model;
      UInt32 m_cache_block_size;
      UInt32 m_core_count;
      std::vector<CxTnLMemShim::EPAgent*> m_ep_agents;

      UInt32 getCacheBlockSize() { return m_cache_block_size; }
      MemoryManagerBase* getMemoryManager() { return m_memory_manager; }
      ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

   public:
      unsigned int cxl_mem_roundtrip;
      int hit_mem_region;    // This variable indicates whether the current memory op locates on CXL-devices
                                    // This is hacking variable to keep the function signiture of getDramPerfModel
      typedef enum
      {
         READ = 0,
         WRITE,
         WRITE_CLEAN,
         CXTNL_RESERVE,
         NUM_ACCESS_TYPES
      } access_t;

      DramCntlrInterface(MemoryManagerBase* memory_manager, ShmemPerfModel* shmem_perf_model, UInt32 cache_block_size);

      virtual ~DramCntlrInterface();

      virtual boost::tuple<SubsecondTime, HitWhere::where_t> getDataFromDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now, ShmemPerf *perf) = 0;
      virtual boost::tuple<SubsecondTime, HitWhere::where_t> putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now) = 0;

      void handleMsgFromTagDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);

      std::vector<CxTnLMemShim::EPAgent*>* getAllEPAgents() { return &m_ep_agents; };
      CxTnLMemShim::EPAgent* getEPAgent(core_id_t core_id) { 
         // std::cout << "CORE ID: " << core_id << std::endl;
         // uint32_t l[4];
         // MurmurHash3_x64_128(&core_id, sizeof(core_id), 114, l);
         // return m_ep_agents[l[2] % m_ep_agents.size()];
         int cores_per_agent = m_core_count / m_ep_agents.size();
         return m_ep_agents[(core_id / cores_per_agent) % m_ep_agents.size()];
      }
      void setEPAgents(std::vector<CxTnLMemShim::EPAgent*> ep_agent) { 
         assert(m_ep_agents.size() == ep_agent.size());
         m_ep_agents = ep_agent;
      }
};

// #endif // __DRAM_CNTLR_INTERFACE_H
