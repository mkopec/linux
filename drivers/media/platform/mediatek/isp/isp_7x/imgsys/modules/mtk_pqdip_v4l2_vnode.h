/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#ifndef _MTK_PQDIP_V4L2_VNODE_H_
#define _MTK_PQDIP_V4L2_VNODE_H_

#include "../mtk_imgsys-dev.h"
#include "../mtk_imgsys-formats.h"
#include "../mtk_imgsys-vnode_id.h"

#define MTK_PQDIP_OUTPUT_MIN_WIDTH		2U
#define MTK_PQDIP_OUTPUT_MIN_HEIGHT		2U
#define MTK_PQDIP_OUTPUT_MAX_WIDTH		5376U
#define MTK_PQDIP_OUTPUT_MAX_HEIGHT		4032U
#define MTK_PQDIP_CAPTURE_MIN_WIDTH		2U
#define MTK_PQDIP_CAPTURE_MIN_HEIGHT		2U
#define MTK_PQDIP_CAPTURE_MAX_WIDTH		5376U
#define MTK_PQDIP_CAPTURE_MAX_HEIGHT		4032U

static const struct mtk_imgsys_dev_format pqdip_pimgi_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format pqdip_wroto_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format pqdip_tccso_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
};

static const struct v4l2_frmsizeenum pqdip_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_PQDIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_PQDIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_PQDIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum pqdip_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_PQDIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_PQDIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_PQDIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_PQDIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc pqdip_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_PIMGI_OUT,
		.name = "PIMGI Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_pimgi_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_pimgi_fmts),
		.default_width = MTK_PQDIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_PQDIP_OUTPUT_MAX_HEIGHT,
		.frmsizeenum = &pqdip_in_frmsizeenum,
		.description = "Imgi image source",
	},
	/* Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WROT_A_CAPTURE,
		.name = "WROTO A Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_wroto_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_wroto_fmts),
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WROT_B_CAPTURE,
		.name = "WROTO B Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_wroto_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_wroto_fmts),
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TCCSO_A_CAPTURE,
		.name = "TCCSO A Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_tccso_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_tccso_fmts),
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.description = "Output tone curve statistics",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TCCSO_B_CAPTURE,
		.name = "TCCSO B Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_tccso_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_tccso_fmts),
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.description = "Output tone curve statistics",
	},
};

#endif /* _MTK_PQDIP_V4L2_VNODE_H_ */
