// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>

#include "../mtk_imgsys-scp.h"
#include "../mtk_imgsys-debug.h"
#include "../mtk_imgsys-engine.h"
#include "mtk_imgsys-pqdip.h"

/********************************************************************
 * Global Define
 ********************************************************************/
#define PQDIP_HW_SET		2

#define PQDIP_BASE_ADDR		0x15210000
#define PQDIP_OFST		0x300000
#define PQDIP_REG_RANGE		0x6000

#define PQDIP_CTL_OFST		0x0
#define PQDIP_CQ_OFST		0x200
#define PQDIP_DMA_OFST		0x1200
#define PQDIP_WROT1_OFST	0x2000
#define PQDIP_WROT2_OFST	0x2F00
#define PQDIP_URZ6T_OFST	0x3000
#define PQDIP_TDSHP1OFST	0x4120
#define PQDIP_TDSHP2OFST	0x4510
#define PQDIP_TDSHP3OFST	0x4570
#define PQDIP_UNP1_OFST		0x5000
#define PQDIP_UNP2_OFST		0x5040
#define PQDIP_UNP3_OFST		0x5080
#define PQDIP_C02_OFST		0x5100
#define PQDIP_C24_OFST		0x5140
#define PQDIP_DRZ8T_OFST	0x5180
#define PQDIP_DRZ1N_OFST	0x5280
#define PQDIP_MCRP_OFST		0x53C0
#define PQDIP_TCC_OFST		0x59E0

#define PQDIP_CTL_REG_CNT		0xE0
#define PQDIP_CQ_REG_CNT		0x140
#define PQDIP_DMA_REG_CNT		0x260
#define PQDIP_WROT1_REG_CNT		0x100
#define PQDIP_WROT2_REG_CNT		0x50
#define PQDIP_URZ6T_REG_CNT		0x260
#define PQDIP_TDSHP1REG_CNT		0x10
#define PQDIP_TDSHP2REG_CNT		0x10
#define PQDIP_TDSHP3REG_CNT		0x10
#define PQDIP_UNP_REG_CNT		0x10
#define PQDIP_C02_REG_CNT		0x20
#define PQDIP_C24_REG_CNT		0x10
#define PQDIP_DRZ8T_REG_CNT		0x30
#define PQDIP_DRZ1N_REG_CNT		0x30
#define PQDIP_MCRP_REG_CNT		0x10
#define PQDIP_TCC_REG_CNT		0x20

#define PQDIPCTL_DBG_SEL_OFST	0xE0
#define PQDIPCTL_DBG_OUT_OFST	0xE4
#define PQ_WROT_DBG_SEL_OFST	0x2018
#define PQ_WROT_DBG_OUT_OFST	0x20D0
#define PQ_URZ6T_DBG_SEL_OFST	0x3044
#define PQ_URZ6T_DBG_OUT_OFST	0x3048

/********************************************************************
 * Global Variable
 ********************************************************************/
static const struct mtk_imgsys_init_array mtk_imgsys_pqdip_init_ary[] = {
	{0x0050, 0x80000000},	/* PQDIPCTL_P1A_REG_PQDIPCTL_INT1_EN */
	{0x0060, 0x0},		/* PQDIPCTL_P1A_REG_PQDIPCTL_INT2_EN */
	{0x0070, 0x0},		/* PQDIPCTL_P1A_REG_PQDIPCTL_CQ_INT1_EN */
	{0x0080, 0x0},		/* PQDIPCTL_P1A_REG_PQDIPCTL_CQ_INT2_EN */
	{0x0090, 0x0},		/* PQDIPCTL_P1A_REG_PQDIPCTL_CQ_INT3_EN */
	{0x0208, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR0_CTL */
	{0x0218, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR1_CTL */
	{0x0228, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR2_CTL */
	{0x0238, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR3_CTL */
	{0x0248, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR4_CTL */
	{0x0258, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR5_CTL */
	{0x0268, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR6_CTL */
	{0x0278, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR7_CTL */
	{0x0288, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR8_CTL */
	{0x0298, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR9_CTL */
	{0x02A8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR10_CTL */
	{0x02B8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR11_CTL */
	{0x02C8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR12_CTL */
	{0x02D8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR13_CTL */
	{0x02E8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR14_CTL */
	{0x02F8, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR15_CTL */
	{0x0308, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR16_CTL */
	{0x0318, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR17_CTL */
	{0x0328, 0x11},		/* DIPCQ_P1A_REG_DIPCQ_CQ_THR18_CTL */
};

#define PQDIP_INIT_ARRAY_COUNT	ARRAY_SIZE(mtk_imgsys_pqdip_init_ary)

static void __iomem *gpqdip_reg_base[PQDIP_HW_SET];

void imgsys_pqdip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0;

	for (hw_idx = 0 ; hw_idx < PQDIP_HW_SET ; hw_idx++) {
		gpqdip_reg_base[hw_idx] =
			of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_PQDIP_A + hw_idx);
	}
}
EXPORT_SYMBOL_GPL(imgsys_pqdip_set_initial_value);

void imgsys_pqdip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *ofset = NULL;
	unsigned int hw_idx = 0;
	unsigned int i = 0;

	for (hw_idx = 0 ; hw_idx < PQDIP_HW_SET ; hw_idx++) {
		for (i = 0 ; i < PQDIP_INIT_ARRAY_COUNT ; i++) {
			ofset = gpqdip_reg_base[hw_idx]
				+ mtk_imgsys_pqdip_init_ary[i].ofset;
			writel(mtk_imgsys_pqdip_init_ary[i].val, ofset);
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_pqdip_set_hw_initial_value);

void imgsys_pqdip_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine)
{
	void __iomem *pqdip_reg_base;
	unsigned int hw_idx = 0;
	unsigned int i = 0;

	for (hw_idx = 0 ; hw_idx < PQDIP_HW_SET ; hw_idx++) {
		pqdip_reg_base = gpqdip_reg_base[hw_idx];
#ifdef DUMP_PQ_ALL
		for (i = 0x0; i < PQDIP_REG_RANGE; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + i,
				 ioread32(pqdip_reg_base + i + 0x00),
				 ioread32(pqdip_reg_base + i + 0x04),
				 ioread32(pqdip_reg_base + i + 0x08),
				 ioread32(pqdip_reg_base + i + 0x0c));
		}
#else
		dev_info(imgsys_dev->dev, "%s:  ctl_reg", __func__);
		for (i = 0x0; i < PQDIP_CTL_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_CTL_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_CTL_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_CTL_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_CTL_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_CTL_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  cq_reg", __func__);
		for (i = 0; i < PQDIP_CQ_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_CQ_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_CQ_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_CQ_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_CQ_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_CQ_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  dma_reg", __func__);
		for (i = 0; i < PQDIP_DMA_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_DMA_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_DMA_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_DMA_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_DMA_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_DMA_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  wrot_reg", __func__);
		for (i = 0; i < PQDIP_WROT1_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + (PQDIP_OFST * hw_idx) + PQDIP_WROT1_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_WROT1_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_WROT1_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_WROT1_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_WROT1_OFST + i + 0x0c));
		}
		for (i = 0; i < PQDIP_WROT2_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_WROT2_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_WROT2_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_WROT2_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_WROT2_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_WROT2_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  urz6t_reg", __func__);
		for (i = 0; i < PQDIP_URZ6T_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_URZ6T_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_URZ6T_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_URZ6T_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_URZ6T_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_URZ6T_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  tdshp_reg", __func__);
		for (i = 0; i < PQDIP_TDSHP1REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_TDSHP1OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_TDSHP1OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP1OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP1OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP1OFST + i + 0x0c));
		}
		for (i = 0; i < PQDIP_TDSHP2REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_TDSHP2OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_TDSHP2OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP2OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP2OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP2OFST + i + 0x0c));
		}
		for (i = 0; i < PQDIP_TDSHP3REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_TDSHP3OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_TDSHP3OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP3OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP3OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_TDSHP3OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  unp_reg", __func__);
		for (i = 0; i < PQDIP_UNP_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_UNP1_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_UNP1_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_UNP1_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_UNP1_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_UNP1_OFST + i + 0x0c));
		}
		for (i = 0; i < PQDIP_UNP_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_UNP2_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_UNP2_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_UNP2_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_UNP2_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_UNP2_OFST + i + 0x0c));
		}
		for (i = 0; i < PQDIP_UNP_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_UNP3_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_UNP3_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_UNP3_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_UNP3_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_UNP3_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  c02_reg", __func__);
		for (i = 0; i < PQDIP_C02_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_C02_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_C02_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_C02_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_C02_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_C02_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  c24_reg", __func__);
		for (i = 0; i < PQDIP_C24_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_C24_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_C24_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_C24_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_C24_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_C24_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  drz8t_reg", __func__);
		for (i = 0; i < PQDIP_DRZ8T_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_DRZ8T_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_DRZ8T_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_DRZ8T_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_DRZ8T_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_DRZ8T_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  drz1n_reg", __func__);
		for (i = 0; i < PQDIP_DRZ1N_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_DRZ1N_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_DRZ1N_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_DRZ1N_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_DRZ1N_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_DRZ1N_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  mcrp_reg", __func__);
		for (i = 0; i < PQDIP_MCRP_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_MCRP_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_MCRP_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_MCRP_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_MCRP_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_MCRP_OFST + i + 0x0c));
		}

		dev_info(imgsys_dev->dev, "%s:  tcc_reg", __func__);
		for (i = 0; i < PQDIP_TCC_REG_CNT; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s:  [0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				 __func__,
				 PQDIP_BASE_ADDR + PQDIP_OFST * hw_idx + PQDIP_TCC_OFST + i,
				 ioread32(pqdip_reg_base + PQDIP_TCC_OFST + i + 0x00),
				 ioread32(pqdip_reg_base + PQDIP_TCC_OFST + i + 0x04),
				 ioread32(pqdip_reg_base + PQDIP_TCC_OFST + i + 0x08),
				 ioread32(pqdip_reg_base + PQDIP_TCC_OFST + i + 0x0c));
		}
#endif

		dev_info(imgsys_dev->dev, "%s: tdr debug\n", __func__);
		iowrite32(0x80004, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tdr   sel(0x80004): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));

		dev_info(imgsys_dev->dev, "%s: module debug\n", __func__);
		iowrite32(0x00001, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: urz6t sel(0x00001): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00101, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tdshp sel(0x00101): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10201, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tcc   sel(0x10201): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20201, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tcc   sel(0x20201): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30201, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tcc   sel(0x30201): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40201, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: tcc   sel(0x40201): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00301, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: wrot  sel(0x00301): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x10401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x20401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x30401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x40401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x50401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x50401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x60401, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp1  sel(0x60401): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x10501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x20501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x30501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x40501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x50501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x50501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x60501, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp2  sel(0x60501): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x10601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x20601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x30601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x40601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x50601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x50601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x60601, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: unp3  sel(0x60601): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00701, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: plnr  sel(0x00701): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00801, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: c02   sel(0x00801): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10801, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: c02   sel(0x10801): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20801, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: c02   sel(0x20801): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30801, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: c02   sel(0x30801): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10901, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: c24   sel(0x10901): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10a01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: mcrp  sel(0x10a01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20a01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: mcrp  sel(0x20a01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30a01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: mcrp  sel(0x30a01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40a01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: mcrp  sel(0x40a01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x50a01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: mcrp  sel(0x50a01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x00b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x10b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x10b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x20b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x20b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x30b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x30b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x40b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x40b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x50b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x50b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x60b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x60b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x70b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x70b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x80b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x80b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x90b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0x90b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0xa0b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0xa0b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0xb0b01, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: r2b   sel(0xb0b01): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00106, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: wif   sel(0x00106): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00107, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: wif   sel(0x00107): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));
		iowrite32(0x00108, pqdip_reg_base + PQDIPCTL_DBG_SEL_OFST);
		dev_info(imgsys_dev->dev, "%s: wif   sel(0x00108): %08X", __func__,
			 ioread32(pqdip_reg_base + PQDIPCTL_DBG_OUT_OFST));

		dev_info(imgsys_dev->dev, "%s: wrot debug\n", __func__);
		for (i = 1; i <= 32; i += 1) {
			iowrite32(i << 8, pqdip_reg_base + PQ_WROT_DBG_SEL_OFST);
			dev_info(imgsys_dev->dev, "%s: sel(0x%04x): %08X", __func__, i << 8,
				 ioread32(pqdip_reg_base + PQ_WROT_DBG_OUT_OFST));
		}

		dev_info(imgsys_dev->dev, "%s: urz6t debug\n", __func__);
		for (i = 0; i < 16; i += 1) {
			iowrite32(i, pqdip_reg_base + PQ_URZ6T_DBG_SEL_OFST);
			dev_info(imgsys_dev->dev, "%s: sel(0x%02x): %08X", __func__, i,
				 ioread32(pqdip_reg_base + PQ_URZ6T_DBG_OUT_OFST));
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_pqdip_debug_dump);

void imgsys_pqdip_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			   struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	char *pqdip_name;
	char file_name[NDD_FP_SIZE] = "\0";
	ssize_t ret;
	void *cq_va;

	if (frm_dump_info->eng_e != IMGSYS_NDD_ENG_PQDIP_A &&
	    frm_dump_info->eng_e != IMGSYS_NDD_ENG_PQDIP_B)
		return;

	cq_va = imgsys_scp_get_reserve_mem_virt(PQDIP_MEM_C_ID);
	if (!cq_va)
		return;

	cq_va += frm_dump_info->cq_ofst[frm_dump_info->eng_e];

	switch (frm_dump_info->eng_e) {
	case IMGSYS_NDD_ENG_PQDIP_A:
		pqdip_name = frm_dump_info->user_buffer ?
			"REG_PQDIP_A_pqdipa.reg" : "REG_PQDIP_A_pqdipa.regKernel";
		break;
	case IMGSYS_NDD_ENG_PQDIP_B:
		pqdip_name = frm_dump_info->user_buffer ?
			"REG_PQDIP_B_pqdipb.reg" : "REG_PQDIP_B_pqdipb.regKernel";
		break;
	default:
		return;
	}

	ret = snprintf(file_name, sizeof(file_name), "%s%s", frm_dump_info->fp, pqdip_name);
	if (ret < 0 || ret >= sizeof(file_name)) {
		dev_err(imgsys_dev->dev, "wrong dip ndd file name %s\n", file_name);
		return;
	}

	if (frm_dump_info->user_buffer) {
		ret = copy_to_user(frm_dump_info->user_buffer, file_name, sizeof(file_name));
		if (ret)
			return;
		frm_dump_info->user_buffer += sizeof(file_name);

		ret = copy_to_user(frm_dump_info->user_buffer, cq_va, PQDIP_REG_RANGE);
		if (ret)
			return;
		frm_dump_info->user_buffer += PQDIP_REG_RANGE;
	}
}
EXPORT_SYMBOL_GPL(imgsys_pqdip_ndd_dump);

void imgsys_pqdip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < PQDIP_HW_SET; i++) {
		iounmap(gpqdip_reg_base[i]);
		gpqdip_reg_base[i] = NULL;
	}
}
EXPORT_SYMBOL_GPL(imgsys_pqdip_uninit);

