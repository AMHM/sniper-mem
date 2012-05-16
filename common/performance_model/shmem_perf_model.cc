#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "simulator.h"
#include "core_manager.h"
#include "fxsupport.h"
#include "subsecond_time.h"

ShmemPerfModel::ShmemPerfModel():
   m_enabled(false),
   m_num_memory_accesses(0),
   m_total_memory_access_latency(SubsecondTime::Zero())
{
   for (UInt32 i = 0; i < NUM_CORE_THREADS; i++)
      m_elapsed_time[i] = SubsecondTime::Zero();
}

ShmemPerfModel::~ShmemPerfModel()
{}

ShmemPerfModel::Thread_t
ShmemPerfModel::getThreadNum(Thread_t thread_num)
{
   if (thread_num != NUM_CORE_THREADS)
   {
      return thread_num;
   }
   if (Sim()->getCoreManager()->amiUserThread()
       || Sim()->getCoreManager()->amiCoreThread())
   {
      return _USER_THREAD;
   }
   else if (Sim()->getCoreManager()->amiSimThread())
   {
      return _SIM_THREAD;
   }
   else
   {
      assert(false);
      return NUM_CORE_THREADS;
   }
}

void
ShmemPerfModel::setElapsedTime(Thread_t thread_num, SubsecondTime time)
{
   LOG_PRINT("setElapsedTime: thread(%u), time(%s)", thread_num, itostr(time).c_str());
   //ScopedLock sl(m_shmem_perf_model_lock);

   assert(thread_num < NUM_CORE_THREADS);
   m_elapsed_time[thread_num] = time;
}

SubsecondTime
ShmemPerfModel::getElapsedTime(Thread_t thread_num)
{
   //ScopedReadLock sl(m_shmem_perf_model_lock);

   return m_elapsed_time[getThreadNum(thread_num)];
}

void
ShmemPerfModel::updateElapsedTime(SubsecondTime time, Thread_t thread_num)
{
   LOG_PRINT("updateElapsedTime: time(%s)", itostr(time).c_str());
   //ScopedLock sl(m_shmem_perf_model_lock);

   thread_num = getThreadNum(thread_num);
   if (m_elapsed_time[thread_num] < time)
      m_elapsed_time[thread_num] = time;
}

void
ShmemPerfModel::incrElapsedTime(SubsecondTime time, Thread_t thread_num)
{
   LOG_PRINT("incrElapsedTime: time(%s)", itostr(time).c_str());
   //ScopedLock sl(m_shmem_perf_model_lock);

   thread_num = getThreadNum(thread_num);

   SubsecondTime i_elapsed_time = m_elapsed_time[thread_num];
   SubsecondTime t_elapsed_time = i_elapsed_time + time;

   LOG_ASSERT_ERROR(t_elapsed_time >= i_elapsed_time,
         "t_elapsed_time(%s) < i_elapsed_time(%s)",
         itostr(t_elapsed_time).c_str(),
         itostr(i_elapsed_time).c_str());

   atomic_add_subsecondtime(m_elapsed_time[thread_num], time);
}

void
ShmemPerfModel::incrTotalMemoryAccessLatency(SubsecondTime shmem_time)
{
   if (m_enabled)
   {
      //ScopedLock sl(m_shmem_perf_model_lock);

      __sync_fetch_and_add(&m_num_memory_accesses, 1);
      atomic_add_subsecondtime(m_total_memory_access_latency, shmem_time);
   }
}

void
ShmemPerfModel::outputSummary(std::ostream& out)
{
   out << "Shmem Perf Model summary: " << std::endl;
   out << "    num memory accesses: " << m_num_memory_accesses << std::endl;
   out << "    average memory access latency: " << "INVALID " <<
      (float) (m_total_memory_access_latency.getInternalDataForced()) / m_num_memory_accesses << std::endl;
}
