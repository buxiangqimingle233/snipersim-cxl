#include "address_home_lookup.h"
#include "log.h"
#include <iostream>
#include "cxtnl_shim.h"

AddressHomeLookup::AddressHomeLookup(UInt32 ahl_param,
      std::vector<core_id_t>& core_list,
      UInt32 cache_block_size, core_id_t m_core_num):
   m_ahl_param(ahl_param),
   m_ahl_mask((UInt64(1) << ahl_param) - 1),
   m_core_list(core_list),
   m_cache_block_size(cache_block_size),
   m_core_num(m_core_num)
{

   // Each Block Address is as follows:
   // /////////////////////////////////////////////////////////// //
   //   block_num               |   block_offset                  //
   // /////////////////////////////////////////////////////////// //

   LOG_ASSERT_ERROR((1 << m_ahl_param) >= (SInt32) m_cache_block_size,
         "2^AHL param(%u) must be >= Cache Block Size(%u)",
         m_ahl_param, m_cache_block_size);
   m_total_modules = core_list.size();
}

AddressHomeLookup::~AddressHomeLookup()
{
   // There is no memory to deallocate, so destructor has no function
}


// core_id_t AddressHomeLookup::getHome(IntPtr address) const
// {
//    SInt32 module_num = (address >> m_ahl_param) % m_total_modules;
//    LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) m_total_modules, "module_num(%i), total_modules(%u)", module_num, m_total_modules);

//    LOG_PRINT("address(0x%x), module_num(%i)", address, module_num);
//    return (m_core_list[module_num]);
// }


core_id_t AddressHomeLookup::getHome(IntPtr address) const
{
   // SInt32 module_num = requester / (m_core_num /  m_total_modules);
   SInt32 module_num = (address >> m_ahl_param) % m_total_modules;
   LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) m_total_modules, "module_num(%i), total_modules(%u)", module_num, m_total_modules);

   // std::cout << "GetHome: " << requester << " " << module_num << " " << m_core_num /  m_total_modules << std::endl;
   LOG_PRINT("address(0x%x), module_num(%i)", address, module_num);
   return (m_core_list[module_num]);
}


core_id_t AddressHomeLookup::getHomeCXL(IntPtr address, core_id_t requester, int hit_mem_region) const
{
   SInt32 module_num = 0;

   // if (BELONGS_TO_TYPE3(hit_mem_region)) {
   //    module_num = requester / (m_core_num /  m_total_modules);
   //    // std::cout << "GetHome: " << requester << " " << address << " " << module_num << std::endl;
   // } else if (BELONGS_TO_TYPE2(hit_mem_region)) {
   //    module_num = (address >> m_ahl_param) % m_total_modules;
   //    // std::cout << "GetHome: " << requester << " " << address << " " << module_num << std::endl;
   // } else {
   //    module_num = (address >> m_ahl_param) % m_total_modules;
   // }

   module_num = (address >> m_ahl_param) % m_total_modules;

   LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) m_total_modules, "module_num(%i), total_modules(%u)", module_num, m_total_modules);

   // std::cout << "GetHome: " << requester << " " << module_num << " " << m_core_num /  m_total_modules << std::endl;
   LOG_PRINT("address(0x%x), module_num(%i)", address, module_num);
   return (m_core_list[module_num]);
}

IntPtr AddressHomeLookup::getLinearBlock(IntPtr address) const
{
   return (address >> m_ahl_param) / m_total_modules;
}

IntPtr AddressHomeLookup::getLinearAddress(IntPtr address) const
{
   // std::cout << "getLinearAddress\n" << std::endl;
   throw std::runtime_error("getLinearAddress is not supported in CXL-based Simulation");
   return (getLinearBlock(address) << m_ahl_param) | (address & m_ahl_mask);
}
