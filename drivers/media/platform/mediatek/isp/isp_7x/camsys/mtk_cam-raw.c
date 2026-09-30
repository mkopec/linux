// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/videodev2.h>
#include <linux/suspend.h>
#include <linux/rtc.h>

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include <soc/mediatek/smi.h>

#include "mtk_cam.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-raw.h"

#include "mtk_cam-regs-mt8188.h"

#include "mtk_cam-video.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_camera-v4l2-controls.h"

#include "mtk_cam-dmadbg.h"
#include "mtk_cam-raw_debug.h"

static unsigned int debug_raw;
module_param(debug_raw, uint, 0644);
MODULE_PARM_DESC(debug_raw, "activates debug info");

static int debug_raw_num = -1;
module_param(debug_raw_num, int, 0644);
MODULE_PARM_DESC(debug_raw_num, "debug: num of used raw devices");

static int debug_pixel_mode = -1;
module_param(debug_pixel_mode, int, 0644);
MODULE_PARM_DESC(debug_pixel_mode, "debug: pixel mode");

static int debug_clk_idx = -1;
module_param(debug_clk_idx, int, 0644);
MODULE_PARM_DESC(debug_clk_idx, "debug: clk idx");

static int debug_dump_fbc;
module_param(debug_dump_fbc, int, 0644);
MODULE_PARM_DESC(debug_dump_fbc, "debug: dump fbc");

#define MTK_CAMSYS_RES_IDXMASK		0xF0
#define MTK_CAMSYS_RES_BIN_TAG		0x10
#define MTK_CAMSYS_RES_FRZ_TAG		0x20
#define MTK_CAMSYS_RES_HWN_TAG		0x30
#define MTK_CAMSYS_RES_CLK_TAG		0x40

#define MTK_CAMSYS_RES_PLAN_NUM		10
#define FRZ_PXLMODE_THRES		71
#define MHz		1000000

/* ISP7_1 */
#define TGO_MAX_PXLMODE				1
#define MTK_CAMSYS_PROC_DEFAULT_PIXELMODE	1
#define sizeof_u32(__struct__) ((sizeof(__struct__) + sizeof(u32) - 1) / \
				sizeof(u32))

enum MTK_CAMSYS_RES_STEP {
	E_RES_BASIC,
	E_RES_BIN_S = MTK_CAMSYS_RES_BIN_TAG,
	E_RES_BIN0 = E_RES_BIN_S,
	E_RES_BIN1,
	E_RES_BIN_E,
	E_RES_FRZ_S = MTK_CAMSYS_RES_FRZ_TAG,
	E_RES_FRZ0 = E_RES_FRZ_S,
	E_RES_FRZ1,
	E_RES_FRZ_E,
	E_RES_HWN_S = MTK_CAMSYS_RES_HWN_TAG,
	E_RES_HWN0 = E_RES_HWN_S,
	E_RES_HWN1,
	E_RES_HWN2,
	E_RES_HWN_E,
	E_RES_CLK_S = MTK_CAMSYS_RES_CLK_TAG,
	E_RES_CLK0 = E_RES_CLK_S,
	E_RES_CLK1,
	E_RES_CLK2,
	E_RES_CLK3,
	E_RES_CLK_E,
};

enum MTK_CAMSYS_MAXLB_CHECK_RESULT {
	LB_CHECK_OK = 0,
	LB_CHECK_CBN,
	LB_CHECK_QBN,
	LB_CHECK_BIN,
	LB_CHECK_FRZ,
	LB_CHECK_TWIN,
	LB_CHECK_RAW,
};

#define CAM_RAW_PROCESS_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_FRZ_MAX_LINE_BUFFER		(6632)
#define CAM_RAW_BIN_MAX_LINE_BUFFER		(12000)
#define CAM_RAW_QBND_MAX_LINE_BUFFER		(16000)
#define CAM_RAW_CBN_MAX_LINE_BUFFER		(18472)
#define CAM_TWIN_PROCESS_MAX_LINE_BUFFER	(12400)

struct cam_resource_plan {
	int cam_resource[MTK_CAMSYS_RES_STEP_NUM];
};

enum resource_strategy_id {
	RESOURCE_STRATEGY_QPR = 0,
	RESOURCE_STRATEGY_PQR,
	RESOURCE_STRATEGY_RPQ,
	RESOURCE_STRATEGY_QRP,
	RESOURCE_STRATEGY_NUMBER
};

static const struct cam_resource_plan raw_resource_strategy_plan[] = {
	[RESOURCE_STRATEGY_QPR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_CLK1, E_RES_CLK2,
			E_RES_CLK3, E_RES_FRZ1, E_RES_BIN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_PQR] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1,
			E_RES_BIN1, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3} },
	[RESOURCE_STRATEGY_RPQ] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_FRZ1, E_RES_BIN1, E_RES_CLK1,
			E_RES_CLK2, E_RES_CLK3, E_RES_HWN1, E_RES_HWN2} },
	[RESOURCE_STRATEGY_QRP] = {
		.cam_resource = {
			E_RES_BASIC, E_RES_CLK1, E_RES_CLK2, E_RES_CLK3,
			E_RES_HWN1, E_RES_HWN2, E_RES_FRZ1, E_RES_BIN1} },
};

static const struct v4l2_mbus_framefmt mfmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static bool mtk_cam_raw_resource_calc(struct mtk_cam_device *cam,
				      struct mtk_cam_resource_config *res,
				      s64 pixel_rate, int res_plan, int in_w,
				      int in_h, int *out_w, int *out_h);

static int mtk_cam_raw_res_copy_fmt_from_user(struct mtk_raw_pipeline *pipeline,
					      struct mtk_cam_resource *res_user,
					      struct v4l2_mbus_framefmt *dest)
{
	long bytes;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	if (!res_user->sink_fmt) {
		dev_err(dev,
			"%s:pipe(%d): sink_fmt can't be NULL for res ctrl\n",
			__func__, pipeline->id);

		return -EINVAL;
	}

	bytes = copy_from_user(dest, (void __user *)res_user->sink_fmt,
			       sizeof(*dest));
	if (bytes) {
		dev_err(dev,
			"%s:pipe(%d): copy_from_user on sink_fmt failed (%ld)\n",
			__func__, pipeline->id, bytes);
		return -EINVAL;
	}

	return 0;
}

static int mtk_cam_raw_res_copy_fmt_to_user(struct mtk_raw_pipeline *pipeline,
					    struct mtk_cam_resource *res_user,
					    struct v4l2_mbus_framefmt *src)
{
	long bytes;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	/* return the fmt to the users */
	bytes = copy_to_user((void __user *)res_user->sink_fmt, src, sizeof(*src));
	if (bytes) {
		dev_err(dev,
			"%s:pipe(%d): copy_to_user on sink_fmt failed (%ld)\n",
			__func__, pipeline->id, bytes);

		return -EINVAL;
	}

	return 0;
}

static int mtk_cam_raw_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_cam_resource *user_res;
	struct mtk_raw_pipeline *pipeline;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	if (ctrl->id == V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC) {
		user_res = (struct mtk_cam_resource *)ctrl->p_new.p;
		user_res->sensor_res = pipeline->user_res.sensor_res;
		user_res->raw_res = pipeline->user_res.raw_res;
		if (user_res->sink_fmt)
			ret = mtk_cam_raw_res_copy_fmt_to_user(pipeline,
							       user_res,
							       (struct v4l2_mbus_framefmt *)
							       pipeline->user_res.sink_fmt);
	} else if (ctrl->id == V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE) {
		ctrl->val = pipeline->sensor_mode_update;
		dev_info(dev,
			 "%s:pipe(%d): V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE: %d\n",
			 __func__, pipeline->id, pipeline->sensor_mode_update);
	} else if (ctrl->id >= V4L2_CID_MTK_CAM_USED_ENGINE_TRY &&
	    ctrl->id <= V4L2_CID_MTK_CAM_FRZ_TRY) {
		/**
		 * Read the determined resource for the "try" format
		 * negotiation result
		 */
		mutex_lock(&pipeline->try_res_config.resource_lock);
		switch (ctrl->id) {
		case V4L2_CID_MTK_CAM_USED_ENGINE_TRY:
			ctrl->val = pipeline->try_res_config.raw_num_used;
			break;
		case V4L2_CID_MTK_CAM_BIN_TRY:
			ctrl->val = pipeline->try_res_config.bin_enable;
			break;
		case V4L2_CID_MTK_CAM_FRZ_TRY:
			ctrl->val = pipeline->try_res_config.frz_enable ?
				    pipeline->try_res_config.frz_ratio : 100;
			break;
		default:
			dev_info(dev,
				 "%s:pipe(%d): unknown resource CID: %d\n",
				 __func__, pipeline->id, ctrl->id);
			break;
		}
		mutex_unlock(&pipeline->try_res_config.resource_lock);
	} else if (ctrl->id == V4L2_CID_MTK_CAM_SYNC_ID) {
		mutex_lock(&pipeline->res_config.resource_lock);
		*ctrl->p_new.p_s64 = pipeline->sync_id;
		mutex_unlock(&pipeline->res_config.resource_lock);
	} else {
		/**
		 * Read the determined resource for the "set" format
		 * negotiations result
		 */
		mutex_lock(&pipeline->res_config.resource_lock);
		switch (ctrl->id) {
		case V4L2_CID_MTK_CAM_USED_ENGINE:
			ctrl->val = pipeline->res_config.raw_num_used;
			break;
		case V4L2_CID_MTK_CAM_BIN_LIMIT:
			ctrl->val = pipeline->res_config.bin_limit;
			break;
		case V4L2_CID_MTK_CAM_BIN:
			ctrl->val = pipeline->res_config.bin_enable;
			break;
		case V4L2_CID_MTK_CAM_FRZ:
			ctrl->val = pipeline->res_config.frz_enable ?
				    pipeline->res_config.frz_ratio : 100;
			break;
		default:
			dev_info(dev,
				 "%s:pipe(%d): unknown try resource CID: %d\n",
				 __func__, pipeline->id, ctrl->id);
			break;
		}
		mutex_unlock(&pipeline->res_config.resource_lock);
	}

	dev_dbg(dev, "%s:pipe(%d):id(%s) val(%d)\n",
		__func__, pipeline->id, ctrl->name, ctrl->val);
	return ret;
}

static int mtk_cam_raw_res_store(struct mtk_raw_pipeline *pipeline,
				 struct mtk_cam_resource *res_user)
{
	struct device *dev = pipeline->raw->devs[pipeline->id];

	dev_info(dev,
		 "%s:pipe(%d): from user: sensor:%d/%d/%lld/%lld/%d/%d, raw:%lld/%d/%d/%d/%d/%d/%d/%d/%lld\n",
		 __func__, pipeline->id,
		 res_user->sensor_res.hblank, res_user->sensor_res.vblank,
		 res_user->sensor_res.pixel_rate,
		 res_user->sensor_res.cust_pixel_rate,
		 res_user->sensor_res.interval.denominator,
		 res_user->sensor_res.interval.numerator,
		 res_user->raw_res.feature, res_user->raw_res.bin,
		 res_user->raw_res.path_sel, res_user->raw_res.raw_max,
		 res_user->raw_res.raw_min, res_user->raw_res.raw_used,
		 res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
		 res_user->raw_res.throughput);

	/* check user value of sensor input parameters */
	if (!mtk_cam_feature_is_pure_m2m(res_user->raw_res.feature) &&
	    (!res_user->sensor_res.pixel_rate || !res_user->sensor_res.hblank ||
	     !res_user->sensor_res.vblank ||
	     !res_user->sensor_res.interval.denominator ||
	     !res_user->sensor_res.interval.numerator)) {
		dev_info(dev,
			 "%s:pipe(%d): sensor info MUST be provided (TEST_PATTERN case)\n",
			 __func__, pipeline->id);
		/*test pattern case resource default copy*/
		pipeline->res_config.raw_num_used = 1;
		pipeline->res_config.bin_enable = 0;
		pipeline->res_config.tgo_pxl_mode = 1;
		pipeline->res_config.raw_path = 0;
		pipeline->res_config.hwn_limit_min = 1;
		pipeline->res_config.raw_feature = res_user->raw_res.feature;
		pipeline->feature_pending = res_user->raw_res.feature;
		pipeline->feature_active = res_user->raw_res.feature;
	}

	/* check user value of raw input parameters */
	if (res_user->raw_res.feature == 0xFFFFFFFF)
		res_user->raw_res.feature = 0;

	if (res_user->raw_res.bin == 0xFF)
		res_user->raw_res.bin = 0;

	if (res_user->raw_res.strategy == 0xFFFF)
		res_user->raw_res.strategy = RESOURCE_STRATEGY_QRP;

	if (res_user->raw_res.raw_max == 0xFF)
		res_user->raw_res.raw_max = 2;

	if (res_user->raw_res.raw_min == 0xFF)
		res_user->raw_res.raw_min = 1;

	if (res_user->raw_res.raw_min > 1)
		res_user->raw_res.strategy = RESOURCE_STRATEGY_QPR;

	if (res_user->raw_res.path_sel == 0xFF)
		res_user->raw_res.path_sel = 0;

	dev_info(dev,
		 "%s:pipe(%d): driver adjusted: raw:%lld/%d/%d/%d/%d/%d/%d/%d/%lld\n",
		 __func__, pipeline->id,
		 res_user->raw_res.feature, res_user->raw_res.bin,
		 res_user->raw_res.path_sel, res_user->raw_res.raw_max,
		 res_user->raw_res.raw_min, res_user->raw_res.raw_used,
		 res_user->raw_res.strategy, res_user->raw_res.pixel_mode,
		 res_user->raw_res.throughput);

	return 0;
}

static s64 mtk_cam_calc_pure_m2m_pixelrate(s64 width, s64 height,
					   struct v4l2_fract *interval)
{
/* process + r/wdma margin = (1 + 5%) x (1 + 10%) */
#define PURE_M2M_PROCESS_MARGIN_N 11550
#define PURE_M2M_PROCESS_MARGIN_D 10000
	u64 prate = 0;
	int fps_n = interval->numerator;
	int fps_d = interval->denominator;

	prate = width * height * fps_d * PURE_M2M_PROCESS_MARGIN_N;
	do_div(prate, fps_n * PURE_M2M_PROCESS_MARGIN_D);

	pr_info("%s:width:%lld height:%lld interval:%d/%d prate:%llu\n",
		__func__, width, height, fps_n, fps_d, prate);
	return prate;
}

static int mtk_cam_raw_try_res_ctrl(struct mtk_raw_pipeline *pipeline,
				    struct mtk_cam_resource *res_user,
				    struct mtk_cam_resource_config *res_cfg,
				    struct v4l2_mbus_framefmt *sink_fmt)
{
	s64 prate = 0;
	int width, height;
	struct device *dev = pipeline->raw->devs[pipeline->id];

	res_cfg->bin_limit = res_user->raw_res.bin; /* 1: force bin on */
	res_cfg->frz_limit = 0;
	res_cfg->hwn_limit_max = res_user->raw_res.raw_max;
	res_cfg->hwn_limit_min = res_user->raw_res.raw_min;
	res_cfg->hblank = res_user->sensor_res.hblank;
	res_cfg->vblank = res_user->sensor_res.vblank;
	res_cfg->sensor_pixel_rate = res_user->sensor_res.pixel_rate;
	res_cfg->res_plan = res_user->raw_res.strategy;
	res_cfg->raw_feature = res_user->raw_res.feature;
	res_cfg->raw_path = res_user->raw_res.path_sel;

	if (res_user->sensor_res.cust_pixel_rate)
		prate = res_user->sensor_res.cust_pixel_rate;
	else if (mtk_cam_feature_is_pure_m2m(res_cfg->raw_feature))
		prate = mtk_cam_calc_pure_m2m_pixelrate(sink_fmt->width, sink_fmt->height,
							&res_cfg->interval);
	else
		prate = mtk_cam_seninf_calc_pixelrate(pipeline->raw->cam_dev,
						      sink_fmt->width,
						      sink_fmt->height,
						      res_user->sensor_res.hblank,
						      res_user->sensor_res.vblank,
						      res_user->sensor_res.interval.denominator,
						      res_user->sensor_res.interval.numerator,
						      res_user->sensor_res.pixel_rate);

	mtk_cam_raw_resource_calc(dev_get_drvdata(pipeline->raw->cam_dev), res_cfg,
				  prate, res_cfg->res_plan, sink_fmt->width,
				  sink_fmt->height, &width, &height);

	if (res_user->raw_res.bin && !res_cfg->bin_enable) {
		dev_err(dev,
			"%s:pipe(%d): res calc failed on fource bin: user(%d)/bin_enable(%d)\n",
			__func__, pipeline->id,	res_user->raw_res.bin,
			res_cfg->bin_enable);
		return -EINVAL;
	}

	if (res_cfg->raw_num_used > res_user->raw_res.raw_max ||
	    res_cfg->raw_num_used < res_user->raw_res.raw_min) {
		dev_err(dev,
			"%s:pipe(%d): res calc failed on raw used: user(%d/%d)/raw_num_used(%d)\n",
			__func__, pipeline->id, res_user->raw_res.raw_max,
			res_user->raw_res.raw_min, res_cfg->raw_num_used);
	}

	res_user->raw_res.pixel_mode = res_cfg->tgo_pxl_mode;
	res_user->raw_res.raw_used = res_cfg->raw_num_used;
	if (res_cfg->bin_limit == BIN_AUTO)
		res_user->raw_res.bin = res_cfg->bin_enable;
	else
		res_user->raw_res.bin = res_cfg->bin_limit;

	dev_info(dev,
		 "%s:pipe(%d): res calc result: raw_used(%d)/bin(%d)/pixelmode(%d)/strategy(%d)\n",
		 __func__, pipeline->id, res_user->raw_res.raw_used,
		 res_user->raw_res.bin, res_user->raw_res.pixel_mode,
		 res_user->raw_res.strategy);

	/**
	 * Other output not reveal to user now:
	 * res_cfg->res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	 * res_cfg->clk_target;
	 * res_cfg->frz_enable;
	 * res_cfg->frz_ratio;
	 * res_cfg->tgo_pxl_mode;
	 */
	if (width != sink_fmt->width || height != sink_fmt->height) {
		dev_info(dev,
			 "%s:pipe(%d): size adjust info: raw: sink(%d,%d) res:(%d,%d)\n",
			__func__, pipeline->id, sink_fmt->width,
			sink_fmt->height, width, height);
	}

	return 0;
}

static int mtk_cam_raw_set_res_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_resource *res_user;
	struct v4l2_mbus_framefmt sink_fmt;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];
	res_user = (struct mtk_cam_resource *)ctrl->p_new.p;

	ret = mtk_cam_raw_res_store(pipeline, res_user);
	pipeline->user_res = *res_user;
	if (media_entity_is_streaming(&pipeline->subdev.entity)) {
		/* If the pipeline is streaming, pending the change */
		dev_dbg(dev, "%s:pipe(%d): pending res calc has not been supported except bin\n",
			__func__, pipeline->id);
		return ret;
	}

	dev_dbg(dev,
		"%s:pipe(%d):streaming(%d), feature_pending(0x%llx), raw_res.feature(0x%llx), feature_active(0x%llx)\n",
		__func__, pipeline->id, media_entity_is_streaming(&pipeline->subdev.entity),
		pipeline->feature_pending, pipeline->user_res.raw_res.feature,
		pipeline->feature_active);

	ret = mtk_cam_raw_res_copy_fmt_from_user(pipeline, res_user, &sink_fmt);
	if (ret)
		return ret;

	pipeline->res_config.sink_fmt = sink_fmt;
	ret = mtk_cam_raw_try_res_ctrl(pipeline, res_user,
				       &pipeline->res_config, &sink_fmt);

	if (ret)
		return -EINVAL;

	return ret;
}

static int mtk_raw_set_res_ctrl(struct device *dev, struct v4l2_ctrl *ctrl,
				struct mtk_cam_resource_config *res_cfg,
				int pipe_id)
{
	int ret = 0;
	struct mtk_raw_pipeline *pipeline;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);

	if (ctrl->type == V4L2_CTRL_TYPE_INTEGER64)
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%lld)\n", __func__,
			pipe_id, ctrl->name, *ctrl->p_new.p_s64);
	else
		dev_dbg(dev, "%s:pipe(%d):(name:%s, val:%d)\n", __func__,
			pipe_id, ctrl->name, ctrl->val);

	mutex_lock(&res_cfg->resource_lock);
	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT:
		res_cfg->hwn_limit_max = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_BIN_LIMIT:
		res_cfg->bin_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_FRZ_LIMIT:
		res_cfg->frz_limit = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY:
		res_cfg->res_plan = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_RAW_PATH_SELECT:
		res_cfg->raw_path = ctrl->val;
		break;
	case V4L2_CID_HBLANK:
		res_cfg->hblank = ctrl->val;
		break;
	case V4L2_CID_VBLANK:
		res_cfg->vblank = ctrl->val;
		break;
	case V4L2_CID_MTK_CAM_PIXEL_RATE:
		res_cfg->sensor_pixel_rate = *ctrl->p_new.p_s64;
		break;
	case V4L2_CID_MTK_CAM_SYNC_ID:
		pipeline->sync_id = *ctrl->p_new.p_s64;
		break;
	default:
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x,val:%d) not handled\n",
			 __func__, pipe_id, ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&res_cfg->resource_lock);

	return ret;
}

static int mtk_cam_raw_try_ctrl(struct v4l2_ctrl *ctrl)
{
	struct device *dev;
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_resource *res_user;
	struct mtk_cam_resource_config res_cfg = {
		.interval = {
			.numerator = 1,
			.denominator = 30
		},
	};
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC:
		res_user = (struct mtk_cam_resource *)ctrl->p_new.p;
		ret = mtk_cam_raw_res_store(pipeline, res_user);
		if (ret)
			break;

		ret = mtk_cam_raw_res_copy_fmt_from_user(pipeline, res_user,
							 &res_cfg.sink_fmt);
		if (ret) {
			ret = 0;
			/* only corret the user's input, it may be request-based try and set fmt */
			dev_dbg(dev, "%s:pipe(%d): no sink_fmt from user, not calc res\n",
				__func__, pipeline->id);
			break;
		}

		dev_dbg(dev, "%s:pipe(%d): res ctrl start\n", __func__,
			pipeline->id);
			ret = mtk_cam_raw_try_res_ctrl(pipeline, res_user, &res_cfg,
						       &res_cfg.sink_fmt);
			if (ret)
				break;

		dev_dbg(dev,
			"%s:pipe(%d):streaming(%d), feature_pending(0x%llx), feature_active(0x%llx)\n",
			__func__, pipeline->id, media_entity_is_streaming(&pipeline->subdev.entity),
			pipeline->feature_pending, pipeline->feature_active);

		dev_dbg(dev, "%s:pipe(%d): res ctrl end\n", __func__,
			pipeline->id);
			break;
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE:
		dev_info(dev,
			 "%s:pipe(%d): skip V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE: %d\n",
			 __func__, pipeline->id, ctrl->val);
		ret = 0; /* no support */
		break;
	/* skip control value checks */
	case V4L2_CID_MTK_CAM_FEATURE:
	case V4L2_CID_MTK_CAM_CAMSYS_HW_MODE:
		ret = 0;
		break;
	default:
		ret = mtk_raw_set_res_ctrl(pipeline->raw->devs[pipeline->id],
					   ctrl, &pipeline->try_res_config,
					   pipeline->id);
		break;
	}

	return ret;
}

static int mtk_cam_raw_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct device *dev;
	struct mtk_raw_pipeline *pipeline;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC:
		/**
		 * It also updates V4L2_CID_MTK_CAM_FEATURE and
		 * V4L2_CID_MTK_CAM_RAW_PATH_SELECT to device
		 */
		ret = mtk_cam_raw_set_res_ctrl(ctrl);
		break;
	case V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE:
		/**
		 * sensor_mode_update should be reset by driver after the completion of
		 * resource updating (seamless switch)
		 */
		pipeline->sensor_mode_update = ctrl->val;
		dev_info(dev, "%s:pipe(%d):streaming(%d), sensor_mode_update(%d)\n",
			 __func__, pipeline->id,
			 media_entity_is_streaming(&pipeline->subdev.entity),
			 pipeline->sensor_mode_update);
		break;
	case V4L2_CID_MTK_CAM_FEATURE:
		pipeline->feature_pending = *ctrl->p_new.p_s64;

		dev_dbg(dev,
			"%s:pipe(%d):streaming(%d), feature_pending(0x%llx), feature_active(0x%llx)\n",
			__func__, pipeline->id, media_entity_is_streaming(&pipeline->subdev.entity),
			pipeline->feature_pending, pipeline->feature_active);
		ret = 0;
		break;
	case V4L2_CID_MTK_CAM_CAMSYS_HW_MODE:
		pipeline->hw_mode = *ctrl->p_new.p_s64;

		dev_dbg(dev,
			"%s:pipe(%d):streaming(%d), hw_mode(0x%llx)\n",
			__func__, pipeline->id, media_entity_is_streaming(&pipeline->subdev.entity),
			pipeline->hw_mode);

		ret = 0;
		break;
	default:
		ret = mtk_raw_set_res_ctrl(pipeline->raw->devs[pipeline->id],
					   ctrl, &pipeline->res_config,
					   pipeline->id);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops cam_ctrl_ops = {
	.g_volatile_ctrl = mtk_cam_raw_get_ctrl,
	.s_ctrl = mtk_cam_raw_set_ctrl,
	.try_ctrl = mtk_cam_raw_try_ctrl,
};

static int mtk_raw_pde_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info_p;
	struct device *dev;
	int ret = 0;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	pde_cfg = &pipeline->pde_config;
	pde_info_p = ctrl->p_new.p;
	dev = pipeline->raw->devs[pipeline->id];

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_PDE_INFO:
		pde_info_p->pdo_max_size = pde_cfg->pde_info.pdo_max_size;
		pde_info_p->pdi_max_size = pde_cfg->pde_info.pdi_max_size;
		pde_info_p->pd_table_offset = pde_cfg->pde_info.pd_table_offset;
		break;
	default:
		dev_info(dev, "%s(id:0x%x,val:%d) is not handled\n",
			 __func__, ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	return ret;
}

static int mtk_raw_pde_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_raw_pde_config *pde_cfg;
	struct mtk_cam_pde_info *pde_info_p;
	struct device *dev;
	int ret = 0;
	struct mtk_cam_video_device *node;
	struct mtk_cam_dev_node_desc *desc;
	const struct v4l2_format *default_fmt;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	pde_cfg = &pipeline->pde_config;
	pde_info_p = ctrl->p_new.p;
	dev = pipeline->raw->devs[pipeline->id];

	node = &pipeline->vdev_nodes[MTK_RAW_META_IN - MTK_RAW_SINK_NUM];
	desc = &node->desc;
	default_fmt = &desc->fmts[desc->default_fmt_idx].vfmt;

	switch (ctrl->id) {
	case V4L2_CID_MTK_CAM_PDE_INFO:
		if (!pde_info_p->pdo_max_size || !pde_info_p->pdi_max_size) {
			dev_info(dev,
				 "%s:pdo_max_sz(%d)/pdi_max_sz(%d) cannot be 0\n",
				 __func__, pde_info_p->pdo_max_size,
				 pde_info_p->pdi_max_size);
			ret = -EINVAL;
			break;
		}

		pde_cfg->pde_info.pdo_max_size = pde_info_p->pdo_max_size;
		pde_cfg->pde_info.pdi_max_size = pde_info_p->pdi_max_size;
		pde_cfg->pde_info.pd_table_offset =
			default_fmt->fmt.meta.buffersize;
		break;
	default:
		dev_info(dev, "%s(id:0x%x,val:%d) is not handled\n",
			 __func__, ctrl->id, ctrl->val);
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops cam_pde_ctrl_ops = {
	.g_volatile_ctrl = mtk_raw_pde_get_ctrl,
	.s_ctrl = mtk_raw_pde_set_ctrl,
	.try_ctrl = mtk_raw_pde_set_ctrl,
};

static const struct v4l2_ctrl_config hwn_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT,
	.name = "Engine resource limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_LIMIT,
	.name = "Binning limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xfff,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config frz_limit = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_LIMIT,
	.name = "Resizer limitation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config hwn = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config raw_path = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_PATH_SELECT,
	.name = "Raw Path",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config hwn_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_USED_ENGINE_TRY,
	.name = "Engine resource",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 2,
	.step = 1,
	.def = 2,
};

static const struct v4l2_ctrl_config bin_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_BIN_TRY,
	.name = "Binning",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config frz_try = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FRZ_TRY,
	.name = "Resizer",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 70,
	.max = 100,
	.step = 1,
	.def = 100,
};

static const struct v4l2_ctrl_config res_plan_policy = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY,
	.name = "Resource planning policy",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 10,
	.step = 1,
	.def = 1,
};

static const struct v4l2_ctrl_config res_pixel_rate = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_PIXEL_RATE,
	.name = "Resource pixel rate",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = 0xFFFFFFFF,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config frame_sync_id = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_SYNC_ID,
	.name = "Frame sync id",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = -1,
	.max = 0x7FFFFFFF,
	.step = 1,
	.def = -1,
};

static const struct v4l2_ctrl_config mtk_feature = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_FEATURE,
	.name = "Mediatek camsys feature",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = RAW_FUNCTION_END,
	.step = 1,
	.def = 0,
};

static struct v4l2_ctrl_config cfg_res_ctrl = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC,
	.name = "resource ctrl",
	.type = V4L2_CTRL_COMPOUND_TYPES, /* V4L2_CTRL_TYPE_U32,*/
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.max = 0xffffffff,
	.step = 1,
	.dims = {sizeof(struct mtk_cam_resource)},
};

static struct v4l2_ctrl_config cfg_res_update = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE,
	.name = "resource update",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xf,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config cfg_pde_info = {
	.ops = &cam_pde_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_PDE_INFO,
	.name = "pde information",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
	.min = 0,
	.max = 0x1fffffff,
	.step = 1,
	.def = 0,
	.dims = {sizeof_u32(struct mtk_cam_pde_info)},
};

static const struct v4l2_ctrl_config mtk_camsys_hw_mode = {
	.ops = &cam_ctrl_ops,
	.id = V4L2_CID_MTK_CAM_CAMSYS_HW_MODE,
	.name = "Mediatek camsys hardware mode",
	.type = V4L2_CTRL_TYPE_INTEGER64,
	.min = 0,
	.max = 0x7FFFFFFF,
	.step = 1,
	.def = DEFAULT,
};

static void mtk_cam_raw_toggle_db(struct mtk_raw_device *dev)
{
	int value;
	u32 val_cfg, val_dcif_ctl, val_sen;

	value = readl_relaxed(dev->base + REG_CTL_MISC);
	val_cfg = readl_relaxed(dev->base_inner + REG_TG_PATH_CFG);
	val_dcif_ctl = readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL);
	val_sen = readl_relaxed(dev->base_inner + REG_TG_SEN_MODE);
	writel_relaxed(value & ~CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* make sure committed */
	writel_relaxed(value | CTL_DB_EN, dev->base + REG_CTL_MISC);
	wmb(); /* make sure committed */
	dev_info(dev->dev,
		 "%s,  read inner AsIs->ToBe TG_PATH_CFG:0x%x->0x%x, TG_DCIF:0x%x->0x%x, TG_SEN:0x%x->0x%x\n",
		 __func__, val_cfg,
		 readl_relaxed(dev->base_inner + REG_TG_PATH_CFG), val_dcif_ctl,
		 readl_relaxed(dev->base_inner + REG_TG_DCIF_CTL), val_sen,
		 readl_relaxed(dev->base_inner + REG_TG_SEN_MODE));
}

void mtk_cam_raw_trigger_rawi(struct mtk_raw_device *dev, struct mtk_cam_ctx *ctx,
			      signed int hw_scene)
{
#define TRIGGER_RAWI_R6 0x10
#define TRIGGER_RAWI_R2 0x01

	u32 cmd = 0;

	cmd = TRIGGER_RAWI_R2;

	writel_relaxed(cmd, dev->base + REG_CTL_RAWI_TRIG);
	wmb(); /* make sure committed */
}

void mtk_cam_raw_apply_cq(struct mtk_raw_device *dev, dma_addr_t cq_addr,
			  unsigned int cq_size, unsigned int cq_offset,
			  unsigned int sub_cq_size, unsigned int sub_cq_offset)
{
	dma_addr_t main, sub;

	dev_dbg(dev->dev,
		"apply raw%d cq - addr:0x%pad,size:%d/%d,offset:%d\n",
		dev->id, &cq_addr, cq_size, sub_cq_size, sub_cq_offset);

	main = cq_addr + cq_offset;
	sub = cq_addr + sub_cq_offset;

	writel_relaxed(dmaaddr_lsb(main),
		       dev->base + REG_CQ_THR0_BASEADDR);
	writel_relaxed(dmaaddr_msb(main),
		       dev->base + REG_CQ_THR0_BASEADDR_MSB);
	writel_relaxed(cq_size,
		       dev->base + REG_CQ_THR0_DESC_SIZE);
	writel_relaxed(dmaaddr_lsb(sub),
		       dev->base + REG_CQ_SUB_THR0_BASEADDR_2);
	writel_relaxed(dmaaddr_msb(sub),
		       dev->base + REG_CQ_SUB_THR0_BASEADDR_MSB_2);
	writel_relaxed(sub_cq_size,
		       dev->base + REG_CQ_SUB_THR0_DESC_SIZE_2);

	/* note: legacy CQ just trigger 1st frame */
	/* USINGSCQ and always trigger now */
	writel_relaxed(CAMCTL_CQ_THR0_DONE_ST,
		       dev->base + REG_CTL_RAW_INT6_EN);
	writel_relaxed(CAMCTL_CQ_THRSUB_DONE_EN,
		       dev->base + REG_CTL_RAW_INT7_EN);
	writel(CTL_CQ_THR0_START, dev->base + REG_CTL_START);

	wmb(); /* make sure committed */
}

/* sw check again for rawi dcif case */
static bool mtk_cam_raw_is_dma_idle(struct mtk_raw_device *dev)
{
	bool ret = false;
	int chasing_stat;
	int raw_rst_stat = readl(dev->base + REG_DMA_SOFT_RST_STAT);
	int raw_rst_stat2 = readl(dev->base + REG_DMA_SOFT_RST_STAT2);
	int yuv_rst_stat = readl(dev->yuv_base + REG_DMA_SOFT_RST_STAT);

	if (raw_rst_stat2 != 0x7 || yuv_rst_stat != 0xfffffff)
		return false;

	/* check beside rawi_r2/r3/r5*/
	if (~raw_rst_stat & 0x7fffffda)
		return false;

	if (~raw_rst_stat & RST_STAT_RAWI_R2) { /* RAWI_R2 */
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS);
		ret = ((chasing_stat & RAWI_R2_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R2_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x1))
			? true : false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%x ret=%d\n",
			 __func__, chasing_stat, ret);
	}
	if (~raw_rst_stat & RST_STAT_RAWI_R3) {
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS);
		ret = ((chasing_stat & RAWI_R3_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R3_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x80))
			? true : false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%x, ret=%d\n",
			 __func__, chasing_stat, ret);
	}
	if (~raw_rst_stat & RST_STAT_RAWI_R5) {
		chasing_stat = readl(dev->base + REG_DMA_DBG_CHASING_STATUS2);
		ret = ((chasing_stat & RAWI_R5_SMI_REQ_ST) == 0 &&
		 (readl(dev->base + REG_RAWI_R5_BASE + DMA_OFFSET_SPECIAL_DCIF)
			& DC_CAMSV_STAGER_EN) &&
		 (readl(dev->base + REG_CTL_MOD6_EN) & 0x1000))
			? true : false;
		dev_info(dev->dev, "%s: chasing_stat: 0x%x, ret=%d\n",
			 __func__, chasing_stat, ret);
	}

	return ret;
}

void mtk_cam_raw_reset(struct mtk_raw_device *dev)
{
	int sw_ctl;
	int ret;

	dev_dbg(dev->dev, "camsys | trigger %s\n", __func__);

	/* Disable all DMA DCM before reset */
	writel(0x00000fff, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0007ffff, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0xffffffff, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	/* enable CQI_R1 ~ R4 before reset and make sure loaded to inner */
	writel(readl(dev->base + REG_CTL_MOD6_EN) | 0x78000,
	       dev->base + REG_CTL_MOD6_EN);
	mtk_cam_raw_toggle_db(dev);

	writel(0, dev->base + REG_CTL_SW_CTL);
	writel(1, dev->base + REG_CTL_SW_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base + REG_CTL_SW_CTL, sw_ctl,
				 sw_ctl & 0x2,
				 1 /* delay, us */,
				 5000 /* timeout, us */);

	if (ret < 0 && !mtk_cam_raw_is_dma_idle(dev)) {
		dev_info(dev->dev,
			 "%s: timeout: tg_sen_mode: 0x%x, ctl_en: 0x%x, mod6_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x,rst_stat:0x%x,rst_stat2:0x%x,yuv_rst_stat:0x%x\n",
			 __func__,
			 readl(dev->base + REG_TG_SEN_MODE),
			 readl(dev->base + REG_CTL_EN),
			 readl(dev->base + REG_CTL_MOD6_EN),
			 readl(dev->base + REG_CTL_SW_CTL),
			 readl(dev->base + REG_FRAME_SEQ_NUM),
			 readl(dev->base + REG_DMA_SOFT_RST_STAT),
			 readl(dev->base + REG_DMA_SOFT_RST_STAT2),
			 readl(dev->yuv_base + REG_DMA_SOFT_RST_STAT));

		goto RESET_FAILURE;
	}

	/* do hw rst */
	writel(4, dev->base + REG_CTL_SW_CTL);
	writel(0, dev->base + REG_CTL_SW_CTL);

RESET_FAILURE:
	/* Enable all DMA DCM back */
	writel(0x0, dev->base + REG_CTL_RAW_MOD5_DCM_DIS);
	writel(0x0, dev->base + REG_CTL_RAW_MOD6_DCM_DIS);
	writel(0x0, dev->yuv_base + REG_CTL_RAW_MOD5_DCM_DIS);

	wmb(); /* make sure committed */
}

static void mtk_cam_raw_reset_reg(struct mtk_raw_device *dev)
{
	int cq_en, sw_done;

	dev_dbg(dev->dev,
		"[%s++] CQ_EN/SW_SUB_CTL/SW_DONE [in] 0x%x/0x%x/0x%x [out] 0x%x/0x%x/0x%x\n",
		__func__,
		readl_relaxed(dev->base_inner + REG_CQ_EN),
		readl_relaxed(dev->base_inner + REG_CTL_SW_SUB_CTL),
		readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE),
		readl_relaxed(dev->base + REG_CQ_EN),
		readl_relaxed(dev->base + REG_CTL_SW_SUB_CTL),
		readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE));

	cq_en = readl_relaxed(dev->base_inner + REG_CQ_EN);
	sw_done = readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE);

	cq_en = cq_en & (~SCQ_SUBSAMPLE_EN) & (~SCQ_STAGGER_MODE);
	writel(cq_en, dev->base_inner + REG_CQ_EN);
	writel(cq_en, dev->base + REG_CQ_EN);

	dev_dbg(dev->dev, "[--] try to disable SCQ_STAGGER_MODE: CQ_EN(0x%x)\n",
		cq_en);

	writel(sw_done & (~SW_DONE_SAMPLE_EN), dev->base_inner + REG_CTL_SW_PASS1_DONE);
	writel(sw_done & (~SW_DONE_SAMPLE_EN), dev->base + REG_CTL_SW_PASS1_DONE);

	writel(0, dev->base_inner + REG_CTL_SW_SUB_CTL);
	writel(0, dev->base + REG_CTL_SW_SUB_CTL);

	wmb(); /* make sure committed */

	dev_dbg(dev->dev,
		"[%s--] CQ_EN/SW_SUB_CTL/SW_DONE [in] 0x%x/0x%x/0x%x [out] 0x%x/0x%x/0x%x\n",
		__func__, readl_relaxed(dev->base_inner + REG_CQ_EN),
		readl_relaxed(dev->base_inner + REG_CTL_SW_SUB_CTL),
		readl_relaxed(dev->base_inner + REG_CTL_SW_PASS1_DONE),
		readl_relaxed(dev->base + REG_CQ_EN),
		readl_relaxed(dev->base + REG_CTL_SW_SUB_CTL),
		readl_relaxed(dev->base + REG_CTL_SW_PASS1_DONE));
}

void mtk_cam_raw_dump_aa_info(struct mtk_cam_ctx *ctx, struct mtk_ae_debug_data *ae_info)
{
	struct mtk_raw_device *raw_dev = NULL;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	int i;

	for (i = 0; i < ctx->cam->num_raw_devices; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			struct device *dev = ctx->cam->raw.devs[i];

			raw_dev = dev_get_drvdata(dev);
			ae_info->obc_r1_sum[0] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R1_R_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R1_R_SUM_L);
			ae_info->obc_r2_sum[0] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R2_R_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R2_R_SUM_L);
			ae_info->obc_r3_sum[0] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R3_R_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R3_R_SUM_L);
			ae_info->ltm_sum[0] +=
				((u64)readl(raw_dev->base + REG_LTM_AE_DEBUG_R_MSB) << 32) |
				readl(raw_dev->base + REG_LTM_AE_DEBUG_R_LSB);
			ae_info->aa_sum[0] +=
				((u64)readl(raw_dev->base + REG_AA_R_SUM_H) << 32) |
				readl(raw_dev->base + REG_AA_R_SUM_L);

			ae_info->obc_r1_sum[1] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R1_B_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R1_B_SUM_L);
			ae_info->obc_r2_sum[1] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R2_B_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R2_B_SUM_L);
			ae_info->obc_r3_sum[1] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R3_B_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R3_B_SUM_L);
			ae_info->ltm_sum[1] +=
				((u64)readl(raw_dev->base + REG_LTM_AE_DEBUG_B_MSB) << 32) |
				readl(raw_dev->base + REG_LTM_AE_DEBUG_B_LSB);
			ae_info->aa_sum[1] +=
				((u64)readl(raw_dev->base + REG_AA_B_SUM_H) << 32) |
				readl(raw_dev->base + REG_AA_B_SUM_L);

			ae_info->obc_r1_sum[2] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R1_GR_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R1_GR_SUM_L);
			ae_info->obc_r2_sum[2] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R2_GR_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R2_GR_SUM_L);
			ae_info->obc_r3_sum[2] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R3_GR_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R3_GR_SUM_L);
			ae_info->ltm_sum[2] +=
				((u64)readl(raw_dev->base + REG_LTM_AE_DEBUG_GR_MSB) << 32) |
				readl(raw_dev->base + REG_LTM_AE_DEBUG_GR_LSB);
			ae_info->aa_sum[2] +=
				((u64)readl(raw_dev->base + REG_AA_GR_SUM_H) << 32) |
				readl(raw_dev->base + REG_AA_GR_SUM_L);

			ae_info->obc_r1_sum[3] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R1_GB_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R1_GB_SUM_L);
			ae_info->obc_r2_sum[3] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R2_GB_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R2_GB_SUM_L);
			ae_info->obc_r3_sum[3] +=
				((u64)readl(raw_dev->base + OFFSET_OBC_R3_GB_SUM_H) << 32) |
				readl(raw_dev->base + OFFSET_OBC_R3_GB_SUM_L);
			ae_info->ltm_sum[3] +=
				((u64)readl(raw_dev->base + REG_LTM_AE_DEBUG_GB_MSB) << 32) |
				readl(raw_dev->base + REG_LTM_AE_DEBUG_GB_LSB);
			ae_info->aa_sum[3] +=
				((u64)readl(raw_dev->base + REG_AA_GB_SUM_H) << 32) |
				readl(raw_dev->base + REG_AA_GB_SUM_L);
		}
	}
}

static int reset_msgfifo(struct mtk_raw_device *dev)
{
	atomic_set(&dev->is_fifo_overflow, 0);
	return kfifo_init(&dev->msg_fifo, dev->msg_buffer, dev->fifo_size);
}

static int push_msgfifo(struct mtk_raw_device *dev,
			struct mtk_camsys_irq_info *info)
{
	int len;

	if (unlikely(kfifo_avail(&dev->msg_fifo) < sizeof(*info))) {
		atomic_set(&dev->is_fifo_overflow, 1);
		return -1;
	}

	len = kfifo_in(&dev->msg_fifo, info, sizeof(*info));
	WARN_ON(len != sizeof(*info));

	return 0;
}

static void set_fifo_size(void __iomem *dma_base, int fifo_size)
{
	writel_relaxed((0x1 << 28) | (fifo_size & 0xFFF),
		       dma_base + DMA_OFFSET_CON0);
}

static void set_fifo_threshold(void __iomem *dma_base)
{
	u32 fifo_size = 0;

	fifo_size = readl_relaxed(dma_base + DMA_OFFSET_CON0) & 0xFFF;
	writel_relaxed((0x1 << 28) |
		       ((fifo_size * 1 / 4) & 0xFFF) << 16 |
		       ((fifo_size * 1 / 8) & 0xFFF),
		       dma_base + DMA_OFFSET_CON1);
	writel_relaxed((0x1 << 28) |
		       ((fifo_size * 1 / 2) & 0xFFF) << 16 |
		       ((fifo_size * 3 / 8) & 0xFFF),
		       dma_base + DMA_OFFSET_CON2);
	writel_relaxed((0x1 << 31) |
		       ((fifo_size * 2 / 3) & 0xFFF) << 16 |
		       ((fifo_size * 13 / 24) & 0xFFF),
		       dma_base + DMA_OFFSET_CON3);
	writel_relaxed((0x1 << 31) |
		       ((fifo_size * 3 / 8) & 0xFFF) << 16 |
		       ((fifo_size * 1 / 4) & 0xFFF),
		       dma_base + DMA_OFFSET_CON4);
}

static void init_dma_threshold(struct mtk_raw_device *dev)
{
	struct mtk_cam_device *cam_dev;

	cam_dev = dev->cam;

	/* work around mt8188 hw default fifo size value error */
	set_fifo_size(dev->base + REG_IMGO_R1_BASE, 0x3C0);
	set_fifo_size(dev->yuv_base + REG_YUVCO_R1_BASE, 0x200);
	set_fifo_size(dev->yuv_base + REG_YUVDO_R1_BASE, 0x80);
	set_fifo_size(dev->yuv_base + REG_YUVO_R3_BASE, 0xc0);
	set_fifo_size(dev->yuv_base + REG_YUVBO_R3_BASE, 0x60);
	set_fifo_size(dev->yuv_base + REG_YUVCO_R3_BASE, 0x40);
	set_fifo_size(dev->yuv_base + REG_YUVO_R2_BASE, 0x80);
	set_fifo_size(dev->yuv_base + REG_YUVBO_R2_BASE, 0x40);
	set_fifo_size(dev->yuv_base + REG_YUVO_R4_BASE, 0x40);
	set_fifo_size(dev->yuv_base + REG_YUVBO_R4_BASE, 0x40);
	set_fifo_size(dev->yuv_base + REG_YUVO_R5_BASE, 0x40);
	set_fifo_threshold(dev->base + REG_IMGO_R1_BASE);
	set_fifo_threshold(dev->base + REG_FHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAHO_R1_BASE);
	set_fifo_threshold(dev->base + REG_PDO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAO_R1_BASE);
	set_fifo_threshold(dev->base + REG_AFO_R1_BASE);
	set_fifo_threshold(dev->base + REG_PDO_R1_BASE);

	set_fifo_threshold(dev->yuv_base + REG_YUVO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVDO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVCO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_LTMSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TSFSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TSFSO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_FLKO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_UFEO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R4_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_YUVBO_R5_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_RZH1N2TBO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R2_BASE);
	set_fifo_threshold(dev->yuv_base + REG_DRZS4NO_R3_BASE);
	set_fifo_threshold(dev->yuv_base + REG_ACTSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSBO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSHO_R1_BASE);
	set_fifo_threshold(dev->yuv_base + REG_TNCSYO_R1_BASE);

	set_fifo_threshold(dev->base + REG_RAWI_R2_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R2_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R3_BASE);
	set_fifo_threshold(dev->base + REG_UFDI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R2_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R3_BASE);
	set_fifo_threshold(dev->base + REG_CQI_R4_BASE);
	set_fifo_threshold(dev->base + REG_LSCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R1_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R2_BASE);
	set_fifo_threshold(dev->base + REG_BPCI_R3_BASE);
	set_fifo_threshold(dev->base + REG_PDI_R1_BASE);
	set_fifo_threshold(dev->base + REG_AAI_R1_BASE);
	set_fifo_threshold(dev->base + REG_CACI_R1_BASE);
	set_fifo_threshold(dev->base + REG_RAWI_R6_BASE);

	writel_relaxed(0x1, cam_dev->base + REG_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_HALT6_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_ULTRA_HALT6_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT1_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT2_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT3_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT4_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT5_EN);
	writel_relaxed(0x1, cam_dev->base + REG_PREULTRA_HALT6_EN);
}

static int get_fps_ratio(struct mtk_raw_device *dev)
{
	int fps = (dev->pipeline->res_config.interval.numerator > 0) ?
			(dev->pipeline->res_config.interval.denominator /
			dev->pipeline->res_config.interval.numerator) : 0;

	if (fps <= 30)
		return 1;
	else if (fps <= 60)
		return 2;
	else
		return 1;
}

void mtk_cam_raw_initialize(struct mtk_raw_device *dev, int is_sub)
{
	u32 val;

	/* USINGSCQ */
	val = readl_relaxed(dev->base + REG_CQ_EN);
	writel_relaxed(val | SCQ_EN, dev->base + REG_CQ_EN);
	writel_relaxed(0xffffffff, dev->base + REG_SCQ_START_PERIOD);

	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_THR0_CTL);
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_CQ_SUB_THR0_CTL);
	writel_relaxed(CAMCTL_CQ_THR0_DONE_ST, dev->base + REG_CTL_RAW_INT6_EN);
	writel_relaxed(CAMCTL_CQ_THRSUB_DONE_EN, dev->base + REG_CTL_RAW_INT7_EN);

	dev->is_sub = is_sub;
	dev->sof_count = 0;
	dev->vsync_count = 0;
	atomic_set(&dev->vf_en, 0);
	reset_msgfifo(dev);

	init_dma_threshold(dev);

	writel_relaxed(0xFFFE0000,
		       dev->base + REG_FLKO_R1_BASE + DMA_OFFSET_ERR_STAT);

	dev_dbg(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
		readl_relaxed(dev->base + REG_CQ_EN),
		readl_relaxed(dev->base + REG_CQ_THR0_CTL));
}

void mtk_cam_raw_stream_on(struct mtk_raw_device *dev, int on)
{
	u32 val;
	u32 chk_val;
	u32 cfg_val;
	u32 fps_ratio = 1;
	struct mtk_cam_ctx *ctx;

	dev_info(dev->dev, "raw %d %s %d\n", dev->id, __func__, on);
	ctx = mtk_cam_find_ctx(dev->cam, &dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	if (on) {
		/* USINGSCQ */
		val = readl_relaxed(dev->base + REG_TG_TIME_STAMP_CNT);
		val = (val == 0) ? 1 : val;
		fps_ratio = get_fps_ratio(dev);
		dev_info(dev->dev, "VF on - REG_TG_TIME_STAMP_CNT val:%d fps(30x):%d\n",
			 val, fps_ratio);
		writel_relaxed(SCQ_DEADLINE_MS * 1000 * SCQ_DEFAULT_CLK_RATE /
			       (val * 2) / fps_ratio, dev->base + REG_SCQ_START_PERIOD);

		mtk_cam_set_topdebug_rdyreq(dev->dev, dev->base, dev->yuv_base,
					    TG_OVERRUN);
		dev->overrun_debug_dump_cnt = 0;
		if (mtk_cam_is_m2m(ctx)) {
			dev_info(dev->dev, "[%s] M2M view finder disable\n", __func__);
		} else {
			val = readl_relaxed(dev->base + REG_TG_VF_CON);
			val |= TG_VFDATA_EN;
			writel_relaxed(val, dev->base + REG_TG_VF_CON);
			wmb(); /* make sure committed */
		}
		atomic_set(&dev->vf_en, 1);
		dev_info(dev->dev,
			 "%s - CQ_EN:0x%x, CQ_THR0_CTL:0x%8x, TG_VF_CON:0x%8x, SCQ_START_PERIOD:%d\n",
			 __func__,
			 readl_relaxed(dev->base + REG_CQ_EN),
			 readl_relaxed(dev->base + REG_CQ_THR0_CTL),
			 readl_relaxed(dev->base + REG_TG_VF_CON),
			 readl_relaxed(dev->base + REG_SCQ_START_PERIOD));
	} else {
		dev_info(dev->dev, "VF off\n");
		atomic_set(&dev->vf_en, 0);

		writel_relaxed((u32)~CQ_THR0_EN, dev->base + REG_CQ_THR0_CTL);
		wmb(); /* make sure committed */

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val |= 0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		val = readl_relaxed(dev->base + REG_TG_VF_CON);
		val &= ~TG_VFDATA_EN;
		writel(val, dev->base + REG_TG_VF_CON);

		cfg_val = readl_relaxed(dev->base + REG_TG_PATH_CFG);
		cfg_val &= ~0x100;
		writel(cfg_val, dev->base + REG_TG_PATH_CFG);

		wmb(); /* make sure committed */
		/* reset hw after vf off */
		if (readx_poll_timeout(readl, dev->base_inner + REG_TG_VF_CON,
				       chk_val, chk_val == val,
				       1 /* sleep, us */,
				       33000 /* timeout, us*/) < 0) {
			dev_info(dev->dev, "%s: wait vf off timeout: TG_VF_CON 0x%x\n",
				 __func__, chk_val);
		}

		mtk_cam_raw_reset_reg(dev);
	}
}

static int mtk_raw_linebuf_chk(bool b_twin, bool b_bin, bool b_frz, bool b_qbn,
			       bool b_cbn, int tg_x, int *frz_ratio)
{
	int input_x = tg_x;
	/* max line buffer check for frontal binning and resizer */
	if (b_twin) {
		if (input_x > CAM_TWIN_PROCESS_MAX_LINE_BUFFER)
			return LB_CHECK_TWIN;
		input_x = input_x >> 1;
	}
	if (b_cbn) {
		if (input_x > CAM_RAW_CBN_MAX_LINE_BUFFER)
			return LB_CHECK_CBN;
		input_x = input_x >> 1;
	}
	if (b_qbn) {
		if (input_x > CAM_RAW_QBND_MAX_LINE_BUFFER)
			return LB_CHECK_QBN;
		input_x = input_x >> 1;
	}
	if (b_bin) {
		if (input_x > CAM_RAW_BIN_MAX_LINE_BUFFER)
			return LB_CHECK_BIN;
		input_x = input_x >> 1;
	}
	if (input_x <= CAM_RAW_PROCESS_MAX_LINE_BUFFER) {
		return LB_CHECK_OK;
	} else if (b_frz) {
		if (input_x > CAM_RAW_FRZ_MAX_LINE_BUFFER)
			return LB_CHECK_FRZ;

		*frz_ratio = input_x * 100 /
			CAM_RAW_PROCESS_MAX_LINE_BUFFER;
		return LB_CHECK_OK;
	} else {
		return LB_CHECK_RAW;
	}
}

static int mtk_raw_pixelmode_calc(int rawpxl, int b_twin, bool b_bin,
				  bool b_frz, int min_ratio)
{
	int pixelmode = rawpxl;

	pixelmode = (b_twin == 2) ? pixelmode << 2 : pixelmode;
	pixelmode = (b_twin == 1) ? pixelmode << 1 : pixelmode;
	pixelmode = b_bin ? pixelmode << 1 : pixelmode;
	pixelmode = (b_frz && (min_ratio < FRZ_PXLMODE_THRES))
			? pixelmode << 1 : pixelmode;
	pixelmode = (pixelmode > TGO_MAX_PXLMODE) ?
		TGO_MAX_PXLMODE : pixelmode;

	return pixelmode;
}

static void mtk_raw_update_debug_param(struct mtk_cam_device *cam,
				       struct mtk_cam_resource_config *res,
				       int *clk_idx)
{
	/* skip if debug is not enabled */
	if (!debug_raw)
		return;

	dev_dbg(cam->dev, "%s:before:BIN/FRZ/HWN/CLK/pxl=%d/%d(%d)/%d/%d/%d, clk:%d\n",
		__func__, res->bin_enable, res->frz_enable, res->frz_ratio,
		res->raw_num_used, *clk_idx, res->tgo_pxl_mode,
		res->clk_target);

	if (debug_raw_num > 0) {
		dev_info(cam->dev, "DEBUG: force raw_num_used: %d\n",
			 debug_raw_num);
		res->raw_num_used = debug_raw_num;
	}

	if (debug_pixel_mode >= 0) {
		dev_info(cam->dev, "DEBUG: force debug_pixel_mode (log2): %d\n",
			 debug_pixel_mode);
		res->tgo_pxl_mode = debug_pixel_mode;
	}

	if (debug_clk_idx >= 0) {
		dev_info(cam->dev, "DEBUG: force debug_clk_idx: %d\n",
			 debug_clk_idx);
		*clk_idx = debug_clk_idx;
	}

	dev_dbg(cam->dev, "%s:after:BIN/FRZ/HWN/CLK/pxl=%d/%d(%d)/%d/%d/%d, clk:%d\n",
		__func__, res->bin_enable, res->frz_enable, res->frz_ratio,
		res->raw_num_used, *clk_idx, res->tgo_pxl_mode,
		res->clk_target);
}

static bool is_cbn_en(int bin_flag)
{
	switch (bin_flag) {
	case CBN_2X2_ON:
	case CBN_3X3_ON:
	case CBN_4X4_ON:
		return true;
	default:
		return false;
	}
}

static bool mtk_cam_raw_resource_calc(struct mtk_cam_device *cam,
				      struct mtk_cam_resource_config *res,
				      s64 pixel_rate, int res_plan,
				      int in_w, int in_h, int *out_w, int *out_h)
{
	int res_step_type = 0;
	int tgo_pxl_mode = 1;
	int pixel_mode[MTK_CAMSYS_RES_STEP_NUM] = {0};
	int bin_temp = 0, frz_temp = 0, hwn_temp = 0;
	int bin_en = 0, frz_en = 0, twin_en = 0, clk_cur = 0;
	int idx = 0, clk_res = 0, idx_res = 0;
	bool res_found = false;
	int lb_chk_res = -1;
	int frz_ratio = 100;
	int p;

	res->res_plan = res_plan;
	res->pixel_rate = pixel_rate;
	/* test pattern */
	if (res->pixel_rate == 0)
		res->pixel_rate = 450 * MHz;
	dev_dbg(cam->dev,
		"[Res] PR = %lld, w/h=%d/%d HWN(%d)/BIN(%d)/FRZ(%d),Plan:%d\n",
		res->pixel_rate, in_w, in_h,
		res->hwn_limit_max, res->bin_limit, res->frz_limit, res->res_plan);
	memcpy(res->res_strategy, raw_resource_strategy_plan + res->res_plan,
	       MTK_CAMSYS_RES_STEP_NUM * sizeof(int));
	res->bin_enable = 0;
	res->raw_num_used = 1;
	res->frz_enable = 0;
	res->frz_ratio = frz_ratio;
	for (idx = 0; idx < MTK_CAMSYS_RES_STEP_NUM ; idx++) {
		res_step_type = res->res_strategy[idx] & MTK_CAMSYS_RES_IDXMASK;
		switch (res_step_type) {
		case MTK_CAMSYS_RES_BIN_TAG:
			bin_temp = res->res_strategy[idx] - E_RES_BIN_S;
			if (bin_temp <= res->bin_limit)
				bin_en = bin_temp;
			if (bin_en && frz_en)
				frz_en = 0;
			break;
		case MTK_CAMSYS_RES_FRZ_TAG:
			frz_temp = res->res_strategy[idx] - E_RES_FRZ_S;
			if (res->frz_limit < 100)
				frz_en = frz_temp;
			break;
		case MTK_CAMSYS_RES_HWN_TAG:
			hwn_temp = res->res_strategy[idx] - E_RES_HWN_S;
			if (hwn_temp + 1 <= res->hwn_limit_max)
				twin_en = hwn_temp;
			break;
		case MTK_CAMSYS_RES_CLK_TAG:
			clk_cur = res->res_strategy[idx] - E_RES_CLK_S;
			break;
		default:
			break;
		}

		/* 1 for force bin on */
		if (res->bin_limit >= 1)
			bin_en = 1;

		if (res->hwn_limit_min > 1)
			twin_en = 1;

		/* max line buffer check*/
		lb_chk_res = mtk_raw_linebuf_chk(twin_en, res->bin_limit & BIN_ON,
						 frz_en, res->bin_limit & QBND_ON,
						 is_cbn_en(res->bin_limit),
						 in_w, &frz_ratio);
		/* frz ratio*/
		if (res_step_type == MTK_CAMSYS_RES_FRZ_TAG) {
			res->frz_ratio =
				res->frz_limit < FRZ_PXLMODE_THRES
				? res->frz_limit : FRZ_PXLMODE_THRES;
		}
		/*try 1-pixel mode first*/
		for (p = 1; p <= MTK_CAMSYS_PROC_DEFAULT_PIXELMODE; p++) {
			tgo_pxl_mode = mtk_raw_pixelmode_calc(p, twin_en, bin_en, frz_en,
							      res->frz_ratio);
			/**
			 * isp throughput along resource strategy
			 * (compared with pixel rate)
			 */
			pixel_mode[idx] = tgo_pxl_mode;

				/* only support 1-pixel mode */
			if (p == 1 &&
			    lb_chk_res == LB_CHECK_OK) {
				if (!res_found) {
					res->bin_enable = bin_en;
					res->frz_enable = frz_en;
					res->raw_num_used = twin_en + 1;
					clk_res = clk_cur;
					idx_res = idx;
					res_found = true;
					break;
				}
			}
		}
		dev_dbg(cam->dev, "Res-%d B/F/H/C=%d/%d/%d/%d -> %d/%d/%d/%d (%d)(%d)\n",
			idx, bin_temp, frz_temp, hwn_temp, clk_cur, bin_en,
			frz_en, twin_en, clk_cur, lb_chk_res, pixel_mode[idx]);
	}

	tgo_pxl_mode = pixel_mode[idx_res];
	switch (tgo_pxl_mode) {
	case 1:
		res->tgo_pxl_mode = 0;
		break;
	case 2:
		res->tgo_pxl_mode = 1;
		break;
	case 4:
		res->tgo_pxl_mode = 2;
		break;
	case 8:
		res->tgo_pxl_mode = 3;
		break;
	default:
		break;
	}

	mtk_raw_update_debug_param(cam, res, &clk_res);

	if (res_found) {
		dev_info(cam->dev, "Res-end:%d BIN/FRZ/HWN/CLK/pxl=%d/%d(%d)/%d/%d/%d, clk:%d\n",
			 idx_res, res->bin_enable, res->frz_enable, res->frz_ratio,
			 res->raw_num_used, clk_res, res->tgo_pxl_mode, res->clk_target);
	} else {
		dev_dbg(cam->dev, "[%s] Error resource result\n",
			__func__);
	}
	if (res->bin_enable) {
		*out_w = in_w >> 1;
		*out_h = in_h >> 1;
	} else if (res->frz_enable) {
		*out_w = in_w * res->frz_ratio / 100;
		*out_h = in_h * res->frz_ratio / 100;
	} else {
		*out_w = in_w;
		*out_h = in_h;
	}

	return res_found;
}

static void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev,
				       int dequeued_frame_seq_no);
static void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev);
static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no);

static void raw_handle_error(struct mtk_raw_device *raw_dev,
			     struct mtk_camsys_irq_info *data)
{
	int err_status = data->e.err_status;
	int frame_idx_inner = data->frame_idx_inner;

	/* Show DMA errors in detail */
	if (err_status & DMA_ERR_ST)
		raw_irq_handle_dma_err(raw_dev);

	/* Show TG register for more error detail*/
	if (err_status & TG_GBERR_ST)
		raw_irq_handle_tg_grab_err(raw_dev, frame_idx_inner);

	if (err_status & TG_OVRUN_ST) {
		if (raw_dev->overrun_debug_dump_cnt < 4) {
			mtk_cam_dump_topdebug_rdyreq(raw_dev->dev,
						     raw_dev->base, raw_dev->yuv_base);
			raw_dev->overrun_debug_dump_cnt++;
		} else {
			dev_dbg(raw_dev->dev, "%s: TG_OVRUN_ST repeated skip dump raw_id:%d\n",
				__func__, raw_dev->id);
		}
		raw_irq_handle_tg_overrun_err(raw_dev, frame_idx_inner);
	}
}

static inline unsigned int mtk_raw_get_err_status(unsigned int irq_status)
{
	return irq_status & INT_ST_MASK_CAM_ERR;
}

static irqreturn_t mtk_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct device *dev = raw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frame_idx, frame_idx_inner, fbc_fho_ctl2;
	unsigned int irq_status, err_status, dmao_done_status, dmai_done_status;
	unsigned int drop_status, dma_ofl_status, cq_done_status, cq2_done_status;
	bool wake_thread = 0;

	irq_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT_STAT);
	dmao_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT2_STAT);
	dmai_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT3_STAT);
	drop_status	= readl_relaxed(raw_dev->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT5_STAT);
	cq_done_status  = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT6_STAT);
	cq2_done_status = readl_relaxed(raw_dev->base + REG_CTL_RAW_INT7_STAT);

	frame_idx	= readl_relaxed(raw_dev->base + REG_FRAME_SEQ_NUM);
	frame_idx_inner	= readl_relaxed(raw_dev->base_inner + REG_FRAME_SEQ_NUM);

	fbc_fho_ctl2 =
		readl_relaxed(REG_FBC_CTL2(raw_dev->base + FBC_R1A_BASE, 1));

	err_status = mtk_raw_get_err_status(irq_status);
	if (unlikely(debug_raw))
		dev_dbg(dev,
			"INT:0x%x(err:0x%x) 2~7 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x (in:%d)\n",
			irq_status, err_status,
			dmao_done_status, dmai_done_status, drop_status,
			dma_ofl_status, cq_done_status, cq2_done_status,
			frame_idx_inner);

	if (unlikely(!raw_dev->pipeline || !raw_dev->pipeline->enabled_raw)) {
		dev_dbg(dev, "%s: %i: raw pipeline is disabled\n",
			__func__, raw_dev->id);
		goto ctx_not_found;
	}

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = frame_idx;
	irq_info.frame_idx_inner = frame_idx_inner;

	/* CQ done */
	if (cq_done_status & CAMCTL_CQ_THR0_DONE_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_SETTING_DONE;
	/* DMAO done, only for AFO */
	if (dmao_done_status & AFO_DONE_ST)
		irq_info.irq_type |= 1 << CAMSYS_IRQ_AFO_DONE;
		/* enable AFO_DONE_EN at backend manually */

	/* Frame done */
	if (irq_status & SW_PASS1_DON_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_DONE;
		raw_dev->overrun_debug_dump_cnt = 0;
	}
	/* Frame start */
	if (irq_status & SOF_INT_ST) {
		irq_info.irq_type |= 1 << CAMSYS_IRQ_FRAME_START;

		raw_dev->sof_count++;
		irq_info.write_cnt = ((fbc_fho_ctl2 & WCNT_BIT_MASK) >> 8) - 1;
		irq_info.fbc_cnt = (fbc_fho_ctl2 & CNT_BIT_MASK) >> 16;
	}
	/* Vsync interrupt */
	if (irq_status & VS_INT_ST)
		raw_dev->vsync_count++;

	if (irq_info.irq_type && !raw_dev->is_sub) {
		if (push_msgfifo(raw_dev, &irq_info) == 0)
			wake_thread = 1;
	}

	/* Check ISP error status */
	if (unlikely(err_status)) {
		struct mtk_camsys_irq_info err_info;

		err_info.irq_type = 1 << CAMSYS_IRQ_ERROR;
		err_info.ts_ns = irq_info.ts_ns;
		err_info.frame_idx = irq_info.frame_idx;
		err_info.frame_idx_inner = irq_info.frame_idx_inner;
		err_info.e.err_status = err_status;

		if (push_msgfifo(raw_dev, &err_info) == 0)
			wake_thread = 1;
	}

	/* enable to debug fbc related */
	if (debug_raw && debug_dump_fbc && (irq_status & SOF_INT_ST))
		mtk_cam_raw_dump_fbc(raw_dev->dev, raw_dev->base, raw_dev->yuv_base);

ctx_not_found:

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_raw(int irq, void *data)
{
	struct mtk_raw_device *raw_dev = (struct mtk_raw_device *)data;
	struct mtk_camsys_irq_info irq_info;

	if (unlikely(atomic_cmpxchg(&raw_dev->is_fifo_overflow, 1, 0)))
		dev_info(raw_dev->dev, "msg fifo overflow\n");

	while (kfifo_len(&raw_dev->msg_fifo) >= sizeof(irq_info)) {
		int len = kfifo_out(&raw_dev->msg_fifo, &irq_info, sizeof(irq_info));

		WARN_ON(len != sizeof(irq_info));

		dev_dbg(raw_dev->dev, "ts=%llu irq_type %d, req:%d/%d\n",
			irq_info.ts_ns,
			irq_info.irq_type,
			irq_info.frame_idx_inner,
			irq_info.frame_idx);

		/* error case */
		if (unlikely(irq_info.irq_type == (1 << CAMSYS_IRQ_ERROR))) {
			raw_handle_error(raw_dev, &irq_info);
			continue;
		}

		/* normal case */

		/* inform interrupt information to camsys controller */
		mtk_camsys_isr_event(raw_dev->cam,
				     CAMSYS_ENGINE_RAW, raw_dev->id,
				     &irq_info);
	}

	return IRQ_HANDLED;
}

void raw_irq_handle_tg_grab_err(struct mtk_raw_device *raw_dev,
				int dequeued_frame_seq_no)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *s_data;
	int val, val2;
	unsigned int inner_val, tg_full_sel;

	val = readl_relaxed(raw_dev->base + REG_TG_PATH_CFG);
	inner_val = readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG);
	val = val | TG_TG_FULL_SEL;
	writel_relaxed(val, raw_dev->base + REG_TG_PATH_CFG);
	writel_relaxed(val, raw_dev->base_inner + REG_TG_PATH_CFG);
	wmb(); /* make sure committed */
	val2 = readl_relaxed(raw_dev->base + REG_TG_SEN_MODE);
	val2 = val2 | TG_CMOS_RDY_SEL;
	writel_relaxed(val2, raw_dev->base + REG_TG_SEN_MODE);
	wmb(); /* make sure committed */

	dev_info_ratelimited(raw_dev->dev,
			     "%d Grab_Err [Outter] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
			     dequeued_frame_seq_no,
			     readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
			     readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
			     readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
			     readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
			     readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
			     readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
	dev_info_ratelimited(raw_dev->dev,
			     "%d Grab_Err [Inner] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
			     dequeued_frame_seq_no,
			     readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG),
			     readl_relaxed(raw_dev->base_inner + REG_TG_SEN_MODE),
			     readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST),
			     readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST_R),
			     readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_PXL),
			     readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_LIN));

	ctx = mtk_cam_find_ctx(raw_dev->cam, &raw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(raw_dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	s_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);
	if (s_data) {
		mtk_cam_debug_seninf_dump(s_data);
		tg_full_sel = ((inner_val & TG_FULLSEL_BIT_MASK) >> 15);

		if (tg_full_sel == 1)
			mtk_cam_req_dump(s_data, MTK_CAM_REQ_DUMP_CHK_DEQUEUE_FAILED,
					 "TG Grab Err", false);
		else
			dev_info(raw_dev->dev, "%s: tg_full_sel 0x%x\n", __func__, tg_full_sel);
	} else {
		dev_info(raw_dev->dev,
			 "%s: req(%d) can't be found for seninf dump\n",
			 __func__, dequeued_frame_seq_no);
	}
}

void raw_irq_handle_dma_err(struct mtk_raw_device *raw_dev)
{
	mtk_cam_raw_dump_dma_err_st(raw_dev->dev, raw_dev->base);
	mtk_cam_yuv_dump_dma_err_st(raw_dev->dev, raw_dev->yuv_base);

	if (raw_dev->pipeline->feature_active)
		mtk_cam_dump_dma_debug(raw_dev->dev,
				       raw_dev->base + CAMDMATOP_BASE,
				       "RAWI_R2",
				       dbg_RAWI_R2, ARRAY_SIZE(dbg_RAWI_R2));

	if (raw_dev->pipeline->pde_config.pde_info.pd_table_offset) {
		dev_dbg_ratelimited(raw_dev->dev,
				    "DMA_ERR:%x,PDI_R1:%x,PDO_R1:%x\n",
				     readl_relaxed(raw_dev->base + 0x4060),
				     readl_relaxed(raw_dev->base + REG_PDI_R1_BASE +
						   DMA_OFFSET_ERR_STAT),
				     readl_relaxed(raw_dev->base + REG_PDO_R1_BASE +
						   DMA_OFFSET_ERR_STAT));
		dev_dbg_ratelimited(raw_dev->dev,
				    "TG_FRMSIZE_ST:%x,TG_FRMSIZE_ST_R:%x\n",
				    readl_relaxed(raw_dev->base + 0x0738),
				    readl_relaxed(raw_dev->base + 0x076c));
		mtk_cam_dump_dma_debug(raw_dev->dev, raw_dev->base + CAMDMATOP_BASE,
				       "PDO_R1", dbg_PDO_R1, ARRAY_SIZE(dbg_PDO_R1));
		mtk_cam_dump_dma_debug(raw_dev->dev, raw_dev->base + CAMDMATOP_BASE,
				       "PDI_R1", dbg_PDI_R1, ARRAY_SIZE(dbg_PDI_R1));
	}
}

static void raw_irq_handle_tg_overrun_err(struct mtk_raw_device *raw_dev,
					  int dequeued_frame_seq_no)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *s_data;
	int val, val2;
	unsigned int inner_val, tg_full_sel;

	val = readl_relaxed(raw_dev->base + REG_TG_PATH_CFG);
	inner_val = readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG);
	val = val | TG_TG_FULL_SEL;
	writel_relaxed(val, raw_dev->base + REG_TG_PATH_CFG);
	writel_relaxed(val, raw_dev->base_inner + REG_TG_PATH_CFG);
	wmb(); /* for dbg dump register */

	val2 = readl_relaxed(raw_dev->base + REG_TG_SEN_MODE);
	val2 = val2 | TG_CMOS_RDY_SEL;
	writel_relaxed(val2, raw_dev->base + REG_TG_SEN_MODE);
	wmb(); /* for dbg dump register */

	dev_info(raw_dev->dev,
		 "%d Overrun_Err [Outter] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		 dequeued_frame_seq_no,
		 readl_relaxed(raw_dev->base + REG_TG_PATH_CFG),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_MODE),
		 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST),
		 readl_relaxed(raw_dev->base + REG_TG_FRMSIZE_ST_R),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_PXL),
		 readl_relaxed(raw_dev->base + REG_TG_SEN_GRAB_LIN));
	dev_info(raw_dev->dev,
		 "%d Overrun_Err [Inner] TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:%x/%x %x/%x %x/%x\n",
		 dequeued_frame_seq_no,
		 readl_relaxed(raw_dev->base_inner + REG_TG_PATH_CFG),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_MODE),
		 readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST),
		 readl_relaxed(raw_dev->base_inner + REG_TG_FRMSIZE_ST_R),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_PXL),
		 readl_relaxed(raw_dev->base_inner + REG_TG_SEN_GRAB_LIN));
	dev_info(raw_dev->dev,
		 "REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_REQ_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_REQ_STAT));
	dev_info(raw_dev->dev,
		 "RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_RDY_STAT),
		 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_RDY_STAT));
	dev_info(raw_dev->dev,
		 "REQ YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_REQ_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_REQ_STAT));
	dev_info(raw_dev->dev,
		 "RDY YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_RDY_STAT),
		 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_RDY_STAT));

	ctx = mtk_cam_find_ctx(raw_dev->cam, &raw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(raw_dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	s_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, dequeued_frame_seq_no);
	if (s_data) {
		/**
		 * Enable the dump manually if needed.
		 * mtk_cam_req_dump() already call mtk_cam_seninf_dump()
		 * in a delayed work if no P1 done comes.
		 */
		if (0 && raw_dev->sof_count > 3)
			mtk_cam_debug_seninf_dump(s_data);

		tg_full_sel = ((inner_val & TG_FULLSEL_BIT_MASK) >> 15);
		if (tg_full_sel == 1)
			mtk_cam_req_dump(s_data, MTK_CAM_REQ_DUMP_CHK_DEQUEUE_FAILED,
					 "TG Overrun Err", true);
		else
			dev_info(raw_dev->dev, "%s: tg_full_sel 0x%x:\n", __func__, tg_full_sel);
	} else {
		dev_info(raw_dev->dev, "%s: req(%d) can't be found for dump\n",
			 __func__, dequeued_frame_seq_no);
		if (0 && raw_dev->sof_count > 3 && ctx->seninf)
			mtk_cam_seninf_dump(ctx->seninf);
	}
}

static int mtk_raw_of_probe(struct platform_device *pdev,
			    struct mtk_raw_device *raw)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	int n_clks;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &raw->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34))) {
		dev_err(dev, "%s: No suitable DMA available\n", __func__);
		return -EIO;
	}

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dma_set_max_seg_size(dev, UINT_MAX)) {
		dev_err(dev, "Failed to set DMA segment size\n");
		return -EIO;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(raw->base);
	}

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	raw->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(raw->base_inner)) {
		dev_err(dev, "failed to map register inner base\n");
		return PTR_ERR(raw->base_inner);
	}

	/* will be assigned later */
	raw->yuv_base = NULL;

	raw->irq = platform_get_irq(pdev, 0);
	if (raw->irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(dev, raw->irq,
					mtk_irq_raw,
					mtk_thread_irq_raw,
					0, dev_name(dev), raw);
	if (ret) {
		dev_err(dev, "failed to request irq=%d\n", raw->irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", raw->irq);

	disable_irq(raw->irq);

	n_clks = devm_clk_bulk_get_all(dev, &raw->clk_b);
	if (n_clks < 0) {
		dev_err(dev, "failed to devm_clk_bulk_get_all=%d\n", n_clks);
		return n_clks;
	}

	raw->num_clks = n_clks;
	dev_info(dev, "clk_num:%d\n", raw->num_clks);

	return 0;
}

static int mtk_raw_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DRAINED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DUMPED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);

	default:
		return -EINVAL;
	}
}

static int mtk_raw_available_resource(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	int res_status = 0;
	int i, j;

	for (i = 0; i < cam_dev->num_raw_devices; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		for (j = 0; j < ARRAY_SIZE(raw->devs); j++) {
			if (pipe->enabled_raw & 1 << j)
				res_status |= 1 << j;
		}
	}
	dev_dbg(raw->cam_dev, "%s raw_status:0x%x Available Engine:A/B/C:%d/%d/%d\n",
		__func__, res_status,
		!(res_status & (1 << MTKCAM_SUBDEV_RAW_0)),
		!(res_status & (1 << MTKCAM_SUBDEV_RAW_1)),
		!(res_status & (1 << MTKCAM_SUBDEV_RAW_2)));

	return res_status;
}

int mtk_cam_s_data_raw_select(struct mtk_cam_request_stream_data *s_data,
			      struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_pipeline *pipe;
	int raw_status = 0;
	bool selected = false;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	pipe = ctx->pipe;
	raw_status = mtk_raw_available_resource(pipe->raw);
	raw_status &= ~pipe->enabled_raw;

	mtk_raw_available_resource(pipe->raw);

	if (!selected)
		return -EINVAL;

	return 0;
}

int mtk_cam_raw_select(struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	int raw_status = 0;
	int mask = 0x0;
	bool selected = false;
	int m;

	pipe->enabled_raw = 0;
	raw_status = mtk_raw_available_resource(pipe->raw);
	if (pipe->res_config.raw_num_used == 3) {
		mask = 1 << MTKCAM_SUBDEV_RAW_0
			| 1 << MTKCAM_SUBDEV_RAW_1 | 1 << MTKCAM_SUBDEV_RAW_2;
		if (!(raw_status & mask)) {
			pipe->enabled_raw |= mask;
			selected = true;
		}
	} else if (pipe->res_config.raw_num_used == 2) {
		for (m = MTKCAM_SUBDEV_RAW_0; m >= MTKCAM_SUBDEV_RAW_0; m--) {
			mask = (1 << m) | (1 << (m + 1));
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	} else {
		for (m = MTKCAM_SUBDEV_RAW_0; m < ARRAY_SIZE(pipe->raw->devs); m++) {
			mask = 1 << m;
			if (!(raw_status & mask)) {
				pipe->enabled_raw |= mask;
				selected = true;
				break;
			}
		}
	}
	mtk_raw_available_resource(pipe->raw);
	if (!selected)
		return -EINVAL;

	return 0;
}

static int mtk_raw_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_raw_pipeline *pipe;
	struct mtk_raw *raw;
	struct mtk_cam_device *cam;
	struct mtk_cam_ctx *ctx;
	unsigned int i;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	raw = pipe->raw;
	if (!raw)
		return -1;
	cam = dev_get_drvdata(raw->cam_dev);
	ctx = mtk_cam_find_ctx(cam, &sd->entity);

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		pipe->feature_active = pipe->user_res.raw_res.feature;
		pipe->enabled_dmas = 0;
		ctx->pipe = pipe;
		ctx->used_raw_num++;
		pipe->feature_active = pipe->user_res.raw_res.feature;
		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |=
				(1ULL << pipe->vdev_nodes[i].desc.dma_port);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d)\n", __func__, i);
				pm_runtime_put_sync(raw->devs[i]);
			}
		}
	}

	dev_info(raw->cam_dev, "%s:raw-%d: en %d, dev 0x%x dmas 0x%lx\n",
		 __func__, pipe->id, enable, pipe->enabled_raw,
		 pipe->enabled_dmas);

	return 0;
}

static int mtk_raw_init_cfg(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, sd_state, i);
		*mf = mfmt_default;

		dev_dbg(raw->cam_dev, "%s init pad:%d format:0x%x\n",
			sd->name, i, mf->code);
	}

	return 0;
}

static bool mtk_raw_try_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	unsigned int user_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);

	dev_dbg(raw->cam_dev, "%s:s_fmt: check format 0x%x, w:%d, h:%d field:%d\n",
		sd->name, fmt->format.code, fmt->format.width,
		fmt->format.height, fmt->format.field);

	/* check sensor format */
	if (fmt->pad == MTK_RAW_SINK) {
		user_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);
		if (user_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN)
			return false;
	}

	return true;
}

static struct v4l2_mbus_framefmt*
mtk_raw_pipeline_get_fmt(struct mtk_raw_pipeline *pipe,
			 struct v4l2_subdev_state *sd_state, u32 padid,
			 int which)
{
	struct mtk_raw *raw = pipe->raw;

	if (padid >= MTK_RAW_PIPELINE_PADS_NUM) {
		dev_err(raw->cam_dev, "Wrong pad id:%d\n", padid);
		return NULL;
	}
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&pipe->subdev, sd_state, padid);

	if (WARN_ON(padid >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].mbus_fmt;

	return &pipe->cfg[padid].mbus_fmt;
}

static struct v4l2_rect*
mtk_raw_pipeline_get_selection(struct mtk_raw_pipeline *pipe,
			       struct v4l2_subdev_state *sd_state, u32 pad,
			       int which)
{
	struct mtk_raw *raw = pipe->raw;

	if (pad >= MTK_RAW_PIPELINE_PADS_NUM) {
		dev_err(raw->cam_dev, "Wrong pad id:%d\n", pad);
		return NULL;
	}
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(&pipe->subdev, sd_state, pad);

	if (WARN_ON(pad >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].crop;

	return &pipe->cfg[pad].crop;
}

static void propagate_fmt(struct v4l2_mbus_framefmt *sink_mf,
			  struct v4l2_mbus_framefmt *source_mf, int w, int h)
{
	source_mf->code = sink_mf->code;
	source_mf->colorspace = sink_mf->colorspace;
	source_mf->field = sink_mf->field;
	source_mf->width = w;
	source_mf->height = h;
}

static bool mtk_raw_fmt_get_res(struct v4l2_subdev *sd,
				struct v4l2_subdev_format *fmt,
				struct mtk_cam_resource *res)
{
	void *user_ptr;
	u64 addr;

	addr = ((u64)fmt->reserved[1] << 32) | fmt->reserved[2];
	user_ptr = (void *)addr;
	if (!user_ptr) {
		dev_info(sd->v4l2_dev->dev, "%s: mtk_cam_resource is null\n",
			 __func__);
		return false;
	}

	if (copy_from_user(res, (void __user *)user_ptr, sizeof(*res))) {
		dev_info(sd->v4l2_dev->dev, "%s: copy_from_user failedm user_ptr:%p\n",
			 __func__, user_ptr);
		return false;
	}

	dev_dbg(sd->v4l2_dev->dev, "%s:sensor:%d/%d/%lld/%d/%d, raw:%lld/%d/%d/%d/%d/%d/%d/%d/%lld\n",
		__func__,
		res->sensor_res.hblank, res->sensor_res.vblank,
		res->sensor_res.pixel_rate,	res->sensor_res.interval.denominator,
		res->sensor_res.interval.numerator,
		res->raw_res.feature, res->raw_res.bin, res->raw_res.path_sel,
		res->raw_res.raw_max, res->raw_res.raw_min, res->raw_res.raw_used,
		res->raw_res.strategy, res->raw_res.pixel_mode,
		res->raw_res.throughput);

	return res;
}

static int mtk_raw_set_src_pad_selection_default(struct v4l2_subdev *sd,
						 struct v4l2_subdev_state *sd_state,
						 struct v4l2_mbus_framefmt *sink_fmt,
						 struct mtk_cam_resource *res,
						 int pad, int which)
{
	struct v4l2_rect *source_sel;
	struct mtk_raw_pipeline *pipe;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	source_sel = mtk_raw_pipeline_get_selection(pipe, sd_state, pad, which);
	if (source_sel->width > sink_fmt->width)
		source_sel->width = sink_fmt->width;

	if (source_sel->height > sink_fmt->height)
		source_sel->height = sink_fmt->height;

	return 0;
}

static int mtk_raw_set_src_pad_selection_yuv(struct v4l2_subdev *sd,
					     struct v4l2_subdev_state *sd_state,
					     struct v4l2_mbus_framefmt *sink_fmt,
					     struct mtk_cam_resource *res,
					     int pad, int which)
{
	int i;
	struct v4l2_rect *prev_yuv = NULL, *source_sel, *tmp_sel;
	struct v4l2_mbus_framefmt *framefmt;
	struct mtk_raw_pipeline *pipe;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	mtk_raw_set_src_pad_selection_default(sd, sd_state, sink_fmt, res, pad, which);
	source_sel = mtk_raw_pipeline_get_selection(pipe, sd_state, pad, which);

	for (i = MTK_RAW_YUVO_1_OUT; i < pad; i++) {
		framefmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, pad, which);
		tmp_sel = mtk_raw_pipeline_get_selection(pipe, sd_state, pad, which);

		/* Skip disabled YUV pad */
		if (!mtk_cam_is_pad_fmt_enable(framefmt))
			continue;

		prev_yuv = tmp_sel;
	}

	if (prev_yuv) {
		if (source_sel->width != prev_yuv->width) {
			source_sel->width = prev_yuv->width;
			/* may need some log */
		}

		if (source_sel->height != prev_yuv->height) {
			source_sel->height = prev_yuv->height;
			/* may need some log */
		}
	}

	return 0;
}

static struct v4l2_mbus_framefmt*
mtk_raw_get_sink_pad_framefmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_state *sd_state, int which)
{
	struct v4l2_mbus_framefmt *sink_fmt = NULL, *tmp_fmt;
	struct mtk_raw_pipeline *pipe;
	int i;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	for (i = MTK_RAW_SINK; i < MTK_RAW_SOURCE_BEGIN; i++) {
		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, i, which);
		if (i != MTK_RAW_META_IN &&
		    mtk_cam_is_pad_fmt_enable(tmp_fmt)) {
			sink_fmt = tmp_fmt;
			break;
		}
	}

	return sink_fmt;
}

static int mtk_raw_set_pad_selection(struct v4l2_subdev *sd,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_selection *sel)
{
	struct v4l2_mbus_framefmt *sink_fmt = NULL;
	struct mtk_raw_pipeline *pipe;
	struct mtk_cam_video_device *node;
	struct mtk_cam_resource *res = NULL;
	struct v4l2_rect *crop;
	int ret;

	if (sel->pad < MTK_RAW_MAIN_STREAM_OUT || sel->pad >= MTK_RAW_META_OUT_BEGIN)
		return -EINVAL;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);

	/* The crop rectangle can't be changed while streaming. */
	if (sel->which == V4L2_SUBDEV_FORMAT_ACTIVE &&
	    !media_entity_is_streaming(&sd->entity)) {
		return -EBUSY;
	}

	/*
	 * Find the sink pad fmt, there must be one eanbled sink pad at least
	 */
	sink_fmt = mtk_raw_get_sink_pad_framefmt(sd, sd_state, sel->which);
	if (!sink_fmt)
		return -EINVAL;

	node = &pipe->vdev_nodes[sel->pad - MTK_RAW_SINK_NUM];
	crop = mtk_raw_pipeline_get_selection(pipe, sd_state, sel->pad, sel->which);
	*crop = sel->r;
	ret = node->desc.pad_ops->set_pad_selection(sd, sd_state, sink_fmt,
						    res, sel->pad, sel->which);
	if (ret)
		return -EINVAL;

	sel->r = *crop;

	return 0;
}

static int mtk_raw_get_pad_selection(struct v4l2_subdev *sd,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_selection *sel)
{
	return 0;
}

static int mtk_raw_set_sink_pad_fmt(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_format *fmt)
{
	struct device *dev;
	struct mtk_cam_video_device *node;
	const char *node_str;
	const struct mtk_cam_format_desc *fmt_desc;
	struct mtk_raw_pipeline *pipe;
	int i;
	int ipi_fmt;
	struct v4l2_mbus_framefmt *framefmt, *source_fmt = NULL, *tmp_fmt;

	/* Do nothing for pad to meta video device */
	if (fmt->pad == MTK_RAW_META_IN)
		return 0;

	dev = sd->v4l2_dev->dev;
	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	framefmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, fmt->pad, fmt->which);

	/* If data from sensor, we check the size with max imgo size*/
	if (fmt->pad < MTK_RAW_SINK_NUM) {
		/* from sensor */
		node = &pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
		node_str = "sink";
	} else {
		/* from memory */
		node = &pipe->vdev_nodes[fmt->pad - MTK_RAW_SINK_NUM];
		node_str = node->desc.name;
	}

	ipi_fmt = mtk_cam_get_sensor_fmt(framefmt->code);
	if (ipi_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		/**
		 * Set imgo's default fmt, the user must check
		 * if the pad sink format is the same as the
		 * source format of the link before stream on.
		 */
		fmt_desc = &node->desc.fmts[node->desc.default_fmt_idx];
		framefmt->code = fmt_desc->pfmt.code;
		dev_info(dev,
			 "%s(%d): Adjust unaccept fmt code on sink pad:%d, 0x%x->0x%x\n",
			 __func__, fmt->which, fmt->pad, fmt->format.code, framefmt->code);
	}

	/* Reset pads' enable state*/
	for (i = MTK_RAW_SINK; i < MTK_RAW_META_OUT_BEGIN; i++) {
		if (i == MTK_RAW_META_IN)
			continue;
		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, i, fmt->which);
		mtk_cam_pad_fmt_disable(tmp_fmt);
	}

	*framefmt = fmt->format;
	if (framefmt->width > node->desc.frmsizes->stepwise.max_width)
		framefmt->width = node->desc.frmsizes->stepwise.max_width;

	if (framefmt->height > node->desc.frmsizes->stepwise.max_height)
		framefmt->height = node->desc.frmsizes->stepwise.max_height;

	mtk_cam_pad_fmt_enable(framefmt);

	dev_info(dev,
		 "%s(%d): Set fmt pad:%d(%s), code/w/h = 0x%x/%d/%d\n",
		 __func__, fmt->which, fmt->pad, node_str,
		 framefmt->code, framefmt->width, framefmt->height);

	/* Propagation inside subdev */
	for (i = MTK_RAW_SOURCE_BEGIN; i < MTK_RAW_META_OUT_BEGIN; i++) {
		source_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, i, fmt->which);

		/* Get default format's desc for the pad */
		node = &pipe->vdev_nodes[i - MTK_RAW_SINK_NUM];

		/**
		 * Propagate the size from sink pad to source pades, adjusted
		 * based on each pad's default format.
		 */
		if (source_fmt->width > node->desc.frmsizes->stepwise.max_width)
			source_fmt->width = node->desc.frmsizes->stepwise.max_width;
		else
			source_fmt->width = framefmt->width;

		if (source_fmt->height > node->desc.frmsizes->stepwise.max_height)
			source_fmt->height = node->desc.frmsizes->stepwise.max_height;
		else
			source_fmt->height = framefmt->height;

		dev_dbg(dev,
			"%s(%d): Propagate to pad:%d(%s), (0x%x/%d/%d)\n",
			__func__, fmt->which, fmt->pad, node->desc.name,
			source_fmt->code, source_fmt->width, source_fmt->height);
	}

	return 0;
}

static int mtk_raw_set_src_pad_fmt_default(struct v4l2_subdev *sd,
					   struct v4l2_subdev_state *sd_state,
					   struct v4l2_mbus_framefmt *sink_fmt,
					   struct mtk_cam_resource *res,
					   int pad, int which)
{
	struct device *dev;
	struct v4l2_mbus_framefmt *source_fmt;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_cam_video_device *node;

	dev = sd->v4l2_dev->dev;
	node = &pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM];
	source_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, pad, which);
	if (source_fmt->width > sink_fmt->width) {
		dev_info(dev,
			 "%s(%d): adjusted: width(%d) over sink (%d)\n",
			 __func__, which, source_fmt->width, sink_fmt->width);
		source_fmt->width = sink_fmt->width;
	}

	if (source_fmt->height > sink_fmt->height) {
		dev_info(dev,
			 "%s(%d): adjusted: width(%d) over sink (%d)\n",
			 __func__, which, source_fmt->height, sink_fmt->height);
		source_fmt->height = sink_fmt->height;
	}

	if (source_fmt->width > node->desc.frmsizes->stepwise.max_width) {
		dev_info(dev,
			 "%s(%d): adjusted: width(%d) over max (%d)\n",
			 __func__, which, source_fmt->width,
			 node->desc.frmsizes->stepwise.max_width);
		source_fmt->width = node->desc.frmsizes->stepwise.max_width;
	}

	if (source_fmt->height > node->desc.frmsizes->stepwise.max_height) {
		dev_info(dev,
			 "%s(%d): adjusted: height(%d) over max (%d)\n",
			 __func__, which, source_fmt->height,
			 node->desc.frmsizes->stepwise.max_height);
	}

	return 0;
}

static int mtk_raw_set_src_pad_fmt_rzh1n2(struct v4l2_subdev *sd,
					  struct v4l2_subdev_state *sd_state,
					  struct v4l2_mbus_framefmt *sink_fmt,
					  struct mtk_cam_resource *res, int pad,
					  int which)
{
	struct device *dev;
	struct v4l2_mbus_framefmt *source_fmt;
	struct v4l2_mbus_framefmt *tmp_fmt;
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);

	dev = sd->v4l2_dev->dev;
	mtk_raw_set_src_pad_fmt_default(sd, sd_state, sink_fmt, res, pad, which);
	source_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, pad, which);

	/* rzh1n2to_r1 and rzh1n2to_r3 size must be the same */
	if (pad == MTK_RAW_RZH1N2TO_3_OUT) {
		tmp_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, MTK_RAW_RZH1N2TO_1_OUT, which);
		if (mtk_cam_is_pad_fmt_enable(tmp_fmt) &&
		    source_fmt->height != tmp_fmt->height &&
		    source_fmt->width != tmp_fmt->width) {
			dev_info(dev,
				 "%s(%d): adjusted: rzh1n2to_r3(%d,%d) and rzh1n2to_r1(%d,%d) must have the same sz\n",
				 __func__, which, source_fmt->width,
				 source_fmt->height, tmp_fmt->width, tmp_fmt->height);
			source_fmt->width = tmp_fmt->width;
			source_fmt->height = tmp_fmt->height;
		}
	}

	return 0;
}

static int mtk_raw_set_src_pad_fmt(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_format *fmt)
{
	struct device *dev;
	struct mtk_cam_resource res;
	struct mtk_cam_video_device *node;
	struct mtk_raw_pipeline *pipe;
	int ret = 0;
	struct v4l2_mbus_framefmt *source_fmt, *sink_fmt = NULL;

	/* Do nothing for pad to meta video device */
	if (fmt->pad >= MTK_RAW_META_OUT_BEGIN)
		return 0;

	pipe = container_of(sd, struct mtk_raw_pipeline, subdev);
	dev = sd->v4l2_dev->dev;
	node = &pipe->vdev_nodes[fmt->pad - MTK_RAW_SINK_NUM];

	/*
	 * Find the sink pad fmt, there must be one eanbled sink pad at least
	 */
	sink_fmt = mtk_raw_get_sink_pad_framefmt(sd, sd_state, fmt->which);
	if (!sink_fmt) {
		dev_info(dev,
			 "%s(%d): Set fmt pad:%d(%s), no s_fmt on sink pad\n",
			 __func__, fmt->which, fmt->pad, node->desc.name);
		return -EINVAL;
	}

	if (!mtk_raw_fmt_get_res(sd, fmt, &res)) {
		dev_info(dev,
			 "%s(%d): Set fmt pad:%d(%s), no mtk_cam_resource found\n",
			 __func__, fmt->which, fmt->pad, node->desc.name);
		return -EINVAL;
	}

	if (node->desc.pad_ops->set_pad_fmt) {
		/* call source pad's set_pad_fmt op to adjust fmt by pad */
		source_fmt = mtk_raw_pipeline_get_fmt(pipe, sd_state, fmt->pad, fmt->which);

		*source_fmt = fmt->format;
		ret = node->desc.pad_ops->set_pad_fmt(sd, sd_state, sink_fmt, &res, fmt->pad,
			fmt->which);
	}

	if (ret)
		return ret;

	if (!source_fmt) {
		dev_info(dev,
			 "%s(%d): Set fmt pad:%d(%s), no s_fmt on source pad\n",
			 __func__, fmt->which, fmt->pad, node->desc.name);
		return -EINVAL;
	}

	dev_dbg(dev,
		"%s(%d): s_fmt to pad:%d(%s), user(0x%x/%d/%d) driver(0x%x/%d/%d)\n",
		__func__, fmt->which, fmt->pad, node->desc.name,
		fmt->format.code, fmt->format.width, fmt->format.height,
		source_fmt->code, source_fmt->width, source_fmt->height);
	mtk_cam_pad_fmt_disable(source_fmt);
	fmt->format = *source_fmt;

	return 0;
}

static int mtk_raw_try_pad_fmt(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_format *fmt)
{
	if (fmt->pad >= MTK_RAW_SINK && fmt->pad < MTK_RAW_SOURCE_BEGIN)
		mtk_raw_set_sink_pad_fmt(sd, sd_state, fmt);
	else if (fmt->pad < MTK_RAW_PIPELINE_PADS_NUM)
		mtk_raw_set_src_pad_fmt(sd, sd_state, fmt);
	else
		return -EINVAL;

	return 0;
}

static int mtk_raw_call_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_format *fmt,
				bool streaming)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;

	if (!sd || !fmt) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), fmt(%p)\n",
			__func__, sd, fmt);
		return -EINVAL;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !sd_state) {
		dev_dbg(raw->cam_dev, "%s: Required sd(%p), cfg(%p) for FORMAT_TRY\n",
			__func__, sd, sd_state);
		return -EINVAL;
	}

	if (!mtk_raw_try_fmt(sd, fmt) &&
	    !mtk_cam_feature_is_pure_m2m(pipe->feature_pending)) {
		mf = mtk_raw_pipeline_get_fmt(pipe, sd_state, fmt->pad, fmt->which);
		fmt->format = *mf;
		dev_info(raw->cam_dev,
			 "sd:%s pad:%d didn't apply and keep format w/h/code %d/%d/0x%x\n",
			 sd->name, fmt->pad, mf->width, mf->height, mf->code);
	} else {
		mf = mtk_raw_pipeline_get_fmt(pipe, sd_state, fmt->pad, fmt->which);
		*mf = fmt->format;
		dev_dbg(raw->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);
	}

	/*sink pad format propagate to source pad*/
	if (fmt->pad == MTK_RAW_SINK) {
		struct v4l2_mbus_framefmt *source_mf;

		source_mf = mtk_raw_pipeline_get_fmt(pipe, sd_state,
						     MTK_RAW_MAIN_STREAM_OUT,
						     fmt->which);

		if (streaming) {
			propagate_fmt(mf, source_mf, mf->width, mf->height);
			return 0;
		}

		/**
		 * User will trigger resource calc with V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC
		 * so we don't need to trigger it here anymore.
		 */
		propagate_fmt(mf, source_mf, mf->width, mf->height);
	}

	return 0;
}

static int mtk_raw_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_raw_try_pad_fmt(sd, sd_state, fmt);

	/* The format can't be changed while streaming. */
	if (!media_entity_is_streaming(&sd->entity))
		return mtk_raw_call_set_fmt(sd, sd_state, fmt, false);

	dev_info(sd->v4l2_dev->dev,
		 "Pipeline is streaming, cannot change format\n");
	return -EBUSY;
}

static int mtk_raw_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf;

	mf = mtk_raw_pipeline_get_fmt(pipe, sd_state, fmt->pad, fmt->which);
	fmt->format = *mf;
	dev_dbg(raw->cam_dev, "sd:%s pad:%d get format w/h/code %d/%d/0x%x\n",
		sd->name, fmt->pad, fmt->format.width, fmt->format.height,
		fmt->format.code);

	return 0;
}

static int mtk_cam_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_raw_pipeline *pipe =
		container_of(entity, struct mtk_raw_pipeline, subdev.entity);
	struct mtk_raw *raw = pipe->raw;
	u32 pad = local->index;

	dev_dbg(raw->cam_dev, "%s: raw %d: remote:%s:%d->local:%s:%d flags:0x%x\n",
		__func__, pipe->id, remote->entity->name, remote->index,
		local->entity->name, local->index, flags);

	if (pad < MTK_RAW_PIPELINE_PADS_NUM && pad != MTK_RAW_SINK)
		pipe->vdev_nodes[pad - MTK_RAW_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!media_entity_is_streaming(entity) && !(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));

	if (pad == MTK_RAW_SINK && flags & MEDIA_LNK_FL_ENABLED)
		pipe->res_config.seninf =
			media_entity_to_v4l2_subdev(remote->entity);

	return 0;
}

static int
mtk_raw_s_frame_interval(struct v4l2_subdev *sd,
			 struct v4l2_subdev_frame_interval *interval)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	dev_dbg(raw->cam_dev, "%s:pipe(%d):current res: fps = %d/%d",
		__func__, pipe->id,
		interval->interval.numerator,
		interval->interval.denominator);
	pipe->res_config.interval = interval->interval;

	return 0;
}

static int
mtk_raw_g_frame_interval(struct v4l2_subdev *sd,
			 struct v4l2_subdev_frame_interval *interval)
{
	struct mtk_raw_pipeline *pipe =
		container_of(sd, struct mtk_raw_pipeline, subdev);
	struct mtk_raw *raw = pipe->raw;

	dev_dbg(raw->cam_dev, "%s:pipe(%d):current res: fps = %d/%d",
		__func__, pipe->id,
		interval->interval.numerator,
		interval->interval.denominator);
	interval->interval = pipe->res_config.interval;

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_raw_subdev_core_ops = {
	.subscribe_event = mtk_raw_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_raw_subdev_video_ops = {
	.s_stream =  mtk_raw_sd_s_stream,
	.s_frame_interval = mtk_raw_s_frame_interval,
	.g_frame_interval = mtk_raw_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops mtk_raw_subdev_pad_ops = {
	.link_validate = mtk_cam_link_validate,
	.init_cfg = mtk_raw_init_cfg,
	.set_fmt = mtk_raw_set_fmt,
	.get_fmt = mtk_raw_get_fmt,
	.set_selection = mtk_raw_set_pad_selection,
	.get_selection = mtk_raw_get_pad_selection,
};

static const struct v4l2_subdev_ops mtk_raw_subdev_ops = {
	.core = &mtk_raw_subdev_core_ops,
	.video = &mtk_raw_subdev_video_ops,
	.pad = &mtk_raw_subdev_pad_ops,
};

static const struct media_entity_operations mtk_cam_media_entity_ops = {
	.link_setup = mtk_cam_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vout_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_out = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_s_selection = mtk_cam_vidioc_s_selection,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_cap = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_cam_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_out = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct mtk_cam_format_desc  stream_out_fmts[] = {
	/* This is a default image format */
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SBGGR14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGBRG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SGRBG14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB10_1X10,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10P,
		},
	},

	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB12_1X12,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_MTISP_SRGGB14_1X14,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SBGGR16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGBRG16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SGRBG16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB16,
			.num_planes = 1,
		},
		.pfmt = {
			.code = MEDIA_BUS_FMT_SRGGB16_1X16,
		}
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER8_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_BAYER14_UFBC,
		},
	},
};

static const struct mtk_cam_format_desc yuv_out_group1_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUV420,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12_UFBC,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB8F,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB10F,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP1_MAX_WIDTH,
			.height = YUV_GROUP1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRB12F,
		},
	}
};

static const struct mtk_cam_format_desc yuv_out_group2_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_12P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = YUV_GROUP2_MAX_WIDTH,
			.height = YUV_GROUP2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_12P,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to1_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO1_MAX_WIDTH,
			.height = RZH1N2TO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to2_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YUYV,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO2_MAX_WIDTH,
			.height = RZH1N2TO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_YVYU,
		},
	}
};

static const struct mtk_cam_format_desc rzh1n2to3_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = RZH1N2TO3_MAX_WIDTH,
			.height = RZH1N2TO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	}
};

static const struct mtk_cam_format_desc drzs4no1_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV16_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV61_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV16_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV61_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO1_MAX_WIDTH,
			.height = DRZS4NO1_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
		},
	},
};

static const struct mtk_cam_format_desc drzs4no2_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO2_MAX_WIDTH,
			.height = DRZS4NO2_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

static const struct mtk_cam_format_desc drzs4no3_out_fmts[] = {
	{
		.vfmt.fmt.pix_mp = {
			.width = DRZS4NO3_MAX_WIDTH,
			.height = DRZS4NO3_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_GREY,
		},
	}
};

#define MTK_RAW_TOTAL_OUTPUT_QUEUES 4

static const struct
mtk_cam_dev_node_desc output_queues[] = {
	{
		.id = MTK_RAW_META_IN,
		.name = "meta input",
		.cap = V4L2_CAP_META_OUTPUT,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = true,
		/* .need_cache_sync_on_prepare = true, */
		.dma_port = MTKCAM_IPI_RAW_META_STATS_CFG,
		.default_fmt_idx = 0,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_out_ioctl_ops,
	},
	{
		.id = MTK_RAW_RAWI_2_IN,
		.name = "rawi 2",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_2,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RAWI_3_IN,
		.name = "rawi 3",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RAWI_4_IN,
		.name = "rawi 4",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.link_flags = 0,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RAWI_3, /* align backend RAWI_3 */
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_cam_v4l2_vout_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	}
};

static const char *output_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_OUTPUT_QUEUES] = {
	{"mtk-cam raw-0 meta-input", "mtk-cam raw-0 rawi-2",
	 "mtk-cam raw-0 rawi-3", "mtk-cam raw-0 rawi-4"},

	{"mtk-cam raw-1 meta-input", "mtk-cam raw-1 rawi-2",
	 "mtk-cam raw-1 rawi-3", "mtk-cam raw-1 rawi-4"},

	{"mtk-cam raw-2 meta-input", "mtk-cam raw-2 rawi-2",
	 "mtk-cam raw-2 rawi-3", "mtk-cam raw-2 rawi-4"},
};

static struct mtk_cam_pad_ops source_pad_ops_default = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};

static struct mtk_cam_pad_ops source_pad_ops_yuv = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_yuv,
};

static struct mtk_cam_pad_ops source_pad_ops_drzs4no = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_default,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};

static struct mtk_cam_pad_ops source_pad_ops_rzh1n2 = {
	.set_pad_fmt = mtk_raw_set_src_pad_fmt_rzh1n2,
	.set_pad_selection = mtk_raw_set_src_pad_selection_default,
};

#define MTK_RAW_TOTAL_CAPTURE_QUEUES 15
static const struct
mtk_cam_dev_node_desc capture_queues[] = {
	{
		.id = MTK_RAW_MAIN_STREAM_OUT,
		.name = "imgo",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_IMGO,
		.fmts = stream_out_fmts,
		.num_fmts = ARRAY_SIZE(stream_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_default,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_1_OUT,
		.name = "yuvo 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_1,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_2_OUT,
		.name = "yuvo 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_2,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_3_OUT,
		.name = "yuvo 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_3,
		.fmts = yuv_out_group1_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group1_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_4_OUT,
		.name = "yuvo 4",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_4,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_YUVO_5_OUT,
		.name = "yuvo 5",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_YUVO_5,
		.fmts = yuv_out_group2_fmts,
		.num_fmts = ARRAY_SIZE(yuv_out_group2_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_yuv,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = YUV_GROUP2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = YUV_GROUP2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_1_OUT,
		.name = "drzs4no 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_1,
		.fmts = drzs4no1_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no1_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_2_OUT,
		.name = "drzs4no 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_2,
		.fmts = drzs4no2_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no2_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_DRZS4NO_3_OUT,
		.name = "drzs4no 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_DRZS4NO_3,
		.fmts = drzs4no3_out_fmts,
		.num_fmts = ARRAY_SIZE(drzs4no3_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_drzs4no,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = DRZS4NO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = DRZS4NO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_1_OUT,
		.name = "rzh1n2to 1",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_1,
		.fmts = rzh1n2to1_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to1_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO1_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO1_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_2_OUT,
		.name = "rzh1n2to 2",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_2,
		.fmts = rzh1n2to2_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to2_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO2_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO2_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_RZH1N2TO_3_OUT,
		.name = "rzh1n2to 3",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_RAW_RZH1N2TO_3,
		.fmts = rzh1n2to3_out_fmts,
		.num_fmts = ARRAY_SIZE(rzh1n2to3_out_fmts),
		.default_fmt_idx = 0,
		.pad_ops = &source_pad_ops_rzh1n2,
		.ioctl_ops = &mtk_cam_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = RZH1N2TO3_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = RZH1N2TO3_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
	{
		.id = MTK_RAW_META_OUT_0,
		.name = "partial meta 0",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = true,
		.dma_port = MTKCAM_IPI_RAW_META_STATS_0,
		.default_fmt_idx = 1,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_1,
		.name = "partial meta 1",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = true,
		/* .need_cache_sync_on_finish = true, */
		.dma_port = MTKCAM_IPI_RAW_META_STATS_1,
		.default_fmt_idx = 2,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
	{
		.id = MTK_RAW_META_OUT_2,
		.name = "partial meta 2",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		/* .need_cache_sync_on_finish = true, */
		.dma_port = MTKCAM_IPI_RAW_META_STATS_2,
		.default_fmt_idx = 3,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_cam_v4l2_meta_cap_ioctl_ops,
	},
};

static const char *capture_queue_names[RAW_PIPELINE_NUM][MTK_RAW_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam raw-0 main-stream",
	 "mtk-cam raw-0 yuvo-1", "mtk-cam raw-0 yuvo-2",
	 "mtk-cam raw-0 yuvo-3", "mtk-cam raw-0 yuvo-4",
	 "mtk-cam raw-0 yuvo-5",
	 "mtk-cam raw-0 drzs4no-1", "mtk-cam raw-0 drzs4no-2", "mtk-cam raw-0 drzs4no-3",
	 "mtk-cam raw-0 rzh1n2to-1", "mtk-cam raw-0 rzh1n2to-2", "mtk-cam raw-0 rzh1n2to-3",
	 "mtk-cam raw-0 partial-meta-0", "mtk-cam raw-0 partial-meta-1",
	 "mtk-cam raw-0 partial-meta-2",
	 },
	{"mtk-cam raw-1 main-stream",
	 "mtk-cam raw-1 yuvo-1", "mtk-cam raw-1 yuvo-2",
	 "mtk-cam raw-1 yuvo-3", "mtk-cam raw-1 yuvo-4",
	 "mtk-cam raw-1 yuvo-5",
	 "mtk-cam raw-1 drzs4no-1", "mtk-cam raw-1 drzs4no-2", "mtk-cam raw-1 drzs4no-3",
	 "mtk-cam raw-1 rzh1n2to-1", "mtk-cam raw-1 rzh1n2to-2", "mtk-cam raw-1 rzh1n2to-3",
	 "mtk-cam raw-1 partial-meta-0", "mtk-cam raw-1 partial-meta-1",
	 "mtk-cam raw-1 partial-meta-2",
	 },

	{"mtk-cam raw-2 main-stream",
	 "mtk-cam raw-2 yuvo-1", "mtk-cam raw-2 yuvo-2",
	 "mtk-cam raw-2 yuvo-3", "mtk-cam raw-2 yuvo-4",
	 "mtk-cam raw-2 yuvo-5",
	 "mtk-cam raw-2 drzs4no-1", "mtk-cam raw-2 drzs4no-2", "mtk-cam raw-2 drzs4no-3",
	 "mtk-cam raw-2 rzh1n2to-1", "mtk-cam raw-2 rzh1n2to-2", "mtk-cam raw-2 rzh1n2to-3",
	 "mtk-cam raw-2 partial-meta-0", "mtk-cam raw-2 partial-meta-1",
	 "mtk-cam raw-2 partial-meta-2",
	 },
};

/* The helper to configure the device context */
static void mtk_raw_pipeline_queue_setup(struct mtk_raw_pipeline *pipe)
{
	unsigned int node_idx, i;

	if (WARN_ON(MTK_RAW_TOTAL_OUTPUT_QUEUES + MTK_RAW_TOTAL_CAPTURE_QUEUES
	    != MTK_RAW_TOTAL_NODES))
		return;

	node_idx = 0;
	/* Setup the output queue */
	for (i = 0; i < MTK_RAW_TOTAL_OUTPUT_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = output_queues[i];

		/* set input meta format */
		if (pipe->vdev_nodes[node_idx].desc.id == MTK_RAW_META_IN)
			pipe->vdev_nodes[node_idx].desc.fmts = camsys_get_meta_fmts();

		pipe->vdev_nodes[node_idx++].desc.name =
			output_queue_names[pipe->id][i];
	}

	/* Setup the capture queue */
	for (i = 0; i < MTK_RAW_TOTAL_CAPTURE_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = capture_queues[i];

		/* set partial meta and external meta format */
		if (pipe->vdev_nodes[node_idx].desc.id >= MTK_RAW_META_OUT_BEGIN &&
		    pipe->vdev_nodes[node_idx].desc.id <= MTK_RAW_META_OUT_2)
			pipe->vdev_nodes[node_idx].desc.fmts = camsys_get_meta_fmts();

		pipe->vdev_nodes[node_idx++].desc.name =
			capture_queue_names[pipe->id][i];
	}
}

static void mtk_raw_pipeline_ctrl_setup(struct mtk_raw_pipeline *pipe)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_ctrl *ctrl;
	struct device *dev = pipe->raw->devs[pipe->id];
	int ret = 0;

	ctrl_hdlr = &pipe->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 5);
	if (ret) {
		dev_info(dev, "v4l2_ctrl_handler init failed\n");
		return;
	}
	mutex_init(&pipe->res_config.resource_lock);
	mutex_init(&pipe->try_res_config.resource_lock);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_limit, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hwn_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frz_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &bin_try, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	v4l2_ctrl_new_custom(ctrl_hdlr, &res_plan_policy, NULL);
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &res_pixel_rate, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &frame_sync_id, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &raw_path, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	v4l2_ctrl_new_std(ctrl_hdlr, &cam_ctrl_ops,
			  V4L2_CID_HBLANK, 0, 65535, 1, 0);
	v4l2_ctrl_new_std(ctrl_hdlr, &cam_ctrl_ops,
			  V4L2_CID_VBLANK, 0, 65535, 1, 0);

	/* PDE */
	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_pde_info, NULL);

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &mtk_feature, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_res_update, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &cfg_res_ctrl, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	ctrl = v4l2_ctrl_new_custom(ctrl_hdlr,
				    &mtk_camsys_hw_mode, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	pipe->res_config.hwn_limit_max = hwn_limit.def;
	pipe->res_config.frz_limit = frz_limit.def;
	pipe->res_config.bin_limit = bin_limit.def;
	pipe->res_config.res_plan = res_plan_policy.def;
	pipe->feature_pending = mtk_feature.def;
	pipe->sync_id = frame_sync_id.def;
	pipe->sensor_mode_update = cfg_res_update.def;
	pipe->pde_config.pde_info.pdo_max_size = cfg_pde_info.def;
	pipe->pde_config.pde_info.pdi_max_size = cfg_pde_info.def;
	pipe->pde_config.pde_info.pd_table_offset = cfg_pde_info.def;
	pipe->subdev.ctrl_handler = ctrl_hdlr;
	pipe->hw_mode = mtk_camsys_hw_mode.def;
}

static int mtk_raw_pipeline_register(unsigned int id, struct device *dev,
				     struct mtk_raw_pipeline *pipe,
				     struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->raw->cam_dev);
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i;
	int ret;

	pipe->id = id;

	/* Initialize raw_pipe subdev */
	v4l2_subdev_init(sd, &mtk_raw_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_cam_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), pipe->id);
	v4l2_set_subdevdata(sd, pipe);
	mtk_raw_pipeline_ctrl_setup(pipe);
	dev_info(dev, "%s: %s\n", __func__, sd->name);

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_info(dev, "Failed to register subdev: %d\n", ret);
		return ret;
	}

	mtk_raw_pipeline_queue_setup(pipe);
	/* setup pads of raw pipeline */
	for (i = 0; i < ARRAY_SIZE(pipe->pads); i++) {
		pipe->pads[i].flags = i < MTK_RAW_SOURCE_BEGIN ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	media_entity_pads_init(&sd->entity, ARRAY_SIZE(pipe->pads), pipe->pads);

	/* setup video node */
	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
		video = pipe->vdev_nodes + i;

		switch (pipe->id) {
		case MTKCAM_SUBDEV_RAW_0:
		case MTKCAM_SUBDEV_RAW_1:
		case MTKCAM_SUBDEV_RAW_2:
			video->uid.pipe_id = pipe->id;
			break;
		default:
			dev_err(dev, "invalid pipe id\n");
			return -EINVAL;
		}

		video->uid.id = video->desc.dma_port;
		video->ctx = &cam->ctxs[id];
		ret = mtk_cam_video_register(video, v4l2_dev);
		if (ret)
			goto fail_unregister_video;

		if (V4L2_TYPE_IS_OUTPUT(video->desc.buf_type))
			ret = media_create_pad_link(&video->vdev.entity, 0,
						    &sd->entity,
						    video->desc.id,
						    video->desc.link_flags);
		else
			ret = media_create_pad_link(&sd->entity,
						    video->desc.id,
						    &video->vdev.entity, 0,
						    video->desc.link_flags);

		if (ret)
			goto fail_unregister_video;
	}

	for (i = 0; i < sd->entity.num_pads; i++)
		pipe->cfg[i].mbus_fmt = mfmt_default;

	return 0;

fail_unregister_video:
	if (!i)
		return ret;

	for (i = i - 1; i >= 0; i--)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	return ret;
}

static void mtk_raw_pipeline_unregister(struct mtk_raw_pipeline *pipe)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);
	v4l2_ctrl_handler_free(&pipe->ctrl_handler);
	mutex_destroy(&pipe->res_config.resource_lock);
	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
}

int mtk_cam_raw_setup_dependencies(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct device *consumer, *supplier;
	struct device_link *link;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	int i;

	for (i = 0; i < cam_dev->num_raw_devices; i++) {
		consumer = raw->devs[i];
		supplier = raw->yuvs[i];
		if (!consumer || !supplier) {
			dev_info(dev, "failed to get raw/yuv dev for id %d\n", i);
			continue;
		}

		raw_dev = dev_get_drvdata(consumer);
		yuv_dev = dev_get_drvdata(supplier);
		raw_dev->yuv_base = yuv_dev->base;

		link = device_link_add(consumer, supplier,
				       DL_FLAG_AUTOREMOVE_CONSUMER |
				       DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_err(dev, "Unable to create link between %s and %s\n",
				dev_name(consumer), dev_name(supplier));
			return -ENODEV;
		}
	}

	return 0;
}

int mtk_cam_raw_register_entities(struct mtk_raw *raw, struct v4l2_device *v4l2_dev)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	unsigned int i;
	int ret;

	for (i = 0; i < cam_dev->num_raw_devices; i++) {
		struct mtk_raw_pipeline *pipe = raw->pipelines + i;

		pipe->raw = raw;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		ret = mtk_raw_pipeline_register(MTKCAM_SUBDEV_RAW_0 + i,
						raw->devs[i],
						raw->pipelines + i, v4l2_dev);
		if (ret)
			return ret;
	}
	return 0;
}

void mtk_cam_raw_unregister_entities(struct mtk_raw *raw)
{
	struct device *dev = raw->cam_dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	unsigned int i;

	for (i = 0; i < cam_dev->num_raw_devices; i++)
		mtk_raw_pipeline_unregister(raw->pipelines + i);
}

static int mtk_raw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev;
	struct v4l2_subdev *sd;
	int ret;

	dev_dbg(dev, "camsys | start %s\n", __func__);

	raw_dev = devm_kzalloc(dev, sizeof(*raw_dev), GFP_KERNEL);
	if (!raw_dev)
		return -ENOMEM;

	raw_dev->dev = dev;
	dev_set_drvdata(dev, raw_dev);

	ret = mtk_raw_of_probe(pdev, raw_dev);
	if (ret)
		return ret;

	raw_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));
	raw_dev->msg_buffer = devm_kzalloc(dev, raw_dev->fifo_size, GFP_KERNEL);
	if (!raw_dev->msg_buffer)
		return -ENOMEM;

	/* register raw as mtk_cam async child */
	sd = &raw_dev->subdev;
	v4l2_subdev_init(sd, &mtk_raw_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "%s",
		 of_node_full_name(dev->of_node));
	sd->dev = dev;
	sd->owner = THIS_MODULE;

	ret = v4l2_async_register_subdev(sd);
	if (ret) {
		dev_err(dev, "%s failed on async_register_subdev\n", __func__);
		return ret;
	}
	dev_dbg(dev, "%s id:%d register subdev\n", __func__, raw_dev->id);

	pm_runtime_enable(dev);

	dev_info(dev, "camsys | [%s] success\n", __func__);

	return 0;
}

static int mtk_raw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);
	struct v4l2_subdev *sd = &raw_dev->subdev;

	dev_dbg(dev, "camsys | start %s\n", __func__);

	pm_runtime_disable(dev);

	v4l2_async_unregister_subdev(sd);

	return 0;
}

static int mtk_raw_runtime_suspend(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);

	dev_dbg(dev, "%s:disable clock\n", __func__);

	disable_irq(drvdata->irq);

	mtk_cam_raw_reset(drvdata);

	clk_bulk_disable_unprepare(drvdata->num_clks, drvdata->clk_b);

	return 0;
}

static int mtk_raw_runtime_resume(struct device *dev)
{
	struct mtk_raw_device *drvdata = dev_get_drvdata(dev);
	int ret;

	/* reset_msgfifo before enable_irq */
	ret = reset_msgfifo(drvdata);
	if (ret)
		return ret;

	enable_irq(drvdata->irq);

	dev_dbg(dev, "%s:enable clock\n", __func__);

	ret = clk_bulk_prepare_enable(drvdata->num_clks, drvdata->clk_b);
	if (ret) {
		dev_info(dev, "failed at clk_bulk_prepare_enable, ret = %d\n", ret);
		clk_bulk_disable_unprepare(drvdata->num_clks, drvdata->clk_b);
		return ret;
	}

	mtk_cam_raw_reset(drvdata);

	return 0;
}

static const struct dev_pm_ops mtk_raw_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_raw_runtime_suspend, mtk_raw_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_raw_of_ids[] = {
	{.compatible = "mediatek,cam-raw",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_raw_of_ids);

struct platform_driver mtk_cam_raw_driver = {
	.probe   = mtk_raw_probe,
	.remove  = mtk_raw_remove,
	.driver  = {
		.name  = "mtk-cam raw",
		.of_match_table = of_match_ptr(mtk_raw_of_ids),
		.pm     = &mtk_raw_pm_ops,
	}
};

static inline unsigned int mtk_yuv_get_err_status(unsigned int irq_status)
{
	return irq_status & YUV_DMA_ERR_ST;
}

static irqreturn_t mtk_irq_yuv(int irq, void *data)
{
	struct mtk_yuv_device *drvdata = (struct mtk_yuv_device *)data;
	struct device *dev = drvdata->dev;

	unsigned int irq_status, err_status, dma_done_status;
	unsigned int drop_status, dma_ofl_status;

	irq_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT_STAT);
	dma_done_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT2_STAT);
	drop_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT4_STAT);
	dma_ofl_status =
		readl_relaxed(drvdata->base + REG_CTL_RAW_INT5_STAT);

	err_status = mtk_yuv_get_err_status(irq_status);

	if (unlikely(debug_raw))
		dev_dbg(dev, "YUV-INT:0x%x(err:0x%x) INT2/4/5 0x%x/0x%x/0x%x\n",
			irq_status, err_status,
			dma_done_status, drop_status, dma_ofl_status);

	return IRQ_HANDLED;
}

static int mtk_yuv_pm_suspend_prepare(struct mtk_yuv_device *dev)
{
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev->dev);
	return ret;
}

static int mtk_yuv_pm_post_suspend(struct mtk_yuv_device *dev)
{
	int ret;

	dev_dbg(dev->dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev->dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev->dev);
	if (ret)
		return ret;

	return 0;
}

static int yuv_pm_notifier(struct notifier_block *nb,
			   unsigned long action, void *data)
{
	struct mtk_yuv_device *yuv_dev =
			container_of(nb, struct mtk_yuv_device, pm_notifier);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		mtk_yuv_pm_suspend_prepare(yuv_dev);
		break;
	case PM_POST_SUSPEND:
		mtk_yuv_pm_post_suspend(yuv_dev);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int mtk_yuv_of_probe(struct platform_device *pdev,
			    struct mtk_yuv_device *drvdata)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret;
	int n_clks;

	ret = of_property_read_u32(dev->of_node, "mediatek,cam-id",
				   &drvdata->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34))) {
		dev_err(dev, "%s: No suitable DMA available\n", __func__);
		return -EIO;
	}

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dma_set_max_seg_size(dev, UINT_MAX)) {
		dev_err(dev, "Failed to set DMA segment size\n");
		return -EIO;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(drvdata->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_yuv, 0,
			       dev_name(dev), drvdata);
	if (ret) {
		dev_err(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	n_clks = devm_clk_bulk_get_all(dev, &drvdata->clk_b);
	if (n_clks < 0) {
		dev_err(dev, "failed to devm_clk_bulk_get_all=%d\n", n_clks);
		return n_clks;
	}

	drvdata->num_clks = n_clks;
	dev_info(dev, "clk_num:%d\n", drvdata->num_clks);

#ifdef CONFIG_PM_SLEEP
	drvdata->pm_notifier.notifier_call = yuv_pm_notifier;
	ret = register_pm_notifier(&drvdata->pm_notifier);
	if (ret) {
		dev_err(dev, "failed to register notifier block.\n");
		return ret;
	}
#endif

	return 0;
}

static int mtk_yuv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *drvdata;
	struct v4l2_subdev *sd;
	int ret;

	dev_dbg(dev, "camsys | start %s\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_yuv_of_probe(pdev, drvdata);
	if (ret) {
		dev_info(dev, "mtk_yuv_of_probe failed\n");
		return ret;
	}

	/* register yuv as mtk_cam async child */
	sd = &drvdata->subdev;
	v4l2_subdev_init(sd, &mtk_raw_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "%s",
		 of_node_full_name(dev->of_node));
	sd->dev = dev;
	sd->owner = THIS_MODULE;

	ret = v4l2_async_register_subdev(sd);
	if (ret) {
		dev_err(dev, "%s failed on async_register_subdev\n", __func__);
		return ret;
	}

	pm_runtime_enable(dev);

	dev_info(dev, "camsys | [%s] success\n", __func__);

	return 0;
}

static int mtk_yuv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_yuv_device *yuv_dev = dev_get_drvdata(dev);
	struct v4l2_subdev *sd = &yuv_dev->subdev;

	dev_dbg(dev, "camsys | start %s\n", __func__);

	unregister_pm_notifier(&yuv_dev->pm_notifier);

	pm_runtime_disable(dev);

	v4l2_async_unregister_subdev(sd);

	return 0;
}

/* driver for yuv part */
static int mtk_yuv_runtime_suspend(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);

	dev_info(dev, "%s:disable clock\n", __func__);

	clk_bulk_disable_unprepare(drvdata->num_clks, drvdata->clk_b);

	return 0;
}

static int mtk_yuv_runtime_resume(struct device *dev)
{
	struct mtk_yuv_device *drvdata = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "%s:enable clock\n", __func__);

	ret = clk_bulk_prepare_enable(drvdata->num_clks, drvdata->clk_b);
	if (ret) {
		dev_info(dev, "failed at clk_bulk_prepare_enable, ret = %d\n", ret);
		clk_bulk_disable_unprepare(drvdata->num_clks, drvdata->clk_b);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mtk_yuv_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_yuv_runtime_suspend, mtk_yuv_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_yuv_of_ids[] = {
	{.compatible = "mediatek,cam-yuv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_yuv_of_ids);

struct platform_driver mtk_cam_yuv_driver = {
	.probe   = mtk_yuv_probe,
	.remove  = mtk_yuv_remove,
	.driver  = {
		.name  = "mtk-cam yuv",
		.of_match_table = of_match_ptr(mtk_yuv_of_ids),
		.pm     = &mtk_yuv_pm_ops,
	}
};
