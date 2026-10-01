// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek ISP imgsys formats
 *
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/mtkisp_imgsys.h>

#include "mtk_header_desc.h"
#include "mtk_imgsys-formats.h"

const struct mtk_imgsys_format mtk_imgsys_format_nv12 = {
	.format = V4L2_PIX_FMT_NV12,
	.num_planes = 2,
	.pixels_per_group = 2,
	.bytes_per_group = {2, 2},
	.vertical_sub_sampling = { 1, 2 },
};

const struct mtk_imgsys_format mtk_imgsys_format_nv21 = {
	.format = V4L2_PIX_FMT_NV21,
	.num_planes = 2,
	.pixels_per_group = 2,
	.bytes_per_group = {2, 2},
	.vertical_sub_sampling = { 1, 2 },
};

const struct mtk_imgsys_format mtk_imgsys_format_grey = {
	.format = V4L2_PIX_FMT_GREY,
	.num_planes = 1,
	.pixels_per_group = 1,
	.bytes_per_group = { 1 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_2p012p = {
	.format = V4L2_PIX_FMT_YUV_2P012P,
	.num_planes = 2,
	.pixels_per_group = 64,
	.bytes_per_group = { 96, 96 },
	.vertical_sub_sampling = { 1, 2 },
};

const struct mtk_imgsys_format mtk_imgsys_format_2p010p = {
	.format = V4L2_PIX_FMT_YUV_2P010P,
	.num_planes = 2,
	.pixels_per_group = 64,
	.bytes_per_group = { 80, 80 },
	.vertical_sub_sampling = { 1, 2 },
};

const struct mtk_imgsys_format mtk_imgsys_format_y8 = {
	.format = V4L2_PIX_FMT_MTISP_Y8,
	.num_planes = 1,
	.pixels_per_group = 1,
	.bytes_per_group = { 1 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_y16 = {
	.format = V4L2_PIX_FMT_MTISP_Y16,
	.num_planes = 1,
	.buffer_size = 0,
	.pixels_per_group = 1,
	.bytes_per_group = { 2 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_y32 = {
	.format = V4L2_PIX_FMT_MTISP_Y32,
	.num_planes = 1,
	.buffer_size = 0,
	.pixels_per_group = 1,
	.bytes_per_group = { 4 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_warp2p = {
	.format = V4L2_PIX_FMT_WARP2P,
	.num_planes = 2,
	.buffer_size = 0,
	.pixels_per_group = 4,
	.bytes_per_group = { 8, 8 },
	.vertical_sub_sampling = { 1, 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_sgbrg10 = {
	.format = V4L2_PIX_FMT_MTISP_SGBRG10,
	.num_planes = 1,
	.buffer_size = 0,
	.pixels_per_group = 64,
	.bytes_per_group = { 80 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_srggb10 = {
	.format = V4L2_PIX_FMT_MTISP_SRGGB10,
	.num_planes = 1,
	.buffer_size = 0,
	.pixels_per_group = 64,
	.bytes_per_group = { 80 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_sgrbg10 = {
	.format = V4L2_PIX_FMT_MTISP_SGRBG10,
	.num_planes = 1,
	.buffer_size = 0,
	.pixels_per_group = 64,
	.bytes_per_group = { 80 },
	.vertical_sub_sampling = { 1 },
};

const struct mtk_imgsys_format mtk_imgsys_format_metai = {
	.format = V4L2_META_FMT_MTISP_DESC,
	.num_planes = 1,
	.buffer_size = 219352,
};

const struct mtk_imgsys_format mtk_imgsys_format_stt = {
	/* TODO: It should be a different format? */
	.format = V4L2_META_FMT_MTISP_DESC,
	.num_planes = 1,
	.buffer_size = 738624,
};

const struct mtk_imgsys_format mtk_imgsys_format_ctrlmeta = {
	.format = V4L2_META_FMT_MTISP_PARAMS,
	.num_planes = 1,
	.buffer_size = 28672,
};
