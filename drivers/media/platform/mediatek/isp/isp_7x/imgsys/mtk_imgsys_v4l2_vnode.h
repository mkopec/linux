/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */
#ifndef _MTK_IMGSYS_V4L2_VNODE_H_
#define _MTK_IMGSYS_V4L2_VNODE_H_

#include "mtk_imgsys-formats.h"
#include "mtk_imgsys-of.h"
#include "mtk_imgsys-dev.h"
#include "modules/mtk_dip_v4l2_vnode.h"
#include "modules/mtk_traw_v4l2_vnode.h"
#include "modules/mtk_pqdip_v4l2_vnode.h"
#include "modules/mtk_wpe_v4l2_vnode.h"
#include "modules/mtk_me_v4l2_vnode.h"

/*
 * TODO: register module pipeline desc in module order
 */
enum mtk_imgsys_module_id {
	IMGSYS_MODULE_TRAW = 0,
	IMGSYS_MODULE_DIP,
	IMGSYS_MODULE_PQDIP,
	IMGSYS_MODULE_ME,
	IMGSYS_MODULE_WPE,
	IMGSYS_MODULE_ADL,
	IMGSYS_MODULE_MAIN,
	IMGSYS_MODULE_NUM,
};

static const struct mtk_imgsys_mod_pipe_desc module_pipe_isp7[] = {
	[IMGSYS_MODULE_TRAW] = {
		.vnode_desc = traw_setting,
		.node_num = ARRAY_SIZE(traw_setting),
	},
	[IMGSYS_MODULE_DIP] = {
		.vnode_desc = dip_setting,
		.node_num = ARRAY_SIZE(dip_setting),
	},
	[IMGSYS_MODULE_PQDIP] = {
		.vnode_desc = pqdip_setting,
		.node_num = ARRAY_SIZE(pqdip_setting),
	},
	[IMGSYS_MODULE_ME] = {
		.vnode_desc = me_setting,
		.node_num = ARRAY_SIZE(me_setting),
	},
	[IMGSYS_MODULE_WPE] = {
		.vnode_desc = wpe_setting,
		.node_num = ARRAY_SIZE(wpe_setting),
	},
	[IMGSYS_MODULE_ADL] = {
		.vnode_desc = NULL,
		.node_num = 0,
	},
	[IMGSYS_MODULE_MAIN] = {
		.vnode_desc = NULL,
		.node_num = 0,
	}
};

static const struct mtk_imgsys_dev_format tuning_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_metai,
	},
};

static const struct mtk_imgsys_dev_format ctrlmeta_fmts[] = {
	{
		.fmt = &mtk_imgsys_format_ctrlmeta,
	},
};

static struct mtk_imgsys_video_device_desc
queues_setting[MTK_IMGSYS_VIDEO_NODE_ID_TOTAL_NUM] = {
	[MTK_IMGSYS_VIDEO_NODE_ID_TUNING_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TUNING_OUT,
		.name = "Tuning",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 1,
		.flags = 0,
		.fmts = tuning_fmts,
		.num_fmts = ARRAY_SIZE(tuning_fmts),
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Tuning data",
	},
	[MTK_IMGSYS_VIDEO_NODE_ID_CTRLMETA_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_CTRLMETA_OUT,
		.name = "CtrlMeta",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 1,
		.flags = 0,
		.fmts = ctrlmeta_fmts,
		.num_fmts = ARRAY_SIZE(ctrlmeta_fmts),
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.description = "Control meta data for flow control",
	},
};

static const struct mtk_imgsys_pipe_desc
pipe_settings_isp7[MTK_IMGSYS_PIPE_ID_TOTAL_NUM] = {
	{
		.name = MTK_DIP_DEV_DIP_PREVIEW_NAME,
		.id = MTK_IMGSYS_PIPE_ID_PREVIEW,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),
	},
};
#endif
