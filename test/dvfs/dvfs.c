#include "sim_api.h"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stdio.h>

int value;

int loop() {

	int i;
	for (i = 0 ; i < 10000; i++) {
		value += i;
	}
	return value;
}

int main() {

	SimRoiStart();

	value = 0;
	loop();

	unsigned long freq = SimGetOwnFreqMHz();
	printf("Current Freq = %lu MHz\n", freq);

	freq = (1<<1) | (1<<1);
	printf("Setting frequency to %lu MHz\n", freq);
	SimSetOwnFreqMHz(freq);

	value = 0;
	loop();

	freq = (1<<1);
	SimSetOwnFreqMHz(freq);
	printf("Setting frequency to %lu MHz\n", freq);

	value = 0;
	loop();

	SimRoiEnd();
}
