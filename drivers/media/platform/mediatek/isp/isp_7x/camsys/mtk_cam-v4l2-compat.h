/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MTK_CAM_V4L2_COMPAT_H
#define __MTK_CAM_V4L2_COMPAT_H

#include <linux/videodev2.h>

#ifndef V4L2_PIX_FMT_YUYV10
#define V4L2_PIX_FMT_YUYV10  v4l2_fourcc('Y', 'U', 'Y', 'A')
#define V4L2_PIX_FMT_YVYU10  v4l2_fourcc('Y', 'V', 'Y', 'A')
#define V4L2_PIX_FMT_UYVY10  v4l2_fourcc('U', 'Y', 'V', 'A')
#define V4L2_PIX_FMT_VYUY10  v4l2_fourcc('V', 'Y', 'U', 'A')
#define V4L2_PIX_FMT_YUYV12  v4l2_fourcc('Y', 'U', 'Y', 'C')
#define V4L2_PIX_FMT_YVYU12  v4l2_fourcc('Y', 'V', 'Y', 'C')
#define V4L2_PIX_FMT_UYVY12  v4l2_fourcc('U', 'Y', 'V', 'C')
#define V4L2_PIX_FMT_VYUY12  v4l2_fourcc('V', 'Y', 'U', 'C')
#endif

#ifndef V4L2_PIX_FMT_NV12_10
#define V4L2_PIX_FMT_NV12_10 v4l2_fourcc('1', '2', 'A', 'U')
#define V4L2_PIX_FMT_NV21_10 v4l2_fourcc('2', '1', 'A', 'U')
#define V4L2_PIX_FMT_NV16_10 v4l2_fourcc('1', '6', 'A', 'U')
#define V4L2_PIX_FMT_NV61_10 v4l2_fourcc('6', '1', 'A', 'U')
#define V4L2_PIX_FMT_NV12_12 v4l2_fourcc('1', '2', 'C', 'U')
#define V4L2_PIX_FMT_NV21_12 v4l2_fourcc('2', '1', 'C', 'U')
#define V4L2_PIX_FMT_NV16_12 v4l2_fourcc('1', '6', 'C', 'U')
#define V4L2_PIX_FMT_NV61_12 v4l2_fourcc('6', '1', 'C', 'U')
#endif

#endif
