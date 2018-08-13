#include "fault_injector_range.h"
#include "rng.h"
#include "simulator.h"
#include "config.hpp"

FaultInjectorRange::FaultInjectorRange(UInt32 core_id, MemComponent::component_t mem_component)
   : FaultInjector(core_id, mem_component)
{
    srand(time(0));
    String affected_memory = Sim()->getCfg()->getString("fault_injection/affected");

   if (affected_memory.find(MemComponentString(mem_component)) != std::string::npos)
   {
       printf("[FI] Created for %s on core : %d \n", MemComponentString(mem_component), core_id);
       m_active = true;
   }
   else
   {
       m_active = false;
   } 
}

void 
FaultInjectorRange::inject_range_fault(IntPtr addr, UInt32 data_size, Byte *fault, double ber)
{
    assert(fault!=NULL);

    if (m_active && ber)
    {
         for (UInt32 bit_location = 0; bit_location < data_size * 8; bit_location++) {
            double random_probability = ((double) rand() / (RAND_MAX));

            // printf("[FI] rand: %lf, ber: %lf \n",random_probability, ber);
            if(random_probability < ber) {
                //  printf("[FI] Inserting bit %d flip at address %" PRIxPTR " on access by core %d to component %s, currently held at %" PRIxPTR "\n",
                //     bit_location, addr, m_core_id, MemComponentString(m_mem_component), fault);

                fault[bit_location / 8] ^= (1 << (bit_location % 8));
            }
        }
    }
}

void
FaultInjectorRange::preRead(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time)
{
   // Data at virtual address <addr> has just been read with size <data_size>.
   // <location> corresponds to the physical location (cache line) where the data lives.
   // Update <fault> here according to errors that have accumulated in this memory location.

    inject_range_fault(addr, data_size, fault, read_bit_eror_rate);
}

void
FaultInjectorRange::postWrite(IntPtr addr, IntPtr location, UInt32 data_size, Byte *fault, SubsecondTime time)
{
   // Data at virtual address <addr> has just been written to.
   // Update <fault> here according to errors that occured during the writing of this memory location.

    inject_range_fault(addr, data_size, fault, write_bit_eror_rate);
}
