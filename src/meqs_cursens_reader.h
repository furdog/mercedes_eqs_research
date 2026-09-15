/**
 * @file meqs_cursens_reader.h
 * @brief Mercedes EQS Current Sensor reader (Hardware-Agnostic)
 *
 * This file contains the software implementation of the
 * Mercedes EQS Current Sensor reader logic.
 * The design is hardware-agnostic, requiring an external adaptation layer
 * for hardware interaction.
 *
 * ```LICENSE
 * Copyright (c) 2026 furdog <https://github.com/furdog>
 *
 * SPDX-License-Identifier: 0BSD
 * ```
 */

#ifndef MEQS_CURSENS_READER_H
#define MEQS_CURSENS_READER_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define MEQS_CURSENS_READER_MAX_FRAME_DATA_LEN 8U
#define MEQS_CURSENS_READER_FRAME_H010 0x010U
#define MEQS_CURSENS_READER_FRAME_H060 0x060U

/* Represents a CAN2.0 frame from the current sensor */
struct meqs_cursens_reader_frame {
	uint32_t id;
	uint16_t len;
	uint8_t	 data[MEQS_CURSENS_READER_MAX_FRAME_DATA_LEN];
};

/* Main instance */
struct meqs_cursens_reader {
	int32_t adc0_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
	int32_t adc1_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
	int32_t adc2_r0p0001a; /**< (Resolution: 0.0001 Ampere per bit) */
};

void meqs_cursens_reader_init(struct meqs_cursens_reader *self);

/** Frame from sensor must go here */
void meqs_cursens_reader_push_input_frame(
    struct meqs_cursens_reader *self, struct meqs_cursens_reader_frame *frame);

#ifdef MEQS_CURSENS_READER_IMPL
void meqs_cursens_reader_init(struct meqs_cursens_reader *self)
{
	self->adc0_r0p0001a = 0;
	self->adc1_r0p0001a = 0;
	self->adc2_r0p0001a = 0;
}

void meqs_cursens_reader_push_input_frame(
    struct meqs_cursens_reader *self, struct meqs_cursens_reader_frame *frame)
{
	switch (frame->id) {
	/* SG_ crc : 7|8@0+ (1,0) [0|1] "" Current Sensor
	   SG_ counter : 11|4@0+ (1,0) [0|1] "" Current Sensor
	   SG_ adc0 : 16|24@1- (0.0001,0) [0|1] "" Current Sensor
	   SG_ adc1 : 40|24@1- (0.0001,0) [0|1] "" Current Sensor */
	case MEQS_CURSENS_READER_FRAME_H010:
		self->adc0_r0p0001a = 0;
		self->adc1_r0p0001a = 0;

		self->adc0_r0p0001a = ((uint32_t)frame->data[2U] << 0U) |
				      ((uint32_t)frame->data[3U] << 8U) |
				      ((uint32_t)frame->data[4U] << 16U);

		self->adc1_r0p0001a = ((uint32_t)frame->data[5U] << 0U) |
				      ((uint32_t)frame->data[6U] << 8U) |
				      ((uint32_t)frame->data[7U] << 16U);
		break;

	/* SG_ adc2 : 0|25@1- (0.0001,0) [0|1] "" Current_Sensor */
	case MEQS_CURSENS_READER_FRAME_H060:
		self->adc2_r0p0001a = 0;

		self->adc2_r0p0001a =
		    ((uint32_t)frame->data[0U] << 0U) |
		    ((uint32_t)frame->data[1U] << 8U) |
		    ((uint32_t)frame->data[2U] << 16U) |
		    ((uint32_t)(frame->data[3U] & 1U) << 24U);
		break;

	default:
		break;
	}
}

#endif /* MEQS_CURSENS_READER_IMPL */
#endif /* MEQS_CURSENS_READER_H */
