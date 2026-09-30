// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include <linux/module.h>
#include <linux/init.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-video.h"
#include "mtk_cam-plat-util.h"
#include "mtk_cam-meta-mt8188.h"

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_1K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_1K) + \
	      MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_LTMSO_SIZE + \
	      MTK_CAM_UAPI_FLK_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_TSFSO_SIZE * 2 + /* r1 & r2 */ \
	      MTK_CAM_UAPI_TNCSYO_SIZE \
	      , (4 * SZ_1K))

#define RAW_STATS_1_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_1K) + \
	      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, (4 * SZ_1K))

#define RAW_STATS_2_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_2), SZ_1K) + \
	      MTK_CAM_UAPI_ACTSO_SIZE, (4 * SZ_1K))

/* ISP platform meta format */
static const struct mtk_cam_format_desc meta_fmts[] = {
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = RAW_STATS_CFG_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = RAW_STATS_0_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_AF,
			.buffersize = RAW_STATS_1_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LCS,
			.buffersize = RAW_STATS_2_SIZE,
		},
	},
};

struct mtk_cam_vb2_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	void				*cookie;
	dma_addr_t			dma_addr;
	unsigned long			attrs;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	refcount_t			refcount;
	struct sg_table			*sgt_base;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
	struct iosys_map map;
};

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, unsigned long *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static void vb2_sync_for_device(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

uint64_t *camsys_get_timestamp_addr(void *vaddr)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats0;

	stats0 = (struct mtk_cam_uapi_meta_raw_stats_0 *)vaddr;
	return (uint64_t *)(stats0->timestamp.timestamp_buf);
}
EXPORT_SYMBOL_GPL(camsys_get_timestamp_addr);

void camsys_set_meta_stats_info(u32 dma_port, struct vb2_buffer *vb,
				struct mtk_raw_pde_config *pde_cfg)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats0;
	struct mtk_cam_uapi_meta_raw_stats_1 *stats1;
	unsigned long offset;
	unsigned int plane;
	void *vaddr = vb2_plane_vaddr(vb, 0);

	switch (dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_0:
		stats0 = (struct mtk_cam_uapi_meta_raw_stats_0 *)vaddr;
		offset = sizeof(*stats0);
		set_payload(&stats0->ae_awb_stats.aao_buf, MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ae_awb_stats.aaho_buf,
			    MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ltm_stats.ltmso_buf, MTK_CAM_UAPI_LTMSO_SIZE, &offset);
		set_payload(&stats0->flk_stats.flko_buf, MTK_CAM_UAPI_FLK_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r1_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r2_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tncy_stats.tncsyo_buf, MTK_CAM_UAPI_TNCSYO_SIZE, &offset);
		if (pde_cfg) {
			if (pde_cfg->pde_info.pd_table_offset) {
				set_payload(&stats0->pde_stats.pdo_buf,
					    pde_cfg->pde_info.pdo_max_size,
					    &offset);
			}
		}
		/* Use scp reserved cache buffer, do buffer cache sync after fill in meta payload */
		for (plane = 0; plane < vb->num_planes; ++plane)
			vb2_sync_for_device(vb->planes[plane].mem_priv);
		break;
	case MTKCAM_IPI_RAW_META_STATS_1:
		stats1 = (struct mtk_cam_uapi_meta_raw_stats_1 *)vaddr;
		offset = sizeof(*stats1);
		set_payload(&stats1->af_stats.afo_buf, MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);
		/* Use scp reserved cache buffer, do buffer cache sync after fill in meta payload */
		for (plane = 0; plane < vb->num_planes; ++plane)
			vb2_sync_for_device(vb->planes[plane].mem_priv);
		break;
	case MTKCAM_IPI_RAW_META_STATS_2:
		pr_info("stats 2 not support");
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}
EXPORT_SYMBOL_GPL(camsys_set_meta_stats_info);

int camsys_get_meta_version(bool major)
{
	if (major)
		return MTK_CAM_META_VERSION_MAJOR;
	else
		return MTK_CAM_META_VERSION_MINOR;
}
EXPORT_SYMBOL_GPL(camsys_get_meta_version);

int camsys_get_meta_size(u32 video_id)
{
	switch (video_id) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		return RAW_STATS_CFG_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_0:
		return RAW_STATS_0_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_1:
		return RAW_STATS_1_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_2:
		return RAW_STATS_2_SIZE;
	default:
		pr_debug("%s: no support stats(%d)\n", __func__, video_id);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(camsys_get_meta_size);

const struct mtk_cam_format_desc *camsys_get_meta_fmts(void)
{
	return meta_fmts;
}
EXPORT_SYMBOL_GPL(camsys_get_meta_fmts);

MODULE_LICENSE("GPL");
