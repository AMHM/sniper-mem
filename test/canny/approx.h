#ifndef _sniper_approx_h
#define _sniper_approx_h
#include <stdio.h>
#include "sim_api.h"
#include "../../common/core/memory_subsystem/mem_component.h"

FILE* debug;

double __attribute__((optimize("O0"))) set_read_ber(double read_ber) 
{
    unsigned int errorProbablity = 1/read_ber;
    set_read_er(MemComponent::L1_DCACHE, errorProbablity);
}

double __attribute__((optimize("O0"))) set_write_ber(double write_ber) 
{
    unsigned int errorProbablity = 1/write_ber;
    set_write_er(MemComponent::L1_DCACHE, errorProbablity);
}

double __attribute__((optimize("O0"))) get_read_ber(double *read_ber) 
{
}

double __attribute__((optimize("O0"))) get_write_ber(double *write_ber) 
{
}

#define APPROX_on

#define heap_array_image
// #define heap_array_nms
// #define heap_array_edge
// #define heap_array_dirim
// #define heap_array_magnitude
// #define heap_array_delta_x
// #define heap_array_delta_y
// #define heap_array_tempim
// #define heap_array_smoothedim
// #define heap_array_kernel
#endif