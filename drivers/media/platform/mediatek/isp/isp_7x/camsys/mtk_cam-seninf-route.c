// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-route.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-drv.h"
#include "kd_imgsensor_define_v4l2.h"

#define to_std_fmt_code(code) \
	((code) & 0xFFFF)

void mtk_cam_seninf_init_res(struct seninf_core *core)
{
	int i;

	INIT_LIST_HEAD(&core->list_mux);
	for (i = 0; i < g_seninf_cfg->mux_num; i++) {
		core->mux[i].idx = i;
		list_add_tail(&core->mux[i].list, &core->list_mux);
	}
}

struct seninf_mux *mtk_cam_seninf_mux_get(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	if (!list_empty(&core->list_mux)) {
		ent = list_first_entry(&core->list_mux,
				       struct seninf_mux, list);
		list_move_tail(&ent->list, &ctx->list_mux);
	}

	mutex_unlock(&core->mutex);

	return ent;
}

struct seninf_mux *mtk_cam_seninf_mux_get_pref(struct seninf_ctx *ctx,
					       int *pref_idx, int pref_cnt)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_mux *ent = NULL;

	mutex_lock(&core->mutex);

	list_for_each_entry(ent, &core->list_mux, list) {
		for (i = 0; i < pref_cnt; i++) {
			if (ent->idx == pref_idx[i]) {
				list_move_tail(&ent->list,
					       &ctx->list_mux);
				mutex_unlock(&core->mutex);
				return ent;
			}
		}
	}

	mutex_unlock(&core->mutex);

	return mtk_cam_seninf_mux_get(ctx);
}

void mtk_cam_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux)
{
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->mutex);
	list_move_tail(&mux->list, &core->list_mux);
	mutex_unlock(&core->mutex);
}

void mtk_cam_seninf_get_vcinfo_test(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	if (ctx->is_test_model == 1) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
	} else if (ctx->is_test_model == 2) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_NE;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_ME;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_SE;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
	} else if (ctx->is_test_model == 3) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x30;
		vc->feature = VC_PDAF_STATS;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;
	}
}

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad == idx)
			return &vcinfo->vc[i];
	}

	return NULL;
}

unsigned int mtk_cam_seninf_get_vc_feature(struct v4l2_subdev *sd,
					   unsigned int pad)
{
	struct seninf_vc *pvc = NULL;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	pvc = mtk_cam_seninf_get_vc_by_pad(ctx, pad);
	if (pvc)
		return pvc->feature;

	return VC_NONE;
}

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	if (!ctx->sensor_sd)
		return -EINVAL;

	vcinfo->cnt = 0;

	switch (to_std_fmt_code(ctx->fmt[PAD_SINK].format.code)) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2a;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2c;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	default:
		return -1;
	}

	return 0;
}

void mtk_cam_seninf_release_mux(struct seninf_ctx *ctx)
{
	struct seninf_mux *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, &ctx->list_mux, list) {
		mtk_cam_seninf_mux_put(ctx, ent);
	}
}

int mtk_cam_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt)
{
	int i;
	struct seninf_vc *vc;

	for (i = 0; i < ctx->vcinfo.cnt; i++) {
		vc = &ctx->vcinfo.vc[i];
		if (vc->vc == ch && vc->dt == dt) {
			if (media_pad_remote_pad_first(&ctx->pads[vc->out_pad]))
				return 1;

			return 0;
		}
	}

	return 0;
}

int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int *pixel_mode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	*pixel_mode = vc->pixel_mode;

	return 0;
}

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int pixel_mode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("%s: invalid pad=%d\n", __func__, pad_id);
		return -1;
	}

	vc->pixel_mode = pixel_mode;
	if (ctx->streaming) {
		update_isp_clk(ctx);
		mtk_cam_seninf_update_mux_pixel_mode(ctx, vc->mux, pixel_mode);
	}

	return 0;
}

static int _mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd,
				     int pad_id, int camtg, bool disable_last)
{
	int old_camtg;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)
		return -EINVAL;

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc)
		return -EINVAL;

	ctx->pad2cam[pad_id] = camtg;

	/* change cam-mux while streaming */
	if (ctx->streaming && vc->cam != camtg) {
		if (camtg == 0xff) {
			old_camtg = vc->cam;
			vc->cam = 0xff;
			mtk_cam_seninf_switch_to_cammux_inner_page(ctx, true);
			mtk_cam_seninf_set_cammux_next_ctrl(ctx, 0x1f, old_camtg);
			mtk_cam_seninf_disable_cammux(ctx, old_camtg);
		} else {
			/* disable old */
			old_camtg = vc->cam;
			/* enable new */
			vc->cam = camtg;
			mtk_cam_seninf_switch_to_cammux_inner_page(ctx, true);
			mtk_cam_seninf_set_cammux_next_ctrl(ctx, 0x1f, vc->cam);

			mtk_cam_seninf_switch_to_cammux_inner_page(ctx, false);

			mtk_cam_seninf_set_cammux_vc(ctx, vc->cam,
						     vc->vc, vc->dt,
						     !!vc->dt, !!vc->dt);
			mtk_cam_seninf_set_cammux_src(ctx, vc->mux, vc->cam,
						      vc->exp_hsize,
						      vc->exp_vsize);
			mtk_cam_seninf_set_cammux_chk_pixel_mode(ctx,
								 vc->cam,
								 vc->pixel_mode);
			if (old_camtg != 0xff && disable_last) {
				/* disable old in next sof */
				mtk_cam_seninf_disable_cammux(ctx, old_camtg);
			}
			mtk_cam_seninf_cammux(ctx, vc->cam); /* enable in next sof */
			mtk_cam_seninf_switch_to_cammux_inner_page(ctx, true);
			mtk_cam_seninf_set_cammux_next_ctrl(ctx, vc->mux, vc->cam);
			if (old_camtg != 0xff && disable_last)
				mtk_cam_seninf_set_cammux_next_ctrl(ctx,
								    vc->mux,
								    old_camtg);

			/* user control sensor fsync after change cam-mux */
		}

		dev_info(ctx->dev, "%s: pad %d mux %d cam %d -> %d\n",
			 __func__, vc->out_pad, vc->mux, old_camtg, vc->cam);
	} else {
		dev_info(ctx->dev, "%s: pad_id %d, camtg %d, ctx->streaming %d, vc->cam %d\n",
			 __func__, pad_id, camtg, ctx->streaming, vc->cam);
	}

	return 0;
}

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	return _mtk_cam_seninf_set_camtg(sd, pad_id, camtg, true);
}

