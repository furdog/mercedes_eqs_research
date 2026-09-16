#define MEQSCS_IMPL /**< Use the implementation, not only declarations */
#include "mercedes_eqs_current_sensor.h"
#include <assert.h>

void test_meqscs_init(void)
{
	struct meqscs	   sensor;
	struct meqscs_vars vars;

	meqscs_init(&sensor);

	/* Initially, timers are at timeout values, so get_vars should return
	 * false */
	assert(meqscs_get_vars(&sensor, &vars) == false);
	assert(sensor.vars.adc0_r0p0001a == 0);
	assert(sensor.vars.adc1_r0p0001a == 0);
	assert(sensor.vars.adc2_r0p0001a == 0);
}

void test_meqscs_h010_parsing(void)
{
	struct meqscs	    sensor;
	struct meqscs_vars  vars;
	struct meqscs_frame frame;

	meqscs_init(&sensor);

	/* Prepare H010 frame */
	memset(&frame, 0, sizeof(frame));
	frame.id  = MEQSCS_FRAME_H010_ID;
	frame.len = 8;
	/* adc0 bytes at data[2], data[3], data[4] */
	frame.data[2] = 0x34;
	frame.data[3] = 0x12;
	frame.data[4] = 0x00;
	/* adc1 bytes at data[5], data[6], data[7] */
	frame.data[5] = 0x78;
	frame.data[6] = 0x56;
	frame.data[7] = 0x00;

	/* Parse frame 0x010 */
	meqscs_hal_parse_frame(&sensor, &frame);

	assert(sensor.vars.adc0_r0p0001a == 0x00001234);
	assert(sensor.vars.adc1_r0p0001a == 0x00005678);
	assert(sensor.vars.adc2_r0p0001a == 0x00000000);

	/** Not all frames has been parsed yet,
	 *  so get_vars should return false */
	assert(meqscs_get_vars(&sensor, &vars) == false);
	assert(vars.adc0_r0p0001a == 0);
	assert(vars.adc1_r0p0001a == 0);
	assert(vars.adc2_r0p0001a == 0);

	/** All frames has been parsed, so get_vars should return true */
	frame.id = MEQSCS_FRAME_H060_ID;
	meqscs_hal_parse_frame(&sensor, &frame);
	assert(meqscs_get_vars(&sensor, &vars) == true);
	assert(vars.adc0_r0p0001a == 0x00001234);
	assert(vars.adc1_r0p0001a == 0x00005678);
	assert(vars.adc2_r0p0001a == 0x00340000);

	/** After timeout, get_vars should return false */
	meqscs_update(&sensor, MEQSCS_FRAME_H010_TIMEOUT_MS - 1);
	assert(meqscs_get_vars(&sensor, &vars) == true);
	meqscs_update(&sensor, 1);
	assert(meqscs_get_vars(&sensor, &vars) == false);
}

int main(void)
{
	test_meqscs_init();
	test_meqscs_h010_parsing();
	return 0;
}
