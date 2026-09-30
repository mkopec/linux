/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_H
#define __MTK_CAM_RAW_H

#include <linux/kfifo.h>
#include <media/v4l2-subdev.h>
#include "mtk_cam-video.h"
#include "mtk_camera-v4l2-controls.h"

struct mtk_cam_request_stream_data;

/* ISP7_1 */
#define RAW_PIPELINE_NUM 3

#define SCQ_DEADLINE_MS  15 /* ~1/2 frame length */
#define SCQ_DEFAULT_CLK_RATE 208 /* default 208MHz */

#define MTK_CAMSYS_RES_STEP_NUM	8

/* ISP7_1 */
#define IMG_MAX_WIDTH		12000
#define IMG_MAX_HEIGHT		9000

#define IMG_MIN_WIDTH		80
#define IMG_MIN_HEIGHT		60
#define YUV_GROUP1_MAX_WIDTH	8160
#define YUV_GROUP1_MAX_HEIGHT	3896
#define YUV_GROUP2_MAX_WIDTH	3060
#define YUV_GROUP2_MAX_HEIGHT	1145
#define YUV1_MAX_WIDTH		8160
#define YUV1_MAX_HEIGHT		2290
#define YUV2_MAX_WIDTH		3060
#define YUV2_MAX_HEIGHT		1145
#define YUV3_MAX_WIDTH		7794
#define YUV3_MAX_HEIGHT		3896
#define YUV4_MAX_WIDTH		1530
#define YUV4_MAX_HEIGHT		572
#define YUV5_MAX_WIDTH		1530
#define YUV5_MAX_HEIGHT		572
#define DRZS4NO1_MAX_WIDTH	2400
#define DRZS4NO1_MAX_HEIGHT	1080
#define DRZS4NO2_MAX_WIDTH	2400
#define DRZS4NO2_MAX_HEIGHT	1080
#define DRZS4NO3_MAX_WIDTH	576
#define DRZS4NO3_MAX_HEIGHT	432
#define RZH1N2TO1_MAX_WIDTH	1280
#define RZH1N2TO1_MAX_HEIGHT	600
#define RZH1N2TO2_MAX_WIDTH	512
#define RZH1N2TO2_MAX_HEIGHT	512
#define RZH1N2TO3_MAX_WIDTH	1280
#define RZH1N2TO3_MAX_HEIGHT	600

#define IMG_PIX_ALIGN		2

enum raw_module_id {
	RAW_A = 0,
	RAW_B = 1,
	RAW_C = 2,
	RAW_NUM,
};

/* feature mask to categorize all raw functions */
#define MTK_CAM_FEATURE_HDR_MASK		0x0000000F
#define MTK_CAM_FEATURE_SUBSAMPLE_MASK		0x000000F0
#define MTK_CAM_FEATURE_OFFLINE_M2M_MASK	0x00000100
#define MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK	0x00000200

enum raw_function_id {
	/* bit [0~3] reserved for hdr */
	/* bit [4~7] reserved for high fps */
	/* bit [8~9] m2m */
	OFFLINE_M2M			= (1 << 8),
	PURE_OFFLINE_M2M		= (1 << 9),
	RAW_FUNCTION_END		= 0xF0000000,
};

enum hardware_mode_id {
	DEFAULT			= 0,
	ON_THE_FLY		= 1,
	DCIF			= 2,
};

/* enum for pads of raw pipeline */
enum {
	MTK_RAW_SINK_BEGIN = 0,
	MTK_RAW_SINK = MTK_RAW_SINK_BEGIN,
	MTK_RAW_SINK_NUM,
	MTK_RAW_META_IN = MTK_RAW_SINK_NUM,
	MTK_RAW_RAWI_2_IN,
	MTK_RAW_RAWI_3_IN,
	MTK_RAW_RAWI_4_IN,
	MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_MAIN_STREAM_OUT = MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_YUVO_1_OUT,
	MTK_RAW_YUVO_2_OUT,
	MTK_RAW_YUVO_3_OUT,
	MTK_RAW_YUVO_4_OUT,
	MTK_RAW_YUVO_5_OUT,
	MTK_RAW_DRZS4NO_1_OUT,
	MTK_RAW_DRZS4NO_2_OUT,
	MTK_RAW_DRZS4NO_3_OUT,
	MTK_RAW_RZH1N2TO_1_OUT,
	MTK_RAW_RZH1N2TO_2_OUT,
	MTK_RAW_RZH1N2TO_3_OUT,
	MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_0 = MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_1,
	MTK_RAW_META_OUT_2,
	MTK_RAW_PIPELINE_PADS_NUM,
};

static inline bool is_yuv_node(u32 desc_id)
{
	switch (desc_id) {
	case MTK_RAW_YUVO_1_OUT:
	case MTK_RAW_YUVO_2_OUT:
	case MTK_RAW_YUVO_3_OUT:
	case MTK_RAW_YUVO_4_OUT:
	case MTK_RAW_YUVO_5_OUT:
	case MTK_RAW_DRZS4NO_1_OUT:
	case MTK_RAW_DRZS4NO_2_OUT:
	case MTK_RAW_DRZS4NO_3_OUT:
	case MTK_RAW_RZH1N2TO_1_OUT:
	case MTK_RAW_RZH1N2TO_2_OUT:
	case MTK_RAW_RZH1N2TO_3_OUT:
		return true;
	default:
		return false;
	}
}

/* max(pdi_table1, pdi_table2, ...)*/
#define RAW_STATS_CFG_VARIOUS_SIZE ALIGN(0x7500, SZ_1K)

#define MTK_RAW_TOTAL_NODES (MTK_RAW_PIPELINE_PADS_NUM - MTK_RAW_SINK_NUM)

struct mtk_cam_dev;
struct mtk_cam_ctx;

struct mtk_raw_pde_config {
	struct mtk_cam_pde_info pde_info;
};

struct mtk_cam_resource_config {
	struct v4l2_subdev *seninf;
	struct mutex resource_lock; /* protect resource calculation */
	struct v4l2_fract interval;
	s64 pixel_rate;
	u32 bin_limit;
	u32 frz_limit;
	u32 hwn_limit_max;
	u32 hwn_limit_min;
	s64 hblank;
	s64 vblank;
	s64 sensor_pixel_rate;
	u32 res_plan;
	u32 raw_feature;
	u32 res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	u32 clk_target;
	u32 raw_num_used;
	u32 bin_enable;
	u32 frz_enable;
	u32 frz_ratio;
	u32 tgo_pxl_mode;
	u32 raw_path;
	/* sink fmt adjusted according resource used*/
	struct v4l2_mbus_framefmt sink_fmt;
};

struct mtk_raw_pad_config {
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_rect crop;
};

/*
 * struct mtk_raw_pipeline - sub dev to use raws.
 *
 * @feature_pending: keep the user value of S_CTRL V4L2_CID_MTK_CAM_FEATURE.
 *		     It it safe save to be used in mtk_cam_vidioc_s_fmt,
 *		     mtk_cam_vb2_queue_setup and mtk_cam_vb2_buf_queue
 *		     But considering that we can't when the user calls S_CTRL,
 *		     please use mtk_cam_request_stream_data's
 *		     feature.raw_feature field
 *		     to avoid the CTRL value change tming issue.
 * @feature_active: The active feature during streaming. It can't be changed
 *		    during streaming and can only be used after streaming on.
 *
 */
struct mtk_raw_pipeline {
	unsigned int id;
	struct v4l2_subdev subdev;
	struct media_pad pads[MTK_RAW_PIPELINE_PADS_NUM];
	struct mtk_cam_video_device vdev_nodes[MTK_RAW_TOTAL_NODES];
	struct mtk_raw *raw;
	struct mtk_raw_pad_config cfg[MTK_RAW_PIPELINE_PADS_NUM];
	/* cached settings */
	unsigned int enabled_raw;
	unsigned long enabled_dmas;
	/* resource controls */
	struct v4l2_ctrl_handler ctrl_handler;
	s64 feature_pending;
	s64 feature_active;

	struct mtk_cam_resource user_res;
	struct mtk_cam_resource_config res_config;
	struct mtk_cam_resource_config try_res_config;
	int sensor_mode_update;
	s64 sync_id;
	/* pde module */
	struct mtk_raw_pde_config pde_config;
	s64 hw_mode;

};

struct mtk_raw_device {
	struct v4l2_subdev subdev;
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq;
	void __iomem *base;
	void __iomem *base_inner;
	void __iomem *yuv_base;
	unsigned int num_clks;
	struct clk_bulk_data *clk_b;

	unsigned int	fifo_size;
	void		*msg_buffer;
	struct kfifo	msg_fifo;
	atomic_t	is_fifo_overflow;

	struct mtk_raw_pipeline *pipeline;
	bool is_sub;

	u64 sof_count;
	u64 vsync_count;

	atomic_t vf_en;
	int overrun_debug_dump_cnt;
};

struct mtk_yuv_device {
	struct v4l2_subdev subdev;
	struct device *dev;
	unsigned int id;
	void __iomem *base;
	unsigned int num_clks;
	struct clk_bulk_data *clk_b;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block pm_notifier;
#endif
};

/* AE information */
struct mtk_ae_debug_data {
	u64 obc_r1_sum[4];
	u64 obc_r2_sum[4];
	u64 obc_r3_sum[4];
	u64 aa_sum[4];
	u64 ltm_sum[4];
};

/*
 * struct mtk_raw - the raw information
 *
 *
 */
struct mtk_raw {
	struct device *cam_dev;
	struct device *devs[RAW_PIPELINE_NUM];
	struct device *yuvs[RAW_PIPELINE_NUM];
	struct mtk_raw_pipeline pipelines[RAW_PIPELINE_NUM];
};

static inline struct mtk_raw_pipeline*
mtk_cam_ctrl_handler_to_raw_pipeline(struct v4l2_ctrl_handler *handler)
{
	return container_of(handler, struct mtk_raw_pipeline, ctrl_handler);
};

int mtk_cam_raw_setup_dependencies(struct mtk_raw *raw);

int mtk_cam_raw_register_entities(struct mtk_raw *raw,
				  struct v4l2_device *v4l2_dev);
void mtk_cam_raw_unregister_entities(struct mtk_raw *raw);

int mtk_cam_raw_select(struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_input_param *cfg_in_param);

void mtk_cam_raw_initialize(struct mtk_raw_device *dev, int is_sub);

void mtk_cam_raw_stream_on(struct mtk_raw_device *dev, int on);

void mtk_cam_raw_apply_cq(struct mtk_raw_device *dev, dma_addr_t cq_addr,
			  unsigned int cq_size, unsigned int cq_offset,
			  unsigned int sub_cq_size, unsigned int sub_cq_offset);

void mtk_cam_raw_trigger_rawi(struct mtk_raw_device *dev, struct mtk_cam_ctx *ctx,
			      signed int hw_scene);

void mtk_cam_raw_reset(struct mtk_raw_device *dev);

void mtk_cam_raw_dump_aa_info(struct mtk_cam_ctx *ctx, struct mtk_ae_debug_data *ae_info);

extern struct platform_driver mtk_cam_raw_driver;
extern struct platform_driver mtk_cam_yuv_driver;

static inline u32 dmaaddr_lsb(dma_addr_t addr)
{
	return addr & (BIT_MASK(32) - 1UL);
}

static inline u32 dmaaddr_msb(dma_addr_t addr)
{
	return (u64)addr >> 32;
}

#endif /*__MTK_CAM_RAW_H*/
