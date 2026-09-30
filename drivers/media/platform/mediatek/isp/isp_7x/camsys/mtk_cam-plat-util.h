/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_PLAT_UTIL_H
#define __MTK_CAM_PLAT_UTIL_H

int camsys_get_meta_version(bool major);
int camsys_get_meta_size(u32 video_id);
const struct mtk_cam_format_desc *camsys_get_meta_fmts(void);
void camsys_set_meta_stats_info(u32 dma_port, struct vb2_buffer *vb,
				struct mtk_raw_pde_config *pde_cfg);
uint64_t *camsys_get_timestamp_addr(void *vaddr);

#endif /*__MTK_CAM_PLAT_UTIL_H*/
