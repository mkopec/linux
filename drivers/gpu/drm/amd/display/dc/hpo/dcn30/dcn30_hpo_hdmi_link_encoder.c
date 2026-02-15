/*
 * HPO HDMI Link Encoder
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#include "dc_bios_types.h"
#include "dcn30_hpo_hdmi_link_encoder.h"
#include "reg_helper.h"
#include "stream_encoder.h"
#include "dc_bios_types.h"

#define DC_LOGGER enc3->base.ctx->logger

#define REG(reg) (enc3->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	enc3->shift->field_name, enc3->mask->field_name

#define CTX enc3->base.ctx

static void dcn30_hpo_hdmi_link_enc_enable(struct hpo_hdmi_link_encoder *enc,
				    uint8_t lane_count)
{
	struct dcn30_hpo_hdmi_link_encoder *enc3 =
		DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(enc);

	REG_UPDATE(HDMI_LINK_ENC_CLK_CTRL, HDMI_LINK_ENC_CLOCK_EN, 1);

	REG_UPDATE(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE_COUNT,
		   (lane_count == 4) ? 1 : 0);

	REG_UPDATE(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_SOFT_RESET, 1);
	REG_UPDATE(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_SOFT_RESET, 0);

	REG_UPDATE(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_ENABLE, 1);
}

static void dcn30_hpo_hdmi_link_enc_disable(struct hpo_hdmi_link_encoder *enc)
{
	struct dcn30_hpo_hdmi_link_encoder *enc3 =
		DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(enc);

	REG_UPDATE_4(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE0_TRAINING_PATTERN, 0,
		     HDMI_LINK_LANE1_TRAINING_PATTERN, 0,
		     HDMI_LINK_LANE2_TRAINING_PATTERN, 0,
		     HDMI_LINK_LANE3_TRAINING_PATTERN, 0);

	// If this is disabled here and the link is later re-enabled, the
	// meterbuffer will overflow, requiring a full platform reset to reset.
	// For some reason enabling training mode fixes that issue.
	REG_UPDATE(HDMI_FRL_ENC_CONFIG, HDMI_LINK_TRAINING_ENABLE, 1);

	REG_UPDATE(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_ENABLE, 0);

	REG_UPDATE(HDMI_LINK_ENC_CLK_CTRL, HDMI_LINK_ENC_CLOCK_EN, 0);
}

static enum bp_result
link_transmitter_control(struct dcn30_hpo_hdmi_link_encoder *enc3,
			 struct bp_transmitter_control *cntl)
{
	enum bp_result result;
	struct dc_bios *bp = enc3->base.ctx->dc_bios;

	result = bp->funcs->transmitter_control(bp, cntl);

	return result;
}

static uint32_t hdmi_frl_rate_to_pixel_clock(uint8_t frl_rate)
{
	/* Lane rate in Gbps per HDMI 2.1 spec */
	static const uint8_t frl_lane_rate_gbps[] = {
		[1] = 3, [2] = 6, [3] = 6, [4] = 8, [5] = 10, [6] = 12,
	};

	uint32_t lane_rate_gbps;

	if (frl_rate < 1 || frl_rate > 6)
		return 0;

	lane_rate_gbps = frl_lane_rate_gbps[frl_rate];

	/*
	 * HDMI 2.1 FRL uses 16b/18b encoding.
	 * Character clock = lane_rate / 18.
	 */
	return (lane_rate_gbps * 1000000) / 18;
}

static void dcn30_hpo_hdmi_link_enc_configure_transmitter(
	struct hpo_hdmi_link_encoder *enc, const struct dc_link *link,
	uint8_t frl_rate, enum transmitter transmitter,
	enum hpd_source_id hpd_source)
{
	struct dcn30_hpo_hdmi_link_encoder *enc3 =
		DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(enc);
	struct bp_transmitter_control cntl = { 0 };
	enum bp_result result;

	/* Set the transmitter */
	enc3->base.transmitter = transmitter;

	/* Set the hpd source */
	enc3->base.hpd_source = hpd_source;

	cntl.action = TRANSMITTER_CONTROL_ENABLE;
	cntl.engine_id = enc3->base.preferred_engine;
	cntl.transmitter = enc3->base.transmitter;
	cntl.signal = SIGNAL_TYPE_HDMI_FRL;
	cntl.lanes_number = (frl_rate <= 2) ? 3 : 4;
	cntl.hpd_sel = enc3->base.hpd_source;
	cntl.pixel_clock = hdmi_frl_rate_to_pixel_clock(frl_rate);
	cntl.color_depth = COLOR_DEPTH_UNDEFINED;
	cntl.hpo_engine_id = enc->inst + ENGINE_ID_HPO_0;

	result = link_transmitter_control(enc3, &cntl);

	if (result != BP_RESULT_OK) {
		DC_LOG_ERROR("%s: Failed to execute VBIOS command table!\n",
			     __func__);
		BREAK_TO_DEBUGGER();
		return;
	}
}

static void dcn30_hpo_hdmi_link_enc_set_training_enable(
	struct hpo_hdmi_link_encoder *enc, bool enable)
{
	struct dcn30_hpo_hdmi_link_encoder *enc3 =
		DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(enc);

	REG_UPDATE(HDMI_FRL_ENC_CONFIG, HDMI_LINK_TRAINING_ENABLE, enable);
}

static void dcn30_hpo_hdmi_link_enc_set_training_patterns(
	struct hpo_hdmi_link_encoder *enc, uint8_t lane0_ltp, uint8_t lane1_ltp,
	uint8_t lane2_ltp, uint8_t lane3_ltp)
{
	struct dcn30_hpo_hdmi_link_encoder *enc3 =
		DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(enc);

	REG_UPDATE_4(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE0_TRAINING_PATTERN,
		     lane0_ltp, HDMI_LINK_LANE1_TRAINING_PATTERN, lane1_ltp,
		     HDMI_LINK_LANE2_TRAINING_PATTERN, lane2_ltp,
		     HDMI_LINK_LANE3_TRAINING_PATTERN, lane3_ltp);
}

static const struct hpo_hdmi_link_encoder_funcs
	dcn30_hpo_hdmi_link_encoder_funcs = {
		.link_enable = dcn30_hpo_hdmi_link_enc_enable,
		.link_disable = dcn30_hpo_hdmi_link_enc_disable,
		.configure_transmitter =
			dcn30_hpo_hdmi_link_enc_configure_transmitter,
		.set_training_enable =
			dcn30_hpo_hdmi_link_enc_set_training_enable,
		.set_training_patterns =
			dcn30_hpo_hdmi_link_enc_set_training_patterns,
	};

void hpo_hdmi_link_encoder31_construct(
	struct dcn30_hpo_hdmi_link_encoder *enc3, struct dc_context *ctx,
	uint32_t inst, const struct dcn30_hpo_hdmi_link_encoder_registers *regs,
	const struct dcn30_hpo_hdmi_link_encoder_shift *shift,
	const struct dcn30_hpo_hdmi_link_encoder_mask *mask)
{
	enc3->base.ctx = ctx;

	enc3->base.inst = inst;
	enc3->base.funcs = &dcn30_hpo_hdmi_link_encoder_funcs;
	enc3->base.hpd_source = HPD_SOURCEID_UNKNOWN;
	enc3->base.transmitter = TRANSMITTER_UNKNOWN;

	enc3->regs = regs;
	enc3->shift = shift;
	enc3->mask = mask;
}
