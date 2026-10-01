/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Mediatek ISP imgsys User space API
 *
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTKISP_IMGSYS_USER_H
#define _MTKISP_IMGSYS_USER_H

#include <linux/v4l2-controls.h>

/* The base for the mediatek imgsys driver controls */
/* We reserve 48 controls for this driver. */
#define V4L2_CID_USER_MTK_IMG_BASE		(V4L2_CID_USER_BASE + 0x1100)

/* MTK ISP imgsys controls */
#define V4L2_CID_MTK_IMG_RESIZE_RATIO	(V4L2_CID_USER_MTK_IMG_BASE + 34)

/* Vendor specific - Mediatek ISP formats */

/* Mediatek warp map 32-bit, 2 plane */
#define V4L2_PIX_FMT_WARP2P      v4l2_fourcc('M', 'W', '2', 'P')
/* YUV-10bit packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YUV_2P010P    v4l2_fourcc('U', '0', '2', 'A')
/* YUV-12bit packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YUV_2P012P    v4l2_fourcc('U', '0', '2', 'C')
/* Y-32bit */
#define V4L2_PIX_FMT_MTISP_Y32   v4l2_fourcc('M', 'T', '3', '2')
/* Y-16bit */
#define V4L2_PIX_FMT_MTISP_Y16   v4l2_fourcc('M', 'T', '1', '6')
/* Y-8bit */
#define V4L2_PIX_FMT_MTISP_Y8   v4l2_fourcc('M', 'T', '0', '8')

/*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR10  v4l2_fourcc('M', 'B', 'B', 'A')
#define V4L2_PIX_FMT_MTISP_SGBRG10  v4l2_fourcc('M', 'B', 'G', 'A')
#define V4L2_PIX_FMT_MTISP_SGRBG10  v4l2_fourcc('M', 'B', 'g', 'A')
#define V4L2_PIX_FMT_MTISP_SRGGB10  v4l2_fourcc('M', 'B', 'R', 'A')

/* Vendor specific - Mediatek ISP parameters for firmware */
#define V4L2_META_FMT_MTISP_PARAMS v4l2_fourcc('M', 'T', 'f', 'p')
/* ISP description fmt*/
#define V4L2_META_FMT_MTISP_DESC   v4l2_fourcc('M', 'T', 'f', 'd')

#endif /* _MTKISP_IMGSYS_USER_H */
