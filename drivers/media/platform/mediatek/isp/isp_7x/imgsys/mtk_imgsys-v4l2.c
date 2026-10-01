// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#include "linux/videodev2.h"
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include "linux/mtkisp_imgsys.h"
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <linux/videodev2.h>

#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>

#include "modules/mtk_imgsys-dip.h"
#include "modules/mtk_imgsys-traw.h"
#include "modules/mtk_imgsys-pqdip.h"
#include "modules/mtk_imgsys-wpe.h"
#include "modules/mtk_imgsys-adl.h"
#include "modules/mtk_imgsys-me.h"

#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hw.h"
#include "mtk_imgsys-vnode_id.h"
#include "mtk_imgsys-of.h"
#include "mtk_imgsys-module.h"
#include "mtk_imgsys-debug.h"
#include "mtk_imgsys_v4l2_vnode.h"
#include "mtk_imgsys-debug.h"

#define IMGSYS_MAX_BUFFERS	256

struct mtk_imgsys_larb_device {
	struct device	*dev;
	struct mtk_imgsys_dev *imgsys_dev;
};

static int mtk_imgsys_larb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_imgsys_larb_device   *larb_dev;
	int ret;

	larb_dev = devm_kzalloc(dev, sizeof(*dev), GFP_KERNEL);
	if (!larb_dev)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_err(dev, "No suitable DMA available\n");

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			dev_err(dev, "Failed to set DMA segment size\n");
	}

	pm_runtime_enable(dev);

	return 0;
}

static int mtk_imgsys_larb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);

	return 0;
}

static const struct of_device_id mtk_imgsys_larb_match[] = {
	{.compatible = "mediatek,imgsys-larb",},
	{},
};

static struct platform_driver mtk_imgsys_larb_driver = {
	.probe  = mtk_imgsys_larb_probe,
	.remove = mtk_imgsys_larb_remove,
	.driver = {
		.name   = "mtk-imgsys-larb",
		.of_match_table = of_match_ptr(mtk_imgsys_larb_match),
	},
};

static int mtk_imgsys_sd_subscribe_event(struct v4l2_subdev *subdev,
					 struct v4l2_fh *fh,
					 struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 64, NULL);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_subdev_core_ops mtk_imgsys_subdev_core_ops = {
	.subscribe_event = mtk_imgsys_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static int mtk_imgsys_subdev_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_imgsys_pipe *pipe = mtk_imgsys_subdev_to_pipe(sd);
	int ret;

	if (enable) {
		ret = mtk_imgsys_hw_streamon(pipe);
		if (ret)
			dev_err(pipe->imgsys_dev->dev,
				"%s: pipe(%d) streamon failed\n",
				pipe->desc->name, pipe->desc->id);
	} else {
		ret = mtk_imgsys_hw_streamoff(pipe);
		if (ret)
			dev_err(pipe->imgsys_dev->dev,
				"%s: pipe(%d) streamon off with errors\n",
				pipe->desc->name, pipe->desc->id);
	}

	return ret;
}

static const struct v4l2_subdev_video_ops mtk_imgsys_subdev_video_ops = {
	.s_stream = mtk_imgsys_subdev_s_stream,
};

static const struct v4l2_subdev_ops mtk_imgsys_subdev_ops = {
	.core = &mtk_imgsys_subdev_core_ops,
	.video = &mtk_imgsys_subdev_video_ops,
};

static int mtk_imgsys_link_setup(struct media_entity *entity,
				 const struct media_pad *local,
				 const struct media_pad *remote,
				 u32 flags)
{
	struct mtk_imgsys_pipe *pipe =
		container_of(entity, struct mtk_imgsys_pipe, subdev.entity);
	u32 pad = local->index;

	WARN_ON(entity->obj_type != MEDIA_ENTITY_TYPE_V4L2_SUBDEV);
	WARN_ON(pad >= pipe->desc->total_queues);

	mutex_lock(&pipe->lock);

	if (flags & MEDIA_LNK_FL_ENABLED)
		pipe->nodes_enabled++;
	else
		pipe->nodes_enabled--;

	pipe->nodes[pad].flags &= ~MEDIA_LNK_FL_ENABLED;
	pipe->nodes[pad].flags |= flags & MEDIA_LNK_FL_ENABLED;

	dev_dbg(pipe->imgsys_dev->dev,
		"%s: link setup, flags(0x%x), (%s)%d -->(%s)%d, nodes_enabled(0x%llx)\n",
		pipe->desc->name, flags, local->entity->name, local->index,
		remote->entity->name, remote->index, pipe->nodes_enabled);

	mutex_unlock(&pipe->lock);

	return 0;
}

static const struct media_entity_operations mtk_imgsys_media_ops = {
	.link_setup = mtk_imgsys_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static int mtk_imgsys_vb2_meta_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_imgsys_video_device *node =
					mtk_imgsys_vbq_to_node(vb->vb2_queue);
	struct device *dev = pipe->imgsys_dev->dev;
	const struct v4l2_format *fmt = &node->vdev_fmt;

	if (vb->planes[0].length < fmt->fmt.meta.buffersize) {
		dev_err(dev,
			"%s:%s: size error(user:%d, required:%d)\n",
			pipe->desc->name, node->desc->name,
			vb->planes[0].length, fmt->fmt.meta.buffersize);
		return -EINVAL;
	}

	return 0;
}

static int mtk_imgsys_vb2_video_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_imgsys_video_device *node =
					mtk_imgsys_vbq_to_node(vb->vb2_queue);
	struct device *dev = pipe->imgsys_dev->dev;
	const struct v4l2_format *fmt = &node->vdev_fmt;
	unsigned int size;
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		size = fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		if (vb->planes[i].length < size) {
			dev_err(dev,
				"%s:%s: size error(user:%d, required:%d)\n",
				pipe->desc->name, node->desc->name,
				vb->planes[i].length, size);
			return -EINVAL;
		}
	}

	return 0;
}

static int mtk_imgsys_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;

	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_imgsys_vb2_meta_buf_init(struct vb2_buffer *vb)
{
	struct mtk_imgsys_dev_buffer *dev_buf =
					mtk_imgsys_vb2_buf_to_dev_buf(vb);
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_imgsys_video_device *node =
					mtk_imgsys_vbq_to_node(vb->vb2_queue);

	/*
	 * The meta buffers are allocated from the SCP reserved memory.
	 * Setup the mapping to imgsys device here.
	 */
	dev_buf->scp_daddr[0] = vb2_dma_contig_plane_dma_addr(vb, 0);
	dev_buf->isp_daddr[0] =
		dma_map_resource(pipe->imgsys_dev->dev,
				 dev_buf->scp_daddr[0],
				 vb->planes[0].length,
				 DMA_BIDIRECTIONAL,
				 DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(pipe->imgsys_dev->dev,
				dev_buf->isp_daddr[0])) {
		dev_err(pipe->imgsys_dev->dev,
			"%s:%s: failed to map buffer: s_daddr(%pad)\n",
			pipe->desc->name, node->desc->name,
			&dev_buf->scp_daddr[0]);
		return -EINVAL;
	}

	return 0;
}

static int mtk_imgsys_vb2_video_buf_init(struct vb2_buffer *vb)
{
	struct mtk_imgsys_dev_buffer *dev_buf =
					mtk_imgsys_vb2_buf_to_dev_buf(vb);
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane)
		dev_buf->isp_daddr[plane] = vb2_dma_contig_plane_dma_addr(vb, plane);

	return 0;
}

static void mtk_imgsys_vb2_queue_meta_buf_cleanup(struct vb2_buffer *vb)
{
	struct mtk_imgsys_dev_buffer *dev_buf =
					mtk_imgsys_vb2_buf_to_dev_buf(vb);
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vb->vb2_queue);

	dma_unmap_resource(pipe->imgsys_dev->dev,
			   dev_buf->isp_daddr[0],
			   vb->planes[0].length, DMA_BIDIRECTIONAL,
			   DMA_ATTR_SKIP_CPU_SYNC);
}

static void mtk_imgsys_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *b = to_vb2_v4l2_buffer(vb);
	struct mtk_imgsys_dev_buffer *dev_buf =
					mtk_imgsys_vb2_buf_to_dev_buf(vb);
	struct mtk_imgsys_request *req = NULL;
	struct mtk_imgsys_video_device *node =
					mtk_imgsys_vbq_to_node(vb->vb2_queue);
	int buf_count;

	if (!vb->request)
		return;

	req = mtk_imgsys_media_req_to_imgsys_req(vb->request);

	dev_buf->dev_fmt = node->dev_q.dev_fmt;
	spin_lock(&node->buf_list_lock);
	list_add_tail(&dev_buf->list, &node->buf_list);
	spin_unlock(&node->buf_list_lock);

	buf_count = atomic_dec_return(&req->buf_count);
	if (!buf_count) {
		dev_dbg(&node->vdev.dev,
			"framo_no: (%d), reqfd-%d\n",
			req->img_fparam.frameparam.frame_no, b->request_fd);
		req->request_fd = b->request_fd;
		mutex_lock(&req->imgsys_pipe->lock);
		mtk_imgsys_pipe_try_enqueue(req->imgsys_pipe);
		mutex_unlock(&req->imgsys_pipe->lock);
	}
}

static int mtk_imgsys_vb2_meta_queue_setup(struct vb2_queue *vq,
					   unsigned int *num_buffers,
					   unsigned int *num_planes,
					   unsigned int sizes[],
					   struct device *alloc_devs[])
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_vbq_to_node(vq);
	const struct v4l2_format *fmt = &node->vdev_fmt;

	if (*num_planes)
		return 0;

	*num_planes = 1;
	sizes[0] = fmt->fmt.meta.buffersize;

	return 0;
}

static int mtk_imgsys_vb2_video_queue_setup(struct vb2_queue *vq,
					    unsigned int *num_buffers,
					    unsigned int *num_planes,
					    unsigned int sizes[],
					    struct device *alloc_devs[])
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_vbq_to_node(vq);
	const struct v4l2_format *fmt = &node->vdev_fmt;
	int i;

	if (*num_planes)
		return 0;

	*num_planes = fmt->fmt.pix_mp.num_planes;

	for (i = 0; i < *num_planes; i++) {
		sizes[i] = fmt->fmt.pix_mp.plane_fmt[i].sizeimage;
		*num_buffers = clamp_val(*num_buffers, 1, IMGSYS_MAX_BUFFERS);
	}
	return 0;
}

static void mtk_imgsys_return_all_buffers(struct mtk_imgsys_pipe *pipe,
					  struct mtk_imgsys_video_device *node,
					  enum vb2_buffer_state state)
{
	struct mtk_imgsys_dev_buffer *b, *b0;

	spin_lock(&node->buf_list_lock);
	list_for_each_entry_safe(b, b0, &node->buf_list, list) {
		list_del(&b->list);
		vb2_buffer_done(&b->vbb.vb2_buf, state);
	}
	spin_unlock(&node->buf_list_lock);
}

static int mtk_imgsys_vb2_start_streaming(struct vb2_queue *vq,
					  unsigned int count)
{
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vq);
	struct mtk_imgsys_video_device *node = mtk_imgsys_vbq_to_node(vq);
	int ret;

	if (!pipe->nodes_streaming) {
		ret = media_pipeline_start(&node->vdev.entity.pads[0], &pipe->pipeline);
		if (ret < 0) {
			dev_info(pipe->imgsys_dev->dev,
				 "%s:%s: media_pipeline_start failed(%d)\n",
				 pipe->desc->name, node->desc->name, ret);
			goto fail_return_bufs;
		}
	}

	mutex_lock(&pipe->lock);
	if (!(node->flags & MEDIA_LNK_FL_ENABLED)) {
		dev_info(pipe->imgsys_dev->dev,
			 "%s:%s: stream on failed, node is not enabled\n",
			 pipe->desc->name, node->desc->name);

		ret = -ENOLINK;
		goto fail_stop_pipeline;
	}

	pipe->nodes_streaming++;
	if (pipe->nodes_streaming == pipe->nodes_enabled) {
		/* Start streaming of the whole pipeline */
		ret = v4l2_subdev_call(&pipe->subdev, video, s_stream, 1);
		if (ret < 0) {
			dev_info(pipe->imgsys_dev->dev,
				 "%s:%s: sub dev s_stream(1) failed(%d)\n",
				 pipe->desc->name, node->desc->name, ret);

			goto fail_stop_pipeline;
		}
	}

	dev_dbg(pipe->imgsys_dev->dev,
		"%s:%s:%s nodes_streaming(0x%llx), nodes_enable(0x%llx)\n",
		__func__, pipe->desc->name, node->desc->name,
		pipe->nodes_streaming, pipe->nodes_enabled);

	mutex_unlock(&pipe->lock);

	return 0;

fail_stop_pipeline:
	mutex_unlock(&pipe->lock);
	media_pipeline_stop(&node->vdev.entity.pads[0]);

fail_return_bufs:
	mtk_imgsys_return_all_buffers(pipe, node, VB2_BUF_STATE_QUEUED);

	return ret;
}

static void mtk_imgsys_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_imgsys_pipe *pipe = vb2_get_drv_priv(vq);
	struct mtk_imgsys_video_device *node = mtk_imgsys_vbq_to_node(vq);
	int ret;

	mutex_lock(&pipe->lock);

	if (pipe->streaming) {
		ret = v4l2_subdev_call(&pipe->subdev, video, s_stream, 0);
		if (ret)
			dev_info(pipe->imgsys_dev->dev,
				 "%s:%s: sub dev s_stream(0) failed(%d)\n",
				 pipe->desc->name, node->desc->name, ret);
	}

	pipe->nodes_streaming--;

	if (!pipe->nodes_streaming)
		media_pipeline_stop(&node->vdev.entity.pads[0]);

	mtk_imgsys_return_all_buffers(pipe, node, VB2_BUF_STATE_ERROR);

	mutex_unlock(&pipe->lock);
}

static void mtk_imgsys_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_imgsys_video_device *node =
					mtk_imgsys_vbq_to_node(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req,
				   &node->ctrl_handler);
}

static const struct vb2_ops mtk_imgsys_vb2_meta_ops = {
	.buf_queue = mtk_imgsys_vb2_buf_queue,
	.queue_setup = mtk_imgsys_vb2_meta_queue_setup,
	.buf_init = mtk_imgsys_vb2_meta_buf_init,
	.buf_prepare  = mtk_imgsys_vb2_meta_buf_prepare,
	.buf_out_validate = mtk_imgsys_vb2_buf_out_validate,
	.buf_cleanup = mtk_imgsys_vb2_queue_meta_buf_cleanup,
	.start_streaming = mtk_imgsys_vb2_start_streaming,
	.stop_streaming = mtk_imgsys_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_imgsys_vb2_request_complete,
};

static const struct vb2_ops mtk_imgsys_vb2_video_ops = {
	.buf_queue = mtk_imgsys_vb2_buf_queue,
	.queue_setup = mtk_imgsys_vb2_video_queue_setup,
	.buf_init = mtk_imgsys_vb2_video_buf_init,
	.buf_prepare  = mtk_imgsys_vb2_video_buf_prepare,
	.buf_out_validate = mtk_imgsys_vb2_buf_out_validate,
	.start_streaming = mtk_imgsys_vb2_start_streaming,
	.stop_streaming = mtk_imgsys_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_imgsys_vb2_request_complete,
};

static int mtk_imgsys_video_device_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_imgsys_video_device *node =
		container_of(ctrl->handler,
			     struct mtk_imgsys_video_device, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_ROTATE:
		node->rotation = ctrl->val;
		break;
	case V4L2_CID_VFLIP:
		node->vflip = ctrl->val;
		break;
	case V4L2_CID_HFLIP:
		node->hflip = ctrl->val;
		break;
	case V4L2_CID_MTK_IMG_RESIZE_RATIO:
		node->resize_ratio = ctrl->val;
		break;
	default:
		dev_dbg(&node->vdev.dev, "[%s] doesn't support ctrl(%d)\n",
			node->desc->name, ctrl->id);
		return -EINVAL;
	}

	node->rotation = ctrl->val;

	return 0;
}

static const struct v4l2_ctrl_ops mtk_imgsys_video_device_ctrl_ops = {
	.s_ctrl = mtk_imgsys_video_device_s_ctrl,
};

static int mtk_imgsys_vidioc_qbuf(struct file *file, void *priv,
				  struct v4l2_buffer *buf)
{
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	struct vb2_buffer *vb = node->dev_q.vbq.bufs[buf->index];
	struct mtk_imgsys_dev_buffer *dev_buf =
					mtk_imgsys_vb2_buf_to_dev_buf(vb);

	if (!dev_buf)
		return -EFAULT;

	dev_buf->fmt = node->vdev_fmt;
	dev_buf->rotation = node->rotation;
	dev_buf->crop.c = node->crop;
	dev_buf->compose = node->compose;
	dev_buf->hflip = node->hflip;
	dev_buf->vflip = node->vflip;
	dev_buf->resize_ratio = node->resize_ratio;

	return vb2_qbuf(node->vdev.queue, &pipe->imgsys_dev->mdev, buf);
}

static int mtk_imgsys_videoc_querycap(struct file *file, void *fh,
				      struct v4l2_capability *cap)
{
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);

	strscpy(cap->driver, pipe->desc->name, sizeof(cap->driver));
	strscpy(cap->card, pipe->desc->name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(pipe->imgsys_dev->mdev.dev));

	return 0;
}

static int mtk_imgsys_videoc_try_fmt(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	const struct mtk_imgsys_dev_format *dev_fmt;
	struct v4l2_format try_fmt;

	memset(&try_fmt, 0, sizeof(try_fmt));

	dev_fmt = mtk_imgsys_pipe_find_fmt(pipe, node,
					   f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_fmt = &node->desc->fmts[0];
		dev_dbg(pipe->imgsys_dev->dev,
			"%s:%s:%s: dev_fmt(%d) not found, use default(%d)\n",
			__func__, pipe->desc->name, node->desc->name,
			f->fmt.pix_mp.pixelformat, dev_fmt->fmt->format);
	}

	mtk_imgsys_pipe_try_fmt(node, &try_fmt, f, dev_fmt);
	*f = try_fmt;

	return 0;
}

static int mtk_imgsys_videoc_g_fmt(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);

	*f = node->vdev_fmt;

	return 0;
}

static int mtk_imgsys_videoc_s_fmt(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);
	const struct mtk_imgsys_dev_format *dev_fmt;

	dev_fmt = mtk_imgsys_pipe_find_fmt(pipe, node,
					   f->fmt.pix_mp.pixelformat);
	if (!dev_fmt) {
		dev_fmt = &node->desc->fmts[0];
		dev_info(pipe->imgsys_dev->dev,
			 "%s:%s:%s: dev_fmt(%d) not found, use default(%d)\n",
			 __func__, pipe->desc->name, node->desc->name,
			 f->fmt.pix_mp.pixelformat, dev_fmt->fmt->format);
	}

	memset(&node->vdev_fmt, 0, sizeof(node->vdev_fmt));

	mtk_imgsys_pipe_try_fmt(node, &node->vdev_fmt, f, dev_fmt);
	*f = node->vdev_fmt;

	node->dev_q.dev_fmt = dev_fmt;
	node->crop.left = 0; /* reset crop setting of nodes */
	node->crop.top = 0;
	node->crop.width = f->fmt.pix_mp.width;
	node->crop.height = f->fmt.pix_mp.height;
	node->compose.left = 0;
	node->compose.top = 0;
	node->compose.width = f->fmt.pix_mp.width;
	node->compose.height = f->fmt.pix_mp.height;
	if (node->resize_ratio_ctrl)
		v4l2_ctrl_s_ctrl(node->resize_ratio_ctrl, 0);

	return 0;
}

static int mtk_imgsys_videoc_enum_framesizes(struct file *file, void *priv,
					     struct v4l2_frmsizeenum *sizes)
{
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	const struct mtk_imgsys_dev_format *dev_fmt;

	dev_fmt = mtk_imgsys_pipe_find_fmt(pipe, node, sizes->pixel_format);

	if (!dev_fmt || sizes->index)
		return -EINVAL;

	sizes->type = node->desc->frmsizeenum->type;
	sizes->stepwise.max_width =
		node->desc->frmsizeenum->stepwise.max_width;
	sizes->stepwise.min_width =
		node->desc->frmsizeenum->stepwise.min_width;
	sizes->stepwise.max_height =
		node->desc->frmsizeenum->stepwise.max_height;
	sizes->stepwise.min_height =
		node->desc->frmsizeenum->stepwise.min_height;
	sizes->stepwise.step_height =
		node->desc->frmsizeenum->stepwise.step_height;
	sizes->stepwise.step_width =
		node->desc->frmsizeenum->stepwise.step_width;

	return 0;
}

static int mtk_imgsys_videoc_enum_fmt(struct file *file, void *fh,
				      struct v4l2_fmtdesc *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);

	if (f->index >= node->desc->num_fmts)
		return -EINVAL;

	strscpy(f->description, node->desc->description, sizeof(f->description));
	f->pixelformat = node->desc->fmts[f->index].fmt->format;
	f->flags = 0;

	return 0;
}

static int mtk_imgsys_meta_enum_format(struct file *file, void *fh,
				       struct v4l2_fmtdesc *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);

	if (f->index > 0)
		return -EINVAL;

	strscpy(f->description, node->desc->description, sizeof(f->description));

	f->pixelformat = node->vdev_fmt.fmt.meta.dataformat;
	f->flags = 0;

	return 0;
}

static int mtk_imgsys_videoc_g_meta_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	*f = node->vdev_fmt;

	return 0;
}

static int mtk_imgsys_videoc_s_meta_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);
	struct mtk_imgsys_pipe *pipe = video_drvdata(file);
	const struct mtk_imgsys_dev_format *dev_fmt;

	if (pipe->streaming || vb2_is_busy(&node->dev_q.vbq))
		return -EBUSY;

	dev_fmt = mtk_imgsys_pipe_find_fmt(pipe, node,
					   f->fmt.meta.dataformat);
	if (!dev_fmt) {
		dev_fmt = &node->desc->fmts[0];
		dev_info(pipe->imgsys_dev->dev,
			 "%s:%s:%s: dev_fmt(%d) not found, use default(%d)\n",
			 __func__, pipe->desc->name, node->desc->name,
			 f->fmt.meta.dataformat, dev_fmt->fmt->format);
	}

	memset(&node->vdev_fmt, 0, sizeof(node->vdev_fmt));

	f->fmt.meta.dataformat = dev_fmt->fmt->format;
	f->fmt.meta.buffersize = dev_fmt->fmt->buffer_size;

	node->dev_q.dev_fmt = dev_fmt;
	node->vdev_fmt = *f;

	return 0;
}

static int mtk_imgsys_g_selection(struct file *file, void *priv,
				  struct v4l2_selection *s)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);

	if (!node)
		return -EINVAL;

	if ((node->desc->buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
	     s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) ||
	    (node->desc->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	     s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		s->r = node->crop;
		break;
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.width = node->desc->default_width;
		s->r.height = node->desc->default_width;
		s->r.left = 0;
		s->r.top = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_imgsys_s_selection(struct file *file, void *priv,
				  struct v4l2_selection *s)
{
	struct mtk_imgsys_video_device *node = mtk_imgsys_file_to_node(file);

	if (!node)
		return -EINVAL;

	if ((node->desc->buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
	     s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) ||
	    (node->desc->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	     s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE))
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		node->crop = s->r;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_framesizes = mtk_imgsys_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_out = mtk_imgsys_videoc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_imgsys_videoc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_imgsys_videoc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_imgsys_videoc_try_fmt,

	.vidioc_g_selection = mtk_imgsys_g_selection,
	.vidioc_s_selection = mtk_imgsys_s_selection,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_remove_bufs = vb2_ioctl_remove_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,

	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_video_cap_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_framesizes = mtk_imgsys_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_imgsys_videoc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_imgsys_videoc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_imgsys_videoc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_imgsys_videoc_try_fmt,

	.vidioc_g_selection = mtk_imgsys_g_selection,
	.vidioc_s_selection = mtk_imgsys_s_selection,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_remove_bufs = vb2_ioctl_remove_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,

	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_fmt_meta_out = mtk_imgsys_meta_enum_format,
	.vidioc_g_fmt_meta_out = mtk_imgsys_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_imgsys_videoc_s_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_imgsys_videoc_g_meta_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_remove_bufs = vb2_ioctl_remove_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_fmt_meta_cap = mtk_imgsys_meta_enum_format,
	.vidioc_g_fmt_meta_cap = mtk_imgsys_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_imgsys_videoc_s_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_imgsys_videoc_g_meta_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_remove_bufs = vb2_ioctl_remove_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static struct media_request *mtk_imgsys_request_alloc(struct media_device *mdev)
{
	struct mtk_imgsys_request *req;
	struct mtk_imgsys_pipe *pipe;

	pipe = &mtk_imgsys_mdev_to_dev(mdev)->imgsys_pipe[0];
	req = vzalloc(sizeof(*req));
	if (!req)
		return NULL;

	req->buf_map = vzalloc(pipe->desc->total_queues *
			       sizeof(struct mtk_imgsys_dev_buffer *));
	if (!req->buf_map)
		goto error;

	return &req->req;

error:
	vfree(req);
	return NULL;
}

static void mtk_imgsys_request_free(struct media_request *req)
{
	struct mtk_imgsys_request *imgsys_req =
					mtk_imgsys_media_req_to_imgsys_req(req);

	vfree(imgsys_req->buf_map);
	vfree(imgsys_req);
}

static int mtk_imgsys_vb2_request_validate(struct media_request *req)
{
	struct media_request_object *obj;
	struct mtk_imgsys_dev *imgsys_dev = mtk_imgsys_mdev_to_dev(req->mdev);
	struct mtk_imgsys_request *imgsys_req =
					mtk_imgsys_media_req_to_imgsys_req(req);
	struct mtk_imgsys_pipe *pipe = NULL;
	struct mtk_imgsys_pipe *pipe_prev = NULL;
	struct mtk_imgsys_dev_buffer **buf_map = imgsys_req->buf_map;
	int buf_count = 0;
	int i;

	for (i = 0; i < imgsys_dev->imgsys_pipe[0].desc->total_queues; i++)
		buf_map[i] = NULL;

	list_for_each_entry(obj, &req->objects, list) {
		struct vb2_buffer *vb;
		struct mtk_imgsys_dev_buffer *dev_buf;
		struct mtk_imgsys_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;

		vb = container_of(obj, struct vb2_buffer, req_obj);
		node = mtk_imgsys_vbq_to_node(vb->vb2_queue);
		pipe = vb2_get_drv_priv(vb->vb2_queue);
		if (pipe_prev && pipe != pipe_prev) {
			dev_dbg(imgsys_dev->dev,
				"%s:%s:%s:found buf of different pipes(%p,%p)\n",
				__func__, node->desc->name,
				req->debug_str, pipe, pipe_prev);
			return -EINVAL;
		}

		pipe_prev = pipe;
		dev_buf = mtk_imgsys_vb2_buf_to_dev_buf(vb);
		imgsys_req->buf_map[node->desc->id] = dev_buf;
		buf_count++;

		dev_dbg(pipe->imgsys_dev->dev,
			"%s:%s: added buf(%p) to pipe-job(%p), buf_count(%d)\n",
			pipe->desc->name, node->desc->name, dev_buf,
			imgsys_req, buf_count);
	}

	if (!pipe) {
		dev_dbg(imgsys_dev->dev,
			"%s: no buffer in the request(%p)\n",
			req->debug_str, req);

		return -ENOENT;
	}

	atomic_set(&imgsys_req->buf_count, buf_count);
	imgsys_req->id = mtk_imgsys_pipe_next_job_id(pipe);
	imgsys_req->imgsys_pipe = pipe;
	mtk_imgsys_pipe_debug_job(pipe, imgsys_req);

	return vb2_request_validate(req);
}

static void mtk_imgsys_vb2_request_queue(struct media_request *req)
{
	struct mtk_imgsys_request *imgsys_req =
					mtk_imgsys_media_req_to_imgsys_req(req);
	struct mtk_imgsys_pipe *pipe = imgsys_req->imgsys_pipe;
	unsigned long flag;

	spin_lock_irqsave(&pipe->pending_job_lock, flag);
	list_add_tail(&imgsys_req->list, &pipe->pipe_job_pending_list);
	pipe->num_pending_jobs++;
	dev_dbg(req->mdev->dev,
		"%s:%s: current num of pending jobs(%d)\n",
		__func__, pipe->desc->name, pipe->num_pending_jobs);
	spin_unlock_irqrestore(&pipe->pending_job_lock, flag);
	vb2_request_queue(req);
}

static const struct media_device_ops mtk_imgsys_media_req_ops = {
	.req_validate = mtk_imgsys_vb2_request_validate,
	.req_queue = mtk_imgsys_vb2_request_queue,
	.req_alloc = mtk_imgsys_request_alloc,
	.req_free = mtk_imgsys_request_free,
};

static const struct v4l2_file_operations mtk_imgsys_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

static int mtk_imgsys_dev_media_register(struct device *dev,
					 struct media_device *media_dev)
{
	int ret;

	media_dev->dev = dev;
	strscpy(media_dev->model, MTK_DIP_DEV_DIP_MEDIA_MODEL_NAME, sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_dev->ops = &mtk_imgsys_media_req_ops;
	media_device_init(media_dev);

	ret = media_device_register(media_dev);
	if (ret)
		media_device_cleanup(media_dev);

	return ret;
}

static int mtk_imgsys_video_device_v4l2_register(struct mtk_imgsys_pipe *pipe,
						 struct mtk_imgsys_video_device *node)
{
	struct vb2_queue *vbq = &node->dev_q.vbq;
	struct video_device *vdev = &node->vdev;
	struct media_link *link;
	int ret;

	switch (node->desc->buf_type) {
	case V4L2_BUF_TYPE_META_OUTPUT:
		vbq->ops = &mtk_imgsys_vb2_meta_ops;
		vdev->ioctl_ops = &mtk_imgsys_v4l2_meta_out_ioctl_ops;
		break;
	case V4L2_BUF_TYPE_META_CAPTURE:
		vbq->ops = &mtk_imgsys_vb2_meta_ops;
		vdev->ioctl_ops = &mtk_imgsys_v4l2_meta_cap_ioctl_ops;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vbq->ops = &mtk_imgsys_vb2_video_ops;
		vdev->ioctl_ops = &mtk_imgsys_v4l2_video_out_ioctl_ops;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vbq->ops = &mtk_imgsys_vb2_video_ops;
		vdev->ioctl_ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops;
		break;
	default:
		dev_info(pipe->imgsys_dev->dev,
			 "unexpected buf_type %u\n", node->desc->buf_type);
		return -EFAULT;
	}

	mutex_init(&node->dev_q.lock);
	vdev->device_caps = node->desc->cap;
	node->vdev_fmt.type = node->desc->buf_type;
	mtk_imgsys_pipe_load_default_fmt(pipe, node, &node->vdev_fmt);

	ret = media_entity_pads_init(&vdev->entity, 1, &node->vdev_pad);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "failed initialize media entity (%d)\n", ret);
		goto err_mutex_destroy;
	}

	node->vdev_pad.flags = V4L2_TYPE_IS_OUTPUT(node->desc->buf_type) ?
		MEDIA_PAD_FL_SOURCE : MEDIA_PAD_FL_SINK;

	vbq->type = node->vdev_fmt.type;
	vbq->io_modes = VB2_MMAP | VB2_DMABUF;
	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->supports_requests = true;
	vbq->requires_requests = true;
	vbq->buf_struct_size = sizeof(struct mtk_imgsys_dev_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	vbq->min_buffers_needed = 0;
	vbq->drv_priv = pipe;
	vbq->lock = &node->dev_q.lock;
	vbq->max_num_buffers = IMGSYS_MAX_BUFFERS;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "%s:%s:%s: failed to init vb2 queue(%d)\n",
			 __func__, pipe->desc->name, node->desc->name,
			 ret);
		goto err_media_entity_cleanup;
	}

	strscpy(vbq->name, node->desc->name, sizeof(vbq->name));

	snprintf(vdev->name, sizeof(vdev->name), "%s %s", pipe->desc->name,
		 node->desc->name);
	vdev->entity.name = vdev->name;
	vdev->entity.function = MEDIA_ENT_F_IO_V4L;
	vdev->entity.ops = NULL;
	vdev->release = video_device_release_empty;
	vdev->fops = &mtk_imgsys_v4l2_fops;
	vdev->lock = &node->dev_q.lock;
	if (node->desc->supports_ctrls)
		vdev->ctrl_handler = &node->ctrl_handler;
	else
		vdev->ctrl_handler = NULL;
	vdev->v4l2_dev = &pipe->imgsys_dev->v4l2_dev;
	vdev->queue = &node->dev_q.vbq;
	vdev->vfl_dir = V4L2_TYPE_IS_OUTPUT(node->desc->buf_type) ?
		VFL_DIR_TX : VFL_DIR_RX;

	if (node->desc->smem_alloc) {
		vdev->queue->dev = pipe->imgsys_dev->smem_dev;
		dev_dbg(pipe->imgsys_dev->dev,
			"%s:%s: select smem_vb2_alloc_ctx(%p)\n",
			pipe->desc->name, node->desc->name,
			vdev->queue->dev);
	} else {
		vdev->queue->dev = pipe->imgsys_dev->dev;
		dev_dbg(pipe->imgsys_dev->dev,
			"%s:%s: select default_vb2_alloc_ctx(%p)\n",
			pipe->desc->name, node->desc->name,
			pipe->imgsys_dev->dev);
	}

	video_set_drvdata(vdev, pipe);

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "failed to register video device (%d)\n", ret);
		goto err_vb2_queue_release;
	}
	dev_dbg(pipe->imgsys_dev->dev, "registered vdev: %s\n",
		vdev->name);

	if (V4L2_TYPE_IS_OUTPUT(node->desc->buf_type))
		ret = media_create_pad_link(&vdev->entity, 0,
					    &pipe->subdev.entity,
					    node->desc->id, node->flags);
	else
		ret = media_create_pad_link(&pipe->subdev.entity,
					    node->desc->id, &vdev->entity,
					    0, node->flags);
	if (ret)
		goto err_video_unregister_device;

	vdev->intf_devnode = media_devnode_create(&pipe->imgsys_dev->mdev,
						  MEDIA_INTF_T_V4L_VIDEO, 0,
						  VIDEO_MAJOR, vdev->minor);
	if (!vdev->intf_devnode) {
		ret = -ENOMEM;
		goto err_rm_links;
	}

	link = media_create_intf_link(&vdev->entity,
				      &vdev->intf_devnode->intf,
				      node->flags);
	if (!link) {
		ret = -ENOMEM;
		goto err_rm_devnode;
	}

	return 0;

err_rm_devnode:
	media_devnode_remove(vdev->intf_devnode);

err_rm_links:
	media_entity_remove_links(&vdev->entity);

err_video_unregister_device:
	video_unregister_device(vdev);

err_vb2_queue_release:
	vb2_queue_release(&node->dev_q.vbq);

err_media_entity_cleanup:
	media_entity_cleanup(&node->vdev.entity);

err_mutex_destroy:
	mutex_destroy(&node->dev_q.lock);

	return ret;
}

/******************************************************************************
 * @array img_resize_ratio_menu
 * @brief IMGSYS hardware supports multiple resizers.
 *        - Any Ratio: Allows for any resize ratio, including
 *          down 2, down 4, and down 42.
 *        - Down 4: Specifically for downscaling by a factor of 4.
 *        - Down 2: Specifically for downscaling by a factor of 2.
 *        - Down42: Used for downscaling by a factor of 4 on the
 *          first use, and by a factor of 2 on subsequent uses.
 *        - NULL: Indicates the end of the menu options
 ******************************************************************************/
static const char *img_resize_ratio_menu[] = {
	"Any Ratio",
	"Down 4",
	"Down 2",
	"Down 42",
	NULL,
};

static const struct v4l2_ctrl_config cfg_mtk_resize_ratio = {
	.ops = &mtk_imgsys_video_device_ctrl_ops,
	.id = V4L2_CID_MTK_IMG_RESIZE_RATIO,
	.name = "Image resize ratio",
	.type = V4L2_CTRL_TYPE_MENU,
	.max = 3,
	.def = 0,
	.qmenu = img_resize_ratio_menu,
};

static int mtk_imgsys_pipe_v4l2_ctrl_init(struct mtk_imgsys_pipe *imgsys_pipe)
{
	int i, ret;
	struct mtk_imgsys_video_device *ctrl_node;

	for (i = 0; i < MTK_IMGSYS_VIDEO_NODE_ID_TOTAL_NUM; i++) {
		ctrl_node = &imgsys_pipe->nodes[i];
		if (!ctrl_node->desc->supports_ctrls)
			continue;

		v4l2_ctrl_handler_init(&ctrl_node->ctrl_handler, 4);
		v4l2_ctrl_new_std(&ctrl_node->ctrl_handler,
				  &mtk_imgsys_video_device_ctrl_ops, V4L2_CID_ROTATE,
				  0, 270, 90, 0);
		v4l2_ctrl_new_std(&ctrl_node->ctrl_handler,
				  &mtk_imgsys_video_device_ctrl_ops, V4L2_CID_VFLIP,
				  0, 1, 1, 0);
		v4l2_ctrl_new_std(&ctrl_node->ctrl_handler,
				  &mtk_imgsys_video_device_ctrl_ops, V4L2_CID_HFLIP,
				  0, 1, 1, 0);
		ctrl_node->resize_ratio_ctrl =
			v4l2_ctrl_new_custom(&ctrl_node->ctrl_handler,
					     &cfg_mtk_resize_ratio, NULL);
		ret = ctrl_node->ctrl_handler.error;
		if (ret) {
			dev_info(imgsys_pipe->imgsys_dev->dev,
				 "%s create rotate ctrl failed:(%d)",
				 ctrl_node->desc->name, ret);
			goto err_free_ctrl_handlers;
		}
	}

	return 0;

err_free_ctrl_handlers:
	for (; i >= 0; i--) {
		ctrl_node = &imgsys_pipe->nodes[i];
		if (!ctrl_node->desc->supports_ctrls)
			continue;
		v4l2_ctrl_handler_free(&ctrl_node->ctrl_handler);
	}

	return ret;
}

static void mtk_imgsys_pipe_v4l2_ctrl_release(struct mtk_imgsys_pipe *imgsys_pipe)
{
	struct mtk_imgsys_video_device *vdev;
	int i;

	for (i = 0; i < imgsys_pipe->desc->total_queues; ++i) {
		vdev = &imgsys_pipe->nodes[i];
		if (vdev->desc->supports_ctrls)
			v4l2_ctrl_handler_free(&vdev->ctrl_handler);
	}
}

int mtk_imgsys_pipe_v4l2_register(struct mtk_imgsys_pipe *pipe,
				  struct media_device *media_dev,
				  struct v4l2_device *v4l2_dev)
{
	int i, ret;

	ret = mtk_imgsys_pipe_v4l2_ctrl_init(pipe);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "%s: failed(%d) to initialize ctrls\n",
			 pipe->desc->name, ret);

		return ret;
	}

	pipe->streaming = 0;

	/* Initialize subdev media entity */
	pipe->subdev_pads = devm_kcalloc(pipe->imgsys_dev->dev,
					 pipe->desc->total_queues,
					 sizeof(*pipe->subdev_pads),
					 GFP_KERNEL);
	if (!pipe->subdev_pads) {
		dev_info(pipe->imgsys_dev->dev,
			 "failed to alloc pipe->subdev_pads (%d)\n", ret);
		ret = -ENOMEM;
		goto err_release_ctrl;
	}
	ret = media_entity_pads_init(&pipe->subdev.entity,
				     pipe->desc->total_queues,
				     pipe->subdev_pads);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "failed initialize subdev media entity (%d)\n", ret);
		goto err_free_subdev_pads;
	}

	/* Initialize subdev */
	v4l2_subdev_init(&pipe->subdev, &mtk_imgsys_subdev_ops);

	pipe->subdev.entity.function =
		MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	pipe->subdev.entity.ops = &mtk_imgsys_media_ops;
	pipe->subdev.flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	pipe->subdev.ctrl_handler = NULL;

	for (i = 0; i < pipe->desc->total_queues; i++)
		pipe->subdev_pads[pipe->nodes[i].desc->id].flags =
			V4L2_TYPE_IS_OUTPUT(pipe->nodes[i].desc->buf_type) ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;

	snprintf(pipe->subdev.name, sizeof(pipe->subdev.name),
		 "%s", pipe->desc->name);
	v4l2_set_subdevdata(&pipe->subdev, pipe);

	ret = v4l2_device_register_subdev(&pipe->imgsys_dev->v4l2_dev,
					  &pipe->subdev);
	if (ret) {
		dev_info(pipe->imgsys_dev->dev,
			 "failed initialize subdev (%d)\n", ret);
		goto err_media_entity_cleanup;
	}

	dev_dbg(pipe->imgsys_dev->dev,
		"register subdev: %s, ctrl_handler %p\n",
		 pipe->subdev.name, pipe->subdev.ctrl_handler);

	/* Create video nodes and links */
	for (i = 0; i < pipe->desc->total_queues; i++) {
		ret =
			mtk_imgsys_video_device_v4l2_register(pipe,
							      &pipe->nodes[i]);
		if (ret)
			goto err_unregister_subdev;
	}

	return 0;

err_unregister_subdev:
	v4l2_device_unregister_subdev(&pipe->subdev);

err_media_entity_cleanup:
	media_entity_cleanup(&pipe->subdev.entity);

err_free_subdev_pads:
	devm_kfree(pipe->imgsys_dev->dev, pipe->subdev_pads);

err_release_ctrl:
	mtk_imgsys_pipe_v4l2_ctrl_release(pipe);

	return ret;
}

void mtk_imgsys_pipe_v4l2_unregister(struct mtk_imgsys_pipe *pipe)
{
	unsigned int i;

	for (i = 0; i < pipe->desc->total_queues; i++) {
		video_unregister_device(&pipe->nodes[i].vdev);
		vb2_queue_release(&pipe->nodes[i].dev_q.vbq);
		media_entity_cleanup(&pipe->nodes[i].vdev.entity);
		mutex_destroy(&pipe->nodes[i].dev_q.lock);
	}

	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
	mtk_imgsys_pipe_v4l2_ctrl_release(pipe);
}

static void mtk_imgsys_dev_media_unregister(struct mtk_imgsys_dev *imgsys_dev)
{
	media_device_unregister(&imgsys_dev->mdev);
	media_device_cleanup(&imgsys_dev->mdev);
}

static int mtk_imgsys_dev_v4l2_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct media_device *media_dev = &imgsys_dev->mdev;
	struct v4l2_device *v4l2_dev = &imgsys_dev->v4l2_dev;
	int i;
	int ret;

	ret = mtk_imgsys_dev_media_register(imgsys_dev->dev, media_dev);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "%s: media device register failed(%d)\n",
			 __func__, ret);
		return ret;
	}

	v4l2_dev->mdev = media_dev;
	v4l2_dev->ctrl_handler = NULL;

	ret = v4l2_device_register(imgsys_dev->dev, v4l2_dev);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "%s: v4l2 device register failed(%d)\n",
			 __func__, ret);
		goto err_release_media_device;
	}

	for (i = 0; i < MTK_IMGSYS_PIPE_ID_TOTAL_NUM; i++) {
		ret = mtk_imgsys_pipe_init(imgsys_dev,
					   &imgsys_dev->imgsys_pipe[i],
					&imgsys_dev->cust_pipes[i]);
		if (ret) {
			dev_info(imgsys_dev->dev,
				 "%s: Pipe id(%d) init failed(%d)\n",
				 imgsys_dev->imgsys_pipe[i].desc->name,
				 i, ret);
			goto err_release_pipe;
		}
	}

	ret = v4l2_device_register_subdev_nodes(&imgsys_dev->v4l2_dev);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "failed to register subdevs (%d)\n", ret);
		goto err_release_pipe;
	}

	return 0;

err_release_pipe:
	for (i--; i >= 0; i--)
		mtk_imgsys_pipe_release(&imgsys_dev->imgsys_pipe[i]);

	v4l2_device_unregister(v4l2_dev);

err_release_media_device:
	mtk_imgsys_dev_media_unregister(imgsys_dev);

	return ret;
}

void mtk_imgsys_dev_v4l2_release(struct mtk_imgsys_dev *imgsys_dev)
{
	int i;

	for (i = 0; i < MTK_IMGSYS_PIPE_ID_TOTAL_NUM; i++)
		mtk_imgsys_pipe_release(&imgsys_dev->imgsys_pipe[i]);

	v4l2_device_unregister(&imgsys_dev->v4l2_dev);
	media_device_unregister(&imgsys_dev->mdev);
	media_device_cleanup(&imgsys_dev->mdev);
}

static int mtk_imgsys_res_init(struct platform_device *pdev,
			       struct mtk_imgsys_dev *imgsys_dev)
{
	int ret;

	imgsys_dev->mdpcb_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE,
					"imgsys_mdp_callback");
	if (!imgsys_dev->mdpcb_wq) {
		dev_err(imgsys_dev->dev,
			"%s: unable to alloc mdpcb workqueue\n", __func__);
		return -ENOMEM;
	}

	imgsys_dev->composer_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE | WQ_HIGHPRI,
					"imgsys_composer");
	if (!imgsys_dev->composer_wq) {
		dev_err(imgsys_dev->dev,
			"%s: unable to alloc composer workqueue\n", __func__);
		ret = -ENOMEM;
		goto destroy_mdpcb_wq;
	}

	imgsys_dev->runner_wq =
		alloc_ordered_workqueue("%s",
					__WQ_LEGACY | WQ_MEM_RECLAIM |
					WQ_FREEZABLE | WQ_HIGHPRI,
					"imgsys_runner");
	if (!imgsys_dev->runner_wq) {
		dev_err(imgsys_dev->dev,
			"%s: unable to alloc runner workqueue\n", __func__);
		ret = -ENOMEM;
		goto destroy_composer_wq;
	}

	init_waitqueue_head(&imgsys_dev->flushing_waitq);

	return 0;

destroy_composer_wq:
	destroy_workqueue(imgsys_dev->composer_wq);

destroy_mdpcb_wq:
	destroy_workqueue(imgsys_dev->mdpcb_wq);

	return ret;
}

static void mtk_imgsys_res_release(struct mtk_imgsys_dev *imgsys_dev)
{
	flush_workqueue(imgsys_dev->mdpcb_wq);
	destroy_workqueue(imgsys_dev->mdpcb_wq);
	imgsys_dev->mdpcb_wq = NULL;

	flush_workqueue(imgsys_dev->composer_wq);
	destroy_workqueue(imgsys_dev->composer_wq);
	imgsys_dev->composer_wq = NULL;

	flush_workqueue(imgsys_dev->runner_wq);
	destroy_workqueue(imgsys_dev->runner_wq);
	imgsys_dev->runner_wq = NULL;

	atomic_set(&imgsys_dev->num_composing, 0);
	atomic_set(&imgsys_dev->imgsys_enqueue_cnt, 0);
	atomic_set(&imgsys_dev->imgsys_user_cnt, 0);
}

static int __maybe_unused mtk_imgsys_pm_suspend(struct device *dev)
{
	struct mtk_imgsys_dev *imgsys_dev = dev_get_drvdata(dev);
	int ret, num;

	ret = wait_event_timeout
		(imgsys_dev->flushing_waitq,
		 !(num = atomic_read(&imgsys_dev->num_composing)),
		 msecs_to_jiffies(1000 / 30 * IMGSYS_WORKING_BUF_NUM * 3));
	if (!ret && num) {
		dev_info(dev, "%s: flushing SCP job timeout, num(%d)\n",
			 __func__, num);

		return -EBUSY;
	}

	if (pm_runtime_suspended(dev)) {
		dev_info(dev, "%s: pm_runtime_suspended is true, no action\n",
			 __func__);
		return 0;
	}

	ret = pm_runtime_force_suspend(dev);
	if (ret) {
		dev_info(dev, "%s: pm_runtime_force_suspend failed:(%d)\n",
			 __func__, ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused mtk_imgsys_pm_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev)) {
		dev_info(dev, "%s: pm_runtime_suspended is true, no action\n",
			 __func__);
		return 0;
	}

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_info(dev, "%s: pm_runtime_force_resume failed:(%d)\n",
			 __func__, ret);
		return ret;
	}

	return 0;
}

static int mtk_imgsys_of_rproc(struct mtk_imgsys_dev *imgsys,
			       struct platform_device *pdev)
{
	struct device *dev = imgsys->dev;

	imgsys->scp = scp_get(pdev);
	if (!imgsys->scp) {
		dev_err(dev, "failed to get scp device\n");
		return -ENODEV;
	}

	imgsys->rproc_handle = scp_get_rproc(imgsys->scp);
	dev_dbg(dev, "imgsys rproc_phandle: 0x%pK\n", imgsys->rproc_handle);
	imgsys->smem_dev = scp_get_device(imgsys->scp);

	return 0;
}

static int mtk_imgsys_probe(struct platform_device *pdev)
{
	struct mtk_imgsys_dev *imgsys_dev;
	struct device **larb_devs;
	const struct cust_data *data;
	struct device_link *link;
	int larbs_num, i;
	int ret;

	imgsys_dev = devm_kzalloc(&pdev->dev, sizeof(*imgsys_dev), GFP_KERNEL);
	if (!imgsys_dev)
		return -ENOMEM;

	data = of_device_get_match_data(&pdev->dev);

	init_imgsys_pipeline(data);

	imgsys_dev->cust_pipes = data->pipe_settings;
	imgsys_dev->modules = data->imgsys_modules;

	imgsys_dev->dev = &pdev->dev;
	imgsys_dev->imgsys_resource = &pdev->resource[0];
	dev_set_drvdata(&pdev->dev, imgsys_dev);
	imgsys_dev->stream_cnt = 0;
	imgsys_dev->clks = data->clks;
	imgsys_dev->num_clks = data->clk_num;
	imgsys_dev->num_mods = data->mod_num;
	imgsys_dev->dump = data->dump;

	ret = devm_clk_bulk_get(&pdev->dev, imgsys_dev->num_clks,
				imgsys_dev->clks);
	if (ret)
		dev_info(&pdev->dev, "Failed to get clks:%d\n", ret);

	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34)))
		dev_info(&pdev->dev, "%s:No DMA available\n", __func__);

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(imgsys_dev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(imgsys_dev->dev, UINT_MAX);
		if (ret)
			dev_info(imgsys_dev->dev, "Failed to set DMA segment size\n");
	}

	if (mtk_imgsys_of_rproc(imgsys_dev, pdev)) {
		ret = -EFAULT;
		goto err_free_dev_alloc;
	}

	larbs_num = of_count_phandle_with_args(pdev->dev.of_node,
					       "mediatek,larbs", NULL);
	dev_dbg(imgsys_dev->dev, "%d larbs to be added", larbs_num);
	larb_devs = devm_kzalloc(&pdev->dev, sizeof(larb_devs) * larbs_num, GFP_KERNEL);
	if (!larb_devs) {
		ret = -ENOMEM;
		goto err_free_dev_alloc;
	}
	for (i = 0; i < larbs_num; i++) {
		struct device_node *larb_node;
		struct platform_device *larb_pdev;

		larb_node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(imgsys_dev->dev,
				 "%d larb node not found\n", i);
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (!larb_pdev) {
			of_node_put(larb_node);
			dev_info(imgsys_dev->dev,
				 "%d larb device not found\n", i);
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(&pdev->dev, &larb_pdev->dev,
				       DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(imgsys_dev->dev, "unable to link SMI LARB idx %d\n", i);

		larb_devs[i] = &larb_pdev->dev;
	}
	imgsys_dev->larbs = larb_devs;
	imgsys_dev->larbs_num = larbs_num;

	atomic_set(&imgsys_dev->imgsys_enqueue_cnt, 0);
	atomic_set(&imgsys_dev->imgsys_user_cnt, 0);
	atomic_set(&imgsys_dev->num_composing, 0);
	mutex_init(&imgsys_dev->hw_op_lock);
	/* Limited by the SCP's queue size */
	sema_init(&imgsys_dev->sem, SCP_COMPOSING_MAX_NUM);

	ret = mtk_imgsys_hw_working_buf_pool_init(imgsys_dev);
	if (ret) {
		dev_info(&pdev->dev, "working buffer init failed(%d)\n", ret);
		goto err_free_larb_alloc;
	}

	ret = mtk_imgsys_dev_v4l2_init(imgsys_dev);
	if (ret) {
		dev_info(&pdev->dev, "v4l2 init failed(%d)\n", ret);

		goto err_release_working_buf_pool;
	}

	ret = mtk_imgsys_res_init(pdev, imgsys_dev);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "%s: mtk_imgsys_res_init failed(%d)\n", __func__, ret);

		ret = -EBUSY;
		goto err_release_deinit_v4l2;
	}

	imgsys_cmdq_init(imgsys_dev, 1);

	pm_runtime_set_autosuspend_delay(&pdev->dev, 3000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = platform_driver_register(&mtk_imgsys_larb_driver);
	if (ret) {
		dev_info(imgsys_dev->dev, "register mtk_imgsys_larb_driver fail\n");
		goto err_release_deinit_v4l2;
	}

	return 0;

err_release_deinit_v4l2:
	mtk_imgsys_dev_v4l2_release(imgsys_dev);
err_release_working_buf_pool:
	mtk_imgsys_hw_working_buf_pool_release(imgsys_dev);
err_free_larb_alloc:
	devm_kfree(&pdev->dev, imgsys_dev->larbs);
err_free_dev_alloc:
	devm_kfree(&pdev->dev, imgsys_dev);
	return ret;
}

static int mtk_imgsys_remove(struct platform_device *pdev)
{
	struct mtk_imgsys_dev *imgsys_dev = dev_get_drvdata(&pdev->dev);

	mtk_imgsys_res_release(imgsys_dev);
	pm_runtime_disable(&pdev->dev);
	platform_driver_unregister(&mtk_imgsys_larb_driver);
	mtk_imgsys_dev_v4l2_release(imgsys_dev);
	mtk_imgsys_hw_working_buf_pool_release(imgsys_dev);
	mutex_destroy(&imgsys_dev->hw_op_lock);
	imgsys_cmdq_release(imgsys_dev);
	devm_kfree(&pdev->dev, imgsys_dev->larbs);
	devm_kfree(&pdev->dev, imgsys_dev);
	return 0;
}

static int __maybe_unused mtk_imgsys_runtime_suspend(struct device *dev)
{
	struct mtk_imgsys_dev *imgsys_dev = dev_get_drvdata(dev);

	clk_bulk_disable_unprepare(imgsys_dev->num_clks,
				   imgsys_dev->clks);

	return 0;
}

static int __maybe_unused mtk_imgsys_runtime_resume(struct device *dev)
{
	struct mtk_imgsys_dev *imgsys_dev = dev_get_drvdata(dev);
	int ret;

	ret = clk_bulk_prepare_enable(imgsys_dev->num_clks,
				      imgsys_dev->clks);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "failed to enable dip clks(%d)\n", ret);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mtk_imgsys_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_imgsys_pm_suspend, mtk_imgsys_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_imgsys_runtime_suspend,
			   mtk_imgsys_runtime_resume, NULL)
};

static struct clk_bulk_data imgsys_isp7_clks[] = {
	{
		.id = "IMGSYS_CG_IMG_TRAW0",
	},
	{
		.id = "IMGSYS_CG_IMG_TRAW1",
	},
	{
		.id = "IMGSYS_CG_IMG_VCORE_GALS",
	},
	{
		.id = "IMGSYS_CG_IMG_DIP0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE1",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE2",
	},
	{
		.id = "IMGSYS_CG_IMG_ADL_LARB",
	},
	{
		.id = "IMGSYS_CG_IMG_ADL_TOP0",
	},
	{
		.id = "IMGSYS_CG_IMG_ADL_TOP1",
	},
	{
		.id = "IMGSYS_CG_IMG_GALS",
	},
	{
		.id = "DIP_TOP_DIP_TOP",
	},
	{
		.id = "DIP_NR_DIP_NR",
	},
	{
		.id = "WPE1_CG_DIP1_WPE",
	},
	{
		.id = "WPE2_CG_DIP1_WPE",
	},
	{
		.id = "WPE3_CG_DIP1_WPE",
	},
	{
		.id = "ME_CG_IPE",
	},
	{
		.id = "ME_CG_IPE_TOP",
	},
	{
		.id = "ME_CG",
	},
	{
		.id = "ME_CG_LARB12"
	}
};

static const struct module_ops imgsys_isp7_modules[] = {
	[IMGSYS_MOD_TRAW] = {
		.module_id = IMGSYS_MOD_TRAW,
		.init = imgsys_traw_set_initial_value,
		.set = imgsys_traw_set_initial_value_hw,
		.dump = imgsys_traw_debug_dump,
		.ndddump = imgsys_traw_ndd_dump,
		.uninit = imgsys_traw_uninit,
	},
	[IMGSYS_MOD_DIP] = {
		.module_id = IMGSYS_MOD_DIP,
		.init = imgsys_dip_set_initial_value,
		.set = imgsys_dip_set_hw_initial_value,
		.dump = imgsys_dip_debug_dump,
		.ndddump = imgsys_dip_ndd_dump,
		.uninit = imgsys_dip_uninit,
	},
	[IMGSYS_MOD_PQDIP] = {
		.module_id = IMGSYS_MOD_PQDIP,
		.init = imgsys_pqdip_set_initial_value,
		.set = imgsys_pqdip_set_hw_initial_value,
		.dump = imgsys_pqdip_debug_dump,
		.ndddump = imgsys_pqdip_ndd_dump,
		.uninit = imgsys_pqdip_uninit,
	},
	[IMGSYS_MOD_ME] = {
		.module_id = IMGSYS_MOD_ME,
		.init = imgsys_me_set_initial_value,
		.set = NULL,
		.dump = imgsys_me_debug_dump,
		.ndddump = imgsys_me_ndd_dump,
		.uninit = imgsys_me_uninit,
	},
	[IMGSYS_MOD_WPE] = {
		.module_id = IMGSYS_MOD_WPE,
		.init = imgsys_wpe_set_initial_value,
		.set = imgsys_wpe_set_hw_initial_value,
		.dump = imgsys_wpe_debug_dump,
		.ndddump = imgsys_wpe_ndd_dump,
		.uninit = imgsys_wpe_uninit,
	},
	[IMGSYS_MOD_ADL] = {
		.module_id = IMGSYS_MOD_ADL,
		.init = imgsys_adl_init,
		.set = imgsys_adl_set,
		.dump = imgsys_adl_debug_dump,
		.ndddump = NULL,
		.uninit = imgsys_adl_uninit,
	},
	/*pure sw usage for timeout debug dump*/
	[IMGSYS_MOD_IMGMAIN] = {
		.module_id = IMGSYS_MOD_IMGMAIN,
		.init = imgsys_main_init,
		.set = imgsys_main_set_init,
		.dump = NULL,
		.ndddump = NULL,
		.uninit = imgsys_main_uninit,
	},
};

static const struct cust_data imgsys_data[] = {
	[0] = {
		.clks = imgsys_isp7_clks,
		.clk_num = ARRAY_SIZE(imgsys_isp7_clks),
		.module_pipes = module_pipe_isp7,
		.mod_num = ARRAY_SIZE(module_pipe_isp7),
		.pipe_settings = pipe_settings_isp7,
		.pipe_num = ARRAY_SIZE(pipe_settings_isp7),
		.imgsys_modules = imgsys_isp7_modules,
		.dump = imgsys_debug_dump_routine,
	},
};

static const struct of_device_id mtk_imgsys_of_match[] = {
	{ .compatible = "mediatek,imgsys", .data = (void *)&imgsys_data},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_imgsys_of_match);

static struct platform_driver mtk_imgsys_driver = {
	.probe   = mtk_imgsys_probe,
	.remove  = mtk_imgsys_remove,
	.driver  = {
		.name = "mtk-imgsys",
		.owner	= THIS_MODULE,
		.pm = &mtk_imgsys_pm_ops,
		.of_match_table = of_match_ptr(mtk_imgsys_of_match),
	}
};

module_platform_driver(mtk_imgsys_driver);

MODULE_AUTHOR("Frederic Chen <frederic.chen@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek DIP driver");

