/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_IMG_IPI_H__
#define __MTK_IMG_IPI_H__

#include <linux/types.h>

#include "mtk_header_desc.h"

/* updated in W1948.1 */
#define HEADER_VER 19481

/* ISP-MDP generic input information */
#define MTK_V4L2_BATCH_MODE_SUPPORT	1

#define IMG_MAX_HW_DMAS		72

#define IMG_IPI_INIT    1
#define IMG_IPI_DEINIT  2
#define IMG_IPI_FRAME   3
#define IMG_IPI_DEBUG   4

#define IMG_MAX_HW	12
#define WPE_ENG_NUM	3

enum img_module_id {
  IMG_MODULE_WPE = 0,
  IMG_MODULE_ADL = 1,
  IMG_MODULE_TRAW = 2,
  IMG_MODULE_DIP = 3,
  IMG_MODULE_PQDIP = 4,
  IMG_MODULE_MAX = 5,
};

struct module_init_info {
	u64	c_wbuf;
	u64	c_wbuf_dma;
	u32	c_wbuf_sz;
	u32	c_wbuf_fd;
	u64	t_wbuf;
	u64	t_wbuf_dma;
	u32	t_wbuf_sz;
	u32	t_wbuf_fd;
} __packed;

struct img_init_info {
	u32	header_version;
	u32	isp_version;
	u32	dip_save_file;
	u32	param_pack_size;
	u32	dip_param_size;
	u32	frameparam_size;
	u32	reg_phys_addr;
	u32	reg_range;
	u64	hw_buf_fd;
	u64	hw_buf;
	u32	hw_buf_size;
	u32	reg_table_size;
	u32	sub_frm_size;
	u32	cq_size;
	u64	drv_data;
	/*new add, need refine*/
	struct module_init_info module_info[IMG_MODULE_MAX];
	u32    g_wbuf_fd;
	u64	g_wbuf;
	u32	g_wbuf_sz;
	u32	sec_tag;
	u16	full_wd;
	u16	full_ht;
	u32	smvr_mode;
} __packed;

struct img_swfrm_info {
	u32 hw_comb;
	int sw_ridx;
	u8 is_time_shared;
	u8 is_sec_frm;
	u8 is_earlycb;
	u8 is_lastingroup;
	u64 sw_goft;
	u64 sw_bwoft;
	int subfrm_idx;
	void *g_swbuf;
	void *bw_swbuf;
	u64 pixel_bw;
	u64 ndd_cq_ofst[IMG_MAX_HW];
	u64 ndd_wpe_psp_ofst[WPE_ENG_NUM];
	char ndd_wpe_fp[64];
	char ndd_fp[256];
} __packed;

struct img_sw_addr {
	u64	va;	/* Used by APMCU access */
	u32	pa;	/* Used by CM4 access */
	u32	offset; /* Used by User Daemon access */
#ifdef MTK_V4L2_BATCH_MODE_SUPPORT
	// Batch mode {
	u32	fd; /* Used by User Daemon access */
	// } Batch mode
#endif
} __packed;

#define TMAX (16)
struct img_ipi_frameparam {
	u8		dmas_enable[IMG_MAX_HW_DMAS][TMAX];
	struct header_desc_norm	dmas[IMG_MAX_HW_DMAS];
	struct header_desc_norm	tuning_meta;
	struct header_desc_norm	ctrl_meta;
};

struct img_sw_buffer {
	u64	handle;		/* Used by APMCU access */
	u32	scp_addr;	/* Used by CM4 access */
	u32	fd;		/* Used by User Daemon access */
	u32	offset;		/* Used by User Daemon access */
} __packed;

struct img_ipi_param {
	u8	usage;
	u8	smvr_mode;
	struct img_sw_buffer frm_param;
#ifdef MTK_V4L2_BATCH_MODE_SUPPORT
	// V3 batch mode added {
	u8	is_batch_mode;
	u8	num_frames;
	u64	req_addr_va;
	u64	frm_param_offset;
	// } V3 batch mode added
#endif
} __packed;

#endif  /* __MTK_IMG_IPI_H__ */

