// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>

#include "../mtk_imgsys-scp.h"
#include "../mtk_imgsys-engine.h"
#include "../mtk_imgsys-hw.h"
#include "mtk_imgsys-dip.h"

static const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary[] = {
	{0x094, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT1_EN */
	{0x0A0, 0x0},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
	{0x0AC, 0x0},	/* DIPCTL_D1A_DIPCTL_INT3_EN */
	{0x0C4, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT1_EN */
	{0x0D0, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT2_EN */
	{0x0DC, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT3_EN */
	{0x208, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR0_CTL */
	{0x218, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR1_CTL */
	{0x228, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR2_CTL */
	{0x238, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR3_CTL */
	{0x248, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR4_CTL */
	{0x258, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR5_CTL */
	{0x268, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR6_CTL */
	{0x278, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR7_CTL */
	{0x288, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR8_CTL */
	{0x298, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR9_CTL */
	{0x2A8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR10_CTL */
	{0x2B8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR11_CTL */
	{0x2C8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR12_CTL */
	{0x2D8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR13_CTL */
	{0x2E8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR14_CTL */
	{0x2F8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR15_CTL */
	{0x308, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR16_CTL */
	{0x318, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR17_CTL */
	{0x328, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR18_CTL */
};

#define DIP_HW_SET 2

static void __iomem *gdip_reg_base[DIP_HW_SET];

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_NR; hw_idx++) {
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdip_reg_base[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdip_reg_base[ary_idx]) {
			dev_info(imgsys_dev->dev,
				 "%s: error: unable to iomap dip_%d registers, devnode(%s).\n",
				 __func__, hw_idx, imgsys_dev->dev->of_node->name);
			continue;
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_dip_set_initial_value);

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *ofset = NULL;
	int num = sizeof(mtk_imgsys_dip_init_ary) / sizeof(struct mtk_imgsys_init_array);
	unsigned int i;

	for (i = 0 ; i < num; i++) {
		ofset = gdip_reg_base[0] + mtk_imgsys_dip_init_ary[i].ofset;
		writel(mtk_imgsys_dip_init_ary[i].val, ofset);
	}
}
EXPORT_SYMBOL_GPL(imgsys_dip_set_hw_initial_value);

void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			   unsigned int engine)
{
	void __iomem *dip_reg_base;
	unsigned int i;

	dip_reg_base = gdip_reg_base[0];

	dev_info(imgsys_dev->dev, "%s: dump dip ctl regs\n", __func__);
	for (i = TOP_CTL_OFFSET; i <= TOP_CTL_OFFSET + TOP_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15100000 + i,
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = DMATOP_OFFSET; i <= DMATOP_OFFSET + DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15100000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = RDMA_OFFSET; i <= RDMA_OFFSET + RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15100000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = WDMA_OFFSET; i <= WDMA_OFFSET + WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15100000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump nr3d ctl regs\n", __func__);
	for (i = NR3D_CTL_OFFSET; i <= NR3D_CTL_OFFSET + NR3D_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15100000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump tnr ctl regs\n", __func__);
	for (i = TNR_CTL_OFFSET; i <= TNR_CTL_OFFSET + TNR_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15100000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15100000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15100000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15100000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dip_reg_base = gdip_reg_base[1]; // dip_nr: 0x15150000
	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15150000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15150000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15150000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15150000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15150000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15150000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15150000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15150000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15150000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15150000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, (0x15150000 + i),
			 ioread32((dip_reg_base + i)),
			 (0x15150000 + i + 0x4),
			 ioread32((dip_reg_base + i + 0x4)),
			 (0x15150000 + i + 0x8),
			 ioread32((dip_reg_base + i + 0x8)),
			 (0x15150000 + i + 0xc),
			 ioread32((dip_reg_base + i + 0xc)));
	}

	dip_reg_base = gdip_reg_base[0]; // dip: 0x15100000

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x18 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_tnc\n", __func__);
	iowrite32(0x1801, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: tnc_debug: %08X", __func__,
		 ioread32(dip_reg_base + DIPCTL_DBG_OUT));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x0 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x0~0xD */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_nr3d\n", __func__);
	iowrite32(0x13, dip_reg_base + NR3D_DBG_SEL);
	iowrite32(0x00001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_32~1: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x20001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_32~1: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x10001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_33~39: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x30001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_33~39: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x40001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif4~1: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x50001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif8~5: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x60001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif12~9: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x70001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif16~13: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x80001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif20~17: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0x90001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif24~21: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0xA0001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif28~25: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0xB0001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif32~29: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0xC0001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif36~33: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
	iowrite32(0xD0001, dip_reg_base + DIPCTL_DBG_SEL);
	dev_info(imgsys_dev->dev, "%s: nr3d_tif39~37: %08X", __func__,
		 ioread32((dip_reg_base + DIPCTL_DBG_OUT)));
}
EXPORT_SYMBOL_GPL(imgsys_dip_debug_dump);

void imgsys_dip_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			 struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	char *dip_name;
	char file_name[NDD_FP_SIZE] = "\0";
	ssize_t ret;
	void *cq_va;

	if (frm_dump_info->eng_e != IMGSYS_NDD_ENG_DIP)
		return;

	cq_va = imgsys_scp_get_reserve_mem_virt(DIP_MEM_C_ID);
	if (!cq_va)
		return;

	cq_va += frm_dump_info->cq_ofst[frm_dump_info->eng_e];

	dip_name = frm_dump_info->user_buffer ? "REG_DIP_dip.reg" : "REG_DIP_dip.regKernel";
	ret = snprintf(file_name, sizeof(file_name), "%s%s", frm_dump_info->fp, dip_name);
	if (ret < 0 || ret >= sizeof(file_name)) {
		dev_err(imgsys_dev->dev, "wrong dip ndd file name %s\n", file_name);
		return;
	}

	if (frm_dump_info->user_buffer) {
		ret = copy_to_user(frm_dump_info->user_buffer, file_name, sizeof(file_name));
		if (ret)
			return;
		frm_dump_info->user_buffer += sizeof(file_name);

		ret = copy_to_user(frm_dump_info->user_buffer, cq_va, 0x9000);
		if (ret)
			return;
		frm_dump_info->user_buffer += 0x48000 + 0x9000;

		ret = copy_to_user(frm_dump_info->user_buffer,
				   cq_va + 0x9000, DIP_REG_RANGE - 0x9000);
		if (ret)
			return;
		frm_dump_info->user_buffer += DIP_REG_RANGE - 0x9000;
	}
}
EXPORT_SYMBOL_GPL(imgsys_dip_ndd_dump);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdip_reg_base[i]);
		gdip_reg_base[i] = NULL;
	}
}
EXPORT_SYMBOL_GPL(imgsys_dip_uninit);
