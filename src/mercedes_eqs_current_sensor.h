/**
 * @file mercedes_eqs_current_sensor.h
 * @brief Mercedes EQS Current Sensor (meqscs) (Hardware-Agnostic)
 *
 * This file contains the software implementation of the
 * Mercedes EQS Current Sensor logic. Currently it only reads
 * incoming CAN frames from the current sensor (HOST mode), but does not
 * implement current sensor logic itself.
 *
 * The software here is not based on any official documentation or
 * specifications (due to closed-source nature of the device),
 * but is reverse-engineered from the device's behavior.
 *
 * Device information (used in the project):
 * 	Mercedes-Benz
 * 	(HELLA) CSM -> | QR: 178990031012320401196163002000000000
 * 	5DI 014.002-20 |
 *	A 789 900 31 0 |
 *	ZG S002 Q01    | 23204 - 01196
 *	HW:79701_60_00 | SW: 01.04.01
 *
 * Important notes about external behavior:
 * - Device uses CAN2.0B protocol for communication.
 * - Baud rate: 1000 kbps
 *
 * Pinout (left to right):
 * - 1 - GND
 * - 2 - CANH
 * - 3 - CANL
 * - 4 - 12Vin
 * - 5 - 5V(unknown direction)
 * - 6 - ??? floating
 *
 * The design of the library is hardware-agnostic,
 * requiring an external adaptation layer for hardware interaction.
 *
 * ```LICENSE
 * Copyright (c) 2026 furdog <https://github.com/furdog>
 *
 * SPDX-License-Identifier: 0BSD
 * ```
 */

#ifndef MEQSCS_H
#define MEQSCS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/** Defines the maximum frame data length for the current sensor reader */
#define MEQSCS_MAX_FRAME_DATA_LEN 8U

/** Defines the frame ID values for the current sensor reader */
#define MEQSCS_FRAME_H010_ID 0x010U

/** Defines the frame ID values for the current sensor reader */
#define MEQSCS_FRAME_H060_ID 0x060U

/** Timeout values for the H010 frame (in milliseconds) */
#define MEQSCS_FRAME_H010_TIMEOUT_MS 100U

/** Timeout value for the H060 frame (in milliseconds) */
#define MEQSCS_FRAME_H060_TIMEOUT_MS 100U

/** Represents a CAN2.0 frame from the current sensor */
struct meqscs_frame {
	uint32_t id;				  /**< Frame ID */
	uint16_t len;				  /**< Frame length */
	uint8_t	 data[MEQSCS_MAX_FRAME_DATA_LEN]; /**< Frame data */
};

/** Runtime variables of the current sensor reader */
struct meqscs_vars {
	int32_t adc0_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
	int32_t adc1_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
	int32_t adc2_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
};

/** Main instance */
struct meqscs {
	struct meqscs_vars vars; /**< Runtime variables */

	uint32_t h010_timer_r1ms; /**< (Resolution: 1 ms) */
	uint32_t h060_timer_r1ms; /**< (Resolution: 1 ms) */
};

/** Initializes the current sensor instance */
void meqscs_init(struct meqscs *self);

/** Parses a frame from the current sensor. Part of HAL. */
void meqscs_hal_parse_frame(struct meqscs *self, struct meqscs_frame *frame);

/** Puts the current sensor variables into the given struct.
 *  Returns false if the variables are not available. */
bool meqscs_get_vars(struct meqscs *self, struct meqscs_vars *vars);

/** Updates the current sensor instance */
void meqscs_update(struct meqscs *self, uint32_t delta_time_ms);

#ifdef MEQSCS_IMPL
void meqscs_init(struct meqscs *self)
{
	self->vars.adc0_r0p0001a = 0;
	self->vars.adc1_r0p0001a = 0;
	self->vars.adc2_r0p0001a = 0;

	/** Initialize the timer counters to the timeout values */
	self->h010_timer_r1ms = MEQSCS_FRAME_H010_TIMEOUT_MS;
	self->h060_timer_r1ms = MEQSCS_FRAME_H060_TIMEOUT_MS;
}

void meqscs_hal_parse_frame(struct meqscs *self, struct meqscs_frame *frame)
{
	switch (frame->id) {
	/* SG_ crc : 7|8@0+ (1,0) [0|1] "" Current Sensor
	   SG_ counter : 11|4@0+ (1,0) [0|1] "" Current Sensor
	   SG_ adc0 : 16|24@1- (0.0001,0) [0|1] "" Current Sensor
	   SG_ adc1 : 40|24@1- (0.0001,0) [0|1] "" Current Sensor */
	case MEQSCS_FRAME_H010_ID:
		self->vars.adc0_r0p0001a = 0;
		self->vars.adc1_r0p0001a = 0;

		self->vars.adc0_r0p0001a = ((uint32_t)frame->data[2U] << 0U) |
					   ((uint32_t)frame->data[3U] << 8U) |
					   ((uint32_t)frame->data[4U] << 16U);

		self->vars.adc1_r0p0001a = ((uint32_t)frame->data[5U] << 0U) |
					   ((uint32_t)frame->data[6U] << 8U) |
					   ((uint32_t)frame->data[7U] << 16U);

		self->h010_uptime_r1ms = 0;
		break;

	/* SG_ adc2 : 0|25@1- (0.0001,0) [0|1] "" Current_Sensor */
	case MEQSCS_FRAME_H060_ID:
		self->vars.adc2_r0p0001a = 0;

		self->vars.adc2_r0p0001a =
		    ((uint32_t)frame->data[0U] << 0U) |
		    ((uint32_t)frame->data[1U] << 8U) |
		    ((uint32_t)frame->data[2U] << 16U) |
		    ((uint32_t)(frame->data[3U] & 1U) << 24U);

		self->h060_uptime_r1ms = 0;
		break;

	default:
		break;
	}
}

bool meqscs_get_vars(struct meqscs *self, struct meqscs_vars *vars)
{
	bool result = false;

	/* Check if the timer counters have exceeded the timeout values */
	if ((self->h010_timer_r1ms >= MEQSCS_FRAME_H010_TIMEOUT_MS) ||
	    (self->h060_timer_r1ms >= MEQSCS_FRAME_H060_TIMEOUT_MS)) {
		/* No valid data yet, return zeroed vars */
		memset(vars, 0, sizeof(struct meqscs_vars));
		result = false;
	} else {
		*vars  = self->vars;
		result = true;
	}

	return result;
}

void meqscs_update(struct meqscs *self, uint32_t delta_time_ms)
{
	if (self->h010_timer_r1ms < MEQSCS_FRAME_H010_TIMEOUT_MS) {
		self->h010_timer_r1ms += delta_time_ms;
	}

	if (self->h060_timer_r1ms < MEQSCS_FRAME_H060_TIMEOUT_MS) {
		self->h060_timer_r1ms += delta_time_ms;
	}
}

#endif /* MEQSCS_IMPL */
#endif /* MEQSCS_H */
