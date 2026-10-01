// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Shih-Fang Chuang <shih-fang.chuang@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <dt-bindings/memory/mtk-memory-port.h>

#include "../mtk_imgsys-scp.h"
#include "../mtk_imgsys-debug.h"
#include "../mtk_imgsys-engine.h"
#include "mtk_imgsys-traw.h"

/********************************************************************
 * Global Define
 ********************************************************************/
#define TRAW_INIT_ARRAY_COUNT	1

#define TRAW_CTL_ADDR_OFST		0x330
#define TRAW_DMA_ADDR_OFST		0x4000
#define TRAW_DMA_ADDR_END		0x5300
#define TRAW_DATA_ADDR_OFST		0x8000
#define TRAW_MAX_ADDR_OFST		0xBD00
#define TRAW_REG_RANGE			0xC000

#define TRAW_HW_SET		3
#define WPE_HW_SET		3

#define TRAW_L9_PORT_CNT	25
#define TRAW_L11_PORT_CNT	16

#define TRAW_M4U   0

/********************************************************************
 * Global Variable
 ********************************************************************/
static const struct mtk_imgsys_init_array
			mtk_imgsys_traw_init_ary[TRAW_INIT_ARRAY_COUNT] = {
	{0x00A0, 0x80000000}, /* TRAWCTL_INT1_EN */
};

static struct traw_dma_dbg_info g_dma_dbg_info[] = {
	{"IMGI", TRAW_ORI_RDMA_DEBUG},
	{"IMGI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"UFDI", TRAW_ORI_RDMA_DEBUG},
	{"IMGBI", TRAW_ORI_RDMA_DEBUG},
	{"IMGBI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"IMGCI", TRAW_ORI_RDMA_DEBUG},
	{"IMGCI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"SMTI_T1", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T2", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T3", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T4", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T5", TRAW_ULC_RDMA_DEBUG},
	{"CACI", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T1", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T2", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T3", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T4", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T5", TRAW_ULC_RDMA_DEBUG},
	{"YUVO_T1", TRAW_ORI_WDMA_DEBUG},
	{"YUVBO_T1", TRAW_ORI_WDMA_DEBUG},
	{"TIMGO", TRAW_ORI_WDMA_DEBUG},
	{"YUVCO", TRAW_ORI_WDMA_DEBUG},
	{"YUVDO", TRAW_ORI_WDMA_DEBUG},
	{"YUVO_T2", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T2", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T3", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T3", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T4", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T4", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T5", TRAW_ORI_WDMA_DEBUG},
	{"TNCSO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSBO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSHO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSYO", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T1", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T2", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T3", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T4", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T5", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T1", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T2", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T3", TRAW_ULC_WDMA_DEBUG},
	{"SMTCI_T1", TRAW_ULC_RDMA_DEBUG},
	{"SMTCI_T4", TRAW_ULC_RDMA_DEBUG},
	{"SMTCO_T1", TRAW_ULC_WDMA_DEBUG},
	{"SMTCO_T4", TRAW_ULC_WDMA_DEBUG},
	{"SMTI_T6", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T7", TRAW_ULC_RDMA_DEBUG},
	{"SMTO_T6", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T7", TRAW_ULC_WDMA_DEBUG},
	{"RZH1N2TO_T1", TRAW_ULC_WDMA_DEBUG},
	{"RZH1N2TBO_T1", TRAW_ULC_WDMA_DEBUG},
	{"DRZS4NO_T1", TRAW_ULC_WDMA_DEBUG},
	{"DBGO_T1", TRAW_ORI_WDMA_DEBUG},
	{"DBGBO_T1", TRAW_ORI_WDMA_DEBUG},
};

static unsigned int g_reg_base_addr = TRAW_A_BASE_ADDR;

static void __iomem *g_traw_reg_ba, *g_ltraw_reg_ba, *g_xtraw_reg_ba;

static unsigned int exe_dbg_cmd(struct mtk_imgsys_dev *imgsys_dev,
				void __iomem *p_reg_ba,
				unsigned int dbd_sel,
				unsigned int dbg_out,
				unsigned int dbg_cmd)
{
	unsigned int data = 0;

	iowrite32(dbg_cmd, p_reg_ba + dbd_sel);
	data = ioread32(p_reg_ba + dbg_out);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		dbg_cmd, g_reg_base_addr + dbg_out, data);

	return data;
}

static void imgsys_traw_dump_dma(struct mtk_imgsys_dev *imgsys_dev,
				 void __iomem *p_reg_ba,
				 unsigned int dbd_sel,
				 unsigned int dbg_out)
{
	unsigned int idx = 0;
	unsigned int dbg_cmd = 0;
	unsigned int dbg_cnt = sizeof(g_dma_dbg_info) / sizeof(struct traw_dma_dbg_info);
	enum traw_dma_dbg_type dbg_type = TRAW_ORI_RDMA_DEBUG;

	/* Dump DMA Debug Info */
	for (idx = 0; idx < dbg_cnt; idx++) {
		/* state_checksum */
		dbg_cmd = TRAW_IMGI_STATE_CHECKSUM + idx;
		exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		/* line_pix_cnt_tmp */
		dbg_cmd = TRAW_IMGI_LINE_PIX_CNT_TMP + idx;
		exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		/* line_pix_cnt */
		dbg_cmd = TRAW_IMGI_LINE_PIX_CNT + idx;
		exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);

		dbg_type = g_dma_dbg_info[idx].dbg_type;

		/* important_status */
		if (dbg_type == TRAW_ULC_RDMA_DEBUG ||
		    dbg_type == TRAW_ULC_WDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_IMPORTANT_STATUS + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* smi_debug_data (case 0) or cmd_data_cnt */
		if (dbg_type == TRAW_ORI_RDMA_DEBUG ||
		    dbg_type == TRAW_ULC_RDMA_DEBUG ||
			dbg_type == TRAW_ULC_WDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_SMI_DEBUG_DATA_CASE0 + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* ULC_RDMA or ULC_WDMA */
		if (dbg_type == TRAW_ULC_RDMA_DEBUG ||
		    dbg_type == TRAW_ULC_WDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_TILEX_BYTE_CNT + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
			dbg_cmd = TRAW_IMGI_TILEY_CNT + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* smi_dbg_data(case 0) or burst_line_cnt or input_v_cnt */
		if (dbg_type == TRAW_ORI_WDMA_DEBUG ||
		    dbg_type == TRAW_ULC_RDMA_DEBUG ||
		    dbg_type == TRAW_ULC_WDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_BURST_LINE_CNT + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* ORI_RDMA */
		if (dbg_type == TRAW_ORI_RDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_FIFO_DEBUG_DATA_CASE1 + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
			dbg_cmd = TRAW_IMGI_FIFO_DEBUG_DATA_CASE3 + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* ORI_WDMA */
		if (dbg_type == TRAW_ORI_WDMA_DEBUG) {
			dbg_cmd = TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE1 + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
			dbg_cmd = TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE3 + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}

		/* xfer_y_cnt */
		if (dbg_type == TRAW_ULC_WDMA_DEBUG) {
			dbg_cmd = TRAW_IMGI_XFER_Y_CNT + idx;
			exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
		}
	}
}

static void imgsys_traw_dump_cq(struct mtk_imgsys_dev *imgsys_dev,
				void __iomem *p_reg_ba,
				unsigned int dbd_sel,
				unsigned int dbg_out)
{
	unsigned int dbg_cmd;

	/* arx/atx/drx/dtx_state */
	dbg_cmd = 0x00000005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* Thr(0~3)_state */
	dbg_cmd = 0x00010005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);

	/* Set DIPCQ_CQ_EN[28] to 1 */
	iowrite32(0x10000000, p_reg_ba + TRAW_DIPCQ_CQ_EN);
	/* cqd0_checksum0 */
	dbg_cmd = 0x00000005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqd0_checksum1 */
	dbg_cmd = 0x00010005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqd0_checksum2 */
	dbg_cmd = 0x00020005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqd1_checksum0 */
	dbg_cmd = 0x00040005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqd1_checksum1 */
	dbg_cmd = 0x00050005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqd1_checksum2 */
	dbg_cmd = 0x00060005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa0_checksum0 */
	dbg_cmd = 0x00080005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa0_checksum1 */
	dbg_cmd = 0x00090005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa0_checksum2 */
	dbg_cmd = 0x000A0005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa1_checksum0 */
	dbg_cmd = 0x000C0005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa1_checksum1 */
	dbg_cmd = 0x000D0005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* cqa1_checksum2 */
	dbg_cmd = 0x000E0005;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
}

static void imgsys_traw_dump_drzh2n(struct mtk_imgsys_dev *imgsys_dev,
				    void __iomem *p_reg_ba,
				    unsigned int dbd_sel,
				    unsigned int dbg_out)
{
	unsigned int dbg_cmd = 0;

	/* drzh2n_t1 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C001;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t1 line_pix_cnt */
	dbg_cmd = 0x0003C001;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t1 handshake signal */
	dbg_cmd = 0x0004C001;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t2 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C101;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t2 line_pix_cnt */
	dbg_cmd = 0x0003C101;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t2 handshake signal */
	dbg_cmd = 0x0004C101;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t3 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C501;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t3 line_pix_cnt */
	dbg_cmd = 0x0003C501;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t3 handshake signal */
	dbg_cmd = 0x0004C501;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t4 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C601;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t4 line_pix_cnt */
	dbg_cmd = 0x0003C601;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t4 handshake signal */
	dbg_cmd = 0x0004C601;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t5 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C701;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t5 line_pix_cnt */
	dbg_cmd = 0x0003C701;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t5 handshake signal */
	dbg_cmd = 0x0004C701;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t6 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C801;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t6 line_pix_cnt */
	dbg_cmd = 0x0003C801;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* drzh2n_t6 handshake signal */
	dbg_cmd = 0x0004C801;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* rzh1n2t_t1 checksum */
	dbg_cmd = 0x0001C201;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* rzh1n2t_t1 tile line_pix_cnt */
	dbg_cmd = 0x0003C201;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* rzh1n2t_t1 tile protocal */
	dbg_cmd = 0x0008C201;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
}

static void imgsys_traw_dump_smto(struct mtk_imgsys_dev *imgsys_dev,
				  void __iomem *p_reg_ba,
				  unsigned int dbd_sel,
				  unsigned int dbg_out)
{
	unsigned int dbg_cmd = 0;

	/* smto_t3 line_pix_cnt_tmp */
	dbg_cmd = 0x0002C401;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* smto_t3 line_pix_cnt */
	dbg_cmd = 0x0003C401;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	/* smto_t3 handshake signal */
	dbg_cmd = 0x0004C401;
	exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
}

static void imgsys_traw_dump_dl(struct mtk_imgsys_dev *imgsys_dev,
				void __iomem *p_reg_ba,
				unsigned int dbd_sel,
				unsigned int dbg_out)
{
	unsigned int dbg_cmd = 0;
	unsigned int data = 0;
	unsigned int linc_cnt = 0, dbg_rdy = 0, dbg_req = 0;
	unsigned int reg = 0;

	/* wpe_wif_t1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000006;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000007;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000008;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* wpe_wif_t2_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000106;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t2_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000107;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t2_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000108;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t2_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* traw_dip_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000206;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[traw_dip_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000207;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[traw_dip_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000208;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[traw_dip_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* wpe_wif_t3_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000306;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t3_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000307;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t3_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000308;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t3_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* adl_wif_t4_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000406;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t4_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000407;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000408;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* adl_wif_t4n_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000506;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t4n_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000507;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4n_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000508;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4n_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);

	/* adl_wif_t5_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	dbg_cmd = 0x00000606;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	dbg_rdy = ((data & 0x800000) > 0) ? 1 : 0;
	dbg_req = ((data & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t5_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		data & 0xFFFF, dbg_rdy, dbg_req);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	dbg_cmd = 0x00000607;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	linc_cnt = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t5_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		data & 0xFFFF, linc_cnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	dbg_cmd = 0x00000608;
	data = exe_dbg_cmd(imgsys_dev, p_reg_ba, dbd_sel, dbg_out, dbg_cmd);
	reg = (data & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t5_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		data & 0xFFFF, reg);
}

void imgsys_traw_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	/* iomap reg base */
	g_traw_reg_ba = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TRAW);
	g_ltraw_reg_ba = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_LTRAW);
	g_xtraw_reg_ba = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_XTRAW);
}
EXPORT_SYMBOL_GPL(imgsys_traw_set_initial_value);

void imgsys_traw_set_initial_value_hw(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *traw_reg_ba = NULL;
	void __iomem *ofset = NULL;
	unsigned int i = 0, idx;

	for (idx = 0; idx < TRAW_HW_SET; idx++) {
		if (idx == 0)
			traw_reg_ba = g_traw_reg_ba;
		else if (idx == 1)
			traw_reg_ba = g_ltraw_reg_ba;
		else
			traw_reg_ba = g_xtraw_reg_ba;

		if (!traw_reg_ba) {
			pr_info("%s: hw(%d)null reg base\n", __func__, idx);
			break;
		}

		for (i = 0 ; i < TRAW_INIT_ARRAY_COUNT ; i++) {
			ofset = traw_reg_ba + mtk_imgsys_traw_init_ary[i].ofset;
			writel(mtk_imgsys_traw_init_ary[i].val, ofset);
		}
	}
}
EXPORT_SYMBOL_GPL(imgsys_traw_set_initial_value_hw);

void imgsys_traw_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine)
{
	void __iomem *traw_reg_ba = NULL;
	unsigned int i;
	unsigned int reg_map = REG_MAP_E_TRAW;

	if (engine & IMGSYS_ENG_LTR) {
		reg_map = REG_MAP_E_LTRAW;
		g_reg_base_addr = TRAW_B_BASE_ADDR;
		traw_reg_ba = g_ltraw_reg_ba;
	} else if (engine & IMGSYS_ENG_XTR) {
		reg_map = REG_MAP_E_XTRAW;
		g_reg_base_addr = TRAW_C_BASE_ADDR;
		traw_reg_ba = g_xtraw_reg_ba;
	} else {
		g_reg_base_addr = TRAW_A_BASE_ADDR;
		traw_reg_ba = g_traw_reg_ba;
	}

	if (!traw_reg_ba) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap regmap(%d)\n",
			 __func__, reg_map);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			 __func__, imgsys_dev->dev->of_node->name);
		return;
	}

	/* DL debug data */
	imgsys_traw_dump_dl(imgsys_dev, traw_reg_ba, TRAW_CTL_DBG_SEL, TRAW_CTL_DBG_PORT);

	/* Ctrl registers */
	for (i = 0x0; i <= TRAW_CTL_ADDR_OFST; i += 16) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_reg_base_addr + i),
			(unsigned int)ioread32(traw_reg_ba + i),
			(unsigned int)ioread32(traw_reg_ba + i + 4),
			(unsigned int)ioread32(traw_reg_ba + i + 8),
			(unsigned int)ioread32(traw_reg_ba + i + 12));
	}
	/* Dma registers */
	for (i = TRAW_DMA_ADDR_OFST; i <= TRAW_DMA_ADDR_END; i += 16) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_reg_base_addr + i),
			(unsigned int)ioread32(traw_reg_ba + i),
			(unsigned int)ioread32(traw_reg_ba + i + 4),
			(unsigned int)ioread32(traw_reg_ba + i + 8),
			(unsigned int)ioread32(traw_reg_ba + i + 12));
	}
	/* Data registers */
	for (i = TRAW_DATA_ADDR_OFST; i <= TRAW_MAX_ADDR_OFST; i += 16) {
		pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_reg_base_addr + i),
			(unsigned int)ioread32(traw_reg_ba + i),
			(unsigned int)ioread32(traw_reg_ba + i + 4),
			(unsigned int)ioread32(traw_reg_ba + i + 8),
			(unsigned int)ioread32(traw_reg_ba + i + 12));
	}

	/* DMA debug data */
	imgsys_traw_dump_dma(imgsys_dev, traw_reg_ba, TRAW_DMA_DBG_SEL, TRAW_DMA_DBG_PORT);
	/* CQ debug data */
	imgsys_traw_dump_cq(imgsys_dev, traw_reg_ba, TRAW_CTL_DBG_SEL, TRAW_CTL_DBG_PORT);
	/* DRZH2N debug data */
	imgsys_traw_dump_drzh2n(imgsys_dev, traw_reg_ba, TRAW_CTL_DBG_SEL, TRAW_CTL_DBG_PORT);
	/* SMTO debug data */
	imgsys_traw_dump_smto(imgsys_dev, traw_reg_ba, TRAW_CTL_DBG_SEL, TRAW_CTL_DBG_PORT);
}
EXPORT_SYMBOL_GPL(imgsys_traw_debug_dump);

void imgsys_traw_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			  struct imgsys_ndd_frm_dump_info *frm_dump_info)
{
	const char *traw_name;
	char file_name[NDD_FP_SIZE] = "\0";
	ssize_t ret;
	void *cq_va;

	if (frm_dump_info->eng_e > IMGSYS_NDD_ENG_XTR ||
	    frm_dump_info->eng_e < IMGSYS_NDD_ENG_TRAW)
		return;

	switch (frm_dump_info->eng_e) {
	case IMGSYS_NDD_ENG_TRAW:
		traw_name = frm_dump_info->user_buffer ?
			"REG_TRAW_traw.reg" : "REG_TRAW_traw.regKernel";
		break;
	case IMGSYS_NDD_ENG_LTR:
		traw_name = frm_dump_info->user_buffer ?
			"REG_LTRAW_ltraw.reg" : "REG_LTRAW_ltraw.regKernel";
		break;
	case IMGSYS_NDD_ENG_XTR:
		traw_name = frm_dump_info->user_buffer ?
			"REG_XTRAW_xtraw.reg" : "REG_XTRAW_xtraw.regKernel";
		break;
	default:
		return;
	}

	cq_va = imgsys_scp_get_reserve_mem_virt(TRAW_MEM_C_ID);
	if (!cq_va)
		return;

	cq_va += frm_dump_info->cq_ofst[frm_dump_info->eng_e];

	ret = snprintf(file_name, sizeof(file_name), "%s%s", frm_dump_info->fp, traw_name);
	if (ret < 0 || ret >= sizeof(file_name)) {
		dev_err(imgsys_dev->dev, "wrong traw ndd file name %s\n", file_name);
		return;
	}

	if (frm_dump_info->user_buffer) {
		ret = copy_to_user(frm_dump_info->user_buffer, file_name, sizeof(file_name));
		if (ret)
			return;
		frm_dump_info->user_buffer += sizeof(file_name);

		ret = copy_to_user(frm_dump_info->user_buffer, cq_va, TRAW_REG_RANGE);
		if (ret)
			return;
		frm_dump_info->user_buffer += TRAW_REG_RANGE;
	}
}
EXPORT_SYMBOL_GPL(imgsys_traw_ndd_dump);

void imgsys_traw_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (g_traw_reg_ba) {
		iounmap(g_traw_reg_ba);
		g_traw_reg_ba = NULL;
	}
	if (g_ltraw_reg_ba) {
		iounmap(g_ltraw_reg_ba);
		g_ltraw_reg_ba = NULL;
	}
	if (g_xtraw_reg_ba) {
		iounmap(g_xtraw_reg_ba);
		g_xtraw_reg_ba = NULL;
	}
}
EXPORT_SYMBOL_GPL(imgsys_traw_uninit);
