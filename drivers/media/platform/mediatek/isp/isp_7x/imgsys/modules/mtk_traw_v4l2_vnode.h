/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Shih-Fang chuang <shih-fang.chuang@mediatek.com>
 *
 */

#ifndef _MTK_TRAW_V4L2_VNODE_H_
#define _MTK_TRAW_V4L2_VNODE_H_

#include "../mtk_imgsys-dev.h"
#include "../mtk_imgsys-formats.h"
#include "../mtk_imgsys-vnode_id.h"

#define MTK_TRAW_OUTPUT_MIN_WIDTH		2U
#define MTK_TRAW_OUTPUT_MIN_HEIGHT		2U
#define MTK_TRAW_OUTPUT_MAX_WIDTH		5376U
#define MTK_TRAW_OUTPUT_MAX_HEIGHT		4032U
#define MTK_TRAW_CAPTURE_MIN_WIDTH		2U
#define MTK_TRAW_CAPTURE_MIN_HEIGHT		2U
#define MTK_TRAW_CAPTURE_MAX_WIDTH		5376U
#define MTK_TRAW_CAPTURE_MAX_HEIGHT		4032U

static const struct mtk_imgsys_dev_format traw_imgi_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_sgbrg10,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p012p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p010p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_warp2p,
		.align = 8,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_srggb10,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_sgrbg10,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format traw_metai_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_metai,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format traw_stato_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_stt,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format traw_yuvo_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p012p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p010p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_warp2p,
		.align = 8,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format traw_pdc_fmts[] = {
	/* Y8 bit */
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
};

static const struct v4l2_frmsizeenum traw_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_TRAW_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_TRAW_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_TRAW_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum traw_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_TRAW_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_TRAW_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_TRAW_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_TRAW_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc traw_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TIMGI_OUT,
		.name = "TIMGI Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_imgi_fmts,
		.num_fmts = ARRAY_SIZE(traw_imgi_fmts),
		.default_width = MTK_TRAW_OUTPUT_MAX_WIDTH,
		.default_height = MTK_TRAW_OUTPUT_MAX_HEIGHT,
		.frmsizeenum = &traw_in_frmsizeenum,
		.description = "Imgi image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_METAI_OUT,
		.name = "METAI Input",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 1,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_metai_fmts,
		.num_fmts = ARRAY_SIZE(traw_metai_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &traw_in_frmsizeenum,
		.description = "METAI image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_PDC_OUT,
		.name = "PDC Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_pdc_fmts,
		.num_fmts = ARRAY_SIZE(traw_pdc_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &traw_in_frmsizeenum,
		.description = "PDC image source",
	},
	/* Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TYUVO_CAPTURE,
		.name = "TYUVO Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "YUVO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TYUV2O_CAPTURE,
		.name = "TYUV2O Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "YUV2O output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TYUV3O_CAPTURE,
		.name = "TYUV3O Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "YUV3O output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TYUV4O_CAPTURE,
		.name = "TYUV4O Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "YUV4O output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TYUV5O_CAPTURE,
		.name = "TYUV5O Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "YUV5O output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_FEO_CAPTURE,
		.name = "FEO Output",
		.cap = V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.smem_alloc = 1,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_metai_fmts,
		.num_fmts = ARRAY_SIZE(traw_metai_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "FEO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TIMGO_CAPTURE,
		.name = "TIMGO Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_imgi_fmts,
		.num_fmts = ARRAY_SIZE(traw_imgi_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "TIMGO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMGSTATO_CAPTURE,
		.name = "IMGSTATO Output",
		.cap = V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.smem_alloc = 1,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_stato_fmts,
		.num_fmts = ARRAY_SIZE(traw_stato_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "IMGSTATO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_XTMEO_CAPTURE,
		.name = "XTMEO Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_pdc_fmts,
		.num_fmts = ARRAY_SIZE(traw_pdc_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "XTMEO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_XTFDO_CAPTURE,
		.name = "XTFDO Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_yuvo_fmts,
		.num_fmts = ARRAY_SIZE(traw_yuvo_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "XTFDO output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_XTADLDBGO_CAPTURE,
		.name = "XTDBGO Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = traw_imgi_fmts,
		.num_fmts = ARRAY_SIZE(traw_imgi_fmts),
		.default_width = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.default_height = MTK_TRAW_CAPTURE_MAX_WIDTH,
		.frmsizeenum = &traw_out_frmsizeenum,
		.description = "XTDBGO output",
	},
};

#endif /* _MTK_TRAW_V4L2_VNODE_H_ */
