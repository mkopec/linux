// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_scp.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam-feature.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-regs-mt8188.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-timesync.h"

static unsigned int debug_ae;
module_param(debug_ae, uint, 0644);
MODULE_PARM_DESC(debug_ae, "activates debug ae info");

#define MTK_CAM_CIO_PAD_SRC		PAD_SRC_RAW0
#define MTK_CAM_CIO_PAD_SINK		MTK_RAW_SINK
#define MTK_CAM_IPI_SEND_TIMEOUT	1000
/* consider running_job_list depth 3*3 */
#define RUNNING_JOB_DEPTH		9
static const struct of_device_id mtk_cam_of_ids[] = {
	{.compatible = "mediatek,camisp",},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_cam_of_ids);

/*
 * All member of mtk_cam_request_stream_data which may be used
 * after media_request_ioctl_reinit and before the next
 * media_request_ioctl_queue must be clean here. For
 * example, the pending set fmt, set selection, and sensor
 * switch extension of camsys driver.
 */
static void
mtk_cam_req_s_data_clean(struct mtk_cam_request_stream_data *s_data)
{
	s_data->sensor = NULL;
	s_data->flags = 0;
}

static void
mtk_cam_req_pipe_s_data_clean(struct mtk_cam_request *req, int pipe_id,
			      int index)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_req_get_s_data(req, pipe_id, index);
	if (req_stream_data) {
		mtk_cam_req_s_data_clean(req_stream_data);
		/**
		 * Notice that the we clean req_stream_data->bufs here so
		 * we should not use it after this function called on it.
		 * mtk_cam_vb2_return_all_buffers() uses another list
		 * of mtk_cam_video_device to keep the vb2 buffers to be clean.
		 */
		 memset(req_stream_data->bufs, 0, sizeof(req_stream_data->bufs));
	}
}

void mtk_cam_s_data_update_timestamp(struct mtk_cam_buffer *buf,
				     struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx;
	struct vb2_buffer *vb;
	struct mtk_cam_video_device *node;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx) {
		pr_info("%s: get ctx from s_data failed", __func__);
		return;
	}

	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	buf->vbb.sequence = s_data->frame_seq_no;
	if (vb->vb2_queue->timestamp_flags & V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC)
		vb->timestamp = s_data->timestamp_mono;
	else
		vb->timestamp = s_data->timestamp;

	/*check buffer's timestamp*/
	if (node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG)
		dev_dbg(ctx->cam->dev,
			"%s:%s:vb sequence:%d, queue type:%d, timestamp_flags:0x%x, timestamp:%lld\n",
			__func__, node->desc.name, buf->vbb.sequence,
			vb->vb2_queue->type, vb->vb2_queue->timestamp_flags,
			vb->timestamp);
}

static void mtk_cam_req_return_pipe_buffers(struct mtk_cam_request *req,
					    int pipe_id, int index)
{
	struct mtk_cam_device *cam =
		container_of(req->req.mdev, struct mtk_cam_device, media_dev);
	struct mtk_cam_request_stream_data *s_data_pipe;
	struct mtk_cam_buffer *buf_ret[MTK_RAW_TOTAL_NODES];
	struct mtk_cam_buffer *buf;
	struct mtk_cam_video_device *node;
	struct vb2_buffer *vb;
	int buf_state;
	u32 i, buf_ret_cnt = 0, buf_start = 0, buf_end = 0;

	s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, index);
	if (!s_data_pipe) {
		pr_info("%s: get s_data pipe failed", __func__);
		return;
	}

	if (is_raw_subdev(pipe_id)) {
		buf_start = MTK_RAW_SINK_NUM;
		buf_end = MTK_RAW_PIPELINE_PADS_NUM;
	}

	for (i = buf_start; i < buf_end; i++) {
		/* make sure do not touch req/s_data after vb2_buffe_done */
		buf = mtk_cam_s_data_get_vbuf(s_data_pipe, i);
		if (!buf)
			continue;
		buf_ret[buf_ret_cnt++] = buf;
		/* clean the stream data for req reinit case */
		mtk_cam_s_data_reset_vbuf(s_data_pipe, i);
	}

	/* clean the req_stream_data being used right after request reinit */
	mtk_cam_req_pipe_s_data_clean(req, pipe_id, index);

	buf_state = atomic_read(&s_data_pipe->buf_state);
	if (buf_state == -1)
		buf_state = VB2_BUF_STATE_ERROR;

	dev_dbg(cam->dev,
		"%s:%s: pipe_id(%d) buf_state(%d) buf_ret_cnt(%d)\n", __func__,
		req->req.debug_str, pipe_id, buf_state, buf_ret_cnt);

	for (i = 0; i < buf_ret_cnt; i++) {
		buf = buf_ret[i];
		vb = &buf->vbb.vb2_buf;
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		if (node->uid.pipe_id != pipe_id) {
			dev_info(cam->dev,
				 "%s:%s:node(%s): invalid pipe id (%d), should be (%d)\n",
				 __func__, req->req.debug_str,
				 node->desc.name, node->uid.pipe_id, pipe_id);
			continue;
		}

		if (atomic_read(&req->state) > MTK_CAM_REQ_STATE_PENDING)
			mtk_cam_s_data_update_timestamp(buf, s_data_pipe);

		vb2_buffer_done(&buf->vbb.vb2_buf, buf_state);
	}
}

struct mtk_cam_request_stream_data*
mtk_cam_get_req_s_data(struct mtk_cam_ctx *ctx, unsigned int pipe_id,
		       unsigned int frame_seq_no)

{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *req_stream_data;
	int i;

	spin_lock(&cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &cam->running_job_list, list) {
		if (req->pipe_used & (1 << pipe_id)) {
			for (i = 0; i < req->p_data[pipe_id].s_data_num; i++) {
				req_stream_data = &req->p_data[pipe_id].s_data[i];
				if (req_stream_data->frame_seq_no == frame_seq_no) {
					spin_unlock(&cam->running_job_lock);
					return req_stream_data;
				}
			}
		}
	}
	spin_unlock(&cam->running_job_lock);

	return NULL;
}

struct mtk_cam_request *mtk_cam_get_req(struct mtk_cam_ctx *ctx,
					unsigned int frame_seq_no)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = mtk_cam_get_req_s_data(ctx, ctx->stream_id, frame_seq_no);
	if (!req_stream_data)
		return NULL;

	return req_stream_data->req;
}

bool watchdog_scenario(struct mtk_cam_ctx *ctx)
{
	if (ctx->sensor && !mtk_cam_is_m2m(ctx) && ctx->used_raw_num)
		return true;
	else
		return false;
}

static bool finish_cq_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	struct mtk_cam_working_buf_entry *cq_buf_entry;

	if (!ctx->used_raw_num)
		return false;

	spin_lock(&ctx->processing_buffer_list.lock);

	cq_buf_entry = req_stream_data->working_buf;
	/* Check if the cq buffer is already finished */
	if (!cq_buf_entry || !cq_buf_entry->s_data) {
		dev_info(ctx->cam->dev,
			 "%s:%s:ctx(%d):req(%d):working_buf is already release\n", __func__,
			 req_stream_data->req->req.debug_str, ctx->stream_id,
			 req_stream_data->frame_seq_no);
		spin_unlock(&ctx->processing_buffer_list.lock);
		return false;
	}

	list_del(&cq_buf_entry->list_entry);
	mtk_cam_s_data_reset_wbuf(req_stream_data);
	ctx->processing_buffer_list.cnt--;
	spin_unlock(&ctx->processing_buffer_list.lock);

	mtk_cam_working_buf_put(cq_buf_entry);

	dev_dbg(ctx->cam->dev, "put cq buf:%pad, %s\n",
		&cq_buf_entry->buffer.iova,
		req_stream_data->req->req.debug_str);

	return true;
}

static void update_hw_mapping(struct mtk_cam_ctx *ctx,
			      struct mtkcam_ipi_config_param *config_param)
{
	// raw
	config_param->maps[0].pipe_id = ctx->pipe->id;
	config_param->maps[0].dev_mask = MTKCAM_SUBDEV_RAW_MASK & ctx->used_raw_dev;
	config_param->n_maps = 1;
}

static void mtk_cam_del_req_from_running(struct mtk_cam_ctx *ctx,
					 struct mtk_cam_request *req, int pipe_id)
{
	struct mtk_cam_request_stream_data *s_data;

	s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
	dev_dbg(ctx->cam->dev,
		"%s: %s: removed, req:%d, ctx:(%d/0x%x/0x%x), pipe:(%d/0x%x/0x%x) done_status:0x%x)\n",
		__func__, req->req.debug_str, s_data->frame_seq_no,
		ctx->stream_id, req->ctx_used, ctx->cam->streaming_ctx,
		pipe_id, req->pipe_used, ctx->cam->streaming_pipe,
		req->done_status);

	atomic_set(&req->state, MTK_CAM_REQ_STATE_COMPLETE);
	spin_lock(&ctx->cam->running_job_lock);
	list_del(&req->list);
	ctx->cam->running_job_count--;
	spin_unlock(&ctx->cam->running_job_lock);
}

static void mtk_cam_req_works_clean(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	char *dbg_str = mtk_cam_s_data_get_dbg_str(s_data);

	/* flush the sensor work */
	if (atomic_read(&s_data->sensor_work.is_queued)) {
		kthread_flush_work(&s_data->sensor_work.work);
		dev_dbg(ctx->cam->dev,
			"%s:ctx(%d):%s:seq(%d): flushed sensor_work\n",
			__func__, ctx->stream_id, dbg_str, s_data->frame_seq_no);
	}
}

static void mtk_cam_get_timestamp(struct mtk_cam_ctx *ctx,
				  struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_buffer *buf;
	struct vb2_buffer *vb;
	void *vaddr;
	u64 *timestamp_addr;
	u64 *fho_va;

	buf = mtk_cam_s_data_get_vbuf(s_data, MTK_RAW_META_OUT_0);
	if (!buf) {
		dev_info(ctx->cam->dev,
			 "ctx(%d): can't get MTK_RAW_META_OUT_0 buf from req(%d)\n",
			 ctx->stream_id, s_data->frame_seq_no);
		return;
	}

	vb = &buf->vbb.vb2_buf;
	if (!vb) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get vb2 buf\n",
			 __func__, ctx->stream_id);
		return;
	}

	vaddr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
	if (!vaddr) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get plane_vadd\n",
			 __func__, ctx->stream_id);
		return;
	}

	if (!s_data->working_buf->buffer.va ||
	    s_data->working_buf->buffer.size == 0) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): can't get working_buf\n",
			 __func__, ctx->stream_id);
		return;
	}

	fho_va = (u64 *)(s_data->working_buf->buffer.va +
		s_data->working_buf->buffer.size - 64);

	timestamp_addr = camsys_get_timestamp_addr(vaddr);
	*timestamp_addr = div_u64(mtk_cam_timesync_to_monotonic(*fho_va), 1000);
	*(timestamp_addr + 1) =	div_u64(mtk_cam_timesync_to_boot(*fho_va), 1000);
	dev_dbg(ctx->cam->dev,
		"timestamp TS:momo %llu us boot %llu us, TS_cnt:%llu\n",
		*timestamp_addr, *(timestamp_addr + 1),	*fho_va);
}

int mtk_cam_dequeue_req_frame(struct mtk_cam_ctx *ctx,
			      unsigned int dequeued_frame_seq_no,
			      int pipe_id)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data, *s_data_pipe;
	struct mtk_cam_request_stream_data *deq_s_data[RUNNING_JOB_DEPTH];
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
	struct mtk_ae_debug_data ae_data;
	int buf_state;
	u32 dequeue_cnt, s_data_cnt, handled_cnt;
	bool del_job, del_req;
	bool unreliable = false;
	unsigned int done_status_latch;

	memset(&ae_data, 0, sizeof(struct mtk_ae_debug_data));
	dequeue_cnt = 0;
	s_data_cnt = 0;
	spin_lock(&ctx->cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, &ctx->cam->running_job_list, list) {
		if (!(req->pipe_used & (1 << pipe_id)))
			continue;

		s_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!s_data) {
			dev_info(ctx->cam->dev,
				 "frame_seq:%d[ctx=%d,pipe=%d], de-queue request not found\n",
				 dequeued_frame_seq_no, ctx->stream_id, pipe_id);
			continue;
		}

		if (s_data->frame_seq_no > dequeued_frame_seq_no)
			goto STOP_SCAN;

		deq_s_data[s_data_cnt++] = s_data;
		if (s_data_cnt >= RUNNING_JOB_DEPTH) {
			dev_info(ctx->cam->dev,
				 "%s:%s:ctx(%d):pipe(%d):seq(%d/%d) dequeue s_data over local buffer cnt(%d)\n",
				 __func__, req->req.debug_str, ctx->stream_id, pipe_id,
				 s_data->frame_seq_no, dequeued_frame_seq_no,
				 s_data_cnt);
			goto STOP_SCAN;
		}
	}

STOP_SCAN:
	spin_unlock(&ctx->cam->running_job_lock);

	for (handled_cnt = 0; handled_cnt < s_data_cnt; handled_cnt++) {
		s_data = deq_s_data[handled_cnt];
		del_req = false;
		del_job = false;
		req = mtk_cam_s_data_get_req(s_data);
		if (!req) {
			dev_info(ctx->cam->dev,
				 "%s:ctx(%d):pipe(%d):seq(%d) req not found\n",
				 __func__, ctx->stream_id, pipe_id,
				 s_data->frame_seq_no);
			continue;
		}

		spin_lock(&req->done_status_lock);

		if (req->done_status & 1 << pipe_id) {
			/* already handled by another job done work */
			spin_unlock(&req->done_status_lock);
			continue;
		}

		/* Check whether all pipelines of single ctx are done */
		req->done_status |= 1 << pipe_id;
		if ((req->done_status & ctx->streaming_pipe) ==
		    (req->pipe_used & ctx->streaming_pipe))
			del_job = true;

		if ((req->done_status & ctx->cam->streaming_pipe) ==
		    (req->pipe_used & ctx->cam->streaming_pipe)) {
			if (MTK_CAM_REQ_STATE_RUNNING ==
			    atomic_cmpxchg(&req->state,
					   MTK_CAM_REQ_STATE_RUNNING,
					   MTK_CAM_REQ_STATE_DELETING))
				del_req = true;
		}
		done_status_latch = req->done_status;
		spin_unlock(&req->done_status_lock);

		if (is_raw_subdev(pipe_id) && debug_ae) {
			mtk_cam_raw_dump_aa_info(ctx, &ae_data);
			dev_dbg(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),del_req(%d),metaout,size(%u,%u),AA(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)(0x%llx,0x%llx,0x%llx,0x%llx)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, done_status_latch,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, del_req,
				pipe->res_config.sink_fmt.width, pipe->res_config.sink_fmt.height,
				ae_data.obc_r1_sum[0], ae_data.obc_r1_sum[1],
				ae_data.obc_r1_sum[2], ae_data.obc_r1_sum[3],
				ae_data.obc_r2_sum[0], ae_data.obc_r2_sum[1],
				ae_data.obc_r2_sum[2], ae_data.obc_r2_sum[3],
				ae_data.obc_r3_sum[0], ae_data.obc_r3_sum[1],
				ae_data.obc_r3_sum[2], ae_data.obc_r3_sum[3],
				ae_data.aa_sum[0], ae_data.aa_sum[1],
				ae_data.aa_sum[2], ae_data.aa_sum[3],
				ae_data.ltm_sum[0], ae_data.ltm_sum[1],
				ae_data.ltm_sum[2], ae_data.ltm_sum[3]);
		} else {
			dev_dbg(ctx->cam->dev,
				"%s:%s:ctx(%d):pipe(%d):de-queue seq(%d):handle seq(%d),done(0x%x),pipes(req:0x%x,ctx:0x%x,all:0x%x),del_job(%d),del_req(%d)\n",
				__func__, req->req.debug_str, ctx->stream_id, pipe_id,
				dequeued_frame_seq_no, s_data->frame_seq_no, done_status_latch,
				req->pipe_used, ctx->streaming_pipe, ctx->cam->streaming_pipe,
				del_job, del_req);
		}

		if (is_raw_subdev(pipe_id)) {
			mtk_cam_get_timestamp(ctx, s_data);
			mtk_cam_req_dbg_works_clean(s_data);
			mtk_cam_req_works_clean(s_data);
		}

		if (del_job) {
			atomic_dec(&ctx->running_s_data_cnt);
			mtk_camsys_state_delete(ctx, sensor_ctrl, req);

			/* release internal buffers */
			finish_cq_buf(s_data);
		}

		if (del_req) {
			mtk_cam_del_req_from_running(ctx, req, pipe_id);
			dequeue_cnt++;
		}

		/* release vb2 buffers of the pipe */
		s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, 0);
		if (!s_data_pipe) {
			dev_info(ctx->cam->dev,
				 "%s:%s:ctx(%d):pipe(%d):seq(%d) s_data_pipe not found\n",
				 __func__, req->req.debug_str, ctx->stream_id, pipe_id,
				 s_data->frame_seq_no);
			continue;
		}

		if (s_data->frame_seq_no < dequeued_frame_seq_no) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, time:%lld drop, ctx:%d\n",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no, s_data->timestamp,
				ctx->stream_id);
		} else if (s_data->state.estate == E_STATE_DONE_MISMATCH) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, state done mismatch",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		} else if (unreliable) {
			buf_state = VB2_BUF_STATE_ERROR;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, done (unreliable)",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		} else {
			buf_state = VB2_BUF_STATE_DONE;
			dev_dbg(ctx->cam->dev,
				"%s:%s:pipe(%d) seq:%d, done success",
				__func__, req->req.debug_str, pipe_id,
				s_data->frame_seq_no);
		}

		if (mtk_cam_s_data_set_buf_state(s_data_pipe, buf_state)) {
			/* handle vb2_buffer_done */
			if (mtk_cam_req_put(req, pipe_id))
				dev_dbg(ctx->cam->dev,
					"%s:%s:pipe(%d) return request",
					__func__, req->req.debug_str, pipe_id);
		}
	}

	return dequeue_cnt;
}

void mtk_cam_dev_req_clean_pending(struct mtk_cam_device *cam, int pipe_id,
				   int buf_state)
{
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data_pipe;
	struct list_head *pending = &cam->pending_job_list;
	struct list_head req_clean_list;

	/* Consider pipe bufs and pipe_used only */

	INIT_LIST_HEAD(&req_clean_list);

	spin_lock(&cam->pending_job_lock);
	list_for_each_entry_safe(req, req_prev, pending, list) {
		/* update pipe_used */
		req->pipe_used &= ~(1 << pipe_id);
		list_add_tail(&req->cleanup_list, &req_clean_list);
		if (!(req->pipe_used & cam->streaming_pipe)) {
			/* the last pipe */
			list_del(&req->list);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d) remove req from pending list\n",
				 __func__, req->req.debug_str, pipe_id);
		}
	}
	spin_unlock(&cam->pending_job_lock);

	list_for_each_entry_safe(req, req_prev, &req_clean_list, cleanup_list) {
		list_del(&req->cleanup_list);
		s_data_pipe = mtk_cam_req_get_s_data(req, pipe_id, 0);
		if (!s_data_pipe) {
			dev_dbg(cam->dev,
				"%s:%s:pipe_used(0x%x):pipe(%d) s_data_pipe not found\n",
				__func__, req->req.debug_str, req->pipe_used,
				pipe_id);
			continue;
		}
		if (mtk_cam_s_data_set_buf_state(s_data_pipe, buf_state)) {
			/* handle vb2_buffer_done */
			if (mtk_cam_req_put(req, pipe_id))
				dev_dbg(cam->dev,
					"%s:%s:pipe_used(0x%x):pipe(%d) return request",
					__func__, req->req.debug_str,
					req->pipe_used, pipe_id);
			/* DO NOT touch req after here */
		}
	}
}

void mtk_cam_dev_req_cleanup(struct mtk_cam_ctx *ctx, int pipe_id, int buf_state)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data, *s_data_pipe;
	struct mtk_cam_request_stream_data *clean_s_data[RUNNING_JOB_DEPTH];
	struct list_head *running = &cam->running_job_list;
	unsigned int other_pipes, done_status;
	int i;
	u32 num_s_data, s_data_cnt, handled_cnt;
	bool need_clean_req;

	mtk_cam_dev_req_clean_pending(cam, pipe_id, buf_state);

	s_data_cnt = 0;
	spin_lock(&cam->running_job_lock);
	list_for_each_entry_safe(req, req_prev, running, list) {
		/* only handle requests belong to current ctx */
		if (!(req->pipe_used & ctx->streaming_pipe))
			continue;

		num_s_data = mtk_cam_req_get_num_s_data(req, pipe_id);
		/* reverse the order for release req with s_data_0 */
		for (i = num_s_data - 1; i >= 0; i--) {
			s_data = mtk_cam_req_get_s_data(req, pipe_id, i);
			if (s_data) {
				clean_s_data[s_data_cnt++] = s_data;
				if (s_data_cnt >= RUNNING_JOB_DEPTH) {
					dev_info(cam->dev,
						 "%s: over local buffer cnt(%d)\n",
						 __func__,  s_data_cnt);
					goto STOP_SCAN;
				}
			} else {
				dev_info(cam->dev,
					 "%s:%s:pipe(%d): get s_data failed\n",
					 __func__, req->req.debug_str, pipe_id);
			}
		}
	}
STOP_SCAN:
	spin_unlock(&cam->running_job_lock);

	for (handled_cnt = 0; handled_cnt < s_data_cnt; handled_cnt++) {
		s_data = clean_s_data[handled_cnt];
		req = mtk_cam_s_data_get_req(s_data);
		if (!req) {
			pr_info("ERR can't be recovered: invalid req found in s_data_clean_list\n");
			continue;
		}

		if (ctx->used_raw_num != 0) {
			if (s_data->index > 0)
				dev_info(cam->dev,
					 "%s:%s:pipe(%d):seq(%d): clean s_data_%d, raw_feature(%lld)\n",
					 __func__, req->req.debug_str, pipe_id,
					 s_data->frame_seq_no, s_data->index,
					 ctx->pipe->feature_pending);
			else
				dev_dbg(cam->dev,
					"%s:%s:pipe(%d):seq(%d): clean s_data_%d, raw_feature(%lld)\n",
					__func__, req->req.debug_str, pipe_id,
					s_data->frame_seq_no, s_data->index,
					ctx->pipe->feature_pending);
		}

		/* Cancel s_data's works before we clean up the data */
		if (atomic_read(&s_data->sensor_work.is_queued)) {
			kthread_cancel_work_sync(&s_data->sensor_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel sensor_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->sensor_work.is_queued, 1);

		if (atomic_read(&s_data->meta1_done_work.is_queued)) {
			cancel_work_sync(&s_data->meta1_done_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel AFO done_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->meta1_done_work.is_queued, 1);

		if (atomic_read(&s_data->frame_done_work.is_queued)) {
			cancel_work_sync(&s_data->frame_done_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel frame_done_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->frame_done_work.is_queued, 1);

		if (atomic_read(&s_data->dbg_exception_work.state) ==
			MTK_CAM_REQ_DBGWORK_S_PREPARED) {
			atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_CANCEL);
			mtk_cam_debug_wakeup(&ctx->cam->debug_exception_waitq);
			cancel_work_sync(&s_data->dbg_exception_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel dbg_exception_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

		if (atomic_read(&s_data->dbg_work.state) ==
			MTK_CAM_REQ_DBGWORK_S_PREPARED) {
			cancel_work_sync(&s_data->dbg_work.work);
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): cancel dbg_work\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}
		atomic_set(&s_data->dbg_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

		spin_lock(&req->done_status_lock);
		dev_dbg(cam->dev,
			"%s:%s:pipe(%d):seq(%d):req staus before clean:done(0x%x),pipe_used(0x%x)\n",
			__func__, req->req.debug_str, pipe_id,
			s_data->frame_seq_no, req->done_status, req->pipe_used);

		need_clean_req = false;
		if (atomic_read(&req->state) == MTK_CAM_REQ_STATE_RUNNING) {
			/* mark request status to done for release */
			req->done_status |= req->pipe_used & (1 << pipe_id);
			if (req->done_status == req->pipe_used &&
			    MTK_CAM_REQ_STATE_RUNNING ==
			    atomic_cmpxchg(&req->state,
					   MTK_CAM_REQ_STATE_RUNNING,
					   MTK_CAM_REQ_STATE_DELETING))
				need_clean_req = true;
		}

		/* if being the last one, check other pipes in the ctx */
		other_pipes = 0;
		done_status = req->done_status;
		if (need_clean_req)
			other_pipes = ctx->streaming_pipe & ~(1 << pipe_id);
		spin_unlock(&req->done_status_lock);

		/**
		 * Before remove the request, flush other pipe's done work
		 * in the same ctx to make sure mtk_cam_dev_job_done finished
		 */
		if (other_pipes) {
			for (i = 0; i < MTKCAM_SUBDEV_MAX; i++) {
				if (!(1 << i & other_pipes & done_status))
					continue;

				s_data_pipe = mtk_cam_req_get_s_data(req, i, 0);
				if (!s_data_pipe)
					continue;

				/**
				 * if done_status is marked, it means the work
				 * is running or complete
				 */
				if (flush_work(&s_data->frame_done_work.work))
					dev_info(cam->dev,
						 "%s:%s:pipe(%d):seq(%d): flush pipe(%d) frame_done_work\n",
						 __func__, req->req.debug_str,
						 pipe_id, s_data_pipe->frame_seq_no,
						 i);
			}
		}

		mtk_cam_complete_sensor_hdl(s_data);
		mtk_cam_complete_raw_hdl(s_data);
		/*
		 * reset fs state, if one sensor off and another one alive,
		 * Let the req be the single sensor case.
		 */
		mutex_lock(&req->fs.op_lock);
		mtk_cam_fs_reset(&req->fs);
		mutex_unlock(&req->fs.op_lock);
		if (need_clean_req) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): remove req from running list\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
			atomic_set(&req->state, MTK_CAM_REQ_STATE_CLEANUP);
			spin_lock(&cam->running_job_lock);
			list_del(&req->list);
			cam->running_job_count--;
			spin_unlock(&cam->running_job_lock);
		} else {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):seq(%d): skip remove req from running list\n",
				 __func__, req->req.debug_str, pipe_id,
				 s_data->frame_seq_no);
		}

		if (mtk_cam_s_data_set_buf_state(s_data, buf_state)) {
			if (s_data->index > 0) {
				mtk_cam_req_return_pipe_buffers(req, pipe_id,
								s_data->index);
			} else {
				/* handle vb2_buffer_done */
				if (mtk_cam_req_put(req, pipe_id))
					dev_dbg(cam->dev,
						"%s:%s:pipe(%d) return request",
						__func__, req->req.debug_str,
						pipe_id);
			}
		}
	}

	/* all bufs in this node should be returned by req */

	dev_dbg(cam->dev,
		"%s: cleanup all stream off req, streaming ctx:0x%x, streaming pipe:0x%x)\n",
		__func__, cam->streaming_ctx, cam->streaming_pipe);
}

void mtk_cam_req_get(struct mtk_cam_request *req, int pipe_id)
{
	atomic_inc(&req->ref_cnt);
}

bool mtk_cam_req_put(struct mtk_cam_request *req, int pipe_id)
{
	bool ret = false;

	if (!atomic_dec_return(&req->ref_cnt))
		ret = true;

	/* release the pipe buf with s_data_pipe buf state */
	mtk_cam_req_return_pipe_buffers(req, pipe_id, 0);

	return ret;
}

static int config_img_in_fmt(struct mtk_cam_device *cam,
			     struct mtk_cam_video_device *node,
			     const struct v4l2_format *cfg_fmt,
			     struct mtkcam_ipi_img_input *in_fmt)
{
	/* Check output & input image size dimension */
	if (node->desc.dma_port != MTKCAM_IPI_RAW_RAWI_2) {
		dev_info(cam->dev,
			 "pipe(%d):dam_port(%d) only support MTKCAM_IPI_RAW_RAWI_2 now\n",
			 node->uid.pipe_id, node->desc.dma_port);
		return -EINVAL;
	}

	in_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (in_fmt->fmt.format == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		dev_info(cam->dev, "pipe: %d, node:%d unknown pixel fmt:%d\n",
			 node->uid.pipe_id, node->desc.dma_port,
			 cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	in_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	in_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;
	in_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

	dev_dbg(cam->dev,
		"pipe: %d dma_port:%d size=%0dx%0d, stride:%d\n",
		node->uid.pipe_id, node->desc.dma_port, in_fmt->fmt.s.w,
		in_fmt->fmt.s.h, in_fmt->fmt.stride[0]);

	return 0;
}

static int config_img_fmt(struct mtk_cam_device *cam,
			  struct mtk_cam_video_device *node,
			  const struct v4l2_format *cfg_fmt,
			  struct mtkcam_ipi_img_output *out_fmt, int sd_width,
			  int sd_height)
{
	/* Check output & input image size dimension */
	if (node->desc.dma_port == MTKCAM_IPI_RAW_IMGO &&
	    (cfg_fmt->fmt.pix_mp.width > sd_width ||
			cfg_fmt->fmt.pix_mp.height > sd_height)) {
		dev_err(cam->dev, "pipe: %d cfg(%d,%d) size is larger than sensor(%d,%d)\n",
			node->uid.pipe_id, cfg_fmt->fmt.pix_mp.width, cfg_fmt->fmt.pix_mp.height,
			sd_width, sd_height);
		return -EINVAL;
	}

	out_fmt->fmt.format =
		mtk_cam_get_img_fmt(cfg_fmt->fmt.pix_mp.pixelformat);
	if (out_fmt->fmt.format == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		dev_err(cam->dev, "pipe: %d, node:%d unknown pixel fmt:%d\n",
			node->uid.pipe_id, node->desc.dma_port,
			cfg_fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	out_fmt->fmt.s.w = cfg_fmt->fmt.pix_mp.width;
	out_fmt->fmt.s.h = cfg_fmt->fmt.pix_mp.height;

	/* not support multi-plane stride */
	out_fmt->fmt.stride[0] = cfg_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;

	if (out_fmt->crop.p.x == 0 && out_fmt->crop.s.w == 0) {
		out_fmt->crop.p.x = 0;
		out_fmt->crop.p.y = 0;
		out_fmt->crop.s.w = sd_width;
		out_fmt->crop.s.h = sd_height;
	}

	dev_dbg(cam->dev,
		"pipe: %d dma_port:%d size=%0dx%0d, stride:%d, crop=%0dx%0d\n",
		node->uid.pipe_id, node->desc.dma_port, out_fmt->fmt.s.w,
		out_fmt->fmt.s.h, out_fmt->fmt.stride[0], out_fmt->crop.s.w,
		out_fmt->crop.s.h);

	return 0;
}

static void mtk_cam_req_set_vfmt(struct mtk_cam_device *cam,
				 struct mtk_raw_pipeline *raw_pipeline,
				 struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_video_device *node;
	struct mtk_cam_request *req;
	struct v4l2_format *f;
	struct v4l2_selection *s;
	int i;

	req = mtk_cam_s_data_get_req(s_data);

	/* force update format to every video device before re-streamon */
	for (i = MTK_RAW_SINK_NUM + 1; i < MTK_RAW_META_OUT_BEGIN; i++) {
		node = &raw_pipeline->vdev_nodes[i - MTK_RAW_SINK_NUM];
		f = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
		if (!f) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
				 __func__, req->req.debug_str,
				 node->uid.pipe_id, node->desc.name);
		} else {
			*f = node->active_fmt;
			dev_dbg(cam->dev,
				"%s:%s:pipe(%d):%s:save v4l2 fmt: pixelfmt(0x%x), w(%d), h(%d)\n",
				__func__, req->req.debug_str,
				node->uid.pipe_id, node->desc.name,
				f->fmt.pix_mp.pixelformat,
				f->fmt.pix_mp.width,
				f->fmt.pix_mp.height);
		}

		s = mtk_cam_s_data_get_vsel(s_data, node->desc.id);
		if (!s) {
			dev_info(cam->dev,
				 "%s:%s:pipe(%d):%s: can't find the vsel field to save\n",
				 __func__, req->req.debug_str,
				 node->uid.pipe_id,
				 node->desc.name);
		} else {
			*s = node->active_crop;
		}
	}
}

static int mtk_cam_req_set_fmt(struct mtk_cam_device *cam,
			       struct mtk_cam_request *req)
{
	int pipe_id;
	int pad;
	struct mtk_cam_request_stream_data *stream_data;
	struct mtk_raw_pipeline *raw_pipeline;

	dev_dbg(cam->dev, "%s:%s\n", __func__, req->req.debug_str);
	for (pipe_id = 0; pipe_id < cam->max_stream_num; pipe_id++) {
		if (req->pipe_used & (1 << pipe_id)) {
			if (is_raw_subdev(pipe_id)) {
				stream_data = mtk_cam_req_get_s_data(req, pipe_id, 0);
				raw_pipeline = &cam->raw.pipelines[pipe_id];
				mtk_cam_req_set_vfmt(cam, raw_pipeline,
						     stream_data);
				pad = MTK_RAW_SINK;

				/* Set MEDIA_PAD_FL_SINK pad's fmt */
				for (pad = MTK_RAW_SINK_BEGIN;
				     pad < MTK_RAW_SOURCE_BEGIN; pad++) {
					stream_data->pad_fmt[pad].format =
						raw_pipeline->cfg[pad].mbus_fmt;
				}
				/* Set MEDIA_PAD_FL_SOURCE pad's fmt */
				for (pad = MTK_RAW_SOURCE_BEGIN;
				     pad < MTK_RAW_PIPELINE_PADS_NUM; pad++) {
					stream_data->pad_fmt[pad].format =
						raw_pipeline->cfg[pad].mbus_fmt;
				}
			}
		}
	}
	return 0;
}

static int mtk_cam_req_update_ctrl(struct mtk_raw_pipeline *raw_pipe,
				   struct mtk_cam_request_stream_data *s_data)
{
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);
	struct mtk_cam_request *req;
	struct mtk_cam_req_raw_pipe_data *raw_pipe_data;

	raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!raw_pipe_data) {
		dev_err(raw_pipe->subdev.v4l2_dev->dev,
			"%s: cannot find raw_pipe_data, pipe_id:%d, frm_seq:%d\n",
			__func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}
	req = mtk_cam_s_data_get_req(s_data);
	if (!req) {
		dev_err(raw_pipe->subdev.v4l2_dev->dev,
			"%s: cannot find req, pipe_id:%d, frm_seq:%d\n",
			__func__, s_data->pipe_id, s_data->frame_seq_no);
		return -EINVAL;
	}

	/* clear seamless switch mode */
	raw_pipe->sensor_mode_update = 0;
	mtk_cam_req_ctrl_setup(raw_pipe, req);

	s_data->feature.raw_feature = raw_pipe->feature_pending;
	atomic_set(&s_data->first_setting_check, 0);

	dev_dbg(raw_pipe->subdev.v4l2_dev->dev,
		"%s:%s:%s:raw_feature(0x%0x), sensor_mode_update(0x%0x)\n",
		__func__, raw_pipe->subdev.name, debug_str,
		s_data->feature.raw_feature,
		raw_pipe->sensor_mode_update);

	if (raw_pipe->sensor_mode_update)
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_SENSOR_MODE_UPDATE_T1;

	raw_pipe_data->res = raw_pipe->user_res;

	return 0;
}

static int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *img_out,
				const struct v4l2_format *f, dma_addr_t daddr)
{
	u32 pixelformat = f->fmt.pix_mp.pixelformat;
	u32 width = f->fmt.pix_mp.width;
	u32 height = f->fmt.pix_mp.height;
	const struct v4l2_plane_pix_format *plane = &f->fmt.pix_mp.plane_fmt[0];
	u32 stride = plane->bytesperline;
	u32 aligned_width;
	unsigned int addr_offset = 0;
	int i;

	if (is_mtk_format(pixelformat)) {
		const struct mtk_format_info *info;

		info = mtk_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		aligned_width = stride / info->bpp[0];
		if (info->mem_planes == 1) {
			if (is_yuv_ufo(pixelformat)) {
				aligned_width = ALIGN(width, 64);
				img_out->buf[0][0].iova = daddr;
				img_out->fmt.stride[0] = aligned_width * info->bit_r_num
							 / info->bit_r_den;
				img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
				img_out->buf[0][0].size += img_out->fmt.stride[0] * height / 2;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height
							   / 2;
				img_out->buf[0][0].size += sizeof(struct ufbc_buffer_header);

				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%lx\n",
					 0, img_out->fmt.stride[0], img_out->buf[0][0].size,
					 (unsigned long)img_out->buf[0][0].iova);
			} else if (is_raw_ufo(pixelformat)) {
				aligned_width = ALIGN(width, 64);
				img_out->buf[0][0].iova = daddr;
				img_out->fmt.stride[0] = aligned_width * info->bit_r_num /
							 info->bit_r_den;
				img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
				img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
				img_out->buf[0][0].size += sizeof(struct ufbc_buffer_header);

				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%lx\n",
					 0, img_out->fmt.stride[0], img_out->buf[0][0].size,
					 (unsigned long)img_out->buf[0][0].iova);
			} else {
				for (i = 0; i < info->comp_planes; i++) {
					unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
					unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

					img_out->buf[0][i].iova = daddr + addr_offset;
					img_out->fmt.stride[i] = info->bpp[i] *
						DIV_ROUND_UP(aligned_width, hdiv);
					img_out->buf[0][i].size = img_out->fmt.stride[i]
						* DIV_ROUND_UP(height, vdiv);
					addr_offset += img_out->buf[0][i].size;
					pr_debug("plane:%d stride:%d plane_size:%d addr:0x%lx\n",
						 i, img_out->fmt.stride[i], img_out->buf[0][i].size,
						 (unsigned long)img_out->buf[0][i].iova);
				}
			}
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	} else {
		const struct v4l2_format_info *info;

		info = v4l2_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		aligned_width = stride / info->bpp[0];
		if (info->mem_planes == 1) {
			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				img_out->buf[0][i].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[0][i].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[0][i].size;
				pr_debug("stride:%d plane_size:%d addr:0x%lx\n",
					 img_out->fmt.stride[i], img_out->buf[0][i].size,
					 (unsigned long)img_out->buf[0][i].iova);
			}
		} else {
			pr_debug("do not support non contiguous mplane\n");
		}
	}

	return 0;
}

static void mtk_cam_config_raw_path(struct mtk_cam_device *cam,
				    struct mtkcam_ipi_frame_param *frame_param,
				    struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	struct mtk_raw_pipeline *raw_pipeline;

	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	raw_pipeline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);
	if (!raw_pipeline) {
		dev_err(cam->dev, "%s: cannot find raw_pipeline, pipe_id:%d\n",
			__func__, node->uid.pipe_id);
		return;
	}

	if (raw_pipeline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_BPC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_BPC;
	else if (raw_pipeline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_FUS)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_FUS;
	else if (raw_pipeline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_DGN)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_DGN;
	else if (raw_pipeline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LSC)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LSC;
	else if (raw_pipeline->res_config.raw_path == V4L2_MTK_CAM_RAW_PATH_SELECT_LTM)
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_AFTER_LTM;
	else
		/* un-processed raw frame */
		frame_param->raw_param.imgo_path_sel = MTKCAM_IPI_IMGO_UNPROCESSED;

	dev_dbg(cam->dev, "%s: node:%d fd:%d idx:%d raw_path(%d) ipi imgo_path_sel(%d))\n",
		__func__, node->desc.id, buf->vbb.request_fd, buf->vbb.vb2_buf.index,
		raw_pipeline->res_config.raw_path, frame_param->raw_param.imgo_path_sel);
}

static void mtk_cam_config_raw_img_out_imgo(struct mtk_cam_device *cam,
					    struct mtkcam_ipi_frame_param *frame_param,
					    struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_img_output *img_out;
	unsigned int pixelformat;

	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	pixelformat = node->active_fmt.fmt.pix_mp.pixelformat;
	img_out->buf[0][0].iova = buf->daddr;
	if (is_raw_ufo(pixelformat))
		mtk_cam_fill_img_buf(img_out, &node->active_fmt, buf->daddr);
}

/* Update dmo's buffer information except imgo (address and size) */
static void mtk_cam_config_raw_img_out(struct mtk_cam_device *cam,
				       struct mtkcam_ipi_frame_param *frame_param,
				       struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_img_output *img_out;

	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	mtk_cam_fill_img_buf(img_out, &node->active_fmt, buf->daddr);
}

static int
mtk_cam_config_raw_img_fmt(struct mtk_cam_device *cam,
			   struct mtkcam_ipi_frame_param *frame_param,
			   struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_img_output *img_out;
	struct v4l2_mbus_framefmt *pfmt;
	int sd_width, sd_height, ret;
	struct mtk_raw_pipeline *raw_pipline;

	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	raw_pipline = mtk_cam_dev_get_raw_pipeline(cam, node->uid.pipe_id);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];

	pfmt = &raw_pipline->cfg[MTK_RAW_SINK].mbus_fmt;
	sd_width = pfmt->width;
	sd_height = pfmt->height;

	img_out->uid.pipe_id = node->uid.pipe_id;
	img_out->uid.id =  node->desc.dma_port;

	img_out->crop.p.x = node->active_crop.r.left;
	img_out->crop.p.y = node->active_crop.r.top;
	img_out->crop.s.w = node->active_crop.r.width;
	img_out->crop.s.h = node->active_crop.r.height;

	ret = config_img_fmt(cam, node, &node->active_fmt, img_out, sd_width, sd_height);
	if (ret)
		return ret;

	return 0;
}

/* Update raw_param.imgo_path_sel */
static void mtk_cam_req_config_raw_path(struct mtk_cam_request_stream_data *s_data,
					struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtkcam_ipi_frame_param *frame_param;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	frame_param = &s_data->frame_params;

	mtk_cam_config_raw_path(cam, frame_param, buf);
}

/*
 * Update:
 * 1. imgo's buffer information (address and size)
 * 2. rawi's buffer information (address and size)
 */
static int mtk_cam_req_config_raw_img_out_imgo(struct mtk_cam_request_stream_data *s_data,
					       struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	const struct v4l2_format *cfg_fmt;
	unsigned int pixelformat;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}
	pixelformat = cfg_fmt->fmt.pix_mp.pixelformat;
	img_out->buf[0][0].iova = buf->daddr;
	if (is_raw_ufo(pixelformat))
		mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);

	return 0;
}

/* Update dmo's buffer information except imgo (address and size) */
static int mtk_cam_req_config_raw_img_out(struct mtk_cam_request_stream_data *s_data,
					  struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	const struct v4l2_format *cfg_fmt;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	mtk_cam_fill_img_buf(img_out, cfg_fmt, buf->daddr);

	return 0;
}

static int
mtk_cam_req_config_raw_img_fmt(struct mtk_cam_request_stream_data *s_data,
			       struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_output *img_out;
	struct v4l2_mbus_framefmt *pfmt;
	int sd_width, sd_height, ret;
	const struct v4l2_format *cfg_fmt;
	struct v4l2_selection *cfg_selection;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);

	/* not support sub-sampling multi-plane buffer */
	img_out = &frame_param->img_outs[node->desc.id - MTK_RAW_SOURCE_BEGIN];
	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_err(cam->dev,
			"%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			__func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	cfg_selection = mtk_cam_s_data_get_vsel(s_data, node->desc.id);
	if (!cfg_selection) {
		dev_err(cam->dev,
			"%s:%s:pipe(%d):%s: can't find the vsel field to save\n",
			__func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	pfmt = mtk_cam_s_data_get_pfmt(s_data, MTK_RAW_SINK);
	sd_width = pfmt->width;
	sd_height = pfmt->height;

	img_out->uid.pipe_id = node->uid.pipe_id;
	img_out->uid.id =  node->desc.dma_port;

	img_out->crop.p.x = cfg_selection->r.left;
	img_out->crop.p.y = cfg_selection->r.top;
	img_out->crop.s.w = cfg_selection->r.width;
	img_out->crop.s.h = cfg_selection->r.height;

	ret = config_img_fmt(cam, node, cfg_fmt, img_out, sd_width, sd_height);
	if (ret)
		return ret;

	return 0;
}

static int
mtk_cam_req_config_raw_img_in_rawi2(struct mtk_cam_request_stream_data *s_data,
				    struct mtk_cam_buffer *buf)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_request *req;
	struct mtk_cam_video_device *node;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_img_input *in_fmt;
	const struct v4l2_format *cfg_fmt;
	int ret;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	req = mtk_cam_s_data_get_req(s_data);
	frame_param = &s_data->frame_params;
	node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
	in_fmt = &frame_param->img_ins[node->desc.id - MTK_RAW_RAWI_2_IN];

	cfg_fmt = mtk_cam_s_data_get_vfmt(s_data, node->desc.id);
	if (!cfg_fmt) {
		dev_info(cam->dev,
			 "%s:%s:pipe(%d):%s: can't find the vfmt field to save\n",
			 __func__, req->req.debug_str, node->uid.pipe_id, node->desc.name);
		return -EINVAL;
	}

	in_fmt->buf[0].iova = buf->daddr;
	frame_param->raw_param.hardware_scenario =
		MTKCAM_IPI_HW_PATH_OFFLINE_M2M;

	in_fmt->uid.pipe_id = node->uid.pipe_id;
	in_fmt->uid.id = node->desc.dma_port;
	ret = config_img_in_fmt(cam, node, cfg_fmt, in_fmt);
	if (ret)
		return ret;

	return 0;
}

static int mtk_cam_req_update(struct mtk_cam_device *cam,
			      struct mtk_cam_request *req)
{
	struct media_request_object *obj, *obj_prev;
	struct vb2_buffer *vb;
	struct mtk_cam_buffer *buf;
	struct mtk_cam_video_device *node;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *req_stream_data;
	int i, ctx_cnt;
	int ret;

	dev_dbg(cam->dev, "update request:%s\n", req->req.debug_str);

	mtk_cam_req_set_fmt(cam, req);

	list_for_each_entry_safe(obj, obj_prev, &req->req.objects, list) {
		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		buf = mtk_cam_vb2_buf_to_dev_buf(vb);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

		ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);
		if (!ctx)
			return -EINVAL;
		req->ctx_used |= 1 << ctx->stream_id;

		req_stream_data = mtk_cam_req_get_s_data(req, node->uid.pipe_id, 0);
		req_stream_data->ctx = ctx;
		req_stream_data->no_frame_done_cnt = 0;
		atomic_set(&req_stream_data->sensor_work.is_queued, 0);
		atomic_set(&req_stream_data->dbg_work.state, MTK_CAM_REQ_DBGWORK_S_INIT);
		req_stream_data->dbg_work.dump_flags = 0;
		atomic_set(&req_stream_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_INIT);
		req_stream_data->dbg_exception_work.dump_flags = 0;
		atomic_set(&req_stream_data->frame_done_work.is_queued, 0);
		req->sync_id = (ctx->used_raw_num) ? ctx->pipe->sync_id : 0;

		/* not support TWIN independent AFO  */
		if (ctx->used_raw_num && ctx->pipe->res_config.raw_num_used == 1)
			req_stream_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT;

		/* update buffer format */
		switch (node->desc.dma_port) {
		case MTKCAM_IPI_RAW_RAWI_2:
			ret = mtk_cam_req_config_raw_img_in_rawi2(req_stream_data, buf);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_RAW_IMGO:
			mtk_cam_req_config_raw_path(req_stream_data, buf);
			ret = mtk_cam_req_config_raw_img_out_imgo(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_req_config_raw_img_fmt(req_stream_data, buf);
			if (ret)
				return ret;
			break;
		case MTKCAM_IPI_RAW_YUVO_1:
		case MTKCAM_IPI_RAW_YUVO_2:
		case MTKCAM_IPI_RAW_YUVO_3:
		case MTKCAM_IPI_RAW_YUVO_4:
		case MTKCAM_IPI_RAW_YUVO_5:
		case MTKCAM_IPI_RAW_RZH1N2TO_1:
		case MTKCAM_IPI_RAW_RZH1N2TO_2:
		case MTKCAM_IPI_RAW_RZH1N2TO_3:
		case MTKCAM_IPI_RAW_DRZS4NO_1:
		case MTKCAM_IPI_RAW_DRZS4NO_2:
		case MTKCAM_IPI_RAW_DRZS4NO_3:
			ret = mtk_cam_req_config_raw_img_out(req_stream_data, buf);
			if (ret)
				return ret;

			ret = mtk_cam_req_config_raw_img_fmt(req_stream_data, buf);
				if (ret)
					return ret;
			break;
		case MTKCAM_IPI_RAW_META_STATS_CFG:
		case MTKCAM_IPI_RAW_META_STATS_0:
		case MTKCAM_IPI_RAW_META_STATS_1:
		case MTKCAM_IPI_RAW_META_STATS_2:
			break;
		default:
			/* Do nothing for the ports not related to crop settings */
			break;
		}
	}

	/* frame sync */
	/* prepare img working buf */
	ctx_cnt = 0;
	for (i = 0; i < cam->max_stream_num; i++) {
		if (!(1 << i & req->ctx_used))
			continue;

		/* internal fs */
		ctx_cnt++;

		ctx = &cam->ctxs[i];
		req_stream_data = mtk_cam_req_get_s_data(req, ctx->stream_id, 0);
		if (!req_stream_data)
			continue;
	}
	req->fs.target = ctx_cnt > 1 ? ctx_cnt : 0;

	return 0;
}

/* Check all pipeline involved in the request are streamed on */
static int mtk_cam_dev_req_is_stream_on(struct mtk_cam_device *cam,
					struct mtk_cam_request *req)
{
	dev_dbg(cam->dev,
		"%s: pipe_used(0x%x), streaming_pipe(0x%x), req(%s)\n",
		__func__, req->pipe_used, cam->streaming_pipe,
		req->req.debug_str);
	return (req->pipe_used & cam->streaming_pipe) == req->pipe_used;
}

static void mtk_cam_req_work_init(struct mtk_cam_req_work *work,
				  struct mtk_cam_request_stream_data *s_data)
{
	work->s_data = s_data;
}

static void mtk_cam_req_s_data_init(struct mtk_cam_request *req,
				    u32 pipe_id,
				    u32 s_data_index)
{
	struct mtk_cam_request_stream_data *req_stream_data;

	req_stream_data = &req->p_data[pipe_id].s_data[s_data_index];
	req_stream_data->req = req;
	req_stream_data->pipe_id = pipe_id;
	req_stream_data->state.estate = E_STATE_READY;
	req_stream_data->index = s_data_index;
	atomic_set(&req_stream_data->buf_state, -1);

	/**
	 * req_stream_data->flags is cleaned by
	 * mtk_cam_req_s_data_clean () at previous job done
	 * and may by updated by qbuf before request enqueue
	 * so we don't reset it here.
	 */
	mtk_cam_req_work_init(&req_stream_data->seninf_s_fmt_work, req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->frame_work, req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->frame_done_work, req_stream_data);
	mtk_cam_req_work_init(&req_stream_data->meta1_done_work, req_stream_data);
	/**
	 * clean the param structs since we support req reinit.
	 * the mtk_cam_request may not be "zero" when it is
	 * enqueued.
	 */
	memset(&req_stream_data->frame_params, 0,
	       sizeof(req_stream_data->frame_params));

	/* generally is single exposure */
	req_stream_data->frame_params.raw_param.exposure_num = 1;
}

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req, *req_prev;
	struct mtk_cam_request_stream_data *s_data;
	int i, s_data_flags;
	int enqueue_req_cnt, job_count, s_data_cnt;
	struct list_head equeue_list;
	struct v4l2_ctrl_handler *hdl;
	struct media_request_object *sensor_hdl_obj, *raw_hdl_obj, *obj;
	unsigned long flags;

	if (!cam->streaming_ctx) {
		dev_info(cam->dev, "streams are off\n");
		return;
	}

	INIT_LIST_HEAD(&equeue_list);

	spin_lock(&cam->running_job_lock);
	job_count = cam->running_job_count;
	spin_unlock(&cam->running_job_lock);

	/* Pick up requests which are runnable */
	enqueue_req_cnt = 0;
	spin_lock(&cam->pending_job_lock);
	list_for_each_entry_safe(req, req_prev, &cam->pending_job_list, list) {
		if (likely(mtk_cam_dev_req_is_stream_on(cam, req))) {
			if (job_count + enqueue_req_cnt >=
			    RAW_PIPELINE_NUM * MTK_CAM_MAX_RUNNING_JOBS) {
				dev_dbg(cam->dev, "jobs are full, job cnt(%d)\n",
					job_count);
				break;
			}
			dev_dbg(cam->dev, "%s job cnt(%d), allow req_enqueue(%s)\n",
				__func__, job_count + enqueue_req_cnt, req->req.debug_str);

			enqueue_req_cnt++;
			list_del(&req->list);
			list_add_tail(&req->list, &equeue_list);
		}
	}
	spin_unlock(&cam->pending_job_lock);

	if (!enqueue_req_cnt)
		return;

	list_for_each_entry_safe(req, req_prev, &equeue_list, list) {
		for (i = 0; i < cam->max_stream_num; i++) {
			if (!(req->pipe_used & 1 << i))
				continue;

			/* Initialize ctx related s_data fields */
			ctx = &cam->ctxs[i];
			sensor_hdl_obj = NULL;
			raw_hdl_obj = NULL;
			s_data_flags = 0;

			/* Update frame_seq_no */
			s_data = mtk_cam_req_get_s_data(req, i, 0);
			s_data->frame_seq_no = atomic_inc_return(&ctx->enqueued_frame_seq_no);
			if (is_raw_subdev(i) && ctx->sensor) {
				s_data_cnt =
					atomic_inc_return(&ctx->running_s_data_cnt);
				s_data->sensor = ctx->sensor;

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (hdl == &ctx->pipe->ctrl_handler)
						raw_hdl_obj = obj;
					else if (hdl == ctx->sensor->ctrl_handler)
						sensor_hdl_obj = obj;
				}
				spin_unlock_irqrestore(&req->req.lock, flags);

				if (raw_hdl_obj) {
					s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN;
					s_data->raw_hdl_obj = raw_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find pipe hdl\n",
						__func__, req->req.debug_str, i);
				}

				/* Apply raw subdev's ctrl */
				mtk_cam_req_update_ctrl(ctx->pipe, s_data);

				if (s_data->sensor && s_data->sensor->ctrl_handler &&
				    sensor_hdl_obj) {
					s_data->sensor_hdl_obj = sensor_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find sensor(%s) hdl\n",
						__func__, req->req.debug_str, i,
						s_data->sensor->name);
					s_data->flags |=
						MTK_CAM_REQ_S_DATA_FLAG_SENSOR_HDL_EN;
				}

				s_data_flags = s_data->flags;

				/* reload s_data */
				s_data->flags = s_data_flags;
				s_data->raw_hdl_obj = raw_hdl_obj;
				s_data->sensor_hdl_obj = sensor_hdl_obj;

				/* copy s_data content */
				if (req->p_data[i].s_data_num == 2)
					dev_info(cam->dev,
						 "%s:req(%s): undefined s_data_1, raw_feature(%lld), s_data_cnt(%d)\n",
						 __func__, req->req.debug_str,
						 ctx->pipe->feature_pending,
						 s_data_cnt);
			} else if (is_raw_subdev(i) && !ctx->sensor) {
				/* pure m2m raw ctrl handle */
				s_data_cnt =
					atomic_inc_return(&ctx->running_s_data_cnt);

				spin_lock_irqsave(&req->req.lock, flags);
				list_for_each_entry(obj, &req->req.objects, list) {
					if (vb2_request_object_is_buffer(obj))
						continue;

					hdl = (struct v4l2_ctrl_handler *)obj->priv;
					if (hdl == &ctx->pipe->ctrl_handler)
						raw_hdl_obj = obj;
				}
				spin_unlock_irqrestore(&req->req.lock, flags);

				if (raw_hdl_obj) {
					s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_RAW_HDL_EN;
					s_data->raw_hdl_obj = raw_hdl_obj;
					dev_dbg(cam->dev,
						"%s:%s:ctx(%d): find pipe hdl, s_data_cnt(%d)\n",
						__func__, req->req.debug_str, i, s_data_cnt);
				}

				/* Apply raw subdev's ctrl */
				mtk_cam_req_update_ctrl(ctx->pipe, s_data);
			}
		}

		if (mtk_cam_req_update(cam, req)) {
			dev_info(cam->dev,
				 "%s:req(%s): invalid req settings which can't be recovered\n",
				 __func__, req->req.debug_str);
			WARN_ON(1);
			return;
		}

		atomic_set(&req->state, MTK_CAM_REQ_STATE_RUNNING);
		spin_lock(&cam->running_job_lock);
		cam->running_job_count++;
		list_del(&req->list);
		list_add_tail(&req->list, &cam->running_job_list);
		spin_unlock(&cam->running_job_lock);
		mtk_cam_dev_req_enqueue(cam, req);
	}
}

static struct media_request *mtk_cam_req_alloc(struct media_device *mdev)
{
	struct mtk_cam_request *cam_req;

	cam_req = vzalloc(sizeof(*cam_req));
	spin_lock_init(&cam_req->done_status_lock);
	mutex_init(&cam_req->fs.op_lock);

	return &cam_req->req;
}

static void mtk_cam_req_free(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);

	vfree(cam_req);
}

static int mtk_cam_req_chk_job_list(struct mtk_cam_device *cam,
				    struct mtk_cam_request *new_req,
				    struct list_head *job_list,
				    char *job_list_name)
{
	if (!job_list || !job_list->prev || !job_list->prev->next ||
	    !new_req) {
		dev_dbg(cam->dev,
			"%s:%s: job_list, job_list->prev, job_list->prev->next, new_req can't be NULL\n",
			__func__, job_list_name);
		return -EINVAL;
	}

	if (job_list->prev->next != job_list) {
		dev_dbg(cam->dev, "%s broken: job_list->prev->next(%p), job_list(%p), req(%s)\n",
			job_list_name, job_list->prev->next, job_list,
			new_req->req.debug_str);
		return -EINVAL;
	}

	if (&new_req->list == job_list->prev || &new_req->list == job_list) {
		dev_dbg(cam->dev, "%s job double add: req(%s)\n",
			job_list_name, new_req->req.debug_str);
		return -EINVAL;
	}

	return 0;
}

static void mtk_cam_req_p_data_init(struct mtk_cam_request *req,
				    u32 pipe_id,
				    u32 s_data_num)
{
	u32 i = 0;

	if (pipe_id >= MTKCAM_SUBDEV_MAX)
		return;

	req->p_data[pipe_id].s_data_num = s_data_num;
	for (i = 0; i < s_data_num; i++)
		mtk_cam_req_s_data_init(req, pipe_id, i);
}

static inline void mtk_cam_req_cnt_init(struct mtk_cam_request *req)
{
	atomic_set(&req->ref_cnt, 0);
}

static unsigned int mtk_cam_req_get_pipe_used(struct media_request *req)
{
	/**
	 * V4L2 framework doesn't trigger q->ops->buf_queue(q, buf) when it is
	 * stream off. We have to check the used context through the request directly
	 * before streaming on.
	 */
	struct media_request_object *obj;
	unsigned int pipe_used = 0;
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	unsigned int i;
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

	list_for_each_entry(obj, &req->objects, list) {
		struct vb2_buffer *vb;
		struct mtk_cam_video_device *node;

		if (!vb2_request_object_is_buffer(obj))
			continue;
		vb = container_of(obj, struct vb2_buffer, req_obj);
		node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
		pipe_used |= 1 << node->uid.pipe_id;
	}

	mtk_cam_req_cnt_init(cam_req);

	/* Initialize per pipe's stream data (without ctx)*/
	for (i = 0; i < cam->max_stream_num; i++) {
		if (pipe_used & (1 << i)) {
			mtk_cam_req_p_data_init(cam_req, i, 1);
			mtk_cam_req_get(cam_req, i); /* pipe id */
		}
	}

	return pipe_used;
}

static void mtk_cam_req_queue(struct media_request *req)
{
	struct mtk_cam_request *cam_req = to_mtk_cam_req(req);
	struct mtk_cam_device *cam =
		container_of(req->mdev, struct mtk_cam_device, media_dev);

	/* reset done status */
	cam_req->done_status = 0;
	cam_req->pipe_used = mtk_cam_req_get_pipe_used(req);
	cam_req->ctx_used = 0; /* will update after stream on */
	mtk_cam_fs_reset(&cam_req->fs);

	/* update frame_params's dma_bufs in mtk_cam_vb2_buf_queue */
	vb2_request_queue(req);

	/* add to pending job list */
	spin_lock(&cam->pending_job_lock);
	if (mtk_cam_req_chk_job_list(cam, cam_req,
				     &cam->pending_job_list,
				     "pending_job_list")) {
		spin_unlock(&cam->pending_job_lock);
		return;
	}

	/**
	 * Add req's ref cnt since it is used by pending_job_list and running
	 * pending_job_list.
	 */
	atomic_set(&cam_req->state, MTK_CAM_REQ_STATE_PENDING);
	list_add_tail(&cam_req->list, &cam->pending_job_list);
	spin_unlock(&cam->pending_job_lock);
	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_try_queue(cam);
	mutex_unlock(&cam->queue_lock);
}

static int mtk_cam_link_notify(struct media_link *link, u32 flags,
			       unsigned int notification)
{
	struct media_entity *source = link->source->entity;
	struct media_entity *sink = link->sink->entity;
	struct v4l2_subdev *subdev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;

	if (source->function != MEDIA_ENT_F_VID_IF_BRIDGE ||
	    notification != MEDIA_DEV_NOTIFY_POST_LINK_CH)
		return v4l2_pipeline_link_notify(link, flags, notification);

	subdev = media_entity_to_v4l2_subdev(sink);
	if (!subdev)
		return -EINVAL;
	cam = container_of(subdev->v4l2_dev->mdev, struct mtk_cam_device, media_dev);
	ctx = mtk_cam_find_ctx(cam, sink);

	if (!ctx || !ctx->streaming || !(flags & MEDIA_LNK_FL_ENABLED))
		return v4l2_pipeline_link_notify(link, flags, notification);

	dev_info(cam->dev, "stream_id(%d) is streaming, can't change link\n",
		 ctx->stream_id);

	return -EBUSY;
}

static const struct media_device_ops mtk_cam_dev_ops = {
	.link_notify = mtk_cam_link_notify,
	.req_alloc = mtk_cam_req_alloc,
	.req_free = mtk_cam_req_free,
	.req_validate = vb2_request_validate,
	.req_queue = mtk_cam_req_queue,
};

static int mtk_cam_of_rproc(struct mtk_cam_device *cam,
			    struct platform_device *pdev)
{
	struct device *dev = cam->dev;

	cam->scp = scp_get(pdev);
	if (!cam->scp) {
		dev_err(dev, "failed to get scp device\n");
		return -ENODEV;
	}

	cam->rproc_handle = scp_get_rproc(cam->scp);
	dev_dbg(dev, "camsys rproc_phandle: 0x%pK\n", cam->rproc_handle);
	cam->smem_dev = scp_get_device(cam->scp);

	return 0;
}

struct mtk_raw_device *get_main_raw_dev(struct mtk_cam_device *cam,
					struct mtk_raw_pipeline *pipe)
{
	struct device *dev_main = NULL;
	unsigned int i;

	for (i = 0; i < cam->num_raw_devices; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_main = cam->raw.devs[i];
			break;
		}
	}

	if (!dev_main) {
		dev_err(cam->dev, "Not found main raw device\n");
		return NULL;
	}

	return dev_get_drvdata(dev_main);
}

struct mtk_raw_device *get_sub_raw_dev(struct mtk_cam_device *cam,
				       struct mtk_raw_pipeline *pipe)
{
	struct device *dev_sub = NULL;
	unsigned int i;

	for (i = 0; i < cam->num_raw_devices - 1; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_sub = cam->raw.devs[i + 1];
			break;
		}
	}

	if (!dev_sub) {
		dev_err(cam->dev, "Not found sub raw device\n");
		return NULL;
	}

	return dev_get_drvdata(dev_sub);
}

struct mtk_raw_device *get_sub2_raw_dev(struct mtk_cam_device *cam,
					struct mtk_raw_pipeline *pipe)
{
	struct device *dev_sub = NULL;
	unsigned int i;

	for (i = 0; i < cam->num_raw_devices; i++) {
		if (pipe->enabled_raw & (1 << i)) {
			dev_sub = cam->raw.devs[i + 2];
			break;
		}
	}

	if (!dev_sub) {
		dev_err(cam->dev, "Not found second sub raw device\n");
		return NULL;
	}

	return dev_get_drvdata(dev_sub);
}

static int isp_composer_handle_ack(struct mtk_cam_device *cam,
				   struct mtkcam_ipi_event *ipi_msg)
{
	struct device *dev = cam->dev;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_raw_device *raw_dev;
	bool is_m2m_apply_cq = false;

	ctx = &cam->ctxs[ipi_msg->cookie.session_id];

	if (mtk_cam_is_m2m(ctx)) {
		if (ipi_msg->cookie.frame_no > 1) {
			dev_dbg(dev, "[M2M] wait_for_completion +\n");
			wait_for_completion(&ctx->m2m_complete);
			dev_dbg(dev, "[M2M] wait_for_completion -\n");
		}
	}

	spin_lock(&ctx->using_buffer_list.lock);

	ctx->composed_frame_seq_no = ipi_msg->cookie.frame_no;
	dev_dbg(dev, "ctx:%d ack frame_num:%d\n",
		ctx->stream_id, ctx->composed_frame_seq_no);

	/* get from using list */
	if (list_empty(&ctx->using_buffer_list.list)) {
		spin_unlock(&ctx->using_buffer_list.lock);
		return -EINVAL;
	}
	/* assign raw using buf */
	buf_entry = list_first_entry(&ctx->using_buffer_list.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->using_buffer_list.cnt--;

	spin_lock(&cam->dma_processing_lock);
	if (!list_empty(&cam->dma_processing)) {
		spin_unlock(&cam->dma_processing_lock);
		buf_entry->cq_desc_offset =
			ipi_msg->ack_data.frame_result.cq_desc_offset;
		buf_entry->cq_desc_size =
			ipi_msg->ack_data.frame_result.cq_desc_size;
		buf_entry->sub_cq_desc_offset =
			ipi_msg->ack_data.frame_result.sub_cq_desc_offset;
		buf_entry->sub_cq_desc_size =
			ipi_msg->ack_data.frame_result.sub_cq_desc_size;

		if (mtk_cam_is_m2m(ctx)) {
			dev_info(dev, "Unsupport M2M with non-request buffer\n");
			return -EINVAL;
		}
		goto skip_req;
	}
	spin_unlock(&cam->dma_processing_lock);

	s_data = buf_entry->s_data;
	if (!s_data) {
		dev_dbg(dev, "ctx:%d no req for ack frame_num:%d\n",
			ctx->stream_id, ctx->composed_frame_seq_no);
		spin_unlock(&ctx->using_buffer_list.lock);
		return -EINVAL;
	}

	buf_entry->cq_desc_offset =
		ipi_msg->ack_data.frame_result.cq_desc_offset;
	buf_entry->cq_desc_size =
		ipi_msg->ack_data.frame_result.cq_desc_size;
	buf_entry->sub_cq_desc_offset =
		ipi_msg->ack_data.frame_result.sub_cq_desc_offset;
	buf_entry->sub_cq_desc_size =
		ipi_msg->ack_data.frame_result.sub_cq_desc_size;

	/* Do nothing if the user doesn't enable force dump */
	mtk_cam_req_dump(s_data,
			 MTK_CAM_REQ_DUMP_FORCE,
			 "Camsys Force Dump", false);

	if (ipi_msg->ack_data.ret) {
		mtk_cam_req_dump(s_data,
				 MTK_CAM_REQ_DUMP_DEQUEUE_FAILED,
				 "Camsys compose error", false);
	}
	if (mtk_cam_is_m2m(ctx)) {
		spin_lock(&ctx->composed_buffer_list.lock);
		dev_dbg(dev, "%s ctx->composed_buffer_list.cnt %d\n", __func__,
			ctx->composed_buffer_list.cnt);

		if (ctx->composed_buffer_list.cnt == 0)
			is_m2m_apply_cq = true;

		spin_unlock(&ctx->composed_buffer_list.lock);

		spin_lock(&ctx->processing_buffer_list.lock);
		dev_dbg(dev, "%s ctx->processing_buffer_list.cnt %d\n", __func__,
			ctx->processing_buffer_list.cnt);
		spin_unlock(&ctx->processing_buffer_list.lock);
	}

skip_req:
	if (ctx->composed_frame_seq_no == 1 || is_m2m_apply_cq) {
		struct device *dev;
		/* apply raw CQ */
		spin_lock(&ctx->processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
			      &ctx->processing_buffer_list.list);
		ctx->processing_buffer_list.cnt++;
		spin_unlock(&ctx->processing_buffer_list.lock);
		spin_unlock(&ctx->using_buffer_list.lock);

		dev = mtk_cam_find_raw_dev(cam, ctx->pipe->enabled_raw);
		if (!dev) {
			dev_dbg(dev, "frm#1 raw device not found\n");
			return -EINVAL;
		}

		raw_dev = dev_get_drvdata(dev);

		if (mtk_cam_is_m2m(ctx)) {
			dev_dbg(dev, "%s M2M apply_cq, composed_buffer_list.cnt %d frame_seq_no %d\n",
				__func__, ctx->composed_buffer_list.cnt,
				s_data->frame_seq_no);
			mtk_cam_m2m_enter_cq_state(&s_data->state);
		}

		mtk_cam_raw_apply_cq(raw_dev, buf_entry->buffer.iova,
				     buf_entry->cq_desc_size,
				     buf_entry->cq_desc_offset,
				     buf_entry->sub_cq_desc_size,
				     buf_entry->sub_cq_desc_offset);
		if (s_data) {
			s_data->timestamp = ktime_get_boottime_ns();
			s_data->timestamp_mono = ktime_get_ns();
		}

		return 0;
	}

	spin_lock(&ctx->composed_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->composed_buffer_list.list);
	ctx->composed_buffer_list.cnt++;
	if (mtk_cam_is_m2m(ctx)) {
		dev_dbg(dev, "%s M2M composed_buffer_list.cnt %d\n",
			__func__, ctx->composed_buffer_list.cnt);
	}
	spin_unlock(&ctx->composed_buffer_list.lock);
	spin_unlock(&ctx->using_buffer_list.lock);

	return 0;
}

static void isp_composer_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)priv;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_event *ipi_msg;
	struct mtk_cam_ctx *ctx;

	ipi_msg = (struct mtkcam_ipi_event *)data;

	if (len < offsetofend(struct mtkcam_ipi_event, ack_data)) {
		dev_dbg(dev, "wrong IPI len:%d\n", len);
		return;
	}

	if (ipi_msg->cmd_id != CAM_CMD_ACK ||
	    (ipi_msg->ack_data.ack_cmd_id != CAM_CMD_FRAME &&
	     ipi_msg->ack_data.ack_cmd_id != CAM_CMD_DESTROY_SESSION))
		return;

	if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_FRAME) {
		isp_composer_handle_ack(cam, ipi_msg);
		return;

	} else if (ipi_msg->ack_data.ack_cmd_id == CAM_CMD_DESTROY_SESSION) {
		ctx = &cam->ctxs[ipi_msg->cookie.session_id];
		complete(&ctx->session_complete);
		dev_info(dev, "%s:ctx(%d): session destroyed",
			 __func__, ctx->stream_id);
	}
}

static int isp_composer_init(struct mtk_cam_device *cam)
{
	struct device *dev = cam->dev;
	int ret;

	ret = rproc_boot(cam->rproc_handle);
	if (ret) {
		dev_err(dev, "failed to rproc_boot\n");
		return ret;
	}

	ret = scp_ipi_register(cam->scp, SCP_IPI_ISP_CMD,
			       isp_composer_handler, cam);
	if (ret) {
		dev_err(dev, "failed to register IPI cmd\n");
		return ret;
	}

	ret = scp_ipi_register(cam->scp, SCP_IPI_ISP_FRAME,
			       isp_composer_handler, cam);
	if (ret) {
		dev_err(dev, "failed to register IPI frame\n");
		goto unreg_ipi_cmd;
	}

	return 0;

unreg_ipi_cmd:
	scp_ipi_unregister(cam->scp, SCP_IPI_ISP_CMD);

	return ret;
}

static void isp_composer_uninit(struct mtk_cam_device *cam)
{
	scp_ipi_unregister(cam->scp, SCP_IPI_ISP_CMD);
	scp_ipi_unregister(cam->scp, SCP_IPI_ISP_FRAME);
}

int isp_composer_create_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_session_param	*session_data = &event.session_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CREATE_SESSION;
	session->session_id = ctx->stream_id;
	session_data->workbuf.iova = ctx->buf_pool.working_buf_iova;
	session_data->workbuf.scp_addr = ctx->buf_pool.working_buf_scp_addr;
	session_data->workbuf.size = ctx->buf_pool.working_buf_size;
	session_data->msg_buf.scp_addr = ctx->buf_pool.msg_buf_scp_addr;
	session_data->msg_buf.size = ctx->buf_pool.msg_buf_size;
	session_data->raw_workbuf.scp_addr = ctx->buf_pool.raw_workbuf_scp_addr;
	session_data->raw_workbuf.size = ctx->buf_pool.raw_workbuf_size;
	session_data->priv_workbuf.scp_addr = ctx->buf_pool.priv_workbuf_scp_addr;
	session_data->priv_workbuf.size = ctx->buf_pool.priv_workbuf_size;
	session_data->session_buf.scp_addr = ctx->buf_pool.session_buf_scp_addr;
	session_data->session_buf.size = ctx->buf_pool.session_buf_size;

	scp_ipi_send(cam->scp, SCP_IPI_ISP_CMD, &event,
		     sizeof(event), MTK_CAM_IPI_SEND_TIMEOUT);
	dev_dbg(cam->dev,
		"%s: IPI send id: %d, cq_buf(scp addr:%x,sz:%d, msg_buf(scp addr:%x,sz%d)\n",
		__func__, event.cmd_id, session_data->workbuf.scp_addr,
		session_data->workbuf.size, session_data->msg_buf.scp_addr,
		session_data->msg_buf.size);
	return 0;
}

void isp_composer_destroy_session(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_DESTROY_SESSION;
	session->session_id = ctx->stream_id;
	scp_ipi_send(cam->scp, SCP_IPI_ISP_CMD, &event,
		     sizeof(event), MTK_CAM_IPI_SEND_TIMEOUT);
	dev_info(cam->dev, "IPI send id: %d\n", event.cmd_id);
}

static void
isp_composer_hw_config(struct mtk_cam_device *cam,
		       struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_config_param *config_param)
{
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_config_param *config = &event.config_data;

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_CONFIG;
	session->session_id = ctx->stream_id;
	memcpy(config, config_param, sizeof(*config_param));
	dev_dbg(cam->dev, "%s sw_feature:%d\n", __func__, config->sw_feature);
	scp_ipi_send(cam->scp, SCP_IPI_ISP_CMD, &event,
		     sizeof(event), MTK_CAM_IPI_SEND_TIMEOUT);
	dev_dbg(cam->dev, "IPI send id: %d\n", event.cmd_id);

	/* For debug dump file */
	memcpy(&ctx->config_params, config_param, sizeof(*config_param));
	dev_dbg(cam->dev, "%s:ctx(%d): save config_param to ctx, sz:%zu\n",
		__func__, ctx->stream_id, sizeof(*config_param));
}

static int mtk_cam_s_data_dev_config(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct device *dev;
	struct mtkcam_ipi_config_param config_param;
	struct mtkcam_ipi_input_param *cfg_in_param;
	struct mtk_raw_pipeline *pipe;
	struct mtk_raw *raw;
	struct v4l2_mbus_framefmt *mf;
	struct device *dev_raw;
	struct mtk_raw_device *raw_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	u32 mf_code;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	cam = ctx->cam;
	dev = cam->dev;
	pipe = ctx->pipe;
	raw = pipe->raw;
	mf = &pipe->cfg[MTK_RAW_SINK].mbus_fmt;
	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data)
		return -EINVAL;

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;

	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	img_fmt = &pipe->vdev_nodes[MTK_RAW_SINK].pending_fmt;
	cfg_in_param->in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;
	dev_dbg(dev, "sink pad code:0x%x, tg size:%d %d\n", mf->code,
		cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);

	mf_code = mf->code & 0xffff; /* sensor mode */
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf_code);
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf_code);
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN) {
		dev_err(dev, "unknown sd code:%d\n", mf_code);
		return -EINVAL;
	}

	s_raw_pipe_data->enabled_raw = ctx->pipe->enabled_raw & MTKCAM_SUBDEV_RAW_MASK;
	config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE;

	dev_dbg(dev, "%s: config_param flag:0x%x enabled_raw:0x%x\n", __func__,
		config_param.flags, s_raw_pipe_data->enabled_raw);

	update_hw_mapping(ctx, &config_param);

	config_param.sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;

	dev_raw = mtk_cam_find_raw_dev(cam, s_raw_pipe_data->enabled_raw);
	if (!dev_raw) {
		dev_err(dev, "config raw device not found\n");
		return -EINVAL;
	}

	raw_dev = dev_get_drvdata(dev_raw);
	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		if (raw->pipelines[i].enabled_raw & 1 << raw_dev->id) {
			raw_dev->pipeline = &raw->pipelines[i];
			/* TWIN case */
			if (raw->pipelines[i].res_config.raw_num_used != 1) {
				struct mtk_raw_device *raw_dev_sub =
						get_sub_raw_dev(cam, ctx->pipe);
				raw_dev_sub->pipeline = &raw->pipelines[i];
				dev_dbg(dev, "twin main/sub raw_id:%d/%d\n",
					raw_dev->id, raw_dev_sub->id);
				if (raw->pipelines[i].res_config.raw_num_used == 3) {
					struct mtk_raw_device *raw_dev_sub2 =
						get_sub2_raw_dev(cam, ctx->pipe);
					raw_dev_sub2->pipeline = &raw->pipelines[i];
					dev_dbg(dev, "triplet m/s/s2 raw_id:%d/%d/%d\n",
						raw_dev->id, raw_dev_sub->id, raw_dev_sub2->id);
				}
			}
			break;
		}

	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}

static void mtk_cam_res_init(struct mtk_cam_resource_config *res_cfg)
{
	res_cfg->raw_num_used = 1;
	res_cfg->bin_enable = 0;
	res_cfg->raw_path = 0;
	res_cfg->hwn_limit_min = 1;
	res_cfg->raw_feature = 0;
}

static int mtk_cam_buf_config(struct mtk_cam_device *cam,
			      struct mtkcam_ipi_frame_param *frame_param,
			      struct mtk_cam_buffer *buf)
{
	struct vb2_buffer *vb;
	struct mtk_cam_video_device *node;
	struct mtk_cam_ctx *ctx;
	int ret;

	vb = &buf->vbb.vb2_buf;
	node = mtk_cam_vbq_to_vdev(vb->vb2_queue);

	ctx = mtk_cam_find_ctx(cam, &node->vdev.entity);

	/* update buffer format */
	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_IMGO:
		mtk_cam_config_raw_path(cam, frame_param, buf);
		mtk_cam_config_raw_img_out_imgo(cam, frame_param, buf);
		ret = mtk_cam_config_raw_img_fmt(cam, frame_param, buf);
		if (ret)
			return ret;
		break;
	case MTKCAM_IPI_RAW_YUVO_1:
	case MTKCAM_IPI_RAW_YUVO_2:
	case MTKCAM_IPI_RAW_YUVO_3:
	case MTKCAM_IPI_RAW_YUVO_4:
	case MTKCAM_IPI_RAW_YUVO_5:
	case MTKCAM_IPI_RAW_RZH1N2TO_1:
	case MTKCAM_IPI_RAW_RZH1N2TO_2:
	case MTKCAM_IPI_RAW_RZH1N2TO_3:
	case MTKCAM_IPI_RAW_DRZS4NO_1:
	case MTKCAM_IPI_RAW_DRZS4NO_2:
	case MTKCAM_IPI_RAW_DRZS4NO_3:
		mtk_cam_config_raw_img_out(cam, frame_param, buf);
		break;
	default:
		/* Do nothing for the ports not related to crop settings */
		break;
	}

	return 0;
}

void mtk_cam_buf_try_queue(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam;
	struct mtk_cam_buffer *buf, *buf_prev;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtkcam_ipi_frame_param *frame_data;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct list_head equeue_list;
	unsigned int processing_cnt, enque_cnt;

	cam = ctx->cam;
	if (!cam->streaming_ctx) {
		dev_info(cam->dev, "streams are off\n");
		return;
	}

	INIT_LIST_HEAD(&equeue_list);

	spin_lock(&cam->dma_processing_lock);
	processing_cnt = cam->dma_processing_count;
	spin_unlock(&cam->dma_processing_lock);

	enque_cnt = 0;
	spin_lock(&cam->dma_pending_lock);
	list_for_each_entry_safe(buf, buf_prev, &cam->dma_pending, list) {
		if (processing_cnt + enque_cnt >= MTK_CAM_MAX_PROCESSING_BUFS) {
			dev_dbg(cam->dev, "processing bufs are full, buf cnt(%d)\n",
				processing_cnt);
			break;
		}
		dev_dbg(cam->dev, "%s buf cnt(%d)\n",
			__func__, processing_cnt + enque_cnt);

		enque_cnt++;
		list_del(&buf->list);
		list_add_tail(&buf->list, &equeue_list);
	}
	spin_unlock(&cam->dma_pending_lock);

	if (!enque_cnt)
		return;

	frame_param = kzalloc(sizeof(*frame_param), GFP_KERNEL);
	if (!frame_param)
		return;

	list_for_each_entry_safe(buf, buf_prev, &equeue_list, list) {
		if (buf->state.estate == E_BUF_STATE_COMPOSED)
			continue;

		memset(&event, 0, sizeof(event));
		event.cmd_id = CAM_CMD_FRAME;
		session->session_id = ctx->stream_id;
		/* prepare working buffer */
		buf_entry = mtk_cam_working_buf_get(ctx);
		if (!buf_entry) {
			dev_info(cam->dev, "%s: No CQ buf availablle: enqueued_frame_seq_no:%d\n",
				 __func__, atomic_read(&ctx->enqueued_frame_seq_no));
			WARN_ON(1);
			goto EXIT;
		}

		spin_lock(&ctx->using_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry, &ctx->using_buffer_list.list);
		ctx->using_buffer_list.cnt++;
		spin_unlock(&ctx->using_buffer_list.lock);

		spin_lock(&cam->dma_processing_lock);
		list_del(&buf->list);
		list_add_tail(&buf->list, &cam->dma_processing);
		cam->dma_processing_count++;
		spin_unlock(&cam->dma_processing_lock);
		/* Prepare rp message */
		frame_info->cur_msgbuf_offset =
			buf_entry->msg_buffer.va -
			cam->ctxs[session->session_id].buf_pool.msg_buf_va;
		frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
		frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;
		session->frame_no = atomic_inc_return(&ctx->enqueued_frame_seq_no);

		if (mtk_cam_buf_config(cam, frame_param, buf)) {
			dev_err(cam->dev, "%s: Buffer config failed\n",	__func__);
			continue;
		}
		memcpy(frame_data, frame_param, sizeof(*frame_param));
		frame_data->cur_workbuf_offset =
			buf_entry->buffer.iova -
			cam->ctxs[session->session_id].buf_pool.working_buf_iova;
		frame_data->cur_workbuf_size = buf_entry->buffer.size;

		if (ctx->pipe->res_config.bin_limit == BIN_AUTO)
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_enable;
		else
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_limit;

		scp_ipi_send(cam->scp, SCP_IPI_ISP_FRAME, &event,
			     sizeof(event), MTK_CAM_IPI_SEND_TIMEOUT);
		buf->state.estate = E_BUF_STATE_COMPOSED;
	}
EXIT:
	kfree(frame_param);
}

static void isp_tx_frame_worker(struct work_struct *work)
{
	struct mtk_cam_req_work *req_work = (struct mtk_cam_req_work *)work;
	struct mtkcam_ipi_event event;
	struct mtkcam_ipi_session_cookie *session = &event.cookie;
	struct mtkcam_ipi_frame_info *frame_info = &event.frame_data;
	struct mtkcam_ipi_frame_param *frame_data;
	struct mtk_cam_request *req;
	struct mtk_cam_request_stream_data *req_stream_data;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_device *cam;
	struct mtk_cam_working_buf_entry *buf_entry;
	struct mtk_cam_resource *res_user;
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;

	req_stream_data = mtk_cam_req_work_get_s_data(req_work);
	if (!req_stream_data) {
		pr_info("%s mtk_cam_req_work(%p), req_stream_data(%p), dropped\n",
			__func__, req_work, req_stream_data);
		return;
	}
	req = mtk_cam_s_data_get_req(req_stream_data);
	if (!req) {
		pr_info("%s req_stream_data(%p), req(%p), dropped\n",
			__func__, req_stream_data, req);
		return;
	}
	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	if (!ctx) {
		pr_info("%s req_stream_data(%p), ctx(%p), dropped\n",
			__func__, req_stream_data, ctx);
		return;
	}

	cam = ctx->cam;
	if (ctx->used_raw_num == 0) {
		dev_dbg(cam->dev, "raw is un-used, skip frame work");
		return;
	}
	/* check if the ctx is streaming */
	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_info(cam->dev,
			 "%s: skip frame work, for stream off ctx:%d, req:%d\n",
			 __func__, ctx->stream_id, req_stream_data->frame_seq_no);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(req_stream_data);

	/* Send CAM_CMD_CONFIG if the sink pad fmt is changed */
	if (req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_SINK_FMT_UPDATE)
		mtk_cam_s_data_dev_config(req_stream_data);

	if (!s_raw_pipe_data) {
		dev_info(cam->dev, "error: %s: s_raw_pipe_data = NULL\n", __func__);
		return;
	}

	memset(&event, 0, sizeof(event));
	event.cmd_id = CAM_CMD_FRAME;
	session->session_id = ctx->stream_id;
	/* prepare working buffer */
	buf_entry = mtk_cam_working_buf_get(ctx);
	if (!buf_entry) {
		dev_info(cam->dev, "%s: No CQ buf availablle: req:%d(%s)\n",
			 __func__, req_stream_data->frame_seq_no,
			 req->req.debug_str);
		WARN_ON(1);
		return;
	}
	mtk_cam_s_data_set_wbuf(req_stream_data, buf_entry);
	/* put to using list */
	spin_lock(&ctx->using_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry, &ctx->using_buffer_list.list);
	ctx->using_buffer_list.cnt++;
	spin_unlock(&ctx->using_buffer_list.lock);

	/* Prepare rp message */
	frame_info->cur_msgbuf_offset =
		buf_entry->msg_buffer.va -
		cam->ctxs[session->session_id].buf_pool.msg_buf_va;
	frame_info->cur_msgbuf_size = buf_entry->msg_buffer.size;
	frame_data = (struct mtkcam_ipi_frame_param *)buf_entry->msg_buffer.va;
	session->frame_no = req_stream_data->frame_seq_no;

	memcpy(frame_data, &req_stream_data->frame_params,
	       sizeof(req_stream_data->frame_params));
	frame_data->cur_workbuf_offset =
		buf_entry->buffer.iova -
		cam->ctxs[session->session_id].buf_pool.working_buf_iova;
	frame_data->cur_workbuf_size = buf_entry->buffer.size;

	res_user = mtk_cam_s_data_get_res(req_stream_data);
	if (res_user && res_user->raw_res.bin) {
		frame_data->raw_param.bin_flag = res_user->raw_res.bin;
	} else {
		if (ctx->pipe->res_config.bin_limit == BIN_AUTO)
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_enable;
		else
			frame_data->raw_param.bin_flag = ctx->pipe->res_config.bin_limit;
	}

	scp_ipi_send(cam->scp, SCP_IPI_ISP_FRAME, &event,
		     sizeof(event), MTK_CAM_IPI_SEND_TIMEOUT);
	dev_dbg(cam->dev,
		"%s: IPI send id: %d, ctx:%d, seq:%d, bin:(0x%x)\n",
		req->req.debug_str, event.cmd_id, session->session_id,
		req_stream_data->frame_seq_no,
		frame_data->raw_param.bin_flag);
}

void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			     struct mtk_cam_request *req)
{
	unsigned int i, j;

	for (i = 0; i < cam->max_stream_num; i++) {
		if (req->pipe_used & (1 << i))  {
			unsigned int stream_id = i;
			struct mtk_cam_req_work *frame_work, *done_work;
			struct mtk_cam_request_stream_data *req_stream_data;
			struct mtk_cam_request_stream_data *pipe_stream_data;
			struct mtk_cam_ctx *ctx = &cam->ctxs[stream_id];
			struct mtk_camsys_sensor_ctrl *sensor_ctrl = &ctx->sensor_ctrl;
			unsigned int drained_seq_no = 0;
			unsigned int initial_frame = 0;

			if (!ctx->streaming)
				continue;
			atomic_set(&ctx->sensor_ctrl.sensor_enq_seq_no,
				   atomic_read(&ctx->enqueued_frame_seq_no));
			/*sensor setting after request drained check*/
			if (ctx->used_raw_num) {
				if (ctx->pipe->feature_active == 0 &&
				    ctx->dequeued_frame_seq_no > 3) {
					drained_seq_no =
						atomic_read(&sensor_ctrl->last_drained_seq_no);
					if (atomic_read(&sensor_ctrl->sensor_enq_seq_no) ==
					    drained_seq_no)
						mtk_cam_submit_kwork_in_sensorctrl
						(sensor_ctrl->sensorsetting_wq, sensor_ctrl);
				}
			}
			req_stream_data = mtk_cam_req_get_s_data(req, stream_id, 0);

			if (req_stream_data->frame_seq_no == 1)
				initial_frame = 1;
			frame_work = &req_stream_data->frame_work;
			mtk_cam_req_work_init(frame_work, req_stream_data);

			for (j = 0 ; j < MTKCAM_SUBDEV_MAX ; j++) {
				if ((1 << j & ctx->streaming_pipe) && (1 << j & req->pipe_used)) {
					pipe_stream_data = mtk_cam_req_get_s_data(req, j, 0);
					done_work = &pipe_stream_data->frame_done_work;
					INIT_WORK(&done_work->work, mtk_cam_frame_done_work);

					done_work = &pipe_stream_data->meta1_done_work;
					atomic_set(&done_work->is_queued, 0);
					INIT_WORK(&done_work->work, mtk_cam_meta1_done_work);
				}
			}

			if (ctx->sensor &&
			    (initial_frame || mtk_cam_is_m2m(ctx)))
				mtk_cam_initial_sensor_setup(req, ctx);

			if (ctx->used_raw_num != 0) {
				if (ctx->sensor &&
				    ctx->pipe->feature_active == 0 &&
				    req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			} else {
				if (ctx->sensor &&
				    req_stream_data->frame_seq_no == 2) {
					mtk_cam_initial_sensor_setup(req, ctx);
				}
			}

			/* Prepare CQ compose work */
			mtk_cam_req_dump_work_init(req_stream_data);
			INIT_WORK(&frame_work->work, isp_tx_frame_worker);
			queue_work(ctx->composer_wq, &frame_work->work);

			dev_dbg(cam->dev, "%s:ctx:%d:req:%d(%s) enqueue ctx_used:0x%x,streaming_ctx:0x%x,job cnt:%d, running(%d)\n",
				__func__, stream_id, req_stream_data->frame_seq_no,
				req->req.debug_str, req->ctx_used, cam->streaming_ctx,
				cam->running_job_count, atomic_read(&ctx->running_s_data_cnt));
		}
	}
}

struct mtk_raw_pipeline*
mtk_cam_dev_get_raw_pipeline(struct mtk_cam_device *cam,
			     unsigned int pipe_id)
{
	if (pipe_id < MTKCAM_SUBDEV_RAW_START ||
	    pipe_id >= MTKCAM_SUBDEV_RAW_START + cam->num_raw_devices)
		return NULL;
	else
		return &cam->raw.pipelines[pipe_id - MTKCAM_SUBDEV_RAW_0];
}

static int
mtk_cam_raw_pipeline_config(struct mtk_cam_ctx *ctx,
			    struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	int ret, i;

	/* reset pm_runtime during streaming dynamic change */
	if (ctx->streaming) {
		for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
			if (pipe->enabled_raw & 1 << i) {
				dev_info(raw->cam_dev, "%s: power off raw (%d) for reset\n",
					 __func__, i);
				pm_runtime_put_sync(raw->devs[i]);
			}
		}
	}

	ret = mtk_cam_raw_select(ctx, cfg_in_param);
	if (ret) {
		dev_info(raw->cam_dev, "failed select raw: %d\n",
			 ctx->stream_id);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(raw->devs); i++) {
		if (pipe->enabled_raw & 1 << i) {
			dev_info(raw->cam_dev, "%s: power on raw (%d)\n", __func__, i);
			ret = pm_runtime_get_sync(raw->devs[i]);

			if (ret < 0) {
				dev_info(raw->cam_dev,
					 "failed at pm_runtime_get_sync: %s\n",
					 dev_driver_string(raw->devs[i]));

				/* put devices already get */
				for (; i >= 0; i--) {
					pm_runtime_put_sync(raw->devs[i]);
					dev_info(raw->cam_dev, "%s: power off raw (%d)\n",
						 __func__, i);
				}

				return ret;
			}
		}
	}

	ctx->used_raw_dev = pipe->enabled_raw;
	dev_info(raw->cam_dev, "ctx_id %d used_raw_dev 0x%x pipe_id %d\n",
		 ctx->stream_id, ctx->used_raw_dev, pipe->id);
	return 0;
}

void mtk_cam_apply_pending_dev_config(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_req_raw_pipe_data *s_raw_pipe_data;
	struct mtk_cam_ctx *ctx;
	char *debug_str = mtk_cam_s_data_get_dbg_str(s_data);

	s_raw_pipe_data = mtk_cam_s_data_get_raw_pipe_data(s_data);
	if (!s_raw_pipe_data)
		return;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx)
		return;
	ctx->pipe->feature_active = ctx->pipe->user_res.raw_res.feature;
	ctx->pipe->enabled_raw = s_raw_pipe_data->enabled_raw;
	ctx->used_raw_dev = s_raw_pipe_data->enabled_raw;

	dev_info(ctx->cam->dev,
		 "%s:%s:pipe(%d):seq(%d):feature_active(0x%llx), ctx->pipe->user_res.raw_res.feature(%lld), enabled_raw(0x%x)\n",
		 __func__, debug_str, ctx->stream_id, s_data->frame_seq_no,
		 ctx->pipe->feature_active,
		 ctx->pipe->user_res.raw_res.feature,
		 ctx->pipe->enabled_raw);
}

static int mtk_cam_dev_config(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_config_param config_param;
	struct mtkcam_ipi_input_param *cfg_in_param;
	struct mtk_raw_pipeline *pipe = ctx->pipe;
	struct mtk_raw *raw = pipe->raw;
	struct v4l2_mbus_framefmt *mf = &pipe->cfg[MTK_RAW_SINK].mbus_fmt;
	struct device *dev_raw;
	struct mtk_raw_device *raw_dev;
	unsigned int i;
	int ret;
	u32 mf_code;

	/**
	 * If don't want to get the first req's raw_feature (not the max exp. num),
	 * you can use read ctx->pipe->feature_pending here.
	 */

	memset(&config_param, 0, sizeof(config_param));

	/* Update cfg_in_param */
	cfg_in_param = &config_param.input;
	cfg_in_param->pixel_mode = ctx->pipe->res_config.tgo_pxl_mode;

	cfg_in_param->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	cfg_in_param->in_crop.s.w = mf->width;
	cfg_in_param->in_crop.s.h = mf->height;

	if (mtk_cam_is_pure_m2m(ctx)) {
		mf = &pipe->cfg[MTK_RAW_RAWI_2_IN].mbus_fmt;
		dev_dbg(dev, "[pure m2m] rawi2 pad code:0x%x, sink tg size:%d %d\n",
			mf->code, cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);
	} else {
		dev_dbg(dev, "sink pad code:0x%x, tg size:%d %d\n", mf->code,
			cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h);
	}

	mf_code = mf->code & 0xffff; /* sensor mode */
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf_code);
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf_code);
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN ||
	    cfg_in_param->raw_pixel_id == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN) {
		dev_err(dev, "unknown sd code:%d\n", mf_code);
		return -EINVAL;
	}

	config_param.flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
	ret = mtk_cam_raw_pipeline_config(ctx, cfg_in_param);
	if (ret)
		return ret;

	dev_dbg(dev, "%s: config_param flag:0x%x enabled_raw:0x%x\n", __func__,
		config_param.flags, ctx->pipe->enabled_raw);

	update_hw_mapping(ctx, &config_param);
	config_param.sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;

	dev_raw = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
	if (!dev_raw) {
		dev_err(dev, "config raw device not found\n");
		return -EINVAL;
	}
	raw_dev = dev_get_drvdata(dev_raw);
	for (i = 0; i < RAW_PIPELINE_NUM; i++)
		if (raw->pipelines[i].enabled_raw & 1 << raw_dev->id) {
			raw_dev->pipeline = &raw->pipelines[i];
			if (raw->pipelines[i].res_config.raw_num_used == 0)
				mtk_cam_res_init(&raw->pipelines[i].res_config);
			break;
		}

	mtk_cam_raw_reset(raw_dev);
	isp_composer_hw_config(cam, ctx, &config_param);
	dev_dbg(dev, "raw %d %s done\n", raw_dev->id, __func__);

	return 0;
}

static void mtk_cam_ctx_watchdog_worker(struct work_struct *work)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_device *raw;
	struct v4l2_subdev *seninf;
	static u64 last_vsync_count;

	ctx = container_of(work, struct mtk_cam_ctx, watchdog_work);

	/* dump cam-raw */
	raw = get_main_raw_dev(ctx->cam, ctx->pipe);
	if (!raw)
		goto EXIT;
	dev_info(ctx->cam->dev,
		 "%s:ctx(%d):[timeout] VF(%d) vsync count(%lld) sof count(%lld) timeout_tg(%d)(>%dms)\n",
		 __func__, ctx->stream_id, atomic_read(&raw->vf_en),
		 raw->vsync_count, raw->sof_count, ctx->watchdog_timeout_tg,
		 ctx->watchdog_timeout_tg * MTK_CAM_CTX_WATCHDOG_INTERVAL);

	if (last_vsync_count == raw->vsync_count)
		dev_err(ctx->cam->dev, "%s:cam-raw abnormal vsync\n", __func__);
	last_vsync_count = raw->vsync_count;

	/* dump seninf */
	seninf = ctx->seninf;
	if (seninf)
		mtk_cam_seninf_dump(seninf);

EXIT:
	atomic_inc(&ctx->watchdog_dump);
}

#define WDT_DUMP_CNT 4

static void mtk_ctx_watchdog_callback(struct timer_list *t)
{
	struct mtk_cam_ctx *ctx = from_timer(ctx, t, watchdog_timer);

	/* disable if not streaming */
	if (!ctx->streaming)
		return;

	if (atomic_read(&ctx->watchdog_dump) < WDT_DUMP_CNT)
		schedule_work(&ctx->watchdog_work);

	dev_info_ratelimited(ctx->cam->dev,
			     "%s:ctx(%d):[TIMEOUT] no_sync_tg:%d(>%dms)\n",
			     __func__, ctx->stream_id, ctx->watchdog_timeout_tg,
			     jiffies_to_msecs(jiffies -
					atomic_long_read(&ctx->watchdog_prev)));

	/* update timer */
	mod_timer(&ctx->watchdog_timer,
		  jiffies + msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL));
}

void mtk_ctx_watchdog_kick(struct mtk_cam_ctx *ctx)
{
	dev_dbg(ctx->cam->dev, "%s:ctx(%d)\n", __func__, ctx->stream_id);
	atomic_set(&ctx->watchdog_dump, 0);

	/* delay timer */
	mod_timer(&ctx->watchdog_timer,
		  jiffies + msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL *
					     ctx->watchdog_timeout_tg));
	atomic_long_set(&ctx->watchdog_prev, jiffies);
}

static void mtk_ctx_watchdog_init(struct mtk_cam_ctx *ctx)
{
	INIT_WORK(&ctx->watchdog_work, mtk_cam_ctx_watchdog_worker);
	timer_setup(&ctx->watchdog_timer, mtk_ctx_watchdog_callback, 0);
}

static void mtk_ctx_watchdog_start(struct mtk_cam_ctx *ctx, int timeout_tg)
{
	dev_info(ctx->cam->dev,
		 "%s:ctx(%d):start the watchdog, timeout setting(%dms)\n",
		 __func__, ctx->stream_id,
		 MTK_CAM_CTX_WATCHDOG_INTERVAL * timeout_tg);

	ctx->watchdog_timeout_tg = timeout_tg;
	atomic_set(&ctx->watchdog_dump, 0);

	mod_timer(&ctx->watchdog_timer,
		  jiffies + msecs_to_jiffies(MTK_CAM_CTX_WATCHDOG_INTERVAL *
					     ctx->watchdog_timeout_tg));
	atomic_long_set(&ctx->watchdog_prev, jiffies);
}

static void mtk_ctx_watchdog_stop(struct mtk_cam_ctx *ctx)
{
	dev_info(ctx->cam->dev, "%s:ctx(%d):stop the watchdog\n",
		 __func__, ctx->stream_id);
	del_timer_sync(&ctx->watchdog_timer);
}

struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity)
{
	unsigned int i;

	for (i = 0;  i < cam->max_stream_num; i++) {
		if (media_entity_pipeline(entity) == &cam->ctxs[i].pipeline)
			return &cam->ctxs[i];
	}

	return NULL;
}

static int __maybe_unused mtk_cam_pm_suspend(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_ctx *ctx = cam_dev->ctxs;
	struct mtk_raw_device *raw_dev;
	unsigned int val;
	int ret;

	dev_dbg(dev, "%s: enter\n", __func__);

	mtk_ctx_watchdog_stop(ctx);

	if (!ctx->streaming || !ctx->pipe) {
		dev_info(dev, "%s: camera not streaming, skip TG/VF disable\n",
			 __func__);
		return 0;
	}

	raw_dev = get_main_raw_dev(cam_dev, ctx->pipe);
	if (!raw_dev || !raw_dev->base) {
		dev_info(dev, "%s: no active raw device\n", __func__);
		return 0;
	}

	if (pm_runtime_suspended(raw_dev->dev)) {
		dev_info(dev, "%s: raw device already runtime suspended\n",
			 __func__);
		return 0;
	}

	dev_dbg(dev, "%s: disabling TG/VF for raw_dev %d\n",
		 __func__, raw_dev->id);

	val = readl(raw_dev->base + REG_TG_VF_CON);
	writel(val & (~TG_VFDATA_EN), raw_dev->base + REG_TG_VF_CON);

	ret = readl_poll_timeout_atomic(raw_dev->base + REG_TG_INTER_ST, val,
					(val & TG_CAM_CS_MASK) == TG_IDLE_ST,
					USEC_PER_MSEC, MTK_RAW_STOP_HW_TIMEOUT);
	if (ret)
		dev_warn(dev, "%s: TG idle timeout: 0x%x\n", __func__, val);

	val = readl(raw_dev->base + REG_TG_SEN_MODE);
	writel(val & (~TG_SEN_MODE_CMOS_EN), raw_dev->base + REG_TG_SEN_MODE);

	dev_dbg(dev, "%s: TG/VF disabled, TG_VF_CON=0x%x TG_SEN_MODE=0x%x\n",
		 __func__,
		 readl(raw_dev->base + REG_TG_VF_CON),
		 readl(raw_dev->base + REG_TG_SEN_MODE));

	if (ctx->pipe->res_config.raw_num_used > 1) {
		struct mtk_raw_device *raw_dev_sub =
			get_sub_raw_dev(cam_dev, ctx->pipe);

		if (raw_dev_sub && raw_dev_sub->base &&
		    !pm_runtime_suspended(raw_dev_sub->dev)) {
			val = readl(raw_dev_sub->base + REG_TG_VF_CON);
			writel(val & (~TG_VFDATA_EN),
			       raw_dev_sub->base + REG_TG_VF_CON);

			val = readl(raw_dev_sub->base + REG_TG_SEN_MODE);
			writel(val & (~TG_SEN_MODE_CMOS_EN),
			       raw_dev_sub->base + REG_TG_SEN_MODE);

			dev_dbg(dev, "%s: disabled TG/VF for sub raw_dev %d\n",
				 __func__, raw_dev_sub->id);
		}

		if (ctx->pipe->res_config.raw_num_used == 3) {
			struct mtk_raw_device *raw_dev_sub2 =
				get_sub2_raw_dev(cam_dev, ctx->pipe);

			if (raw_dev_sub2 && raw_dev_sub2->base &&
			    !pm_runtime_suspended(raw_dev_sub2->dev)) {
				val = readl(raw_dev_sub2->base + REG_TG_VF_CON);
				writel(val & (~TG_VFDATA_EN),
				       raw_dev_sub2->base + REG_TG_VF_CON);

				val = readl(raw_dev_sub2->base + REG_TG_SEN_MODE);
				writel(val & (~TG_SEN_MODE_CMOS_EN),
				       raw_dev_sub2->base + REG_TG_SEN_MODE);

				dev_dbg(dev, "%s: disabled TG/VF for sub2 raw_dev %d\n",
					 __func__, raw_dev_sub2->id);
			}
		}
	}

	return 0;
}

static int __maybe_unused mtk_cam_pm_resume(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct mtk_cam_ctx *ctx = cam_dev->ctxs;
	struct mtk_raw_device *raw_dev;
	unsigned int val;

	dev_dbg(dev, "%s: enter\n", __func__);

	if (!ctx->streaming || !ctx->pipe) {
		dev_info(dev, "%s: camera not streaming, skip TG/VF enable\n",
			 __func__);
		goto restart_watchdog;
	}

	raw_dev = get_main_raw_dev(cam_dev, ctx->pipe);
	if (!raw_dev || !raw_dev->base) {
		dev_info(dev, "%s: no active raw device\n", __func__);
		goto restart_watchdog;
	}

	if (pm_runtime_suspended(raw_dev->dev)) {
		dev_info(dev, "%s: raw device runtime suspended\n", __func__);
		goto restart_watchdog;
	}

	dev_dbg(dev, "%s: enabling TG/VF for raw_dev %d\n",
		 __func__, raw_dev->id);

	val = readl(raw_dev->base + REG_TG_SEN_MODE);
	writel(val | TG_SEN_MODE_CMOS_EN, raw_dev->base + REG_TG_SEN_MODE);

	val = readl(raw_dev->base + REG_TG_VF_CON);
	writel(val | TG_VFDATA_EN, raw_dev->base + REG_TG_VF_CON);

	dev_dbg(dev, "%s: TG/VF enabled, TG_VF_CON=0x%x TG_SEN_MODE=0x%x\n",
		 __func__,
		 readl(raw_dev->base + REG_TG_VF_CON),
		 readl(raw_dev->base + REG_TG_SEN_MODE));

	if (ctx->pipe->res_config.raw_num_used > 1) {
		struct mtk_raw_device *raw_dev_sub =
			get_sub_raw_dev(cam_dev, ctx->pipe);

		if (raw_dev_sub && raw_dev_sub->base &&
		    !pm_runtime_suspended(raw_dev_sub->dev)) {
			val = readl(raw_dev_sub->base + REG_TG_SEN_MODE);
			writel(val | TG_SEN_MODE_CMOS_EN,
			       raw_dev_sub->base + REG_TG_SEN_MODE);

			val = readl(raw_dev_sub->base + REG_TG_VF_CON);
			writel(val | TG_VFDATA_EN,
			       raw_dev_sub->base + REG_TG_VF_CON);

			dev_dbg(dev, "%s: enabled TG/VF for sub raw_dev %d\n",
				 __func__, raw_dev_sub->id);
		}

		if (ctx->pipe->res_config.raw_num_used == 3) {
			struct mtk_raw_device *raw_dev_sub2 =
				get_sub2_raw_dev(cam_dev, ctx->pipe);

			if (raw_dev_sub2 && raw_dev_sub2->base &&
			    !pm_runtime_suspended(raw_dev_sub2->dev)) {
				val = readl(raw_dev_sub2->base + REG_TG_SEN_MODE);
				writel(val | TG_SEN_MODE_CMOS_EN,
				       raw_dev_sub2->base + REG_TG_SEN_MODE);

				val = readl(raw_dev_sub2->base + REG_TG_VF_CON);
				writel(val | TG_VFDATA_EN,
				       raw_dev_sub2->base + REG_TG_VF_CON);

				dev_dbg(dev, "%s: enabled TG/VF for sub2 raw_dev %d\n",
					 __func__, raw_dev_sub2->id);
			}
		}
	}

restart_watchdog:
	if (watchdog_scenario(ctx)) {
		dev_info(dev, "%s: restarting watchdog\n", __func__);
		mtk_ctx_watchdog_start(ctx, 4);
	}

	return 0;
}

static int __maybe_unused mtk_cam_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int __maybe_unused mtk_cam_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node)
{
	struct mtk_cam_ctx *ctx = node->ctx;
	struct device *dev;
	struct v4l2_subdev **target_sd;
	int ret, i, is_first_ctx;
	struct media_entity *entity = &node->vdev.entity;
	struct media_graph graph;

	dev_info(cam->dev, "%s:ctx(%d): triggered by %s\n",
		 __func__, ctx->stream_id, entity->name);

	atomic_set(&ctx->enqueued_frame_seq_no, 0);
	ctx->composed_frame_seq_no = 0;
	ctx->dequeued_frame_seq_no = 0;
	atomic_set(&ctx->running_s_data_cnt, 0);
	init_completion(&ctx->session_complete);
	init_completion(&ctx->m2m_complete);

	is_first_ctx = !cam->composer_cnt;
	if (is_first_ctx) {
		spin_lock(&cam->dma_processing_lock);
		cam->dma_processing_count = 0;
		spin_unlock(&cam->dma_processing_lock);
		spin_lock(&cam->running_job_lock);
		cam->running_job_count = 0;
		spin_unlock(&cam->running_job_lock);

		dev_info(cam->dev, "%s: power on camsys\n", __func__);
		ret = pm_runtime_resume_and_get(cam->dev);
		if (ret < 0) {
			dev_info(cam->dev, "%s: power on camsys failed\n", __func__);
			return NULL;
		}

		ret = isp_composer_init(cam);
		if (ret)
			goto fail_shutdown;

		/* To catch camsys exception and trigger dump */
		if (cam->debug_fs)
			cam->debug_fs->ops->exp_reinit(cam->debug_fs);
	}
	cam->composer_cnt++;
	if (is_yuv_node(node->desc.id))
		dev = cam->raw.yuvs[0];
	else
		dev = cam->raw.devs[0];

	ret = mtk_cam_working_buf_pool_init(ctx, dev);
	if (ret) {
		dev_info(cam->dev, "failed to reserve DMA memory:%d\n", ret);
		goto fail_uninit_composer;
	}

	kthread_init_worker(&ctx->sensor_worker);
	ctx->sensor_worker_task = kthread_run(kthread_worker_fn,
					      &ctx->sensor_worker,
					      "sensor_worker-%d",
					      ctx->stream_id);
	if (IS_ERR(ctx->sensor_worker_task)) {
		dev_info(cam->dev, "%s:ctx(%d): could not create sensor_worker_task\n",
			 __func__, ctx->stream_id);
		goto fail_release_buffer_pool;
	}

	sched_set_fifo(ctx->sensor_worker_task);

	ctx->composer_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->composer_wq) {
		dev_info(cam->dev, "failed to alloc composer workqueue\n");
		goto fail_uninit_sensor_worker_task;
	}

	ctx->frame_done_wq =
			alloc_ordered_workqueue(dev_name(cam->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!ctx->frame_done_wq) {
		dev_info(cam->dev, "failed to alloc frame_done workqueue\n");
		goto fail_uninit_composer_wq;
	}

	ret = media_pipeline_start(&entity->pads[0], &ctx->pipeline);
	if (ret) {
		dev_warn(cam->dev,
			 "%s:pipe(%d):failed in media_pipeline_start:%d\n",
			 __func__, node->uid.pipe_id, ret);
		goto fail_uninit_frame_done_wq;
	}

	/* traverse to update used subdevs & number of nodes */
	i = 0;
	ret = media_graph_walk_init(&graph, entity->graph_obj.mdev);
	if (ret)
		goto fail_stop_pipeline;

	media_graph_walk_start(&graph, entity);
	while ((entity = media_graph_walk_next(&graph))) {
		dev_dbg(cam->dev, "linked entity %s\n", entity->name);

		target_sd = NULL;

		switch (entity->function) {
		case MEDIA_ENT_F_IO_V4L:
			ctx->enabled_node_cnt++;
			break;
		case MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER: /* pipeline */
			if (i >= MAX_PIPES_PER_STREAM)
				goto fail_stop_pipeline;
			target_sd = ctx->pipe_subdevs + i;
			i++;
			break;
		case MEDIA_ENT_F_VID_IF_BRIDGE: /* seninf */
			target_sd = &ctx->seninf;
			break;
		case MEDIA_ENT_F_CAM_SENSOR:
			target_sd = &ctx->sensor;
			break;
		default:
			break;
		}

		if (!target_sd)
			continue;

		if (*target_sd) {
			dev_info(cam->dev, "duplicated subdevs!!!\n");
			goto fail_traverse_subdev;
		}

		if (is_media_entity_v4l2_subdev(entity))
			*target_sd = media_entity_to_v4l2_subdev(entity);
	}
	media_graph_walk_cleanup(&graph);

	return ctx;

fail_traverse_subdev:
	media_graph_walk_cleanup(&graph);
fail_stop_pipeline:
	media_pipeline_stop(&entity->pads[0]);
fail_uninit_frame_done_wq:
	destroy_workqueue(ctx->frame_done_wq);
fail_uninit_composer_wq:
	destroy_workqueue(ctx->composer_wq);
fail_uninit_sensor_worker_task:
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;
fail_release_buffer_pool:
	mtk_cam_working_buf_pool_release(ctx, dev);
fail_uninit_composer:
	isp_composer_uninit(cam);
	cam->composer_cnt--;
fail_shutdown:
	if (is_first_ctx)
		rproc_shutdown(cam->rproc_handle);

	return NULL;
}

void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct mtk_cam_video_device *node)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct media_entity *entity = &node->vdev.entity;
	struct device *dev;
	unsigned int i;

	if (is_yuv_node(node->desc.id))
		dev = cam->raw.yuvs[0];
	else
		dev = cam->raw.devs[0];

	dev_info(cam->dev, "%s:ctx(%d): triggered by %s\n",
		 __func__, ctx->stream_id, entity->name);

	media_pipeline_stop(&entity->pads[0]);

	if (ctx->session_created) {
		dev_dbg(cam->dev,
			"%s:ctx(%d): session_created, wait for composer session destroy\n",
			__func__, ctx->stream_id);
		if (wait_for_completion_timeout(&ctx->session_complete,
						msecs_to_jiffies(300)) == 0)
			dev_info(cam->dev, "%s:ctx(%d): complete timeout\n",
				 __func__, ctx->stream_id);
	}

	/* For M2M feature, signal all waiters */
	if (mtk_cam_is_m2m(ctx))
		complete_all(&ctx->m2m_complete);

	if (!cam->streaming_ctx) {
		struct v4l2_subdev *sd;

		v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
			if (sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE) {
				int ret;

				ret = v4l2_subdev_call(sd, video, s_stream, 0);
				if (ret)
					dev_err(cam->dev,
						"failed to streamoff %s:%d\n",
						sd->name, ret);
			}
		}
	}

	drain_workqueue(ctx->composer_wq);
	destroy_workqueue(ctx->composer_wq);
	ctx->composer_wq = NULL;
	drain_workqueue(ctx->frame_done_wq);
	destroy_workqueue(ctx->frame_done_wq);
	ctx->frame_done_wq = NULL;
	kthread_flush_worker(&ctx->sensor_worker);
	kthread_stop(ctx->sensor_worker_task);
	ctx->sensor_worker_task = NULL;
	ctx->session_created = 0;
	ctx->enabled_node_cnt = 0;
	ctx->streaming_node_cnt = 0;
	ctx->streaming_pipe = 0;
	ctx->sensor = NULL;
	ctx->seninf = NULL;
	atomic_set(&ctx->enqueued_frame_seq_no, 0);
	ctx->composed_frame_seq_no = 0;
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	ctx->used_raw_num = 0;

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);

	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	for (i = 0; i < MAX_PIPES_PER_STREAM; i++)
		ctx->pipe_subdevs[i] = NULL;

	isp_composer_uninit(cam);
		cam->composer_cnt--;

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d\n",
		 __func__, ctx->stream_id, cam->composer_cnt);

	mtk_cam_working_buf_pool_release(ctx, dev);

	if (ctx->cam->rproc_handle && !ctx->cam->composer_cnt) {
		dev_info(cam->dev, "%s power off camsys\n", __func__);
		pm_runtime_put_sync(cam->dev);
		rproc_shutdown(cam->rproc_handle);
	}
}

static int pipeid_to_tgidx(int pipe_id)
{
	switch (pipe_id) {
	case MTKCAM_SUBDEV_RAW_0:
		return 0;
	case MTKCAM_SUBDEV_RAW_1:
		return 1;
	case MTKCAM_SUBDEV_RAW_2:
		return 2;
	default:
		break;
	}
	return -1;
}

int mtk_cam_call_seninf_set_pixelmode(struct mtk_cam_ctx *ctx,
				      struct v4l2_subdev *sd,
				      int pad_id, int pixel_mode)
{
	int ret;

	ret = mtk_cam_seninf_set_pixelmode(sd, pad_id, pixel_mode);
	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d): seninf(%s): pad(%d), pixel_mode(%d)\n, ret(%d)",
		__func__, ctx->stream_id, sd->name, pad_id, pixel_mode,
		ret);

	return ret;
}

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx, struct mtk_cam_video_device *node)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	int i, ret;
	int tgo_pxl_mode;
	bool need_dump_mem = false;
	unsigned int streaming_ctx_latch;

	dev_dbg(cam->dev, "ctx %d stream on, streaming_pipe:0x%x\n",
		ctx->stream_id, ctx->streaming_pipe);

	if (ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming on\n", ctx->stream_id);
		return 0;
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 1);
		if (ret) {
			dev_info(cam->dev, "failed to stream on %s: %d\n",
				 ctx->pipe_subdevs[i]->name, ret);
			goto fail_pipe_off;
		}
	}

	if (ctx->used_raw_num) {
		tgo_pxl_mode = ctx->pipe->res_config.tgo_pxl_mode;

		ret = mtk_cam_dev_config(ctx);
		if (ret)
			goto fail_pipe_off;
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamon raw device not found\n");
			goto fail_pipe_off;
		}
		raw_dev = dev_get_drvdata(dev);

		if (!mtk_cam_is_m2m(ctx)) {
			mtk_cam_call_seninf_set_pixelmode(ctx, ctx->seninf,
							  PAD_SRC_RAW0,
							  tgo_pxl_mode);
			mtk_cam_seninf_set_camtg(ctx->seninf, PAD_SRC_RAW0,
						 pipeid_to_tgidx(raw_dev->id));
		}
	}

	if (!mtk_cam_is_m2m(ctx)) {
		ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 1);
		if (ret) {
			dev_info(cam->dev, "failed to stream on seninf %s:%d\n",
				 ctx->seninf->name, ret);
			goto fail_pipe_off;
		}
	} else {
		ctx->processing_buffer_list.cnt = 0;
		ctx->composed_buffer_list.cnt = 0;
		dev_dbg(cam->dev, "[M2M] reset processing_buffer_list.cnt & composed_buffer_list.cnt\n");
	}
	if (ctx->used_raw_num) {
		mtk_cam_raw_initialize(raw_dev, 0);
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_sub =
			get_sub_raw_dev(cam, ctx->pipe);
			mtk_cam_raw_initialize(raw_dev_sub, 1);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_sub2 =
					get_sub2_raw_dev(cam, ctx->pipe);
				mtk_cam_raw_initialize(raw_dev_sub2, 1);
			}
		}
	}

	spin_lock(&ctx->streaming_lock);
	if (!cam->streaming_ctx && cam->debug_fs)
		need_dump_mem = true;

	streaming_ctx_latch = cam->streaming_ctx;
	ctx->streaming = true;
	cam->streaming_ctx |= 1 << ctx->stream_id;
	spin_unlock(&ctx->streaming_lock);

	if (need_dump_mem)
		cam->debug_fs->ops->reinit(cam->debug_fs, ctx->stream_id);
	else
		dev_dbg(cam->dev,
			"No need to alloc mem for ctx: streaming_ctx(0x%x), debug_fs(%p)\n",
			streaming_ctx_latch, cam->debug_fs);
	ret = mtk_camsys_ctrl_start(ctx);
	if (ret)
		goto fail_streaming_off;

	mutex_lock(&cam->queue_lock);
	mtk_cam_dev_req_try_queue(cam);  /* request moved into working list */
	mutex_unlock(&cam->queue_lock);
	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_start(ctx, 4);

	dev_dbg(cam->dev, "streamed on camsys ctx:%d\n", ctx->stream_id);

	return 0;

fail_streaming_off:
	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);
	if (!mtk_cam_is_m2m(ctx))
		v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
fail_pipe_off:
	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++)
		v4l2_subdev_call(ctx->pipe_subdevs[i], video, s_stream, 0);

	return ret;
}

int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx, struct mtk_cam_video_device *node)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev;
	struct mtk_raw_device *raw_dev;
	unsigned int i;
	int ret;

	if (!ctx->streaming) {
		dev_dbg(cam->dev, "ctx-%d is already streaming off\n",
			ctx->stream_id);
		return 0;
	}

	if (watchdog_scenario(ctx))
		mtk_ctx_watchdog_stop(ctx);

	dev_info(cam->dev, "%s: ctx-%d:  composer_cnt:%d, streaming_pipe:0x%x\n",
		 __func__, ctx->stream_id, cam->composer_cnt, ctx->streaming_pipe);

	spin_lock(&ctx->streaming_lock);
	ctx->streaming = false;
	cam->streaming_ctx &= ~(1 << ctx->stream_id);
	spin_unlock(&ctx->streaming_lock);

	if (ctx->synced)
		ctx->synced = 0;

	if (!mtk_cam_is_m2m(ctx)) {
		ret = v4l2_subdev_call(ctx->seninf, video, s_stream, 0);
		if (ret) {
			dev_err(cam->dev, "failed to stream off %s:%d\n",
				ctx->seninf->name, ret);
			return -EPERM;
		}
	}

	if (ctx->used_raw_num) {
		dev = mtk_cam_find_raw_dev(cam, ctx->used_raw_dev);
		if (!dev) {
			dev_info(cam->dev, "streamoff raw device not found\n");
			goto fail_stream_off;
		}
		raw_dev = dev_get_drvdata(dev);
		mtk_cam_raw_stream_on(raw_dev, 0);
		/* Twin */
		if (ctx->pipe->res_config.raw_num_used != 1) {
			struct mtk_raw_device *raw_dev_sub =
						get_sub_raw_dev(cam, ctx->pipe);
			mtk_cam_raw_stream_on(raw_dev_sub, 0);
			if (ctx->pipe->res_config.raw_num_used == 3) {
				struct mtk_raw_device *raw_dev_sub2 =
					get_sub2_raw_dev(cam, ctx->pipe);
				mtk_cam_raw_stream_on(raw_dev_sub2, 0);
			}
		}
	}

	for (i = 0; i < MAX_PIPES_PER_STREAM && ctx->pipe_subdevs[i]; i++) {
		ret = v4l2_subdev_call(ctx->pipe_subdevs[i], video,
				       s_stream, 0);
		if (ret) {
			dev_err(cam->dev, "failed to stream off %s: %d\n",
				ctx->pipe_subdevs[i]->name, ret);
			return -EPERM;
		}
	}

	if (ctx->img_buf_pool.working_img_buf_size > 0) {
		if (is_yuv_node(node->desc.id))
			dev = cam->raw.yuvs[0];
		else
			dev = cam->raw.devs[0];

		mtk_cam_img_working_buf_pool_release(ctx, dev);
	}

	mtk_camsys_ctrl_stop(ctx);

fail_stream_off:
	if (ctx->used_raw_num)
		isp_composer_destroy_session(ctx);

	dev_dbg(cam->dev, "streamed off camsys ctx:%d\n", ctx->stream_id);

	return 0;
}

static int config_bridge_pad_links(struct mtk_cam_device *cam,
				   struct v4l2_subdev *seninf)
{
	struct media_entity *pipe_entity;
	unsigned int i;
	int ret;

	for (i = MTKCAM_SUBDEV_RAW_START;
	     i < MTKCAM_SUBDEV_RAW_START + cam->num_raw_devices; i++) {
		pipe_entity = &cam->raw.pipelines[i].subdev.entity;

		dev_info(cam->dev, "create pad link %s %s\n",
			 seninf->entity.name, pipe_entity->name);

		ret = media_create_pad_link(&seninf->entity,
					    MTK_CAM_CIO_PAD_SRC,
					    pipe_entity,
					    MTK_CAM_CIO_PAD_SINK,
					    MEDIA_LNK_FL_DYNAMIC);

		if (ret) {
			dev_warn(cam->dev,
				 "failed to create pad link %s %s err:%d\n",
				 seninf->entity.name, pipe_entity->name,
				 ret);
			return ret;
		}
	}

	return 0;
}

static int mtk_cam_create_links(struct mtk_cam_device *cam)
{
	struct v4l2_subdev *sd;
	int ret = 0;

	v4l2_device_for_each_subdev(sd, &cam->v4l2_dev) {
		if (sd->entity.function == MEDIA_ENT_F_VID_IF_BRIDGE)
			ret = config_bridge_pad_links(cam, sd);
	}

	return ret;
}

static int mtk_cam_master_register(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct media_device *media_dev = &cam_dev->media_dev;
	int ret;

	dev_info(dev, "camsys | start %s\n", __func__);

	media_dev->dev = cam_dev->dev;
	strscpy(media_dev->model, dev_driver_string(dev),
		sizeof(media_dev->model));
	snprintf(media_dev->bus_info, sizeof(media_dev->bus_info),
		 "platform:%s", dev_name(dev));
	media_dev->hw_revision = 0;
	media_dev->ops = &mtk_cam_dev_ops;
	media_device_init(media_dev);

	cam_dev->v4l2_dev.mdev = media_dev;
	ret = v4l2_device_register(cam_dev->dev, &cam_dev->v4l2_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register V4L2 device: %d\n", ret);
		goto fail_media_device_cleanup;
	}

	ret = media_device_register(media_dev);
	if (ret) {
		dev_dbg(dev, "Failed to register media device: %d\n",
			ret);
		goto fail_v4l2_device_unreg;
	}

	dev_info(dev, "%s success\n", __func__);
	return 0;

fail_v4l2_device_unreg:
	v4l2_device_unregister(&cam_dev->v4l2_dev);

fail_media_device_cleanup:
	media_device_cleanup(&cam_dev->media_dev);

	return ret;
}

static void mtk_cam_master_unregister(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	dev_info(dev, "camsys | start %s\n", __func__);

	media_device_unregister(&cam_dev->media_dev);
	v4l2_device_unregister(&cam_dev->v4l2_dev);
	media_device_cleanup(&cam_dev->media_dev);
}

static int mtk_cam_async_add_by_driver(struct device *dev,
				       struct platform_driver *drv,
				       struct v4l2_async_notifier *notifier)
{
	struct fwnode_handle *fwnode;
	struct device *p = NULL;
	struct v4l2_async_subdev *asd;
	int dev_num = 0;

	p = platform_find_device_by_driver(NULL, &drv->driver);
	while (p) {
		dev_info(dev, "camsys | %s add %s\n", __func__, p->kobj.name);

		fwnode = dev_fwnode(p);
		asd = v4l2_async_nf_add_fwnode(notifier, fwnode,
					       struct v4l2_async_subdev);
		put_device(p);

		if (IS_ERR(asd)) {
			dev_info(dev, "%s add fwnode fail %ld\n", __func__,
				 PTR_ERR(asd));

			return PTR_ERR(asd);
		}
		++dev_num;

		p = platform_find_device_by_driver(p, &drv->driver);
	}

	return dev_num;
}

static int mtk_cam_async_subdev_add(struct device *dev)
{
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);
	struct v4l2_async_notifier *notifier = &cam_dev->notifier;
	int raw_num, yuv_num, seninf_num;

	raw_num = mtk_cam_async_add_by_driver(dev, &mtk_cam_raw_driver,
					      notifier);
	yuv_num = mtk_cam_async_add_by_driver(dev, &mtk_cam_yuv_driver,
					      notifier);
	seninf_num = mtk_cam_async_add_by_driver(dev, &seninf_pdrv, notifier);

	if (raw_num < 0 || yuv_num < 0 || seninf_num < 0) {
		dev_err(dev, "%s failed\n", __func__);
		return -ENODEV;
	}

	cam_dev->num_raw_devices = raw_num;
	cam_dev->num_seninf_devices = seninf_num;
	dev_info(dev, "dependent module #: raw %d, yuv %d, seninf %d\n",
		 cam_dev->num_raw_devices, yuv_num,
		 cam_dev->num_seninf_devices);

	return 0;
}

static void mtk_cam_ctx_init(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_device *cam,
			     unsigned int stream_id)
{
	ctx->cam = cam;
	ctx->stream_id = stream_id;
	ctx->sensor = NULL;

	ctx->streaming_pipe = 0;
	ctx->streaming_node_cnt = 0;

	ctx->used_raw_num = 0;
	ctx->used_raw_dev = 0;
	ctx->processing_buffer_list.cnt = 0;
	ctx->composed_buffer_list.cnt = 0;
	ctx->is_first_cq_done = 0;
	ctx->cq_done_status = 0;
	ctx->session_created = 0;

	INIT_LIST_HEAD(&ctx->using_buffer_list.list);
	INIT_LIST_HEAD(&ctx->composed_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_buffer_list.list);
	INIT_LIST_HEAD(&ctx->processing_img_buffer_list.list);
	spin_lock_init(&ctx->using_buffer_list.lock);
	spin_lock_init(&ctx->composed_buffer_list.lock);
	spin_lock_init(&ctx->processing_buffer_list.lock);
	spin_lock_init(&ctx->streaming_lock);
	spin_lock_init(&ctx->first_cq_lock);
	spin_lock_init(&ctx->processing_img_buffer_list.lock);

	mtk_ctx_watchdog_init(ctx);
}

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt)
{
	int ret = 0;

	/* The width, height and code must match. */
	if (source_fmt->format.width != sink_fmt->format.width) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: width does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.width, sink_fmt->format.width);
		ret = -EPIPE;
	}

	if (source_fmt->format.height != sink_fmt->format.height) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: height does not match (source %u, sink %u)\n",
			__func__,
			source_fmt->format.height, sink_fmt->format.height);
		ret = -EPIPE;
	}

	if (source_fmt->format.code != sink_fmt->format.code) {
		dev_err(sd->entity.graph_obj.mdev->dev,
			"%s: warn: media bus code does not match (source 0x%8.8x, sink 0x%8.8x)\n",
			__func__,
			source_fmt->format.code, sink_fmt->format.code);
		ret = -EPIPE;
	}

	dev_dbg(sd->entity.graph_obj.mdev->dev,
		"%s: link was \"%s\":%u -> \"%s\":%u\n", __func__,
		link->source->entity->name, link->source->index,
		link->sink->entity->name, link->sink->index);

	if (ret)
		dev_info(sd->v4l2_dev->dev, "%s: link validate failed pad/code/w/h: SRC(%d/0x%x/%d/%d), SINK(%d:0x%x/%d/%d)\n",
			 sd->name, source_fmt->pad, source_fmt->format.code,
			 source_fmt->format.width, source_fmt->format.height,
			 sink_fmt->pad, sink_fmt->format.code,
			 sink_fmt->format.width, sink_fmt->format.height);

	return ret;
}

static int mtk_cam_debug_fs_init(struct mtk_cam_device *cam)
{
	/**
	 * The dump buffer size depdends on the meta buffer size
	 * which is variable among devices using different type of sensors
	 * , e.g. PD's statistic buffers.
	 */
	u32 dump_mem_size = MTK_CAM_DEBUG_DUMP_HEADER_MAX_SIZE +
			    CQ_BUF_SIZE +
			    camsys_get_meta_size(MTKCAM_IPI_RAW_META_STATS_CFG) +
			    RAW_STATS_CFG_VARIOUS_SIZE +
			    sizeof(struct mtkcam_ipi_frame_param) +
			    sizeof(struct mtkcam_ipi_config_param) *
			    RAW_PIPELINE_NUM;

	cam->debug_fs = mtk_cam_get_debugfs();
	if (!cam->debug_fs)
		return 0;

	cam->debug_fs->ops->init(cam->debug_fs, cam, dump_mem_size);
	cam->debug_wq = alloc_ordered_workqueue(dev_name(cam->dev),
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE);
	if (!cam->debug_wq)
		return -EINVAL;

	cam->debug_exception_wq = alloc_ordered_workqueue(dev_name(cam->dev),
							  __WQ_LEGACY |
							  WQ_MEM_RECLAIM |
							  WQ_FREEZABLE);
	if (!cam->debug_exception_wq) {
		destroy_workqueue(cam->debug_wq);
		return -EINVAL;
	}

	init_waitqueue_head(&cam->debug_exception_waitq);

	return 0;
}

static void mtk_cam_debug_fs_deinit(struct mtk_cam_device *cam)
{
	if (!cam->debug_fs)
		return;

	drain_workqueue(cam->debug_wq);
	destroy_workqueue(cam->debug_wq);
	drain_workqueue(cam->debug_exception_wq);
	destroy_workqueue(cam->debug_exception_wq);
	cam->debug_fs->ops->deinit(cam->debug_fs);
}

static int register_sub_drivers(struct device *dev)
{
	int ret;

	ret = platform_driver_register(&seninf_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_pdrv fail\n", __func__);
		goto REGISTER_SENINF_FAIL;
	}

	ret = platform_driver_register(&seninf_core_pdrv);
	if (ret) {
		dev_info(dev, "%s seninf_core_pdrv fail\n", __func__);
		goto REGISTER_SENINF_CORE_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_raw_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_RAW_FAIL;
	}

	ret = platform_driver_register(&mtk_cam_yuv_driver);
	if (ret) {
		dev_info(dev, "%s mtk_cam_raw_driver fail\n", __func__);
		goto REGISTER_YUV_FAIL;
	}

	ret = mtk_cam_master_register(dev);
	if (ret) {
		dev_err(dev, "%s mtk_cam_master_register fail\n", __func__);
		goto ADD_CAM_MASTER_FAIL;
	}

	return 0;

ADD_CAM_MASTER_FAIL:
	platform_driver_unregister(&mtk_cam_yuv_driver);

REGISTER_YUV_FAIL:
	platform_driver_unregister(&mtk_cam_raw_driver);

REGISTER_RAW_FAIL:
	platform_driver_unregister(&seninf_core_pdrv);

REGISTER_SENINF_CORE_FAIL:
	platform_driver_unregister(&seninf_pdrv);

REGISTER_SENINF_FAIL:
	return ret;
}

static void unregister_sub_drivers(struct device *dev)
{
	mtk_cam_master_unregister(dev);

	platform_driver_unregister(&mtk_cam_yuv_driver);
	platform_driver_unregister(&mtk_cam_raw_driver);
	platform_driver_unregister(&seninf_core_pdrv);
	platform_driver_unregister(&seninf_pdrv);
}

static int mtk_cam_master_complete(struct v4l2_async_notifier *notifier)
{
	struct mtk_cam_device *cam_dev =
		container_of(notifier, struct mtk_cam_device, notifier);
	struct device *dev = cam_dev->dev;
	int ret;

	dev_info(dev, "cmasys | trigger %s\n", __func__);

	/* set raw and yuv internal */
	ret = mtk_cam_raw_setup_dependencies(&cam_dev->raw);
	if (ret) {
		dev_err(dev, "Failed to mtk_cam_raw_setup_dependencies: %d\n", ret);
		goto fail_unbind_all;
	}

	/* register raw subdev */
	ret = mtk_cam_raw_register_entities(&cam_dev->raw, &cam_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to init raw subdevs: %d\n", ret);
		goto fail_remove_dependencies;
	}

	mtk_cam_create_links(cam_dev);

	/* Expose all subdev's nodes */
	ret = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register subdev nodes\n");
		goto fail_unreg_raw_entities;
	}

	dev_info(dev, "%s success\n", __func__);

	return 0;

fail_unreg_raw_entities:
	mtk_cam_raw_unregister_entities(&cam_dev->raw);

fail_remove_dependencies:
	/* nothing to do for now */

fail_unbind_all:
	return ret;
}

static int mtk_cam_master_bound(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	struct mtk_cam_device *cam_dev =
		container_of(notifier, struct mtk_cam_device, notifier);
	struct mtk_raw *raw = &cam_dev->raw;
	struct device *cam = cam_dev->dev;
	struct device *dev = subdev->dev;

	dev_info(cam, "cmasys | %s trigger %s\n", subdev->name, __func__);

	if (strcmp(dev_driver_string(dev), "seninf") == 0) {
		dev_dbg(cam, "%s@(seninf) done\n", __func__);
	} else if (strcmp(dev_driver_string(dev), "mtk-cam raw") == 0) {
		struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);

		raw_dev->cam = cam_dev;
		raw->devs[raw_dev->id] = dev;
		raw->cam_dev = cam_dev->dev;
		dev_dbg(cam, "%s@(mtk-cam raw) done\n", __func__);
	} else if (strcmp(dev_driver_string(dev), "mtk-cam yuv") == 0) {
		struct mtk_yuv_device *yuv_dev = dev_get_drvdata(dev);

		raw->yuvs[yuv_dev->id] = dev;
		dev_dbg(cam, "%s@(mtk-cam yuv) done\n", __func__);
	} else {
		dev_warn(cam, "%s got unrecongized device\n", __func__);
	}

	return 0;
}

static void mtk_cam_master_unbound(struct v4l2_async_notifier *notifier,
				   struct v4l2_subdev *subdev,
				   struct v4l2_async_subdev *asd)
{
	struct mtk_cam_device *cam_dev =
		container_of(notifier, struct mtk_cam_device, notifier);
	struct mtk_raw *raw = &cam_dev->raw;
	struct device *cam = cam_dev->dev;
	struct device *dev = subdev->dev;

	dev_info(cam, "cmasys | %s trigger %s\n", subdev->name, __func__);

	if (strcmp(dev_driver_string(dev), "seninf") == 0) {
		dev_dbg(cam, "%s@(seninf) done\n", __func__);
	} else if (strcmp(dev_driver_string(dev), "mtk-cam raw") == 0) {
		struct mtk_raw_device *raw_dev = dev_get_drvdata(dev);

		mtk_cam_raw_unregister_entities(&cam_dev->raw);

		raw_dev->cam = NULL;
		raw->devs[raw_dev->id] = NULL;
		raw->cam_dev = NULL;
		dev_dbg(cam, "%s@(mtk-cam raw) done\n", __func__);
	} else if (strcmp(dev_driver_string(dev), "mtk-cam yuv") == 0) {
		struct mtk_yuv_device *yuv_dev = dev_get_drvdata(dev);

		raw->yuvs[yuv_dev->id] = NULL;
		dev_dbg(cam, "%s@(mtk-cam yuv) done\n", __func__);
	} else {
		dev_warn(cam, "%s got unrecongized device\n", __func__);
	}
}

static const struct v4l2_async_notifier_operations mtk_cam_async_nf_ops = {
	.complete = mtk_cam_master_complete,
	.bound = mtk_cam_master_bound,
	.unbind = mtk_cam_master_unbound,
};

static int mtk_cam_probe(struct platform_device *pdev)
{
	struct mtk_cam_device *cam_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	unsigned int i;

	dev_dbg(dev, "camsys | start %s\n", __func__);

	/* initialize structure */
	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34))) {
		dev_err(dev, "%s: No suitable DMA available\n", __func__);
		return -EIO;
	}

	if (!dev->dma_parms) {
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}

	if (dma_set_max_seg_size(dev, UINT_MAX)) {
		dev_err(dev, "Failed to set DMA segment size\n");
		return -EIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mem\n");
		return -ENODEV;
	}

	cam_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(cam_dev->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(cam_dev->base);
	}

	cam_dev->dev = dev;
	dev_set_drvdata(dev, cam_dev);

	cam_dev->composer_cnt = 0;
	cam_dev->num_seninf_devices = 0;

	cam_dev->max_stream_num = MTKCAM_SUBDEV_MAX;
	cam_dev->ctxs = devm_kcalloc(dev, cam_dev->max_stream_num,
				     sizeof(*cam_dev->ctxs), GFP_KERNEL);
	if (!cam_dev->ctxs)
		return -ENOMEM;

	cam_dev->streaming_ctx = 0;
	for (i = 0; i < cam_dev->max_stream_num; i++)
		mtk_cam_ctx_init(cam_dev->ctxs + i, cam_dev, i);

	cam_dev->running_job_count = 0;
	spin_lock_init(&cam_dev->pending_job_lock);
	spin_lock_init(&cam_dev->running_job_lock);
	INIT_LIST_HEAD(&cam_dev->pending_job_list);
	INIT_LIST_HEAD(&cam_dev->running_job_list);

	cam_dev->dma_processing_count = 0;
	spin_lock_init(&cam_dev->dma_pending_lock);
	spin_lock_init(&cam_dev->dma_processing_lock);
	INIT_LIST_HEAD(&cam_dev->dma_pending);
	INIT_LIST_HEAD(&cam_dev->dma_processing);

	mutex_init(&cam_dev->queue_lock);

	pm_runtime_enable(dev);

	ret = mtk_cam_of_rproc(cam_dev, pdev);
	if (ret)
		goto fail_destroy_mutex;

	ret = register_sub_drivers(dev);
	if (ret) {
		dev_err(dev, "fail to register_sub_drivers\n");
		goto fail_destroy_mutex;
	}

	/* register mtk_cam as all isp subdev async parent */
	cam_dev->notifier.ops = &mtk_cam_async_nf_ops;
	v4l2_async_nf_init(&cam_dev->notifier);
	ret = mtk_cam_async_subdev_add(dev); /* wait all isp sub drivers */
	if (ret) {
		dev_err(dev, "%s failed mtk_cam_async_subdev_add\n", __func__);
		goto fail_unregister_sub_drivers;
	}

	ret = v4l2_async_nf_register(&cam_dev->v4l2_dev, &cam_dev->notifier);
	if (ret) {
		dev_err(dev, "%s async_nf_register ret:%d\n", __func__, ret);
		v4l2_async_nf_cleanup(&cam_dev->notifier);
		goto fail_unregister_sub_drivers;
	}

	ret = mtk_cam_debug_fs_init(cam_dev);
	if (ret < 0)
		goto fail_unregister_async_nf;

	dev_info(dev, "camsys | [%s] success\n", __func__);

	return 0;

fail_unregister_async_nf:
	v4l2_async_nf_unregister(&cam_dev->notifier);

fail_unregister_sub_drivers:
	unregister_sub_drivers(dev);

fail_destroy_mutex:
	mutex_destroy(&cam_dev->queue_lock);

	return ret;
}

static int mtk_cam_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_cam_device *cam_dev = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	mtk_cam_debug_fs_deinit(cam_dev);

	v4l2_async_nf_unregister(&cam_dev->notifier);

	unregister_sub_drivers(dev);

	mutex_destroy(&cam_dev->queue_lock);

	return 0;
}

static const struct dev_pm_ops mtk_cam_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_cam_pm_suspend, mtk_cam_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_cam_runtime_suspend, mtk_cam_runtime_resume,
			   NULL)
};

static struct platform_driver mtk_cam_driver = {
	.probe   = mtk_cam_probe,
	.remove  = mtk_cam_remove,
	.driver  = {
		.name  = "mtk-cam",
		.of_match_table = of_match_ptr(mtk_cam_of_ids),
		.pm     = &mtk_cam_pm_ops,
	}
};

module_platform_driver(mtk_cam_driver);

MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
