/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Mediatek ISP camsys User space API
 *
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTKISP_CAMSYS_USER_H
#define _MTKISP_CAMSYS_USER_H

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

/* MTK ISP camsys events */
#define V4L2_EVENT_REQUEST_DRAINED              (V4L2_EVENT_PRIVATE_START + 1)
#define V4L2_EVENT_REQUEST_DUMPED               (V4L2_EVENT_PRIVATE_START + 2)

/* The base for the mediatek camsys driver controls */
/* We reserve 48 controls for this driver. */
#define V4L2_CID_USER_MTK_CAM_BASE		(V4L2_CID_USER_BASE + 0x10d0)

/* MTK ISP camsys controls */
#define V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT	(V4L2_CID_USER_MTK_CAM_BASE + 1)
#define V4L2_CID_MTK_CAM_BIN_LIMIT		(V4L2_CID_USER_MTK_CAM_BASE + 2)
#define V4L2_CID_MTK_CAM_FRZ_LIMIT		(V4L2_CID_USER_MTK_CAM_BASE + 3)
#define V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY	(V4L2_CID_USER_MTK_CAM_BASE + 4)
#define V4L2_CID_MTK_CAM_USED_ENGINE		(V4L2_CID_USER_MTK_CAM_BASE + 5)
#define V4L2_CID_MTK_CAM_BIN			(V4L2_CID_USER_MTK_CAM_BASE + 6)
#define V4L2_CID_MTK_CAM_FRZ			(V4L2_CID_USER_MTK_CAM_BASE + 7)
#define V4L2_CID_MTK_CAM_USED_ENGINE_TRY	(V4L2_CID_USER_MTK_CAM_BASE + 8)
#define V4L2_CID_MTK_CAM_BIN_TRY		(V4L2_CID_USER_MTK_CAM_BASE + 9)
#define V4L2_CID_MTK_CAM_FRZ_TRY		(V4L2_CID_USER_MTK_CAM_BASE + 10)
#define V4L2_CID_MTK_CAM_PIXEL_RATE		(V4L2_CID_USER_MTK_CAM_BASE + 11)
#define V4L2_CID_MTK_CAM_FEATURE		(V4L2_CID_USER_MTK_CAM_BASE + 12)
#define V4L2_CID_MTK_CAM_SYNC_ID		(V4L2_CID_USER_MTK_CAM_BASE + 13)
#define V4L2_CID_MTK_CAM_RAW_PATH_SELECT	(V4L2_CID_USER_MTK_CAM_BASE + 14)
#define V4L2_CID_MTK_CAM_HSF_EN			(V4L2_CID_USER_MTK_CAM_BASE + 15)
#define V4L2_CID_MTK_CAM_PDE_INFO		(V4L2_CID_USER_MTK_CAM_BASE + 16)
#define V4L2_CID_MTK_CAM_MSTREAM_EXPOSURE	(V4L2_CID_USER_MTK_CAM_BASE + 17)
#define V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC	(V4L2_CID_USER_MTK_CAM_BASE + 18)
#define V4L2_CID_MTK_CAM_TG_FLASH_CFG		(V4L2_CID_USER_MTK_CAM_BASE + 19)
#define V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE	(V4L2_CID_USER_MTK_CAM_BASE + 20)
#define V4L2_CID_MTK_CAM_CAMSYS_HW_MODE		(V4L2_CID_USER_MTK_CAM_BASE + 21)

/* Vendor specific - Mediatek ISP bayer formats */
#define V4L2_PIX_FMT_MTISP_SBGGR8   v4l2_fourcc('M', 'B', 'B', '8') /*  Packed  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG8   v4l2_fourcc('M', 'B', 'G', '8') /*  Packed  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG8   v4l2_fourcc('M', 'B', 'g', '8') /*  Packed  8-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB8   v4l2_fourcc('M', 'B', 'R', '8') /*  Packed  8-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR10  v4l2_fourcc('M', 'B', 'B', 'A') /*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG10  v4l2_fourcc('M', 'B', 'G', 'A') /*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG10  v4l2_fourcc('M', 'B', 'g', 'A') /*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB10  v4l2_fourcc('M', 'B', 'R', 'A') /*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR12  v4l2_fourcc('M', 'B', 'B', 'C') /*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG12  v4l2_fourcc('M', 'B', 'G', 'C') /*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG12  v4l2_fourcc('M', 'B', 'g', 'C') /*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB12  v4l2_fourcc('M', 'B', 'R', 'C') /*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR14  v4l2_fourcc('M', 'B', 'B', 'E') /*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG14  v4l2_fourcc('M', 'B', 'G', 'E') /*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG14  v4l2_fourcc('M', 'B', 'g', 'E') /*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB14  v4l2_fourcc('M', 'B', 'R', 'E') /*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR8F  v4l2_fourcc('M', 'F', 'B', '8') /*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG8F  v4l2_fourcc('M', 'F', 'G', '8') /*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG8F  v4l2_fourcc('M', 'F', 'g', '8') /*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB8F  v4l2_fourcc('M', 'F', 'R', '8') /*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR10F  v4l2_fourcc('M', 'F', 'B', 'A') /*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG10F  v4l2_fourcc('M', 'F', 'G', 'A') /*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG10F  v4l2_fourcc('M', 'F', 'g', 'A') /*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB10F  v4l2_fourcc('M', 'F', 'R', 'A') /*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR12F  v4l2_fourcc('M', 'F', 'B', 'C') /*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG12F  v4l2_fourcc('M', 'F', 'G', 'C') /*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG12F  v4l2_fourcc('M', 'F', 'g', 'C') /*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB12F  v4l2_fourcc('M', 'F', 'R', 'C') /*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR14F  v4l2_fourcc('M', 'F', 'B', 'E') /*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG14F  v4l2_fourcc('M', 'F', 'G', 'E') /*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG14F  v4l2_fourcc('M', 'F', 'g', 'E') /*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB14F  v4l2_fourcc('M', 'F', 'R', 'E') /*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGRB8F  v4l2_fourcc('M', 'F', '8', 'P') /* three planes Full-G 8-bit */
#define V4L2_PIX_FMT_MTISP_SGRB10F  v4l2_fourcc('M', 'F', 'A', 'P') /* three planes Full-G 10-bit */
#define V4L2_PIX_FMT_MTISP_SGRB12F  v4l2_fourcc('M', 'F', 'C', 'P') /* three planes Full-G 12-bit */

/* Vendor specific - Mediatek Luminance+Chrominance formats */
#define V4L2_PIX_FMT_MTISP_YUYV10P v4l2_fourcc('Y', 'U', 'A', 'P') /* YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_YVYU10P v4l2_fourcc('Y', 'V', 'A', 'P') /* YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_UYVY10P v4l2_fourcc('U', 'Y', 'A', 'P') /* YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_VYUY10P v4l2_fourcc('V', 'Y', 'A', 'P') /* YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_NV12_10P v4l2_fourcc('1', '2', 'A', 'P') /* Y/CbCr 4:2:0 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV21_10P v4l2_fourcc('2', '1', 'A', 'P') /* Y/CrCb 4:2:0 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV16_10P v4l2_fourcc('1', '6', 'A', 'P') /* Y/CbCr 4:2:2 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV61_10P v4l2_fourcc('6', '1', 'A', 'P') /* Y/CrCb 4:2:2 10 bits packed */
#define V4L2_PIX_FMT_MTISP_YUYV12P v4l2_fourcc('Y', 'U', 'C', 'P') /* YUV 4:2:2 12-bit packed */
#define V4L2_PIX_FMT_MTISP_YVYU12P v4l2_fourcc('Y', 'V', 'C', 'P') /* YUV 4:2:2 12-bit packed */
#define V4L2_PIX_FMT_MTISP_UYVY12P v4l2_fourcc('U', 'Y', 'C', 'P') /* YUV 4:2:2 12-bit packed */
#define V4L2_PIX_FMT_MTISP_VYUY12P v4l2_fourcc('V', 'Y', 'C', 'P') /* YUV 4:2:2 12-bit packed */
#define V4L2_PIX_FMT_MTISP_NV12_12P v4l2_fourcc('1', '2', 'C', 'P') /* Y/CbCr 4:2:0 12 bits packed */
#define V4L2_PIX_FMT_MTISP_NV21_12P v4l2_fourcc('2', '1', 'C', 'P') /* Y/CrCb 4:2:0 12 bits packed */
#define V4L2_PIX_FMT_MTISP_NV16_12P v4l2_fourcc('1', '6', 'C', 'P') /* Y/CbCr 4:2:2 12 bits packed */
#define V4L2_PIX_FMT_MTISP_NV61_12P v4l2_fourcc('6', '1', 'C', 'P') /* Y/CrCb 4:2:2 12 bits packed */

/* Vendor specific - Mediatek specified compressed format */
#define V4L2_PIX_FMT_MTISP_NV12_UFBC v4l2_fourcc('1', '2', '8', 'F') /* Y/CbCr 4:2:0 8 bits compressed */
#define V4L2_PIX_FMT_MTISP_NV21_UFBC v4l2_fourcc('2', '1', '8', 'F') /* Y/CrCb 4:2:0 8 bits compressed */
#define V4L2_PIX_FMT_MTISP_NV12_10_UFBC v4l2_fourcc('1', '2', 'A', 'F') /* Y/CbCr 4:2:0 10 bits compressed */
#define V4L2_PIX_FMT_MTISP_NV21_10_UFBC v4l2_fourcc('2', '1', 'A', 'F') /* Y/CrCb 4:2:0 10 bits compressed */
#define V4L2_PIX_FMT_MTISP_NV12_12_UFBC v4l2_fourcc('1', '2', 'C', 'F') /* Y/CbCr 4:2:0 12 bits compressed */
#define V4L2_PIX_FMT_MTISP_NV21_12_UFBC v4l2_fourcc('2', '1', 'C', 'F') /* Y/CrCb 4:2:0 12 bits compressed */
#define V4L2_PIX_FMT_MTISP_BAYER8_UFBC v4l2_fourcc('M', 'B', '8', 'U') /* Raw 8 bits compressed */
#define V4L2_PIX_FMT_MTISP_BAYER10_UFBC v4l2_fourcc('M', 'B', 'A', 'U') /* Raw 10 bits compressed */
#define V4L2_PIX_FMT_MTISP_BAYER12_UFBC v4l2_fourcc('M', 'B', 'C', 'U') /* Raw 12 bits compressed */
#define V4L2_PIX_FMT_MTISP_BAYER14_UFBC v4l2_fourcc('M', 'B', 'E', 'U') /* Raw 14 bits compressed */

/* Vendor specific - Mediatek ISP parameters for firmware */
#define V4L2_META_FMT_MTISP_3A    v4l2_fourcc('M', 'T', 'f', 'a') /* AE/AWB histogram */
#define V4L2_META_FMT_MTISP_AF    v4l2_fourcc('M', 'T', 'f', 'f') /* AF histogram */
#define V4L2_META_FMT_MTISP_LCS   v4l2_fourcc('M', 'T', 'f', 'c') /* Local contrast enhanced statistics */
#define V4L2_META_FMT_MTISP_LMV   v4l2_fourcc('M', 'T', 'f', 'm') /* Local motion vector histogram */
#define V4L2_META_FMT_MTISP_PARAMS v4l2_fourcc('M', 'T', 'f', 'p') /* ISP tuning parameters */

/*
 * struct mtk_cam_resource_sensor - sensor resoruces for format negotiation
 *
 */
struct mtk_cam_resource_sensor {
	struct v4l2_fract interval;
	__u32 hblank;
	__u32 vblank;
	__u64 pixel_rate;
	__u64 cust_pixel_rate;
};

/*
 * struct mtk_cam_resource_raw - MTK camsys raw resoruces for format negotiation
 *
 * @feature: value of V4L2_CID_MTK_CAM_FEATURE the user want to check the
 *		  resource with. If it is used in set CTRL, we will apply the value
 *		  to V4L2_CID_MTK_CAM_FEATURE ctrl directly.
 * @strategy: indicate the order of multiple raws, binning or DVFS to be selected
 *	      when doing format negotiation of raw's source pads (output pads).
 *	      Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	      determine it.
 * @raw_max: indicate the max number of raw to be used for the raw pipeline.
 *	     Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	     determine it.
 * @raw_min: indicate the max number of raw to be used for the raw pipeline.
 *	     Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	     determine it.
 * @raw_used: The number of raw used. The used don't need to writ this failed,
 *	      the driver always updates the field.
 * @bin: indicate if the driver should enable the bining or not. The driver
 *	 update the field depanding the hardware supporting status. Please pass
 *	 MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to determine it.
 * @path_sel: indicate the user selected raw path. The driver
 *	      update the field depanding the hardware supporting status. Please
 *	      pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	      determine it.
 * @pixel_mode: the pixel mode driver used in the raw pipeline. It is written by
 *		driver only.
 * @throughput: the throughput be used in the raw pipeline. It is written by
 *		driver only.
 *
 */
struct mtk_cam_resource_raw {
	__s64	feature;
	__u16	strategy;
	__u8	raw_max;
	__u8	raw_min;
	__u8	raw_used;
	__u8	bin;
	__u8	path_sel;
	__u8	pixel_mode;
	__u64	throughput;
};

/*
 * struct mtk_cam_resource - MTK camsys resoruces for format negotiation
 *
 * @sink_fmt: sink_fmt pad's format, it must be return by g_fmt or s_fmt
 *		from driver.
 * @sensor_res: senor information to calculate the required resource, it is
 *		read-only and camsys driver will not change it.
 * @raw_res: user hint and resource negotiation result.
 * @status: resource negotiation status.
 *
 */
struct mtk_cam_resource {
	__u64 sink_fmt;
	struct mtk_cam_resource_sensor sensor_res;
	struct mtk_cam_resource_raw raw_res;
	__u8 status;
};

/**
 * struct mtk_cam_pde_info - PDE module information for raw
 *
 * @pdo_max_size: the max pdo size of pde sensor.
 * @pdi_max_size: the max pdi size of pde sensor or max pd table size.
 * @pd_table_offset: the offest of meta config for pd table content.
 */
struct mtk_cam_pde_info {
	__u32 pdo_max_size;
	__u32 pdi_max_size;
	__u32 pd_table_offset;
};
#endif /* _MTKISP_CAMSYS_USER_H */
