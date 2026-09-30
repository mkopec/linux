/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2022 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_HW_H__
#define __MTK_CAM_SENINF_HW_H__

//#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>

enum SET_REG_KEYS {
	REG_KEY_MIN = 0,
	REG_KEY_SETTLE_CK = REG_KEY_MIN,
	REG_KEY_SETTLE_DT,
	REG_KEY_HS_TRAIL_EN,
	REG_KEY_HS_TRAIL_PARAM,
	REG_KEY_CSI_IRQ_STAT,
	REG_KEY_CSI_RESYNC_CYCLE,
	REG_KEY_MUX_IRQ_STAT,
	REG_KEY_CAMMUX_IRQ_STAT,
	REG_KEY_CAMMUX_VSYNC_IRQ_EN,
	REG_KEY_CSI_IRQ_EN,
	REG_KEY_MAX_NUM
};

#define SET_REG_KEYS_NAMES \
	"RG_SETTLE_CK", \
	"RG_SETTLE_DT", \
	"RG_HS_TRAIL_EN", \
	"RG_HS_TRAIL_PARAM", \
	"RG_CSI_IRQ_STAT", \
	"RG_CSI_RESYNC_CYCLE", \
	"RG_MUX_IRQ_STAT", \
	"RG_CAMMUX_IRQ_STAT", \
	"REG_VSYNC_IRQ_EN", \
	"RG_CSI_IRQ_EN", \

struct mtk_cam_seninf_mux_meter {
	u32 width;
	u32 height;
	u32 h_valid;
	u32 h_blank;
	u32 v_valid;
	u32 v_blank;
	s64 mipi_pixel_rate;
	s64 vb_in_us;
	s64 hb_in_us;
	s64 line_time_in_us;
};

struct mtk_cam_seninf_cfg {
	unsigned int seninf_num;
	unsigned int mux_num;
	unsigned int cam_mux_num;
	unsigned int pref_mux_num;
};

extern struct mtk_cam_seninf_cfg *g_seninf_cfg;

int mtk_cam_seninf_init_iomem(struct seninf_ctx *ctx, void __iomem *if_base,
			      void __iomem *ana_base);
int mtk_cam_seninf_init_port(struct seninf_ctx *ctx, int port);
int mtk_cam_seninf_is_cammux_used(struct seninf_ctx *ctx, int cam_mux);
int mtk_cam_seninf_cammux(struct seninf_ctx *ctx, int cam_mux);
int mtk_cam_seninf_disable_cammux(struct seninf_ctx *ctx, int cam_mux);
int mtk_cam_seninf_disable_all_cammux(struct seninf_ctx *ctx);
int mtk_cam_seninf_set_top_mux_ctrl(struct seninf_ctx *ctx, int mux_idx,
				    int seninf_src);
int mtk_cam_seninf_get_top_mux_ctrl(struct seninf_ctx *ctx, int mux_idx);
int mtk_cam_seninf_get_cammux_ctrl(struct seninf_ctx *ctx, int cam_mux);
u32 mtk_cam_seninf_get_cammux_res(struct seninf_ctx *ctx, int cam_mux);
int mtk_cam_seninf_set_cammux_vc(struct seninf_ctx *ctx, int cam_mux,
				 int vc_sel, int dt_sel, int vc_en,
				 int dt_en);
int mtk_cam_seninf_set_cammux_src(struct seninf_ctx *ctx, int src,
				  int target, int exp_hsize, int exp_vsize);
int mtk_cam_seninf_set_vc(struct seninf_ctx *ctx, u32 seninf_idx,
			  struct seninf_vcinfo *vcinfo);
int mtk_cam_seninf_set_mux_ctrl(struct seninf_ctx *ctx, u32 mux, int hs_pol,
				int vs_pol, int src_sel, int pixel_mode);
int mtk_cam_seninf_set_mux_crop(struct seninf_ctx *ctx, u32 mux, int start_x,
				int end_x, int enable);
int mtk_cam_seninf_is_mux_used(struct seninf_ctx *ctx, u32 mux);
int mtk_cam_seninf_mux(struct seninf_ctx *ctx, u32 mux);
int mtk_cam_seninf_disable_mux(struct seninf_ctx *ctx, u32 mux);
int mtk_cam_seninf_disable_all_mux(struct seninf_ctx *ctx);
int mtk_cam_seninf_set_cammux_chk_pixel_mode(struct seninf_ctx *ctx,
					     int cam_mux, int pixel_mode);
int mtk_cam_seninf_set_test_model(struct seninf_ctx *ctx, int mux, int cam_mux,
				  int pixel_mode);
int mtk_cam_seninf_set_csi_mipi(struct seninf_ctx *ctx);
int mtk_cam_seninf_poweroff(struct seninf_ctx *ctx);
int mtk_cam_seninf_reset(struct seninf_ctx *ctx, u32 seninf_idx);
int mtk_cam_seninf_set_idle(struct seninf_ctx *ctx);
int mtk_cam_seninf_get_mux_meter(struct seninf_ctx *ctx, u32 mux,
				 struct mtk_cam_seninf_mux_meter *meter);
ssize_t mtk_cam_seninf_show_status(struct device *dev,
				   struct device_attribute *attr, char *buf);
int mtk_cam_seninf_switch_to_cammux_inner_page(struct seninf_ctx *ctx,
					       bool inner);
int mtk_cam_seninf_set_cammux_next_ctrl(struct seninf_ctx *ctx, int src,
					int target);
int mtk_cam_seninf_update_mux_pixel_mode(struct seninf_ctx *ctx, u32 mux,
					 int pixel_mode);
int mtk_cam_seninf_irq_handler(int irq, void *data);
int mtk_cam_seninf_set_sw_cfg_busy(struct seninf_ctx *ctx, bool enable,
				   int index);
int mtk_cam_seninf_set_cam_mux_dyn_en(struct seninf_ctx *ctx, bool enable,
				      int cam_mux, int index);
int mtk_cam_seninf_reset_cam_mux_dyn_en(struct seninf_ctx *ctx, int index);
int mtk_cam_seninf_enable_global_drop_irq(struct seninf_ctx *ctx, bool enable,
					  int index);
int mtk_cam_seninf_enable_cam_mux_vsync_irq(struct seninf_ctx *ctx, bool enable,
					    int cam_mux);
int mtk_cam_seninf_disable_all_cam_mux_vsync_irq(struct seninf_ctx *ctx);
int mtk_cam_seninf_debug(struct seninf_ctx *ctx);
int mtk_cam_seninf_set_reg(struct seninf_ctx *ctx, u32 key, u32 val);
ssize_t mtk_cam_seninf_show_err_status(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

#endif /* __MTK_CAM_SENINF_HW_H__ */
