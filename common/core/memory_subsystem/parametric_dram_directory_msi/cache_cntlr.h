#pragma once

#include "core.h"
#include "cache.h"
#include "prefetcher.h"
#include "shared_cache_block_info.h"
#include "address_home_lookup.h"
#include "../pr_l1_pr_l2_dram_directory_msi/shmem_msg.h"
#include "../pr_l1_pr_l2_dram_directory_msi/dram_cntlr.h"
#include "mem_component.h"
#include "semaphore.h"
#include "lock.h"
#include "setlock.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "contention_model.h"
#include "req_queue_list_template.h"
#include "stats.h"
#include "subsecond_time.h"

/* Enable to get a detailed count of state transitions */
//#define ENABLE_TRANSITIONS

/* Enable to track latency by HitWhere */
//#define TRACK_LATENCY_BY_HITWHERE

// Forward declarations
namespace ParametricDramDirectoryMSI
{
   class CacheCntlr;
   class MemoryManager;
}

namespace ParametricDramDirectoryMSI
{
   class Transition
   {
      public:
         enum reason_t
         {
            REASON_FIRST = 0,
            CORE_RD = REASON_FIRST,
            CORE_WR,
            CORE_RDEX,
            UPGRADE,
            EVICT,
            EVICT_LOWER,
            COHERENCY,
            NUM_REASONS
         };
   };

   class Prefetch
   {
      public:
         enum prefetch_type_t
         {
            NONE,    // Not a prefetch
            OWN,     // Prefetch initiated locally
            OTHER    // Prefetch initiated by a higher-level cache
         };
   };

   class CacheParameters
   {
      public:
         String configName;
         UInt32 size;
         UInt32 associativity;
         String replacement_policy;
         bool perfect;
         ComponentLatency data_access_time;
         ComponentLatency tags_access_time;
         ComponentLatency writeback_time;
         ComponentBandwidthPerCycle next_level_read_bandwidth;
         String perf_model_type;
         bool writethrough;
         UInt32 shared_cores;
         bool prefetcher;
         UInt32 outstanding_misses;

         CacheParameters()
            : data_access_time(NULL,0)
            , tags_access_time(NULL,0)
            , writeback_time(NULL,0)
         {}
         CacheParameters(String _configName, UInt32 _size, UInt32 _associativity, String _replacement_policy, bool _perfect,
            const ComponentLatency& _data_access_time, const ComponentLatency& _tags_access_time,
            const ComponentLatency& _writeback_time, const ComponentBandwidthPerCycle& _next_level_read_bandwidth,
            String _perf_model_type, bool _writethrough, UInt32 _shared_cores, bool _prefetcher, UInt32 _outstanding_misses) :
            configName(_configName), size(_size), associativity(_associativity), replacement_policy(_replacement_policy), perfect(_perfect),
            data_access_time(_data_access_time), tags_access_time(_tags_access_time),
            writeback_time(_writeback_time), next_level_read_bandwidth(_next_level_read_bandwidth),
            perf_model_type(_perf_model_type), writethrough(_writethrough), shared_cores(_shared_cores),
            prefetcher(_prefetcher), outstanding_misses(_outstanding_misses)
         {}
   };

   class CacheCntlrList : public std::vector<CacheCntlr*>
   {
      public:
         PrevCacheIndex find(core_id_t core_id, MemComponent::component_t mem_component);
   };

   class CacheDirectoryWaiter
   {
      public:
         bool exclusive;
         bool isPrefetch;
         CacheCntlr* cache_cntlr;
         SubsecondTime t_issue;
         CacheDirectoryWaiter(bool _exclusive, bool _isPrefetch, CacheCntlr* _cache_cntlr, SubsecondTime _t_issue) :
            exclusive(_exclusive), isPrefetch(_isPrefetch), cache_cntlr(_cache_cntlr), t_issue(_t_issue)
         {}
   };

   typedef ReqQueueListTemplate<CacheDirectoryWaiter> CacheDirectoryWaiterMap;

   struct MshrEntry {
      SubsecondTime t_issue, t_complete;
   };
   typedef std::unordered_map<IntPtr, MshrEntry> Mshr;

   class CacheMasterCntlr
   {
      private:
         Cache* m_cache;
         Lock m_cache_lock;
         Lock m_smt_lock; //< Only used in L1 cache, to protect against concurrent access from sibling SMT threads
         CacheCntlrList m_prev_cache_cntlrs;
         Prefetcher* m_prefetcher;
         PrL1PrL2DramDirectoryMSI::DramCntlr* m_dram_cntlr;

         Mshr mshr;
         ContentionModel m_l1_mshr;
         ContentionModel m_next_level_read_bandwidth;
         CacheDirectoryWaiterMap m_directory_waiters;
         IntPtr m_evicting_address;
         Byte* m_evicting_buf;

         std::vector<SetLock> m_setlocks;
         UInt32 m_log_blocksize;
         UInt32 m_num_sets;

         void createSetLocks(UInt32 cache_block_size, UInt32 num_sets, UInt32 core_offset, UInt32 num_cores);
         SetLock* getSetLock(IntPtr addr);

         CacheMasterCntlr(String name, core_id_t core_id, UInt32 outstanding_misses)
            : m_cache(NULL)
            , m_prefetcher(NULL)
            , m_dram_cntlr(NULL)
            , m_l1_mshr(name + ".mshr", core_id, outstanding_misses)
            , m_next_level_read_bandwidth(name + ".next_read", core_id)
         {}
         ~CacheMasterCntlr();

         friend class CacheCntlr;
   };

   class CacheCntlr
   {
      private:
         // Data Members
         bool m_enabled;
         MemComponent::component_t m_mem_component;
         MemoryManager* m_memory_manager;
         CacheMasterCntlr* m_master;
         CacheCntlr* m_next_cache_cntlr;
         CacheCntlr* m_last_level;
         AddressHomeLookup* m_dram_directory_home_lookup;
         std::unordered_map<IntPtr, MemComponent::component_t> m_shmem_req_source_map;
         bool m_perfect;
         bool m_l1_mshr;
         UInt32 m_num_prefetches;

         struct {
           UInt64 loads, stores;
           UInt64 load_misses, store_misses;
           UInt64 load_overlapping_misses, store_overlapping_misses;
           UInt64 loads_state[CacheState::NUM_CSTATE_STATES], stores_state[CacheState::NUM_CSTATE_STATES];
           UInt64 loads_where[HitWhere::NUM_HITWHERES], stores_where[HitWhere::NUM_HITWHERES];
           UInt64 load_misses_state[CacheState::NUM_CSTATE_STATES], store_misses_state[CacheState::NUM_CSTATE_STATES];
           UInt64 loads_prefetch, stores_prefetch;
           UInt64 hits_prefetch, // lines which were prefetched and subsequently used by a non-prefetch access
                  evict_prefetch; // lines which were prefetched and evicted before being used
                  // Note: hits_prefetch and evict_prefetch will not account for all prefetched lines,
                  // some may still be in the cache, or have been removed for other reasons (invalidate)
                  // Also, in a shared cache, the prefetch may have been triggered by another core than the one
                  // accessing/evicting the line so *_prefetch statistics should be summed across the shared cache
           UInt64 evict_shared, evict_modified;
           SubsecondTime total_latency;
           SubsecondTime snoop_latency;
           SubsecondTime mshr_latency;
           UInt64 prefetches;
           #ifdef ENABLE_TRANSITIONS
           UInt64 transitions[CacheState::NUM_CSTATE_STATES][CacheState::NUM_CSTATE_STATES];
           UInt64 transition_reasons[Transition::NUM_REASONS][CacheState::NUM_CSTATE_SPECIAL_STATES][CacheState::NUM_CSTATE_SPECIAL_STATES];
           std::unordered_map<IntPtr, Transition::reason_t> seen;
           #endif
         } stats;
         #ifdef TRACK_LATENCY_BY_HITWHERE
         std::unordered_map<HitWhere::where_t, StatHist> lat_by_where;
         #endif

         void updateCounters(Core::mem_op_t mem_op_type, IntPtr address, bool cache_hit, CacheState::cstate_t state, Prefetch::prefetch_type_t isPrefetch);
         void cleanupMshr();
         void transition(IntPtr address, Transition::reason_t reason, CacheState::cstate_t old_state, CacheState::cstate_t new_state);

         core_id_t m_core_id;
         UInt32 m_cache_block_size;
         bool m_cache_writethrough;
         ComponentLatency m_writeback_time;
         ComponentBandwidthPerCycle m_next_level_read_bandwidth;

         UInt32 m_shared_cores;        /**< Number of cores this cache is shared with */
         core_id_t m_core_id_master;   /**< Core id of the 'master' (actual) cache controller we're proxying */

         Semaphore* m_user_thread_sem;
         Semaphore* m_network_thread_sem;
         volatile HitWhere::where_t m_last_remote_hit_where;

         ShmemPerfModel* m_shmem_perf_model;

         // Core-interfacing stuff
         void accessCache(
               Core::mem_op_t mem_op_type,
               IntPtr ca_address, UInt32 offset,
               Byte* data_buf, UInt32 data_length);
         bool operationPermissibleinCache(
               IntPtr address, Core::mem_op_t mem_op_type);

         void copyDataFromNextLevel(Core::mem_op_t mem_op_type, IntPtr address, bool modeled, SubsecondTime t_start);
         void Prefetch(Core::mem_op_t mem_op_type, IntPtr address, SubsecondTime t_start);
         void doPrefetch(IntPtr prefetch_address, SubsecondTime t_start);

         // Cache meta-data operations
         SharedCacheBlockInfo* getCacheBlockInfo(IntPtr address);
         CacheState::cstate_t getCacheState(IntPtr address);
         SharedCacheBlockInfo* setCacheState(IntPtr address, CacheState::cstate_t cstate);

         // Cache data operations
         void invalidateCacheBlock(IntPtr address);
         void retrieveCacheBlock(IntPtr address, Byte* data_buf, ShmemPerfModel::Thread_t thread_num);


         SharedCacheBlockInfo* insertCacheBlock(IntPtr address, CacheState::cstate_t cstate, Byte* data_buf, ShmemPerfModel::Thread_t thread_num);
         std::pair<SubsecondTime, bool> updateCacheBlock(IntPtr address, CacheState::cstate_t cstate, Transition::reason_t reason, Byte* out_buf, ShmemPerfModel::Thread_t thread_num);
         void writeCacheBlock(IntPtr address, UInt32 offset, Byte* data_buf, UInt32 data_length, ShmemPerfModel::Thread_t thread_num);

         // Handle Request from previous level cache
         HitWhere::where_t processShmemReqFromPrevCache(CacheCntlr* requester, Core::mem_op_t mem_op_type, IntPtr address, bool modeled, Prefetch::prefetch_type_t isPrefetch, SubsecondTime t_issue, bool have_write_lock);

         // Process Request from L1 Cache
         void accessDRAM(Core::mem_op_t mem_op_type, IntPtr address, bool isPrefetch, Byte* data_buf);
         void initiateDirectoryAccess(Core::mem_op_t mem_op_type, IntPtr address, bool isPrefetch, SubsecondTime t_issue);
         void processExReqToDirectory(IntPtr address);
         void processShReqToDirectory(IntPtr address);

         // Process Request from Dram Dir
         void processExRepFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
         void processShRepFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
         void processInvReqFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
         void processFlushReqFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
         void processWbReqFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);

         // Cache Block Size
         UInt32 getCacheBlockSize() { return m_cache_block_size; }
         MemoryManager* getMemoryManager() { return m_memory_manager; }
         ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

         // Wake up User Thread
         void wakeUpUserThread(Semaphore* user_thread_sem = NULL);
         // Wait for User Thread
         void waitForUserThread(Semaphore* network_thread_sem = NULL);

         // Wait for Network Thread
         void waitForNetworkThread(void);
         // Wake up Network Thread
         void wakeUpNetworkThread(void);

         Semaphore* getUserThreadSemaphore(void);
         Semaphore* getNetworkThreadSemaphore(void);

         // Dram Directory Home Lookup
         core_id_t getHome(IntPtr address) { return m_dram_directory_home_lookup->getHome(address); }

         CacheCntlr* lastLevelCache(void);

      public:

         CacheCntlr(MemComponent::component_t mem_component,
               String name,
               core_id_t core_id,
               MemoryManager* memory_manager,
               AddressHomeLookup* dram_directory_home_lookup,
               Semaphore* user_thread_sem,
               Semaphore* network_thread_sem,
               UInt32 cache_block_size,
               CacheParameters & cache_params,
               ShmemPerfModel* shmem_perf_model);

         ~CacheCntlr();

         Cache* getCache() { return m_master->m_cache; }
         Lock& getLock() { return m_master->m_cache_lock; }

         void setPrevCacheCntlrs(CacheCntlrList& prev_cache_cntlrs);
         void setNextCacheCntlr(CacheCntlr* next_cache_cntlr) { m_next_cache_cntlr = next_cache_cntlr; }
         void createSetLocks(UInt32 cache_block_size, UInt32 num_sets, UInt32 core_offset, UInt32 num_cores) { m_master->createSetLocks(cache_block_size, num_sets, core_offset, num_cores); }
         void setDRAMDirectAccess(PrL1PrL2DramDirectoryMSI::DramCntlr* dram_cntlr) { m_master->m_dram_cntlr = dram_cntlr; }

         HitWhere::where_t processMemOpFromCore(
               Core::lock_signal_t lock_signal,
               Core::mem_op_t mem_op_type,
               IntPtr ca_address, UInt32 offset,
               Byte* data_buf, UInt32 data_length,
               bool modeled);
         void updateHits(Core::mem_op_t mem_op_type, UInt64 hits);

         // Notify next level cache of so it can update its sharing set
         void notifyPrevLevelInsert(core_id_t core_id, MemComponent::component_t mem_component, IntPtr address);
         void notifyPrevLevelEvict(core_id_t core_id, MemComponent::component_t mem_component, IntPtr address);

         // Handle message from Dram Dir
         void handleMsgFromDramDirectory(core_id_t sender, PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg);
         // Acquiring and Releasing per-set Locks
         void acquireLock(UInt64 address);
         void releaseLock(UInt64 address);
         void acquireStackLock(UInt64 address, bool this_is_locked = false);
         void releaseStackLock(UInt64 address, bool this_is_locked = false);

         bool isMasterCache(void) { return m_core_id == m_core_id_master; }
         bool isFirstLevel(void) { return m_master->m_prev_cache_cntlrs.empty(); }
         bool isLastLevel(void) { return ! m_next_cache_cntlr; }

         void enable() { m_enabled = true; m_master->m_cache->enable(); }
         void disable() { m_enabled = false; m_master->m_cache->disable(); }

         void outputSummary(std::ostream& out);

         friend class CacheCntlrList;
         friend class MemoryManager;
   };

}
