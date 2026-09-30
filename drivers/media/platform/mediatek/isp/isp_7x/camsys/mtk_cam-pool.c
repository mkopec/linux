// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/remoteproc.h>
#include <linux/spinlock.h>

#include "mtk_cam.h"
#include "mtk_cam-pool.h"

int mtk_cam_working_buf_pool_init(struct mtk_cam_ctx *ctx, struct device *dev)
{
	int i, ret;
	void *ptr;
	dma_addr_t addr;
	struct mtk_cam_device *cam = ctx->cam;
	const unsigned int working_buf_size = round_up(CQ_BUF_SIZE, PAGE_SIZE);
	const unsigned int msg_buf_size = round_up(IPI_FRAME_BUF_SIZE, PAGE_SIZE);

	INIT_LIST_HEAD(&ctx->buf_pool.cam_freelist.list);
	spin_lock_init(&ctx->buf_pool.cam_freelist.lock);
	ctx->buf_pool.cam_freelist.cnt = 0;
	ctx->buf_pool.working_buf_size = CAM_CQ_BUF_NUM * working_buf_size;
	ctx->buf_pool.msg_buf_size = CAM_CQ_BUF_NUM * msg_buf_size;
	ctx->buf_pool.raw_workbuf_size = round_up(SIZE_OF_RAW_WORKBUF, PAGE_SIZE);
	ctx->buf_pool.priv_workbuf_size = round_up(SIZE_OF_RAW_PRIV, PAGE_SIZE);
	ctx->buf_pool.session_buf_size = round_up(SIZE_OF_SESSION, PAGE_SIZE);

	/* raw working buffer */
	ptr = dma_alloc_coherent(cam->smem_dev, ctx->buf_pool.raw_workbuf_size,
				 &addr, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ctx->buf_pool.raw_workbuf_scp_addr = addr;
	ctx->buf_pool.raw_workbuf_va = ptr;
	dev_dbg(dev, "[%s] raw working buf scp addr:%pad va:%pK size %u\n",
		__func__,
		&ctx->buf_pool.raw_workbuf_scp_addr,
		ctx->buf_pool.raw_workbuf_va,
		ctx->buf_pool.raw_workbuf_size);

	/* raw priv working buffer */
	ptr = dma_alloc_coherent(cam->smem_dev, ctx->buf_pool.priv_workbuf_size,
				 &addr, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ctx->buf_pool.priv_workbuf_scp_addr = addr;
	ctx->buf_pool.priv_workbuf_va = ptr;
	dev_dbg(dev, "[%s] raw pri working buf scp addr:%pad va:%pK size %u\n",
		__func__,
		&ctx->buf_pool.priv_workbuf_scp_addr,
		ctx->buf_pool.priv_workbuf_va,
		ctx->buf_pool.priv_workbuf_size);

	/* session buffer */
	ptr = dma_alloc_coherent(cam->smem_dev, ctx->buf_pool.session_buf_size,
				 &addr, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ctx->buf_pool.session_buf_scp_addr = addr;
	ctx->buf_pool.session_buf_va = ptr;
	dev_dbg(dev, "[%s] session buf scp addr:%pad va:%pK size %u\n",
		__func__,
		&ctx->buf_pool.session_buf_scp_addr,
		ctx->buf_pool.session_buf_va,
		ctx->buf_pool.session_buf_size);

	/* working buffer */
	ptr = dma_alloc_coherent(cam->smem_dev, ctx->buf_pool.working_buf_size,
				 &addr, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ctx->buf_pool.working_buf_scp_addr = addr;
	ctx->buf_pool.working_buf_va = ptr;
	addr = dma_map_resource(dev, addr, ctx->buf_pool.working_buf_size,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dev, addr)) {
		dev_err(dev, "failed to map scp iova\n");
		ret = -ENOMEM;
		goto fail_free_mem;
	}
	ctx->buf_pool.working_buf_iova = addr;
	dev_dbg(dev, "[%s] CQ buf scp addr:%pad va:%pK iova:%pad size %u\n",
		__func__,
		&ctx->buf_pool.working_buf_scp_addr,
		ctx->buf_pool.working_buf_va,
		&ctx->buf_pool.working_buf_iova,
		ctx->buf_pool.working_buf_size);

	/* msg buffer */
	ptr = dma_alloc_coherent(cam->smem_dev, ctx->buf_pool.msg_buf_size,
				 &addr, GFP_KERNEL);
	if (!ptr) {
		ret = -ENOMEM;
		goto fail_free_mem;
	}
	ctx->buf_pool.msg_buf_scp_addr = addr;
	ctx->buf_pool.msg_buf_va = ptr;
	dev_dbg(dev, "[%s] msg buf scp addr:%pad va:%pK size %u\n",
		__func__, &ctx->buf_pool.msg_buf_scp_addr,
		ctx->buf_pool.msg_buf_va, ctx->buf_pool.msg_buf_size);

	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &ctx->buf_pool.working_buf[i];
		unsigned int offset = i * working_buf_size;
		unsigned int offset_msg = i * msg_buf_size;

		buf->ctx = ctx;
		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.scp_addr = ctx->buf_pool.working_buf_scp_addr + offset;
		buf->buffer.size = working_buf_size;
		buf->msg_buffer.va = ctx->buf_pool.msg_buf_va + offset_msg;
		buf->msg_buffer.scp_addr = ctx->buf_pool.msg_buf_scp_addr + offset_msg;
		buf->msg_buffer.size = msg_buf_size;
		buf->s_data = NULL;

		list_add_tail(&buf->list_entry, &ctx->buf_pool.cam_freelist.list);
		ctx->buf_pool.cam_freelist.cnt++;
	}

	dev_dbg(ctx->cam->dev,
		"%s: ctx(%d): cq buffers init, freebuf cnt(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.cam_freelist.cnt);

	return 0;
fail_free_mem:
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.working_buf_size,
			  ctx->buf_pool.working_buf_va,
			  ctx->buf_pool.working_buf_scp_addr);
	ctx->buf_pool.working_buf_scp_addr = 0;

	return ret;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx, struct device *dev)
{
	struct mtk_cam_device *cam = ctx->cam;

	/* msg buffer */
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.msg_buf_size,
			  ctx->buf_pool.msg_buf_va,
			  ctx->buf_pool.msg_buf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):msg buffers release, va:%pK scp_addr %pad sz %u\n",
		__func__, ctx->stream_id,
		ctx->buf_pool.msg_buf_va,
		&ctx->buf_pool.msg_buf_scp_addr,
		ctx->buf_pool.msg_buf_size);

	/* working buffer */
	dma_unmap_page_attrs(dev, ctx->buf_pool.working_buf_iova,
			     ctx->buf_pool.working_buf_size,
			     DMA_BIDIRECTIONAL,
			     DMA_ATTR_SKIP_CPU_SYNC);
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.working_buf_size,
			  ctx->buf_pool.working_buf_va,
			  ctx->buf_pool.working_buf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):cq buffers release, iova %pad scp_addr %pad sz %u\n",
		__func__, ctx->stream_id,
		&ctx->buf_pool.working_buf_iova,
		&ctx->buf_pool.working_buf_scp_addr,
		ctx->buf_pool.working_buf_size);

	/* session buffer */
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.session_buf_size,
			  ctx->buf_pool.session_buf_va,
			  ctx->buf_pool.session_buf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):session buffers release, scp_addr %pad sz %u\n",
		__func__, ctx->stream_id,
		&ctx->buf_pool.session_buf_scp_addr,
		ctx->buf_pool.session_buf_size);

	/* raw priv working buffer */
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.priv_workbuf_size,
			  ctx->buf_pool.priv_workbuf_va,
			  ctx->buf_pool.priv_workbuf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):raw pri working buffers release, scp_addr %pad sz %u\n",
		__func__, ctx->stream_id,
		&ctx->buf_pool.priv_workbuf_scp_addr,
		ctx->buf_pool.priv_workbuf_size);

	/* raw working buffer */
	dma_free_coherent(cam->smem_dev, ctx->buf_pool.raw_workbuf_size,
			  ctx->buf_pool.raw_workbuf_va,
			  ctx->buf_pool.raw_workbuf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):raw working buffers release, scp_addr %pad sz %u\n",
		__func__, ctx->stream_id,
		&ctx->buf_pool.raw_workbuf_scp_addr,
		ctx->buf_pool.raw_workbuf_size);
}

void
mtk_cam_working_buf_put(struct mtk_cam_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->buf_pool.cam_freelist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.cam_freelist.list);
	cnt = ++ctx->buf_pool.cam_freelist.cnt;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);
}

struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->buf_pool.cam_freelist.lock);
	if (list_empty(&ctx->buf_pool.cam_freelist.list)) {
		spin_unlock(&ctx->buf_pool.cam_freelist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.cam_freelist.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->buf_pool.cam_freelist.cnt;
	buf_entry->ctx = ctx;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);

	return buf_entry;
}

int mtk_cam_img_working_buf_pool_init(struct mtk_cam_ctx *ctx, int buf_num,
				      struct device *dev)
{
	int i, ret;
	u32 working_buf_size;
	void *ptr;
	dma_addr_t addr;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_video_device *vdev;

	if (buf_num > CAM_IMG_BUF_NUM) {
		dev_err(ctx->cam->dev,
			"%s: ctx(%d): image buffers number too large(%d)\n",
			__func__, ctx->stream_id, buf_num);
		WARN_ON(1);
		return 0;
	}

	vdev = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	working_buf_size = vdev->active_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
	INIT_LIST_HEAD(&ctx->img_buf_pool.cam_freeimglist.list);
	spin_lock_init(&ctx->img_buf_pool.cam_freeimglist.lock);
	ctx->img_buf_pool.cam_freeimglist.cnt = 0;
	ctx->img_buf_pool.working_img_buf_size = buf_num * working_buf_size;
	ptr = dma_alloc_coherent(cam->smem_dev,
				 ctx->img_buf_pool.working_img_buf_size,
				 &addr, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ctx->img_buf_pool.working_img_buf_scp_addr = addr;
	ctx->img_buf_pool.working_img_buf_va = ptr;
	addr = dma_map_resource(dev, addr, ctx->img_buf_pool.working_img_buf_size,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(dev, addr)) {
		dev_err(dev, "failed to map scp iova\n");
		ret = -ENOMEM;
		goto fail_free_mem;
	}
	ctx->img_buf_pool.working_img_buf_iova = addr;
	dev_dbg(dev, "[%s] img working buf scp addr:%pad va:%pK iova: %pad size %d\n",
		__func__,
		&ctx->img_buf_pool.working_img_buf_scp_addr,
		ctx->img_buf_pool.working_img_buf_va,
		&ctx->img_buf_pool.working_img_buf_iova,
		ctx->img_buf_pool.working_img_buf_size);

	for (i = 0; i < buf_num; i++) {
		struct mtk_cam_img_working_buf_entry *buf = &ctx->img_buf_pool.img_working_buf[i];
		int offset = i * working_buf_size;

		buf->ctx = ctx;
		buf->img_buffer.va = ctx->img_buf_pool.working_img_buf_va + offset;
		buf->img_buffer.scp_addr = ctx->img_buf_pool.working_img_buf_scp_addr + offset;
		buf->img_buffer.iova = ctx->img_buf_pool.working_img_buf_iova + offset;
		buf->img_buffer.size = working_buf_size;
		list_add_tail(&buf->list_entry, &ctx->img_buf_pool.cam_freeimglist.list);
		ctx->img_buf_pool.cam_freeimglist.cnt++;
	}

	dev_dbg(dev,
		"%s: ctx(%d): image buffers init, freebuf cnt(%d)\n",
		__func__, ctx->stream_id, ctx->img_buf_pool.cam_freeimglist.cnt);
	return 0;

fail_free_mem:
	dma_free_coherent(cam->smem_dev, ctx->img_buf_pool.working_img_buf_size,
			  ctx->img_buf_pool.working_img_buf_va,
			  ctx->img_buf_pool.working_img_buf_scp_addr);
	return ret;
}

void mtk_cam_img_working_buf_pool_release(struct mtk_cam_ctx *ctx,
					  struct device *dev)
{
	struct mtk_cam_device *cam = ctx->cam;

	dma_unmap_page_attrs(dev, ctx->img_buf_pool.working_img_buf_iova,
			     ctx->img_buf_pool.working_img_buf_size,
			     DMA_BIDIRECTIONAL,
			     DMA_ATTR_SKIP_CPU_SYNC);
	dma_free_coherent(cam->smem_dev, ctx->img_buf_pool.working_img_buf_size,
			  ctx->img_buf_pool.working_img_buf_va,
			  ctx->img_buf_pool.working_img_buf_scp_addr);
	dev_dbg(dev,
		"%s:ctx(%d):img working buf release, scp addr %pad va %pK iova %pad, sz %u\n",
		__func__, ctx->stream_id,
		&ctx->img_buf_pool.working_img_buf_scp_addr,
		ctx->img_buf_pool.working_img_buf_va,
		&ctx->img_buf_pool.working_img_buf_iova,
		ctx->img_buf_pool.working_img_buf_size);
}

void mtk_cam_img_working_buf_put(struct mtk_cam_img_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->img_buf_pool.cam_freeimglist.list);
	cnt = ++ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->img_buffer.iova, cnt);
}

struct mtk_cam_img_working_buf_entry*
mtk_cam_img_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_img_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);
	if (list_empty(&ctx->img_buf_pool.cam_freeimglist.list)) {
		spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->img_buf_pool.cam_freeimglist.list,
				     struct mtk_cam_img_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->img_buffer.iova, cnt);

	return buf_entry;
}

