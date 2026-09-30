/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2022 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_H__
#define __MTK_CAM_SENINF_H__

#include <linux/kthread.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>

#include "mtk_cam-seninf-def.h"
#include "kd_imgsensor_define_v4l2.h"
#include "mtk_cam-seninf-regs.h"

struct seninf_mux {
	struct list_head list;
	int idx;
};

struct seninf_cam_mux {
	struct list_head list;
	int idx;
};

struct seninf_vc {
	u8 vc;
	u8 dt;
	u8 feature;
	u8 out_pad;
	u8 pixel_mode;
	u8 group;
	u8 mux; /* allocated per group */
	u8 cam; /* assigned by cam driver */
	u8 enable;
	u16 exp_hsize;
	u16 exp_vsize;
};

struct seninf_vcinfo {
	struct seninf_vc vc[SENINF_VC_MAXCNT];
	int cnt;
};

struct seninf_dfs {
	struct device *dev;
	struct regulator *reg;
	unsigned long *freqs;
	unsigned long *volts;
	int cnt;
};

struct seninf_core {
	struct device *dev;
	int pm_domain_cnt;
	struct device **pm_domain_devs;
	struct clk *clk[CLK_MAXCNT];
	struct seninf_dfs dfs;
	struct list_head list;
	struct list_head list_mux;
	struct seninf_mux mux[SENINF_MUX_NUM];
	struct mutex mutex;  /* protect seninf core operations */
	void __iomem *reg_if;
	void __iomem *reg_ana;
	int refcnt;

	/* platform properties */
	int cphy_settle_delay_dt;
	int dphy_settle_delay_dt;
	int settle_delay_ck;
	int hs_trail_parameter;

	/* protect variables in irq handler */
	spinlock_t spinlock_irq;

	/* mipi error detection count */
	unsigned int detection_cnt;
	/* enable csi irq flag */
	unsigned int csi_irq_en_flag;
};

struct seninf_ctx {
	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct device *dev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad pads[PAD_MAXCNT];
	struct v4l2_subdev_format fmt[PAD_MAXCNT];
	struct seninf_core *core;
	struct list_head list;

	u32 port;
	u32 port_a;
	u32 port_b;
	u32 port_num;
	u32 num_data_lanes;
	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;
	s64 customized_pixel_rate;
	unsigned int m_csi_efuse;

	unsigned int is_4d1c:1;
	unsigned int is_cphy:1;
	unsigned int is_test_model:4;
	unsigned int is_secure:1;
	unsigned int sec_info_addr;
	u32 seninf_idx;
	int pad2cam[PAD_MAXCNT];

	/* remote sensor */
	struct v4l2_subdev *sensor_sd;
	u32 sensor_pad_idx;

	/* provided by sensor */
	struct seninf_vcinfo vcinfo;
	int fps_n;
	int fps_d;

	/* dfs */
	int isp_freq;

	void __iomem *reg_ana_csi_rx[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_dphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_cphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_if_top;
	void __iomem *reg_if_ctrl[SENINF_NUM];
	void __iomem *reg_if_cam_mux;
	void __iomem *reg_if_cam_mux_gcsr;
	void __iomem *reg_if_cam_mux_pcsr[SENINF_CAM_MUX_NUM];
	void __iomem *reg_if_tg[SENINF_NUM];
	void __iomem *reg_if_csi2[SENINF_NUM];
	void __iomem *reg_if_mux[SENINF_MUX_NUM];

	/* resources */
	struct list_head list_mux;
	struct list_head list_cam_mux;

	/* flags */
	unsigned int streaming:1;

	int cphy_settle_delay_dt;
	int dphy_settle_delay_dt;
	int settle_delay_ck;
	int hs_trail_parameter;

	int open_refcnt;
	struct mutex mutex;  /* protect seninf context */

	/* csi irq */
	unsigned int data_not_enough_cnt;
	unsigned int err_lane_resync_cnt;
	unsigned int crc_err_cnt;
	unsigned int ecc_err_double_cnt;
	unsigned int ecc_err_corrected_cnt;
	/* seninf_mux fifo overrun irq */
	unsigned int fifo_overrun_cnt;
	/* cam_mux h/v size irq */
	unsigned int size_err_cnt;
	/* error flag */
	unsigned int data_not_enough_flag;
	unsigned int err_lane_resync_flag;
	unsigned int crc_err_flag;
	unsigned int ecc_err_double_flag;
	unsigned int ecc_err_corrected_flag;
	unsigned int fifo_overrun_flag;
	unsigned int size_err_flag;
};

#endif
