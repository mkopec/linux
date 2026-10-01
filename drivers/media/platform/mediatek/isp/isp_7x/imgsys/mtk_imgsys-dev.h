/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_DEV_H_
#define _MTK_IMGSYS_DEV_H_

#include <linux/completion.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_header_desc.h"
#include "mtk_imgsys-hw.h"
#include "mtk_imgsys-module.h"
#include "mtk-img-ipi.h"

#define MTK_IMGSYS_PIPE_ID_PREVIEW		0
#define MTK_IMGSYS_PIPE_ID_CAPTURE		1
#define MTK_IMGSYS_PIPE_ID_REPROCESS		2
#define MTK_IMGSYS_PIPE_ID_TOTAL_NUM		1

#define MTK_DIP_OUTPUT_MIN_WIDTH		1U
#define MTK_DIP_OUTPUT_MIN_HEIGHT		1U
#define MTK_DIP_OUTPUT_MAX_WIDTH		5376U
#define MTK_DIP_OUTPUT_MAX_HEIGHT		4032U
#define MTK_DIP_CAPTURE_MIN_WIDTH		1U
#define MTK_DIP_CAPTURE_MIN_HEIGHT		1U
#define MTK_DIP_CAPTURE_MAX_WIDTH		5376U
#define MTK_DIP_CAPTURE_MAX_HEIGHT		4032U

#define MTK_DIP_DEV_DIP_MEDIA_MODEL_NAME	"MTK-ISP-DIP-V4L2"
#define MTK_DIP_DEV_DIP_PREVIEW_NAME \
	MTK_DIP_DEV_DIP_MEDIA_MODEL_NAME
#define MTK_DIP_DEV_DIP_CAPTURE_NAME		"MTK-ISP-DIP-CAP-V4L2"
#define MTK_DIP_DEV_DIP_REPROCESS_NAME		"MTK-ISP-DIP-REP-V4L2"

#define MTK_IMGSYS_OPP_SET			2
#define MTK_IMGSYS_CLK_LEVEL_CNT		5

extern unsigned int nodes_num;

struct mtk_imgsys_dev_format {
	const struct mtk_imgsys_format *fmt;
	u32 align;
	u32 scan_align;
};

struct mtk_imgsys_dev_buffer {
	struct vb2_v4l2_buffer vbb;
	struct v4l2_format fmt;
	const struct mtk_imgsys_dev_format *dev_fmt;
	int pipe_job_id;
	dma_addr_t isp_daddr[VB2_MAX_PLANES];
	dma_addr_t scp_daddr[VB2_MAX_PLANES];
	u64 va_daddr[VB2_MAX_PLANES];
	__u32 dma_port;
	struct mtk_imgsys_crop crop;
	struct v4l2_rect compose;
	__u32 rotation;
	__u32 hflip;
	__u32 vflip;
	__u32 resize_ratio;
	__u32 dataofst;
	struct list_head list;
};

struct mtk_imgsys_pipe_desc {
	char *name;
	int id;
	struct mtk_imgsys_video_device_desc *queue_descs;
	int total_queues;
};

struct mtk_imgsys_video_device_desc {
	int id;
	char *name;
	u32 buf_type;
	u32 cap;
	int smem_alloc;
	int supports_ctrls;
	const struct mtk_imgsys_dev_format *fmts;
	int num_fmts;
	char *description;
	int default_width;
	int default_height;
	unsigned int dma_port;
	const struct v4l2_frmsizeenum *frmsizeenum;
	u32 flags;
};

struct mtk_imgsys_dev_queue {
	struct vb2_queue vbq;
	/* Serializes vb2 queue and video device operations */
	struct mutex lock;
	const struct mtk_imgsys_dev_format *dev_fmt;
};

struct mtk_imgsys_video_device {
	struct video_device vdev;
	struct mtk_imgsys_dev_queue dev_q;
	struct v4l2_format vdev_fmt;
	struct media_pad vdev_pad;
	struct v4l2_mbus_framefmt pad_fmt;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *resize_ratio_ctrl;
	u32 flags;
	const struct mtk_imgsys_video_device_desc *desc;
	struct list_head buf_list;
	struct v4l2_rect crop;
	struct v4l2_rect compose;
	int rotation;
	bool vflip;
	bool hflip;
	int resize_ratio;
	/* protect the in-device buffer list */
	spinlock_t buf_list_lock;
};

struct sensor_info {
	u16 full_wd;
	u16 full_ht;
};

struct init_info {
	struct sensor_info sensor;
	u32 sec_tag;
};

struct mtk_imgsys_pipe {
	struct mtk_imgsys_dev *imgsys_dev;
	struct mtk_imgsys_video_device *nodes;
	unsigned long long nodes_streaming;
	unsigned long long nodes_enabled;
	int streaming;
	struct media_pad *subdev_pads;
	struct media_pipeline pipeline;
	struct v4l2_subdev subdev;
	struct v4l2_subdev_fh *fh;
	atomic_t pipe_job_sequence;
	struct list_head pipe_job_pending_list;
	int num_pending_jobs;
	/* protect pipe_job_pending_list and num_pending_jobs */
	spinlock_t pending_job_lock;
	struct list_head pipe_job_running_list;
	/* protect pipe_job_running_list */
	spinlock_t running_job_lock;
	int num_jobs;
	/* Serializes pipe's stream on/off and buffers enqueue operations */
	struct mutex lock;
	//spinlock_t job_lock; /* protect the pipe job list */
	//struct mutex job_lock;
	const struct mtk_imgsys_pipe_desc *desc;
	struct init_info init_info;
};

typedef void (*debug_dump)(struct mtk_imgsys_dev *imgsys_dev,
			   const struct module_ops *imgsys_modules,
			   int imgsys_module_num,
			   unsigned int hw_comb);

struct mtk_imgsys_dev {
	struct device *dev;
	struct resource *imgsys_resource;
	struct media_device mdev;
	struct v4l2_device v4l2_dev;
	struct mtk_imgsys_pipe imgsys_pipe[MTK_IMGSYS_PIPE_ID_TOTAL_NUM];
	struct clk_bulk_data *clks;
	int num_clks;
	int num_mods;
	struct workqueue_struct *composer_wq;
	struct workqueue_struct *runner_wq;
	wait_queue_head_t flushing_waitq;
	/* larb control */
	struct device **larbs;
	unsigned int larbs_num;
	atomic_t num_composing;	/* increase after ipi */
	/* MDP/GCE callback workqueue */
	struct workqueue_struct *mdpcb_wq;
	/* for SCP driver  */
	struct mtk_scp *scp;
	struct rproc *rproc_handle;
	struct device *smem_dev;

	struct mtk_imgsys_hw_working_buf *working_bufs;
	struct mtk_imgsys_hw_working_buf_list free_working_bufs;
	/* increase after enqueue */
	atomic_t imgsys_enqueue_cnt;
	/* increase after stream on, decrease when stream off */
	int stream_cnt;
	/* To serialize request opertion to DIP co-procrosser and hardware */
	struct mutex hw_op_lock;
	/* To restrict the max number of request be send to SCP */
	struct semaphore sem;
	u32 reg_table_size;
	u32 isp_version;
	const struct mtk_imgsys_pipe_desc *cust_pipes;
	const struct module_ops *modules;
	debug_dump dump;
	atomic_t imgsys_user_cnt;
	struct kref init_kref;
};

struct gce_timeout_work {
	struct work_struct work;
	struct mtk_imgsys_request *req;
	void *req_sbuf_kva;
	void *pipe;
	u32 fail_uinfo_idx;
	s8 hw_hang;
};

struct gce_cb_work {
	struct work_struct work;
	u32 reqfd;
	void *req_sbuf_kva;
	void *pipe;
};

struct req_frameparam {
	u32		index;
	u32		frame_no;
	u64		timestamp;
	u8		state;
	u8		num_inputs;
	u8		num_outputs;
	struct img_sw_addr	self_data;
} __packed;

struct req_frame {
	struct req_frameparam frameparam;
};

struct mtk_imgsys_request {
	struct media_request req;
	struct mtk_imgsys_pipe *imgsys_pipe;
	int id;
	struct mtk_imgsys_dev_buffer
				**buf_map;
	struct list_head list;
	struct req_frame img_fparam;
	struct work_struct composer_work;
	struct work_struct runner_work;
	struct mtk_imgsys_hw_working_buf *working_buf;
	struct swfrm_info_t *swfrm_info;
	atomic_t buf_count;
	int request_fd;
};

void mtk_imgsys_dev_v4l2_release(struct mtk_imgsys_dev *imgsys_dev);

int mtk_imgsys_pipe_v4l2_register(struct mtk_imgsys_pipe *pipe,
				  struct media_device *media_dev,
				  struct v4l2_device *v4l2_dev);

void mtk_imgsys_pipe_v4l2_unregister(struct mtk_imgsys_pipe *pipe);

int mtk_imgsys_pipe_init(struct mtk_imgsys_dev *imgsys_dev,
			 struct mtk_imgsys_pipe *pipe,
			 const struct mtk_imgsys_pipe_desc *setting);

int mtk_imgsys_pipe_release(struct mtk_imgsys_pipe *pipe);

struct mtk_imgsys_request *
mtk_imgsys_pipe_get_running_job(struct mtk_imgsys_pipe *pipe, int id);

void mtk_imgsys_pipe_remove_job(struct mtk_imgsys_request *req);

int mtk_imgsys_pipe_next_job_id(struct mtk_imgsys_pipe *pipe);

void mtk_imgsys_pipe_debug_job(struct mtk_imgsys_pipe *pipe,
			       struct mtk_imgsys_request *req);

void mtk_imgsys_pipe_job_finish(struct mtk_imgsys_request *req,
				enum vb2_buffer_state vbf_state);

const struct mtk_imgsys_dev_format *
mtk_imgsys_pipe_find_fmt(struct mtk_imgsys_pipe *pipe,
			 struct mtk_imgsys_video_device *node,
			 u32 format);

void mtk_imgsys_pipe_try_fmt(struct mtk_imgsys_video_device *node,
			     struct v4l2_format *fmt,
			     const struct v4l2_format *ufmt,
			     const struct mtk_imgsys_dev_format *dfmt);

void mtk_imgsys_pipe_load_default_fmt(struct mtk_imgsys_pipe *pipe,
				      struct mtk_imgsys_video_device *node,
				      struct v4l2_format *fmt_to_fill);

void mtk_imgsys_singledevice_ipi_params_config(struct mtk_imgsys_request *req);

void mtk_imgsys_pipe_try_enqueue(struct mtk_imgsys_pipe *pipe);

void mtk_imgsys_hw_enqueue(struct mtk_imgsys_dev *imgsys_dev,
			   struct mtk_imgsys_request *req);

int mtk_imgsys_hw_streamoff(struct mtk_imgsys_pipe *pipe);

int mtk_imgsys_hw_streamon(struct mtk_imgsys_pipe *pipe);

static inline struct mtk_imgsys_pipe*
mtk_imgsys_dev_get_pipe(struct mtk_imgsys_dev *imgsys_dev, unsigned int pipe_id)
{
	if (pipe_id >= MTK_IMGSYS_PIPE_ID_TOTAL_NUM)
		return NULL;

	return &imgsys_dev->imgsys_pipe[pipe_id];
}

static inline struct mtk_imgsys_video_device*
mtk_imgsys_file_to_node(struct file *file)
{
	return container_of(video_devdata(file),
			    struct mtk_imgsys_video_device, vdev);
}

static inline struct mtk_imgsys_pipe*
mtk_imgsys_subdev_to_pipe(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mtk_imgsys_pipe, subdev);
}

static inline struct mtk_imgsys_dev*
mtk_imgsys_mdev_to_dev(struct media_device *mdev)
{
	return container_of(mdev, struct mtk_imgsys_dev, mdev);
}

static inline struct mtk_imgsys_video_device*
mtk_imgsys_vbq_to_node(struct vb2_queue *vq)
{
	return container_of(vq, struct mtk_imgsys_video_device, dev_q.vbq);
}

static inline struct mtk_imgsys_dev_buffer*
mtk_imgsys_vb2_buf_to_dev_buf(struct vb2_buffer *vb)
{
	return container_of(vb, struct mtk_imgsys_dev_buffer, vbb.vb2_buf);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_media_req_to_imgsys_req(struct media_request *req)
{
	return container_of(req, struct mtk_imgsys_request, req);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_composer_work_to_req(struct work_struct *composer_work)
{
	return container_of(composer_work, struct mtk_imgsys_request, composer_work);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_runner_work_to_req(struct work_struct *runner_work)
{
	return container_of(runner_work, struct mtk_imgsys_request, runner_work);
}

static inline struct mtk_imgsys_request *
mtk_imgsys_hw_timeout_work_to_req(struct work_struct *gcetimeout_work)
{
	struct gce_timeout_work *gwork  = container_of(gcetimeout_work,
		struct gce_timeout_work, work);

	return gwork->req;
}

static inline int mtk_imgsys_buf_is_meta(u32 type)
{
	return type == V4L2_BUF_TYPE_META_CAPTURE ||
	       type == V4L2_BUF_TYPE_META_OUTPUT;
}

static inline int mtk_imgsys_pipe_get_pipe_from_job_id(int id)
{
	return (id >> 16) & 0x0000FFFF;
}

int mtk_imgsys_hw_working_buf_pool_init(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_hw_working_buf_pool_release(struct mtk_imgsys_dev *imgsys_dev);

void mtk_imgsys_mod_get(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mod_put(struct mtk_imgsys_dev *imgsys_dev);

struct mtk_imgsys_timeval {
	__kernel_old_time_t	tv_sec;		/* seconds */
	__kernel_suseconds_t	tv_usec;	/* microseconds */
};

struct swfrm_info_t {
	u32 req_sbuf_goft;
	void *req_sbuf_kva;
	int swfrminfo_ridx;
	int request_fd;
	int request_no;
	int frame_no;
	u64 frm_owner;
	u8 is_sec_req;
	int fps;
	int cb_frmcnt;
	int total_taskcnt;
	int exp_totalcb_cnt;
	int handle;
	u64 req_vaddr;
	int sync_id;
	int total_frmnum;
	struct img_swfrm_info user_info[TMAX];
	u8 is_earlycb;
	int earlycb_sidx;
	u8 is_lastfrm;
	s8 group_id;
	s8 batchnum;
	s8 is_sent;	/*check the frame is sent to gce or not*/
	void *req;		/*mtk_dip_request*/
	void *pipe;
	u32 fail_uinfo_idx;
	s8 hw_hang;
	struct mtk_imgsys_timeval eqtime;
	int chan_id;
	char *hw_ts_log;
	struct completion *ndd_user_complete;
} __attribute((__packed__));

struct buf_va_info_t {
	int buf_fd;
	unsigned long kva;
	void *dma_buf_putkva;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
};

#endif /* _MTK_IMGSYS_DEV_H_ */
