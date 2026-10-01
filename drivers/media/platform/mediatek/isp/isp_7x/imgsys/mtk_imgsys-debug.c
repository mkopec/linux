// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/uaccess.h>
#include <soc/mediatek/smi.h>
#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-debug.h"

static DEFINE_MUTEX(ndd_mutex);

struct ndd_swfrm_info_t {
	struct mtk_imgsys_dev *imgsys_dev;
	struct swfrm_info_t *gwfrm_info;
	struct list_head list;
} __packed;

LIST_HEAD(ndd_swfrm_list);

#define DEBUGFS_DIR "mtk_imgsys_debug"
#define DEBUGFS_FILE "mtk_imgsys_ndd"
#define FILE_PERMISSIONS 0644

static struct dentry *debugfs_dir, *debugfs_file;

#define WPE_ENG_NUM	3
#define TRAW_ENG_NUM	3
#define DIP_ENG_NUM	1
#define PQDIP_ENG_NUM	2
#define ME_ENG_NUM	1

#define WPE_NDD_SIZE	0x1000
#define TRAW_NDD_SIZE	0xC000
#define DIP_NDD_SIZE	0x5F000
#define PQDIP_NDD_SIZE	0x6000
#define ME_NDD_SIZE	0xA20

struct ndd_request_t {
	int fd;
	int total_frmnum;
} __packed;

static struct ndd_request_t ndd_request;

struct ndd_wpe_info_t {
	char fp[NDD_FP_SIZE];
	u8 data[WPE_NDD_SIZE];
};

struct ndd_traw_info_t {
	char fp[NDD_FP_SIZE];
	u8 data[TRAW_NDD_SIZE];
};

struct ndd_dip_info_t {
	char fp[NDD_FP_SIZE];
	u8 data[DIP_NDD_SIZE];
};

struct ndd_pqdip_info_t {
	char fp[NDD_FP_SIZE];
	u8 data[PQDIP_NDD_SIZE];
};

struct ndd_me_info_t {
	char fp[NDD_FP_SIZE];
	u8 data[ME_NDD_SIZE];
};

struct ndd_frm_info_t {
	struct ndd_wpe_info_t	wpe[WPE_ENG_NUM];
	struct ndd_traw_info_t	traw[TRAW_ENG_NUM];
	struct ndd_dip_info_t	dip[DIP_ENG_NUM];
	struct ndd_pqdip_info_t	pqdip[PQDIP_ENG_NUM];
	struct ndd_me_info_t	me[ME_ENG_NUM];
} __packed;

static ssize_t debugfs_write(struct file *file, const char __user *user_buffer,
			     size_t count, loff_t *ppos)
{
	if (count != sizeof(ndd_request))
		return -EINVAL;

	if (copy_from_user(&ndd_request, user_buffer, count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static ssize_t debugfs_read(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct ndd_swfrm_info_t *cur_ndd_swfrm = NULL, *temp = NULL;
	struct img_swfrm_info *subfrm_info = NULL;
	struct imgsys_ndd_frm_dump_info frm_dump_info = { 0 };
	ssize_t read_count = 0;
	size_t buffer_size = count;
	bool found = false;
	int frm, ret = 0;

	mutex_lock(&ndd_mutex);

	list_for_each_entry_safe(cur_ndd_swfrm, temp, &ndd_swfrm_list, list) {
		if (cur_ndd_swfrm->gwfrm_info &&
		    cur_ndd_swfrm->gwfrm_info->request_fd == ndd_request.fd) {
			found = true;
			for (frm = 0; frm < cur_ndd_swfrm->gwfrm_info->total_frmnum; frm++) {
				subfrm_info = &cur_ndd_swfrm->gwfrm_info->user_info[frm];
				memset(&frm_dump_info, 0, sizeof(frm_dump_info));

				if (buffer_size < sizeof(struct ndd_frm_info_t)) {
					ret = -ENOMEM;
					goto out;
				}

				if (subfrm_info->ndd_fp[0] == '\0')
					continue;

				frm_dump_info.user_buffer = user_buffer +
					sizeof(struct ndd_frm_info_t) * frm;
				frm_dump_info.hw_comb = subfrm_info->hw_comb;
				frm_dump_info.cq_ofst = subfrm_info->ndd_cq_ofst;
				frm_dump_info.wpe_psp_ofst = subfrm_info->ndd_wpe_psp_ofst;
				frm_dump_info.wpe_fp = subfrm_info->ndd_wpe_fp;
				frm_dump_info.fp = subfrm_info->ndd_fp;

				imgsys_ndd_dump_routine(cur_ndd_swfrm->imgsys_dev, &frm_dump_info);
				read_count += sizeof(struct ndd_frm_info_t);
				buffer_size -= sizeof(struct ndd_frm_info_t);
			}

			complete(cur_ndd_swfrm->gwfrm_info->ndd_user_complete);

			list_del(&cur_ndd_swfrm->list);
			kfree(cur_ndd_swfrm);
			cur_ndd_swfrm = NULL;
			break;
		}
	}

	if (!found)
		ret = -ENOENT;
out:
	mutex_unlock(&ndd_mutex);

	return ret == 0 ? read_count : ret;
}

static const struct file_operations debugfs_fops = {
	.write = debugfs_write,
	.read = debugfs_read,
};

#define DL_CHECK_ENG_NUM 11
#define WPE_HW_SET    3
#define ADL_HW_SET    2
#define SW_RST   (0x000C)

static struct imgsys_dbg_engine_t dbg_engine_name_list[DL_CHECK_ENG_NUM] = {
	{IMGSYS_ENG_WPE_EIS, "WPE_EIS"},
	{IMGSYS_ENG_WPE_TNR, "WPE_TNR"},
	{IMGSYS_ENG_WPE_LITE, "WPE_LITE"},
	{IMGSYS_ENG_TRAW, "TRAW"},
	{IMGSYS_ENG_LTR, "LTRAW"},
	{IMGSYS_ENG_XTR, "XTRAW"},
	{IMGSYS_ENG_DIP, "DIP"},
	{IMGSYS_ENG_PQDIP_A, "PQDIPA"},
	{IMGSYS_ENG_PQDIP_B, "PQDIPB"},
	{IMGSYS_ENG_ADL_A, "ADLA"},
	{IMGSYS_ENG_ADL_B, "ADLB"},
};

static void __iomem *imgsys_main_reg_base;
static void __iomem *wpe_dip1_reg_base;
static void __iomem *wpe_dip2_reg_base;
static void __iomem *wpe_dip3_reg_base;
static void __iomem *dip_reg_base;
static void __iomem *dip1_reg_base;
static void __iomem *adla_reg_base;
static void __iomem *adlb_reg_base;

void imgsys_main_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct resource adl;

	imgsys_main_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);
	if (!imgsys_main_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpe_dip1_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE1_DIP1);
	if (!wpe_dip1_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpe_dip2_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE2_DIP1);
	if (!wpe_dip2_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpe_dip3_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE3_DIP1);
	if (!wpe_dip3_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dip_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP);
	if (!dip_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dip1_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP_NR);
	if (!dip1_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top_nr registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	if (of_address_to_resource(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A, &adl)) {
		dev_info(imgsys_dev->dev, "%s: of_address_to_resource fail\n", __func__);
		return;
	}

	if (adl.start) {
		adla_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);
		if (!adla_reg_base) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl a registers\n",
				 __func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				 __func__, imgsys_dev->dev->of_node->name);
			return;
		}

		adlb_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);
		if (!adlb_reg_base) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl b registers\n",
				 __func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				 __func__, imgsys_dev->dev->of_node->name);
			return;
		}
	} else {
		adla_reg_base = NULL;
		adlb_reg_base = NULL;
		dev_info(imgsys_dev->dev, "%s Do not have ADL hardware.\n", __func__);
	}

	debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);
	if (!debugfs_dir) {
		dev_err(imgsys_dev->dev, "%s Failed to create debugfs dir\n", __func__);
		return;
	}

	debugfs_file = debugfs_create_file(DEBUGFS_FILE, FILE_PERMISSIONS,
					   debugfs_dir, NULL, &debugfs_fops);
	if (!debugfs_file) {
		dev_err(imgsys_dev->dev, "%s Failed to create debugfs file\n", __func__);
		return;
	}

	mutex_init(&ndd_mutex);
}

void imgsys_main_set_init(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *wpe_reg_base = NULL;
	void __iomem *adl_reg_base = NULL;
	void __iomem *wpe_ctrl = NULL;
	unsigned int hw_idx = 0;
	u32 count;
	u32 value;
	int i, num;

	num = imgsys_dev->larbs_num;
	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp(imgsys_dev->larbs[i], 1);

	iowrite32(0xFFFFFFFF, dip_reg_base + SW_RST);
	iowrite32(0xFFFFFFFF, dip1_reg_base + SW_RST);

	for (hw_idx = 0; hw_idx < WPE_HW_SET; hw_idx++) {
		if (hw_idx == 0)
			wpe_reg_base = wpe_dip1_reg_base;
		else if (hw_idx == 1)
			wpe_reg_base = wpe_dip2_reg_base;
		else
			wpe_reg_base = wpe_dip3_reg_base;

		/* Wpe Macro HW Reset */
		wpe_ctrl = wpe_reg_base + SW_RST;
		iowrite32(0xFFFFFFFF, wpe_ctrl);
		/* Clear HW Reset */
		iowrite32(0x0, wpe_ctrl);
	}

	if (adla_reg_base || adlb_reg_base) {
		/* Reset ADL A */
		for (hw_idx = 0; hw_idx < ADL_HW_SET; hw_idx++) {
			if (hw_idx == 0)
				adl_reg_base = adla_reg_base;
			else if (hw_idx == 1)
				adl_reg_base = adlb_reg_base;

			if (!adl_reg_base)
				continue;

			value = ioread32(adl_reg_base + 0x300);
			value |= ((0x1 << 8) | (0x1 << 9));
			iowrite32(value, (adl_reg_base + 0x300));

			count = 0;
			while (count < 1000000) {
				value = ioread32(adl_reg_base + 0x300);
				if ((value & 0x3) == 0x3)
					break;
				count++;
			}

			value = ioread32(adl_reg_base + 0x300);
			value &= ~((0x1 << 8) | (0x1 << 9));
			iowrite32(value, adl_reg_base + 0x300);
		}
	}

	iowrite32(0x00CF00FF, imgsys_main_reg_base + SW_RST);
	iowrite32(0x0, imgsys_main_reg_base + SW_RST);

	iowrite32(0x0, dip_reg_base + SW_RST);
	iowrite32(0x0, dip1_reg_base + SW_RST);

	for (hw_idx = 0; hw_idx < WPE_HW_SET; hw_idx++) {
		if (hw_idx == 0)
			wpe_reg_base = wpe_dip1_reg_base;
		else if (hw_idx == 1)
			wpe_reg_base = wpe_dip2_reg_base;
		else
			wpe_reg_base = wpe_dip3_reg_base;

		/* Wpe Macro HW Reset */
		wpe_ctrl = wpe_reg_base + SW_RST;
		iowrite32(0xFFFFFFFF, wpe_ctrl);
		/* Clear HW Reset */
		iowrite32(0x0, wpe_ctrl);
	}

	iowrite32(0x00CF00FF, imgsys_main_reg_base + SW_RST);
	iowrite32(0x0, imgsys_main_reg_base + SW_RST);

	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp(imgsys_dev->larbs[i], 0);

}

void imgsys_main_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (imgsys_main_reg_base) {
		iounmap(imgsys_main_reg_base);
		imgsys_main_reg_base = NULL;
	}

	if (wpe_dip1_reg_base) {
		iounmap(wpe_dip1_reg_base);
		wpe_dip1_reg_base = NULL;
	}

	if (wpe_dip2_reg_base) {
		iounmap(wpe_dip2_reg_base);
		wpe_dip2_reg_base = NULL;
	}

	if (wpe_dip3_reg_base) {
		iounmap(wpe_dip3_reg_base);
		wpe_dip3_reg_base = NULL;
	}

	if (dip_reg_base) {
		iounmap(dip_reg_base);
		dip_reg_base = NULL;
	}

	if (dip1_reg_base) {
		iounmap(dip1_reg_base);
		dip1_reg_base = NULL;
	}

	if (adla_reg_base) {
		iounmap(adla_reg_base);
		adla_reg_base = NULL;
	}

	if (adlb_reg_base) {
		iounmap(adlb_reg_base);
		adlb_reg_base = NULL;
	}

	debugfs_remove(debugfs_file);
	debugfs_remove(debugfs_dir);
}

void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
			       const struct module_ops *imgsys_modules,
			       int imgsys_module_num, unsigned int hw_comb)
{
	bool module_on[IMGSYS_MOD_MAX] = {
		false, false, false, false, false, false, false};
	int i = 0;

	dev_info(imgsys_dev->dev,
		 "%s: hw comb set: 0x%x\n",
		 __func__, hw_comb);

	imgsys_dl_debug_dump(imgsys_dev, hw_comb);

	if ((hw_comb & IMGSYS_ENG_WPE_EIS) || (hw_comb & IMGSYS_ENG_WPE_TNR) ||
	    (hw_comb & IMGSYS_ENG_WPE_LITE))
		module_on[IMGSYS_MOD_WPE] = true;
	if ((hw_comb & IMGSYS_ENG_TRAW) || (hw_comb & IMGSYS_ENG_LTR) ||
	    (hw_comb & IMGSYS_ENG_XTR))
		module_on[IMGSYS_MOD_TRAW] = true;
	if (hw_comb & IMGSYS_ENG_DIP)
		module_on[IMGSYS_MOD_DIP] = true;
	if ((hw_comb & IMGSYS_ENG_PQDIP_A) || (hw_comb & IMGSYS_ENG_PQDIP_B))
		module_on[IMGSYS_MOD_PQDIP] = true;
	if ((hw_comb & IMGSYS_ENG_ME))
		module_on[IMGSYS_MOD_ME] = true;
	if ((hw_comb & IMGSYS_ENG_ADL_A) || (hw_comb & IMGSYS_ENG_ADL_B))
		module_on[IMGSYS_MOD_ADL] = true;

	/* in case module driver did not set imgsys_modules in module order */
	dev_info(imgsys_dev->dev,
		 "%s: imgsys_module_num: %d\n",
		 __func__, imgsys_module_num);
	for (i = 0 ; i < imgsys_module_num ; i++) {
		if (module_on[imgsys_modules[i].module_id])
			imgsys_modules[i].dump(imgsys_dev, hw_comb);
	}
}
EXPORT_SYMBOL_GPL(imgsys_debug_dump_routine);

void imgsys_ndd_swfrm_list_add(struct mtk_imgsys_dev *imgsys_dev,
			       struct swfrm_info_t *gwfrm_info)
{
	struct ndd_swfrm_info_t *new_ndd_swfrm;

	new_ndd_swfrm = kzalloc(sizeof(*new_ndd_swfrm), GFP_KERNEL);
	if (!new_ndd_swfrm)
		return;

	new_ndd_swfrm->imgsys_dev = imgsys_dev;
	new_ndd_swfrm->gwfrm_info = gwfrm_info;

	mutex_lock(&ndd_mutex);
	list_add_tail(&new_ndd_swfrm->list, &ndd_swfrm_list);
	mutex_unlock(&ndd_mutex);
}
EXPORT_SYMBOL_GPL(imgsys_ndd_swfrm_list_add);

void imgsys_ndd_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
			     struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	int i = 0;
	int mid = 0;
	unsigned int hw_num = 0x1;
	unsigned int module_copy_ofst = 0;
	const struct module_ops *imgsys_modules = imgsys_dev->modules;

	dev_dbg(imgsys_dev->dev,
		"%s: hw comb set: 0x%x\n", __func__, frm_dump_info->hw_comb);

	for (i = 0; hw_num < IMGSYS_ENG_MAX_NUM; i++, hw_num = (0x1 << i)) {
		switch (hw_num) {
		case IMGSYS_ENG_WPE_EIS:
		case IMGSYS_ENG_WPE_TNR:
		case IMGSYS_ENG_WPE_LITE:
			module_copy_ofst = (NDD_FP_SIZE + WPE_NDD_SIZE);
			mid = IMGSYS_MOD_WPE;
			break;
		case IMGSYS_ENG_ADL_A:
		case IMGSYS_ENG_ADL_B:
			module_copy_ofst = 0;
			break;
		case IMGSYS_ENG_TRAW:
		case IMGSYS_ENG_LTR:
		case IMGSYS_ENG_XTR:
			module_copy_ofst = (NDD_FP_SIZE + TRAW_NDD_SIZE);
			mid = IMGSYS_MOD_TRAW;
			break;
		case IMGSYS_ENG_DIP:
			module_copy_ofst = (NDD_FP_SIZE + DIP_NDD_SIZE);
			mid = IMGSYS_MOD_DIP;
			break;
		case IMGSYS_ENG_PQDIP_A:
		case IMGSYS_ENG_PQDIP_B:
			module_copy_ofst = (NDD_FP_SIZE + PQDIP_NDD_SIZE);
			mid = IMGSYS_MOD_PQDIP;
			break;
		case IMGSYS_ENG_ME:
			module_copy_ofst = (NDD_FP_SIZE + ME_NDD_SIZE);
			mid = IMGSYS_MOD_ME;
			break;
		default:
			dev_err(imgsys_dev->dev, "%s get wrong hw id %d\n", __func__, hw_num);
			break;
		}

		if (frm_dump_info->hw_comb & hw_num) {
			frm_dump_info->eng_e = i;
			if (frm_dump_info->eng_e <= IMGSYS_NDD_ENG_ME)
				imgsys_modules[mid].ndddump(imgsys_dev, frm_dump_info);
		} else {
			if (frm_dump_info->user_buffer)
				frm_dump_info->user_buffer += module_copy_ofst;
			else
				frm_dump_info->user_buffer = NULL;
		}

		if ((frm_dump_info->hw_comb >> i) == 0)
			return;
	}
}
EXPORT_SYMBOL_GPL(imgsys_ndd_dump_routine);

void imgsys_cg_debug_dump(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i = 0;

	if (!imgsys_main_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x500; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15000000 + i, ioread32(imgsys_main_reg_base + i),
			 0x15000000 + i + 0x4, ioread32(imgsys_main_reg_base + i + 0x4),
			 0x15000000 + i + 0x8, ioread32(imgsys_main_reg_base + i + 0x8),
			 0x15000000 + i + 0xc, ioread32(imgsys_main_reg_base + i + 0xc));
	}

	if (!dip_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15110000 + i, ioread32(dip_reg_base + i),
			 0x15110000 + i + 0x4, ioread32(dip_reg_base + i + 0x4),
			 0x15110000 + i + 0x8, ioread32(dip_reg_base + i + 0x8),
			 0x15110000 + i + 0xc, ioread32(dip_reg_base + i + 0xc));
	}

	if (!dip1_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip1 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15130000 + i, ioread32(dip1_reg_base + i),
			 0x15130000 + i + 0x4, ioread32(dip1_reg_base + i + 0x4),
			 0x15130000 + i + 0x8, ioread32(dip1_reg_base + i + 0x8),
			 0x15130000 + i + 0xc, ioread32(dip1_reg_base + i + 0xc));
	}

	if (!wpe_dip1_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15220000 + i, ioread32(wpe_dip1_reg_base + i),
			 0x15220000 + i + 0x4, ioread32(wpe_dip1_reg_base + i + 0x4),
			 0x15220000 + i + 0x8, ioread32(wpe_dip1_reg_base + i + 0x8),
			 0x15220000 + i + 0xc, ioread32(wpe_dip1_reg_base + i + 0xc));
	}

	if (!wpe_dip2_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15520000 + i, ioread32(wpe_dip2_reg_base + i),
			 0x15520000 + i + 0x4, ioread32(wpe_dip2_reg_base + i + 0x4),
			 0x15520000 + i + 0x8, ioread32(wpe_dip2_reg_base + i + 0x8),
			 0x15520000 + i + 0xc, ioread32(wpe_dip2_reg_base + i + 0xc));
	}

	if (!wpe_dip3_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			 __func__, 0x15620000 + i, ioread32(wpe_dip3_reg_base + i),
			 0x15620000 + i + 0x4, ioread32(wpe_dip3_reg_base + i + 0x4),
			 0x15620000 + i + 0x8, ioread32(wpe_dip3_reg_base + i + 0x8),
			 0x15620000 + i + 0xc, ioread32(wpe_dip3_reg_base + i + 0xc));
	}
}

#define log_length (64)
static void imgsys_dl_checksum_dump(struct mtk_imgsys_dev *imgsys_dev,
				    unsigned int hw_comb, char *logbuf_path,
				    char *logbuf_inport, char *logbuf_outport, int dl_path)
{
	unsigned int checksum_dbg_sel = 0x0;
	unsigned int original_dbg_sel_value = 0x0;
	char logbuf_final[log_length * 4];
	int debug0_req[2] = {0, 0};
	int debug0_rdy[2] = {0, 0};
	int debug0_checksum[2] = {0, 0};
	int debug1_line_cnt[2] = {0, 0};
	int debug1_pix_cnt[2] = {0, 0};
	int debug2_line_cnt[2] = {0, 0};
	int debug2_pix_cnt[2] = {0, 0};
	unsigned int dbg_sel_value[2] = {0x0, 0x0};
	unsigned int debug0_value[2] = {0x0, 0x0};
	unsigned int debug1_value[2] = {0x0, 0x0};
	unsigned int debug2_value[2] = {0x0, 0x0};
	unsigned int wpe_pqdip_mux_v = 0x0;
	unsigned int wpe_pqdip_mux2_v = 0x0;
	unsigned int wpe_pqdip_mux3_v = 0x0;
	char logbuf_temp[log_length];
	int ret;

	memset((char *)logbuf_final, 0x0, log_length * 4);
	logbuf_final[strlen(logbuf_final)] = '\0';
	memset((char *)logbuf_temp, 0x0, log_length);
	logbuf_temp[strlen(logbuf_temp)] = '\0';

	dev_info(imgsys_dev->dev,
		 "%s: + hw_comb/path(0x%x/%s) dl_path:%d, start dump\n",
		 __func__, hw_comb, logbuf_path, dl_path);
	/*imgsys_main_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);*/
	if (!imgsys_main_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	/*dump former engine in DL (imgsys main in port) status */
	checksum_dbg_sel = ((dl_path << 1) | (0 << 0));
	original_dbg_sel_value = ioread32((imgsys_main_reg_base + 0x4C));
	original_dbg_sel_value = original_dbg_sel_value & 0xff00ffff; /*clear last time data*/
	dbg_sel_value[0] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[0], (imgsys_main_reg_base + 0x4C));
	dbg_sel_value[0] = ioread32((imgsys_main_reg_base + 0x4C));
	debug0_value[0] = ioread32((imgsys_main_reg_base + 0x200));
	debug0_checksum[0] = (debug0_value[0] & 0x0000ffff);
	debug0_rdy[0] = (debug0_value[0] & 0x00800000) >> 23;
	debug0_req[0] = (debug0_value[0] & 0x01000000) >> 24;
	debug1_value[0] = ioread32((imgsys_main_reg_base + 0x204));
	debug1_line_cnt[0] = ((debug1_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[0] = (debug1_value[0] & 0x0000ffff);
	debug2_value[0] = ioread32((imgsys_main_reg_base + 0x208));
	debug2_line_cnt[0] = ((debug2_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[0] = (debug2_value[0] & 0x0000ffff);

	/*dump later engine in DL (imgsys main out port) status */
	checksum_dbg_sel = ((dl_path << 1) | (1 << 0));
	dbg_sel_value[1] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[1], (imgsys_main_reg_base + 0x4C));
	dbg_sel_value[1] = ioread32((imgsys_main_reg_base + 0x4C));
	debug0_value[1] = ioread32((imgsys_main_reg_base + 0x200));
	debug0_checksum[1] = (debug0_value[1] & 0x0000ffff);
	debug0_rdy[1] = (debug0_value[1] & 0x00800000) >> 23;
	debug0_req[1] = (debug0_value[1] & 0x01000000) >> 24;
	debug1_value[1] = ioread32((imgsys_main_reg_base + 0x204));
	debug1_line_cnt[1] = ((debug1_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[1] = (debug1_value[1] & 0x0000ffff);
	debug2_value[1] = ioread32((imgsys_main_reg_base + 0x208));
	debug2_line_cnt[1] = ((debug2_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[1] = (debug2_value[1] & 0x0000ffff);

	/* macro_comm status */
	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	/*wpe_dip1_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE1_DIP1);*/
	if (!wpe_dip1_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux_v = ioread32((wpe_dip1_reg_base + 0xA8));
	/*iounmap(wpe_dip1_reg_base);*/

	/*wpe_dip2_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE2_DIP1);*/
	if (!wpe_dip2_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux2_v = ioread32((wpe_dip2_reg_base + 0xA8));
	/*iounmap(wpe_dip2_reg_base);*/

	/*wpe_dip3_reg_base = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE3_DIP1);*/
	if (!wpe_dip3_reg_base) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
			 __func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux3_v = ioread32((wpe_dip3_reg_base + 0xA8));
	/*iounmap(wpe_dip3_reg_base);*/
	/*}*/

	/* dump information */
	if (dl_path == IMGSYS_DL_WPET_TRAW) {
	} else {
		if (debug0_req[0] == 1) {
			snprintf(logbuf_temp, log_length,
				 "%s req to send data to %s/",
				 logbuf_inport, logbuf_outport);
		} else {
			snprintf(logbuf_temp, log_length,
				 "%s not send data to %s/",
				 logbuf_inport, logbuf_outport);
		}
		strncat(logbuf_final, logbuf_temp, strlen(logbuf_temp));
		memset((char *)logbuf_temp, 0x0, log_length);
		logbuf_temp[strlen(logbuf_temp)] = '\0';
		if (debug0_rdy[0] == 1) {
			ret = snprintf(logbuf_temp, log_length,
				       "%s rdy to receive data from %s",
				       logbuf_outport, logbuf_inport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logbuf_temp, log_length,
				       "%s not rdy to receive data from %s",
				       logbuf_outport, logbuf_inport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logbuf_final, logbuf_temp, strlen(logbuf_temp));
		dev_info(imgsys_dev->dev,
			 "%s: %s", __func__, logbuf_final);

		memset((char *)logbuf_final, 0x0, log_length * 4);
		logbuf_final[strlen(logbuf_final)] = '\0';
		memset((char *)logbuf_temp, 0x0, log_length);
		logbuf_temp[strlen(logbuf_temp)] = '\0';
		if (debug0_req[1] == 1) {
			ret = snprintf(logbuf_temp, log_length,
				       "%s req to send data to %sPIPE/",
				       logbuf_outport, logbuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logbuf_temp, log_length,
				       "%s not send data to %sPIPE/",
				       logbuf_outport, logbuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logbuf_final, logbuf_temp, strlen(logbuf_temp));
		memset((char *)logbuf_temp, 0x0, log_length);
		logbuf_temp[strlen(logbuf_temp)] = '\0';
		if (debug0_rdy[1] == 1) {
			ret = snprintf(logbuf_temp, log_length,
				       "%sPIPE rdy to receive data from %s",
				       logbuf_outport, logbuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logbuf_temp, log_length,
				       "%sPIPE not rdy to receive data from %s",
				       logbuf_outport, logbuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logbuf_final, logbuf_temp, strlen(logbuf_temp));
		dev_info(imgsys_dev->dev,
			 "%s: %s", __func__, logbuf_final);
		dev_info(imgsys_dev->dev,
			 "%s: in_req/in_rdy/out_req/out_rdy = %d/%d/%d/%d,(cheskcum: in/out) = (%d/%d)",
			 __func__,
			 debug0_req[0], debug0_rdy[0],
			 debug0_req[1], debug0_rdy[1],
			 debug0_checksum[0], debug0_checksum[1]);
		dev_info(imgsys_dev->dev,
			 "%s: info01 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			 __func__,
			 debug1_line_cnt[0], debug1_pix_cnt[0], debug1_line_cnt[1],
			 debug1_pix_cnt[1]);
		dev_info(imgsys_dev->dev,
			 "%s: info02 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			 __func__,
			 debug2_line_cnt[0], debug2_pix_cnt[0], debug2_line_cnt[1],
			 debug2_pix_cnt[1]);
	}
	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		 __func__, logbuf_path, logbuf_inport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x4C), dbg_sel_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x200), debug0_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x204), debug1_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x208), debug2_value[0]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		 __func__, logbuf_path, logbuf_outport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x4C), dbg_sel_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x200), debug0_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x204), debug1_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15000000 + 0x208), debug2_value[1]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): IMGMAIN CG INFO===",
		 __func__, logbuf_path);
	dev_info(imgsys_dev->dev, "%s: CG_CON  0x%08X %08X", __func__,
		 (0x15000000 + 0x0),
		 ioread32((imgsys_main_reg_base + 0x0)));
	dev_info(imgsys_dev->dev, "%s: CG_SET  0x%08X %08X", __func__,
		 (0x15000000 + 0x4),
		 ioread32((imgsys_main_reg_base + 0x4)));
	dev_info(imgsys_dev->dev, "%s: CG_CLR  0x%08X %08X", __func__,
		 (0x15000000 + 0x8),
		 ioread32((imgsys_main_reg_base + 0x8)));

	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15220000 + 0xA8), wpe_pqdip_mux_v);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15520000 + 0xA8), wpe_pqdip_mux2_v);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		 (0x15620000 + 0xA8), wpe_pqdip_mux3_v);
	/*}*/
	/*iounmap(imgsys_main_reg_base);*/
}

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb)
{
	int dl_path = 0;
	char logbuf_path[log_length];
	char logbuf_inport[log_length];
	char logbuf_outport[log_length];
	char logbuf_eng[log_length];
	int i = 0, get = false;

	memset((char *)logbuf_path, 0x0, log_length);
	logbuf_path[strlen(logbuf_path)] = '\0';
	memset((char *)logbuf_inport, 0x0, log_length);
	logbuf_inport[strlen(logbuf_inport)] = '\0';
	memset((char *)logbuf_outport, 0x0, log_length);
	logbuf_outport[strlen(logbuf_outport)] = '\0';

	for (i = 0 ; i < DL_CHECK_ENG_NUM ; i++) {
		memset((char *)logbuf_eng, 0x0, log_length);
		logbuf_eng[strlen(logbuf_eng)] = '\0';
		if (hw_comb & dbg_engine_name_list[i].eng_e) {
			if (get) {
				snprintf(logbuf_eng, log_length, "-%s",
					 dbg_engine_name_list[i].eng_name);
			} else {
				snprintf(logbuf_eng, log_length, "%s",
					 dbg_engine_name_list[i].eng_name);
			}
			get = true;
		}
		strncat(logbuf_path, logbuf_eng, strlen(logbuf_eng));
	}
	memset((char *)logbuf_eng, 0x0, log_length);
	logbuf_eng[strlen(logbuf_eng)] = '\0';
	snprintf(logbuf_eng, log_length, "%s", " FAIL");
	strncat(logbuf_path, logbuf_eng, strlen(logbuf_eng));

	dev_info(imgsys_dev->dev, "%s: %s\n",
		 __func__, logbuf_path);
	switch (hw_comb) {
	/*DL checksum case*/
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPEE_XTRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPET_XTRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logbuf_inport, log_length, "%s",
			 "WPE_LITE");
		snprintf(logbuf_outport, log_length, "%s",
			 "TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			 "%s: we dont have checksum for WPELITE DL TRAW\n",
			 __func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logbuf_inport, log_length, "%s",
			 "WPE_LITE");
		snprintf(logbuf_outport, log_length, "%s",
			 "LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			 "%s: we dont have checksum for WPELITE DL LTRAW\n",
			 __func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logbuf_inport, log_length, "%s",
			 "WPE_LITE");
		snprintf(logbuf_outport, log_length, "%s",
			 "XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			 "%s: we dont have checksum for WPELITE DL XTRAW\n",
			 __func__);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
	      IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logbuf_inport, log_length, "%s", "TRAW");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logbuf_inport, log_length, "%s", "TRAW");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		/**/
		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEL_TRAW;
		snprintf(logbuf_inport, log_length, "%s", "WPE_LITE");
		snprintf(logbuf_outport, log_length, "%s", "TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			 "%s: we dont have checksum for WPELITE DL TRAW\n",
			 __func__);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logbuf_inport, log_length, "%s", "TRAW");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_EIS");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logbuf_inport, log_length, "%s", "WPE_TNR");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dev_info(imgsys_dev->dev,
			 "%s: TOBE CHECKED SELECTION BASED ON FMT..\n",
			 __func__);
		break;
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logbuf_inport, log_length, "%s", "TRAW");
		snprintf(logbuf_outport, log_length, "%s", "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);

		memset((char *)logbuf_inport, 0x0, log_length);
		logbuf_inport[strlen(logbuf_inport)] = '\0';
		memset((char *)logbuf_outport, 0x0, log_length);
		logbuf_outport[strlen(logbuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logbuf_inport, log_length, "%s", "DIP");
		snprintf(logbuf_outport, log_length, "%s", "PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
					logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_ADL_A | IMGSYS_ENG_XTR):
	case (IMGSYS_ENG_ADL_A | IMGSYS_ENG_ADL_B | IMGSYS_ENG_XTR):
		/**
		 * dl_path = IMGSYS_DL_ADLA_XTRAW;
		 * snprintf(logbuf_inport, log_length, "%s", "ADL");
		 * snprintf(logbuf_outport, log_length, "%s", "XTRAW");
		 * imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
		 *  logbuf_path, logbuf_inport, logbuf_outport, dl_path);
		 */
		dev_info(imgsys_dev->dev,
			 "%s: we dont have checksum for ADL DL XTRAW\n",
			 __func__);
		break;
	case (IMGSYS_ENG_ME):
		imgsys_cg_debug_dump(imgsys_dev);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(imgsys_dl_debug_dump);
