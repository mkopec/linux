/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_FEATURE_H
#define __MTK_CAM_FEATURE_H

#include "mtk_cam.h"
#include "mtk_cam-raw.h"

static inline bool mtk_cam_feature_is_m2m(int feature)
{
	return !!(feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK) ||
			!!(feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK);
}

static inline bool mtk_cam_feature_is_pure_m2m(int feature)
{
	return !!(feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK);
}

bool mtk_cam_is_m2m(struct mtk_cam_ctx *ctx);
bool mtk_cam_is_pure_m2m(struct mtk_cam_ctx *ctx);
int mtk_cam_get_feature_switch(struct mtk_raw_pipeline *raw_pipe,
			       int prev);
#endif /*__MTK_CAM_FEATURE_H */
