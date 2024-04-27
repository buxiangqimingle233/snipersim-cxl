#ifndef __ADDRESS_HOME_LOOKUP_H__
#define __ADDRESS_HOME_LOOKUP_H__

#include <vector>

#include "fixed_types.h"

/*
 * TODO abstract MMU stuff to a configure file to allow
 * user to specify number of memory controllers, and
 * the address space that each is in charge of.  Default behavior:
 * AHL assumes that every core has a memory controller,
 * and that each shares an equal number of address spaces.
 *
 * Each core has an AHL, but they must keep their data consistent
 * regarding boundaries!
 *
 * Maybe allow the ability to have public and private memory space?
 */

class AddressHomeLookup
{
   public:
      AddressHomeLookup(UInt32 ahl_param,
            std::vector<core_id_t>& core_list,
            UInt32 cache_block_size,
            core_id_t m_core_num);
      ~AddressHomeLookup();
      // Return home node for a given address

      // core_id_t getHome(IntPtr address) const;
      // In CXL, each home node manages one coherence domain. We determine which domain the core belongs to by 
      // looking at the requester core id. The CXL EP Agent locates at the DRAM controller of the home node. 
      core_id_t getHome(IntPtr address, core_id_t requester) const;  
      // Within home node, return unique, incrementing block number
      IntPtr getLinearBlock(IntPtr address) const;
      // Within home node, return unique, incrementing address to be used in cache set selection
      IntPtr getLinearAddress(IntPtr address) const;

   private:
      UInt32 m_ahl_param;
      UInt64 m_ahl_mask;
      std::vector<core_id_t> m_core_list;
      UInt32 m_total_modules;
      UInt32 m_cache_block_size;
      core_id_t m_core_num;
};

#endif /* __ADDRESS_HOME_LOOKUP_H__ */
