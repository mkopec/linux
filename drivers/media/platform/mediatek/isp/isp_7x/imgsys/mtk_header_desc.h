/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 */

#ifndef _HEADER_DESC_
#define _HEADER_DESC_

#include <linux/videodev2.h>

#define COMPACT_USE
struct v4l2_ext_plane {
	union {
		struct {
			__u32		offset;
			__u64		phyaddr;
		} dma_buf;
	} m;
	__u64			isp_addr;
	__u64			size;
};

#define IMGBUF_MAX_PLANES (3)

struct v4l2_ext_buffer {
	struct v4l2_ext_plane	planes[IMGBUF_MAX_PLANES];
	__u32			num_planes;
	__u64			reserved[2];
};

struct mtk_imgsys_crop {
	struct v4l2_rect	c;
	struct v4l2_fract	left_subpix;
	struct v4l2_fract	top_subpix;
	struct v4l2_fract	width_subpix;
	struct v4l2_fract	height_subpix;
};

struct plane_pix_format {
	__u32		sizeimage;
	__u32		bytesperline;
} __packed;

struct pix_format_mplane {
	__u32				width;
	__u32				height;
	__u32				pixelformat;
	struct plane_pix_format	plane_fmt[IMGBUF_MAX_PLANES];
} __packed;

struct frameparams {
	struct v4l2_ext_buffer buf;
	struct pix_format_mplane fmt;
	struct mtk_imgsys_crop crop;
	/* struct v4l2_rect compose; */
	__u32 rotation;
	__u32 hflip;
	__u32 vflip;
	__u8  resizeratio;
};

#define TMAX (16)
struct header_desc_norm {
	__u32 fparams_tnum;
	struct frameparams fparams[TMAX];
};

#define IMG_MAX_HW_DMAS     72

struct singlenode_desc_norm {
	__u8 dmas_enable[IMG_MAX_HW_DMAS][TMAX];
	struct header_desc_norm	dmas[IMG_MAX_HW_DMAS];
	struct header_desc_norm	tuning_meta;
	struct header_desc_norm	ctrl_meta;
};

#endif
