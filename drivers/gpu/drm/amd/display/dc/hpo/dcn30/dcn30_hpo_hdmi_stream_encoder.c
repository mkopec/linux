/*
 * HPO HDMI Stream Encoder
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#include "dc_bios_types.h"
#include "dcn30_hpo_hdmi_stream_encoder.h"
#include "reg_helper.h"
#include "dc.h"

#define DC_LOGGER enc3->base.ctx->logger

#define REG(reg) (enc3->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	enc3->hpo_se_shift->field_name, enc3->hpo_se_mask->field_name

#define VBI_LINE_0 0

#define CTX enc3->base.ctx

static void
dcn30_hpo_hdmi_stream_enc_enable(struct hpo_hdmi_stream_encoder *enc,
				 uint8_t source_select)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);

	/*
	 * Enable all clock domains first - clocks must be running
	 * before we attempt any reset or FIFO operation.
	 */
	REG_UPDATE_4(HDMI_STREAM_ENC_CLOCK_CONTROL, HDMI_STREAM_ENC_CLOCK_EN, 1,
		     HDMI_STREAM_ENC_CLOCK_ON_DISPCLK, 1,
		     HDMI_STREAM_ENC_CLOCK_ON_SOCCLK, 1,
		     HDMI_STREAM_ENC_CLOCK_ON_HDMISTREAMCLK, 1);

	REG_UPDATE(HDMI_STREAM_ENC_INPUT_MUX_CONTROL,
		   HDMI_STREAM_ENC_INPUT_MUX_SOURCE_SEL, source_select);

	REG_UPDATE(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		   FIFO_RESET, 1);

	REG_WAIT(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		 FIFO_RESET_DONE, 1, 10, 1000);

	REG_UPDATE(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		   FIFO_RESET, 0);

	REG_WAIT(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		 FIFO_RESET_DONE, 0, 10, 1000);

	REG_UPDATE(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		   FIFO_ENABLE, 1);

	REG_UPDATE(HDMI_TB_ENC_CONTROL, HDMI_RESET, 1);

	REG_WAIT(HDMI_TB_ENC_CONTROL, HDMI_RESET_DONE, 1, 10, 1000);

	REG_UPDATE(HDMI_TB_ENC_CONTROL, HDMI_RESET, 0);

	REG_WAIT(HDMI_TB_ENC_CONTROL, HDMI_RESET_DONE, 0, 10, 1000);

	REG_UPDATE(HDMI_TB_ENC_CONTROL, HDMI_TB_ENC_EN, 1);
}

static void
dcn30_hpo_hdmi_stream_enc_disable(struct hpo_hdmi_stream_encoder *enc)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);

	/* Disable TB encoder first */
	REG_UPDATE(HDMI_TB_ENC_CONTROL, HDMI_TB_ENC_EN, 0);

	/* Disable FIFO */
	REG_UPDATE(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,
		   FIFO_ENABLE, 0);

	/* Disable all clock domains */
	REG_UPDATE_4(HDMI_STREAM_ENC_CLOCK_CONTROL, HDMI_STREAM_ENC_CLOCK_EN, 0,
		     HDMI_STREAM_ENC_CLOCK_ON_DISPCLK, 0,
		     HDMI_STREAM_ENC_CLOCK_ON_SOCCLK, 0,
		     HDMI_STREAM_ENC_CLOCK_ON_HDMISTREAMCLK, 0);
}

static void dcn30_hpo_hdmi_stream_enc_update_info_packet(
	struct dcn30_hpo_hdmi_stream_encoder *enc3, uint32_t packet_index,
	const struct dc_info_packet *info_packet)
{
	uint32_t cont, send, line;

	if (info_packet->valid) {
		/*
		 * Write packet data via VPG - same as DIO path.
		 * VPG is shared infrastructure, works identically
		 * for both DIO and HPO encoders.
		 */
		enc3->base.vpg->funcs->update_generic_info_packet(
			enc3->base.vpg, packet_index, info_packet, true);

		cont = 1;
		send = 1;
		line = 2;
	} else {
		cont = 0;
		send = 0;
		line = 0;
	}

	/*
	 * Enable/disable packet transmission via TB encoder registers.
	 * Layout mirrors DIO enc3_update_hdmi_info_packet() but uses
	 * HDMI_TB_ENC_GENERIC_PACKET_CONTROL* registers.
	 *
	 * CONTROL0/1 handle SEND+CONT for all 15 packets.
	 * LINE registers pack two packets each.
	 */
	switch (packet_index) {
	case 0:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC0_SEND, send, HDMI_GENERIC0_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE,
			   HDMI_GENERIC0_LINE, line);
		break;
	case 1:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC1_SEND, send, HDMI_GENERIC1_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE,
			   HDMI_GENERIC1_LINE, line);
		break;
	case 2:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC2_SEND, send, HDMI_GENERIC2_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE,
			   HDMI_GENERIC2_LINE, line);
		break;
	case 3:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC3_SEND, send, HDMI_GENERIC3_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE,
			   HDMI_GENERIC3_LINE, line);
		break;
	case 4:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC4_SEND, send, HDMI_GENERIC4_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE,
			   HDMI_GENERIC4_LINE, line);
		break;
	case 5:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC5_SEND, send, HDMI_GENERIC5_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE,
			   HDMI_GENERIC5_LINE, line);
		break;
	case 6:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC6_SEND, send, HDMI_GENERIC6_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE,
			   HDMI_GENERIC6_LINE, line);
		break;
	case 7:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,
			     HDMI_GENERIC7_SEND, send, HDMI_GENERIC7_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE,
			   HDMI_GENERIC7_LINE, line);
		break;
	case 8:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC8_SEND, send, HDMI_GENERIC8_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE,
			   HDMI_GENERIC8_LINE, line);
		break;
	case 9:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC9_SEND, send, HDMI_GENERIC9_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE,
			   HDMI_GENERIC9_LINE, line);
		break;
	case 10:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC10_SEND, send, HDMI_GENERIC10_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,
			   HDMI_GENERIC10_LINE, line);
		break;
	case 11:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC11_SEND, send, HDMI_GENERIC11_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,
			   HDMI_GENERIC11_LINE, line);
		break;
	case 12:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC12_SEND, send, HDMI_GENERIC12_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,
			   HDMI_GENERIC12_LINE, line);
		break;
	case 13:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC13_SEND, send, HDMI_GENERIC13_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,
			   HDMI_GENERIC13_LINE, line);
		break;
	case 14:
		REG_UPDATE_2(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,
			     HDMI_GENERIC14_SEND, send, HDMI_GENERIC14_CONT,
			     cont);
		REG_UPDATE(HDMI_TB_ENC_GENERIC_PACKET14_LINE,
			   HDMI_GENERIC14_LINE, line);
		break;
	default:
		DC_LOG_WARNING("Invalid packet index: %s()\n", __func__);
		return;
	}
}

static void dcn30_hpo_hdmi_stream_enc_update_info_packets(
	struct hpo_hdmi_stream_encoder *enc,
	const struct encoder_info_frame *info_frame)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);

	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 0, &info_frame->avi);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 5,
						     &info_frame->hfvsif);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 2,
						     &info_frame->gamut);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 1,
						     &info_frame->vendor);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 3, &info_frame->spd);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 4,
						     &info_frame->hdrsmd);
	dcn30_hpo_hdmi_stream_enc_update_info_packet(enc3, 6,
						     &info_frame->vtem);
}

static void
dcn30_hpo_hdmi_stream_enc_stop_info_packets(struct hpo_hdmi_stream_encoder *enc)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);

	/* Disable all generic packet slots */
	REG_UPDATE_8(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC0_SEND, 0,
		     HDMI_GENERIC0_CONT, 0, HDMI_GENERIC1_SEND, 0,
		     HDMI_GENERIC1_CONT, 0, HDMI_GENERIC2_SEND, 0,
		     HDMI_GENERIC2_CONT, 0, HDMI_GENERIC3_SEND, 0,
		     HDMI_GENERIC3_CONT, 0);

	REG_UPDATE_8(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC4_SEND, 0,
		     HDMI_GENERIC4_CONT, 0, HDMI_GENERIC5_SEND, 0,
		     HDMI_GENERIC5_CONT, 0, HDMI_GENERIC6_SEND, 0,
		     HDMI_GENERIC6_CONT, 0, HDMI_GENERIC7_SEND, 0,
		     HDMI_GENERIC7_CONT, 0);

	REG_UPDATE_8(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC8_SEND, 0,
		     HDMI_GENERIC8_CONT, 0, HDMI_GENERIC9_SEND, 0,
		     HDMI_GENERIC9_CONT, 0, HDMI_GENERIC10_SEND, 0,
		     HDMI_GENERIC10_CONT, 0, HDMI_GENERIC11_SEND, 0,
		     HDMI_GENERIC11_CONT, 0);

	REG_UPDATE_6(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC12_SEND,
		     0, HDMI_GENERIC12_CONT, 0, HDMI_GENERIC13_SEND, 0,
		     HDMI_GENERIC13_CONT, 0, HDMI_GENERIC14_SEND, 0,
		     HDMI_GENERIC14_CONT, 0);
}

static void dcn30_hpo_hdmi_stream_enc_setup_stream_attribute(
	struct hpo_hdmi_stream_encoder *enc, struct dc_crtc_timing *timing,
	enum dc_color_space output_color_space)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);
	uint32_t h_blank;

	uint32_t pixel_encoding;
	uint32_t deep_color_depth;
	uint32_t deep_color_enable;

	switch (timing->pixel_encoding) {
	case PIXEL_ENCODING_YCBCR422:
		pixel_encoding = 1;
		break;
	case PIXEL_ENCODING_YCBCR420:
		pixel_encoding = 2;
		break;
	case PIXEL_ENCODING_YCBCR444:
	default: /* RGB */
		pixel_encoding = 0;
		break;
	}

	switch (timing->display_color_depth) {
	case COLOR_DEPTH_888:
		deep_color_depth = 0;
		deep_color_enable = 0;
		break;
	case COLOR_DEPTH_101010:
		deep_color_depth = 1;
		deep_color_enable =
			(timing->pixel_encoding != PIXEL_ENCODING_YCBCR422) ?
				1 :
				0;
		break;
	case COLOR_DEPTH_121212:
		deep_color_depth = 2;
		deep_color_enable =
			(timing->pixel_encoding != PIXEL_ENCODING_YCBCR422) ?
				1 :
				0;
		break;
	case COLOR_DEPTH_161616:
		deep_color_depth = 3;
		deep_color_enable = 1;
		break;
	default:
		deep_color_depth = 0;
		deep_color_enable = 0;
		break;
	}

	REG_UPDATE_4(HDMI_TB_ENC_PIXEL_FORMAT,
		     HDMI_PIXEL_ENCODING, pixel_encoding,
		     HDMI_DEEP_COLOR_DEPTH, deep_color_depth,
		     HDMI_DEEP_COLOR_ENABLE, deep_color_enable,
		     HDMI_DSC_MODE, 0);

	/* H timing */
	h_blank = timing->h_total - timing->h_addressable -
		  timing->h_border_left - timing->h_border_right;

	REG_UPDATE_2(HDMI_TB_ENC_H_ACTIVE_BLANK,
		     HDMI_H_ACTIVE, timing->h_addressable,
		     HDMI_H_BLANK, h_blank);

	/* CRC */
	REG_UPDATE_2(HDMI_TB_ENC_CRC_CNTL, HDMI_CRC_EN, 1, HDMI_CRC_CONT_EN, 1);

	/* Borrow mode */
	REG_UPDATE(HDMI_TB_ENC_MODE, HDMI_BORROW_MODE, 1);

	/* Disable double buffering */
	REG_UPDATE(HDMI_TB_ENC_DB_CONTROL, HDMI_DB_DISABLE, 1);

	/* Max 1 packet per line */
	REG_UPDATE(HDMI_TB_ENC_PACKET_CONTROL, HDMI_MAX_PACKETS_PER_LINE, 1);

	/* Enable VBI packet transmission */
	REG_UPDATE_2(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_GC_SEND, 1,
		     HDMI_GC_CONT, 1);

	/* Disable Audio Content Protection packet transmission */
	REG_UPDATE(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_ACP_SEND, 0);
	/* following belongs to audio */
	/* Enable Audio InfoFrame packet transmission. */
	REG_UPDATE(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_AUDIO_INFO_SEND, 1);

	/* update double-buffered AUDIO_INFO registers immediately */
	ASSERT(enc->afmt);
	enc->afmt->funcs->audio_info_immediate_update(enc->afmt);

	/* Select line number on which to send Audio InfoFrame packets */
	REG_UPDATE(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_AUDIO_INFO_LINE,
		   VBI_LINE_0 + 2);

	/* Clear AVMUTE */
	REG_UPDATE(HDMI_TB_ENC_GC_CONTROL, HDMI_GC_AVMUTE, 0);
}

struct frl_audio_clock_info {
	uint32_t frl_lane_rate;
	/* N - 32KHz audio */
	uint32_t n_32khz;
	/* CTS - 32KHz audio*/
	uint32_t cts_32khz;
	uint32_t n_44khz;
	uint32_t cts_44khz;
	uint32_t n_48khz;
	uint32_t cts_48khz;
};

/* Values set by the Windows driver seem to depend only on FRL rate*/
static const struct frl_audio_clock_info frl_audio_clock_info_table[5] = {
	{3,  4224, 171875, 5292, 156250, 5760, 156250},
	{6,  4032, 328125, 5292, 312500, 6048, 328125},
	{8,  4032, 437500, 3969, 312500, 6048, 437500},
	{10, 3456, 468750, 3969, 390625, 5184, 468750},
	{12, 3072, 500000, 3969, 468750, 4752, 515625},
};

static void get_frl_audio_clock_info(
	struct frl_audio_clock_info *audio_clock_info,
	uint8_t frl_rate)
{
	uint32_t index;
	uint32_t frl_lane_rates[] = { 3, 6, 8, 10, 12 };
	uint32_t frl_lane_rate;

	ASSERT(frl_rate >= 0 && frl_rate <= 6);

	frl_lane_rate = frl_lane_rates[frl_rate - 1];

	/* search for FRL rate in table */
	for (index = 0; index < sizeof(frl_lane_rates) / sizeof(uint32_t); index++)
		if (frl_audio_clock_info_table[index].frl_lane_rate == frl_lane_rate)
			*audio_clock_info = frl_audio_clock_info_table[index];

	/* Should never happen */
	*audio_clock_info = frl_audio_clock_info_table[0];
}

static void setup_hdmi_audio(struct hpo_hdmi_stream_encoder *enc,
			     const struct audio_crtc_info *crtc_info, uint8_t frl_rate)
{
	struct dcn30_hpo_hdmi_stream_encoder *enc3 =
		DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(enc);

	struct frl_audio_clock_info audio_clock_info = { 0 };

	/* Setup audio in AFMT - program AFMT block associated with HPO */
	ASSERT(enc->afmt);
	enc->afmt->funcs->setup_hdmi_audio(enc->afmt);

	REG_UPDATE_3(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_AUTO_SEND, 1,
		     HDMI_ACR_SOURCE, 0, HDMI_ACR_AUDIO_PRIORITY, 0);

	/* Program audio clock sample/regeneration parameters */
	get_frl_audio_clock_info(&audio_clock_info, frl_rate);

	REG_UPDATE(HDMI_TB_ENC_ACR_32_0, HDMI_ACR_CTS_32,
		   audio_clock_info.cts_32khz);
	REG_UPDATE(HDMI_TB_ENC_ACR_32_1, HDMI_ACR_N_32,
		   audio_clock_info.n_32khz);

	REG_UPDATE(HDMI_TB_ENC_ACR_44_0, HDMI_ACR_CTS_44,
		   audio_clock_info.cts_44khz);
	REG_UPDATE(HDMI_TB_ENC_ACR_44_1, HDMI_ACR_N_44,
		   audio_clock_info.n_44khz);

	REG_UPDATE(HDMI_TB_ENC_ACR_48_0, HDMI_ACR_CTS_48,
		   audio_clock_info.cts_48khz);
	REG_UPDATE(HDMI_TB_ENC_ACR_48_1, HDMI_ACR_N_48,
		   audio_clock_info.n_48khz);
}

static void dcn30_hpo_hdmi_stream_enc_hdmi_audio_setup(
	struct hpo_hdmi_stream_encoder *enc, unsigned int az_inst,
	struct audio_info *info, struct audio_crtc_info *audio_crtc_info,
	uint8_t frl_rate)
{
	setup_hdmi_audio(enc, audio_crtc_info, frl_rate);
	ASSERT(enc->afmt);
	enc->afmt->funcs->se_audio_setup(enc->afmt, az_inst, info);
}

static void
dcn30_hpo_hdmi_stream_enc_hdmi_audio_enable(struct hpo_hdmi_stream_encoder *enc)
{
	enc->afmt->funcs->audio_mute_control(enc->afmt, false);
}

static void dcn30_hpo_hdmi_stream_enc_hdmi_audio_disable(
	struct hpo_hdmi_stream_encoder *enc)
{
	if (enc->afmt && enc->afmt->funcs->afmt_powerdown)
		enc->afmt->funcs->afmt_powerdown(enc->afmt);
}

static const struct hpo_hdmi_stream_encoder_funcs
	dcn30_hpo_hdmi_stream_enc_funcs = {
		.enable = dcn30_hpo_hdmi_stream_enc_enable,
		.disable = dcn30_hpo_hdmi_stream_enc_disable,
		.setup_stream_attribute =
			dcn30_hpo_hdmi_stream_enc_setup_stream_attribute,
		.update_hdmi_info_packets =
			dcn30_hpo_hdmi_stream_enc_update_info_packets,
		.stop_hdmi_info_packets =
			dcn30_hpo_hdmi_stream_enc_stop_info_packets,
		.hdmi_audio_setup = dcn30_hpo_hdmi_stream_enc_hdmi_audio_setup,
		.hdmi_audio_enable =
			dcn30_hpo_hdmi_stream_enc_hdmi_audio_enable,
		.hdmi_audio_disable =
			dcn30_hpo_hdmi_stream_enc_hdmi_audio_disable,
	};

void dcn30_hpo_hdmi_stream_encoder_construct(
	struct dcn30_hpo_hdmi_stream_encoder *enc, struct dc_context *ctx,
	uint32_t inst, struct vpg *vpg, struct afmt *afmt,
	const struct dcn30_hpo_hdmi_stream_encoder_registers *regs,
	const struct dcn30_hpo_hdmi_stream_encoder_shift *shift,
	const struct dcn30_hpo_hdmi_stream_encoder_mask *mask)
{
	enc->base.funcs = &dcn30_hpo_hdmi_stream_enc_funcs;
	enc->base.ctx = ctx;
	enc->base.inst = inst;
	enc->base.vpg = vpg;
	enc->base.afmt = afmt;
	enc->regs = regs;
	enc->hpo_se_shift = shift;
	enc->hpo_se_mask = mask;
}
