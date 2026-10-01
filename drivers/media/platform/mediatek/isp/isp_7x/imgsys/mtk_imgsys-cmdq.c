// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */
#include <linux/dma-mapping.h>
#include <linux/mailbox_controller.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-plat.h"

static struct workqueue_struct *imgsys_cmdq_wq;
static u32 is_stream_off;

static struct imgsys_event_history event_hist[IMGSYS_CMDQ_SYNC_POOL_NUM];
static void imgsys_cmdq_task_cb(struct mbox_client *cl, void *mssg);

static void imgsys_destroy_pkt(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = (struct cmdq_client *)pkt->cl;

	dma_unmap_single(client->chan->mbox->dev, pkt->pa_base, pkt->buf_size,
			 DMA_TO_DEVICE);
	kfree(pkt->va_base);
}

static int imgsys_create_pkt(struct cmdq_pkt *pkt, struct cmdq_client *client, u32 size)
{
	struct device *dev;
	dma_addr_t dma_addr;

	pkt->va_base = kzalloc(size, GFP_KERNEL);

	if (!pkt->va_base)
		return -ENOMEM;

	pkt->buf_size = size;
	pkt->cl = (void *)client;

	if (!pkt->cl || !client->chan || !client->chan->mbox)
		return -EINVAL;

	dev = client->chan->mbox->dev;
	dma_addr = dma_map_single(dev, pkt->va_base, pkt->buf_size,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma_addr)) {
		dev_err(dev, "dma map failed, size=%u\n", (u32)(u64)size);
		kfree(pkt->va_base);
		return -ENOMEM;
	}

	pkt->pa_base = dma_addr;

	return 0;
}

static struct cmdq_client *imgsys_clt[IMGSYS_ENG_MAX];

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	u32 idx = 0;

	/* Only first user has to do init work queue */
	if (nr_imgsys_dev == 1) {
		imgsys_cmdq_wq =
			alloc_ordered_workqueue("%s",
						__WQ_LEGACY | WQ_MEM_RECLAIM |
						WQ_FREEZABLE,
						"imgsys_cmdq_cb_wq");
		if (!imgsys_cmdq_wq)
			return;
	}

	switch (nr_imgsys_dev) {
	case 1: /* DIP */
		/* request thread by index (in dts) 0 */
		for (idx = 0; idx < IMGSYS_ENG_MAX; idx++) {
			imgsys_clt[idx] = kzalloc(sizeof(*imgsys_clt[idx]), GFP_KERNEL);
			if (!imgsys_clt[idx])
				return;

			imgsys_clt[idx]->client.dev = dev;
			imgsys_clt[idx]->client.tx_block = false;
			imgsys_clt[idx]->client.knows_txdone = true;
			imgsys_clt[idx]->client.rx_callback = imgsys_cmdq_task_cb;
			imgsys_clt[idx]->chan =
			mbox_request_channel(&imgsys_clt[idx]->client, idx);
		}

		/* parse hardware event */
		for (idx = 0; idx < IMGSYS_CMDQ_EVENT_MAX; idx++)
			of_property_read_u16(dev->of_node,
					     imgsys_event[idx].dts_name,
					     &imgsys_event[idx].event);
		break;
	default:
		break;
	}
}

void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	/* Destroy cmdq client */
	for (idx = 0; idx < IMGSYS_ENG_MAX; idx++) {
		cmdq_mbox_destroy(imgsys_clt[idx]);
		imgsys_clt[idx] = NULL;
	}

	/* Release work_quque */
	flush_workqueue(imgsys_cmdq_wq);
	destroy_workqueue(imgsys_cmdq_wq);
	imgsys_cmdq_wq = NULL;
}

void imgsys_cmdq_streamon(struct mtk_imgsys_dev *imgsys_dev)
{
	dev_info(imgsys_dev->dev, "%s: cmdq stream on (%d)\n", __func__, is_stream_off);
	is_stream_off = 0;

	memset((void *)event_hist, 0x0,
	       sizeof(struct imgsys_event_history) * IMGSYS_CMDQ_SYNC_POOL_NUM);
}

void imgsys_cmdq_streamoff(struct mtk_imgsys_dev *imgsys_dev)
{
	is_stream_off = 1;
}

static void imgsys_cmdq_cmd_dump(struct swfrm_info_t *frm_info, u32 frm_idx)
{
	struct gce_recorder *cmd_buf = NULL;
	struct command *cmd = NULL;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;

	cmd_buf = (struct gce_recorder *)frm_info->user_info[frm_idx].g_swbuf;
	cmd_num = cmd_buf->curr_length / sizeof(struct command);

	if (sizeof(struct gce_recorder) != (u64)cmd_buf->cmd_offset) {
		pr_info("%s: [ERROR]cmd offset is not match (0x%x/0x%zu)!\n",
			__func__, cmd_buf->cmd_offset, sizeof(struct gce_recorder));
		return;
	}

	cmd = (struct command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
		(unsigned long)(cmd_buf->cmd_offset));

	for (cmd_idx = 0; cmd_idx < cmd_num; cmd_idx++) {
		switch (cmd[cmd_idx].opcode) {
		case IMGSYS_CMD_READ:
			pr_info("READ with source(0x%08llx) target(0x%08llx) mask(0x%08x)\n",
				cmd[cmd_idx].u.source, cmd[cmd_idx].u.target, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WRITE:
			pr_debug("WRITE with addr(0x%08llx) value(0x%08x) mask(0x%08x)\n",
				 cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_POLL:
			pr_info("POLL with addr(0x%08llx) value(0x%08x) mask(0x%08x)\n",
				cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WAIT:
			pr_info("WAIT event(%d/%d) action(%d)\n",
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_UPDATE:
			pr_info("UPDATE event(%d/%d) action(%d)\n",
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_ACQUIRE:
			pr_info("ACQUIRE event(%d/%d) action(%d)\n",
				cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
				cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_TIME:
			pr_info("%s: Get cmdq TIME stamp\n", __func__);
			break;
		case IMGSYS_CMD_STOP:
			pr_info("%s: End Of Cmd!\n", __func__);
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd[cmd_idx].opcode);
			break;
		}
	}
}

static void imgsys_cmdq_cb_work(struct work_struct *work)
{
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dev *imgsys_dev = NULL;
	u32 cb_frm_cnt;
	bool last_task_in_req;

	cb_param = container_of(work, struct mtk_imgsys_cb_param, cmdq_cb_work);
	imgsys_dev = cb_param->imgsys_dev;

	dev_dbg(imgsys_dev->dev,
		"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%zx/%zx/%zx/%zx/%zx)\n",
		__func__, cb_param, cb_param->group_id,
		cb_param->blk_idx,  cb_param->blk_num,
		cb_param->frm_idx, cb_param->frm_num,
		cb_param->is_blk_last, cb_param->is_frm_last, cb_param->is_task_last,
		cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
		cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
		cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);

	if (cb_param->err != 0)
		dev_info(imgsys_dev->dev,
			 "%s: [ERROR] cb(%p) error(%d) gid(%d) for frm(%d/%d) blk(%d/%d) lst(%d/%d) earlycb(%d) task(%d/%d/%d) ofst(%zx/%zx/%zx/%zx/%zx)",
			 __func__, cb_param, cb_param->err, cb_param->group_id,
			 cb_param->frm_idx, cb_param->frm_num,
			 cb_param->blk_idx, cb_param->blk_num,
			 cb_param->is_blk_last, cb_param->is_frm_last,
			 cb_param->is_earlycb,
			 cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			 cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			 cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
	if (is_stream_off == 1)
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)!\n",
			__func__, cb_param, is_stream_off);

	dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) is_blk_last(%d) is_frm_last(%d) isECB(%d) isGPLast(%d) isGPECB(%d) for frm(%d/%d)\n",
		__func__, cb_param->frm_info->request_fd,
		cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
		cb_param, cb_param->frm_info, cb_param->is_blk_last,
		cb_param->is_frm_last, cb_param->is_earlycb,
		cb_param->frm_info->user_info[cb_param->frm_idx].is_lastingroup,
		cb_param->frm_info->user_info[cb_param->frm_idx].is_earlycb,
		cb_param->frm_idx, cb_param->frm_num);

	cb_param->frm_info->cb_frmcnt++;
	cb_frm_cnt = cb_param->frm_info->cb_frmcnt;

	dev_dbg(imgsys_dev->dev,
		"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) is_blk_last(%d) cb_param->frm_num(%d) cb_frm_cnt(%d)\n",
		__func__, cb_param->frm_info->request_fd,
		cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
		cb_param, cb_param->frm_info, cb_param->is_blk_last, cb_param->frm_num,
		cb_frm_cnt);

	if (cb_param->is_blk_last && cb_param->user_cmdq_cb &&
	    (cb_param->frm_info->total_taskcnt == cb_frm_cnt || cb_param->is_earlycb)) {
		struct imgsys_cb_data user_cb_data;

		if (cb_param->frm_info->total_taskcnt == cb_frm_cnt)
			last_task_in_req = 1;
		else
			last_task_in_req = 0;

		user_cb_data.cmdq_data.sta = cb_param->err;
		user_cb_data.cmdq_data.pkt = NULL;
		user_cb_data.data = (void *)cb_param->frm_info;
		cb_param->user_cmdq_cb(user_cb_data, cb_param->frm_idx, last_task_in_req);
	}

	imgsys_destroy_pkt(&cb_param->pkt);
	vfree(cb_param);
}

static void imgsys_cmdq_task_cb(struct mbox_client *cl, void *mssg)
{
	struct cmdq_cb_data *data = mssg;
	struct mtk_imgsys_cb_param *cb_param =
		container_of(data->pkt, struct mtk_imgsys_cb_param, pkt);

	struct mtk_imgsys_pipe *pipe;
	size_t err_ofst;
	u32 idx = 0, err_idx = 0, real_frm_idx = 0;

	cb_param->err = data->sta;

	pr_debug("%s: Receive cb(%p) with sta(%d) for frm(%d/%d)\n",
		 __func__, cb_param, data->sta, cb_param->frm_idx, cb_param->frm_num);

	if (cb_param->err != 0) {
		err_ofst = 0; /* cb_param->pkt->err_data.offset; */
		err_idx = 0;
		for (idx = 0; idx < cb_param->task_cnt; idx++)
			if (err_ofst > cb_param->pkt_ofst[idx])
				err_idx++;
			else
				break;
		if (err_idx >= cb_param->task_cnt) {
			pr_info("%s: [ERROR] can't find task in task list! cb(%p) error(%d) gid(%d) for frm(%d/%d) blk(%d/%d) erridx(%d/%d) task(%d/%d/%d) ofst(%zx/%zx/%zx/%zx/%zx)",
				__func__, cb_param, cb_param->err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				err_idx, real_frm_idx,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
				cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
				cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
			err_idx = cb_param->task_cnt - 1;
		}
		real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + err_idx;
		pr_info("%s: [ERROR] cb(%p) error(%d) gid(%d) for frm(%d/%d) blk(%d/%d) lst(%d/%d/%d) earlycb(%d) erridx(%d/%d) task(%d/%d/%d) ofst(%zx/%zx/%zx/%zx/%zx)",
			__func__, cb_param, cb_param->err, cb_param->group_id,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->is_blk_last, cb_param->is_frm_last, cb_param->is_task_last,
			cb_param->is_earlycb,
			err_idx, real_frm_idx,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
		if (is_stream_off == 1)
			pr_info("%s: [ERROR] cb(%p) pipe had been turned off(%d)!\n",
				__func__, cb_param, is_stream_off);
		pipe = (struct mtk_imgsys_pipe *)cb_param->frm_info->pipe;
		if (!pipe->streaming) {
			/* is_stream_off = 1; */
			pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
				__func__, cb_param, is_stream_off);
		}

		imgsys_cmdq_cmd_dump(cb_param->frm_info, real_frm_idx);

		if (cb_param->user_cmdq_err_cb) {
			struct imgsys_cb_data user_cb_data;

			user_cb_data.cmdq_data.pkt = NULL;
			user_cb_data.cmdq_data.sta = cb_param->err;
			user_cb_data.data = (void *)cb_param->frm_info;
			cb_param->user_cmdq_err_cb(user_cb_data, real_frm_idx, 1);
		}
	}

	INIT_WORK(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work);
	queue_work(imgsys_cmdq_wq, &cb_param->cmdq_cb_work);
}

static int imgsys_cmdq_parser(struct swfrm_info_t *frm_info, struct cmdq_pkt *pkt,
			      struct command *cmd, u32 hw_comb, u32 cmd_num,
			      dma_addr_t dma_pa, u32 *num, u32 thd_idx)
{
	bool stop = 0;
	int count = 0;
	int req_fd = 0, req_no = 0, frm_no = 0;
	u32 event = 0;

	req_fd = frm_info->request_fd;
	req_no = frm_info->request_no;
	frm_no = frm_info->frame_no;

	do {
		switch (cmd->opcode) {
		case IMGSYS_CMD_READ:
			/* do nothing */
			break;
		case IMGSYS_CMD_WRITE:
			if (cmd->u.address < IMGSYS_REG_START ||
			    cmd->u.address > IMGSYS_REG_END) {
				pr_info("%s: [ERROR] WRITE with addr(0x%08llx) value(0x%08x) mask(0x%08x)\n",
					__func__, cmd->u.address, cmd->u.value, cmd->u.mask);
				return -1;
			}
			pr_debug("%s: WRITE with addr(0x%08llx) value(0x%08x) mask(0x%08x)\n",
				 __func__, cmd->u.address, cmd->u.value, cmd->u.mask);
			/* assign address high bit to temp SPR0 */
			cmdq_pkt_assign(pkt, CMDQ_THR_SPR_IDX0,
					CMDQ_ADDR_HIGH(cmd->u.address));
			if (cmd->u.mask != GENMASK(31, 0))
				cmdq_pkt_write_s_mask_value(pkt, CMDQ_THR_SPR_IDX0,
							    CMDQ_ADDR_LOW(cmd->u.address),
							    cmd->u.value, cmd->u.mask);
			else
				cmdq_pkt_write_s_value(pkt, CMDQ_THR_SPR_IDX0,
						       CMDQ_ADDR_LOW(cmd->u.address),
						       cmd->u.value);
			break;
		case IMGSYS_CMD_POLL:
			if (cmd->u.address < IMGSYS_REG_START ||
			    cmd->u.address > IMGSYS_REG_END) {
				pr_info("%s: [ERROR] POLL with addr(0x%08llx) value(0x%08x) mask(0x%08x)\n",
					__func__, cmd->u.address, cmd->u.value, cmd->u.mask);
				return -1;
			}
			pr_debug("%s: POLL with addr(0x%08llx) value(0x%08x) mask(0x%08x) thd(%d)\n",
				 __func__, cmd->u.address, cmd->u.value, cmd->u.mask, thd_idx);
			cmdq_pkt_poll_addr(pkt, cmd->u.address, cmd->u.value, cmd->u.mask);
			break;
		case IMGSYS_CMD_WAIT:
			if (cmd->u.event >= IMGSYS_CMDQ_EVENT_MAX) {
				pr_info("%s: [ERROR] WAIT event(%d) index is over maximum(%d) with action(%d)!\n",
					__func__, cmd->u.event, IMGSYS_CMDQ_EVENT_MAX,
					cmd->u.action);
				return -1;
			}
			pr_debug("%s: WAIT event(%d/%d) action(%d)\n",
				 __func__, cmd->u.event, imgsys_event[cmd->u.event].event,
				 cmd->u.action);
			if (cmd->u.action == 1) {
				cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event, true);
				if (cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START &&
				    cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st++;
					event_hist[event].wait.req_fd = req_fd;
					event_hist[event].wait.req_no = req_no;
					event_hist[event].wait.frm_no = frm_no;
					event_hist[event].wait.ts = div_u64(ktime_get_boottime_ns(), 1000);
					event_hist[event].wait.frm_info = frm_info;
					event_hist[event].wait.pkt = pkt;
				}
			} else if (cmd->u.action == 0) {
				cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event, false);
			}
			break;
		case IMGSYS_CMD_UPDATE:
			if (cmd->u.event >= IMGSYS_CMDQ_EVENT_MAX) {
				pr_info("%s: [ERROR] UPDATE event(%d) index is over maximum(%d) with action(%d)!\n",
					__func__, cmd->u.event, IMGSYS_CMDQ_EVENT_MAX,
					cmd->u.action);
				return -1;
			}
			pr_debug("%s: UPDATE event(%d/%d) action(%d)\n",
				 __func__, cmd->u.event, imgsys_event[cmd->u.event].event,
				 cmd->u.action);
			if (cmd->u.action == 1) {
				cmdq_pkt_set_event(pkt, imgsys_event[cmd->u.event].event);
				if (cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START &&
				    cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st--;
					event_hist[event].set.req_fd = req_fd;
					event_hist[event].set.req_no = req_no;
					event_hist[event].set.frm_no = frm_no;
					event_hist[event].set.ts = div_u64(ktime_get_boottime_ns(), 1000);
					event_hist[event].set.frm_info = frm_info;
					event_hist[event].set.pkt = pkt;
				}
			} else if (cmd->u.action == 0) {
				cmdq_pkt_clear_event(pkt, imgsys_event[cmd->u.event].event);
			}
			break;
		case IMGSYS_CMD_ACQUIRE:
			if (cmd->u.event < 0 || cmd->u.event >= IMGSYS_CMDQ_EVENT_MAX) {
				pr_info("%s: [ERROR] ACQUIRE event(%d) index is over maximum(%d) with action(%d)!\n",
					__func__, cmd->u.event, IMGSYS_CMDQ_EVENT_MAX,
					cmd->u.action);
				return -1;
			}
			pr_debug("%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
				 cmd->u.event, imgsys_event[cmd->u.event].event, cmd->u.action);
			cmdq_pkt_acquire_event(pkt, imgsys_event[cmd->u.event].event);
			break;
		case IMGSYS_CMD_STOP:
			pr_debug("%s: End Of Cmd!\n", __func__);
			stop = 1;
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd->opcode);
			return -1;
		}
		cmd++;
		count++;
	} while ((stop == 0) && (count < cmd_num));

	return count;
}

int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
			 struct swfrm_info_t *frm_info,
			 void (*cmdq_cb)(struct imgsys_cb_data data,
					 u32 subfidx, bool last_task_in_req),
			 void (*cmdq_err_cb)(struct imgsys_cb_data data,
					     u32 fail_subfidx, bool hw_hang))
{
	struct cmdq_client *clt = NULL;
	//struct cmdq_pkt *pkt = NULL;
	struct gce_recorder *cmd_buf = NULL;
	struct command *cmd = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	dma_addr_t pkt_ts_pa = 0;
	u32 pkt_ts_num = 0;
	u32 pkt_ts_ofst = 0;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 blk_idx = 0; /* For Vss block cnt */
	u32 thd_idx = 0;
	u32 hw_comb = 0;
	int ret = 0, ret_flush = 0;
	u32 frm_num = 0, frm_idx = 0;
	bool is_pack = 0;
	u32 task_idx = 0;
	u32 task_id = 0;
	u32 task_num = 0;
	u32 task_cnt = 0;
	size_t pkt_ofst[MAX_FRAME_IN_TASK] = {0};

	/* is_stream_off = 0; */
	frm_num = frm_info->total_frmnum;
	frm_info->cb_frmcnt = 0;
	frm_info->total_taskcnt = 0;

	for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
		cmd_buf = (struct gce_recorder *)frm_info->user_info[frm_idx].g_swbuf;

		if (cmd_buf->header_code != 0x5A5A5A5A ||
		    cmd_buf->check_pre != 0x55AA55AA ||
		    cmd_buf->check_post != 0xAA55AA55 ||
		    cmd_buf->footer_code != 0xA5A5A5A5) {
			pr_info("%s: Incorrect guard word: %08x/%08x/%08x/%08x", __func__,
				cmd_buf->header_code, cmd_buf->check_pre, cmd_buf->check_post,
				cmd_buf->footer_code);
			return -1;
		}

		cmd_num = cmd_buf->curr_length / sizeof(struct command);
		cmd = (struct command *)((unsigned long)frm_info->user_info[frm_idx].g_swbuf +
			(unsigned long)cmd_buf->cmd_offset);
		hw_comb = frm_info->user_info[frm_idx].hw_comb;

		if (is_pack == 0) {
			if (frm_info->group_id == -1) {
				/* Choose cmdq_client base on hw scenario */
				for (thd_idx = 0; thd_idx < IMGSYS_ENG_MAX; thd_idx++) {
					if (hw_comb & 0x1) {
						clt = imgsys_clt[thd_idx];
						pr_debug("%s: chosen mbox thread (%d, %p) for frm(%d/%d)\n",
							 __func__, thd_idx, clt, frm_idx, frm_num);
						break;
					}
					hw_comb = hw_comb >> 1;
				}
				/* This segment can be removed since user had set dependency */
				if (frm_info->user_info[frm_idx].hw_comb & IMGSYS_ENG_DIP) {
					thd_idx = 4;
					clt = imgsys_clt[thd_idx];
				}
			} else {
				if (frm_info->group_id >= 0 &&
				    frm_info->group_id < IMGSYS_NOR_THD) {
					thd_idx = frm_info->group_id;
					clt = imgsys_clt[thd_idx];
				} else {
					pr_info("%s: [ERROR] group_id(%d) is not in range(%d) for hw_comb(0x%x) frm(%d/%d)!\n",
						__func__, frm_info->group_id, IMGSYS_NOR_THD,
						frm_info->user_info[frm_idx].hw_comb,
						frm_idx, frm_num);
					return -1;
				}
			}

			/* This is work around for low latency flow.		*/
			/* If we change to request base,			*/
			/* we don't have to take this condition into account.	*/
			if (frm_info->sync_id != -1) {
				thd_idx = 0;
				clt = imgsys_clt[thd_idx];
			}

			if (!clt) {
				pr_info("%s: [ERROR] No HW Found (0x%x) for frm(%d/%d)!\n",
					__func__, frm_info->user_info[frm_idx].hw_comb,
					frm_idx, frm_num);
				return -1;
			}
		}

		cmd_idx = 0;

		for (blk_idx = 0; blk_idx < cmd_buf->frame_block; blk_idx++) {
			if (is_pack == 0) {
				/* Prepare cb param */
				cb_param = vzalloc(sizeof(*cb_param));
				if (!cb_param)
					return -1;

				dev_dbg(imgsys_dev->dev,
					"%s: cb_param vzalloc success cb(%p) in block(%d) for frm(%d/%d)!\n",
					__func__, cb_param, blk_idx, frm_idx, frm_num);

				/* create pkt and hook clt as pkt's private data */
				if (imgsys_create_pkt(&cb_param->pkt, clt, 0x00004000)) {
					vfree(cb_param);
					pr_info("%s: [ERROR] imgsys_create_pkt fail in block(%d)!\n",
						__func__, blk_idx);
					return -1;
				}

				dev_dbg(imgsys_dev->dev,
					"%s: imgsys_create_pkt success(%p) in block(%d) for frm(%d/%d)\n",
					__func__, &cb_param->pkt, blk_idx, frm_idx, frm_num);

				/* Reset pkt timestamp num */
				pkt_ts_num = 0;
			}

			ret = imgsys_cmdq_parser(frm_info, &cb_param->pkt, &cmd[cmd_idx],
						 hw_comb, cmd_num, (pkt_ts_pa + 4 * pkt_ts_ofst),
						 &pkt_ts_num, thd_idx);

			if (ret < 0) {
				pr_info("%s: [ERROR] parsing idx(%d) with cmd(%d) in block(%d) for frm(%d/%d) fail\n",
					__func__, cmd_idx, cmd[cmd_idx].opcode,
					blk_idx, frm_idx, frm_num);
				imgsys_destroy_pkt(&cb_param->pkt);
				vfree(cb_param);
				goto sendtask_done;
			}
			cmd_idx += ret;

			/* Check for packing gce task */
			pkt_ofst[task_cnt] = cb_param->pkt.cmd_buf_size - CMDQ_INST_SIZE;
			task_cnt++;
			if (frm_info->user_info[frm_idx].is_time_shared ||
			    frm_info->user_info[frm_idx].is_sec_frm ||
			    frm_info->user_info[frm_idx].is_earlycb ||
			    frm_idx + 1 == frm_num) {
				task_num++;
				//cb_param->pkt = pkt;
				cb_param->frm_info = frm_info;
				cb_param->frm_idx = frm_idx;
				cb_param->frm_num = frm_num;
				cb_param->user_cmdq_cb = cmdq_cb;
				cb_param->user_cmdq_err_cb = cmdq_err_cb;
				if ((blk_idx + 1) == cmd_buf->frame_block)
					cb_param->is_blk_last = 1;
				else
					cb_param->is_blk_last = 0;
				if ((frm_idx + 1) == frm_num)
					cb_param->is_frm_last = 1;
				else
					cb_param->is_frm_last = 0;
				cb_param->blk_idx = blk_idx;
				cb_param->blk_num = cmd_buf->frame_block;
				cb_param->is_earlycb = frm_info->user_info[frm_idx].is_earlycb;
				cb_param->group_id = frm_info->group_id;
				cb_param->imgsys_dev = imgsys_dev;
				cb_param->thd_idx = thd_idx;
				cb_param->clt = clt;
				cb_param->task_cnt = task_cnt;
				for (task_idx = 0; task_idx < task_cnt; task_idx++)
					cb_param->pkt_ofst[task_idx] = pkt_ofst[task_idx];
				task_cnt = 0;
				cb_param->task_id = task_id;
				task_id++;
				if (cb_param->is_blk_last && cb_param->is_frm_last) {
					cb_param->is_task_last = 1;
					cb_param->task_num = task_num;
					frm_info->total_taskcnt = task_num;
				} else {
					cb_param->is_task_last = 0;
					cb_param->task_num = 0;
				}

				dev_dbg(imgsys_dev->dev,
					"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%zx/%zx/%zx/%zx/%zx)\n",
					__func__, cb_param, cb_param->group_id,
					cb_param->blk_idx,  cb_param->blk_num,
					cb_param->frm_idx, cb_param->frm_num,
					cb_param->is_blk_last, cb_param->is_frm_last,
					cb_param->is_task_last, cb_param->task_id,
					cb_param->task_num, cb_param->task_cnt,
					cb_param->pkt_ofst[0], cb_param->pkt_ofst[1],
					cb_param->pkt_ofst[2], cb_param->pkt_ofst[3],
					cb_param->pkt_ofst[4]);

				/* flush synchronized, block API */
				cmdq_pkt_finalize(&cb_param->pkt);
				dma_sync_single_for_device(clt->chan->mbox->dev,
							   cb_param->pkt.pa_base,
							   cb_param->pkt.cmd_buf_size,
							   DMA_TO_DEVICE);

				mbox_send_message(clt->chan, &cb_param->pkt);
				mbox_client_txdone(clt->chan, 0);
				if (ret_flush < 0)
					pr_info("%s: [ERROR] cmdq_pkt_flush_async fail(%d) for frm(%d/%d)!\n",
						__func__, ret_flush, frm_idx, frm_num);
				is_pack = 0;
			} else {
				is_pack = 1;
			}
		}
	}

sendtask_done:
	return 0;
}
