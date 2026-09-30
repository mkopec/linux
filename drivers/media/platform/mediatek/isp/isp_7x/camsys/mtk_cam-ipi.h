/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_IPI_H__
#define __MTK_CAM_IPI_H__

#define MTK_CAM_IPI_VERSION_MAJOR (0)
#define MTK_CAM_IPI_VERSION_MINOR (1)

#include <linux/types.h>
#include "mtk_cam-defs.h"

#define MTK_CAM_MAX_RUNNING_JOBS (3)
#define CAM_MAX_PLANENUM (3)
#define CAM_MAX_SUBSAMPLE (32)
#define MTK_CAM_MAX_PROCESSING_BUFS (2)

/*
 * struct mtkcam_ipi_point - Point
 *
 * @x: x-coordinate of the point (zero-based).
 * @y: y-coordinate of the point (zero-based).
 */
struct mtkcam_ipi_point {
	u16 x;
	u16 y;
} __packed;

/*
 * struct mtkcam_ipi_size - Size
 *
 * @w: width (in pixels).
 * @h: height (in pixels).
 */
struct mtkcam_ipi_size {
	u16 w;
	u16 h;
} __packed;

/*
 * struct mtkcam_ipi_fract - fraction
 *
 * @numerator: numerator part of the fraction.
 * @denominator: denominator part of the fraction.
 */
struct mtkcam_ipi_fract {
	u8 numerator;
	u8 denominator;
};

/*
 * struct mtkcam_ipi_sw_buffer
 *	- Shared buffer between cam-device and co-processor.
 *
 * @iova: DMA address for CAM DMA device. isp7_1: u64.
 * @scp_addr: SCP address for external co-processor unit.
 * @size: buffer size.
 */
struct mtkcam_ipi_sw_buffer {
	u64 iova;
	u32 scp_addr;
	u32 size;
} __packed;

/*
 * struct mtkcam_ipi_hw_buffer - DMA buffer for CAM DMA device.
 *
 * @iova: DMA address for CAM DMA device. isp7_1: u64.
 * @size: buffer size.
 */
struct mtkcam_ipi_hw_buffer {
	u64 iova;
	u32 size;
} __packed;

struct mtkcam_ipi_pix_fmt {
	u32			format;
	struct mtkcam_ipi_size	s;
	u16			stride[CAM_MAX_PLANENUM];
} __packed;

struct mtkcam_ipi_crop {
	struct mtkcam_ipi_point p;
	struct mtkcam_ipi_size s;
} __packed;

struct mtkcam_ipi_uid {
	u8 pipe_id;
	u8 id;
} __packed;

struct mtkcam_ipi_img_input {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_pix_fmt	fmt;
	struct mtkcam_ipi_sw_buffer	buf[CAM_MAX_PLANENUM];
} __packed;

struct mtkcam_ipi_img_output {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_pix_fmt	fmt;
	struct mtkcam_ipi_sw_buffer	buf[CAM_MAX_SUBSAMPLE][CAM_MAX_PLANENUM];
	struct mtkcam_ipi_crop		crop;
} __packed;

struct mtkcam_ipi_meta_input {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_sw_buffer	buf;
} __packed;

struct mtkcam_ipi_meta_output {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_sw_buffer	buf;
} __packed;

struct mtkcam_ipi_input_param {
	u32	fmt;
	u8	raw_pixel_id;
	u8	data_pattern;
	u8	pixel_mode;
	u8	subsample;
	struct mtkcam_ipi_crop in_crop;
} __packed;

struct mtkcam_ipi_raw_frame_param {
	u8	imgo_path_sel; /* mtkcam_ipi_raw_path_control */
	u8	hardware_scenario;
	u32	bin_flag;
	u8    exposure_num;
	u8    previous_exposure_num;
	struct mtkcam_ipi_fract	frz_ratio;
} __packed;

struct mtkcam_ipi_session_cookie {
	u8 session_id;
	u32 frame_no;
} __packed;

struct mtkcam_ipi_session_param {
	struct mtkcam_ipi_sw_buffer workbuf;
	struct mtkcam_ipi_sw_buffer msg_buf;
	struct mtkcam_ipi_sw_buffer raw_workbuf;
	struct mtkcam_ipi_sw_buffer priv_workbuf;
	struct mtkcam_ipi_sw_buffer session_buf;
} __packed;

struct mtkcam_ipi_hw_mapping {
	u8 pipe_id; /* ref. to mtkcam_pipe_subdev */
	u16 dev_mask; /* ref. to mtkcam_pipe_dev */
	u8 exp_order;
} __packed;

/*  Control flags of CAM_CMD_CONFIG */
#define MTK_CAM_IPI_CONFIG_TYPE_INIT			0x0001
#define MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE		0x0002
#define MTK_CAM_IPI_CONFIG_TYPE_EXEC_TWICE		0x0004
#define MTK_CAM_IPI_CONFIG_TYPE_SMVR_PREVIEW		0x0008

struct mtkcam_ipi_config_param {
	u8 flags;
	struct mtkcam_ipi_input_param	input;
	u8 n_maps;
	/* maximum # of pipes per stream */
	struct mtkcam_ipi_hw_mapping maps[6];
	/* sub_ratio:8, valid number: 8 */
	u16 valid_numbers[MTKCAM_IPI_FBCX_LAST];
	u8	sw_feature;
} __packed;

#define CAM_MAX_IMAGE_INPUT	(5)
#define CAM_MAX_IMAGE_OUTPUT	(15)
#define CAM_MAX_META_OUTPUT	(4)
#define CAM_MAX_PIPE_USED	(4)

struct mtkcam_ipi_frame_param {
	u32 cur_workbuf_offset;
	u32 cur_workbuf_size;

	struct mtkcam_ipi_raw_frame_param raw_param;
	struct mtkcam_ipi_img_input img_ins[CAM_MAX_IMAGE_INPUT];
	struct mtkcam_ipi_img_output img_outs[CAM_MAX_IMAGE_OUTPUT];
	struct mtkcam_ipi_meta_output meta_outputs[CAM_MAX_META_OUTPUT];
	struct mtkcam_ipi_meta_input meta_inputs[CAM_MAX_PIPE_USED];
} __packed;

struct mtkcam_ipi_frame_info {
	u32	cur_msgbuf_offset;
	u32	cur_msgbuf_size;
} __packed;

struct mtkcam_ipi_frame_ack_result {
	u32 cq_desc_offset;
	u32 sub_cq_desc_offset;
	u32 cq_desc_size;
	u32 sub_cq_desc_size;
} __packed;

struct mtkcam_ipi_ack_info {
	u8 ack_cmd_id;
	s32 ret;
	struct mtkcam_ipi_frame_ack_result frame_result;
} __packed;

/*
 * The IPI command enumeration.
 */
enum mtkcam_ipi_cmds {
	/* request for a new streaming: mtkcam_ipi_session_param */
	CAM_CMD_CREATE_SESSION,
	/* config the stream: mtkcam_ipi_config_param */
	CAM_CMD_CONFIG,
	/* per-frame: mtkcam_ipi_frame_param */
	CAM_CMD_FRAME,
	/* release certain streaming: mtkcam_ipi_session_param */
	CAM_CMD_DESTROY_SESSION,
	/* ack: mtkcam_ipi_ack_info */
	CAM_CMD_ACK,
	CAM_CMD_RESERVED,
};

struct mtkcam_ipi_event  {
	struct mtkcam_ipi_session_cookie cookie;
	u8 cmd_id;
	union {
		struct mtkcam_ipi_session_param	session_data;
		struct mtkcam_ipi_config_param	config_data;
		struct mtkcam_ipi_frame_info	frame_data;
		struct mtkcam_ipi_ack_info	ack_data;
	};
} __packed;

#endif /* __MTK_CAM_IPI_H__ */
