#include <math.h>

#define UPPER_BOUND 1.1 
#define LOWER_BOUND 0.9

#define RESOLUTION 1E-6

double manual_calibrations(double current_knob, double current_error, double target_error)
{
	double next_knob = current_knob;

	if (current_error > UPPER_BOUND * target_error) {
		//increase accuracy

		int multiplier = target_error > 0 ? log(current_error / target_error) + 1 : 1;
		// printf("%d\n", multiplier);
		double step = RESOLUTION * multiplier;
		next_knob = next_knob - step;
	}

	else if (current_error < LOWER_BOUND * target_error) {
		//decrease accuracy

		int multiplier = current_error > 0 ? log(target_error /current_error) + 1 : 1;
		// printf("%d\n", multiplier);
		double step = RESOLUTION * multiplier;
		next_knob = next_knob + step;
	}

	return next_knob;
}