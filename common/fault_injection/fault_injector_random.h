#ifndef __FAULT_INJECTOR_RANDOM_H
#define __FAULT_INJECTOR_RANDOM_H

#include "fault_injection.h"

class FaultInjectorRandom : public FaultInjector
{
   public:
      FaultInjectorRandom(UInt32 core_id, MemComponent::component_t mem_component);

      virtual void preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
      virtual void postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time);
   
   protected:
      bool m_active;
      UInt64 m_rng;
      virtual void inject_random_fault(IntPtr addr, UInt32 data_size, Byte *fault);
};

#endif // __FAULT_INJECTION_RANDOM_H
