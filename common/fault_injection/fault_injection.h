#ifndef __FAULT_INJECTION_H
#define __FAULT_INJECTION_H

#include "fixed_types.h"
#include "core.h"
#include <set>

typedef UInt64 addr_64;
typedef std::pair<addr_64, addr_64> Range;

class FaultInjector;

class FaultinjectionManager
{
   private:
      enum fault_type_t {
         FAULT_TYPE_TOGGLE,
         FAULT_TYPE_SET0,
         FAULT_TYPE_SET1,
      };
      fault_type_t m_type;

      enum fault_injector_t {
         FAULT_INJECTOR_NONE,
         FAULT_INJECTOR_RANDOM,
         FAULT_INJECTOR_RANGE,
      };
      fault_injector_t m_injector;

   public:
      static FaultinjectionManager* create();

      FaultinjectionManager(fault_type_t type, fault_injector_t injector);

      FaultInjector* getFaultInjector(UInt32 core_id, MemComponent::component_t mem_component);

      void applyFault(Core *core, IntPtr read_address, UInt32 data_size, MemoryResult &memres, Byte *data, const Byte *fault);
};

class FaultInjector
{
   protected:
      UInt32 m_core_id;
      MemComponent::component_t m_mem_component;

      struct RangeCompare
      {
            //overlapping ranges are considered equivalent
            bool operator()(const Range& lhv, const Range& rhv) const
            {   
                  return lhv.second < rhv.first;
            } 
      };
      std::set<Range, RangeCompare> approxRanges;
      UInt64 read_bit_eror_rate = 0;
      UInt64 write_bit_eror_rate = 0;
      
   public:
      FaultInjector(UInt32 core_id, MemComponent::component_t mem_component);

      virtual void preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);

      void addApprox(addr_64 start, addr_64 end);
      void removeApprox(addr_64 start, addr_64 end);
      bool in_range(addr_64 start, UInt32 data_length);
      bool InjectFault(Byte* data, UInt32 len, double ber);
      void setReadBitErrorRate(UInt64 rate);
      void setWriteBitErrorRate(UInt64 rate);
};

#endif // __FAULT_INJECTION_H
