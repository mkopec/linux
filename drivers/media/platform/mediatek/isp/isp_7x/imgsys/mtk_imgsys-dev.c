// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#include "linux/list.h"
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/hashtable.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-event.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-formats.h"
#include "mtk_imgsys-vnode_id.h"

unsigned int nodes_num;

int mtk_imgsys_pipe_init(struct mtk_imgsys_dev *imgsys_dev,
			 struct mtk_imgsys_pipe *pipe,
			 const struct mtk_imgsys_pipe_desc *setting)
{
	int ret;
	u32 i;
	size_t nodes_size;

	pipe->imgsys_dev = imgsys_dev;
	pipe->desc = setting;
	pipe->nodes_enabled = 0ULL;
	pipe->nodes_streaming = 0ULL;

	atomic_set(&pipe->pipe_job_sequence, 0);
	INIT_LIST_HEAD(&pipe->pipe_job_running_list);
	INIT_LIST_HEAD(&pipe->pipe_job_pending_list);

	spin_lock_init(&pipe->pending_job_lock);
	spin_lock_init(&pipe->running_job_lock);
	mutex_init(&pipe->lock);

	nodes_num = pipe->desc->total_queues;
	nodes_size = sizeof(*pipe->nodes) * nodes_num;
	pipe->nodes = devm_kzalloc(imgsys_dev->dev, nodes_size, GFP_KERNEL);
	if (!pipe->nodes)
		return -ENOMEM;

	for (i = 0; i < nodes_num; i++) {
		pipe->nodes[i].desc = &pipe->desc->queue_descs[i];
		pipe->nodes[i].flags = pipe->nodes[i].desc->flags;
		spin_lock_init(&pipe->nodes[i].buf_list_lock);
		INIT_LIST_HEAD(&pipe->nodes[i].buf_list);

		pipe->nodes[i].crop.left = 0;
		pipe->nodes[i].crop.top = 0;
		pipe->nodes[i].crop.width =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_width;
		pipe->nodes[i].crop.height =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_height;
		pipe->nodes[i].compose.left = 0;
		pipe->nodes[i].compose.top = 0;
		pipe->nodes[i].compose.width =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_width;
		pipe->nodes[i].compose.height =
			pipe->nodes[i].desc->frmsizeenum->stepwise.max_height;
	}

	ret = mtk_imgsys_pipe_v4l2_register(pipe, &imgsys_dev->mdev,
					    &imgsys_dev->v4l2_dev);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "%s: failed(%d) to create V4L2 devices\n",
			 pipe->desc->name, ret);

		goto err_destroy_pipe_lock;
	}

	return 0;

err_destroy_pipe_lock:
	mutex_destroy(&pipe->lock);

	return ret;
}

int mtk_imgsys_pipe_release(struct mtk_imgsys_pipe *pipe)
{
	mtk_imgsys_pipe_v4l2_unregister(pipe);
	mutex_destroy(&pipe->lock);

	return 0;
}

int mtk_imgsys_pipe_next_job_id(struct mtk_imgsys_pipe *pipe)
{
	int global_job_id = atomic_inc_return(&pipe->pipe_job_sequence);

	return (global_job_id & 0x0000FFFF) | (pipe->desc->id << 16);
}

struct mtk_imgsys_request *
mtk_imgsys_pipe_get_running_job(struct mtk_imgsys_pipe *pipe, int id)
{
	struct mtk_imgsys_request *req;
	unsigned long flag;

	spin_lock_irqsave(&pipe->running_job_lock, flag);
	list_for_each_entry(req,
			    &pipe->pipe_job_running_list, list) {
		if (req->id == id) {
			spin_unlock_irqrestore(&pipe->running_job_lock, flag);
			return req;
		}
	}
	spin_unlock_irqrestore(&pipe->running_job_lock, flag);

	return NULL;
}

void mtk_imgsys_pipe_remove_job(struct mtk_imgsys_request *req)
{
	unsigned long flag;

	spin_lock_irqsave(&req->imgsys_pipe->running_job_lock, flag);
	list_del(&req->list);
	req->imgsys_pipe->num_jobs--;
	spin_unlock_irqrestore(&req->imgsys_pipe->running_job_lock, flag);

	dev_dbg(req->imgsys_pipe->imgsys_dev->dev,
		"%s:%s:req->id(%d),num of running jobs(%d) entry(%p)\n", __func__,
		req->imgsys_pipe->desc->name, req->id,
		req->imgsys_pipe->num_jobs, &req->list);
}

void mtk_imgsys_pipe_debug_job(struct mtk_imgsys_pipe *pipe,
			       struct mtk_imgsys_request *req)
{
	int i;

	dev_dbg(pipe->imgsys_dev->dev, "%s:%s: pipe-job(%p),id(%d)\n",
		__func__, pipe->desc->name, req, req->id);

	for (i = 0; i < pipe->desc->total_queues ; i++) {
		if (req->buf_map[i])
			dev_dbg(pipe->imgsys_dev->dev, "%s:%s:buf(%p)\n",
				pipe->desc->name, pipe->nodes[i].desc->name,
				req->buf_map[i]);
	}
}

void mtk_imgsys_pipe_job_finish(struct mtk_imgsys_request *req,
				enum vb2_buffer_state vbf_state)
{
	struct mtk_imgsys_pipe *pipe = req->imgsys_pipe;
	int i;

	if (req->req.state != MEDIA_REQUEST_STATE_QUEUED) {
		dev_info(pipe->imgsys_dev->dev, "%s: req %d 0x%lx flushed in state(%d)", __func__,
			 req->request_fd, (unsigned long)&req->req, req->req.state);
		return;
	}

	for (i = 0; i < pipe->desc->total_queues; i++) {
		struct mtk_imgsys_dev_buffer *dev_buf = req->buf_map[i];
		struct mtk_imgsys_video_device *node;

		if (!dev_buf)
			continue;

		node = mtk_imgsys_vbq_to_node(dev_buf->vbb.vb2_buf.vb2_queue);
		spin_lock(&node->buf_list_lock);
		list_del(&dev_buf->list);
		spin_unlock(&node->buf_list_lock);
		vb2_buffer_done(&dev_buf->vbb.vb2_buf, vbf_state);
	}
}

static unsigned int
mtk_imgsys_get_stride(unsigned int width,
		      const struct mtk_imgsys_dev_format *dfmt,
		      unsigned int plane)
{
	unsigned int stride =
		(width + dfmt->fmt->pixels_per_group - 1) /
		dfmt->fmt->pixels_per_group * dfmt->fmt->bytes_per_group[plane];

	return (stride + dfmt->align - 1) / dfmt->align * dfmt->align;
}

static unsigned int
mtk_imgsys_get_height(unsigned int height,
		      const struct mtk_imgsys_dev_format *dfmt,
		      unsigned int plane)
{
	unsigned int plane_height =
		(height + dfmt->fmt->vertical_sub_sampling[plane] - 1) /
		dfmt->fmt->vertical_sub_sampling[plane];

	return (plane_height + dfmt->scan_align - 1) / dfmt->scan_align * dfmt->scan_align;
}

void mtk_imgsys_pipe_try_fmt(struct mtk_imgsys_video_device *node,
			     struct v4l2_format *fmt,
			     const struct v4l2_format *ufmt,
			     const struct mtk_imgsys_dev_format *dfmt)
{
	unsigned int i;

	if (!dfmt)
		return;

	fmt->type = ufmt->type;
	fmt->fmt.pix_mp.width =
		clamp_val(ufmt->fmt.pix_mp.width,
			  node->desc->frmsizeenum->stepwise.min_width,
			  node->desc->frmsizeenum->stepwise.max_width);
	fmt->fmt.pix_mp.height =
		clamp_val(ufmt->fmt.pix_mp.height,
			  node->desc->frmsizeenum->stepwise.min_height,
			  node->desc->frmsizeenum->stepwise.max_height);
	fmt->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->fmt.pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	fmt->fmt.pix_mp.field = V4L2_FIELD_NONE;

	fmt->fmt.pix_mp.pixelformat = dfmt->fmt->format;
	fmt->fmt.pix_mp.num_planes = dfmt->fmt->num_planes;

	for (i = 0; i < fmt->fmt.pix_mp.num_planes; ++i) {
		struct v4l2_plane_pix_format *plane =
			&fmt->fmt.pix_mp.plane_fmt[i];
		const struct v4l2_plane_pix_format *uplane;

		uplane = &ufmt->fmt.pix_mp.plane_fmt[i];
		plane->bytesperline =
			mtk_imgsys_get_stride(fmt->fmt.pix_mp.width, dfmt, i);
		if (plane->bytesperline < uplane->bytesperline)
			plane->bytesperline = uplane->bytesperline;

		plane->sizeimage = plane->bytesperline *
			mtk_imgsys_get_height(fmt->fmt.pix_mp.height, dfmt, i);
	}
}

static void set_meta_fmt(struct mtk_imgsys_pipe *pipe,
			 struct v4l2_meta_format *fmt,
			 const struct mtk_imgsys_dev_format *dev_fmt)
{
	fmt->dataformat = dev_fmt->fmt->format;
	fmt->buffersize = dev_fmt->fmt->buffer_size;
}

void mtk_imgsys_pipe_load_default_fmt(struct mtk_imgsys_pipe *pipe,
				      struct mtk_imgsys_video_device *node,
				      struct v4l2_format *fmt)
{
	fmt->type = node->desc->buf_type;
	if (mtk_imgsys_buf_is_meta(node->desc->buf_type)) {
		set_meta_fmt(pipe, &fmt->fmt.meta,
			     &node->desc->fmts[0]);
	} else {
		fmt->fmt.pix_mp.width = node->desc->default_width;
		fmt->fmt.pix_mp.height = node->desc->default_height;
		mtk_imgsys_pipe_try_fmt(node, fmt, fmt,
					&node->desc->fmts[0]);
	}
}

const struct mtk_imgsys_dev_format*
mtk_imgsys_pipe_find_fmt(struct mtk_imgsys_pipe *pipe,
			 struct mtk_imgsys_video_device *node,
			 u32 format)
{
	int i;

	for (i = 0; i < node->desc->num_fmts; i++) {
		if (node->desc->fmts[i].fmt->format == format)
			return &node->desc->fmts[i];
	}

	return NULL;
}

void mtk_imgsys_pipe_try_enqueue(struct mtk_imgsys_pipe *pipe)
{
	struct mtk_imgsys_request *req = NULL;
	unsigned long flag;

	if (!pipe->streaming)
		return;

	spin_lock_irqsave(&pipe->pending_job_lock, flag);
	if (list_empty(&pipe->pipe_job_pending_list)) {
		spin_unlock_irqrestore(&pipe->pending_job_lock, flag);
		return;
	}

	req = list_first_entry(&pipe->pipe_job_pending_list, struct mtk_imgsys_request, list);
	list_del(&req->list);
	pipe->num_pending_jobs--;
	spin_unlock_irqrestore(&pipe->pending_job_lock, flag);

	spin_lock_irqsave(&pipe->running_job_lock, flag);
	list_add_tail(&req->list,
		      &pipe->pipe_job_running_list);
	pipe->num_jobs++;
	spin_unlock_irqrestore(&pipe->running_job_lock, flag);

	mtk_imgsys_hw_enqueue(pipe->imgsys_dev, req);
	dev_dbg(pipe->imgsys_dev->dev,
		"%s:%s: pending jobs(%d), running jobs(%d)\n",
		__func__, pipe->desc->name, pipe->num_pending_jobs,
		pipe->num_jobs);
}
