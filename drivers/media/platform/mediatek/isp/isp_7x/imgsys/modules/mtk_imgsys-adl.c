// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

#include "mtk_imgsys-adl.h"
#include "../mtk_imgsys-engine.h"

static bool have_adl;
static void __iomem *g_adl_a_va;
static void __iomem *g_adl_b_va;

enum imgsys_smc_control {
	IMGSYS_ADL_SET_DOMAIN
};

void imgsys_adl_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct resource adl;

	adl.start = 0;
	if (of_address_to_resource(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A, &adl)) {
		pr_info("%s: of_address_to_resource fail\n", __func__);
		return;
	}

	if (adl.start) {
		g_adl_a_va = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);
		g_adl_b_va = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);
		have_adl = true;
	} else {
		have_adl = false;
	}
}
EXPORT_SYMBOL_GPL(imgsys_adl_init);

void imgsys_adl_set(struct mtk_imgsys_dev *imgsys_dev)
{
	struct arm_smccc_res res;

	if (have_adl) {
		arm_smccc_smc(1, IMGSYS_ADL_SET_DOMAIN, 0,
			      0, 0, 0, 0, 0, &res);
	}
}
EXPORT_SYMBOL_GPL(imgsys_adl_set);

static void dump_adl_register(struct mtk_imgsys_dev *imgsys_dev,
			      u32 reg_base_pa, void __iomem *reg_base_va, u32 size)
{
	s32 index;

	for (index = 0; index <= size; index += 0x10) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(u32)(reg_base_pa + index),
			(u32)ioread32((reg_base_va + index)),
			(u32)ioread32((reg_base_va + index + 0x4)),
			(u32)ioread32((reg_base_va + index + 0x8)),
			(u32)ioread32((reg_base_va + index + 0xC)));
	}
}

static u32 dump_debug_data(struct mtk_imgsys_dev *imgsys_dev,
			   u32 reg_base_pa, void __iomem *reg_base_va, u32 sel_reg_ofst,
			   u32 debug_cmd, u32 data_reg_ofst)
{
	u32 value;

	iowrite32(debug_cmd, (reg_base_va + sel_reg_ofst));
	value = (u32)ioread32((reg_base_va + data_reg_ofst));
	pr_info("[0x%08x:0x%08X](0x%08X,0x%08X)\n",
		(reg_base_pa + sel_reg_ofst), debug_cmd, (reg_base_pa + data_reg_ofst), value);

	return value;
}

static void dump_dma_debug_data(struct mtk_imgsys_dev *imgsys_dev,
				u32 reg_base_pa, void __iomem *reg_base_va)
{
	u32 debug_cmd;
	u32 debug_data;
	u32 debug_rdy;
	u32 debug_req;

	/* cmd_length[15:0], mdle_cnt[15:0] */
	debug_cmd = 0x0A0;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_even]mdle_cnt(0x%X), cmd_length(0x%X)\n",
		debug_data & 0x0FFFF, (debug_data >> 16) & 0x0FFFF);

	/* sot_st, eol_st, eot_st, sof,sot, eol, eot, req, rdy,7b0, checksum_out */
	debug_cmd = 0x100;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	debug_rdy = ((debug_data & 0x0800000) > 0) ? 1 : 0;
	debug_req = ((debug_data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_ipui_dma_even]checksum(0x%X),rdy(%d) req(%d)\n",
		debug_data & 0x0FFFF, debug_rdy, debug_req);

	/* line_cnt[15:0],  pix_cnt[15:0] */
	debug_cmd = 0x200;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_even]pix_cnt(0x%X), line_cnt(0x%X)\n",
		debug_data & 0x0FFFF, (debug_data >> 16) & 0x0FFFF);

	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	debug_cmd = 0x300;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_even]pix_cnt_reg(0x%X), line_cnt_reg(0x%X)\n",
		debug_data & 0xFFFF, (debug_data >> 16) & 0x0FFFF);

	debug_cmd = 0x400;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x500;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x600;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x700;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x800;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x900;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	/* cmd_length[15:0], mdle_cnt[15:0] */
	debug_cmd = 0x0A1;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_odd]mdle_cnt(0x%X), cmd_length(0x%X)\n",
		debug_data & 0x0FFFF, (debug_data >> 16) & 0x0FFFF);

	/* sot_st, eol_st, eot_st, sof,sot, eol, eot, req, rdy,7b0, checksum_out */
	debug_cmd = 0x1100;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	debug_rdy = ((debug_data & 0x0800000) > 0) ? 1 : 0;
	debug_req = ((debug_data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_ipui_dma_odd]checksum(0x%X),rdy(%d) req(%d)\n",
		debug_data & 0x0FFFF, debug_rdy, debug_req);

	/* line_cnt[15:0],  pix_cnt[15:0] */
	debug_cmd = 0x1200;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_odd]pix_cnt(0x%X), line_cnt(0x%X)\n",
		debug_data & 0x0FFFF, (debug_data >> 16) & 0x0FFFF);

	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	debug_cmd = 0x1300;
	debug_data = dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
				     debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
	pr_info("[adl_ipui_dma_odd]pix_cnt_reg(0x%X), line_cnt_reg(0x%X)\n",
		debug_data & 0xFFFF, (debug_data >> 16) & 0x0FFFF);

	debug_cmd = 0x1400;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x1500;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x1600;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x1700;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x1800;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);

	debug_cmd = 0x1900;
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IPUDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_DMA_0_DEBUG);
}

static void dump_cq_debug_data(struct mtk_imgsys_dev *imgsys_dev,
			       u32 reg_base_pa, void __iomem *reg_base_va)
{
	void __iomem *cq_ctrl = reg_base_va + IMGADLCQ_CQ_EN;
	u32 debug_cmd;

	/* Set ADLCQ_CQ_EN[28] to 1 */
	iowrite32(0x10000000, cq_ctrl);

	/* cqd0_checksum0 */
	debug_cmd = (0x02 << 8) | (0x00 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqd0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x01 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqd0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x02 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqd1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x04 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqd1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x05 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqd1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x06 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa0_checksum0 */
	debug_cmd = (0x02 << 8) | (0x08 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa0_checksum1 */
	debug_cmd = (0x02 << 8) | (0x09 << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa0_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0A << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa1_checksum0 */
	debug_cmd = (0x02 << 8) | (0x0C << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa1_checksum1 */
	debug_cmd = (0x02 << 8) | (0x0D << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);

	/* cqa1_checksum2 */
	debug_cmd = (0x02 << 8) | (0x0E << 4);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, IMGADL_ADL_DMA_0_DEBUG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DEBUG);
}

static void dump_cq_rdma_status(struct mtk_imgsys_dev *imgsys_dev,
				u32 reg_base_pa, void __iomem *reg_base_va)
{
	u32 debug_cmd;

	/* 0x3: cq_rdma_req_st     */
	debug_cmd = (0x3 << 8);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, CQP2ENGDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DMA_DEBUG);

	/* 0x4: cq_rdma_rdy_st     */
	debug_cmd = (0x4 << 8);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, CQP2ENGDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DMA_DEBUG);

	/* 0x5: cq_rdma_valid      */
	debug_cmd = (0x5 << 8);
	dump_debug_data(imgsys_dev, reg_base_pa, reg_base_va, CQP2ENGDMATOP_DMA_DBG_SEL,
			debug_cmd, IMGADL_ADL_CQ_DMA_DEBUG);
}

void imgsys_adl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, u32 engine)
{
	if (!have_adl)
		return;

	if (engine & IMGSYS_ENG_ADL_A) {
		dump_dma_debug_data(imgsys_dev, IMGADL_A_REG_BASE, g_adl_a_va);

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, IMGADL_A_REG_BASE, g_adl_a_va);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, IMGADL_A_REG_BASE, g_adl_a_va);

		/* dump adl register map */
		dump_adl_register(imgsys_dev, IMGADL_A_REG_BASE, g_adl_a_va, IMGADL_A_DUMP_SIZE);
	}

	if (engine & IMGSYS_ENG_ADL_B) {
		dump_dma_debug_data(imgsys_dev, IMGADL_B_REG_BASE, g_adl_b_va);

		/* 0x2: cq_debug_data */
		dump_cq_debug_data(imgsys_dev, IMGADL_B_REG_BASE, g_adl_b_va);

		/* 0x3 ~ 0x5 cq rdma status */
		dump_cq_rdma_status(imgsys_dev, IMGADL_B_REG_BASE, g_adl_b_va);

		/* dump adl register map */
		dump_adl_register(imgsys_dev, IMGADL_B_REG_BASE, g_adl_b_va, IMGADL_B_DUMP_SIZE);
	}
}
EXPORT_SYMBOL_GPL(imgsys_adl_debug_dump);

void imgsys_adl_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (have_adl) {
		if (g_adl_a_va) {
			iounmap(g_adl_a_va);
			g_adl_a_va = NULL;
		}

		if (g_adl_b_va) {
			iounmap(g_adl_b_va);
			g_adl_b_va = NULL;
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_adl_uninit);
