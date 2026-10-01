/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_DEBUG_H_
#define _MTK_IMGSYS_DEBUG_H_

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-module.h"
#include "mtk_imgsys-engine.h"

#define NDD_FP_SIZE 256 /* NDD file path size */

/**
 * enum IMGSYS_DL_PATH_E
 *
 * Definition about supported direct link path
 */
enum IMGSYS_DL_PATH_E {
	IMGSYS_DL_WPEE_TRAW = 0,
	IMGSYS_DL_WPEE_DIP = 1,
	IMGSYS_DL_TRAW_DIP = 4,
	IMGSYS_DL_WPE_PQDIP = 5, /*no more used*/
	IMGSYS_DL_DIP_PQDIPA = 6,
	IMGSYS_DL_DIP_PQDIPB = 7,
	IMGSYS_DL_WPET_TRAW = 10,
	IMGSYS_DL_WPET_DIP = 11,
	IMGSYS_DL_ADLA_XTRAW = 14,
	IMGSYS_DL_ADLB_XTRAW = 15,
	IMGSYS_DL_ADLA_ODD_XTRAW = 18,
	IMGSYS_DL_WPEE_XTRAW = 20,
	IMGSYS_DL_WPET_XTRAW = 21,
	IMGSYS_DL_WPEL_TRAW, /*no this*/
};

enum mtk_imgsys_ndd_eng {
	IMGSYS_NDD_ENG_WPE_EIS	= 0,
	IMGSYS_NDD_ENG_WPE_TNR	= 1,
	IMGSYS_NDD_ENG_WPE_LITE	= 2,
	IMGSYS_NDD_ENG_ADL_A	= 3,
	IMGSYS_NDD_ENG_ADL_B	= 4,
	IMGSYS_NDD_ENG_TRAW	= 5,
	IMGSYS_NDD_ENG_LTR	= 6,
	IMGSYS_NDD_ENG_XTR	= 7,
	IMGSYS_NDD_ENG_DIP	= 8,
	IMGSYS_NDD_ENG_PQDIP_A	= 9,
	IMGSYS_NDD_ENG_PQDIP_B	= 10,
	IMGSYS_NDD_ENG_ME	= 11,
	IMGSYS_NDD_ENG_MAX_NUM	= 12
};

struct imgsys_dbg_engine_t {
	enum mtk_imgsys_engine eng_e;
	char eng_name[8];
};

struct imgsys_ndd_frm_dump_info {
	char __user *user_buffer;
	unsigned int hw_comb;
	enum mtk_imgsys_ndd_eng eng_e;
	uint64_t *cq_ofst;
	uint64_t *wpe_psp_ofst;
	char *wpe_fp;
	char *fp;
};

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb);
void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
			       const struct module_ops *imgsys_modules,
			       int imgsys_module_num,
			       unsigned int hw_comb);
void imgsys_ndd_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
			     struct imgsys_ndd_frm_dump_info *frm_dump_info);
void imgsys_ndd_swfrm_list_add(struct mtk_imgsys_dev *imgsys_dev,
			       struct swfrm_info_t *gwfrm_info);
void imgsys_main_init(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_main_set_init(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_main_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_DEBUG_H_ */
