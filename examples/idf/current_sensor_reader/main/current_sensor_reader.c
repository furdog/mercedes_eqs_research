/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define MEQSCS_IMPL
#include "mercedes_eqs_current_sensor.h"

#define TWAI_LISTENER_TX_GPIO -1 // Listen only node doesn't need TX pin
#define TWAI_LISTENER_RX_GPIO 22 // Default RX pin
#define TWAI_BITRATE 1000000

// Buffer for burst data handling
#define POLL_DEPTH 200

static const char *TAG = "current_sensor_reader";

/******************************************************************************
 * DELTA TIME
 *****************************************************************************/
struct delta_time {
	uint32_t timestamp_prev;
};

static void delta_time_init(struct delta_time *self)
{
	self->timestamp_prev = 0;
}

static uint32_t delta_time_update_ms(struct delta_time *self,
				     uint32_t		timestamp)
{
	uint32_t delta_time_ms = 0;

	delta_time_ms	     = timestamp - self->timestamp_prev;
	self->timestamp_prev = timestamp;

	return delta_time_ms;
}

typedef struct {
	twai_frame_t frame;
	uint8_t	     data[TWAI_FRAME_MAX_LEN];
} twai_listener_data_t;

typedef struct {
	twai_node_handle_t    node_hdl;
	twai_listener_data_t *rx_pool;
	SemaphoreHandle_t     free_pool_semaphore;
	SemaphoreHandle_t     rx_result_semaphore;
	int		      write_idx;
	int		      read_idx;
	struct meqscs	      meqscs;
	struct delta_time     dt;
} twai_listener_ctx_t;

// Error callback
static bool IRAM_ATTR twai_listener_on_error_callback(
    twai_node_handle_t handle, const twai_error_event_data_t *edata,
    void *user_ctx)
{
	ESP_EARLY_LOGW(TAG, "bus error: 0x%x", edata->err_flags.val);
	return false;
}

// Node state
static bool IRAM_ATTR twai_listener_on_state_change_callback(
    twai_node_handle_t handle, const twai_state_change_event_data_t *edata,
    void *user_ctx)
{
	const char *twai_state_name[] = {"error_active", "error_warning",
					 "error_passive", "bus_off"};
	ESP_EARLY_LOGI(TAG, "state changed: %s -> %s",
		       twai_state_name[edata->old_sta],
		       twai_state_name[edata->new_sta]);
	return false;
}

// TWAI receive callback - store data and signal
static bool IRAM_ATTR twai_listener_rx_callback(
    twai_node_handle_t handle, const twai_rx_done_event_data_t *edata,
    void *user_ctx)
{
	BaseType_t	     woken = pdFALSE;
	twai_listener_ctx_t *ctx   = (twai_listener_ctx_t *)user_ctx;

	if (xSemaphoreTakeFromISR(ctx->free_pool_semaphore, &woken) !=
	    pdTRUE) {
		return (woken == pdTRUE);
	}
	if (twai_node_receive_from_isr(
		handle, &ctx->rx_pool[ctx->write_idx].frame) == ESP_OK) {
		ctx->write_idx = (ctx->write_idx + 1) % POLL_DEPTH;
		xSemaphoreGiveFromISR(ctx->rx_result_semaphore, &woken);
	}
	return (woken == pdTRUE);
}

void app_main(void)
{
	printf("===================Mercedes EQS Current Sensor Reader "
	       "Starting...===================\n");

	// Create semaphore for receive notification
	twai_listener_ctx_t twai_listener_ctx = {0};
	twai_listener_ctx.free_pool_semaphore =
	    xSemaphoreCreateCounting(POLL_DEPTH, POLL_DEPTH);
	twai_listener_ctx.rx_result_semaphore =
	    xSemaphoreCreateCounting(POLL_DEPTH, 0);
	assert(twai_listener_ctx.free_pool_semaphore != NULL);
	assert(twai_listener_ctx.rx_result_semaphore != NULL);

	twai_listener_ctx.rx_pool =
	    calloc(POLL_DEPTH, sizeof(twai_listener_data_t));
	assert(twai_listener_ctx.rx_pool != NULL);
	for (int i = 0; i < POLL_DEPTH; i++) {
		twai_listener_ctx.rx_pool[i].frame.buffer =
		    twai_listener_ctx.rx_pool[i].data;
		twai_listener_ctx.rx_pool[i].frame.buffer_len =
		    sizeof(twai_listener_ctx.rx_pool[i].data);
	}
	ESP_LOGI(TAG, "Buffer initialized: %d slots for burst data",
		 POLL_DEPTH);

	// Initialize current sensor instance and delta time
	meqscs_init(&twai_listener_ctx.meqscs);
	delta_time_init(&twai_listener_ctx.dt);
	uint32_t initial_time = (uint32_t)(esp_timer_get_time() / 1000ULL);
	(void)delta_time_update_ms(&twai_listener_ctx.dt, initial_time);

	// Configure TWAI node
	twai_onchip_node_config_t node_config = {
	    .io_cfg =
		{
			 .tx		       = TWAI_LISTENER_TX_GPIO,
			 .rx		       = TWAI_LISTENER_RX_GPIO,
			 .quanta_clk_out    = GPIO_NUM_NC,
			 .bus_off_indicator = GPIO_NUM_NC,
			 },
	    .bit_timing.bitrate	      = TWAI_BITRATE,
	    .timestamp_resolution_hz  = 1000000,
	    .flags.enable_listen_only = true,
	};

	// Create TWAI node
	ESP_ERROR_CHECK(
	    twai_new_node_onchip(&node_config, &twai_listener_ctx.node_hdl));
	ESP_LOGI(TAG, "TWAI node created");

	// Configure acceptance filter - accept H010 (0x010) and H060 (0x060)
	// or accept all
	twai_mask_filter_config_t data_filter = {
	    .id	    = 0x000,
	    .mask   = 0x000, // Accept all IDs
	    .is_ext = false,
	};
	ESP_ERROR_CHECK(twai_node_config_mask_filter(
	    twai_listener_ctx.node_hdl, 0, &data_filter));
	ESP_LOGI(TAG, "Filter enabled for all standard IDs");

	// Register callbacks
	twai_event_callbacks_t callbacks = {
	    .on_rx_done	     = twai_listener_rx_callback,
	    .on_error	     = twai_listener_on_error_callback,
	    .on_state_change = twai_listener_on_state_change_callback,
	};
	ESP_ERROR_CHECK(twai_node_register_event_callbacks(
	    twai_listener_ctx.node_hdl, &callbacks, &twai_listener_ctx));

	// Enable TWAI node
	ESP_ERROR_CHECK(twai_node_enable(twai_listener_ctx.node_hdl));
	ESP_LOGI(TAG, "TWAI start listening...");

	// Main loop - process buffered data and update timeout timer
	while (1) {
		// Wait for incoming frame or timeout (e.g. 10ms) to regularly
		// tick delta_time and sensor timeouts
		if (xSemaphoreTake(twai_listener_ctx.rx_result_semaphore,
				   pdMS_TO_TICKS(10)) == pdTRUE) {
			twai_frame_t *frame =
			    &twai_listener_ctx
				 .rx_pool[twai_listener_ctx.read_idx]
				 .frame;

			struct meqscs_frame meqs_frame;
			meqs_frame.id  = frame->header.id;
			meqs_frame.len = frame->header.dlc;
			if (meqs_frame.len > MEQSCS_MAX_FRAME_DATA_LEN) {
				meqs_frame.len = MEQSCS_MAX_FRAME_DATA_LEN;
			}
			for (int j = 0; j < meqs_frame.len; j++) {
				meqs_frame.data[j] = frame->buffer[j];
			}
			meqscs_hal_parse_frame(&twai_listener_ctx.meqscs,
					       &meqs_frame);

			twai_listener_ctx.read_idx =
			    (twai_listener_ctx.read_idx + 1) % POLL_DEPTH;
			xSemaphoreGive(twai_listener_ctx.free_pool_semaphore);
		}

		// Update delta time and sensor timeouts
		uint32_t current_time =
		    (uint32_t)(esp_timer_get_time() / 1000ULL);
		uint32_t dt_ms =
		    delta_time_update_ms(&twai_listener_ctx.dt, current_time);
		if (dt_ms > 0 && dt_ms < 1000) {
			meqscs_update(&twai_listener_ctx.meqscs, dt_ms);
		}

		// Get and log current variables if valid
		struct meqscs_vars vars;
		if (meqscs_get_vars(&twai_listener_ctx.meqscs, &vars)) {
			float adc0_a = (float)vars.adc0_r0p0001a * 0.0001f;
			float adc1_a = (float)vars.adc1_r0p0001a * 0.0001f;
			float adc2_a = (float)vars.adc2_r0p0001a * 0.0001f;
			ESP_LOGI(TAG,
				 "Currents -> ADC0: %.4f A, ADC1: %.4f A, "
				 "ADC2: %.4f A",
				 adc0_a, adc1_a, adc2_a);
		}
	}

	// Cleanup (unreachable in main loop, but good practice)
	vSemaphoreDelete(twai_listener_ctx.rx_result_semaphore);
	vSemaphoreDelete(twai_listener_ctx.free_pool_semaphore);
	free(twai_listener_ctx.rx_pool);
	ESP_ERROR_CHECK(twai_node_disable(twai_listener_ctx.node_hdl));
	ESP_ERROR_CHECK(twai_node_delete(twai_listener_ctx.node_hdl));
}
