/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_HW_H_
#define _MTK_IMGSYS_HW_H_

#include "linux/dma-buf.h"
#include "linux/scatterlist.h"
#include "mtk_header_desc.h"
#include <linux/clk.h>

#define SCP_COMPOSING_MAX_NUM		30
#define IMGSYS_WORKING_BUF_NUM		32

#define FRAME_STATE_INIT		0
#define FRAME_STATE_HW_TIMEOUT		1

struct mtk_imgsys_hw_working_buf {
	struct singlenode_desc_norm *sd_norm;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t scp_daddr;
	dma_addr_t isp_daddr;
	struct list_head list;
};

struct mtk_imgsys_hw_working_buf_list {
	struct list_head list;
	spinlock_t lock; /* protect the list */
};

struct mtk_imgsys_init_array {
	unsigned int    ofset;
	unsigned int    val;
};

#endif /* _MTK_IMGSYS_HW_H_ */

