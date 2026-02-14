/*
 * HPO HDMI Stream Encoder
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#ifndef __DAL_DCN401_HPO_HDMI_STREAM_ENCODER_H__
#define __DAL_DCN401_HPO_HDMI_STREAM_ENCODER_H__

#include "dcn30/dcn30_vpg.h"
#include "dcn30/dcn30_afmt.h"
#include "stream_encoder.h"
#include "dcn20/dcn20_stream_encoder.h"

/* ------------------------------------------------------------------------- */
/* Container macro                                                           */
/* ------------------------------------------------------------------------- */

#define DCN3_0_HPO_HDMI_STREAM_ENC_FROM_HPO_STREAM_ENC(hpo_enc) \
	container_of(hpo_enc, struct dcn30_hpo_hdmi_stream_encoder, base)

/* ------------------------------------------------------------------------- */
/* Register list                                                             */
/* ------------------------------------------------------------------------- */

#define DCN3_0_HPO_HDMI_STREAM_ENC_REG_LIST()                                 \
	/* HPO stream encoder - clock and FIFO control */                     \
	SR(HDMI_STREAM_ENC_CLOCK_CONTROL),                                    \
	SR(HDMI_STREAM_ENC_INPUT_MUX_CONTROL),                                \
	SR(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0),         \
	SR(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1),         \
	SR(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL2),         \
								              \
	/* TB encoder - control and pixel format */                           \
	SR(HDMI_TB_ENC_CONTROL), SR(HDMI_TB_ENC_PIXEL_FORMAT),                \
	SR(HDMI_TB_ENC_MODE), SR(HDMI_TB_ENC_DB_CONTROL),                     \
								              \
	/* TB encoder - packet control */                                     \
	SR(HDMI_TB_ENC_PACKET_CONTROL),                                       \
	SR(HDMI_TB_ENC_VBI_PACKET_CONTROL1),                                  \
	SR(HDMI_TB_ENC_VBI_PACKET_CONTROL2),                                  \
	SR(HDMI_TB_ENC_GC_CONTROL),                                           \
	SR(HDMI_TB_ENC_ACR_PACKET_CONTROL),                                   \
								              \
	/* TB encoder - generic packet control (SEND/CONT) */                 \
	SR(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0),                              \
	SR(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1),                              \
	SR(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2),                              \
								              \
	/* TB encoder - generic packet line numbers */                        \
	SR(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE),                               \
	SR(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE),                               \
	SR(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE),                               \
	SR(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE),                               \
	SR(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE),                               \
	SR(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE),                             \
	SR(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE),                             \
	SR(HDMI_TB_ENC_GENERIC_PACKET14_LINE),                                \
								              \
	/* TB encoder - timing */                                             \
	SR(HDMI_TB_ENC_H_ACTIVE_BLANK),                                       \
	SR(HDMI_TB_ENC_HC_ACTIVE_BLANK),                                      \
								              \
	/* TB encoder - ACR */                                                \
	SR(HDMI_TB_ENC_ACR_32_0), SR(HDMI_TB_ENC_ACR_32_1),                   \
	SR(HDMI_TB_ENC_ACR_44_0), SR(HDMI_TB_ENC_ACR_44_1),                   \
	SR(HDMI_TB_ENC_ACR_48_0), SR(HDMI_TB_ENC_ACR_48_1),                   \
	SR(HDMI_TB_ENC_ACR_STATUS_0), SR(HDMI_TB_ENC_ACR_STATUS_1),           \
								              \
	/* TB encoder - CRC */                                                \
	SR(HDMI_TB_ENC_CRC_CNTL), SR(HDMI_TB_ENC_CRC_RESULT_0),               \
	SR(HDMI_TB_ENC_CRC_RESULT_1),                                         \
								              \
	/* TB encoder - memory and buffer control */                          \
	SR(HDMI_TB_ENC_MEM_CTRL), SR(HDMI_TB_ENC_BUFFER_CONTROL),             \
								              \
	/* TB encoder - status */                                             \
	SR(HDMI_TB_ENC_INPUT_FIFO_STATUS)

/* ------------------------------------------------------------------------- */
/* Register storage                                                          */
/* ------------------------------------------------------------------------- */

#define DCN3_0_HPO_HDMI_STREAM_ENC_REGS                                    \
	/* HPO stream encoder */                                           \
	uint32_t HDMI_STREAM_ENC_CLOCK_CONTROL;                            \
	uint32_t HDMI_STREAM_ENC_INPUT_MUX_CONTROL;                        \
	uint32_t HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0; \
	uint32_t HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1; \
	uint32_t HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL2; \
	/* TB encoder - control */                                         \
	uint32_t HDMI_TB_ENC_CONTROL;                                      \
	uint32_t HDMI_TB_ENC_PIXEL_FORMAT;                                 \
	uint32_t HDMI_TB_ENC_MODE;                                         \
	uint32_t HDMI_TB_ENC_DB_CONTROL;                                   \
	/* TB encoder - packets */                                         \
	uint32_t HDMI_TB_ENC_PACKET_CONTROL;                               \
	uint32_t HDMI_TB_ENC_VBI_PACKET_CONTROL1;                          \
	uint32_t HDMI_TB_ENC_VBI_PACKET_CONTROL2;                          \
	uint32_t HDMI_TB_ENC_GC_CONTROL;                                   \
	uint32_t HDMI_TB_ENC_ACR_PACKET_CONTROL;                           \
	/* TB encoder - generic packet control */                          \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET_CONTROL0;                      \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET_CONTROL1;                      \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET_CONTROL2;                      \
	/* TB encoder - generic packet lines */                            \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET0_1_LINE;                       \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET2_3_LINE;                       \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET4_5_LINE;                       \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET6_7_LINE;                       \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET8_9_LINE;                       \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET10_11_LINE;                     \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET12_13_LINE;                     \
	uint32_t HDMI_TB_ENC_GENERIC_PACKET14_LINE;                        \
	/* TB encoder - timing */                                          \
	uint32_t HDMI_TB_ENC_H_ACTIVE_BLANK;                               \
	uint32_t HDMI_TB_ENC_HC_ACTIVE_BLANK;                              \
	/* TB encoder - ACR */                                             \
	uint32_t HDMI_TB_ENC_ACR_32_0;                                     \
	uint32_t HDMI_TB_ENC_ACR_32_1;                                     \
	uint32_t HDMI_TB_ENC_ACR_44_0;                                     \
	uint32_t HDMI_TB_ENC_ACR_44_1;                                     \
	uint32_t HDMI_TB_ENC_ACR_48_0;                                     \
	uint32_t HDMI_TB_ENC_ACR_48_1;                                     \
	uint32_t HDMI_TB_ENC_ACR_STATUS_0;                                 \
	uint32_t HDMI_TB_ENC_ACR_STATUS_1;                                 \
	/* TB encoder - CRC */                                             \
	uint32_t HDMI_TB_ENC_CRC_CNTL;                                     \
	uint32_t HDMI_TB_ENC_CRC_RESULT_0;                                 \
	uint32_t HDMI_TB_ENC_CRC_RESULT_1;                                 \
	/* TB encoder - memory and buffer */                               \
	uint32_t HDMI_TB_ENC_MEM_CTRL;                                     \
	uint32_t HDMI_TB_ENC_BUFFER_CONTROL;                               \
	/* TB encoder - status */                                          \
	uint32_t HDMI_TB_ENC_INPUT_FIFO_STATUS;

struct dcn30_hpo_hdmi_stream_encoder_registers {
	DCN3_0_HPO_HDMI_STREAM_ENC_REGS;
};

/* ------------------------------------------------------------------------- */
/* Field list                                                                */
/* ------------------------------------------------------------------------- */

#define DCN3_0_HPO_HDMI_STREAM_ENC_MASK_SH_LIST(mask_sh)                           \
	/* HDMI_STREAM_ENC_CLOCK_CONTROL */                                        \
	SE_SF(HDMI_STREAM_ENC_CLOCK_CONTROL, HDMI_STREAM_ENC_CLOCK_EN,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_STREAM_ENC_CLOCK_CONTROL,                                       \
	      HDMI_STREAM_ENC_CLOCK_ON_DISPCLK, mask_sh),                          \
	SE_SF(HDMI_STREAM_ENC_CLOCK_CONTROL,                                       \
	      HDMI_STREAM_ENC_CLOCK_ON_SOCCLK, mask_sh),                           \
	SE_SF(HDMI_STREAM_ENC_CLOCK_CONTROL,                                       \
	      HDMI_STREAM_ENC_CLOCK_ON_HDMISTREAMCLK, mask_sh),                    \
									           \
	/* HDMI_STREAM_ENC_INPUT_MUX_CONTROL */                                    \
	SE_SF(HDMI_STREAM_ENC_INPUT_MUX_CONTROL,                                   \
	      HDMI_STREAM_ENC_INPUT_MUX_SOURCE_SEL, mask_sh),                      \
									           \
	/* HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0 */             \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_ENABLE, mask_sh),                                               \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_RESET, mask_sh),                                                \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_PIXEL_ENCODING_TYPE, mask_sh),                                  \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_RESET_DONE, mask_sh),                                           \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_VIDEO_STREAM_ACTIVE, mask_sh),                                  \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL0,            \
	      FIFO_ERROR, mask_sh),                                                \
									           \
	/* HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1 */             \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_USE_OVERWRITE_LEVEL, mask_sh),                                  \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_FORCE_RECAL_AVERAGE, mask_sh),                                  \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_FORCE_RECOMP_MINMAX, mask_sh),                                  \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_OVERWRITE_LEVEL, mask_sh),                                      \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_MINIMUM_LEVEL, mask_sh),                                        \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_MAXIMUM_LEVEL, mask_sh),                                        \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_CAL_AVERAGE_LEVEL, mask_sh),                                    \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL1,            \
	      FIFO_CALIBRATED, mask_sh),                                           \
									           \
	/* HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL2 */             \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL2,            \
	      FIFO_READ_START_LEVEL, mask_sh),                                     \
	SE_SF(HDMI_STREAM_ENC_CLOCK_RAMP_ADJUSTER_FIFO_STATUS_CONTROL2,            \
	      FIFO_READ_CLOCK_SRC, mask_sh),                                       \
									           \
	/* HDMI_TB_ENC_CONTROL */                                                  \
	SE_SF(HDMI_TB_ENC_CONTROL, HDMI_TB_ENC_EN, mask_sh),                       \
	SE_SF(HDMI_TB_ENC_CONTROL, HDMI_RESET, mask_sh),                           \
	SE_SF(HDMI_TB_ENC_CONTROL, HDMI_RESET_DONE, mask_sh),                      \
									           \
	/* HDMI_TB_ENC_PIXEL_FORMAT */                                             \
	SE_SF(HDMI_TB_ENC_PIXEL_FORMAT, HDMI_DEEP_COLOR_ENABLE,                    \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_PIXEL_FORMAT, HDMI_DEEP_COLOR_DEPTH,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_PIXEL_FORMAT, HDMI_PIXEL_ENCODING, mask_sh),             \
	SE_SF(HDMI_TB_ENC_PIXEL_FORMAT, HDMI_DSC_MODE, mask_sh),                   \
									           \
	/* HDMI_TB_ENC_MODE */                                                     \
	SE_SF(HDMI_TB_ENC_MODE, HDMI_BORROW_MODE, mask_sh),                        \
	SE_SF(HDMI_TB_ENC_MODE, HDMI_SKIP_FIRST_HBLANK, mask_sh),                  \
									           \
	/* HDMI_TB_ENC_DB_CONTROL */                                               \
	SE_SF(HDMI_TB_ENC_DB_CONTROL, HDMI_DB_PENDING, mask_sh),                   \
	SE_SF(HDMI_TB_ENC_DB_CONTROL, HDMI_DB_DISABLE, mask_sh),                   \
	SE_SF(HDMI_TB_ENC_DB_CONTROL, VUPDATE_DB_PENDING, mask_sh),                \
									           \
	/* HDMI_TB_ENC_PACKET_CONTROL */                                           \
	SE_SF(HDMI_TB_ENC_PACKET_CONTROL, HDMI_MAX_PACKETS_PER_LINE,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_PACKET_CONTROL, HDMI_MAX_ISLANDS_PER_LINE,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_PACKET_CONTROL, HDMI_ACR_AUTO_SEND_OVERFLOW,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_PACKET_CONTROL,                                          \
	      HDMI_TB_ENC_PACKET_ERROR_CLEAR, mask_sh),                            \
									           \
	/* HDMI_TB_ENC_VBI_PACKET_CONTROL1 */                                      \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_GC_SEND, mask_sh),             \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_GC_CONT, mask_sh),             \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_ISRC_SEND,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_ISRC_CONT,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_ACP_SEND,                      \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_AUDIO_INFO_SEND,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_AUDIO_INFO_CONT,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL1, HDMI_AUDIO_INFO_LINE,               \
	      mask_sh),                                                            \
									           \
	/* HDMI_TB_ENC_VBI_PACKET_CONTROL2 */                                      \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL2, HDMI_ISRC_LINE,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_VBI_PACKET_CONTROL2, HDMI_ACP_LINE,                      \
	      mask_sh),                                                            \
									           \
	/* HDMI_TB_ENC_GC_CONTROL */                                               \
	SE_SF(HDMI_TB_ENC_GC_CONTROL, HDMI_GC_AVMUTE, mask_sh),                    \
	SE_SF(HDMI_TB_ENC_GC_CONTROL, HDMI_GC_AVMUTE_CONT, mask_sh),               \
	SE_SF(HDMI_TB_ENC_GC_CONTROL, HDMI_DEFAULT_PHASE, mask_sh),                \
									           \
	/* HDMI_TB_ENC_ACR_PACKET_CONTROL */                                       \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_SEND, mask_sh),             \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_CONT, mask_sh),             \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_SELECT,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_SOURCE,                     \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_AUTO_SEND,                  \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_N_MULTIPLE,                 \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_ACR_PACKET_CONTROL, HDMI_ACR_AUDIO_PRIORITY,             \
	      mask_sh),                                                            \
									           \
	/* HDMI_TB_ENC_GENERIC_PACKET_CONTROL0 - packets 0-7 SEND/CONT */          \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC0_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC0_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC0_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC0_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC1_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC1_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC1_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC1_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC2_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC2_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC2_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC2_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC3_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC3_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC3_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC3_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC4_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC4_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC4_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC4_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC5_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC5_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC5_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC5_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC6_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC6_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC6_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC6_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC7_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0, HDMI_GENERIC7_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC7_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL0,                                 \
	      HDMI_GENERIC7_LINE_REFERENCE, mask_sh),                              \
									           \
	/* HDMI_TB_ENC_GENERIC_PACKET_CONTROL1 - packets 8-14 SEND/CONT */         \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC8_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC8_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC8_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC8_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC9_SEND,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1, HDMI_GENERIC9_CONT,             \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC9_LOCK_EN, mask_sh),                                     \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC9_LINE_REFERENCE, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC10_SEND, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC10_CONT, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC10_LOCK_EN, mask_sh),                                    \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC10_LINE_REFERENCE, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC11_SEND, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC11_CONT, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC11_LOCK_EN, mask_sh),                                    \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC11_LINE_REFERENCE, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC12_SEND, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC12_CONT, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC12_LOCK_EN, mask_sh),                                    \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC12_LINE_REFERENCE, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC13_SEND, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC13_CONT, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC13_LOCK_EN, mask_sh),                                    \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC13_LINE_REFERENCE, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC14_SEND, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC14_CONT, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC14_LOCK_EN, mask_sh),                                    \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL1,                                 \
	      HDMI_GENERIC14_LINE_REFERENCE, mask_sh),                             \
									           \
	/* HDMI_TB_ENC_GENERIC_PACKET_CONTROL2 - immediate send */                 \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC0_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC1_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC2_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC3_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC4_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC5_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC6_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC7_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC8_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC9_IMMEDIATE_SEND, mask_sh),                              \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC10_IMMEDIATE_SEND, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC11_IMMEDIATE_SEND, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC12_IMMEDIATE_SEND, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC13_IMMEDIATE_SEND, mask_sh),                             \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET_CONTROL2,                                 \
	      HDMI_GENERIC14_IMMEDIATE_SEND, mask_sh),                             \
									           \
	/* HDMI_TB_ENC_GENERIC_PACKET line registers */                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE, HDMI_GENERIC0_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE, HDMI_GENERIC0_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE, HDMI_GENERIC1_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET0_1_LINE, HDMI_GENERIC1_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE, HDMI_GENERIC2_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE, HDMI_GENERIC2_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE, HDMI_GENERIC3_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET2_3_LINE, HDMI_GENERIC3_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE, HDMI_GENERIC4_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE, HDMI_GENERIC4_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE, HDMI_GENERIC5_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET4_5_LINE, HDMI_GENERIC5_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE, HDMI_GENERIC6_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE, HDMI_GENERIC6_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE, HDMI_GENERIC7_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET6_7_LINE, HDMI_GENERIC7_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE, HDMI_GENERIC8_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE, HDMI_GENERIC8_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE, HDMI_GENERIC9_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET8_9_LINE, HDMI_GENERIC9_EMP,               \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,                                \
	      HDMI_GENERIC10_LINE, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,                                \
	      HDMI_GENERIC10_EMP, mask_sh),                                        \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,                                \
	      HDMI_GENERIC11_LINE, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET10_11_LINE,                                \
	      HDMI_GENERIC11_EMP, mask_sh),                                        \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,                                \
	      HDMI_GENERIC12_LINE, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,                                \
	      HDMI_GENERIC12_EMP, mask_sh),                                        \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,                                \
	      HDMI_GENERIC13_LINE, mask_sh),                                       \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET12_13_LINE,                                \
	      HDMI_GENERIC13_EMP, mask_sh),                                        \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET14_LINE, HDMI_GENERIC14_LINE,              \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_GENERIC_PACKET14_LINE, HDMI_GENERIC14_EMP,               \
	      mask_sh),                                                            \
									           \
	/* HDMI_TB_ENC_H_ACTIVE_BLANK */                                           \
	SE_SF(HDMI_TB_ENC_H_ACTIVE_BLANK, HDMI_H_ACTIVE, mask_sh),                 \
	SE_SF(HDMI_TB_ENC_H_ACTIVE_BLANK, HDMI_H_BLANK, mask_sh),                  \
									           \
	/* HDMI_TB_ENC_HC_ACTIVE_BLANK */                                          \
	SE_SF(HDMI_TB_ENC_HC_ACTIVE_BLANK, HDMI_HC_ACTIVE, mask_sh),               \
	SE_SF(HDMI_TB_ENC_HC_ACTIVE_BLANK, HDMI_HC_BLANK, mask_sh),                \
									           \
	/* HDMI_TB_ENC_ACR registers */                                            \
	SE_SF(HDMI_TB_ENC_ACR_32_0, HDMI_ACR_CTS_32, mask_sh),                     \
	SE_SF(HDMI_TB_ENC_ACR_32_1, HDMI_ACR_N_32, mask_sh),                       \
	SE_SF(HDMI_TB_ENC_ACR_44_0, HDMI_ACR_CTS_44, mask_sh),                     \
	SE_SF(HDMI_TB_ENC_ACR_44_1, HDMI_ACR_N_44, mask_sh),                       \
	SE_SF(HDMI_TB_ENC_ACR_48_0, HDMI_ACR_CTS_48, mask_sh),                     \
	SE_SF(HDMI_TB_ENC_ACR_48_1, HDMI_ACR_N_48, mask_sh),                       \
	SE_SF(HDMI_TB_ENC_ACR_STATUS_0, HDMI_ACR_CTS, mask_sh),                    \
	SE_SF(HDMI_TB_ENC_ACR_STATUS_1, HDMI_ACR_N, mask_sh),                      \
									           \
	/* HDMI_TB_ENC_CRC_CNTL */                                                 \
	SE_SF(HDMI_TB_ENC_CRC_CNTL, HDMI_CRC_EN, mask_sh),                         \
	SE_SF(HDMI_TB_ENC_CRC_CNTL, HDMI_CRC_CONT_EN, mask_sh),                    \
	SE_SF(HDMI_TB_ENC_CRC_CNTL, HDMI_CRC_TYPE, mask_sh),                       \
	SE_SF(HDMI_TB_ENC_CRC_CNTL, HDMI_CRC_SRC_SEL, mask_sh),                    \
									           \
	/* HDMI_TB_ENC_MEM_CTRL */                                                 \
	SE_SF(HDMI_TB_ENC_MEM_CTRL, BORROWBUFFER_MEM_PWR_DIS,                      \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_MEM_CTRL, BORROWBUFFER_MEM_PWR_FORCE,                    \
	      mask_sh),                                                            \
	SE_SF(HDMI_TB_ENC_MEM_CTRL, BORROWBUFFER_MEM_PWR_STATE,                    \
	      mask_sh),                                                            \
									           \
	/* HDMI_TB_ENC_INPUT_FIFO_STATUS */                                        \
	SE_SF(HDMI_TB_ENC_INPUT_FIFO_STATUS, INPUT_FIFO_ERROR,                     \
	      mask_sh)

/* ------------------------------------------------------------------------- */
/* Register field list for shift/mask structs                                */
/* ------------------------------------------------------------------------- */

#define DCN3_0_HPO_HDMI_STREAM_ENC_REG_FIELD_LIST(type)          \
	/* Stream encoder clock/FIFO */                          \
	type HDMI_STREAM_ENC_CLOCK_EN;                           \
	type HDMI_STREAM_ENC_CLOCK_ON_DISPCLK;                   \
	type HDMI_STREAM_ENC_CLOCK_ON_SOCCLK;                    \
	type HDMI_STREAM_ENC_CLOCK_ON_HDMISTREAMCLK;             \
	type HDMI_STREAM_ENC_INPUT_MUX_SOURCE_SEL;               \
	type FIFO_ENABLE;                                        \
	type FIFO_RESET;                                         \
	type FIFO_PIXEL_ENCODING_TYPE;                           \
	type FIFO_UNCOMPRESSED_PIXEL_FORMAT;                     \
	type FIFO_COMPRESSED_PIXEL_FORMAT;                       \
	type FIFO_RESET_DONE;                                    \
	type FIFO_VIDEO_STREAM_ACTIVE;                           \
	type FIFO_ERROR;                                         \
	type FIFO_USE_OVERWRITE_LEVEL;                           \
	type FIFO_FORCE_RECAL_AVERAGE;                           \
	type FIFO_FORCE_RECOMP_MINMAX;                           \
	type FIFO_OVERWRITE_LEVEL;                               \
	type FIFO_MINIMUM_LEVEL;                                 \
	type FIFO_MAXIMUM_LEVEL;                                 \
	type FIFO_CAL_AVERAGE_LEVEL;                             \
	type FIFO_CALIBRATED;                                    \
	type FIFO_READ_START_LEVEL;                              \
	type FIFO_READ_CLOCK_SRC;                                \
	/* TB encoder control */                                 \
	type HDMI_TB_ENC_EN;                                     \
	type HDMI_RESET;                                         \
	type HDMI_RESET_DONE;                                    \
	/* TB encoder pixel format */                            \
	type HDMI_DEEP_COLOR_ENABLE;                             \
	type HDMI_DEEP_COLOR_DEPTH;                              \
	type HDMI_PIXEL_ENCODING;                                \
	type HDMI_DSC_MODE;                                      \
	/* TB encoder mode */                                    \
	type HDMI_BORROW_MODE;                                   \
	type HDMI_SKIP_FIRST_HBLANK;                             \
	/* TB encoder DB control */                              \
	type HDMI_DB_PENDING;                                    \
	type HDMI_DB_DISABLE;                                    \
	type VUPDATE_DB_PENDING;                                 \
	/* TB encoder packet control */                          \
	type HDMI_MAX_PACKETS_PER_LINE;                          \
	type HDMI_MAX_ISLANDS_PER_LINE;                          \
	type HDMI_ACR_AUTO_SEND_OVERFLOW;                        \
	type HDMI_TB_ENC_PACKET_ERROR_CLEAR;                     \
	/* TB encoder VBI packet control */                      \
	type HDMI_GC_SEND;                                       \
	type HDMI_GC_CONT;                                       \
	type HDMI_ISRC_SEND;                                     \
	type HDMI_ISRC_CONT;                                     \
	type HDMI_ACP_SEND;                                      \
	type HDMI_AUDIO_INFO_SEND;                               \
	type HDMI_AUDIO_INFO_CONT;                               \
	type HDMI_AUDIO_INFO_LINE;                               \
	type HDMI_ISRC_LINE;                                     \
	type HDMI_ACP_LINE;                                      \
	/* TB encoder GC control */                              \
	type HDMI_GC_AVMUTE;                                     \
	type HDMI_GC_AVMUTE_CONT;                                \
	type HDMI_DEFAULT_PHASE;                                 \
	/* TB encoder ACR packet control */                      \
	type HDMI_ACR_SEND;                                      \
	type HDMI_ACR_CONT;                                      \
	type HDMI_ACR_SELECT;                                    \
	type HDMI_ACR_SOURCE;                                    \
	type HDMI_ACR_AUTO_SEND;                                 \
	type HDMI_ACR_N_MULTIPLE;                                \
	type HDMI_ACR_AUDIO_PRIORITY;                            \
	/* Generic packet SEND/CONT - packets 0-7 (CONTROL0) */  \
	type HDMI_GENERIC0_SEND;                                 \
	type HDMI_GENERIC0_CONT;                                 \
	type HDMI_GENERIC0_LOCK_EN;                              \
	type HDMI_GENERIC0_LINE_REFERENCE;                       \
	type HDMI_GENERIC1_SEND;                                 \
	type HDMI_GENERIC1_CONT;                                 \
	type HDMI_GENERIC1_LOCK_EN;                              \
	type HDMI_GENERIC1_LINE_REFERENCE;                       \
	type HDMI_GENERIC2_SEND;                                 \
	type HDMI_GENERIC2_CONT;                                 \
	type HDMI_GENERIC2_LOCK_EN;                              \
	type HDMI_GENERIC2_LINE_REFERENCE;                       \
	type HDMI_GENERIC3_SEND;                                 \
	type HDMI_GENERIC3_CONT;                                 \
	type HDMI_GENERIC3_LOCK_EN;                              \
	type HDMI_GENERIC3_LINE_REFERENCE;                       \
	type HDMI_GENERIC4_SEND;                                 \
	type HDMI_GENERIC4_CONT;                                 \
	type HDMI_GENERIC4_LOCK_EN;                              \
	type HDMI_GENERIC4_LINE_REFERENCE;                       \
	type HDMI_GENERIC5_SEND;                                 \
	type HDMI_GENERIC5_CONT;                                 \
	type HDMI_GENERIC5_LOCK_EN;                              \
	type HDMI_GENERIC5_LINE_REFERENCE;                       \
	type HDMI_GENERIC6_SEND;                                 \
	type HDMI_GENERIC6_CONT;                                 \
	type HDMI_GENERIC6_LOCK_EN;                              \
	type HDMI_GENERIC6_LINE_REFERENCE;                       \
	type HDMI_GENERIC7_SEND;                                 \
	type HDMI_GENERIC7_CONT;                                 \
	type HDMI_GENERIC7_LOCK_EN;                              \
	type HDMI_GENERIC7_LINE_REFERENCE;                       \
	/* Generic packet SEND/CONT - packets 8-14 (CONTROL1) */ \
	type HDMI_GENERIC8_SEND;                                 \
	type HDMI_GENERIC8_CONT;                                 \
	type HDMI_GENERIC8_LOCK_EN;                              \
	type HDMI_GENERIC8_LINE_REFERENCE;                       \
	type HDMI_GENERIC9_SEND;                                 \
	type HDMI_GENERIC9_CONT;                                 \
	type HDMI_GENERIC9_LOCK_EN;                              \
	type HDMI_GENERIC9_LINE_REFERENCE;                       \
	type HDMI_GENERIC10_SEND;                                \
	type HDMI_GENERIC10_CONT;                                \
	type HDMI_GENERIC10_LOCK_EN;                             \
	type HDMI_GENERIC10_LINE_REFERENCE;                      \
	type HDMI_GENERIC11_SEND;                                \
	type HDMI_GENERIC11_CONT;                                \
	type HDMI_GENERIC11_LOCK_EN;                             \
	type HDMI_GENERIC11_LINE_REFERENCE;                      \
	type HDMI_GENERIC12_SEND;                                \
	type HDMI_GENERIC12_CONT;                                \
	type HDMI_GENERIC12_LOCK_EN;                             \
	type HDMI_GENERIC12_LINE_REFERENCE;                      \
	type HDMI_GENERIC13_SEND;                                \
	type HDMI_GENERIC13_CONT;                                \
	type HDMI_GENERIC13_LOCK_EN;                             \
	type HDMI_GENERIC13_LINE_REFERENCE;                      \
	type HDMI_GENERIC14_SEND;                                \
	type HDMI_GENERIC14_CONT;                                \
	type HDMI_GENERIC14_LOCK_EN;                             \
	type HDMI_GENERIC14_LINE_REFERENCE;                      \
	/* Generic packet immediate send (CONTROL2) */           \
	type HDMI_GENERIC0_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC1_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC2_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC3_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC4_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC5_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC6_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC7_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC8_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC9_IMMEDIATE_SEND;                       \
	type HDMI_GENERIC10_IMMEDIATE_SEND;                      \
	type HDMI_GENERIC11_IMMEDIATE_SEND;                      \
	type HDMI_GENERIC12_IMMEDIATE_SEND;                      \
	type HDMI_GENERIC13_IMMEDIATE_SEND;                      \
	type HDMI_GENERIC14_IMMEDIATE_SEND;                      \
	/* Generic packet line numbers */                        \
	type HDMI_GENERIC0_LINE;                                 \
	type HDMI_GENERIC0_EMP;                                  \
	type HDMI_GENERIC1_LINE;                                 \
	type HDMI_GENERIC1_EMP;                                  \
	type HDMI_GENERIC2_LINE;                                 \
	type HDMI_GENERIC2_EMP;                                  \
	type HDMI_GENERIC3_LINE;                                 \
	type HDMI_GENERIC3_EMP;                                  \
	type HDMI_GENERIC4_LINE;                                 \
	type HDMI_GENERIC4_EMP;                                  \
	type HDMI_GENERIC5_LINE;                                 \
	type HDMI_GENERIC5_EMP;                                  \
	type HDMI_GENERIC6_LINE;                                 \
	type HDMI_GENERIC6_EMP;                                  \
	type HDMI_GENERIC7_LINE;                                 \
	type HDMI_GENERIC7_EMP;                                  \
	type HDMI_GENERIC8_LINE;                                 \
	type HDMI_GENERIC8_EMP;                                  \
	type HDMI_GENERIC9_LINE;                                 \
	type HDMI_GENERIC9_EMP;                                  \
	type HDMI_GENERIC10_LINE;                                \
	type HDMI_GENERIC10_EMP;                                 \
	type HDMI_GENERIC11_LINE;                                \
	type HDMI_GENERIC11_EMP;                                 \
	type HDMI_GENERIC12_LINE;                                \
	type HDMI_GENERIC12_EMP;                                 \
	type HDMI_GENERIC13_LINE;                                \
	type HDMI_GENERIC13_EMP;                                 \
	type HDMI_GENERIC14_LINE;                                \
	type HDMI_GENERIC14_EMP;                                 \
	/* TB encoder timing */                                  \
	type HDMI_H_ACTIVE;                                      \
	type HDMI_H_BLANK;                                       \
	type HDMI_HC_ACTIVE;                                     \
	type HDMI_HC_BLANK;                                      \
	/* TB encoder ACR values */                              \
	type HDMI_ACR_CTS_32;                                    \
	type HDMI_ACR_N_32;                                      \
	type HDMI_ACR_CTS_44;                                    \
	type HDMI_ACR_N_44;                                      \
	type HDMI_ACR_CTS_48;                                    \
	type HDMI_ACR_N_48;                                      \
	type HDMI_ACR_CTS;                                       \
	type HDMI_ACR_N;                                         \
	/* TB encoder CRC */                                     \
	type HDMI_CRC_EN;                                        \
	type HDMI_CRC_CONT_EN;                                   \
	type HDMI_CRC_TYPE;                                      \
	type HDMI_CRC_SRC_SEL;                                   \
	/* TB encoder memory control */                          \
	type BORROWBUFFER_MEM_PWR_DIS;                           \
	type BORROWBUFFER_MEM_PWR_FORCE;                         \
	type BORROWBUFFER_MEM_PWR_STATE;                         \
	/* TB encoder status */                                  \
	type INPUT_FIFO_ERROR;

/* ------------------------------------------------------------------------- */
/* Shift / mask tables                                                       */
/* ------------------------------------------------------------------------- */

struct dcn30_hpo_hdmi_stream_encoder_shift {
	DCN3_0_HPO_HDMI_STREAM_ENC_REG_FIELD_LIST(uint8_t);
};

struct dcn30_hpo_hdmi_stream_encoder_mask {
	DCN3_0_HPO_HDMI_STREAM_ENC_REG_FIELD_LIST(uint32_t);
};

/* ------------------------------------------------------------------------- */
/* Main object                                                               */
/* ------------------------------------------------------------------------- */

struct dcn30_hpo_hdmi_stream_encoder {
	struct hpo_hdmi_stream_encoder base;

	const struct dcn30_hpo_hdmi_stream_encoder_registers *regs;
	const struct dcn30_hpo_hdmi_stream_encoder_shift *hpo_se_shift;
	const struct dcn30_hpo_hdmi_stream_encoder_mask *hpo_se_mask;
};

/* ------------------------------------------------------------------------- */
/* Constructor                                                               */
/* ------------------------------------------------------------------------- */

void dcn30_hpo_hdmi_stream_encoder_construct(
	struct dcn30_hpo_hdmi_stream_encoder *enc, struct dc_context *ctx,
	uint32_t inst, struct vpg *vpg, struct afmt *afmt,
	const struct dcn30_hpo_hdmi_stream_encoder_registers *regs,
	const struct dcn30_hpo_hdmi_stream_encoder_shift *shift,
	const struct dcn30_hpo_hdmi_stream_encoder_mask *mask);

#endif /* __DAL_DCN401_HPO_HDMI_STREAM_ENCODER_H__ */