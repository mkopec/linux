// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/dma-direction.h>
#include "mtk_imgsys-debug.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-scp.h"

#define SCP_TIMEOUT_MS          4000U

/**
 * struct mtk_scp_reserve_memblock - Represents a memory block reserved by the kernel
 *                                 for usage by the SCP (System Control Processor).
 *
 * @id:         Unique identifier for the memory block. This ID is used to reference
 *              and manage the memory block within the system.
 * @start_phys: Physical address the starting point of the allocated buffer.
 * @start_virt: Kernel virtual address of the starting point of the allocated buffer.
 * @start_dma:  DMA (Direct Memory Access) address of the starting point of the
 *              allocated buffer.
 * @size:       Size of the allocated buffer, in bytes.
 * @dma_dir:	Direction of DMA data transfer, indicating whether the data is being
 *				transferred to the device, from the device, or bidirectionally.
 */
struct mtk_scp_reserve_memblock {
	unsigned int id;
	phys_addr_t start_phys;
	void *start_virt;
	phys_addr_t start_dma;
	unsigned int size;
	enum dma_data_direction dma_dir;
};

/* Array of reserved memory blocks for various hardware components within the image system. */
struct mtk_scp_reserve_memblock reserve_memblock[] = {
	{
		/*WPE CQ buffer for WPE HW usage*/
		.id = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0xE1000,   /*900KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*WPE TDR buffer for tile driver usage*/
		.id = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x196000,   /*1MB + 600KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*TRAW CQ buffer for TRAW driver usage*/
		.id = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x4C8000,   /*4MB + 800KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*TRAW TDR buffer for tile driver usage*/
		.id = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x1AC8000,   /*26MB + 800KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*DIP CQ buffer for TRAW HW usage*/
		.id = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x5C8000,   /*5MB + 800KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*DIP TDR buffer for tile driver usage*/
		.id = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x1FAF000,   /*31MB + 700KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*PQDIP CQ buffer for TRAW HW usage*/
		.id = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*PQDIP TDR buffer for tile driver usage*/
		.id = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x170000,   /*1MB + 500KB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*ADL CQ buffer for TRAW HW usage*/
		.id = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*ADL TDR buffer for tile driver usage*/
		.id = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x200000,   /*2MB*/
		.dma_dir = DMA_TO_DEVICE,
	},
	{
		/*GCE command buffer for GCE HW usage*/
		.id = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = NULL,
		.start_dma  = 0x0,
		.size = 0x1800000,
		.dma_dir = DMA_BIDIRECTIONAL,
	},
};

static phys_addr_t imgsys_scp_get_reserve_mem_phys(enum img_mem_id id)
{
	if (id >= IMG_MEM_ID_COUNT) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	}
	return reserve_memblock[id].start_phys;
}
static phys_addr_t imgsys_scp_get_reserve_mem_dma(enum img_mem_id id)
{
	if (id >= IMG_MEM_ID_COUNT) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	}
	return reserve_memblock[id].start_dma;
}

void *imgsys_scp_get_reserve_mem_virt(enum img_mem_id id)
{
	if (id >= IMG_MEM_ID_COUNT)
		return NULL;
	else
		return reserve_memblock[id].start_virt;
}
EXPORT_SYMBOL(imgsys_scp_get_reserve_mem_virt);

unsigned int imgsys_scp_get_reserve_mem_size(enum img_mem_id id)
{
	if (id >= IMG_MEM_ID_COUNT) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	}
	return reserve_memblock[id].size;
}

int imgsys_scp_alloc_reserve_mem(struct mtk_imgsys_dev *imgsys_dev)
{
	enum img_mem_id id;
	unsigned int block_num;
	void *ptr;
	dma_addr_t addr;

	block_num = ARRAY_SIZE(reserve_memblock);
	for (id = 0; id < block_num; id++) {
		ptr = dma_alloc_coherent(imgsys_dev->smem_dev, reserve_memblock[id].size,
						 &addr, GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;

		reserve_memblock[id].start_virt = ptr;
		reserve_memblock[id].start_phys = addr;
		reserve_memblock[id].start_dma =
			dma_map_resource(imgsys_dev->dev, addr, reserve_memblock[id].size,
						reserve_memblock[id].dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(imgsys_dev->dev, reserve_memblock[id].start_dma)) {
			dev_err(imgsys_dev->dev, "failed to map scp iova\n");
			return -ENOMEM;
		}
	}

	return 0;
}

int imgsys_scp_free_reserve_mem(struct mtk_imgsys_dev *imgsys_dev)
{
	enum img_mem_id id;

	/* release reserved memory */
	for (id = 0; id < IMG_MEM_ID_COUNT; id++) {
		dma_unmap_resource(imgsys_dev->dev,  reserve_memblock[id].start_dma,
						reserve_memblock[id].size,
						reserve_memblock[id].dma_dir,
						DMA_ATTR_SKIP_CPU_SYNC);
		dma_free_coherent(imgsys_dev->smem_dev, reserve_memblock[id].size,
					reserve_memblock[id].start_virt,
					reserve_memblock[id].start_phys);

		reserve_memblock[id].start_dma = 0x0;
		reserve_memblock[id].start_virt = NULL;
		reserve_memblock[id].start_phys = 0x0;
        dev_dbg(imgsys_dev->dev,"%s:[SCP](%d) phys:0x%p, virt:0x%p, dma:0x%p, size:0x%x\n",
		__func__, id, (void *)imgsys_scp_get_reserve_mem_phys(id),
		imgsys_scp_get_reserve_mem_virt(id),
		(void *)imgsys_scp_get_reserve_mem_dma(id),
		imgsys_scp_get_reserve_mem_size(id));
	}

	return 0;
}

int imgsys_scp_get_reserve_mem_info(struct img_init_info *info)
{
	if (!info) {
		pr_err("%s:NULL info\n", __func__);
		return -EINVAL;
	}

	/* WPE */
	info->module_info[IMG_MODULE_WPE].c_wbuf =
				imgsys_scp_get_reserve_mem_phys(WPE_MEM_C_ID);
	info->module_info[IMG_MODULE_WPE].c_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(WPE_MEM_C_ID);
	info->module_info[IMG_MODULE_WPE].c_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(WPE_MEM_C_ID);
	info->module_info[IMG_MODULE_WPE].c_wbuf_fd = 0;
	info->module_info[IMG_MODULE_WPE].t_wbuf =
				imgsys_scp_get_reserve_mem_phys(WPE_MEM_T_ID);
	info->module_info[IMG_MODULE_WPE].t_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(WPE_MEM_T_ID);
	info->module_info[IMG_MODULE_WPE].t_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(WPE_MEM_T_ID);
	info->module_info[IMG_MODULE_WPE].t_wbuf_fd = 0;

	/* ADL */
	info->module_info[IMG_MODULE_ADL].c_wbuf =
				imgsys_scp_get_reserve_mem_phys(ADL_MEM_C_ID);
	info->module_info[IMG_MODULE_ADL].c_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(ADL_MEM_C_ID);
	info->module_info[IMG_MODULE_ADL].c_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(ADL_MEM_C_ID);
	info->module_info[IMG_MODULE_ADL].c_wbuf_fd = 0;
	info->module_info[IMG_MODULE_ADL].t_wbuf =
				imgsys_scp_get_reserve_mem_phys(ADL_MEM_T_ID);
	info->module_info[IMG_MODULE_ADL].t_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(ADL_MEM_T_ID);
	info->module_info[IMG_MODULE_ADL].t_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(ADL_MEM_T_ID);
 	info->module_info[IMG_MODULE_ADL].t_wbuf_fd = 0;

	/* TRAW */
	info->module_info[IMG_MODULE_TRAW].c_wbuf =
				imgsys_scp_get_reserve_mem_phys(TRAW_MEM_C_ID);
	info->module_info[IMG_MODULE_TRAW].c_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(TRAW_MEM_C_ID);
	info->module_info[IMG_MODULE_TRAW].c_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(TRAW_MEM_C_ID);
	info->module_info[IMG_MODULE_TRAW].c_wbuf_fd = 0;
	info->module_info[IMG_MODULE_TRAW].t_wbuf =
				imgsys_scp_get_reserve_mem_phys(TRAW_MEM_T_ID);
	info->module_info[IMG_MODULE_TRAW].t_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(TRAW_MEM_T_ID);
	info->module_info[IMG_MODULE_TRAW].t_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(TRAW_MEM_T_ID);
	info->module_info[IMG_MODULE_TRAW].t_wbuf_fd = 0;

	/* DIP */
	info->module_info[IMG_MODULE_DIP].c_wbuf =
				imgsys_scp_get_reserve_mem_phys(DIP_MEM_C_ID);
	info->module_info[IMG_MODULE_DIP].c_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(DIP_MEM_C_ID);
	info->module_info[IMG_MODULE_DIP].c_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(DIP_MEM_C_ID);
	info->module_info[IMG_MODULE_DIP].c_wbuf_fd = 0;
	info->module_info[IMG_MODULE_DIP].t_wbuf =
				imgsys_scp_get_reserve_mem_phys(DIP_MEM_T_ID);
	info->module_info[IMG_MODULE_DIP].t_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(DIP_MEM_T_ID);
	info->module_info[IMG_MODULE_DIP].t_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(DIP_MEM_T_ID);
	info->module_info[IMG_MODULE_DIP].t_wbuf_fd = 0;

	/* PQDIP */
	info->module_info[IMG_MODULE_PQDIP].c_wbuf =
				imgsys_scp_get_reserve_mem_phys(PQDIP_MEM_C_ID);
	info->module_info[IMG_MODULE_PQDIP].c_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(PQDIP_MEM_C_ID);
	info->module_info[IMG_MODULE_PQDIP].c_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(PQDIP_MEM_C_ID);
	info->module_info[IMG_MODULE_PQDIP].c_wbuf_fd = 0;
	info->module_info[IMG_MODULE_PQDIP].t_wbuf =
				imgsys_scp_get_reserve_mem_phys(PQDIP_MEM_T_ID);
	info->module_info[IMG_MODULE_PQDIP].t_wbuf_dma =
				imgsys_scp_get_reserve_mem_dma(PQDIP_MEM_T_ID);
	info->module_info[IMG_MODULE_PQDIP].t_wbuf_sz =
				imgsys_scp_get_reserve_mem_size(PQDIP_MEM_T_ID);
	info->module_info[IMG_MODULE_PQDIP].t_wbuf_fd = 0;

	/* GCE command */
	info->g_wbuf_fd = 0;
	info->g_wbuf = imgsys_scp_get_reserve_mem_phys(IMG_MEM_G_ID);
	info->g_wbuf_sz = imgsys_scp_get_reserve_mem_size(IMG_MEM_G_ID);

	return 0;
}

int imgsys_scp_init(struct mtk_imgsys_dev *imgsys_dev, scp_ipi_handler_t imgsys_scp_handler)
{
	int ret = 0;

	ret = rproc_boot(imgsys_dev->rproc_handle);
	if (ret) {
		dev_err(imgsys_dev->dev, "%s failed to rproc_boot\n", __func__);
		return ret;
	}

	ret = scp_ipi_register(imgsys_dev->scp, SCP_IPI_IMGSYS_CMD,
			       imgsys_scp_handler, imgsys_dev);
	if (ret) {
		dev_err(imgsys_dev->dev, "%s failed to register IPI cmd\n", __func__);
		return ret;
	}

	dev_info(imgsys_dev->dev, "%s success to register IPI cmd of SCP_IPI_IMGSYS_CMD\n", __func__);

	return ret;
}

void imgsys_scp_deinit(struct mtk_imgsys_dev *imgsys_dev)
{
	rproc_shutdown(imgsys_dev->rproc_handle);
}

int imgsys_scp_send(struct mtk_imgsys_dev *imgsys_dev,
			     enum imgsys_ipi_id id, void *buf,
			     unsigned int len, int req_fd,
			     unsigned int wait)
{
	struct share_buf send_obj = { 0 };
	int ret = 0;

	dev_dbg(imgsys_dev->dev, "%s id:%d len %d\n",
		__func__, id, len);

	if (id < IPI_INIT_ID || id >= IPI_MAX_ID ||
	    len > sizeof(send_obj.share_data) || !buf) {
		dev_info(imgsys_dev->dev,
			 "%s failed to send scp message (Invalid arg.), len/sz(%d/%zu)\n",
			 __func__, len, sizeof(send_obj.share_data));
		return -EINVAL;
	}

	send_obj.len = len;
	send_obj.id = id;
	memcpy((void *)send_obj.share_data, buf, len);

	send_obj.info.send.ipi = id;
	send_obj.info.send.req = req_fd;
	send_obj.info.send.ack = (wait ? 1 : 0);
	send_obj.info.send.seq = 0;

	ret = scp_ipi_send(imgsys_dev->scp, SCP_IPI_IMGSYS_CMD, (void *)&send_obj,
			   sizeof(send_obj), wait ? msecs_to_jiffies(SCP_TIMEOUT_MS) : 0);
	if (ret)
		dev_err(imgsys_dev->dev, "%s: send SCP(%d) failed %d\n", __func__, id, ret);

	return 0;
}
