#include "dram_cntlr_interface.h"
#include "memory_manager.h"
#include "shmem_msg.h"
#include "shmem_perf.h"
#include "log.h"
#include "fixed_types.h"
#include "cxtnl_shim.h"

DramCntlrInterface::DramCntlrInterface(MemoryManagerBase* memory_manager, ShmemPerfModel* shmem_perf_model, UInt32 cache_block_size)
   : m_memory_manager(memory_manager)
   , m_shmem_perf_model(shmem_perf_model)
   , m_cache_block_size(cache_block_size)
   , cxl_mem_roundtrip(0)
   , hit_mem_region(0)
{
   if (Sim()->getCfg()->hasKey("perf_model/cxl/enabled") && Sim()->getCfg()->getBool("perf_model/cxl/enabled")) {
      cxl_mem_roundtrip = Sim()->getCfg()->getInt("perf_model/cxl/cxl_mem_roundtrip");
   }
   int num_memory_controllers = Sim()->getCfg()->getInt("perf_model/dram/num_controllers");
   if (num_memory_controllers == -1) {
      SInt32 core_count = Config::getSingleton()->getApplicationCores();
      UInt32 smt_cores = Sim()->getCfg()->getInt("perf_model/core/logical_cpus");
      SInt32 memory_controllers_interleaving = Sim()->getCfg()->getInt("perf_model/dram/controllers_interleaving") * smt_cores;
      num_memory_controllers = (core_count + memory_controllers_interleaving - 1) / memory_controllers_interleaving;
   }
   m_core_count = Config::getSingleton()->getApplicationCores();
   m_ep_agents = std::vector<CxTnLMemShim::EPAgent*>(num_memory_controllers, NULL);
}

DramCntlrInterface::~DramCntlrInterface()
{
   // delete m_ep_agent;
}


void DramCntlrInterface::handleMsgFromTagDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg)
{
   PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t shmem_msg_type = shmem_msg->getMsgType();
   SubsecondTime msg_time = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD);
   shmem_msg->getPerf()->updateTime(msg_time);

   switch (shmem_msg_type)
   {
      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_READ_REQ:
      {
         IntPtr address = shmem_msg->getAddress();
         Byte data_buf[getCacheBlockSize()];
         SubsecondTime dram_latency;
         HitWhere::where_t hit_where;
         
         // pass the region from msg to dram-cn
         hit_mem_region = shmem_msg->hit_mem_region;
         boost::tie(dram_latency, hit_where) = getDataFromDram(address, shmem_msg->getRequester(), data_buf, msg_time, shmem_msg->getPerf());

         getShmemPerfModel()->incrElapsedTime(dram_latency, ShmemPerfModel::_SIM_THREAD);

         shmem_msg->getPerf()->updateTime(getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_SIM_THREAD),
            hit_where == HitWhere::DRAM_CACHE ? ShmemPerf::DRAM_CACHE : ShmemPerf::DRAM);

         getMemoryManager()->sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_READ_REP,
               MemComponent::DRAM, MemComponent::TAG_DIR,
               shmem_msg->getRequester() /* requester */,
               sender /* receiver */,
               address,
               data_buf, getCacheBlockSize(),
               hit_where, shmem_msg->getPerf(), ShmemPerfModel::_SIM_THREAD);
         break;
      }

      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_WRITE_REQ:
      {
         // set the memory region
         hit_mem_region = shmem_msg->hit_mem_region;
         putDataToDram(shmem_msg->getAddress(), shmem_msg->getRequester(), shmem_msg->getDataBuf(), msg_time);

         // DRAM latency is ignored on write
         break;
      }

      case PrL1PrL2DramDirectoryMSI::ShmemMsg::DRAM_WRITE_CLEAN_REQ: {
         LOG_ASSERT_ERROR(BELONGS_TO_TYPE3(shmem_msg->hit_mem_region), "WRITE CLEAN Req must target Type-3 memory region");
         if (BELONGS_TO_TYPE3(hit_mem_region) && IS_TRACKED_READ(hit_mem_region)) {
            core_id_t requester = shmem_msg->getRequester();
            IntPtr address = shmem_msg->getAddress();
            getEPAgent(requester)->RemoveViewBackBF(address);  
         }
         break;
      }

      default:
         LOG_PRINT_ERROR("Unrecognized Shmem Msg Type: %u", shmem_msg_type);
         break;
   }
}
