#include "core.h"
#include "network.h"
#include "syscall_model.h"
#include "memory_manager_base.h"
#include "pin_memory_manager.h"
#include "performance_model.h"
#include "core_manager.h"
#include "dvfs_manager.h"
#include "hooks_manager.h"
#include "trace_manager.h"
#include "simulator.h"
#include "log.h"
#include "config.hpp"
#include "stats.h"

#include <cstring>

#if 0
   extern Lock iolock;
#  define MYLOG(...) { ScopedLock l(iolock); fflush(stderr); fprintf(stderr, "[%8lu] %dcor %-25s@%03u: ", getPerformanceModel()->getCycleCount(ShmemPerfModel::_USER_THREAD), m_core_id, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); }
#else
#  define MYLOG(...) {}
#endif

const char * ModeledString(Core::MemModeled modeled) {
   switch(modeled)
   {
      case Core::MEM_MODELED_NONE:           return "none";
      case Core::MEM_MODELED_COUNT:          return "count";
      case Core::MEM_MODELED_COUNT_TLBTIME:  return "count/tlb";
      case Core::MEM_MODELED_TIME:           return "time";
      case Core::MEM_MODELED_FENCED:         return "fenced";
      case Core::MEM_MODELED_DYNINFO:        return "dyninfo";
      case Core::MEM_MODELED_RETURN:         return "return";
   }
  return "?";
}



Lock Core::m_global_core_lock;

Core::Core(SInt32 id)
   : m_core_id(id)
   , m_dyninfo_save_used(false)
   , m_bbv(id)
   , m_core_state(Core::IDLE)
   , m_icache_last_block(-1)
   , m_icache_hits(0)
   , m_instructions(0)
   , m_instructions_callback(UINT64_MAX)
{
   LOG_PRINT("Core ctor for: %d", id);

   registerStatsMetric("core", id, "instructions", &m_instructions);

   m_network = new Network(this);

   m_performance_model = PerformanceModel::create(this);

   m_shmem_perf_model = new ShmemPerfModel();

   LOG_PRINT("instantiated memory manager model");
   m_memory_manager = MemoryManagerBase::createMMU(
         Sim()->getCfg()->getString("caching_protocol/type"),
         this, m_network, m_shmem_perf_model);

   m_pin_memory_manager = new PinMemoryManager(this);
}

Core::~Core()
{
   delete m_pin_memory_manager;
   delete m_memory_manager;
   delete m_shmem_perf_model;
   delete m_performance_model;
   delete m_network;
}

void Core::outputSummary(std::ostream &os)
{
   getPerformanceModel()->outputSummary(os);
   getNetwork()->outputSummary(os);
   getShmemPerfModel()->outputSummary(os);
   getMemoryManager()->outputSummary(os);
}

int Core::coreSendW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   SInt32 sent;
   if (receiver == CAPI_ENDPOINT_ALL)
      sent = m_network->netBroadcast(pkt_type, buffer, size);
   else
      sent = m_network->netSend(receiver, pkt_type, buffer, size);

   LOG_ASSERT_ERROR(sent == size, "Bytes Sent(%i), Message Size(%i)", sent, size);

   return sent == size ? 0 : -1;
}

int Core::coreRecvW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType pkt_type = getPktTypeFromUserNetType(net_type);

   NetPacket packet;
   if (sender == CAPI_ENDPOINT_ANY)
      packet = m_network->netRecvType(pkt_type);
   else
      packet = m_network->netRecv(sender, pkt_type);

   LOG_PRINT("Got packet: from %i, to %i, type %i, len %i", packet.sender, packet.receiver, (SInt32)packet.type, packet.length);

   LOG_ASSERT_ERROR((unsigned)size == packet.length, "Core: User thread requested packet of size: %d, got a packet from %d of size: %d", size, sender, packet.length);

   memcpy(buffer, packet.data, size);

   // De-allocate dynamic memory
   // Is this the best place to de-allocate packet.data ??
   delete [](Byte*)packet.data;

   return (unsigned)size == packet.length ? 0 : -1;
}

PacketType Core::getPktTypeFromUserNetType(carbon_network_t net_type)
{
   switch(net_type)
   {
      case CARBON_NET_USER_1:
         return USER_1;

      case CARBON_NET_USER_2:
         return USER_2;

      default:
         LOG_PRINT_ERROR("Unrecognized User Network(%u)", net_type);
         return (PacketType) -1;
   }
}

const ComponentPeriod* Core::getDvfsDomain() const
{
   return Sim()->getDvfsManager()->getCoreDomain(this->getId());
}

void Core::enablePerformanceModels()
{
   getShmemPerfModel()->enable();
   getMemoryManager()->enableModels();
   getNetwork()->enableModels();
   getPerformanceModel()->enable();
}

void Core::disablePerformanceModels()
{
   getShmemPerfModel()->disable();
   getMemoryManager()->disableModels();
   getNetwork()->disableModels();
   getPerformanceModel()->disable();
}

void
Core::countInstructions(IntPtr address, UInt32 count)
{
   m_instructions += count;
   if (m_bbv.sample())
      m_bbv.count(address, count);
   m_performance_model->countInstructions(address, count);

   if (isEnabledInstructionsCallback()) {
      if (m_instructions >= m_instructions_callback)
      {
         disableInstructionsCallback();
         Sim()->getHooksManager()->callHooks(HookType::HOOK_INSTR_COUNT, m_core_id);
      }
   }
}

MemoryResult
makeMemoryResult(HitWhere::where_t _hit_where, SubsecondTime _latency)
{
   LOG_ASSERT_ERROR(_hit_where < HitWhere::NUM_HITWHERES, "Invalid HitWhere %u", (long)_hit_where);
   MemoryResult res;
   res.hit_where = _hit_where;
   res.latency = _latency;
   return res;
}

void
Core::logMemoryHit(bool icache, mem_op_t mem_op_type, IntPtr address, MemModeled modeled, IntPtr eip)
{
   getMemoryManager()->addL1Hits(icache, mem_op_type, 1);
   if (modeled == MEM_MODELED_DYNINFO)
   {
      DynamicInstructionInfo info = DynamicInstructionInfo::createMemoryInfo(eip, true, SubsecondTime::Zero(), address, 8, (mem_op_type == WRITE) ? Operand::WRITE : Operand::READ, 0, (HitWhere::where_t)(icache ? MemComponent::L1_ICACHE : MemComponent::L1_DCACHE));
      m_performance_model->pushDynamicInstructionInfo(info);
   }
}

MemoryResult
Core::readInstructionMemory(IntPtr address, UInt32 instruction_size)
{
   LOG_PRINT("Instruction: Address(0x%x), Size(%u), Start READ",
           address, instruction_size);

   UInt32 blockmask = ~(getMemoryManager()->getCacheBlockSize() - 1);
   bool single_cache_line = ((address & blockmask) == ((address + instruction_size - 1) & blockmask));

   // TODO: Nehalem gets 16 bytes at once from the L1I, so if an access is in the same 16-byte block
   //   as the previous one we shouldn't even count it as a hit

   // If we in the same cache line as the last icache access, report a hit
   if (single_cache_line && ((address & blockmask) == m_icache_last_block))
   {
      m_icache_hits++;
      return makeMemoryResult(HitWhere::L1I, getMemoryManager()->getL1HitLatency());
   }

   // Update cache counters if needed
   if (m_icache_hits)
   {
      getMemoryManager()->addL1Hits(true, Core::READ, m_icache_hits);
      m_icache_hits = 0;
   }

   // Update the most recent cache line accessed
   if (single_cache_line)
   {
      m_icache_last_block = address & blockmask;
   }

   // Cases with multiple cache lines or when we are not sure that it will be a hit call into the caches
   return initiateMemoryAccess(MemComponent::L1_ICACHE,
             Core::NONE, Core::READ, address, NULL, instruction_size, MEM_MODELED_COUNT_TLBTIME, 0, SubsecondTime::MaxTime());
}

MemoryResult
Core::initiateMemoryAccess(MemComponent::component_t mem_component,
      lock_signal_t lock_signal,
      mem_op_t mem_op_type,
      IntPtr address,
      Byte* data_buf, UInt32 data_size,
      MemModeled modeled,
      IntPtr eip,
      SubsecondTime now)
{
   MYLOG("access %lx+%u %c%c modeled(%s)", address, data_size, mem_op_type == Core::WRITE ? 'W' : 'R', mem_op_type == Core::READ_EX ? 'X' : ' ', ModeledString(modeled));

   if (data_size <= 0)
   {
      if (modeled == MEM_MODELED_DYNINFO)
      {
         DynamicInstructionInfo info = DynamicInstructionInfo::createMemoryInfo(eip, true, SubsecondTime::Zero(), address, data_size, (mem_op_type == WRITE) ? Operand::WRITE : Operand::READ, 0, (HitWhere::where_t)mem_component);
         m_performance_model->pushDynamicInstructionInfo(info);
      }
      return makeMemoryResult((HitWhere::where_t)mem_component,SubsecondTime::Zero());
   }

   // Setting the initial time
   SubsecondTime initial_time = (now == SubsecondTime::MaxTime()) ? getPerformanceModel()->getElapsedTime() : now;

   // Protect from concurrent access by user thread (doing rewritten memops) and core thread (doing icache lookups)
   if (lock_signal != Core::UNLOCK)
      m_mem_lock.acquire();

#if 0
   static int i = 0;
   static Lock iolock;
   if ((i++) % 1000 == 0) {
      ScopedLock slio(iolock);
      printf("[TIME],%lu,", (Timer::now() / 100000) % 10000000);
      for(int i = 0; i < Sim()->getConfig()->getApplicationCores(); ++i)
        if (i == m_core_id)
          printf("%lu,%lu,%lu,", initial_time, getShmemPerfModel()->getCycleCount(ShmemPerfModel::_USER_THREAD), getShmemPerfModel()->getCycleCount(ShmemPerfModel::_SIM_THREAD));
        else
          printf(",,,");
      printf("\n");
   }
#endif

   getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD, initial_time);

   LOG_PRINT("Time(%s), %s - ADDR(0x%x), data_size(%u), START",
        itostr(initial_time).c_str(),
        ((mem_op_type == READ) ? "READ" : "WRITE"),
        address, data_size);

   UInt32 num_misses = 0;
   HitWhere::where_t hit_where = HitWhere::UNKNOWN;
   UInt32 cache_block_size = getMemoryManager()->getCacheBlockSize();

   IntPtr begin_addr = address;
   IntPtr end_addr = address + data_size;
   IntPtr begin_addr_aligned = begin_addr - (begin_addr % cache_block_size);
   IntPtr end_addr_aligned = end_addr - (end_addr % cache_block_size);
   Byte *curr_data_buffer_head = (Byte*) data_buf;

   for (IntPtr curr_addr_aligned = begin_addr_aligned; curr_addr_aligned <= end_addr_aligned; curr_addr_aligned += cache_block_size)
   {
      // Access the cache one line at a time
      UInt32 curr_offset;
      UInt32 curr_size;

      // Determine the offset
      if (curr_addr_aligned == begin_addr_aligned)
      {
         curr_offset = begin_addr % cache_block_size;
      }
      else
      {
         curr_offset = 0;
      }

      // Determine the size
      if (curr_addr_aligned == end_addr_aligned)
      {
         curr_size = (end_addr % cache_block_size) - (curr_offset);
         if (curr_size == 0)
         {
            continue;
         }
      }
      else
      {
         curr_size = cache_block_size - (curr_offset);
      }

      LOG_PRINT("Start InitiateSharedMemReq: ADDR(0x%x), offset(%u), curr_size(%u)", curr_addr_aligned, curr_offset, curr_size);

      HitWhere::where_t this_hit_where = getMemoryManager()->coreInitiateMemoryAccess(
               mem_component,
               lock_signal,
               mem_op_type,
               curr_addr_aligned, curr_offset,
               data_buf ? curr_data_buffer_head : NULL, curr_size,
               modeled);

      if (hit_where != (HitWhere::where_t)mem_component)
      {
         // If it is a READ or READ_EX operation,
         // 'initiateSharedMemReq' causes curr_data_buffer_head
         // to be automatically filled in
         // If it is a WRITE operation,
         // 'initiateSharedMemReq' reads the data
         // from curr_data_buffer_head
         num_misses ++;
      }
      if (hit_where == HitWhere::UNKNOWN || (this_hit_where != HitWhere::UNKNOWN && this_hit_where > hit_where))
         hit_where = this_hit_where;

      LOG_PRINT("End InitiateSharedMemReq: ADDR(0x%x), offset(%u), curr_size(%u)", curr_addr_aligned, curr_offset, curr_size);

      // Increment the buffer head
      curr_data_buffer_head += curr_size;
   }

   // Get the final cycle time
   SubsecondTime final_time = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
   LOG_ASSERT_ERROR(final_time >= initial_time,
         "final_time(%s) < initial_time(%s)",
         itostr(final_time).c_str(),
         itostr(initial_time).c_str());

   LOG_PRINT("Time(%s), %s - ADDR(0x%x), data_size(%u), END\n",
        itostr(final_time).c_str(),
        ((mem_op_type == READ) ? "READ" : "WRITE"),
        address, data_size);

   if (lock_signal != Core::LOCK) {
      m_mem_lock.release();
      if (m_dyninfo_save_used && modeled == MEM_MODELED_DYNINFO) {
         // Now we released the cache lock, and we're in a context that normally pushes dyninfos,
         // push a possible saved dyninfo
         m_performance_model->pushDynamicInstructionInfo(m_dyninfo_save);
         m_dyninfo_save_used = false;
      }
   }

   // Calculate the round-trip time
   SubsecondTime shmem_time = final_time - initial_time;

   switch(modeled) {
      case MEM_MODELED_DYNINFO:
      {
         LOG_ASSERT_ERROR(hit_where != HitWhere::UNKNOWN, "hit_where = HitWhere::UNKNOWN"); // HitWhere::UNKNOWN is used to indicate the timing thread still needs to do the access.
         DynamicInstructionInfo info = DynamicInstructionInfo::createMemoryInfo(eip, true, shmem_time, address, data_size, (mem_op_type == WRITE) ? Operand::WRITE : Operand::READ, num_misses, hit_where);
         if (lock_signal == Core::LOCK) {
            // deadlock can occur if we try to push a dyninfo to a full queue while holding m_mem_lock
            LOG_ASSERT_ERROR(m_dyninfo_save_used == false, "We already have a saved m_dyninfo_save");
            m_dyninfo_save = info;
            m_dyninfo_save_used = true;
         } else
            m_performance_model->pushDynamicInstructionInfo(info);
         break;
      }
      case MEM_MODELED_TIME:
      case MEM_MODELED_FENCED:
         if (m_performance_model->isEnabled()) {
            /* queue a fake instruction that will account for the access latency */
            Instruction *i = new MemAccessInstruction(shmem_time, address, data_size, modeled == MEM_MODELED_FENCED);
            m_performance_model->queueDynamicInstruction(i);
         }
         break;
      case MEM_MODELED_NONE:
      case MEM_MODELED_COUNT:
      case MEM_MODELED_COUNT_TLBTIME:
      case MEM_MODELED_RETURN:
         break;
   }

   if (modeled != MEM_MODELED_NONE)
   {
      getShmemPerfModel()->incrTotalMemoryAccessLatency(shmem_time);
   }

   LOG_ASSERT_ERROR(hit_where != HitWhere::UNKNOWN, "HitWhere == UNKNOWN");

   return makeMemoryResult(hit_where, shmem_time);
}

// FIXME: This should actually be 'accessDataMemory()'
/*
 * accessMemory (lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size)
 *
 * Arguments:
 *   lock_signal :: NONE, LOCK, or UNLOCK
 *   mem_op_type :: READ, READ_EX, or WRITE
 *   d_addr :: address of location we want to access (read or write)
 *   data_buffer :: buffer holding data for WRITE or buffer which must be written on a READ
 *   data_size :: size of data we must read/write
 *
 * Return Value:
 *   number of misses :: State the number of cache misses
 */
MemoryResult
Core::accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size, MemModeled modeled, IntPtr eip, SubsecondTime now)
{
   if (modeled == MEM_MODELED_DYNINFO)
      LOG_ASSERT_ERROR(eip != 0, "modeled == MEM_MODELED_DYNINFO but no eip given");

   // In PINTOOL mode, if the data is requested, copy it to/from real memory
   if (data_buffer)
   {
      if (Sim()->getConfig()->getSimulationMode() == Config::PINTOOL)
      {
         nativeMemOp (NONE, mem_op_type, d_addr, data_buffer, data_size);
      }
      else if (Sim()->getConfig()->getSimulationMode() == Config::STANDALONE)
      {
         Sim()->getTraceManager()->accessMemory(m_core_id, lock_signal, mem_op_type, d_addr, data_buffer, data_size);
      }
      data_buffer = NULL; // initiateMemoryAccess's data is not used
   }

   return initiateMemoryAccess(MemComponent::L1_DCACHE, lock_signal, mem_op_type, d_addr, (Byte*) data_buffer, data_size, modeled, eip, now);
}


MemoryResult
Core::nativeMemOp(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size)
{
   if (data_size <= 0)
   {
      return makeMemoryResult(HitWhere::UNKNOWN,SubsecondTime::Zero());
   }

   if (lock_signal == LOCK)
   {
      assert(mem_op_type == READ_EX);
      m_global_core_lock.acquire();
   }

   if ( (mem_op_type == READ) || (mem_op_type == READ_EX) )
   {
      memcpy ((void*) data_buffer, (void*) d_addr, (size_t) data_size);
   }
   else if (mem_op_type == WRITE)
   {
      memcpy ((void*) d_addr, (void*) data_buffer, (size_t) data_size);
   }

   if (lock_signal == UNLOCK)
   {
      assert(mem_op_type == WRITE);
      m_global_core_lock.release();
   }

   return makeMemoryResult(HitWhere::UNKNOWN,SubsecondTime::Zero());
}
