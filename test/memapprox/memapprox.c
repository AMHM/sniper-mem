#include "sim_api.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stdio.h>

main(argc, argv)

int argc;
char *argv;
{
	int i, arr[100];

	SimRoiStart();
	add_approx((uint64_t)&arr[0],(uint64_t)&arr[99]);

	for (i=0;i<100;i++)
		arr[i] = 5;
	arr[1] = 16000000;
	arr[2] = 16000000;

	fflush(stdout);

	// set_read_ber(0.1);

	// for (i = 0; i < 100; i++)
	// 	printf("%d ", arr[i]);

	// set_read_ber(0.0001);

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