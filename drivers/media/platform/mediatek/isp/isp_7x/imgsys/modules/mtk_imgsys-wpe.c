// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#include "../mtk_imgsys-scp.h"
#include "../mtk_imgsys-debug.h"
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>

#define M4U_PORT_DUMMY_EIS  (0)
#define M4U_PORT_DUMMY_TNR  (1)
#define M4U_PORT_DUMMY_LITE (2)

#include "../mtk_imgsys-engine.h"
#include "mtk_imgsys-wpe.h"

#define WPE_A_BASE        (0x15200000)
static const unsigned int mtk_imgsys_wpe_base_ofst[] = {0x0, 0x300000, 0x400000};
#define WPE_HW_NUM        ARRAY_SIZE(mtk_imgsys_wpe_base_ofst)

#define PQDIP_DL  0x40000
#define DIP_DL    0x80000
#define TRAW_DL   0x100000

#define CQ_THRX_CTL_EN		BIT(0)
#define CQ_THRX_CTL_MODE	BIT(4)
#define CQ_THRX_CTL		(CQ_THRX_CTL_EN | CQ_THRX_CTL_MODE)

#define WPE_REG_DBG_SET     (0x48)
#define WPE_REG_DBG_PORT    (0x4C)
#define WPE_REG_CQ_THR0_CTL (0xB08)
#define WPE_REG_CQ_THR1_CTL (0xB18)

#define WPE_REG_RANGE		0x1000
#define WPE_PSP_END_OFST	0x554
#define WPE_COEF_END_OFST	0x5D8

static const struct mtk_imgsys_init_array mtk_imgsys_wpe_init_ary[] = {
	{0x0018, 0x80000000}, /* WPE_TOP_CTL_INT_EN, en w-clr */
	{0x0024, 0xFFFFFFFF}, /* WPE_TOP_CTL_INT_STATUSX, w-clr */
	{0x00D4, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN, en w-clr */
	{0x00DC, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX, w-clr */
	{0x00E0, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN2, en w-clr */
	{0x00E8, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX2, w-clr */
	{0x00EC, 0x80000000}, /* WPE_TOP_CQ_IRQ_EN3, en w-clr */
	{0x00F4, 0xFFFFFFFF}, /* WPE_TOP_CQ_IRQ_STX3, w-clr */
	{0x0204, 0x00000002}, /* WPE_CACHE_RWCTL_CTL */
	{0x03D4, 0x80000000}, /* WPE_DMA_DMA_ERR_CTRL */
	{0x0B08, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR0_CTL */
	{0x0B18, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR1_CTL */
	{0x0B28, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR2_CTL */
	{0x0B38, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR3_CTL */
	{0x0B48, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR4_CTL */
	{0x0B58, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR5_CTL */
	{0x0B68, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR6_CTL */
	{0x0B78, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR7_CTL */
	{0x0B88, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR8_CTL */
	{0x0B98, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR9_CTL */
	{0x0BA8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR10_CTL */
	{0x0BB8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR11_CTL */
	{0x0BC8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR12_CTL */
	{0x0BD8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR13_CTL */
	{0x0BE8, CQ_THRX_CTL}, /*DIPCQ_W1A_DIPCQ_CQ_THR14_CTL */
};

struct imgsys_reg_range {
	u32 str;
	u32 end;
};

static const struct imgsys_reg_range wpe_regs[] = {
	{0x0000, 0x0164}, /* TOP,VECI,VEC2I */
	{0x0200, 0x027C}, /* CACHE */
	{0x0300, 0x032C}, /* WPEO */
	{0x0340, 0x0368}, /* WPEO2 */
	{0x0380, 0x03A8}, /* MSKO */
	{0x03C0, 0x0408}, /* DMA */
	{0x0440, 0x0450}, /* TDRI */
	{0x04C0, 0x0508}, /* VGEN */
	{0x0540, 0x05D4}, /* PSP */
	{0x0600, 0x0620}, /* C24,C02 */
	{0x0640, 0x0654}, /* DL CROP */
	{0x0680, 0x0694}, /* DMA CROP */
	{0x06C0, 0x07B4}, /* DEC,PAK */
	{0x07C0, 0x07D0}, /* TOP2 */
	{0x0800, 0x080C},
	{0x0B00, 0x0C34}, /* DIPCQ_W1 */
};

static void __iomem *gwpe_reg_base[WPE_HW_NUM];

void imgsys_wpe_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	for (hw_idx = REG_MAP_E_WPE_EIS; hw_idx <= REG_MAP_E_WPE_LITE; hw_idx++) {
		ary_idx = hw_idx - REG_MAP_E_WPE_EIS;
		gwpe_reg_base[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gwpe_reg_base[ary_idx]) {
			dev_info(imgsys_dev->dev,
				 "%s: error: unable to iomap wpe_%d registers, devnode(%s).\n",
				 __func__, hw_idx, imgsys_dev->dev->of_node->name);
			continue;
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_wpe_set_initial_value);

void imgsys_wpe_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *ofset;
	unsigned int i;
	unsigned int hw_idx, ary_idx;

	for (hw_idx = REG_MAP_E_WPE_EIS; hw_idx <= REG_MAP_E_WPE_LITE; hw_idx++) {
		ary_idx = hw_idx - REG_MAP_E_WPE_EIS;
		for (i = 0 ; i < ARRAY_SIZE(mtk_imgsys_wpe_init_ary) ; i++) {
			ofset = gwpe_reg_base[ary_idx] + mtk_imgsys_wpe_init_ary[i].ofset;
			writel(mtk_imgsys_wpe_init_ary[i].val, ofset);
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_wpe_set_hw_initial_value);

static void imgsys_wpe_debug_dl_dump(struct mtk_imgsys_dev *imgsys_dev,
				     void __iomem *wpe_reg_base)
{
	unsigned int dbg_sel_value[3] = {0x0, 0x0, 0x0};
	unsigned int debug_value[3] = {0x0, 0x0, 0x0};
	unsigned int sel_value[3] = {0x0, 0x0, 0x0};

	dbg_sel_value[0] = 0xC << 12;
	dbg_sel_value[1] = 0xD << 12;
	dbg_sel_value[2] = 0xE << 12;

	writel((dbg_sel_value[0] | (0x1 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[1] | (0x1 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[2] | (0x1 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
		 "%s: [0x%x]dbg_sel,[0x%x](31:16)LnCnt(15:0)PixCnt: PQDIP[0x%x]0x%x, DIP[0x%x]0x%x, TRAW[0x%x]0x%x",
		  __func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
		  sel_value[0], debug_value[0], sel_value[1], debug_value[1],
		  sel_value[2], debug_value[2]);

	writel((dbg_sel_value[0] | (0x0 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[1] | (0x0 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value[2] | (0x0 << 8)), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
		 "%s: [0x%x]dbg_sel,[0x%x]val/REQ/RDY: PQDIP[0x%x]0x%x/%d/%d, DIP[0x%x]0x%x/%d/%d, TRAW[0x%x]0x%x/%d/%d",
		 __func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
		sel_value[0], debug_value[0],
		(debug_value[0] >> 24) & 0x1, (debug_value[0] >> 23) & 0x1,
		sel_value[1], debug_value[1],
		(debug_value[1] >> 24) & 0x1, (debug_value[1] >> 23) & 0x1,
		sel_value[2], debug_value[2],
		(debug_value[2] >> 24) & 0x1, (debug_value[2] >> 23) & 0x1);
}

static void imgsys_wpe_debug_cq_dump(struct mtk_imgsys_dev *imgsys_dev,
				     void __iomem *wpe_reg_base)
{
	unsigned int dbg_sel_value = 0x0;
	unsigned int debug_value[5] = {0x0};
	unsigned int sel_value[5] = {0x0};

	debug_value[0] = ioread32((wpe_reg_base + WPE_REG_CQ_THR0_CTL));
	debug_value[1] = ioread32((wpe_reg_base + WPE_REG_CQ_THR1_CTL));
	if (!debug_value[0] || !debug_value[1]) {
		dev_info(imgsys_dev->dev, "%s: No cq_thr enabled! cq0:0x%x, cq1:0x%x",
			 __func__, debug_value[0], debug_value[1]);
		return;
	}

	dbg_sel_value = (0x18 << 12);

	writel((dbg_sel_value | 0x0), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[0] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x1), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[1] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x2), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[2] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x3), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[3] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[3] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	writel((dbg_sel_value | 0x4), (wpe_reg_base + WPE_REG_DBG_SET));
	sel_value[4] = ioread32((wpe_reg_base + WPE_REG_DBG_SET));
	debug_value[4] = ioread32((wpe_reg_base + WPE_REG_DBG_PORT));

	dev_info(imgsys_dev->dev,
		 "%s: [0x%x]dbg_sel,[0x%x]cq_st[0x%x]0x%x, dma_dbg[0x%x]0x%x, dma_req[0x%x]0x%x, dma_rdy[0x%x]0x%x, dma_valid[0x%x]0x%x",
		 __func__, WPE_REG_DBG_SET, WPE_REG_DBG_PORT,
		 sel_value[0], debug_value[0], sel_value[1], debug_value[1],
		 sel_value[2], debug_value[2], sel_value[3], debug_value[3],
		 sel_value[4], debug_value[4]);
}

void imgsys_wpe_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			   unsigned int engine)
{
	void __iomem *wpe_reg_base;
	unsigned int i, j, ctl_en;
	unsigned int hw_idx = 0, ofst_idx;
	unsigned int wpe_base = 0;
	unsigned int start_hw = REG_MAP_E_WPE_EIS, end_hw = REG_MAP_E_WPE_TNR;

	if ((engine & IMGSYS_ENG_WPE_EIS) && !(engine & IMGSYS_ENG_WPE_TNR))
		end_hw = REG_MAP_E_WPE_EIS;

	if (!(engine & IMGSYS_ENG_WPE_EIS) && (engine & IMGSYS_ENG_WPE_TNR))
		start_hw = REG_MAP_E_WPE_TNR;

	if (engine & IMGSYS_ENG_WPE_LITE) {
		start_hw = REG_MAP_E_WPE_LITE;
		end_hw = REG_MAP_E_WPE_LITE;
	}

	for (hw_idx = start_hw; hw_idx <= end_hw; hw_idx++) {
		ofst_idx = hw_idx - REG_MAP_E_WPE_EIS;
		if (ofst_idx >= WPE_HW_NUM)
			continue;

		wpe_base = WPE_A_BASE + mtk_imgsys_wpe_base_ofst[ofst_idx];
		wpe_reg_base = gwpe_reg_base[ofst_idx];
		if (!wpe_reg_base) {
			dev_info(imgsys_dev->dev, "%s: WPE_%d, RegBA = 0", __func__, ofst_idx);
			continue;
		}
		dev_info(imgsys_dev->dev, "%s: ==== Dump WPE_%d =====",
			 __func__, ofst_idx);

		ctl_en = ioread32(wpe_reg_base + 0x4);
		if (ctl_en & (PQDIP_DL | DIP_DL | TRAW_DL)) {
			dev_info(imgsys_dev->dev, "%s: WPE Done: %d", __func__,
				 !(ioread32(wpe_reg_base)) &&
				 (ioread32(wpe_reg_base + 0x24) & 0x1));
			dev_info(imgsys_dev->dev,
				 "%s: WPE_DL: PQDIP(%d), DIP(%d), TRAW(%d)", __func__,
				 (ctl_en & PQDIP_DL) > 0, (ctl_en & DIP_DL) > 0,
				 (ctl_en & TRAW_DL) > 0);
			imgsys_wpe_debug_dl_dump(imgsys_dev, wpe_reg_base);
		}

		imgsys_wpe_debug_cq_dump(imgsys_dev, wpe_reg_base);

		for (j = 0; j < ARRAY_SIZE(wpe_regs); j++) {
			for (i = wpe_regs[j].str; i <= wpe_regs[j].end; i += 0x10) {
				dev_info(imgsys_dev->dev,
					 "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X", __func__,
					 wpe_base + i,
					 ioread32(wpe_reg_base + i),
					 ioread32(wpe_reg_base + i + 0x4),
					 ioread32(wpe_reg_base + i + 0x8),
					 ioread32(wpe_reg_base + i + 0xC));
			}
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_wpe_debug_dump);

void imgsys_wpe_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			 struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	char file_name[NDD_FP_SIZE] = "\0";
	void *cq_va, *psp_va;
	u64 psp_ofst;
	ssize_t ret;

	if (frm_dump_info->eng_e > IMGSYS_NDD_ENG_WPE_LITE)
		return;

	if (!frm_dump_info->user_buffer)
		return;

	cq_va = imgsys_scp_get_reserve_mem_virt(WPE_MEM_C_ID);
	if (!cq_va)
		return;

	cq_va += frm_dump_info->cq_ofst[frm_dump_info->eng_e];

	psp_va = imgsys_scp_get_reserve_mem_virt(WPE_MEM_C_ID);
	if (!psp_va)
		return;

	psp_ofst = frm_dump_info->wpe_psp_ofst[frm_dump_info->eng_e];
	psp_va += psp_ofst;

	ret = snprintf(file_name, sizeof(file_name),
		       "%s%s", frm_dump_info->fp, frm_dump_info->wpe_fp);
	if (ret < 0 || ret >= sizeof(file_name))
		return;

	ret = copy_to_user(frm_dump_info->user_buffer, file_name, sizeof(file_name));
	frm_dump_info->user_buffer += sizeof(file_name);

	if (psp_ofst != 0) {
		ret = copy_to_user(frm_dump_info->user_buffer,
				   cq_va, WPE_PSP_END_OFST);
		frm_dump_info->user_buffer += WPE_PSP_END_OFST;

		ret = copy_to_user(frm_dump_info->user_buffer,
				   psp_va, WPE_COEF_END_OFST - WPE_PSP_END_OFST);
		frm_dump_info->user_buffer += (WPE_COEF_END_OFST - WPE_PSP_END_OFST);

		ret = copy_to_user(frm_dump_info->user_buffer,
				   cq_va + WPE_COEF_END_OFST, WPE_REG_RANGE - WPE_COEF_END_OFST);
		frm_dump_info->user_buffer += (WPE_REG_RANGE - WPE_COEF_END_OFST);
	} else {
		ret = copy_to_user(frm_dump_info->user_buffer,
				   cq_va, WPE_REG_RANGE);
		frm_dump_info->user_buffer += WPE_REG_RANGE;
	}
}
EXPORT_SYMBOL_GPL(imgsys_wpe_ndd_dump);

void imgsys_wpe_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < WPE_HW_NUM; i++) {
		iounmap(gwpe_reg_base[i]);
		gwpe_reg_base[i] = NULL;
	}
}
EXPORT_SYMBOL_GPL(imgsys_wpe_uninit);

MODULE_LICENSE("GPL");
