/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#ifndef _MTK_WPE_V4L2_VNODE_H_
#define _MTK_WPE_V4L2_VNODE_H_

#include "../mtk_imgsys-dev.h"
#include "../mtk_imgsys-formats.h"
#include "../mtk_imgsys-vnode_id.h"

#define MTK_WPE_OUTPUT_MIN_WIDTH	2U
#define MTK_WPE_OUTPUT_MIN_HEIGHT	2U
#define MTK_WPE_OUTPUT_MAX_WIDTH	18472U
#define MTK_WPE_OUTPUT_MAX_HEIGHT	13856U

#define MTK_WPE_MAP_OUTPUT_MIN_WIDTH	2U
#define MTK_WPE_MAP_OUTPUT_MIN_HEIGHT	2U
#define MTK_WPE_MAP_OUTPUT_MAX_WIDTH	640U
#define MTK_WPE_MAP_OUTPUT_MAX_HEIGHT	480U

#define MTK_WPE_PSP_OUTPUT_WIDTH	8U
#define MTK_WPE_PSP_OUTPUT_HEIGHT	33U

#define MTK_WPE_CAPTURE_MIN_WIDTH	2U
#define MTK_WPE_CAPTURE_MIN_HEIGHT	2U
#define MTK_WPE_CAPTURE_MAX_WIDTH	18472U
#define MTK_WPE_CAPTURE_MAX_HEIGHT	13856U

static const struct mtk_imgsys_dev_format wpe_wpei_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_nv21,
		.align = 1,
		.scan_align = 1,
	},
	/* Y8 bit */
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 192,
		.scan_align = 192,
	},
	/* Bayer 10 bit */
	{
		.fmt = &mtk_imgsys_format_sgbrg10,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p010p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p012p,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format wpe_veci_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_warp2p,
		.align = 8,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format wpe_pspi_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_y32,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format wpe_wpeo_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_nv12,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_nv21,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 192,
		.scan_align = 192,
	},
	{
		.fmt = &mtk_imgsys_format_sgbrg10,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p010p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p012p,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format wpe_msko_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
};

static const struct v4l2_frmsizeenum wpe_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum wpe_in_map_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_MAP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_MAP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_MAP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_MAP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum wpe_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc wpe_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WWPEI_OUT,
		.name = "WPEI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpei_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpei_fmts),
		.default_width = MTK_WPE_OUTPUT_MAX_WIDTH,
		.default_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.description = "WPE main image input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WVECI_OUT,
		.name = "VECI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_veci_fmts,
		.num_fmts = ARRAY_SIZE(wpe_veci_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_map_frmsizeenum,
		.description = "WarpMap input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WPSP_COEFI_OUT,
		.name = "PSPI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_pspi_fmts,
		.num_fmts = ARRAY_SIZE(wpe_pspi_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.description = "PSP coef. table input",
	},
	/* WPE_EIS Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WWPEO_CAPTURE,
		.name = "WPEO_E Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpeo_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpeo_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.description = "WPE image output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WMSKO_CAPTURE,
		.name = "MSKO_E Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_msko_fmts,
		.num_fmts = ARRAY_SIZE(wpe_msko_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.description = "WPE valid map output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTWPEI_OUT,
		.name = "WPEI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpei_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpei_fmts),
		.default_width = MTK_WPE_OUTPUT_MAX_WIDTH,
		.default_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.description = "WPE main image input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTVECI_OUT,
		.name = "VECI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_veci_fmts,
		.num_fmts = ARRAY_SIZE(wpe_veci_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_map_frmsizeenum,
		.description = "WarpMap input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTPSP_COEFI_OUT,
		.name = "PSPI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_pspi_fmts,
		.num_fmts = ARRAY_SIZE(wpe_pspi_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.description = "PSP coef. table input",
	},
	/* WPE_TNR Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTWPEO_CAPTURE,
		.name = "WPEO_T Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpeo_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpeo_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.description = "WPE image output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTMSKO_CAPTURE,
		.name = "MSKO_T Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_msko_fmts,
		.num_fmts = ARRAY_SIZE(wpe_msko_fmts),
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.description = "WPE valid map output",
	},
};

#endif /* _MTK_WPE_V4L2_VNODE_H_ */
