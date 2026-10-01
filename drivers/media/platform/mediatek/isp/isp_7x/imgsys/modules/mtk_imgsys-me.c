// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include "mtk_imgsys-me.h"
#include "../mtk_imgsys-debug.h"
#include "../mtk_imgsys-engine.h"

#define ME_REG_RANGE       0xA20

/* Base address for the motion engine registers, mapped to I/O memory */
static void __iomem *me_reg_base;

void imgsys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	me_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ME);
	if (!me_reg_base) {
		dev_info(imgsys_dev->dev,
			 "%s: error: unable to iomap ME register, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
	}
	dev_info(imgsys_dev->dev, "%s: ME module initial value set successfully.\n", __func__);
}
EXPORT_SYMBOL(imgsys_me_set_initial_value);

void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	iounmap(me_reg_base);
	me_reg_base = NULL;
}
EXPORT_SYMBOL(imgsys_me_uninit);

void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine)
{
	unsigned int i;
	struct resource res;
	int ret;

	ret = of_address_to_resource(imgsys_dev->dev->of_node, REG_MAP_E_ME, &res);
	if (ret) {
		dev_err(imgsys_dev->dev, "Failed to get resource from device tree\n");
		return;
	}

	if (!me_reg_base) {
		dev_info(imgsys_dev->dev, "%s: of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}
	dev_info(imgsys_dev->dev, "%s: dump me regs\n", __func__);
	for (i = 0; i <= ME_REG_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
			 (unsigned int)(res.start + i),
			 ioread32(me_reg_base + i),
			 ioread32(me_reg_base + i + 0x4),
			 ioread32(me_reg_base + i + 0x8),
			 ioread32(me_reg_base + i + 0xC));
	}
}
EXPORT_SYMBOL(imgsys_me_debug_dump);

void imgsys_me_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	char *me_name;
	char file_name[NDD_FP_SIZE] = "\0";
	void *reg_va;
	ssize_t ret;

	if (frm_dump_info->eng_e != IMGSYS_NDD_ENG_ME)
		return;

	reg_va = ioremap(frm_dump_info->cq_ofst[frm_dump_info->eng_e], ME_REG_RANGE);
	if (!reg_va)
		return;

	me_name = frm_dump_info->user_buffer ? "REG_ME_me.reg" : "REG_ME_me.regKernel";

	ret = snprintf(file_name, sizeof(file_name), "%s%s", frm_dump_info->fp, me_name);
	if (ret < 0 || ret >= sizeof(file_name)) {
		iounmap(reg_va);
		return;
	}

	if (frm_dump_info->user_buffer) {
		ret = copy_to_user(frm_dump_info->user_buffer, file_name, sizeof(file_name));
		if (ret) {
			iounmap(reg_va);
			return;
		}
		frm_dump_info->user_buffer += sizeof(file_name);

		ret = copy_to_user(frm_dump_info->user_buffer, reg_va, ME_REG_RANGE);
		if (ret != 0) {
			iounmap(reg_va);
			return;
		}
		frm_dump_info->user_buffer += ME_REG_RANGE;
	}

	iounmap(reg_va);
}
EXPORT_SYMBOL(imgsys_me_ndd_dump);

