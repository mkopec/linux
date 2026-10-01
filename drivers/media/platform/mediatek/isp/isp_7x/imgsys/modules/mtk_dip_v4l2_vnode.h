/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_V4L2_VNODE_H_
#define _MTK_DIP_V4L2_VNODE_H_

#include "../mtk_imgsys-dev.h"
#include "../mtk_imgsys-formats.h"
#include "../mtk_imgsys-vnode_id.h"

static const struct mtk_imgsys_dev_format dip_imgi_fmts[] = {
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
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_vipi_fmts[] = {
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
};

static const struct mtk_imgsys_dev_format dip_rec_dsi_fmts[] = {
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
		.fmt = &mtk_imgsys_format_2p012p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_rec_dpi_fmts[] = {
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
		.fmt = &mtk_imgsys_format_y32,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_meta_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_y32,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 16,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_tnrw_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 192,
		.scan_align = 192,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_tnrci_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_grey,
		.align = 144,
		.scan_align = 108,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_tnrli_fmts[] = {
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
};

static const struct mtk_imgsys_dev_format dip_img2o_fmts[] = {
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
		.align = 16,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_2p010p,
		.align = 1,
		.scan_align = 1,
	},
	{
		.fmt = &mtk_imgsys_format_y8,
		.align = 1,
		.scan_align = 1,
	},
};

static const struct mtk_imgsys_dev_format dip_img3o_fmts[] = {
	/* YUV422, 3 plane 8 bit */
	/* YUV420, 2 plane 8 bit */
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
};

static const struct mtk_imgsys_dev_format dip_img4o_fmts[] = {
	/* YUV420, 2 plane 8 bit */
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
};

static const struct v4l2_frmsizeenum dip_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum dip_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc dip_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMGI_OUT,
		.name = "Imgi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_imgi_fmts,
		.num_fmts = ARRAY_SIZE(dip_imgi_fmts),
		.default_width = MTK_DIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Main image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_VIPI_OUT,
		.name = "Vipi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_vipi_fmts,
		.num_fmts = ARRAY_SIZE(dip_vipi_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Vipi image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_REC_DSI_OUT,
		.name = "Rec_Dsi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_rec_dsi_fmts,
		.num_fmts = ARRAY_SIZE(dip_rec_dsi_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Down Source Image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_REC_DPI_OUT,
		.name = "Rec_Dpi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_rec_dpi_fmts,
		.num_fmts = ARRAY_SIZE(dip_rec_dpi_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Down Processed Image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_CNR_BLURMAPI_OUT,
		.name = "Bokeh Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Bokehi data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_LFEI_OUT,
		.name = "Dmgi_FM Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Dmgi_FM data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_RFEI_OUT,
		.name = "Depi_FM Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Depi_FM data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRSI_OUT,
		.name = "Tnrsi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Statistics input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRWI_OUT,
		.name = "Tnrwi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_tnrw_fmts,
		.num_fmts = ARRAY_SIZE(dip_tnrw_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Weighting input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRMI_OUT,
		.name = "Tnrmi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Motion Map input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRCI_OUT,
		.name = "Tnrci Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_tnrci_fmts,
		.num_fmts = ARRAY_SIZE(dip_tnrci_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Confidence Map input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRLI_OUT,
		.name = "Tnrli Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_tnrli_fmts,
		.num_fmts = ARRAY_SIZE(dip_tnrli_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Low Frequency Diff input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRVBI_OUT,
		.name = "Tnrvbi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Valid Bit Map input",
	},
	/* Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG2O_CAPTURE,
		.name = "Img2o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img2o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img2o_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "Resized output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG3O_CAPTURE,
		.name = "Img3o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img3o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img3o_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "Dip output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG4O_CAPTURE,
		.name = "Img4o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img4o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img4o_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "Nr3d output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_FMO_CAPTURE,
		.name = "FM Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "FM output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRSO_CAPTURE,
		.name = "Tnrso Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "statistics output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRWO_CAPTURE,
		.name = "Tnrwo Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_tnrw_fmts,
		.num_fmts = ARRAY_SIZE(dip_tnrw_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "Weighting output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRMO_CAPTURE,
		.name = "Tnrmo Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.supports_ctrls = true,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.frmsizeenum = &dip_out_frmsizeenum,
		.description = "Motion Map output",
	},
};

#endif // _MTK_DIP_V4L2_VNODE_H_
