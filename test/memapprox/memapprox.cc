#include "sim_api.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stdio.h>
#include "../../common/core/memory_subsystem/mem_component.h"

#define ToUnsignedInt(X) *((unsigned long long*)(&X))
#define ARRAY_SIZE 100
int main(int argc,char **argv)
{
	int i, arr[ARRAY_SIZE];	
	double ber = 0;

	SimRoiStart();
	add_approx((uint64_t)&arr[0],(uint64_t)&arr[ARRAY_SIZE-1]);

	ber = 0;
	set_write_er(MemComponent::DRAM, ToUnsignedInt(ber));
	for (i=0;i<ARRAY_SIZE;i++)
		arr[i] = i;

	arr[5]=879767;

	// set_read_ber(0.1);

	// for (i = 0; i < ARRAY_SIZE; i++)
	// 	printf("%d ", arr[i]);

	// set_read_ber(0.0001);
	
	ber = 1;
	set_read_er(MemComponent::DRAM, ToUnsignedInt(ber));
	for (i=0;i<ARRAY_SIZE;i++)
		printf("%d ", arr[i]);
	printf("\n");

	ber = 0;
	set_read_er(MemComponent::DRAM, ToUnsignedInt(ber));
	for (i=0;i<ARRAY_SIZE;i++)
		printf("%d ", arr[i]);
	printf("\n");

	fflush(stdout);

	if (SimInSimulator()) {
		printf("Memory Test: Running in the simulator\n"); fflush(stdout);
	} else {
		printf("Memory approximation Test: Not running in the simulator\n"); fflush(stdout);
	}

	remove_approx((uint64_t)&arr[0],(uint64_t)&arr[ARRAY_SIZE-1]);

	SimRoiEnd();
}