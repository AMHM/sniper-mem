#ifndef PERFORMANCE_MODEL_H
#define PERFORMANCE_MODEL_H
// This class represents the actual performance model for a given core

#include "instruction.h"
#include "basic_block.h"
#include "fixed_types.h"
#include "mt_circular_queue.h"
#include "lock.h"
#include "dynamic_instruction_info.h"
#include "subsecond_time.h"

#include <queue>
#include <iostream>

// Forward Decls
class Core;
class BranchPredictor;

class PerformanceModel
{
public:
   static const SubsecondTime DyninsninfoNotAvailable() { return SubsecondTime::MaxTime(); }

   PerformanceModel(Core* core);
   virtual ~PerformanceModel();

   void queueDynamicInstruction(Instruction *i);
   void queueBasicBlock(BasicBlock *basic_block);
   void handleIdleInstruction(Instruction *instruction);
   void iterate();

   virtual void outputSummary(std::ostream &os) const = 0;

   UInt64 getInstructionCount() const { return m_instruction_count; }

   void resetElapsedTime() { m_elapsed_time.reset(); }
   SubsecondTime getElapsedTime() const { return m_elapsed_time.getElapsedTime(); }
   SubsecondTime getNonIdleElapsedTime() const { return getElapsedTime() - m_idle_elapsed_time.getElapsedTime(); }

   void countInstructions(IntPtr address, UInt32 count);
   void pushDynamicInstructionInfo(DynamicInstructionInfo &i);
   void popDynamicInstructionInfo();
   DynamicInstructionInfo* getDynamicInstructionInfo(const Instruction &instruction);

   static PerformanceModel *create(Core* core);

   BranchPredictor *getBranchPredictor() { return m_bp; }
   BranchPredictor const* getConstBranchPredictor() const { return m_bp; }

   void disable();
   void enable();
   bool isEnabled() { return m_enabled; }
   void setHold(bool hold) { m_hold = hold; }

protected:
   friend class SpawnInstruction;

   void setElapsedTime(SubsecondTime time);
   void incrementElapsedTime(SubsecondTime time) { m_elapsed_time.addLatency(time); }
   void incrementIdleElapsedTime(SubsecondTime time);

   #ifdef ENABLE_PERF_MODEL_OWN_THREAD
      typedef MTCircularQueue<DynamicInstructionInfo> DynamicInstructionInfoQueue;
      typedef MTCircularQueue<BasicBlock *> BasicBlockQueue;
   #else
      typedef CircularQueue<DynamicInstructionInfo> DynamicInstructionInfoQueue;
      typedef CircularQueue<BasicBlock *> BasicBlockQueue;
   #endif

   Core* getCore() { return m_core; }

private:

   DynamicInstructionInfo* getDynamicInstructionInfo();

   // Simulate a single instruction
   virtual bool handleInstruction(Instruction const* instruction) = 0;

   // When time is jumped ahead outside of control of the performance model (synchronization instructions, etc.)
   // notify it here. This may be used to synchronize internal time or to flush various instruction queues
   virtual void notifyElapsedTimeUpdate() {}

   Core* m_core;

   bool m_enabled;

   bool m_hold;

protected:
   UInt64 m_instruction_count;

   ComponentTime m_elapsed_time;
private:
   ComponentTime m_idle_elapsed_time;

   SubsecondTime m_cpiStartTime;
   // CPI components for Sync and Recv instructions
   SubsecondTime m_cpiSyncFutex;
   SubsecondTime m_cpiSyncPthreadMutex;
   SubsecondTime m_cpiSyncPthreadCond;
   SubsecondTime m_cpiSyncPthreadBarrier;
   SubsecondTime m_cpiSyncJoin;
   SubsecondTime m_cpiSyncDvfsTransition;
   SubsecondTime m_cpiRecv;

   BasicBlockQueue m_basic_block_queue;
   DynamicInstructionInfoQueue m_dynamic_info_queue;

   UInt32 m_current_ins_index;

   BranchPredictor *m_bp;
};

#endif
