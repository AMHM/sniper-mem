#ifndef __FAULT_INJECTOR_RANGE_H
#define __FAULT_INJECTOR_RANGE_H

#include "fault_injector_random.h"

class FaultInjectorRange : public FaultInjectorRandom
{
    public:
      FaultInjectorRange(UInt32 core_id, MemComponent::component_t mem_component);
      virtual void preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      
    protected:
      virtual void inject_random_fault(IntPtr addr, UInt32 data_size, Byte *fault, UInt64 rate);
};

#endif // __FAULT_INJECTOR_RANGE_H
