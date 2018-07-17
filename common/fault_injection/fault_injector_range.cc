#include "fault_injector_range.h"
#include "rng.h"

FaultInjectorRange::FaultInjectorRange(UInt32 core_id, MemComponent::component_t mem_component)
   : FaultInjectorRandom(core_id, mem_component)
{
    srand(time(0));
    printf("[FI] Created for %s on core : %d \n", MemComponentString(mem_component), core_id);
}

void 
FaultInjectorRange::inject_random_fault(IntPtr addr, UInt32 data_size, Byte *fault, UInt64 rate)
{
   // Dummy random fault injector
   if (m_active && rate)
   {
      for (UInt32 bit_location = 0; bit_location < data_size * 8; bit_location++) {
            UInt64 random_probability = rand();
            if (random_probability % rate == 0)
            {
                // printf("[FI] Inserting bit %d flip at address %" PRIxPTR " on access by core %d to component %s\n",
                //     bit_location, addr, m_core_id, MemComponentString(m_mem_component));

                fault[bit_location / 8] |= 1 << (bit_location % 8);
            }
      }
   }
}

// preRead changes the Cache permanently
void
FaultInjectorRange::preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time)
{
   // Data at virtual address <addr> has just been read with size <data_size>.
   // <location> corresponds to the physical location (cache line) where the data lives.
   // Update <fault> here according to errors that have accumulated in this memory location.
}

// postRead changes the memory bus
void
FaultInjectorRange::postRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time)
{
   // Data at virtual address <addr> has just been read with size <data_size>.
   // <location> corresponds to the physical location (cache line) where the data lives.
   // Update <fault> here according to errors that have accumulated in this memory location.
 
    if (in_range(addr, data_size))
    {
        inject_random_fault(addr, data_size, fault, read_bit_eror_rate);
    }
}

void
FaultInjectorRange::postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time)
{
   // Data at virtual address <addr> has just been written to.
   // Update <fault> here according to errors that occured during the writing of this memory location.

   if (in_range(addr, data_size))
    {
        inject_random_fault(addr, data_size, fault, write_bit_eror_rate);
    }
}
