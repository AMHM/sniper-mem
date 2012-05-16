#ifndef MAGIC_SERVER_H
#define MAGIC_SERVER_H

#include "fixed_types.h"

class MagicServer
{
   public:
      // data type to hold arguments in a HOOK_MAGIC_MARKER callback
      struct MagicMarkerType {
         thread_id_t thread_id;
         core_id_t core_id;
         UInt64 arg0, arg1;
      };

      MagicServer();
      ~MagicServer();

      UInt64 Magic(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1);
      bool inROI(void) const { return m_performance_enabled; }
      static UInt64 getGlobalInstructionCount(void);

      // To be called while holding the thread lock
      UInt64 setFrequency(UInt64 core_number, UInt64 freq_in_mhz);
      UInt64 getFrequency(UInt64 core_number);

   private:
      bool m_performance_enabled;

      void enablePerformance();
      void disablePerformance();
      UInt64 setPerformance(bool enabled);

      UInt64 setInstrumentationMode(UInt64 sim_api_opt);
};

#endif // SYNC_SERVER_H
