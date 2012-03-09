#ifndef CORE_H
#define CORE_H

// some forward declarations for cross includes
class Network;
class MemoryManagerBase;
class SyscallMdl;
class SyncClient;
class ClockSkewMinimizationClient;
class PerformanceModel;
class ShmemPerfModel;

// FIXME: Move this out of here eventually
class PinMemoryManager;

#include "mem_component.h"
#include "fixed_types.h"
#include "config.h"
#include "capi.h"
#include "lock.h"
#include "packet_type.h"
#include "dynamic_instruction_info.h"
#include "subsecond_time.h"
#include "bbv_count.h"

struct MemoryResult {
   HitWhere::where_t hit_where;
   subsecond_time_t latency;
};

MemoryResult makeMemoryResult(HitWhere::where_t _hit_where, SubsecondTime _latency);

class Core
{
   public:

      enum State
      {
         RUNNING = 0,
         INITIALIZING,
         STALLED,
         SLEEPING,
         WAKING_UP,
         IDLE,
         BROKEN,
         NUM_STATES
      };

      enum lock_signal_t
      {
         INVALID_LOCK_SIGNAL = 0,
         MIN_LOCK_SIGNAL,
         NONE = MIN_LOCK_SIGNAL,
         LOCK,
         UNLOCK,
         MAX_LOCK_SIGNAL = UNLOCK,
         NUM_LOCK_SIGNAL_TYPES = MAX_LOCK_SIGNAL - MIN_LOCK_SIGNAL + 1
      };

      enum mem_op_t
      {
         INVALID_MEM_OP = 0,
         MIN_MEM_OP,
         READ = MIN_MEM_OP,
         READ_EX,
         WRITE,
         MAX_MEM_OP = WRITE,
         NUM_MEM_OP_TYPES = MAX_MEM_OP - MIN_MEM_OP + 1
      };

      /* To what extend to make a memory access visible to the simulated instruction */
      enum MemModeled
      {
         MEM_MODELED_NONE,      /* Not at all (pure backdoor access) */
         MEM_MODELED_COUNT,     /* Count in #accesses/#misses */
         MEM_MODELED_COUNT_TLBTIME, /* Count in #accesses/#misses, queue TLBMissInstruction on TLB miss */
         MEM_MODELED_TIME,      /* Count + account for access latency (using MemAccessInstruction) */
         MEM_MODELED_FENCED,    /* Count + account for access latency as memory fence (using MemAccessInstruction) */
         MEM_MODELED_DYNINFO,   /* Count + time + queue a DynamicInstructionInfo (corresponds to a real instruction) */
         MEM_MODELED_RETURN,    /* Count + time + return data to construct DynamicInstructionInfo */
      };

      Core(SInt32 id);
      ~Core();

      void outputSummary(std::ostream &os);

      int coreSendW(int sender, int receiver, char *buffer, int size, carbon_network_t net_type);
      int coreRecvW(int sender, int receiver, char *buffer, int size, carbon_network_t net_type);

      MemoryResult readInstructionMemory(IntPtr address,
            UInt32 instruction_size);

      MemoryResult accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size, MemModeled modeled = MEM_MODELED_NONE, IntPtr eip = 0);
      MemoryResult nativeMemOp(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char* data_buffer, UInt32 data_size);

      void logMemoryHit(bool icache, mem_op_t mem_op_type, IntPtr address, MemModeled modeled = MEM_MODELED_NONE, IntPtr eip = 0);
      void countInstructions(IntPtr address, UInt32 count);

      // network accessor since network is private
      int getId() const { return m_core_id; }
      Network *getNetwork() { return m_network; }
      PerformanceModel *getPerformanceModel() { return m_performance_model; }
      MemoryManagerBase *getMemoryManager() { return m_memory_manager; }
      PinMemoryManager *getPinMemoryManager() { return m_pin_memory_manager; }
      SyscallMdl *getSyscallMdl() { return m_syscall_model; }
      SyncClient *getSyncClient() { return m_sync_client; }
      ClockSkewMinimizationClient* getClockSkewMinimizationClient() { return m_clock_skew_minimization_client; }
      ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }
      const ComponentPeriod* getDvfsDomain() const;

      State getState();
      void setState(State core_state);
      UInt64 getInstructionCount() { return m_instructions; }
      BbvCount *getBbvCount() { return &m_bbv; }
      UInt64 getInstructionsCallback() { return m_instructions_callback; }
      bool isEnabledInstructionsCallback() { return m_instructions_callback != UINT64_MAX; }
      void setInstructionsCallback(UInt64 instructions) { m_instructions_callback = m_instructions + instructions; }
      void disableInstructionsCallback() { m_instructions_callback = UINT64_MAX; }

      void enablePerformanceModels();
      void disablePerformanceModels();

   private:
      core_id_t m_core_id;
      MemoryManagerBase *m_memory_manager;
      PinMemoryManager *m_pin_memory_manager;
      Network *m_network;
      PerformanceModel *m_performance_model;
      SyscallMdl *m_syscall_model;
      SyncClient *m_sync_client;
      ClockSkewMinimizationClient *m_clock_skew_minimization_client;
      Lock m_mem_lock;
      DynamicInstructionInfo m_dyninfo_save;
      bool m_dyninfo_save_used;
      ShmemPerfModel* m_shmem_perf_model;
      BbvCount m_bbv;

      State m_core_state;

      static Lock m_global_core_lock;

      MemoryResult initiateMemoryAccess(
            MemComponent::component_t mem_component,
            lock_signal_t lock_signal,
            mem_op_t mem_op_type,
            IntPtr address,
            Byte* data_buf, UInt32 data_size,
            MemModeled modeled,
            IntPtr eip);

      PacketType getPktTypeFromUserNetType(carbon_network_t net_type);

   protected:
      // Optimized version of countInstruction has direct access to m_instructions and m_instructions_callback
      friend class InstructionModeling;

      // In contrast to core->m_performance_model->m_instructions, this one always increments,
      // also when performance modeling is disabled or when instrumenation mode is CACHE_ONLY or FAST_FORWARD
      UInt64 m_instructions;
      UInt64 m_instructions_callback;
};

#endif
