/*
 * HDMI 2.1 Fixed Rate Link (FRL) training
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#include "link_frl_training.h"
#include "core_types.h"
#include "dm_helpers.h"
#include "link_hwss.h"
#include "link_ddc.h"
#include "dc_hdmi_types.h"

#define DC_LOGGER link->ctx->logger

#define SCDC_FLT_READY_BIT 0x40
#define SCDC_FLT_UPDATE_BIT 0x20
#define SCDC_FRL_START_BIT 0x10

#define HDMI_LTP_PASS 0x0
#define HDMI_LTP_NEXT_FFE 0xE
#define HDMI_LTP_LOWER_RATE 0xF

#define FRL_MAX_LANES 4
#define FRL_POLL_DELAY_MS 2
#define FRL_MAX_POLLS 100

static bool frl_read_scdc(struct dc_link *link, uint8_t offset, void *buf,
			  uint32_t size)
{
	uint8_t slave_address = HDMI_SCDC_ADDRESS;

	return link_query_ddc_data(link->ddc, slave_address, &offset,
				   sizeof(offset), buf, size);
}

static bool frl_write_scdc(struct dc_link *link, uint8_t offset,
			   const void *buf, uint32_t size)
{
	uint8_t slave_address = HDMI_SCDC_ADDRESS;
	uint8_t write_buffer[17] = { 0 };

	if (size > 16)
		return false;

	write_buffer[0] = offset;
	for (int i = 0; i < size; ++i)
		write_buffer[i + 1] = ((uint8_t *)buf)[i];

	return link_query_ddc_data(link->ddc, slave_address, write_buffer,
				   size + 1, NULL, 0);
}

bool dc_link_perform_frl_training(struct dc_link *link,
				  const struct link_resource *link_res,
				  uint8_t frl_rate, uint8_t lane_count)
{
	struct hpo_hdmi_link_encoder *enc = link_res->hpo_hdmi_link_enc;
	uint8_t sink_version;
	uint8_t update;
	uint8_t status;
	uint8_t flags;
	uint16_t ltp_req;
	uint8_t write_buffer[2];
	int poll;
	int lane;

	if (!enc || lane_count == 0 || lane_count > FRL_MAX_LANES)
		return false;

	DC_LOG_HW_LINK_TRAINING(
		"HDMI FRL: starting training (rate=%u lanes=%u)\n", frl_rate,
		lane_count);

	/* ------------------------------------------------------------------ */
	/* Step 1: Read sink version                                           */
	/* ------------------------------------------------------------------ */
	if (!frl_read_scdc(link, HDMI_SCDC_SINK_VERSION, &sink_version, 1)) {
		DC_LOG_ERROR("FRL: failed to read sink version\n");
		return false;
	}

	if (sink_version == 0) {
		DC_LOG_HW_LINK_TRAINING("FRL: sink does not support FRL\n");
		return false;
	}

	/* Write source version = 1 */
	write_buffer[0] = 0x01;
	frl_write_scdc(link, HDMI_SCDC_SOURCE_VERSION, &write_buffer[0], 1);

	/* Reset FRL rate */
	write_buffer[0] = 0;
	write_buffer[1] = 0;
	frl_write_scdc(link, 0x30, &write_buffer[0], 2);

	/* ------------------------------------------------------------------ */
	/* Step 2: Wait for FLT_READY                                          */
	/* ------------------------------------------------------------------ */
	for (poll = 0; poll < FRL_MAX_POLLS; poll++) {
		if (!frl_read_scdc(link, HDMI_SCDC_STATUS_FLAGS_0, &status, 1))
			goto poll_delay;

		if (status & SCDC_FLT_READY_BIT)
			break;

poll_delay:
		msleep(FRL_POLL_DELAY_MS);
	}

	if (!(status & SCDC_FLT_READY_BIT)) {
		DC_LOG_ERROR("FRL: FLT_READY timeout\n");
		return false;
	}

	/* Write FRL rate */
	write_buffer[0] = 0;
	write_buffer[1] = frl_rate;
	frl_write_scdc(link, HDMI_SCDC_CONFIG_0, write_buffer, 2);

	/* ------------------------------------------------------------------ */
	/* Step 3: Enable FRL training in HW                                   */
	/* ------------------------------------------------------------------ */
	enc->funcs->set_training_enable(enc, true);

	pr_err("FRL DEBUG: entering training loop\n");

	for (poll = 0; poll < FRL_MAX_POLLS; poll++) {
		pr_err("FRL DEBUG: poll %d\n", poll);

		/* Read UPDATE_0 */
		if (!frl_read_scdc(link, HDMI_SCDC_UPDATE_0, &update, 1)) {
			pr_err("FRL DEBUG: failed to read SCDC_UPDATE_0\n");
			msleep(FRL_POLL_DELAY_MS);
			continue;
		}

		pr_err("FRL DEBUG: UPDATE_0 = 0x%02x\n", update);

		if (update & SCDC_FRL_START_BIT) {
			pr_err("FRL DEBUG: FRL_START asserted\n");
			DC_LOG_HW_LINK_TRAINING(
				"HDMI FRL: Link already trained\n");
			enc->funcs->set_training_enable(enc, false);
			return true;
		}

		if (!(update & SCDC_FLT_UPDATE_BIT)) {
			pr_err("FRL DEBUG: FLT_UPDATE not set\n");
			msleep(FRL_POLL_DELAY_MS);
			continue;
		}

		pr_err("FRL DEBUG: FLT_UPDATE asserted\n");

		/* Clear UPDATE_0 */
		if (!frl_write_scdc(link, HDMI_SCDC_UPDATE_0, &update, 1)) {
			pr_err("FRL DEBUG: failed to clear SCDC_UPDATE_0\n");
			msleep(FRL_POLL_DELAY_MS);
			continue;
		}

		/* Read STATUS_FLAGS_1 (LTP requests) */
		if (!frl_read_scdc(link, HDMI_SCDC_STATUS_FLAGS_1, &ltp_req, 2)) {
			pr_err("FRL DEBUG: failed to read SCDC_STATUS_FLAGS_1\n");
			continue;
		}

		/* Decode per-lane LTP */
		for (lane = 0; lane < lane_count; lane++) {
			uint8_t ltp = (ltp_req >> (lane * 4)) & 0xF;
			pr_err("FRL DEBUG: lane %d LTP = 0x%x\n", lane, ltp);
		}

		/* Check for PASS on all active lanes */
		for (lane = 0; lane < lane_count; lane++) {
			uint8_t ltp = (ltp_req >> (lane * 4)) & 0xF;
			if (ltp != HDMI_LTP_PASS)
				break;
		}

		if (lane == lane_count) {
			DC_LOG_HW_LINK_TRAINING(
				"HDMI FRL: training successful\n");
			enc->funcs->set_training_enable(enc, false);
			return true;
		}

		/* Program requested training patterns */
		pr_err("FRL DEBUG: programming training patterns\n");
		enc->funcs->set_training_patterns(enc,
						  ((ltp_req >> 0) & 0xF) - 1,
						  ((ltp_req >> 4) & 0xF) - 1,
						  ((ltp_req >> 8) & 0xF) - 1,
						  ((ltp_req >> 12) & 0xF) - 1);

		/* Detect LOWER_RATE / NEXT_FFE */
		for (lane = 0; lane < lane_count; lane++) {
			uint8_t ltp = (ltp_req >> (lane * 4)) & 0xF;

			if (ltp == HDMI_LTP_LOWER_RATE) {
				pr_err("FRL DEBUG: lane %d requested LOWER_RATE\n",
				       lane);
				DC_LOG_ERROR(
					"FRL: sink requested lower rate (unsupported)\n");
				goto fail;
			}

			if (ltp == HDMI_LTP_NEXT_FFE) {
				pr_err("FRL DEBUG: lane %d requested NEXT_FFE (ignored)\n",
				       lane);
			}
		}

		msleep(FRL_POLL_DELAY_MS);
	}

fail:
	DC_LOG_ERROR("FRL: training failed\n");

	/* Reset FRL rate */
	write_buffer[0] = 0;
	write_buffer[1] = 0;
	frl_write_scdc(link, 0x30, write_buffer, 2);

	return false;
}
