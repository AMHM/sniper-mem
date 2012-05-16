#ifndef __THREAD_H
#define __THREAD_H

#include "cond.h"
#include "subsecond_time.h"

class Core;
class SyscallMdl;
class SyncClient;
class ClockSkewMinimizationClient;

class Thread
{
   private:
      thread_id_t m_thread_id;
      ConditionVariable m_cond;
      SubsecondTime m_wakeup_time;
      void *m_wakeup_msg;
      Core *m_core;
      SyscallMdl *m_syscall_model;
      SyncClient *m_sync_client;
      ClockSkewMinimizationClient *m_clock_skew_minimization_client;

   public:
      Thread(thread_id_t thread_id);
      ~Thread();

      thread_id_t getId() const { return m_thread_id; }
      SyncClient *getSyncClient() const { return m_sync_client; }
      ClockSkewMinimizationClient* getClockSkewMinimizationClient() const { return m_clock_skew_minimization_client; }

      SubsecondTime wait(Lock &lock)
      {
         m_wakeup_msg = NULL;
         m_cond.wait(lock);
         return m_wakeup_time;
      }
      void signal(SubsecondTime time, void* msg = NULL)
      {
         m_wakeup_time = time;
         m_wakeup_msg = msg;
         m_cond.signal();
      }
      void *getWakeupMsg() const { return m_wakeup_msg; }

      Core* getCore() const { return m_core; }
      void setCore(Core* core) { m_core = core; }

      bool reschedule(SubsecondTime &time, Core *current_core);
      bool updateCoreTLS(int threadIndex = -1);

      SyscallMdl *getSyscallMdl() { return m_syscall_model; }
};

#endif // __THREAD_H
