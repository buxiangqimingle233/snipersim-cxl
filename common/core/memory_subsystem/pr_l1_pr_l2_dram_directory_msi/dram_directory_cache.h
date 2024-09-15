#pragma once

#include <vector>

#include "directory.h"
#include "shmem_perf_model.h"
#include "subsecond_time.h"
#include "shmem_req.h"
#include "stats.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class DramDirectoryCache
   {
      protected:
         Directory* m_directory;
         UInt32* m_replacement_ptrs;
         std::vector<DirectoryEntry*> m_replaced_directory_entry_list;

         UInt32 m_total_entries;
         UInt32 m_associativity;

         UInt32 m_num_sets;
         UInt32 m_cache_block_size;
         UInt32 m_log_num_sets;
         UInt32 m_log_cache_block_size;

         UInt64 allocation_try;

         ComponentLatency m_dram_directory_cache_access_time;
         ShmemPerfModel* m_shmem_perf_model;

         ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

         void splitAddress(IntPtr address, IntPtr& tag, UInt32& set_index);
         UInt32 getCacheBlockSize() { return m_cache_block_size; }
         UInt32 getLogCacheBlockSize() { return m_log_cache_block_size; }
         UInt32 getNumSets() { return m_num_sets; }
         UInt32 getLogNumSets() { return m_log_num_sets; }

      public:
         DramDirectoryCache(core_id_t core_id,
               String directory_type_str,
               UInt32 total_entries,
               UInt32 associativity,
               UInt32 cache_block_size,
               UInt32 max_hw_sharers,
               UInt32 max_num_sharers,
               ComponentLatency dram_directory_cache_access_time,
               ShmemPerfModel* shmem_perf_model,
               String name = "");
         ~DramDirectoryCache();

         DirectoryEntry* getDirectoryEntry(IntPtr address, bool modeled = true);
         DirectoryEntry* replaceDirectoryEntry(IntPtr replaced_address, IntPtr address, bool modeled);
         void invalidateDirectoryEntry(IntPtr address);
         void getReplacementCandidates(IntPtr address, std::vector<DirectoryEntry*>& replacement_candidate_list);

         UInt32 getMaxHwSharers() const { return m_directory->getMaxHwSharers(); }
   };

   class CXLSnoopFilter: public DramDirectoryCache {
      public:
      SubsecondTime& cxl_bi_overhead;
      float cxl_backinv_roundtrip;

      UInt64 evict;

      CXLSnoopFilter(core_id_t core_id,
               String directory_type_str,
               UInt32 total_entries,
               UInt32 associativity,
               UInt32 cache_block_size,
               UInt32 max_hw_sharers,
               UInt32 max_num_sharers,
               ComponentLatency dram_directory_cache_access_time,
               ShmemPerfModel* shmem_perf_model,
               float cxl_backinv_roundtrip,
               SubsecondTime& cxl_bi_overhead,
               String name = "")
      : DramDirectoryCache(core_id, directory_type_str, total_entries, associativity, \
         cache_block_size, max_hw_sharers, max_num_sharers, dram_directory_cache_access_time, \
         shmem_perf_model, name), cxl_bi_overhead(cxl_bi_overhead), cxl_backinv_roundtrip(cxl_backinv_roundtrip), 
         evict(0)
         { 
            registerStatsMetric("snoop-filter-directory", core_id, String("evict"), &evict);
         }

      void TryAllocate(ShmemReq* shmem_req); 
   };
}
