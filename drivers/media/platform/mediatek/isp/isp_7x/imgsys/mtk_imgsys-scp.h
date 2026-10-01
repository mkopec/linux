/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_IMGSYS_SCP_H
#define MTK_IMGSYS_SCP_H

#include <linux/remoteproc/mtk_scp.h>
#include "mtk-img-ipi.h"
#include "mtk_imgsys-dev.h"

/* the size of share buffer between the Application Processor (AP) and IMGSYS */
#define SCP_SHARE_BUF_SIZE      576

/**
 * struct object_info - Represents information associated with a specific object,
 *                      particularly in the context of inter-processor communication
 *                      and media processing.
 *
 * @ipi:            Specifies the IPI (Inter-Processor Interrupt) ID, using 5 bits.
 * @ack:            Indicates whether an acknowledgment is required, using 1 bit.
 * @req:            Holds the media request ID, using 10 bits.
 * @seq:            A sequence number, using 16 bits, for tracking the order of
 *                  objects or messages.
 */
struct object_info {
	union {
		struct send {
			u32 ipi: 5;  // IPI ID
			u32 ack: 1;  // Acknowledgment flag
			u32 req: 10; // Media request ID
			u32 seq: 16; // Sequence number
		} __packed send;
		u32 cmd; // Combined command representation
	};
} __packed;

/**
 * struct share_buf - DTCM (Data Tightly-Coupled Memory) buffer shared with
 *                    AP and hardware components of IMGSYS
 *
 * @id:             IPI id
 * @len:            Share buffer length
 * @share_buf:      Share buffer data
 * @info:           Object information
 */
struct share_buf {
	u32 id;
	u32 len;
	u8 share_data[SCP_SHARE_BUF_SIZE];
	struct object_info info;
};

enum img_mem_id {
	WPE_MEM_C_ID,		/* Warping Engine (WPE) module command queue (CQ) buffer */
	WPE_MEM_T_ID,		/* WPE module tile driver (TDR) buffer */
	TRAW_MEM_C_ID,		/* Tile-Raw (TRAW) module CQ buffer */
	TRAW_MEM_T_ID,		/* TRAW module TDR buffer */
	DIP_MEM_C_ID,		/* Digital Image Processing (DIP) module CQ buffer */
	DIP_MEM_T_ID,		/* DIP module TDR buffer */
	PQDIP_MEM_C_ID,		/* Picture Quality image processing (PQDIP) CQ buffer */
	PQDIP_MEM_T_ID,		/* PQDIP TDR buffer */
	ADL_MEM_C_ID,		/* ADL Module CQ buffer */
	ADL_MEM_T_ID,		/* ADL Module TDR buffer */
	IMG_MEM_G_ID,		/* GCE command buffer */
	IMG_MEM_ID_COUNT,	/* Total number of memory IDs */
};

/*
 * enum imgsys_ipi_id - the id of inter-processor interrupt
 */
enum imgsys_ipi_id {
	IPI_INIT_ID = 0,
	IPI_ISP_CMD_ID,
	IPI_ISP_FRAME_ID,
	IPI_DIP_INIT_ID,
	IPI_IMGSYS_INIT_ID = IPI_DIP_INIT_ID,
	IPI_DIP_FRAME_ID,
	IPI_IMGSYS_FRAME_ID = IPI_DIP_FRAME_ID,
	IPI_DIP_HW_TIMEOUT_ID,
	IPI_IMGSYS_HW_TIMEOUT_ID = IPI_DIP_HW_TIMEOUT_ID,
	IPI_IMGSYS_SW_TIMEOUT_ID,
	IPI_DIP_DEQUE_DUMP_ID,
	IPI_IMGSYS_DEQUE_DONE_ID,
	IPI_IMGSYS_DEINIT_ID,
	IPI_IMGSYS_IOVA_FDS_ADD_ID,
	IPI_IMGSYS_IOVA_FDS_DEL_ID,
	IPI_IMGSYS_UVA_FDS_ADD_ID,
	IPI_IMGSYS_UVA_FDS_DEL_ID,
	IPI_IMGSYS_SET_CONTROL_ID,
	IPI_IMGSYS_GET_CONTROL_ID,
	IPI_IMGSYS_CLEAR_HWTOKEN_ID,
	IPI_FD_CMD_ID,
	IPI_FD_FRAME_ID,
	IPI_RSC_INIT_ID,
	IPI_RSC_FRAME_ID,
	IPI_MAX_ID,
};


/**
 * imgsys_scp_alloc_reserve_mem - allocate reserved memory for use by the kernel and
 * the SCP (System Control Processor)
 *
 * @imgsys_dev:	imgsys platform device
 *
 **/
int imgsys_scp_alloc_reserve_mem(struct mtk_imgsys_dev *imgsys_dev);

/**
 * imgsys_scp_free_reserve_mem - free reserved memory for use by the kernel and SCP
 *
 * @imgsys_dev:	imgsys platform device
 *
 **/
int imgsys_scp_free_reserve_mem(struct mtk_imgsys_dev *imgsys_dev);

/**
 * imgsys_scp_get_reserve_mem_info - get the information about reserved memory
 *
 * @info:	A pointer to an already allocated structure of type img_init_info,
 * where this function will populate the reserved memory details.
 *
 **/
int imgsys_scp_get_reserve_mem_info(struct img_init_info *info);

/**
 * imgsys_scp_get_reserve_mem_virt - get the virtual address of reserved memory by a
 * particular memory ID
 *
 * @id:	the identifier of the reserved memory block
 *
 **/
void *imgsys_scp_get_reserve_mem_virt(enum img_mem_id id);

/**
 * imgsys_scp_get_reserve_mem_size - get the size of reserved memory by a particular
 *  memory ID
 *
 * @id:	the identifier of the reserved memory block
 *
 **/
unsigned int imgsys_scp_get_reserve_mem_size(enum img_mem_id id);

/**
 * imgsys_scp_init - start the SCP
 *
 * @imgsys_dev:	imgsys platform device
 *
 **/
int imgsys_scp_init(struct mtk_imgsys_dev *imgsys_dev, scp_ipi_handler_t imgsys_scp_handler);

/**
 * imgsys_scp_deinit - stop the SCP
 *
 * @imgsys_dev:	imgsys platform device
 *
 **/
void imgsys_scp_deinit(struct mtk_imgsys_dev *imgsys_dev);

/**
 * imgsys_scp_send - send data from AP to SCP
 *
 * @imgsys_dev:	imgsys platform device
 * @id:			IPI ID
 * @buf:		the data buffer
 * @len:		the data buffer length
 * @req_fd: 	the file descriptor of media request
 * @wait:		wait until ipi done or not
 *
 * This function is thread-safe. When this function returns,
 * SCP has received the data and starts the processing.
 * When the processing completes, IPI handler registered
 * by scp_ipi_register will be called in interrupt context.
 *
 **/
int imgsys_scp_send(struct mtk_imgsys_dev *imgsys_dev, enum imgsys_ipi_id id, void *buf,
                    unsigned int len, int req_fd, unsigned int wait);

#endif /* MTK_IMGSYS_SCP_H */
