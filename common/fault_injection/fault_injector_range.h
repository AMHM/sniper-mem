#ifndef __FAULT_INJECTOR_RANGE_H
#define __FAULT_INJECTOR_RANGE_H

#include "fault_injection.h"

class FaultInjectorRange : public FaultInjector
{
    public:
      FaultInjectorRange(UInt32 core_id, MemComponent::component_t mem_component);

      virtual void preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      
    protected:
      bool m_active;
      virtual bool inject_range_fault(IntPtr addr, UInt32 data_size, Byte *fault, double rate) __attribute__((optimize(0)));
      UInt64 total_read;
      UInt64 faulty_read;
      UInt64 total_write;
      UInt64 faulty_write;
};

#endif // __FAULT_INJECTOR_RANGE_H
