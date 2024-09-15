#include "dram_directory_cache.h"
#include "log.h"
#include "utils.h"
#include "shmem_req.h"
#include "simulator.h"
#include "core_manager.h"
#include "cxtnl_shim.h"
#include "performance_model.h"
#include "shmem_perf.h"
#include "../parametric_dram_directory_msi/memory_manager.h"

namespace PrL1PrL2DramDirectoryMSI
{

DramDirectoryCache::DramDirectoryCache(
      core_id_t core_id,
      String directory_type_str,
      UInt32 total_entries,
      UInt32 associativity,
      UInt32 cache_block_size,
      UInt32 max_hw_sharers,
      UInt32 max_num_sharers,
      ComponentLatency dram_directory_cache_access_time,
      ShmemPerfModel* shmem_perf_model,
      String name):
   m_total_entries(total_entries),
   m_associativity(associativity),
   m_cache_block_size(cache_block_size),
   m_dram_directory_cache_access_time(dram_directory_cache_access_time),
   m_shmem_perf_model(shmem_perf_model)
{
   m_num_sets = m_total_entries / m_associativity;

   // Instantiate the directory
   m_directory = new Directory(core_id, directory_type_str, total_entries, max_hw_sharers, max_num_sharers, name);
   m_replacement_ptrs = new UInt32[m_num_sets];

   // Logs
   m_log_num_sets = floorLog2(m_num_sets);
   m_log_cache_block_size = floorLog2(m_cache_block_size);

   registerStatsMetric(name + "directory", core_id, String("allocation-try"), &allocation_try);
}

DramDirectoryCache::~DramDirectoryCache()
{
   delete m_replacement_ptrs;
   delete m_directory;
}

DirectoryEntry*
DramDirectoryCache::getDirectoryEntry(IntPtr address, bool modeled)
{
   if (m_shmem_perf_model && modeled)
      getShmemPerfModel()->incrElapsedTime(m_dram_directory_cache_access_time.getLatency(), ShmemPerfModel::_SIM_THREAD);

   IntPtr tag;
   UInt32 set_index;

   // Assume that it always hit in the Dram Directory Cache for now
   splitAddress(address, tag, set_index);

   // Find the relevant directory entry
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      DirectoryEntry* directory_entry = m_directory->getDirectoryEntry(set_index * m_associativity + i);

      if (directory_entry->getAddress() == address)
      {
         if (m_shmem_perf_model && modeled)
            getShmemPerfModel()->incrElapsedTime(directory_entry->getLatency(), ShmemPerfModel::_SIM_THREAD);
         // Simple check for now. Make sophisticated later
         return directory_entry;
      }
   }

   allocation_try++;

   // Find a free directory entry if one does not currently exist
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      DirectoryEntry* directory_entry = m_directory->getDirectoryEntry(set_index * m_associativity + i);
      if (directory_entry->getAddress() == INVALID_ADDRESS)
      {
         // Simple check for now. Make sophisticated later
         directory_entry->setAddress(address);
         return directory_entry;
      }
   }

   // Check in the m_replaced_directory_entry_list
   std::vector<DirectoryEntry*>::iterator it;
   for (it = m_replaced_directory_entry_list.begin(); it != m_replaced_directory_entry_list.end(); it++)
   {
      if ((*it)->getAddress() == address)
      {
         return (*it);
      }
   }

   return (DirectoryEntry*) NULL;
}

void
DramDirectoryCache::getReplacementCandidates(IntPtr address, std::vector<DirectoryEntry*>& replacement_candidate_list)
{
   if (getDirectoryEntry(address) != NULL) {
      return;
   }
   assert(getDirectoryEntry(address) == NULL);

   IntPtr tag;
   UInt32 set_index;
   splitAddress(address, tag, set_index);

   for (UInt32 i = 0; i < m_associativity; i++)
   {
      replacement_candidate_list.push_back(m_directory->getDirectoryEntry(set_index * m_associativity + ((i + m_replacement_ptrs[set_index]) % m_associativity)));
   }
   ++m_replacement_ptrs[set_index];
}

DirectoryEntry*
DramDirectoryCache::replaceDirectoryEntry(IntPtr replaced_address, IntPtr address, bool modeled)
{
   if (m_shmem_perf_model && modeled)
      getShmemPerfModel()->incrElapsedTime(m_dram_directory_cache_access_time.getLatency(), ShmemPerfModel::_SIM_THREAD);

   IntPtr tag;
   UInt32 set_index;
   splitAddress(replaced_address, tag, set_index);

   for (UInt32 i = 0; i < m_associativity; i++)
   {
      DirectoryEntry* replaced_directory_entry = m_directory->getDirectoryEntry(set_index * m_associativity + i);
      if (replaced_directory_entry->getAddress() == replaced_address)
      {
         m_replaced_directory_entry_list.push_back(replaced_directory_entry);

         DirectoryEntry* directory_entry = m_directory->createDirectoryEntry();
         directory_entry->setAddress(address);
         m_directory->setDirectoryEntry(set_index * m_associativity + i, directory_entry);

         return directory_entry;
      }
   }

   // Should not reach here
   LOG_PRINT_ERROR("");
}

void
DramDirectoryCache::invalidateDirectoryEntry(IntPtr address)
{
   std::vector<DirectoryEntry*>::iterator it;
   for (it = m_replaced_directory_entry_list.begin(); it != m_replaced_directory_entry_list.end(); it++)
   {
      if ((*it)->getAddress() == address)
      {
         delete (*it);
         m_replaced_directory_entry_list.erase(it);

         return;
      }
   }

   // Should not reach here
   LOG_PRINT_ERROR("");
}

void
DramDirectoryCache::splitAddress(IntPtr address, IntPtr& tag, UInt32& set_index)
{
   IntPtr cache_block_address = address >> getLogCacheBlockSize();
   tag = cache_block_address >> getLogNumSets();
   set_index = ((UInt32) cache_block_address) & (getNumSets() - 1);

}


void CXLSnoopFilter::TryAllocate(ShmemReq* shmem_req) {
   IntPtr address = shmem_req->getShmemMsg()->getAddress();
   core_id_t requester = shmem_req->getShmemMsg()->getRequester();
   SubsecondTime msg_time = m_shmem_perf_model->getElapsedTime(ShmemPerfModel::_SIM_THREAD);

   // Not belongs to type 2
   if (!BELONGS_TO_TYPE2(shmem_req->getShmemMsg()->hit_mem_region)) {
      return;
   }

   // No replacement is needed
   if (getDirectoryEntry(address) != NULL) {
      return;
   }

   std::vector<DirectoryEntry*> replacement_candidate_list;
   getReplacementCandidates(address, replacement_candidate_list);

   std::vector<DirectoryEntry*>::iterator it;
   std::vector<DirectoryEntry*>::iterator replacement_candidate = replacement_candidate_list.end();
   for (it = replacement_candidate_list.begin(); it != replacement_candidate_list.end(); it++)
   {
      if ( ( (replacement_candidate == replacement_candidate_list.end()) ||
            ((*replacement_candidate)->getNumSharers() > (*it)->getNumSharers())
         ))
      {
         replacement_candidate = it;
      }
   }

   // FIXME: No candidate means that the address is already located in the SF
   // This errors is caused by the fact that the allocated address is determined by the directory but not the SF, 
   // so that we could not ensure the address misses on SF 
   DirectoryState::dstate_t curr_dstate = (*replacement_candidate)->getDirectoryBlockInfo()->getDState();

   IntPtr replaced_address = (*replacement_candidate)->getAddress();

   replaceDirectoryEntry(replaced_address, address, true);

   evict++;

   SubsecondTime l = SubsecondTime::NSfromFloat(cxl_backinv_roundtrip);
   atomic_add_subsecondtime(cxl_bi_overhead, l);
   Sim()->getCoreManager()->getCoreFromID(requester)->getPerformanceModel()->incrementElapsedTime(l);

   shmem_req->updateTime(shmem_req->getTime() + l);
   shmem_req->getShmemMsg()->getPerf()->updateTime(shmem_req->getTime() + l, ShmemPerf::CXL_BI);

   SubsecondTime now = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD);
   ((ParametricDramDirectoryMSI::MemoryManager*)(Sim()->getCoreManager()->getCoreFromID(requester)->getMemoryManager()))->getDramCntlr()->getEPAgent(shmem_req->getShmemMsg()->getRequester())->recordBusTraffic(now, shmem_req->getShmemMsg()->getMsgLen());

   // getMemoryManager()->getShmemPerfModel()->incrElapsedTime(l, ShmemPerfModel::_USER_THREAD);
   // updateShmemPerf(shmem_req, ShmemPerf::CXL_CC);

   // LOG_ASSERT_ERROR(replacement_candidate != replacement_candidate_list.end(),
   //       "Cannot find a directory entry to be replaced with a non-zero request list (see Redmine #175)");
}

}
