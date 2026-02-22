/*
 * HPO HDMI Link Encoder
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#ifndef __DAL_DCN401_HPO_HDMI_LINK_ENCODER_H__
#define __DAL_DCN401_HPO_HDMI_LINK_ENCODER_H__

#include "link_encoder.h"

#define DCN3_0_HPO_HDMI_LINK_ENC_FROM_HPO_LINK_ENC(hpo_hdmi_link_encoder) \
	container_of(hpo_hdmi_link_encoder,                                   \
		     struct dcn30_hpo_hdmi_link_encoder, base)

#define DCN3_0_HPO_HDMI_LINK_ENC_REG_LIST()                    \
	SR(HDMI_LINK_ENC_CONTROL), SR(HDMI_LINK_ENC_CLK_CTRL),     \
	SR(HDMI_FRL_ENC_CONFIG), SR(HDMI_FRL_ENC_CONFIG2),         \
	SR(HDMI_FRL_ENC_METER_BUFFER_STATUS),                      \
	SR(HDMI_FRL_ENC_MEM_CTRL)

#define DCN3_0_HPO_HDMI_LINK_ENC_REGS          \
	uint32_t HDMI_LINK_ENC_CONTROL;            \
	uint32_t HDMI_LINK_ENC_CLK_CTRL;           \
	uint32_t HDMI_FRL_ENC_CONFIG;              \
	uint32_t HDMI_FRL_ENC_CONFIG2;             \
	uint32_t HDMI_FRL_ENC_METER_BUFFER_STATUS; \
	uint32_t HDMI_FRL_ENC_MEM_CTRL;

struct dcn30_hpo_hdmi_link_encoder_registers {
	DCN3_0_HPO_HDMI_LINK_ENC_REGS;
};

#define DCN3_0_HPO_HDMI_LINK_ENC_MASK_SH_LIST(mask_sh)                     \
	SE_SF(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_ENABLE, mask_sh),           \
	SE_SF(HDMI_LINK_ENC_CONTROL, HDMI_LINK_ENC_SOFT_RESET, mask_sh),       \
	SE_SF(HDMI_LINK_ENC_CLK_CTRL, HDMI_LINK_ENC_CLOCK_EN,                  \
		  mask_sh),                                                        \
	SE_SF(HDMI_LINK_ENC_CLK_CTRL,                                          \
		  HDMI_LINK_ENC_CLOCK_ON_HDMICHARCLK, mask_sh),                    \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE_COUNT, mask_sh),             \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_TRAINING_ENABLE,                  \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_SCRAMBLER_DISABLE,                \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE0_TRAINING_PATTERN,           \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE1_TRAINING_PATTERN,           \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE2_TRAINING_PATTERN,           \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG, HDMI_LINK_LANE3_TRAINING_PATTERN,           \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_MAX_JITTER_VALUE,                \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_JITTER_THRESHOLD,                \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_JITTER_CAL_EN, mask_sh),         \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_RC_COMPRESS_DISABLE,             \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_FRL_HDMISTREAMCLK_DB_SEL,             \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_MAX_JITTER_VALUE_RESET,          \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2, HDMI_LINK_JITTER_EXCEED_STATUS,            \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_CONFIG2,                                            \
		  HDMI_LINK_METER_BUFFER_OVERFLOW_STATUS, mask_sh),                \
	SE_SF(HDMI_FRL_ENC_METER_BUFFER_STATUS,                                \
		  HDMI_LINK_MAX_METER_BUFFER_LEVEL, mask_sh),                      \
	SE_SF(HDMI_FRL_ENC_METER_BUFFER_STATUS,                                \
		  HDMI_LINK_METER_BUFFER_MAX_LEVEL_RESET, mask_sh),                \
	SE_SF(HDMI_FRL_ENC_MEM_CTRL, METERBUFFER_MEM_PWR_DIS,                  \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_MEM_CTRL, METERBUFFER_MEM_PWR_FORCE,                \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_MEM_CTRL, METERBUFFER_MEM_PWR_STATE,                \
		  mask_sh),                                                        \
	SE_SF(HDMI_FRL_ENC_MEM_CTRL,                                           \
		  METERBUFFER_MEM_DEFAULT_MEM_LOW_POWER_STATE, mask_sh)

#define DCN3_0_HPO_HDMI_LINK_ENC_REG_FIELD_LIST(type) \
	type HDMI_LINK_ENC_ENABLE;                    \
	type HDMI_LINK_ENC_SOFT_RESET;                \
	type HDMI_LINK_ENC_CLOCK_EN;                  \
	type HDMI_LINK_ENC_CLOCK_ON_HDMICHARCLK;      \
	type HDMI_LINK_LANE_COUNT;                    \
	type HDMI_LINK_TRAINING_ENABLE;               \
	type HDMI_LINK_SCRAMBLER_DISABLE;             \
	type HDMI_LINK_LANE0_TRAINING_PATTERN;        \
	type HDMI_LINK_LANE1_TRAINING_PATTERN;        \
	type HDMI_LINK_LANE2_TRAINING_PATTERN;        \
	type HDMI_LINK_LANE3_TRAINING_PATTERN;        \
	type HDMI_LINK_MAX_JITTER_VALUE;              \
	type HDMI_LINK_JITTER_THRESHOLD;              \
	type HDMI_LINK_JITTER_CAL_EN;                 \
	type HDMI_LINK_RC_COMPRESS_DISABLE;           \
	type HDMI_FRL_HDMISTREAMCLK_DB_SEL;           \
	type HDMI_LINK_MAX_JITTER_VALUE_RESET;        \
	type HDMI_LINK_JITTER_EXCEED_STATUS;          \
	type HDMI_LINK_METER_BUFFER_OVERFLOW_STATUS;  \
	type HDMI_LINK_MAX_METER_BUFFER_LEVEL;        \
	type HDMI_LINK_METER_BUFFER_MAX_LEVEL_RESET;  \
	type METERBUFFER_MEM_PWR_DIS;                 \
	type METERBUFFER_MEM_PWR_FORCE;               \
	type METERBUFFER_MEM_PWR_STATE;               \
	type METERBUFFER_MEM_DEFAULT_MEM_LOW_POWER_STATE

struct dcn30_hpo_hdmi_link_encoder_shift {
	DCN3_0_HPO_HDMI_LINK_ENC_REG_FIELD_LIST(uint8_t);
};

struct dcn30_hpo_hdmi_link_encoder_mask {
	DCN3_0_HPO_HDMI_LINK_ENC_REG_FIELD_LIST(uint32_t);
};

struct dcn30_hpo_hdmi_link_encoder {
	struct hpo_hdmi_link_encoder base;

	const struct dcn30_hpo_hdmi_link_encoder_registers *regs;
	const struct dcn30_hpo_hdmi_link_encoder_shift *shift;
	const struct dcn30_hpo_hdmi_link_encoder_mask *mask;
};

void hpo_hdmi_link_encoder31_construct(
	struct dcn30_hpo_hdmi_link_encoder *enc3, struct dc_context *ctx,
	uint32_t inst, const struct dcn30_hpo_hdmi_link_encoder_registers *regs,
	const struct dcn30_hpo_hdmi_link_encoder_shift *shift,
	const struct dcn30_hpo_hdmi_link_encoder_mask *mask);

#endif /* __DAL_DCN401_HPO_HDMI_LINK_ENCODER_H__ */
