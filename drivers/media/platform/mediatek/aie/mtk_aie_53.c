// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/types.h>
#include <linux/version.h>

#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include <soc/mediatek/smi.h>

#include "mtk_aie.h"

static const struct mtk_aie_variant *mtk_aie_get_variant(struct device *dev);

static struct clk_bulk_data aie_clks[] = {
	{ .id = "IMG_IPE" },
	{ .id = "IPE_FDVT" },
	{ .id = "IPE_TOP" },
	{ .id = "IPE_SMI_LARB12" },
};

#ifdef CONFIG_DEBUG_FS
#define AIE_DUMP_BUFFER_SIZE (1 * 1024 * 1024)
#define AIE_DEBUG_RAW_DATA_SIZE (1 * 640 * 480)
static unsigned char srcrawdata[AIE_DEBUG_RAW_DATA_SIZE];
static unsigned char dstrawdata[AIE_DEBUG_RAW_DATA_SIZE];
static struct debugfs_blob_wrapper srcdata, dstdata;
#endif

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
#define AIE_QOS_MAX 4
#define AIE_QOS_RA_IDX 0
#define AIE_QOS_RB_IDX 1
#define AIE_QOS_WA_IDX 2
#define AIE_QOS_WB_IDX 3
#define AIE_READ_AVG_BW 213
#define AIE_WRITE_AVG_BW 145
#endif

#define V4L2_CID_MTK_AIE_INIT (V4L2_CID_USER_MTK_FD_BASE + 1)
#define V4L2_CID_MTK_AIE_PARAM (V4L2_CID_USER_MTK_FD_BASE + 2)

#define V4L2_CID_MTK_AIE_MAX 2

static const struct v4l2_pix_format_mplane mtk_aie_img_fmts[] = {
	{
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_GREY,
		.num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
	},
};

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
static const struct mtk_aie_qos_path aie_qos_path[AIE_QOS_MAX] = {
	{ NULL, "l12_fdvt_rda", 0 },
	{ NULL, "l12_fdvt_rdb", 0 },
	{ NULL, "l12_fdvt_wra", 0 },
	{ NULL, "l12_fdvt_wrb", 0 }
};
#endif

#define NUM_FORMATS ARRAY_SIZE(mtk_aie_img_fmts)

static inline struct mtk_aie_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_aie_ctx, fh);
}

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
static int mtk_aie_mmdvfs_init(struct mtk_aie_dev *fd)
{
	u64 freq = 0;
	int ret = -ENOMEM;
	int opp_num = 0, opp_idx = 0, idx = 0, volt = 0;
	struct device_node *np = NULL, *child_np = NULL;
	struct of_phandle_iterator it;

	fd->dvfs_info = kzalloc(sizeof(*fd->dvfs_info), GFP_KERNEL);
	if (!fd->dvfs_info)
		return ret;

	fd->dvfs_info->dev = fd->dev;
	ret = dev_pm_opp_of_add_table(fd->dvfs_info->dev);
	if (ret < 0) {
		dev_err(fd->dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return ret;
	}

	fd->dvfs_info->reg =
		devm_regulator_get(fd->dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(fd->dvfs_info->reg)) {
		dev_err(fd->dvfs_info->dev, "can't get dvfsrc-vcore\n");
		ret = PTR_ERR(fd->dvfs_info->reg);
		fd->dvfs_info->reg = NULL;
		return ret;
	}

	opp_num = regulator_count_voltages(fd->dvfs_info->reg);
	of_for_each_phandle(&it, ret, fd->dvfs_info->dev->of_node, "operating-points-v2", NULL, 0) {
		if (!it.node) {
			dev_err(fd->dvfs_info->dev, "of_node_get fail\n");
			return ret;
		}
		np = of_node_get(it.node);

		do {
			child_np = of_get_next_available_child(np, child_np);
			if (child_np) {
				of_property_read_u64(child_np, "opp-hz", &freq);
				of_property_read_u32(child_np, "opp-microvolt", &volt);
				if (freq == 0 || volt == 0) {
					dev_err(fd->dvfs_info->dev,
						"%s: [ERROR] parsing zero freq/volt(%d/%d) at idx(%d)\n",
						__func__,
						freq,
						volt,
						idx
					);
					continue;
				}
				fd->dvfs_info->clklv[opp_idx][idx] = freq;
				fd->dvfs_info->voltlv[opp_idx][idx] = volt;
				dev_dbg(fd->dvfs_info->dev,
					"[%s] opp=%d, idx=%d, clk=%d volt=%d\n",
					__func__,
					opp_idx,
					idx,
					fd->dvfs_info->clklv[opp_idx][idx],
					fd->dvfs_info->voltlv[opp_idx][idx]
				);
				idx++;
			}
		} while (child_np);
		fd->dvfs_info->clklv_num[opp_idx] = idx;
		fd->dvfs_info->clklv_target[opp_idx] = fd->dvfs_info->clklv[opp_idx][0];
		fd->dvfs_info->clklv_idx[opp_idx] = 0;
		idx = 0;
		opp_idx++;
		of_node_put(np);
	}
	fd->dvfs_info->cur_volt = 0;

	return 0;
}

static void mtk_aie_mmdvfs_uninit(struct mtk_aie_dev *fd)
{
	int volt = 0;

	if (fd->dvfs_info) {
		fd->dvfs_info->cur_volt = volt;

		regulator_set_voltage(fd->dvfs_info->reg, volt, INT_MAX);

		kfree(fd->dvfs_info);
	}
}

static void mtk_aie_mmdvfs_set(struct mtk_aie_dev *fd, bool is_set,
			       unsigned int level)
{
	int volt = 0, idx = 0, opp_idx = 0;

	if (is_set) {
		if (level < fd->dvfs_info->clklv_num[opp_idx])
			idx = level;
	}
	volt = fd->dvfs_info->voltlv[opp_idx][idx];

	if (fd->dvfs_info->cur_volt != volt) {
		dev_dbg(fd->dvfs_info->dev,
			"[%s] volt change opp=%d, idx=%d, clk=%d volt=%d\n",
			__func__,
			opp_idx,
			idx,
			fd->dvfs_info->clklv[opp_idx][idx],
			fd->dvfs_info->voltlv[opp_idx][idx]
		);
		regulator_set_voltage(fd->dvfs_info->reg, volt, INT_MAX);
		fd->dvfs_info->cur_volt = volt;
	}
}

static int mtk_aie_mmqos_init(struct mtk_aie_dev *fd)
{
	int idx = 0;
	int ret = -ENOMEM;

	fd->qos_info = kzalloc(sizeof(*fd->qos_info), GFP_KERNEL);
	if (!fd->qos_info)
		return ret;

	fd->qos_info->dev = fd->dev;
	fd->qos_info->qos_path = (struct mtk_aie_qos_path *)aie_qos_path;

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		fd->qos_info->qos_path[idx].path =
			of_mtk_icc_get(fd->qos_info->dev, fd->qos_info->qos_path[idx].dts_name);
		fd->qos_info->qos_path[idx].bw = 0;
		dev_dbg(fd->qos_info->dev,
			"[%s] idx=%d, path=%p, name=%s, bw=%llu\n",
			__func__,
			idx,
			fd->qos_info->qos_path[idx].path,
			fd->qos_info->qos_path[idx].dts_name,
			fd->qos_info->qos_path[idx].bw
		);
	}
	return 0;
}

static void mtk_aie_mmqos_uninit(struct mtk_aie_dev *fd)
{
	int idx = 0;

	if (fd->qos_info) {
		for (idx = 0; idx < AIE_QOS_MAX; idx++) {
			if (!fd->qos_info->qos_path[idx].path) {
				dev_dbg(fd->qos_info->dev,
					"[%s] path of idx(%d) is NULL\n",
					__func__,
					idx
				);
				continue;
			}
			dev_dbg(fd->qos_info->dev,
				"[%s] idx=%d, path=%p, bw=%llu\n",
				__func__,
				idx,
				fd->qos_info->qos_path[idx].path,
				fd->qos_info->qos_path[idx].bw
			);
			fd->qos_info->qos_path[idx].bw = 0;
			mtk_icc_set_bw(fd->qos_info->qos_path[idx].path, 0, 0);
		}
		kfree(fd->qos_info);
	}
}

static void mtk_aie_mmqos_set(struct mtk_aie_dev *fd, bool is_set)
{
	int r_bw = 0;
	int w_bw = 0;
	int idx = 0;

	if (is_set) {
		r_bw = AIE_READ_AVG_BW;
		w_bw = AIE_WRITE_AVG_BW;
	}

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		if (!fd->qos_info->qos_path[idx].path) {
			dev_dbg(fd->qos_info->dev,
				"[%s] path of idx(%d) is NULL\n",
				__func__,
				idx
			);
			continue;
		}

		if (idx == AIE_QOS_RA_IDX &&
		    fd->qos_info->qos_path[idx].bw != r_bw) {
			dev_dbg(fd->qos_info->dev,
				"[%s] idx=%d, path=%p, bw=%llu/%d,\n",
				__func__,
				idx,
				fd->qos_info->qos_path[idx].path,
				fd->qos_info->qos_path[idx].bw,
				r_bw
			);
			fd->qos_info->qos_path[idx].bw = r_bw;
			mtk_icc_set_bw(fd->qos_info->qos_path[idx].path,
				       MBps_to_icc(fd->qos_info->qos_path[idx].bw),
				       0
			);
		}

		if (idx == AIE_QOS_WA_IDX &&
		    fd->qos_info->qos_path[idx].bw != w_bw) {
			dev_dbg(fd->qos_info->dev,
				"[%s] idx=%d, path=%p, bw=%llu/%d,\n",
				__func__,
				idx,
				fd->qos_info->qos_path[idx].path,
				fd->qos_info->qos_path[idx].bw,
				w_bw
			);
			fd->qos_info->qos_path[idx].bw = w_bw;
			mtk_icc_set_bw(fd->qos_info->qos_path[idx].path,
				       MBps_to_icc(fd->qos_info->qos_path[idx].bw),
				       0
			);
		}
	}
}
#endif

static int mtk_aie_fill_init_param(struct mtk_aie_dev *fd,
				   struct user_init *user_init,
				   struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl = NULL;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_INIT);
	if (ctrl) {
		memcpy(user_init, ctrl->p_new.p_u32, sizeof(struct user_init));
		dev_info(fd->dev,
			 "init param : max w:%d, max h:%d",
			 user_init->max_img_width,
			 user_init->max_img_height
		);
		dev_info(fd->dev,
			 "init param : p_w:%d, p_h:%d, f thread:%d",
			 user_init->pyramid_width,
			 user_init->pyramid_height,
			 user_init->feature_threshold
		);
	} else {
		dev_err(fd->dev, "NO V4L2_CID_MTK_AIE_INIT!\n");
		return -EINVAL;
	}

	return 0;
}

static int mtk_aie_hw_enable(struct mtk_aie_dev *fd)
{
	struct mtk_aie_ctx *ctx = fd->ctx;
	struct user_init user_init;
	int ret = -EINVAL;

	memset(&user_init, 0, sizeof(struct user_init));

	/* initial value */
	ret = mtk_aie_fill_init_param(fd, &user_init, &ctx->hdl);
	if (ret != 0)
		goto init_fail;

	ret = aie_init(fd, &user_init);

init_fail:
	return ret;
}

static void mtk_aie_hw_job_finish(struct mtk_aie_dev *fd,
				  enum vb2_buffer_state vb_state)
{
	struct mtk_aie_ctx *ctx = NULL;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	pm_runtime_put(fd->dev);
	ctx = v4l2_m2m_get_curr_priv(fd->m2m_dev);
	if (!ctx) {
		dev_err(fd->dev, "Failed to do v4l2_m2m_get_curr_priv!\n");
	} else {
		src_vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		dst_vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		if (src_vbuf && dst_vbuf)
			v4l2_m2m_buf_copy_metadata(src_vbuf, dst_vbuf, true);
		if (src_vbuf)
			v4l2_m2m_buf_done(src_vbuf, vb_state);
		if (dst_vbuf)
			v4l2_m2m_buf_done(dst_vbuf, vb_state);
		if (src_vbuf && dst_vbuf)
			v4l2_m2m_job_finish(fd->m2m_dev, ctx->fh.m2m_ctx);
	}
	complete_all(&fd->fd_job_finished);
}

static int mtk_aie_hw_connect(struct mtk_aie_dev *fd)
{
	if (mtk_aie_hw_enable(fd))
		return -EINVAL;
#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
	mtk_aie_mmdvfs_set(fd, 1, 0);
	mtk_aie_mmqos_set(fd, 1);
#endif

	return 0;
}

static void mtk_aie_hw_disconnect(struct mtk_aie_dev *fd)
{
#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
	mtk_aie_mmqos_set(fd, 0);
	mtk_aie_mmdvfs_set(fd, 0, 0);
#endif
	aie_uninit(fd);
}

static int mtk_aie_hw_job_exec(struct mtk_aie_dev *fd)
{
	pm_runtime_get_sync(fd->dev);

	reinit_completion(&fd->fd_job_finished);
	schedule_delayed_work(&fd->job_timeout_work,
			      msecs_to_jiffies(MTK_FD_HW_TIMEOUT_IN_MSEC)
	);

	return 0;
}

static int mtk_aie_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;
	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_aie_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct device *dev = ctx->dev;
	struct v4l2_pix_format_mplane *pixfmt = NULL;
	int ret = 0;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		if (vb2_plane_size(vb, 0) < ctx->dst_fmt.buffersize) {
			dev_err(dev, "meta size %lu is too small\n", vb2_plane_size(vb, 0));
			ret = -EINVAL;
		} else {
			vb2_set_plane_payload(vb, 0, ctx->dst_fmt.buffersize);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		pixfmt = &ctx->src_fmt;

		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;

		if (vb->num_planes > 2 || vbuf->field != V4L2_FIELD_NONE) {
			dev_dbg(dev,
				"plane %d or field %d not supported\n",
				vb->num_planes,
				vbuf->field
			);
			ret = -EINVAL;
		}

		if (vb2_plane_size(vb, 0) < pixfmt->plane_fmt[0].sizeimage) {
			dev_dbg(dev,
				"plane 0 %lu is too small than %x\n",
				vb2_plane_size(vb, 0),
				pixfmt->plane_fmt[0].sizeimage
			);
			ret = -EINVAL;
		} else {
			vb2_set_plane_payload(vb, 0, pixfmt->plane_fmt[0].sizeimage);
		}

		if (pixfmt->num_planes == 2 &&
		    vb2_plane_size(vb, 1) < pixfmt->plane_fmt[1].sizeimage) {
			dev_dbg(dev,
				"plane 1 %lu is too small than %x\n",
				vb2_plane_size(vb, 1),
				pixfmt->plane_fmt[1].sizeimage
			);
			ret = -EINVAL;
		} else {
			vb2_set_plane_payload(vb, 1, pixfmt->plane_fmt[1].sizeimage);
		}
		break;
	}

	return ret;
}

static void mtk_aie_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int mtk_aie_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct device *dev = ctx->dev;
	unsigned int size[2] = { 0, 0 };
	unsigned int plane = 0;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		size[0] = ctx->dst_fmt.buffersize;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		size[0] = ctx->src_fmt.plane_fmt[0].sizeimage;
		size[1] = ctx->src_fmt.plane_fmt[1].sizeimage;
		break;
	}

	dev_info(dev, "vq type =%d, size[0]=%d, size[1]=%d\n", vq->type, size[0], size[1]);

	if (*num_planes > 2)
		return -EINVAL;

	*num_buffers = clamp_val(*num_buffers, 1, VB2_MAX_FRAME);

	if (*num_planes == 0) {
		if (vq->type == V4L2_BUF_TYPE_META_CAPTURE) {
			sizes[0] = ctx->dst_fmt.buffersize;
			*num_planes = 1;
			return 0;
		}

		*num_planes = ctx->src_fmt.num_planes;
		if (*num_planes > 2)
			return -EINVAL;
		for (plane = 0; plane < *num_planes; plane++)
			sizes[plane] = ctx->src_fmt.plane_fmt[plane].sizeimage;

		return 0;
	}

	return 0;
}

static int mtk_aie_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_aie_dev *fd = NULL;

	if (!ctx)
		return -EINVAL;

	fd = ctx->fd_dev;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fd->fd_stream_count++;
		if (fd->fd_stream_count == 1)
			return mtk_aie_hw_connect(ctx->fd_dev);
	}

	return 0;
}

static void aie_dumpinfo(struct mtk_aie_dev *fd)
{
	unsigned int i = 0;
	u32 *cfg_value = NULL;

	for (i = 0; i < 0x158; i = i + 4) {
		fd->tr_info.dump_offset +=
			scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
				  fd->tr_info.dump_size - fd->tr_info.dump_offset,
				  "Reg: %x %x\n",
				  i,
				  readl(fd->fd_base + i)
			);
	}
	for (i = 0x200; i < 0x3F8; i = i + 4) {
		fd->tr_info.dump_offset +=
			scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
				  fd->tr_info.dump_size - fd->tr_info.dump_offset,
				  "Dma Reg: %x %x\n",
				  i,
				  readl(fd->fd_base + i)
			);
	}
	if (fd->aie_cfg->sel_mode == FDMODE) {
		cfg_value = (u32 *)fd->base_para->fd_yuv2rgb_cfg_va;
		for (i = 0; i < fd->fd_yuv2rgb_cfg_size / 4; i++) {
			fd->tr_info.dump_offset +=
				scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
					  fd->tr_info.dump_size - fd->tr_info.dump_offset,
					  "YUV config: %x, %x\n",
					  i,
					  cfg_value[i]
				);
		}
		cfg_value = (u32 *)fd->base_para->fd_rs_cfg_va;
		for (i = 0; i < fd->fd_rs_cfg_size / 4; i++) {
			fd->tr_info.dump_offset +=
				scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
					  fd->tr_info.dump_size - fd->tr_info.dump_offset,
					  "RS config: %x, %x\n",
					  i,
					  cfg_value[i]
				);
		}
		cfg_value = (u32 *)fd->base_para->fd_fd_cfg_va;
		for (i = 0; i < fd->fd_fd_cfg_size / 4; i++) {
			fd->tr_info.dump_offset +=
				scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
					  fd->tr_info.dump_size - fd->tr_info.dump_offset,
					  "FD config: %x, %x\n",
					  i,
					  cfg_value[i]
				);
		}
	}

	if (fd->aie_cfg->sel_mode == ATTRIBUTEMODE) {
		cfg_value = (u32 *)fd->base_para->attr_yuv2rgb_cfg_va;
		for (i = 0; i < fd->fd_yuv2rgb_cfg_size / 4; i++) {
			fd->tr_info.dump_offset +=
				scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
					  fd->tr_info.dump_size - fd->tr_info.dump_offset,
					  "YUV config: %x, %x\n",
					  i,
					  cfg_value[i]
				);
		}
		cfg_value = (u32 *)fd->base_para->attr_fd_cfg_va;
		for (i = 0; i < fd->fd_fd_cfg_size / 4; i++) {
			fd->tr_info.dump_offset +=
				scnprintf(fd->tr_info.dump_buffer + fd->tr_info.dump_offset,
					  fd->tr_info.dump_size - fd->tr_info.dump_offset,
					  "FD config: %x, %x\n",
					  i,
					  cfg_value[i]
				);
		}
	}

	if (fd->tr_info.srcdata_size <= AIE_DEBUG_RAW_DATA_SIZE)
		memcpy(srcrawdata, fd->tr_info.srcdata_buf, fd->tr_info.srcdata_size);

	if (fd->tr_info.dstdata_size <= AIE_DEBUG_RAW_DATA_SIZE)
		memcpy(dstrawdata, fd->tr_info.dstdata_buf, fd->tr_info.dstdata_size);
}

static void mtk_aie_job_timeout_work(struct work_struct *work)
{
	struct mtk_aie_dev *fd =
		container_of(work, struct mtk_aie_dev, job_timeout_work.work);

	dev_err(fd->dev, "FD Job timeout!");

	dev_dbg(fd->dev,
		"%s result result1: %x, %x, %x",
		__func__,
		readl(fd->fd_base + AIE_RESULT_0_REG),
		readl(fd->fd_base + AIE_RESULT_1_REG),
		readl(fd->fd_base + AIE_DMA_CTL_REG)
	);

	fd->aie_cfg->irq_status = readl(fd->fd_base + AIE_INT_EN_REG);

#ifdef CONFIG_DEBUG_FS
	if (fd->aie_cfg->irq_status & 0x00010000 ||
	    fd->aie_cfg->irq_status & 0x00100000 ||
	    fd->aie_cfg->irq_status & 0x01000000)
		aie_dumpinfo(fd);
#endif

	if (fd->aie_cfg->sel_mode == ATTRIBUTEMODE)
		dev_dbg(fd->dev,
			"[ATTRMODE] w_idx = %d, r_idx = %d\n",
			fd->attr_para->w_idx,
			fd->attr_para->r_idx
		);

	aie_irqhandle(fd);
	aie_reset(fd);
	atomic_dec(&fd->num_composing);
	mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_ERROR);
	wake_up(&fd->flushing_waitq);
}

static int mtk_aie_job_wait_finish(struct mtk_aie_dev *fd)
{
	return wait_for_completion_timeout(&fd->fd_job_finished, msecs_to_jiffies(1000));
}

static void mtk_aie_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *vb = NULL;
	struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;
	struct v4l2_m2m_queue_ctx *queue_ctx;

	if (!mtk_aie_job_wait_finish(fd))
		dev_info(fd->dev, "wait job finish timeout\n");

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fd->fd_stream_count--;
		if (fd->fd_stream_count > 0)
			dev_info(fd->dev, "stop: fd_stream_count = %d\n", fd->fd_stream_count);
		else
			mtk_aie_hw_disconnect(fd);
	}

	queue_ctx = V4L2_TYPE_IS_OUTPUT(vq->type) ? &m2m_ctx->out_q_ctx :
						    &m2m_ctx->cap_q_ctx;
	while ((vb = v4l2_m2m_buf_remove(queue_ctx)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
}

static void mtk_aie_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req, &ctx->hdl);
}

static int mtk_aie_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct mtk_aie_dev *fd = video_drvdata(file);
	struct device *dev = fd->dev;

	strscpy(cap->driver, dev_driver_string(dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(dev), sizeof(cap->card));

	cap->device_caps = V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			   V4L2_CAP_STREAMING | V4L2_CAP_META_CAPTURE;
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	return 0;
}

static int mtk_aie_enum_fmt_out_mp(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	f->pixelformat = mtk_aie_img_fmts[f->index].pixelformat;
	return 0;
}

static void mtk_aie_fill_pixfmt_mp(struct v4l2_pix_format_mplane *dfmt,
				   const struct v4l2_pix_format_mplane *sfmt)
{
	dfmt->field = V4L2_FIELD_NONE;
	dfmt->colorspace = V4L2_COLORSPACE_BT2020;
	dfmt->num_planes = sfmt->num_planes;
	dfmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	dfmt->quantization = V4L2_QUANTIZATION_DEFAULT;
	dfmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(dfmt->colorspace);
	dfmt->pixelformat = sfmt->pixelformat;

	/* Keep user setting as possible */
	dfmt->width = clamp(dfmt->width, MTK_FD_OUTPUT_MIN_WIDTH,
			    MTK_FD_OUTPUT_MAX_WIDTH);
	dfmt->height = clamp(dfmt->height, MTK_FD_OUTPUT_MIN_HEIGHT,
			     MTK_FD_OUTPUT_MAX_HEIGHT);

	dfmt->plane_fmt[0].bytesperline = ALIGN(dfmt->width, 16);
	dfmt->plane_fmt[1].bytesperline = ALIGN(dfmt->width, 16);

	if (sfmt->num_planes == 2) {
		dfmt->plane_fmt[0].sizeimage =
			dfmt->height * dfmt->plane_fmt[0].bytesperline;
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12M)
			dfmt->plane_fmt[1].sizeimage =
				dfmt->height * dfmt->plane_fmt[1].bytesperline /
				2;
		else
			dfmt->plane_fmt[1].sizeimage =
				dfmt->height * dfmt->plane_fmt[1].bytesperline;
	} else {
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12)
			dfmt->plane_fmt[0].sizeimage =
				dfmt->height * dfmt->plane_fmt[0].bytesperline *
				3 / 2;
		else
			dfmt->plane_fmt[0].sizeimage =
				dfmt->height * dfmt->plane_fmt[0].bytesperline;
	}
}

static const struct v4l2_pix_format_mplane *mtk_aie_find_fmt(u32 format)
{
	unsigned int i = 0;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (mtk_aie_img_fmts[i].pixelformat == format)
			return &mtk_aie_img_fmts[i];
	}

	return NULL;
}

static int mtk_aie_try_fmt_out_mp(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct v4l2_pix_format_mplane *fmt = NULL;

	fmt = mtk_aie_find_fmt(pix_mp->pixelformat);
	if (!fmt)
		fmt = &mtk_aie_img_fmts[0]; /* Get default img fmt */

	mtk_aie_fill_pixfmt_mp(pix_mp, fmt);
	return 0;
}

static int mtk_aie_g_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);

	f->fmt.pix_mp = ctx->src_fmt;

	return 0;
}

static int mtk_aie_s_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	struct mtk_aie_dev *fd = NULL;
	const struct v4l2_pix_format_mplane *fmt = NULL;

	if (!ctx)
		return -EINVAL;

	fd = ctx->fd_dev;

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(vq))
		return -EBUSY;

	fmt = mtk_aie_find_fmt(f->fmt.pix_mp.pixelformat);
	if (!fmt)
		fmt = &mtk_aie_img_fmts[0]; /* Get default img fmt */
	else if (&fd->ctx->fh != file->private_data)
		return -EBUSY;
	if (fd->ctx != ctx)
		fd->ctx = ctx;

	mtk_aie_fill_pixfmt_mp(&f->fmt.pix_mp, fmt);
	ctx->src_fmt = f->fmt.pix_mp;

	return 0;
}

static int mtk_aie_enum_fmt_meta_cap(struct file *file, void *fh,
				     struct v4l2_fmtdesc *f)
{
	if (f->index)
		return -EINVAL;

	strscpy(f->description, "Face detection result",
		sizeof(f->description));

	f->pixelformat = V4L2_META_FMT_MTFD_RESULT;
	f->flags = 0;

	return 0;
}

static int mtk_aie_g_fmt_meta_cap(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	f->fmt.meta.dataformat = V4L2_META_FMT_MTFD_RESULT;
	f->fmt.meta.buffersize = sizeof(struct aie_enq_info);

	return 0;
}

static const struct vb2_ops mtk_aie_vb2_ops = {
	.queue_setup = mtk_aie_vb2_queue_setup,
	.buf_out_validate = mtk_aie_vb2_buf_out_validate,
	.buf_prepare = mtk_aie_vb2_buf_prepare,
	.buf_queue = mtk_aie_vb2_buf_queue,
	.start_streaming = mtk_aie_vb2_start_streaming,
	.stop_streaming = mtk_aie_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_aie_vb2_request_complete,
};

static const struct v4l2_ioctl_ops mtk_aie_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_aie_querycap,
	.vidioc_enum_fmt_vid_out = mtk_aie_enum_fmt_out_mp,
	.vidioc_g_fmt_vid_out_mplane = mtk_aie_g_fmt_out_mp,
	.vidioc_s_fmt_vid_out_mplane = mtk_aie_s_fmt_out_mp,
	.vidioc_try_fmt_vid_out_mplane = mtk_aie_try_fmt_out_mp,
	.vidioc_enum_fmt_meta_cap = mtk_aie_enum_fmt_meta_cap,
	.vidioc_g_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_s_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_try_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int mtk_aie_queue_init(void *priv, struct vb2_queue *src_vq,
			      struct vb2_queue *dst_vq)
{
	struct mtk_aie_ctx *ctx = priv;
	int ret = -EINVAL;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->supports_requests = true;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_aie_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->fd_dev->vfd_lock;
	src_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_META_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_aie_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->fd_dev->vfd_lock;
	dst_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	return vb2_queue_init(dst_vq);
}

static struct v4l2_ctrl_config mtk_aie_controls[] = {
	{
		.id = V4L2_CID_MTK_AIE_INIT,
		.name = "FD detection init",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = { sizeof(struct user_init) / 4 },
	},
	{
		.id = V4L2_CID_MTK_AIE_PARAM,
		.name = "FD detection param",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = { sizeof(struct user_param) / 4 },
	},
};

static int mtk_aie_ctrls_setup(struct mtk_aie_ctx *ctx)
{
	struct v4l2_ctrl_handler *hdl = &ctx->hdl;
	int i;

	v4l2_ctrl_handler_init(hdl, V4L2_CID_MTK_AIE_MAX);
	if (hdl->error)
		return hdl->error;

	for (i = 0; i < ARRAY_SIZE(mtk_aie_controls); i++) {
		v4l2_ctrl_new_custom(hdl, &mtk_aie_controls[i], ctx);
		if (hdl->error) {
			v4l2_ctrl_handler_free(hdl);
			dev_err(ctx->dev, "Failed to register controls:%d", i);
			return hdl->error;
		}
	}

	ctx->fh.ctrl_handler = &ctx->hdl;
	v4l2_ctrl_handler_setup(hdl);

	return 0;
}

static void init_ctx_fmt(struct mtk_aie_ctx *ctx)
{
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	struct v4l2_meta_format *dst_fmt = &ctx->dst_fmt;

	/* Initialize M2M source fmt */
	src_fmt->width = MTK_FD_OUTPUT_MAX_WIDTH;
	src_fmt->height = MTK_FD_OUTPUT_MAX_HEIGHT;
	mtk_aie_fill_pixfmt_mp(src_fmt, &mtk_aie_img_fmts[0]);

	/* Initialize M2M destination fmt */
	dst_fmt->buffersize = sizeof(struct aie_enq_info);
	dst_fmt->dataformat = V4L2_META_FMT_MTFD_RESULT;
}

/*
 * V4L2 file operations.
 */
static int mtk_vfd_open(struct file *filp)
{
	struct mtk_aie_dev *fd = video_drvdata(filp);
	struct video_device *vdev = video_devdata(filp);
	struct mtk_aie_ctx *ctx = NULL;
	int ret = -EINVAL;

	mutex_lock(&fd->dev_lock);

	if (fd->fd_state & STATE_OPEN) {
		dev_info(fd->dev, "vfd_open again");
		ret =  -EBUSY;
		goto err_unlock;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret =  -ENOMEM;
		goto err_unlock;
	}

	ctx->fd_dev = fd;
	ctx->dev = fd->dev;
	fd->ctx = ctx;

	v4l2_fh_init(&ctx->fh, vdev);
	filp->private_data = &ctx->fh;

	init_ctx_fmt(ctx);

	ret = mtk_aie_ctrls_setup(ctx);
	if (ret) {
		dev_err(ctx->dev, "Failed to set up controls:%d\n", ret);
		goto err_fh_exit;
	}
	ctx->fh.m2m_ctx =
		v4l2_m2m_ctx_init(fd->m2m_dev, ctx, &mtk_aie_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto err_free_ctrl_handler;
	}
	v4l2_fh_add(&ctx->fh);
	fd->fd_state |= STATE_OPEN;

	mutex_unlock(&fd->dev_lock);

	return 0;
err_free_ctrl_handler:
	v4l2_ctrl_handler_free(&ctx->hdl);
err_fh_exit:
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
err_unlock:
	mutex_unlock(&fd->dev_lock);

	return ret;
}

static int mtk_vfd_release(struct file *filp)
{
	struct mtk_aie_ctx *ctx =
		container_of(filp->private_data, struct mtk_aie_ctx, fh);
	struct mtk_aie_dev *fd = video_drvdata(filp);

	mutex_lock(&fd->dev_lock);

	fd->fd_state &= ~STATE_OPEN;

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_ctrl_handler_free(&ctx->hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	mutex_unlock(&fd->dev_lock);

	return 0;
}

static __poll_t mtk_vfd_fop_poll(struct file *file, poll_table *wait)
{
	struct mtk_aie_ctx *ctx =
		container_of(file->private_data, struct mtk_aie_ctx, fh);

	struct mtk_aie_dev *fd = ctx->fd_dev;

	if (fd->fd_state & STATE_INIT) {
		if (!mtk_aie_job_wait_finish(ctx->fd_dev)) {
			dev_info(ctx->dev, "wait job finish timeout from poll\n");
			return EPOLLERR;
		}
	}

	return v4l2_m2m_fop_poll(file, wait);
}

static const struct v4l2_file_operations fd_video_fops = {
	.owner = THIS_MODULE,
	.open = mtk_vfd_open,
	.release = mtk_vfd_release,
	.poll = mtk_vfd_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_m2m_fop_mmap,
};

static void mtk_aie_fill_user_param(struct mtk_aie_dev *fd,
				    struct user_param *user_param,
				    struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_PARAM);
	if (ctrl)
		memcpy(user_param, ctrl->p_new.p_u32, sizeof(struct user_param));
	else
		dev_err(fd->dev, "NO V4L2_CID_MTK_AIE_PARAM!\n");
}

static int mtk_aie_job_ready(void *priv)
{
	struct mtk_aie_ctx *ctx = priv;
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct fd_enq_param fd_param = {};
	void *plane_vaddr = NULL;
	int ret = 1;

	if (!ctx->fh.m2m_ctx) {
		dev_info(fd->dev, "Memory-to-memory context is NULL\n");
		ret = 0;
		goto ctrl_ret;
	}

	if (!(fd->fd_state & (STATE_INIT | STATE_OPEN))) {
		dev_err(fd->dev, "%s fd state fail: %d\n", __func__, fd->fd_state);
		ret = 0;
		goto ctrl_ret;
	}

	mutex_lock(&fd->fd_lock);

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	mtk_aie_fill_user_param(fd, &fd_param.user_param, &ctx->hdl);

	plane_vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);
	if (!plane_vaddr) {
		dev_info(fd->dev, "Failed to get plane virtual address\n");
		ret = 0;
		goto err_unlock;
	}

#ifdef CONFIG_DEBUG_FS
	fd->tr_info.srcdata_buf = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	fd->tr_info.srcdata_size = vb2_get_plane_payload(&src_buf->vb2_buf, 0);
	fd->tr_info.dstdata_buf = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);
	fd->tr_info.dstdata_size = vb2_get_plane_payload(&dst_buf->vb2_buf, 0);
#endif

	fd->aie_cfg = (struct aie_enq_info *)plane_vaddr;

	memset(fd->aie_cfg, 0, sizeof(struct aie_enq_info));

	memcpy(fd->aie_cfg, &fd_param.user_param, sizeof(struct user_param));

	if (fd->variant->fld_enable) {
		fd->aie_cfg->fld_face_num = fd_param.user_param.fld_face_num;
		memcpy(fd->aie_cfg->fld_input,
		       fd_param.user_param.fld_input,
		       FLD_MAX_FRAME * sizeof(struct fld_crop_rip_rop)
		);
	}

	fd_param.src_img[0].dma_addr =
		vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);

	if (ctx->src_fmt.num_planes == 2) {
		fd_param.src_img[1].dma_addr =
			vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
	}

	if ((fd->aie_cfg->sel_mode == FDMODE || fd->aie_cfg->sel_mode == ATTRIBUTEMODE) &&
	    fd->aie_cfg->src_img_fmt == FMT_YUV420_1P) {
		fd_param.src_img[1].dma_addr =
			fd_param.src_img[0].dma_addr +
			fd_param.user_param.src_img_stride *
			fd_param.user_param.src_img_height;
	}

	fd->aie_cfg->src_img_addr = fd_param.src_img[0].dma_addr;
	fd->aie_cfg->src_img_addr_uv = fd_param.src_img[1].dma_addr;

	if (aie_prepare(fd, fd->aie_cfg)) {
		dev_err(fd->dev, "Failed to prepare aie setting\n");
		ret = 0;
		goto err_unlock;
	}

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
	mtk_aie_mmdvfs_set(fd, 1, fd->aie_cfg->freq_level);
#endif

err_unlock:
	mutex_unlock(&fd->fd_lock);

ctrl_ret:
	/* Complete request controls if any */
	v4l2_ctrl_request_complete(src_buf->vb2_buf.req_obj.req, &ctx->hdl);

	return ret;
}

static void mtk_aie_device_run(void *priv)
{
	struct mtk_aie_ctx *ctx = priv;
	struct mtk_aie_dev *fd = ctx->fd_dev;
	int ret = 0;

	ret = mtk_aie_job_ready(priv);
	if (ret != 1) {
		dev_err(fd->dev, "Failed to run job ready\n");
		return;
	}

	atomic_inc(&fd->num_composing);
	mtk_aie_hw_job_exec(fd);
	aie_execute(fd, fd->aie_cfg);
}

static struct v4l2_m2m_ops fd_m2m_ops = {
	.device_run = mtk_aie_device_run,
};

static const struct media_device_ops fd_m2m_media_ops = {
	.req_validate = vb2_request_validate,
	.req_queue = v4l2_m2m_request_queue,
};

static int mtk_aie_video_device_register(struct mtk_aie_dev *fd)
{
	struct video_device *vfd = &fd->vfd;
	struct v4l2_m2m_dev *m2m_dev = fd->m2m_dev;
	struct device *dev = fd->dev;
	int ret = -EINVAL;

	vfd->fops = &fd_video_fops;
	vfd->release = video_device_release_empty;
	vfd->lock = &fd->vfd_lock;
	vfd->v4l2_dev = &fd->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			   V4L2_CAP_META_CAPTURE;
	vfd->ioctl_ops = &mtk_aie_v4l2_video_out_ioctl_ops;

	strscpy(vfd->name, dev_driver_string(dev), sizeof(vfd->name));

	video_set_drvdata(vfd, fd);

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		dev_err(dev, "Failed to register video device\n");
		goto err_free_dev;
	}
#ifdef CONFIG_MEDIA_CONTROLLER
	ret = v4l2_m2m_register_media_controller(m2m_dev, vfd, MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret) {
		dev_err(dev, "Failed to init mem2mem media controller\n");
		goto err_unreg_video;
	}
#endif
	return 0;
#ifdef CONFIG_MEDIA_CONTROLLER
err_unreg_video:
	video_unregister_device(vfd);
#endif
err_free_dev:
	return ret;
}

static int mtk_aie_dev_v4l2_init(struct mtk_aie_dev *fd)
{
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device *mdev = &fd->mdev;
#endif
	struct device *dev = fd->dev;
	int ret = -EINVAL;

	ret = v4l2_device_register(dev, &fd->v4l2_dev);
	if (ret) {
		dev_err(dev, "Failed to register v4l2 device\n");
		return ret;
	}

	fd->m2m_dev = v4l2_m2m_init(&fd_m2m_ops);
	if (IS_ERR(fd->m2m_dev)) {
		dev_err(dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(fd->m2m_dev);
		goto err_unreg_v4l2_dev;
	}
#ifdef CONFIG_MEDIA_CONTROLLER
	mdev->dev = dev;
	strscpy(mdev->model, dev_driver_string(dev), sizeof(mdev->model));
	media_device_init(mdev);
	mdev->ops = &fd_m2m_media_ops;
	fd->v4l2_dev.mdev = mdev;
#endif

	ret = mtk_aie_video_device_register(fd);
	if (ret)
		goto err_cleanup_mdev;

#ifdef CONFIG_MEDIA_CONTROLLER
	ret = media_device_register(mdev);
	if (ret) {
		dev_err(dev, "Failed to register mem2mem media device\n");
		goto err_unreg_vdev;
	}
#endif
	return 0;

#ifdef CONFIG_MEDIA_CONTROLLER
err_unreg_vdev:
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
#endif
err_cleanup_mdev:
#ifdef CONFIG_MEDIA_CONTROLLER
	media_device_cleanup(mdev);
#endif
	v4l2_m2m_release(fd->m2m_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&fd->v4l2_dev);
	return ret;
}

static void mtk_aie_video_device_unregister(struct mtk_aie_dev *fd)
{
#ifdef CONFIG_MEDIA_CONTROLLER
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
#endif
	video_unregister_device(&fd->vfd);
#ifdef CONFIG_MEDIA_CONTROLLER
	media_device_cleanup(&fd->mdev);
#endif
	v4l2_m2m_release(fd->m2m_dev);
	v4l2_device_unregister(&fd->v4l2_dev);
}

static void mtk_aie_frame_done_worker(struct work_struct *work)
{
	struct mtk_aie_req_work *req_work = (struct mtk_aie_req_work *)work;
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)req_work->fd_dev;

	if (fd->reg_cfg.fd_mode == FDMODE) {
		fd->reg_cfg.hw_result = readl(fd->fd_base + AIE_RESULT_0_REG);
		fd->reg_cfg.hw_result1 = readl(fd->fd_base + AIE_RESULT_1_REG);
	}

	mutex_lock(&fd->fd_lock);

	switch (fd->aie_cfg->sel_mode) {
	case FDMODE:
		aie_get_fd_result(fd, fd->aie_cfg);
		break;
	case ATTRIBUTEMODE:
		aie_get_attr_result(fd, fd->aie_cfg);
		break;
	case FLDMODE:
		if (fd->variant->fld_enable)
			aie_get_fld_result(fd, fd->aie_cfg);
		break;
	default:
		dev_dbg(fd->dev, "Wrong sel_mode\n");
		break;
	}

	mutex_unlock(&fd->fd_lock);

#ifdef CONFIG_DEBUG_FS
	if (fd->tr_info.trigger_dump[0] != '0') {
		cancel_delayed_work(&fd->job_timeout_work);
		aie_irqhandle(fd);
		aie_reset(fd);
		atomic_dec(&fd->num_composing);
		mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_ERROR);
		wake_up(&fd->flushing_waitq);
		aie_dumpinfo(fd);
		fd->tr_info.trigger_dump[0] = '0';
		return;
	}
#endif
	if (!cancel_delayed_work(&fd->job_timeout_work))
		return;

	atomic_dec(&fd->num_composing);
	mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_DONE);
	wake_up(&fd->flushing_waitq);
}

#ifdef CONFIG_DEBUG_FS
static int aie_dbg_dump_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t aie_dbg_dump_read(struct file *file, char __user *buf,
				 size_t len, loff_t *ppos)
{
	struct mtk_aie_dev *fd = file->private_data;
	ssize_t ret_size = 0;

	ret_size = simple_read_from_buffer(buf, len, ppos,
					   fd->tr_info.dump_buffer,
					   fd->tr_info.dump_offset);

	return ret_size;
}

static const struct file_operations aie_debug_dump_fops = {
	.owner = THIS_MODULE,
	.open = aie_dbg_dump_open,
	.read = aie_dbg_dump_read,
};

static int aie_dbg_trigger_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t aie_dbg_trigger_write(struct file *file, const char __user *buf,
				     size_t len, loff_t *ppos)
{
	struct mtk_aie_dev *fd = file->private_data;
	size_t ret = -EINVAL;

	if (*ppos > sizeof(fd->tr_info.trigger_dump))
		return -EINVAL;

	ret = simple_write_to_buffer(fd->tr_info.trigger_dump,
				     sizeof(fd->tr_info.trigger_dump), ppos,
				     buf, len);

	return ret;
}

static ssize_t aie_dbg_trigger_read(struct file *file, char __user *buf,
				    size_t len, loff_t *ppos)
{
	struct mtk_aie_dev *fd = file->private_data;

	return simple_read_from_buffer(buf, len, ppos,
				       fd->tr_info.trigger_dump,
				       sizeof(fd->tr_info.trigger_dump));
}

static const struct file_operations aie_debug_trigger_fops = {
	.owner = THIS_MODULE,
	.open = aie_dbg_trigger_open,
	.write = aie_dbg_trigger_write,
	.read = aie_dbg_trigger_read,
};

static struct dentry *aie_debugfs_dump, *aie_debugfs_trigger,
	*aie_debugfs_srcdata, *aie_debugfs_dstdata, *aie_debugfs_root;
#endif

#ifdef CONFIG_DEBUG_FS
static int mtk_aie_debugfs_init(struct mtk_aie_dev *fd)
{
	aie_debugfs_root = debugfs_create_dir("mtk_aie", NULL);
	if (!aie_debugfs_root)
		return -ENOMEM;

	aie_debugfs_dump = debugfs_create_file("dumpinfo",
					       0444,
					       aie_debugfs_root,
					       fd,
					       &aie_debug_dump_fops
				);
	if (!aie_debugfs_dump)
		return -ENOMEM;

	aie_debugfs_trigger = debugfs_create_file("trigger_dump",
						  0444,
						  aie_debugfs_root,
						  fd,
						  &aie_debug_trigger_fops
				);
	if (!aie_debugfs_trigger)
		return -ENOMEM;

	srcdata.data = (void *)srcrawdata;
	srcdata.size = AIE_DEBUG_RAW_DATA_SIZE;

	aie_debugfs_srcdata = debugfs_create_blob("srcdata.raw",
						  0444,
						  aie_debugfs_root,
						  &srcdata
				);
	if (!aie_debugfs_srcdata)
		return -ENOMEM;

	dstdata.data = (void *)dstrawdata;
	dstdata.size = AIE_DEBUG_RAW_DATA_SIZE;

	aie_debugfs_dstdata = debugfs_create_blob("dstdata.raw",
						  0444,
						  aie_debugfs_root,
						  &dstdata
				);
	if (!aie_debugfs_dstdata)
		return -ENOMEM;

	fd->tr_info.dump_size = AIE_DUMP_BUFFER_SIZE;
	fd->tr_info.dump_buffer =
		kmalloc(fd->tr_info.dump_size, GFP_KERNEL);
	if (!fd->tr_info.dump_buffer)
		return -ENOMEM;

	fd->tr_info.trigger_dump[0] = '0';
	fd->tr_info.trigger_dump[1] = '\n';
	fd->tr_info.trigger_dump[2] = '\0';

	return 0;
}

static void mtk_aie_debugfs_free(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

	kfree(fd->tr_info.dump_buffer);
	fd->tr_info.dump_buffer = NULL;

	debugfs_remove_recursive(aie_debugfs_root);
	aie_debugfs_root = NULL;
}
#else
static int mtk_aie_debugfs_init(struct mtk_aie_dev *fd)
{
	return 0;
}

static void mtk_aie_debugfs_free(struct platform_device *pdev)
{
}
#endif

static int mtk_aie_resource_init(struct mtk_aie_dev *fd)
{
	int ret = 0;

	mutex_init(&fd->vfd_lock);
	mutex_init(&fd->dev_lock);
	mutex_init(&fd->fd_lock);

	init_completion(&fd->fd_job_finished);
	complete_all(&fd->fd_job_finished);
	INIT_DELAYED_WORK(&fd->job_timeout_work, mtk_aie_job_timeout_work);
	init_waitqueue_head(&fd->flushing_waitq);
	atomic_set(&fd->num_composing, 0);
	fd->fd_stream_count = 0;

	fd->frame_done_wq = alloc_ordered_workqueue(dev_name(fd->dev),
						    WQ_HIGHPRI | WQ_FREEZABLE
				);
	if (!fd->frame_done_wq) {
		dev_err(fd->dev, "failed to alloc frame_done workqueue\n");
		mutex_destroy(&fd->vfd_lock);
		mutex_destroy(&fd->dev_lock);
		mutex_destroy(&fd->fd_lock);
		return -ENOMEM;
	}

	INIT_WORK(&fd->req_work.work, mtk_aie_frame_done_worker);
	fd->req_work.fd_dev = fd;

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
	ret = mtk_aie_mmdvfs_init(fd);
	if (ret)
		return ret;
	ret = mtk_aie_mmqos_init(fd);
	if (ret)
		return ret;
	}
#endif
	return ret;
}

static void mtk_aie_resource_free(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

#ifdef CONFIG_INTERCONNECT_MTK_EXTENSION
	mtk_aie_mmdvfs_uninit(fd);
	mtk_aie_mmqos_uninit(fd);
#endif
	if (fd->frame_done_wq)
		destroy_workqueue(fd->frame_done_wq);
	fd->frame_done_wq = NULL;
	mutex_destroy(&fd->vfd_lock);
	mutex_destroy(&fd->dev_lock);
	mutex_destroy(&fd->fd_lock);
}

static irqreturn_t mtk_aie_irq(int irq, void *data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data;

	aie_irqhandle(fd);

	queue_work(fd->frame_done_wq, &fd->req_work.work);

	return IRQ_HANDLED;
}

static int mtk_aie_probe(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = NULL;
	struct device *dev = &pdev->dev;

	int irq = -1;
	int ret = -EINVAL;

	fd = devm_kzalloc(&pdev->dev, sizeof(struct mtk_aie_dev), GFP_KERNEL);
	if (!fd)
		return -ENOMEM;

	fd->variant = mtk_aie_get_variant(dev);
	if (!fd->variant)
		return -ENODEV;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret) {
		dev_err(dev, "%s: No suitable DMA available\n", __func__);
		return ret;
	}

	dev_set_drvdata(dev, fd);
	fd->dev = dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Failed to get irq by platform: %d\n", irq);
		return irq;
	}

	ret = devm_request_irq(dev,
			       irq,
			       mtk_aie_irq,
			       IRQF_SHARED,
			       dev_driver_string(dev),
			       fd
		);
	if (ret) {
		dev_err(dev, "Failed to request irq\n");
		return ret;
	}
	fd->irq = irq;

	fd->fd_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(fd->fd_base)) {
		dev_err(dev, "Failed to get fd reg base\n");
		return PTR_ERR(fd->fd_base);
	}

	fd->aie_clk.clk_num = ARRAY_SIZE(aie_clks);
	fd->aie_clk.clks = aie_clks;
	ret = devm_clk_bulk_get(&pdev->dev, fd->aie_clk.clk_num, fd->aie_clk.clks);
	if (ret) {
		dev_err(dev, "failed to get raw clock:%d\n", ret);
		return ret;
	}

	ret = mtk_aie_debugfs_init(fd);
	if (ret)
		goto err_free;

	ret = mtk_aie_resource_init(fd);
	if (ret)
		goto err_free;
	pm_runtime_enable(dev);
	ret = mtk_aie_dev_v4l2_init(fd);
	if (ret)
		goto err_free;
	dev_info(dev, "AIE : Success to %s\n", __func__);

	return 0;

err_free:
	pm_runtime_disable(&pdev->dev);
	mtk_aie_resource_free(pdev);

	return ret;
}

static int mtk_aie_remove(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

	mtk_aie_video_device_unregister(fd);
	pm_runtime_disable(&pdev->dev);
	mtk_aie_debugfs_free(pdev);
	mtk_aie_resource_free(pdev);

#ifdef CONFIG_DEBUG_FS
	kfree(fd->tr_info.dump_buffer);
#endif

	return 0;
}

static int mtk_aie_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret = -EINVAL, num = 0;

	if (pm_runtime_suspended(dev))
		return 0;

	num = atomic_read(&fd->num_composing);
	dev_info(dev, "%s: suspend aie job start, num(%d)\n", __func__, num);

	ret = wait_event_timeout(fd->flushing_waitq,
				 !(num = atomic_read(&fd->num_composing)),
				 msecs_to_jiffies(MTK_FD_HW_TIMEOUT_IN_MSEC)
		);
	if (!ret && num) {
		dev_dbg(dev,
			"%s: flushing aie job timeout num %d\n",
			__func__,
			num
		);

		return -EBUSY;
	}

	dev_info(dev, "%s: suspend aie job end num(%d)\n", __func__, num);

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		return ret;

	return 0;
}

static int mtk_aie_resume(struct device *dev)
{
	int ret = -EINVAL;

	dev_info(dev, "%s: resume aie job start)\n", __func__);

	if (pm_runtime_suspended(dev)) {
		dev_info(dev,
			 "%s: pm_runtime_suspended is true, no action\n",
			 __func__
		);
		return 0;
	}

	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	dev_info(dev, "%s: resume aie job end)\n", __func__);
	return 0;
}

static int mtk_aie_runtime_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);

	clk_bulk_disable_unprepare(fd->aie_clk.clk_num, fd->aie_clk.clks);

	return 0;
}

static int mtk_aie_runtime_resume(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret = -EINVAL;

	ret = clk_bulk_prepare_enable(fd->aie_clk.clk_num, fd->aie_clk.clks);
	if (ret) {
		dev_err(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mtk_aie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_aie_suspend, mtk_aie_resume)
	SET_RUNTIME_PM_OPS(mtk_aie_runtime_suspend, mtk_aie_runtime_resume, NULL)
};

static const struct mtk_aie_variant aie_30_drvdata = {
	.hw_version = 30,
	.fld_enable = 0,
	.y2r_cfg_size = 32,
	.rs_cfg_size = 28,
	.fd_cfg_size = 54,
};

static const struct mtk_aie_variant aie_31_drvdata = {
	.hw_version = 31,
	.fld_enable = 1,
	.y2r_cfg_size = 34,
	.rs_cfg_size = 30,
	.fd_cfg_size = 56,
};

static const struct of_device_id mtk_aie_of_ids[] = {
	{
		.compatible = "mediatek,aie-hw3.0",
		.data = &aie_30_drvdata,
	},
	{
		.compatible = "mediatek,aie-hw3.1",
		.data = &aie_31_drvdata,
	},
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, mtk_aie_of_ids);

static const struct mtk_aie_variant *mtk_aie_get_variant(struct device *dev)
{
	const struct mtk_aie_variant *driver_data = NULL;
	const struct of_device_id *match = NULL;

	match = of_match_node(mtk_aie_of_ids, dev->of_node);

	if (match)
		driver_data = (const struct mtk_aie_variant *)match->data;
	else
		return &aie_30_drvdata;

	return driver_data;
}

static struct platform_driver mtk_aie_driver = {
	.probe = mtk_aie_probe,
	.remove = mtk_aie_remove,
	.driver = {
			.name = "mtk-aie-5.3",
			.of_match_table = of_match_ptr(mtk_aie_of_ids),
			.pm = pm_ptr(&mtk_aie_pm_ops),
	}
};

module_platform_driver(mtk_aie_driver);
MODULE_AUTHOR("Fish Wu <fish.wu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek AIE driver");
