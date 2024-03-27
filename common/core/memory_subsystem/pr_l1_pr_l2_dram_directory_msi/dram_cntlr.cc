#include "dram_cntlr.h"
// #include "../parametric_dram_directory_msi/memory_manager.h"
#include "core.h"
#include "log.h"
#include "subsecond_time.h"
#include "stats.h"
#include "fault_injection.h"
#include "shmem_perf.h"
#include "ep_agent.h"
#include "cxtnl_shim.h"

#if 0
   extern Lock iolock;
#  include "core_manager.h"
#  include "simulator.h"
#  define MYLOG(...) { ScopedLock l(iolock); fflush(stdout); printf("[%s] %d%cdr %-25s@%3u: ", itostr(getShmemPerfModel()->getElapsedTime()).c_str(), getMemoryManager()->getCore()->getId(), Sim()->getCoreManager()->amiUserThread() ? '^' : '_', __FUNCTION__, __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); }
#else
#  define MYLOG(...) {}
#endif

class TimeDistribution;

namespace PrL1PrL2DramDirectoryMSI
{

DramCntlr::DramCntlr(MemoryManagerBase* memory_manager,
      ShmemPerfModel* shmem_perf_model,
      UInt32 cache_block_size)
   : DramCntlrInterface(memory_manager, shmem_perf_model, cache_block_size)
   , m_reads(0)
   , m_writes(0)
   , m_cxl_mem_overhead(SubsecondTime::Zero())
{
   m_dram_perf_model = DramPerfModel::createDramPerfModel(
         memory_manager->getCore()->getId(),
         cache_block_size);

   m_fault_injector = Sim()->getFaultinjectionManager()
      ? Sim()->getFaultinjectionManager()->getFaultInjector(memory_manager->getCore()->getId(), MemComponent::DRAM)
      : NULL;

   m_dram_access_count = new AccessCountMap[DramCntlrInterface::NUM_ACCESS_TYPES];
   registerStatsMetric("dram", memory_manager->getCore()->getId(), "reads", &m_reads);
   registerStatsMetric("dram", memory_manager->getCore()->getId(), "writes", &m_writes);
   registerStatsMetric("dram", memory_manager->getCore()->getId(), "cxl-mem-overhead", &m_cxl_mem_overhead);
}

DramCntlr::~DramCntlr()
{
   printDramAccessCount();
   delete [] m_dram_access_count;

   delete m_dram_perf_model;
}

boost::tuple<SubsecondTime, HitWhere::where_t>
DramCntlr::getDataFromDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now, ShmemPerf *perf)
{
   if (Sim()->getFaultinjectionManager())
   {
      if (m_data_map.count(address) == 0)
      {
         m_data_map[address] = new Byte[getCacheBlockSize()];
         memset((void*) m_data_map[address], 0x00, getCacheBlockSize());
      }

      // NOTE: assumes error occurs in memory. If we want to model bus errors, insert the error into data_buf instead
      if (m_fault_injector)
         m_fault_injector->preRead(address, address, getCacheBlockSize(), (Byte*)m_data_map[address], now);

      memcpy((void*) data_buf, (void*) m_data_map[address], getCacheBlockSize());
   }

   // RC -- EP
   if (hit_mem_region & WITH_CXL_MEM) {
      SubsecondTime cxl_link_latency = SubsecondTime::NSfromFloat(cxl_mem_roundtrip);
      now += cxl_link_latency;     

      perf->updateTime(now, ShmemPerf::CXL_LINK);     // update shmemPerf
      m_dram_perf_model->increaseAccessLatency(cxl_link_latency);   // update dramPerfModel
      m_cxl_mem_overhead += cxl_link_latency;  // update sqlite
   }

   // EP -- EP Memory Controller
   if ((hit_mem_region & WITH_CXL_MEM) && !(hit_mem_region & WITH_CXL_BNISP)) {
      IntPtr dram_address = 0;
#ifdef RECORD_CXL_TRACE
      SubsecondTime view_address_translate_latency = m_ep_agent->Record(address, requester, READ, dram_address);
#else
      SubsecondTime view_address_translate_latency = m_ep_agent->Translate(address, requester, READ, dram_address);
#endif
      now += view_address_translate_latency;

      perf->updateTime(now, ShmemPerf::CXTNL_VAT);
      m_dram_perf_model->increaseAccessLatency(view_address_translate_latency);
      m_cxl_mem_overhead += view_address_translate_latency;
   }

   // EP Memory Controller -- DRAM
   SubsecondTime dram_access_latency = runDramPerfModel(requester, now, address, READ, perf);

   ++m_reads;
   #ifdef ENABLE_DRAM_ACCESS_COUNT
   if ((hit_mem_region & WITH_CXL_MEM) && !(hit_mem_region & WITH_CXL_BNISP)) {
      addToDramAccessCount(address, READ);
   }
   // addToDramAccessCount(address, READ);
   #endif
   MYLOG("R @ %08lx latency %s", address, itostr(dram_access_latency).c_str());

   return boost::tuple<SubsecondTime, HitWhere::where_t>(dram_access_latency, HitWhere::DRAM);
}


boost::tuple<SubsecondTime, HitWhere::where_t>
DramCntlr::putDataToDram(IntPtr address, core_id_t requester, Byte* data_buf, SubsecondTime now)
{
   if (Sim()->getFaultinjectionManager())
   {
      if (m_data_map[address] == NULL)
      {
         LOG_PRINT_ERROR("Data Buffer does not exist");
      }
      memcpy((void*) m_data_map[address], (void*) data_buf, getCacheBlockSize());

      // NOTE: assumes error occurs in memory. If we want to model bus errors, insert the error into data_buf instead
      if (m_fault_injector)
         m_fault_injector->postWrite(address, address, getCacheBlockSize(), (Byte*)m_data_map[address], now);
   }

   // RC -- EP
   if ((hit_mem_region & WITH_CXL_MEM)) {
      SubsecondTime cxl_link_latency = SubsecondTime::NSfromFloat(cxl_mem_roundtrip);
      now += cxl_link_latency;     

      m_dummy_shmem_perf.updateTime(now, ShmemPerf::CXL_LINK);     // update shmemPerf
      m_dram_perf_model->increaseAccessLatency(cxl_link_latency);   // update dramPerfModel
      m_cxl_mem_overhead += cxl_link_latency;  // update sqlite
   }

   // EP -- EP Memory Controller
   if ((hit_mem_region & WITH_CXL_MEM) && !(hit_mem_region & WITH_CXL_BNISP)) {
      IntPtr dram_address = 0;
#ifdef RECORD_CXL_TRACE
      SubsecondTime view_address_translate_latency = m_ep_agent->Record(address, requester, WRITE, dram_address);
#else
      SubsecondTime view_address_translate_latency = m_ep_agent->Translate(address, requester, WRITE, dram_address);
#endif
      now += view_address_translate_latency;

      m_dummy_shmem_perf.updateTime(now, ShmemPerf::CXTNL_VAT);      // FIXME: Write not on the critical path ? 
      m_dram_perf_model->increaseAccessLatency(view_address_translate_latency);
      m_cxl_mem_overhead += view_address_translate_latency;
   }

   // EP Memory Controller -- DRAM
   SubsecondTime dram_access_latency = runDramPerfModel(requester, now, address, WRITE, &m_dummy_shmem_perf);

   ++m_writes;
   #ifdef ENABLE_DRAM_ACCESS_COUNT
   if ((hit_mem_region & WITH_CXL_MEM) && !(hit_mem_region & WITH_CXL_BNISP)) {
      addToDramAccessCount(address, WRITE);
   }
   // addToDramAccessCount(address, WRITE);
   #endif
   MYLOG("W @ %08lx", address);

   return boost::tuple<SubsecondTime, HitWhere::where_t>(dram_access_latency, HitWhere::DRAM);
}

SubsecondTime
DramCntlr::runDramPerfModel(core_id_t requester, SubsecondTime time, IntPtr address, DramCntlrInterface::access_t access_type, ShmemPerf *perf)
{
   UInt64 pkt_size = getCacheBlockSize();
   SubsecondTime dram_access_latency = m_dram_perf_model->getAccessLatency(time, pkt_size, requester, address, access_type, perf);
   return dram_access_latency;
}

void
DramCntlr::addToDramAccessCount(IntPtr address, DramCntlrInterface::access_t access_type)
{
   m_dram_access_count[access_type][address] = m_dram_access_count[access_type][address] + 1;
}

void
DramCntlr::printDramAccessCount()
{
   for (UInt32 k = 0; k < DramCntlrInterface::NUM_ACCESS_TYPES; k++)
   {
      std::cout << m_dram_access_count[k].size() << " " << ((k == READ)? "READ" : "WRITE") << " accesses" << std::endl;
      for (AccessCountMap::iterator i = m_dram_access_count[k].begin(); i != m_dram_access_count[k].end(); i++)
      {
         if ((*i).second > 100)
         {
            LOG_PRINT("Dram Cntlr(%i), Address(0x%x), Access Count(%llu), Access Type(%s)",
                  m_memory_manager->getCore()->getId(), (*i).first, (*i).second,
                  (k == READ)? "READ" : "WRITE");
         }
      }
   }
}

}
