// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include "mtk_cam-feature.h"

struct mtk_cam_ctx;

/*
 * mtk_cam_is_[feature] is used for raw_feature,
 * which must not change during streaming.
 */

static bool mtk_cam_is_no_seninf(struct mtk_cam_ctx *ctx)
{
	if (!media_pad_remote_pad_first(&ctx->pipe->pads[MTK_RAW_SINK]))
		return true;

	return false;
}

bool mtk_cam_is_pure_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (ctx->pipe->feature_pending & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK ||
	    mtk_cam_is_no_seninf(ctx))
		return true;
	else
		return false;
}

bool mtk_cam_is_m2m(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_raw_num)
		return false;

	if (mtk_cam_is_no_seninf(ctx))
		return true;
	else
		return mtk_cam_feature_is_m2m(ctx->pipe->feature_pending);
}

int mtk_cam_get_feature_switch(struct mtk_raw_pipeline *raw_pipe,
			       int prev)
{
	int cur = raw_pipe->feature_pending;
	int res = EXPOSURE_CHANGE_NONE;

	if (cur == prev)
		return EXPOSURE_CHANGE_NONE;

	dev_dbg(raw_pipe->subdev.dev, "[%s] res:%d\n", __func__, res);

	return res;
}
