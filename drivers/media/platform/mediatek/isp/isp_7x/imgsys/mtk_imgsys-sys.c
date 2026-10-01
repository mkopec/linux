// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include "linux/dma-direction.h"
#include "linux/dma-mapping.h"
#include "linux/gfp_types.h"
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/hashtable.h>
#include "media/videobuf2-dma-contig.h"
#include <media/v4l2-event.h>
#include "mtk_imgsys-scp.h"
#include "mtk_header_desc.h"
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hw.h"
#include "mtk_imgsys-vnode_id.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-module.h"
#include "mtk_imgsys-debug.h"

static int imgsys_quick_onoff_en;
module_param(imgsys_quick_onoff_en, int, 0644);


int mtk_imgsys_hw_working_buf_pool_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_hw_working_buf *buf;
	void *sd;
	dma_addr_t scp_addr, isp_daddr;
	size_t size = sizeof(struct singlenode_desc_norm);
	int i;

	if (imgsys_dev->working_bufs)
		return 0;

	INIT_LIST_HEAD(&imgsys_dev->free_working_bufs.list);
	spin_lock_init(&imgsys_dev->free_working_bufs.lock);

	imgsys_dev->working_bufs = kmalloc_array(IMGSYS_WORKING_BUF_NUM, sizeof(*buf), GFP_KERNEL);
	if (!imgsys_dev->working_bufs)
		return -ENOMEM;

	sd = dma_alloc_coherent(imgsys_dev->smem_dev,
				size * IMGSYS_WORKING_BUF_NUM,
				&scp_addr, GFP_KERNEL);
	if (!sd)
		goto err_alloc;

	isp_daddr = dma_map_resource(imgsys_dev->dev,
				     scp_addr,
				     size * IMGSYS_WORKING_BUF_NUM,
				     DMA_TO_DEVICE, 0);
	if (!isp_daddr)
		goto err_map;

	for (i = 0; i < IMGSYS_WORKING_BUF_NUM; i++) {
		buf = &imgsys_dev->working_bufs[i];

		buf->sd_norm = sd;
		buf->isp_daddr = isp_daddr;
		buf->scp_daddr = scp_addr;

		sd += size;
		isp_daddr += size;
		scp_addr += size;

		list_add_tail(&buf->list,
			      &imgsys_dev->free_working_bufs.list);
	}

	return 0;

err_map:
	dma_free_coherent(imgsys_dev->dev,
			  sizeof(struct singlenode_desc_norm),
			  sd, scp_addr);
err_alloc:
	kfree(imgsys_dev->working_bufs);

	return -ENOMEM;
}

void mtk_imgsys_hw_working_buf_pool_release(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_hw_working_buf *buf = &imgsys_dev->working_bufs[0];

	dma_unmap_resource(imgsys_dev->dev, buf->isp_daddr,
			   sizeof(struct singlenode_desc_norm) * IMGSYS_WORKING_BUF_NUM,
			   DMA_TO_DEVICE, 0);
	dma_free_coherent(imgsys_dev->dev,
			  sizeof(struct singlenode_desc_norm) * IMGSYS_WORKING_BUF_NUM,
			  buf->sd_norm, buf->scp_daddr);
	kfree(imgsys_dev->working_bufs);
}

static void mtk_imgsys_hw_working_buf_free(struct mtk_imgsys_dev *imgsys_dev,
					   struct mtk_imgsys_hw_working_buf *working_buf)
{
	if (!working_buf)
		return;

	spin_lock(&imgsys_dev->free_working_bufs.lock);
	list_add_tail(&working_buf->list,
		      &imgsys_dev->free_working_bufs.list);
	spin_unlock(&imgsys_dev->free_working_bufs.lock);
}

static struct mtk_imgsys_hw_working_buf*
mtk_imgsys_hw_working_buf_alloc(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgsys_hw_working_buf *working_buf;

	spin_lock(&imgsys_dev->free_working_bufs.lock);
	if (list_empty(&imgsys_dev->free_working_bufs.list)) {
		spin_unlock(&imgsys_dev->free_working_bufs.lock);
		return NULL;
	}

	working_buf = list_first_entry(&imgsys_dev->free_working_bufs.list,
				       struct mtk_imgsys_hw_working_buf,
				       list);
	list_del(&working_buf->list);
	spin_unlock(&imgsys_dev->free_working_bufs.lock);

	return working_buf;
}

static void mtk_imgsys_notify(struct mtk_imgsys_request *req, uint64_t frm_owner)
{
	struct mtk_imgsys_dev *imgsys_dev = req->imgsys_pipe->imgsys_dev;
	struct mtk_imgsys_pipe *pipe = req->imgsys_pipe;
	struct req_frameparam *iparam = &req->img_fparam.frameparam;
	enum vb2_buffer_state vbf_state;

	if (!mtk_imgsys_pipe_get_running_job(pipe, req->id))
		goto out;

	if (iparam->state != FRAME_STATE_HW_TIMEOUT)
		vbf_state = VB2_BUF_STATE_DONE;
	else
		vbf_state = VB2_BUF_STATE_ERROR;

	atomic_dec(&imgsys_dev->num_composing);

	mtk_imgsys_hw_working_buf_free(imgsys_dev, req->working_buf);
	req->working_buf = NULL;
	mtk_imgsys_pipe_remove_job(req);
	if (vbf_state == VB2_BUF_STATE_DONE)
		mtk_imgsys_pipe_job_finish(req, vbf_state);

	wake_up(&imgsys_dev->flushing_waitq);
	dev_dbg(imgsys_dev->dev,
		"%s:%s:(reqfd-%d) job id(%d), frame_no(%d) finished\n",
		__func__, pipe->desc->name, req->request_fd, iparam->index,
		iparam->frame_no);

out:
	media_request_put(&req->req);
}

static void cmdq_cb_done_worker(struct work_struct *work)
{
	struct mtk_imgsys_pipe *pipe;
	struct swfrm_info_t *gwfrm_info = NULL;
	struct gce_cb_work *gwork;
	struct img_sw_buffer swbuf_data = {0};

	gwork = container_of(work, struct gce_cb_work, work);
	gwfrm_info = gwork->req_sbuf_kva;

	pipe = gwork->pipe;
	if (!pipe->streaming)
		goto out;

	if (gwfrm_info->user_info[0].ndd_fp[0] != '\0')
		wait_for_completion(gwfrm_info->ndd_user_complete);

	/* send to SCP after frame done & del node from list */
	swbuf_data.offset = gwfrm_info->req_sbuf_goft;
	imgsys_scp_send(pipe->imgsys_dev, IPI_IMGSYS_DEQUE_DONE_ID,
		    &swbuf_data, sizeof(struct img_sw_buffer),
		    gwork->reqfd, 1);

out:
	if (gwfrm_info->user_info[0].ndd_fp[0] != '\0')
		kfree(gwfrm_info->ndd_user_complete);

	kfree(gwork);
}

static void imgsys_mdp_cb_func(struct imgsys_cb_data data,
			       unsigned int subfidx, bool last_task_in_req)
{
	struct mtk_imgsys_pipe *pipe;
	struct mtk_imgsys_request *req;
	struct mtk_imgsys_dev *imgsys_dev;
	struct swfrm_info_t *swfrminfo_cb;
	struct gce_cb_work *gwork;

	swfrminfo_cb = data.data;
	pipe = (struct mtk_imgsys_pipe *)swfrminfo_cb->pipe;
	if (!pipe->streaming) {
		pr_info("%s pipe already streamoff\n", __func__);
		return;
	}

	req = (struct mtk_imgsys_request *)(swfrminfo_cb->req);
	if (!req) {
		pr_info("%s NULL request Address\n", __func__);
		return;
	}

	imgsys_dev = req->imgsys_pipe->imgsys_dev;
	dev_dbg(imgsys_dev->dev, "%s:(reqfd-%d)frame_no(%d) +", __func__,
		req->request_fd,
		req->img_fparam.frameparam.frame_no);

	if (swfrminfo_cb->hw_hang >= 0)
		req->img_fparam.frameparam.state = FRAME_STATE_HW_TIMEOUT;

	mtk_imgsys_notify(req, swfrminfo_cb->frm_owner);

	if (swfrminfo_cb->hw_hang < 0) {
		gwork = kzalloc(sizeof(*gwork), GFP_KERNEL);
		if (!gwork)
			return;

		gwork->reqfd = swfrminfo_cb->request_fd;
		gwork->req_sbuf_kva = swfrminfo_cb->req_sbuf_kva;
		gwork->pipe = swfrminfo_cb->pipe;

		INIT_WORK(&gwork->work, cmdq_cb_done_worker);
		queue_work(req->imgsys_pipe->imgsys_dev->mdpcb_wq, &gwork->work);
	}
}

static void imgsys_runner_func(struct work_struct *work)
{
	struct mtk_imgsys_request *req = mtk_imgsys_runner_work_to_req(work);
	struct mtk_imgsys_dev *imgsys_dev = req->imgsys_pipe->imgsys_dev;
	struct swfrm_info_t *frm_info;
	int ret;

	frm_info = req->swfrm_info;
	frm_info->is_sent = true;

	ret = imgsys_cmdq_sendtask(imgsys_dev, frm_info, imgsys_mdp_cb_func, NULL);
	if (ret)
		dev_info(imgsys_dev->dev,
			 "%s: imgsys_cmdq_sendtask fail(%d)\n", __func__, ret);
}

static void imgsys_scp_handler(void *data, unsigned int len, void *priv)
{
	struct share_buf *scp_msg = (struct share_buf *)data;
	int job_id;
	struct mtk_imgsys_pipe *pipe;
	int pipe_id;
	struct mtk_imgsys_request *req;
	struct mtk_imgsys_dev *imgsys_dev = (struct mtk_imgsys_dev *)priv;
	struct img_sw_buffer *swbuf_data;
	struct swfrm_info_t *swfrm_info;
	int i = 0;
	void *gce_virt;
	int total_framenum = 0;

	if (!data) {
		WARN_ONCE(!data, "%s: failed due to NULL data\n", __func__);
		return;
	}

	// this function just handles id IPI_IMGSYS_FRAME_ID
	if (scp_msg->id != IPI_IMGSYS_FRAME_ID) {
		return;
	}

	if (WARN_ONCE(scp_msg->len != sizeof(struct img_sw_buffer),
		      "%s: len(%d) not match img_sw_buffer\n", __func__, len))
		return;

	swbuf_data = (struct img_sw_buffer *)scp_msg->share_data;
	gce_virt = imgsys_scp_get_reserve_mem_virt(IMG_MEM_G_ID);
	if (!gce_virt) {
		pr_info("%s: invalid gce_virt(%p)\n",
			__func__, gce_virt);
		return;
	}

	swfrm_info = (struct swfrm_info_t *)(gce_virt + (swbuf_data->offset));
	if ((int)swbuf_data->offset < 0 ||
	    swbuf_data->offset > imgsys_scp_get_reserve_mem_size(IMG_MEM_G_ID)) {
		pr_info("%s: invalid swbuf_data->offset(%d), max(%llu)\n",
			__func__, swbuf_data->offset,
			(u64)imgsys_scp_get_reserve_mem_size(IMG_MEM_G_ID));
		return;
	}

	swfrm_info->req_sbuf_goft = swbuf_data->offset;
	swfrm_info->req_sbuf_kva = gce_virt + (swbuf_data->offset);

	job_id = swfrm_info->handle;
	pipe_id = mtk_imgsys_pipe_get_pipe_from_job_id(job_id);
	pipe = mtk_imgsys_dev_get_pipe(imgsys_dev, pipe_id);
	if (!pipe) {
		dev_info(imgsys_dev->dev,
			 "%s: get invalid img_ipi_frameparam index(%d) from firmware\n",
			 __func__, job_id);
		return;
	}

	req = (struct mtk_imgsys_request *)swfrm_info->req_vaddr;
	if (!req) {
		WARN_ONCE(!req, "%s: frame_no(%d) is lost\n", __func__, job_id);
		return;
	}

	if (!req->working_buf) {
		dev_dbg(imgsys_dev->dev,
			"%s: (reqfd-%d) composing\n",
			__func__, req->request_fd);
	}

	swfrm_info->is_sent = false;
	swfrm_info->req = (void *)req;
	swfrm_info->pipe = (void *)pipe;
	swfrm_info->cb_frmcnt = 0;
	swfrm_info->total_taskcnt = 0;
	swfrm_info->chan_id = 0;
	swfrm_info->hw_hang = -1;
	total_framenum = swfrm_info->total_frmnum;

	if (total_framenum < 0 || total_framenum > TMAX) {
		dev_info(imgsys_dev->dev,
			 "%s:unexpected total_framenum (%d -> %d), batchnum(%d) MAX (%d/%d)\n",
			 __func__, swfrm_info->total_frmnum,
			 total_framenum,
			 swfrm_info->batchnum,
			 TMAX, TMAX);
		return;
	}

	for (i = 0 ; i < total_framenum ; i++) {
		swfrm_info->user_info[i].g_swbuf = gce_virt + (swfrm_info->user_info[i].sw_goft);
		swfrm_info->user_info[i].bw_swbuf = gce_virt + (swfrm_info->user_info[i].sw_bwoft);
	}

	req->swfrm_info = swfrm_info;

	up(&imgsys_dev->sem);

	if (swfrm_info->user_info[0].ndd_fp[0] != '\0') {
		swfrm_info->ndd_user_complete =
			kzalloc(sizeof(*swfrm_info->ndd_user_complete), GFP_KERNEL);
		init_completion(swfrm_info->ndd_user_complete);
		imgsys_ndd_swfrm_list_add(imgsys_dev, swfrm_info);
	}

	INIT_WORK(&req->runner_work, imgsys_runner_func);
	queue_work(req->imgsys_pipe->imgsys_dev->runner_wq,
		   &req->runner_work);
}

static int mtk_imgsys_hw_flush_pipe_jobs(struct mtk_imgsys_pipe *pipe)
{
	struct mtk_imgsys_request *req;
	struct list_head job_list = LIST_HEAD_INIT(job_list);
	unsigned long flag;
	int num;
	int ret;

	spin_lock_irqsave(&pipe->running_job_lock, flag);
	list_splice_init(&pipe->pipe_job_running_list, &job_list);
	pipe->num_jobs = 0;
	spin_unlock_irqrestore(&pipe->running_job_lock, flag);

	ret = wait_event_timeout
		(pipe->imgsys_dev->flushing_waitq,
		 !(num = atomic_read(&pipe->imgsys_dev->num_composing)),
		 msecs_to_jiffies(1000 / 30 * IMGSYS_WORKING_BUF_NUM * 3));
	if (!ret && num) {
		dev_info(pipe->imgsys_dev->dev,
			 "flushing is aborted, num(%d)\n", num);
		return -EINVAL;
	}

	list_for_each_entry(req, &job_list, list)
		mtk_imgsys_pipe_job_finish(req, VB2_BUF_STATE_ERROR);

	dev_dbg(pipe->imgsys_dev->dev,
		"%s: wakeup num(%d)\n", __func__, num);
	return 0;
}

static void module_uninit(struct kref *kref)
{
	struct mtk_imgsys_dev *imgsys_dev;
	int i;

	imgsys_dev = container_of(kref, struct mtk_imgsys_dev, init_kref);

	for (i = 0; i < (imgsys_dev->num_mods); i++)
		if (imgsys_dev->modules[i].uninit)
			imgsys_dev->modules[i].uninit(imgsys_dev);
}

void mtk_imgsys_mod_put(struct mtk_imgsys_dev *imgsys_dev)
{
	struct kref *kref;

	kref = &imgsys_dev->init_kref;
	kref_put(kref, module_uninit);
}

void mtk_imgsys_mod_get(struct mtk_imgsys_dev *imgsys_dev)
{
	struct kref *kref;

	kref = &imgsys_dev->init_kref;
	kref_get(kref);
}

static int mtk_imgsys_hw_connect(struct mtk_imgsys_dev *imgsys_dev)
{
	int ret, i;
	u32 user_cnt = 0;
	struct img_init_info info;

	user_cnt = atomic_read(&imgsys_dev->imgsys_user_cnt);
	if (user_cnt != 0)
		dev_info(imgsys_dev->dev,
			 "%s: [ERROR] imgsys user count is not zero(%d)\n",
			 __func__, user_cnt);

	atomic_set(&imgsys_dev->imgsys_user_cnt, 0);

	pm_runtime_get_sync(imgsys_dev->dev);
	/* set default value for hw module */
	for (i = 0; i < (imgsys_dev->num_mods); i++)
		imgsys_dev->modules[i].init(imgsys_dev);
	kref_init(&imgsys_dev->init_kref);

	pm_runtime_put_sync(imgsys_dev->dev);
	if (!imgsys_quick_onoff_en) {
		pm_runtime_get_sync(imgsys_dev->dev);
		for (i = 0; i < (imgsys_dev->num_mods); i++)
			if (imgsys_dev->modules[i].set)
				imgsys_dev->modules[i].set(imgsys_dev);
	}

	ret = imgsys_scp_alloc_reserve_mem(imgsys_dev);
	if (ret) {
		dev_err(imgsys_dev->dev, "%s: imgsys_scp_alloc_reserve_mem failed %d\n",
			__func__, ret);
		return -EBUSY;
	}

	/* IMGSYS HW INIT */
	memset(&info, 0, sizeof(info));
	info.drv_data = (u64)&imgsys_dev;
	info.header_version = HEADER_VER;
	info.frameparam_size = sizeof(struct img_ipi_frameparam);
	info.reg_phys_addr = imgsys_dev->imgsys_resource->start;
	info.reg_range = resource_size(imgsys_dev->imgsys_resource);

	imgsys_scp_get_reserve_mem_info(&info);
	info.sec_tag = imgsys_dev->imgsys_pipe[0].init_info.sec_tag;
	info.full_wd = imgsys_dev->imgsys_pipe[0].init_info.sensor.full_wd;
	info.full_ht = imgsys_dev->imgsys_pipe[0].init_info.sensor.full_ht;
	info.smvr_mode = 0;

	imgsys_scp_init(imgsys_dev, imgsys_scp_handler);

	ret = imgsys_scp_send(imgsys_dev, IPI_IMGSYS_INIT_ID,
			  (void *)&info, sizeof(info), 0, 1);

	if (ret) {
		dev_err(imgsys_dev->dev, "%s: send SCP_IPI_DIP_FRAME failed %d\n",
			__func__, ret);
		return -EBUSY;
	}

	/* calling cmdq stream on */
	imgsys_cmdq_streamon(imgsys_dev);


	return 0;
}

static void mtk_imgsys_hw_disconnect(struct mtk_imgsys_dev *imgsys_dev)
{
	int ret;
	struct img_init_info info = {0};
	u32 user_cnt = 0;

	imgsys_scp_send(imgsys_dev, IPI_IMGSYS_DEINIT_ID,
		    (void *)&info, sizeof(info), 0, 1);

	imgsys_scp_deinit(imgsys_dev);

	/* calling cmdq stream off */
	imgsys_cmdq_streamoff(imgsys_dev);

	/* RELEASE IMGSYS WORKING BUFFER FIRST */
	ret = imgsys_scp_free_reserve_mem(imgsys_dev);
	if (ret) {
		dev_info(imgsys_dev->dev,
			 "%s: imgsys_scp_free_reserve_mem failed(%d)\n",
			 __func__, ret);
	}

	if (!imgsys_quick_onoff_en)
		pm_runtime_put_sync(imgsys_dev->dev);

	mtk_imgsys_mod_put(imgsys_dev);

	user_cnt = atomic_read(&imgsys_dev->imgsys_user_cnt);
	if (user_cnt != 0)
		dev_info(imgsys_dev->dev,
			 "%s: [ERROR] imgsys user count is not yet return to zero(%d)\n",
			 __func__, user_cnt);
}

int mtk_imgsys_hw_streamon(struct mtk_imgsys_pipe *pipe)
{
	struct mtk_imgsys_dev *imgsys_dev = pipe->imgsys_dev;
	int ret;

	mutex_lock(&imgsys_dev->hw_op_lock);
	if (!imgsys_dev->stream_cnt) {
		ret = mtk_imgsys_hw_connect(pipe->imgsys_dev);
		if (ret) {
			dev_info(pipe->imgsys_dev->dev,
				 "%s:%s: pipe(%d) connect to dip_hw failed\n",
				 __func__, pipe->desc->name, pipe->desc->id);

			mutex_unlock(&imgsys_dev->hw_op_lock);

			return ret;
		}
		INIT_LIST_HEAD(&pipe->pipe_job_running_list);
	}
	imgsys_dev->stream_cnt++;
	atomic_set(&imgsys_dev->num_composing, 0);
	mutex_unlock(&imgsys_dev->hw_op_lock);

	pipe->streaming = 1;
	dev_dbg(pipe->imgsys_dev->dev,
		"%s:%s: started stream, id(%d), stream cnt(%d)\n",
		__func__, pipe->desc->name, pipe->desc->id, imgsys_dev->stream_cnt);

	return 0;
}

int mtk_imgsys_hw_streamoff(struct mtk_imgsys_pipe *pipe)
{
	struct mtk_imgsys_dev *imgsys_dev = pipe->imgsys_dev;
	int ret;

	dev_dbg(imgsys_dev->dev,
		"%s:%s: streamoff, removing all running jobs\n",
		__func__, pipe->desc->name);

	pipe->streaming = 0;

	ret = mtk_imgsys_hw_flush_pipe_jobs(pipe);
	if (ret)
		return ret;

	/* Stop the hardware if there is no streaming pipe */
	mutex_lock(&imgsys_dev->hw_op_lock);
	imgsys_dev->stream_cnt--;
	if (!imgsys_dev->stream_cnt)
		mtk_imgsys_hw_disconnect(imgsys_dev);

	mutex_unlock(&imgsys_dev->hw_op_lock);

	return 0;
}

static void imgsys_fill_img_buf(struct mtk_imgsys_dev_buffer *dev_buf,
				struct header_desc_norm *desc_norm,
				struct mtk_imgsys_request *req)
{
	struct frameparams *buf_info = &desc_norm->fparams[0];
	int i;

	desc_norm->fparams_tnum = 1;

	buf_info->buf.num_planes = dev_buf->fmt.fmt.pix_mp.num_planes;
	buf_info->fmt.width = dev_buf->fmt.fmt.pix_mp.width;
	buf_info->fmt.height = dev_buf->fmt.fmt.pix_mp.height;
	buf_info->fmt.pixelformat = dev_buf->fmt.fmt.pix_mp.pixelformat;
	buf_info->crop = dev_buf->crop;
	buf_info->rotation = dev_buf->rotation;
	buf_info->hflip = dev_buf->hflip;
	buf_info->vflip = dev_buf->vflip;
	buf_info->resizeratio = dev_buf->resize_ratio;

	for (i = 0; i < buf_info->buf.num_planes; i++) {
		buf_info->buf.planes[i].m.dma_buf.offset = dev_buf->vbb.planes[i].data_offset;
		buf_info->buf.planes[i].isp_addr = dev_buf->isp_daddr[i];
		buf_info->buf.planes[i].size = dev_buf->vbb.vb2_buf.planes[i].length;
		buf_info->fmt.plane_fmt[i].sizeimage =
			dev_buf->vbb.vb2_buf.planes[i].min_length;
		buf_info->fmt.plane_fmt[i].bytesperline =
			dev_buf->fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
	}
}

static void imgsys_fill_meta_buf(struct mtk_imgsys_dev_buffer *dev_buf,
				 struct header_desc_norm *desc_norm,
				 struct mtk_imgsys_request *req)
{
	struct frameparams *buf_info = &desc_norm->fparams[0];

	memset(&buf_info->buf, 0, sizeof(buf_info->buf));

	desc_norm->fparams_tnum = 1;

	buf_info->buf.num_planes = 1;

	buf_info->fmt.width = dev_buf->fmt.fmt.meta.buffersize;
	buf_info->fmt.height = 1;
	buf_info->fmt.pixelformat = dev_buf->fmt.fmt.meta.dataformat;
	buf_info->fmt.plane_fmt[0].sizeimage = dev_buf->fmt.fmt.meta.buffersize;
	buf_info->fmt.plane_fmt[0].bytesperline = dev_buf->fmt.fmt.meta.buffersize;
	buf_info->buf.planes[0].isp_addr = dev_buf->isp_daddr[0];
	buf_info->buf.planes[0].size = dev_buf->vbb.vb2_buf.planes[0].length;
	buf_info->buf.planes[0].m.dma_buf.phyaddr = dev_buf->scp_daddr[0];
}

static void imgsys_fill_buf_info(int idx, struct mtk_imgsys_dev_buffer *dev_buf,
				 struct singlenode_desc_norm *sd_norm,
				 struct mtk_imgsys_request *req)
{
	if (!dev_buf)
		return;

	if (idx < 0 || idx >= IMG_MAX_HW_DMAS)
		return;

	sd_norm->dmas_enable[idx][0] = 1;

	switch (idx) {
	case MTK_IMGSYS_VIDEO_NODE_ID_TUNING_OUT:
		imgsys_fill_meta_buf(dev_buf, &sd_norm->tuning_meta, req);
		break;
	case MTK_IMGSYS_VIDEO_NODE_ID_CTRLMETA_OUT:
		imgsys_fill_meta_buf(dev_buf, &sd_norm->ctrl_meta, req);
		break;
	case MTK_IMGSYS_VIDEO_NODE_ID_METAI_OUT:
	case MTK_IMGSYS_VIDEO_NODE_ID_FEO_CAPTURE:
	case MTK_IMGSYS_VIDEO_NODE_ID_IMGSTATO_CAPTURE:
		imgsys_fill_meta_buf(dev_buf, &sd_norm->dmas[idx], req);
		break;
	default:
		imgsys_fill_img_buf(dev_buf, &sd_norm->dmas[idx], req);
		break;
	}
}

static void imgsys_fill_singledev_buf(struct mtk_imgsys_request *req)
{
	struct mtk_imgsys_hw_working_buf *buf = req->working_buf;
	int i;

	if (!buf)
		return;

	memset(buf->sd_norm, 0, sizeof(*buf->sd_norm));
	for (i = 0; i < req->imgsys_pipe->desc->total_queues; i++)
		imgsys_fill_buf_info(i, req->buf_map[i], buf->sd_norm, req);
}

static void imgsys_composer_workfunc(struct work_struct *work)
{
	struct mtk_imgsys_request *req = mtk_imgsys_composer_work_to_req(work);
	struct mtk_imgsys_dev *imgsys_dev = req->imgsys_pipe->imgsys_dev;
	struct img_ipi_param ipi_param;
	int ret;

	imgsys_fill_singledev_buf(req);

	dev_dbg(imgsys_dev->dev,
		"%s:(reqfd-%d) to send frame_no(%d)\n",
		__func__, req->request_fd,
		req->img_fparam.frameparam.frame_no);
	media_request_get(&req->req);

	if (down_interruptible(&imgsys_dev->sem))
		return;

	ipi_param.usage = IMG_IPI_FRAME;
	ipi_param.req_addr_va = (u64)req;
	ipi_param.smvr_mode = false;
	ipi_param.frm_param.handle = req->id;
	ipi_param.frm_param.offset = 0;
	ipi_param.frm_param.scp_addr = req->working_buf->scp_daddr;

	mutex_lock(&imgsys_dev->hw_op_lock);
	atomic_inc(&imgsys_dev->num_composing);

	ret = imgsys_scp_send(imgsys_dev, IPI_IMGSYS_FRAME_ID,
			  &ipi_param, sizeof(ipi_param),
			  req->request_fd, 0);

	if (ret) {
		dev_info(imgsys_dev->dev,
			 "frame_no(%d) send SCP_IPI_DIP_FRAME failed %d\n",
			 req->img_fparam.frameparam.frame_no, ret);
		mtk_imgsys_pipe_remove_job(req);
		mtk_imgsys_hw_working_buf_free(imgsys_dev, req->working_buf);
		req->working_buf = NULL;
		mtk_imgsys_pipe_job_finish(req, VB2_BUF_STATE_ERROR);
		wake_up(&imgsys_dev->flushing_waitq);
	}
	mutex_unlock(&imgsys_dev->hw_op_lock);

	dev_dbg(imgsys_dev->dev, "%s:(reqfd-%d) sent\n", __func__,
		req->request_fd);
}

void mtk_imgsys_hw_enqueue(struct mtk_imgsys_dev *imgsys_dev,
			   struct mtk_imgsys_request *req)
{
	struct req_frameparam *frameparam = &req->img_fparam.frameparam;

	req->working_buf = mtk_imgsys_hw_working_buf_alloc(imgsys_dev);
	if (!req->working_buf) {
		dev_info(imgsys_dev->dev,
			 "%s:req(%p): no free working buffer available\n",
			 req->imgsys_pipe->desc->name, req);
		return;
	}

	frameparam->state = FRAME_STATE_INIT;
	frameparam->frame_no =
			atomic_inc_return(&imgsys_dev->imgsys_enqueue_cnt);

	INIT_WORK(&req->composer_work, imgsys_composer_workfunc);
	queue_work(req->imgsys_pipe->imgsys_dev->composer_wq,
		   &req->composer_work);
}
