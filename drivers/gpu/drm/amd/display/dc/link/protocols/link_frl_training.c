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

#define FRL_TRAINING_RETRIES 10

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
				  const struct link_resource *link_res)
{
	struct hpo_hdmi_link_encoder *enc = link_res->hpo_hdmi_link_enc;
	uint8_t lane_count = link->cur_link_settings.lane_count;
	uint8_t frl_rate = link->cur_link_settings.frl_rate;
	uint8_t sink_version;
	uint8_t update;
	uint8_t status;
	uint16_t ltp_req;
	uint8_t write_buffer[2];
	int poll;
	int lane;
	int post_training_timeout;

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
	frl_write_scdc(link, HDMI_SCDC_SOURCE_VERSION, write_buffer, 1);

	/* ------------------------------------------------------------------ */
	/* Step 2: Wait for FLT_READY                                          */
	/* ------------------------------------------------------------------ */
	for (poll = 0; poll < FRL_MAX_POLLS; poll++) {
		if (!frl_read_scdc(link, HDMI_SCDC_STATUS_FLAGS_0, &status, 1)) {
			msleep(FRL_POLL_DELAY_MS);
			continue;
		}

		if (status & SCDC_FLT_READY_BIT)
			break;

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
			enc->funcs->set_training_enable(enc, false);
			for (post_training_timeout = 0;
				post_training_timeout < FRL_MAX_POLLS;
				post_training_timeout++) {
				if (!frl_read_scdc(link, HDMI_SCDC_UPDATE_0, &update, 1)) {
					msleep(FRL_POLL_DELAY_MS);
					continue;
				}

				if (update & (SCDC_FLT_UPDATE_BIT | SCDC_FRL_START_BIT))
					break;

				msleep(FRL_POLL_DELAY_MS);
			}

			frl_write_scdc(link, HDMI_SCDC_UPDATE_0, &update, 1);

			if (update & SCDC_FRL_START_BIT) {
				DC_LOG_HW_LINK_TRAINING("HDMI FRL: training successful\n");
				return true;
			} else if (update & SCDC_FLT_UPDATE_BIT) {
				DC_LOG_HW_LINK_TRAINING("HDMI FRL: retraining needed\n");
				return false;
			} else {
				DC_LOG_HW_LINK_TRAINING("HDMI FRL: did not receive status update!\n");
				return false;
			}
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

bool dc_link_perform_frl_training_with_retries(struct dc_link *link,
				  const struct link_resource *link_res)
{
	bool success;
	int i;

	for (i = 0; i < FRL_TRAINING_RETRIES; ++i) {
		success = dc_link_perform_frl_training(link, link_res);

		if (success)
			break;

		DC_LOG_HW_LINK_TRAINING("FRL: Training attempt %d failed!");
	}

	return success;
}

void dc_link_disable_frl(struct dc_link *link)
{
	uint8_t write_buffer[2];

	/* Inform the sink not to expect an FRL signal */
	write_buffer[0] = 0;
	write_buffer[1] = 0;
	frl_write_scdc(link, HDMI_SCDC_CONFIG_0, write_buffer, 2);
}

/* DFM calc
 * References:
 * https://github.com/openharmony/device_soc_hisilicon/blob/master/hi3516dv300/sdk_linux/drv/mpp/component/hdmi/src/mkp/drv_hdmi_dfm.c#L264
 * https://github.com/jelyoussefi/intel-gpu-i915/blob/e366083d562341ce15d5fe1a39bb6e07f18a4745/drivers/gpu/drm/i915/display/intel_hdmi.c#L4217
 * TODO Move to generic DRM */

#define DFM_TOLERANCE_PIXEL_CLOCK_PCT_X1000 5   /* +0.50% -> 5/1000 */
#define DFM_TOLERANCE_AUDIO_CLOCK_PPM     1000 /* +/- 1000 ppm */
#define DFM_TOLERANCE_FRL_BIT_RATE_PPM    300  /* +/- 300 ppm */
#define DFM_TB_BORROWED_MAX               400
#define DFM_FRL_CFRL_CB                   510

struct dfm_config {
	/* Video Timing */
	uint32_t pixel_clock_khz; /* Nominal */
	uint32_t hactive;
	uint32_t hblank;
	uint32_t bpc;             /* Bits per component */
	enum dc_pixel_encoding encoding;

	/* FRL Configuration */
	uint32_t frl_bit_rate_gbps; /* 3, 6, 8, 10, 12 */
	uint8_t lanes;              /* 3 or 4 */

	/* Audio */
	bool audio_enable;
	uint32_t audio_freq_hz;
	uint8_t audio_channels;
};

struct dfm_results {
	/* Calculated Parameters */
	uint64_t f_pixel_clock_max;
	uint64_t r_frl_char_min;
	uint32_t overhead_ppm;

	/* Audio */
	uint32_t audio_packets_line;
	uint32_t hblank_audio_min;

	/* Tri-Bytes */
	uint32_t tb_active;
	uint32_t tb_blank;

	/* Compression */
	uint32_t cfrl_rc_savings;

	/* Timing (ns) */
	uint64_t t_active_min;
	uint64_t t_active_ref;
	uint64_t t_blank_min;
	uint64_t t_blank_ref;

	/* Borrowing */
	uint64_t tb_borrowed;

	/* Utilization */
	uint32_t cfrl_line;
	uint32_t cfrl_actual_payload;
	int32_t  margin_ppm; /* Can be negative */

	/* Pass/Fail Flags */
	bool audio_supported;
	bool dfm_supported;
	bool utilization_supported;
	bool total_supported;
};


#define CEILING(x, y) (((x) + (y) - 1) / (y))

static uint32_t calc_overhead_ppm(uint8_t lanes)
{
	uint32_t cfrl_sb = (4 * 510) + lanes;

	uint64_t oh_sb_num = (uint64_t)lanes * 1000000ULL;
	uint32_t oh_sb = (uint32_t)((oh_sb_num + (cfrl_sb/2)) / cfrl_sb); /* Rounding */

	uint64_t oh_rs_num = 32ULL * 1000000ULL;
	uint32_t oh_rs = (uint32_t)((oh_rs_num + (cfrl_sb/2)) / cfrl_sb);

	uint64_t oh_map_num = 2500000ULL; /* 2.5 * 1M */
	uint32_t oh_map = (uint32_t)((oh_map_num + (cfrl_sb/2)) / cfrl_sb);

	uint32_t oh_m = 3000;

	return oh_sb + oh_rs + oh_map + oh_m;
}

static void calc_audio(const struct dfm_config *cfg,
					   uint64_t t_line_ns,
					   struct dfm_results *res)
{
	if (!cfg->audio_enable) {
		res->audio_packets_line = 0;
		res->hblank_audio_min = 64;
		return;
	}

	uint32_t ap = 1;
	if (cfg->audio_channels > 22) {
		ap = 4; /* ACAT 3 (up to 30.2ch) */
	} else if (cfg->audio_channels > 10) {
		ap = 3; /* ACAT 2 (up to 22.2ch) */
	} else if (cfg->audio_channels > 8) {
		ap = 2; /* ACAT 1 (up to 10.2ch) */
	} else {
		ap = 1; /* Layout 0/1 */
	}

	uint64_t rap = (uint64_t)cfg->audio_freq_hz * (1000000 + DFM_TOLERANCE_AUDIO_CLOCK_PPM);
	rap = (rap * ap) / 1000000;

	uint64_t avg_pkts_x1000 = (rap * t_line_ns) / 1000000; /* ns to ms scale */

	res->audio_packets_line = (uint32_t)((avg_pkts_x1000 + 999) / 1000);

	res->hblank_audio_min = 64 + (32 * res->audio_packets_line);
}

/* Table 6-44: RC Compression Savings */
static void calc_rc_compression(const struct dfm_config *cfg, struct dfm_results *res)
{
	uint32_t k420 = (cfg->encoding == PIXEL_ENCODING_YCBCR420) ? 2 : 1;
	uint32_t kcd = (cfg->encoding == PIXEL_ENCODING_YCBCR422) ? 1 : cfg->bpc;

	uint64_t raw_num = (uint64_t)cfg->hblank * kcd * 8;
	uint64_t raw_den = (uint64_t)k420 * 7;
	uint32_t raw_capacity = (uint32_t)(raw_num / raw_den); /* FLOOR */

	uint32_t deduct = 32 * (1 + res->audio_packets_line) + 7;
	int32_t free_chars = (int32_t)raw_capacity - (int32_t)deduct;

	if (free_chars < 0) free_chars = 0;

	int32_t margin = 4;

	int32_t base = free_chars - margin;
	if (base < 0) base = 0;

	res->cfrl_rc_savings = (uint32_t)((base * 7) / 8);
}

/* Core DFM Check Function */
static void perform_dfm_check(const struct dfm_config *cfg, struct dfm_results *res)
{
	memset(res, 0, sizeof(struct dfm_results));

	uint32_t htotal = cfg->hactive + cfg->hblank;

	res->overhead_ppm = calc_overhead_ppm(cfg->lanes);

	res->f_pixel_clock_max = (uint64_t)cfg->pixel_clock_khz * 1000;
	res->f_pixel_clock_max = res->f_pixel_clock_max * (1000 + DFM_TOLERANCE_PIXEL_CLOCK_PCT_X1000) / 1000;

	uint64_t t_line_ns = ((uint64_t)htotal * 1000000000ULL) / res->f_pixel_clock_max;

	uint64_t r_bit_nominal = (uint64_t)cfg->frl_bit_rate_gbps * 1000000000ULL;
	uint64_t r_bit_min = r_bit_nominal * (1000000 - DFM_TOLERANCE_FRL_BIT_RATE_PPM) / 1000000;

	res->r_frl_char_min = r_bit_min / 18;

	uint64_t cap_calc = (t_line_ns * res->r_frl_char_min * cfg->lanes) / 1000000000ULL;
	res->cfrl_line = (uint32_t)cap_calc;

	calc_audio(cfg, t_line_ns, res);

	calc_rc_compression(cfg, res);

	uint32_t bpp;
	uint32_t k420 = (cfg->encoding == PIXEL_ENCODING_YCBCR420) ? 2 : 1;
	uint32_t kcd = (cfg->encoding == PIXEL_ENCODING_YCBCR422) ? 1 : cfg->bpc;

	bpp = (24 * kcd) / (k420 * 8);

	uint64_t bytes_line = ((uint64_t)bpp * cfg->hactive) / 8;

	res->tb_active = (uint32_t)CEILING(bytes_line, 3);

	uint64_t tb_blank_num = (uint64_t)cfg->hblank * kcd;
	uint64_t tb_blank_den = (uint64_t)k420 * 8;
	res->tb_blank = (uint32_t)CEILING(tb_blank_num, tb_blank_den);

	if (res->hblank_audio_min <= res->tb_blank) {
		res->audio_supported = true;
	} else {
		res->audio_supported = false;
		res->total_supported = false;
		return;
	}

	uint64_t tb_total = res->tb_active + res->tb_blank;
	uint64_t f_tb_avg = (res->f_pixel_clock_max * tb_total) / htotal;

	res->t_active_ref = (t_line_ns * cfg->hactive) / htotal;

	res->t_blank_ref = (t_line_ns * cfg->hblank) / htotal;

	uint64_t r_frl_eff = res->r_frl_char_min * (1000000 - res->overhead_ppm) / 1000000;

	res->t_active_min = (3ULL * res->tb_active * 1000000000ULL) / (2ULL * cfg->lanes * r_frl_eff);

	res->t_blank_min = (res->tb_blank * 1000000000ULL) / (cfg->lanes * r_frl_eff);

	if (res->t_active_ref >= res->t_active_min && res->t_blank_ref >= res->t_blank_min) {
		res->tb_borrowed = 0;
		res->dfm_supported = true;
	} else if (res->t_active_ref < res->t_active_min) {
		uint64_t borrow_time_ns = res->t_active_min - res->t_active_ref;

		res->tb_borrowed = CEILING(borrow_time_ns * f_tb_avg, 1000000000ULL);

		if (res->tb_borrowed <= DFM_TB_BORROWED_MAX) {
			res->dfm_supported = true;
		} else {
			res->dfm_supported = false;
		}
	} else {
		/* Blanking needs more time than available */
		res->dfm_supported = false;
	}

	if (!res->dfm_supported) {
		res->total_supported = false;
		return;
	}

	uint64_t total_chars_req = (3 * tb_total) / 2;
	if (total_chars_req > res->cfrl_rc_savings) {
		res->cfrl_actual_payload = (uint32_t)CEILING((total_chars_req - res->cfrl_rc_savings), 1);
	} else {
		res->cfrl_actual_payload = 0;
	}

	uint64_t util_ppm = ((uint64_t)res->cfrl_actual_payload * 1000000ULL) / res->cfrl_line;

	int32_t margin_ppm = 1000000 - (int32_t)util_ppm + (int32_t)res->overhead_ppm;
	res->margin_ppm = margin_ppm;

	if (margin_ppm >= 0) {
		res->utilization_supported = true;
		res->total_supported = true;
	} else {
		res->utilization_supported = false;
		res->total_supported = false;
	}
}

bool hdmi_decide_link_settings(
	struct dc_stream_state *stream,
	struct pipe_ctx *pipe_ctx)
{
	struct dfm_config cfg;
	struct dfm_results res;
	uint8_t frl_rate;
	static const uint32_t frl_rates_gbps[] = {3, 6, 6, 8, 10, 12};
	uint8_t max_rate = stream->link->local_sink->edid_caps.frl_caps.max_rate;

	if (max_rate == 0 || max_rate > 6)
		return false;

	memset(&cfg, 0, sizeof(cfg));

	cfg.pixel_clock_khz = stream->timing.pix_clk_100hz / 10;
	cfg.hactive = stream->timing.h_addressable;
	cfg.hblank = stream->timing.h_total - stream->timing.h_addressable;
	cfg.encoding = stream->timing.pixel_encoding;

	switch (stream->timing.display_color_depth) {
	case COLOR_DEPTH_888:    cfg.bpc = 8;  break;
	case COLOR_DEPTH_101010: cfg.bpc = 10; break;
	case COLOR_DEPTH_121212: cfg.bpc = 12; break;
	case COLOR_DEPTH_161616: cfg.bpc = 16; break;
	default:
		cfg.bpc = 8;
		break;
	}

	if (stream->audio_info.mode_count) {
		cfg.audio_enable = true;
		/* If we assume the worst case (e.g. 192KHz 32 channel audio) we risk
		 * unfairly pruning low res modes with short Hblank periods. Instead,
		 * assume a standard surround sound mode that should allow all video
		 * modes to work.
		 */
		cfg.audio_freq_hz = 48000;
		cfg.audio_channels = 8;
	}

	for (frl_rate = 1; frl_rate <= max_rate; frl_rate++) {
		cfg.frl_bit_rate_gbps = frl_rates_gbps[frl_rate - 1];
		cfg.lanes = (frl_rate <= 2) ? 3 : 4;

		perform_dfm_check(&cfg, &res);

		if (res.total_supported) {
			pipe_ctx->link_config.dp_link_settings.frl_rate = frl_rate;
			pipe_ctx->link_config.dp_link_settings.lane_count = cfg.lanes;

			pr_info("HDMI FRL: Rate %d Supported. Borrowed: %d, Margin: %d ppm\n",
					frl_rate, res.tb_borrowed, res.margin_ppm);
			return true;
		}
	}

	return false;
}