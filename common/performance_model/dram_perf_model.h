#ifndef __DRAM_PERF_MODEL_H__
#define __DRAM_PERF_MODEL_H__

#include "queue_model.h"
#include "fixed_types.h"
#include "moving_average.h"
#include "subsecond_time.h"

// Note: Each Dram Controller owns a single DramModel object
// Hence, m_dram_bandwidth is the bandwidth for a single DRAM controller
// Total Bandwidth = m_dram_bandwidth * Number of DRAM controllers
// Number of DRAM controllers presently = Number of Cores
// m_dram_bandwidth is expressed in GB/s
// Assuming the frequency of a core is 1GHz,
// m_dram_bandwidth is also expressed in 'Bytes per clock cycle'
// This DRAM model is not entirely correct.
// It sort of increases the queueing delay to a huge value if
// the arrival times of adjacent packets are spread over a large
// simulated time period
class DramPerfModel
{
   private:
      QueueModel* m_queue_model;
      SubsecondTime m_dram_access_cost;
      ComponentBandwidth m_dram_bandwidth;

      bool m_enabled;

      UInt64 m_num_accesses;
      SubsecondTime m_total_access_latency;
      SubsecondTime m_total_queueing_delay;

   public:
      DramPerfModel(core_id_t core_id,
            SubsecondTime dram_access_cost,
            ComponentBandwidth dram_bandwidth,
            bool queue_model_enabled,
            String queue_model_type,
            UInt32 cache_block_size);

      ~DramPerfModel();

      SubsecondTime getAccessLatency(SubsecondTime pkt_time, UInt64 pkt_size, core_id_t requester);
      void enable() { m_enabled = true; }
      void disable() { m_enabled = false; }

      UInt64 getTotalAccesses() { return m_num_accesses; }
      void outputSummary(std::ostream& out);

      static void dummyOutputSummary(std::ostream& out);
};

#endif /* __DRAM_PERF_MODEL_H__ */
