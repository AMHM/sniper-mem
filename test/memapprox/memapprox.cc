#include "sim_api.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stdio.h>
#include "../../common/core/memory_subsystem/mem_component.h"

#define ToUnsignedInt(X) *((unsigned long long*)(&X))

int main(int argc,char *argv)
{
	int i, arr[100];	
	double ber = 0;

	SimRoiStart();
	add_approx((uint64_t)&arr[0],(uint64_t)&arr[99]);

	ber = 0;
	set_write_er(MemComponent::L1_DCACHE, ToUnsignedInt(ber));
	for (i=0;i<100;i++)
		arr[i] = i;


	// set_read_ber(0.1);

	// for (i = 0; i < 100; i++)
	// 	printf("%d ", arr[i]);

	// set_read_ber(0.0001);
	
	ber = 0.01;
	set_read_er(MemComponent::L1_DCACHE, ToUnsignedInt(ber));
	for (i=0;i<100;i++)
		printf("%d ", arr[i]);
	printf("\n");

	ber = 0;
	set_read_er(3, ToUnsignedInt(ber));
	for (i=0;i<100;i++)
		printf("%d ", arr[i]);
	printf("\n");

	fflush(stdout);

	if (SimInSimulator()) {
		printf("Memory Test: Running in the simulator\n"); fflush(stdout);
	} else {
		printf("Memory approximation Test: Not running in the simulator\n"); fflush(stdout);
	}

	remove_approx((uint64_t)&arr[0],(uint64_t)&arr[99]);

	SimRoiEnd();
}