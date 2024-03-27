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
      CxTnLMemShim::EPAgent* m_ep_agent;

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
         NUM_ACCESS_TYPES
      } access_t;

      DramCntlrInterface(MemoryManagerBase* memory_manager, ShmemPerfModel* shmem_perf_model, UInt32 cache_block_size);

      virtual ~DramCntlrInterface();

      virtual boost::tuple<SubsecondTime, HitWhere::where_t> getDataFromDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now, ShmemPerf *perf) = 0;
      virtual boost::tuple<SubsecondTime, HitWhere::where_t> putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now) = 0;

      void handleMsgFromTagDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
      CxTnLMemShim::EPAgent* getEPAgent() { return m_ep_agent; };
};

// #endif // __DRAM_CNTLR_INTERFACE_H
