/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_SENINF_DRV_H
#define __MTK_CAM_SENINF_DRV_H
#include "mtk_cam-seninf.h"

extern struct platform_driver seninf_core_pdrv;
extern struct platform_driver seninf_pdrv;

int update_isp_clk(struct seninf_ctx *ctx);

#endif /*__MTK_CAM_SENINF_DRV_H */
