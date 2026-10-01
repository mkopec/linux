/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */
#ifndef _MTK_IMGSYS_CMDQ_H_
#define _MTK_IMGSYS_CMDQ_H_

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "mtk_imgsys-dev.h"

#define MAX_FRAME_IN_TASK		64

struct imgsys_cb_data {
	struct cmdq_cb_data cmdq_data;
	void *data;
};

struct mtk_imgsys_cb_param {
	struct work_struct cmdq_cb_work;
	//struct cmdq_pkt *pkt;
	struct cmdq_pkt pkt;
	struct swfrm_info_t *frm_info;
	struct mtk_imgsys_dev *imgsys_dev;
	struct cmdq_client *clt;
	void (*user_cmdq_cb)(struct imgsys_cb_data data, u32 subfidx, bool last_task_in_req);
	void (*user_cmdq_err_cb)(struct imgsys_cb_data data, u32 fail_subfidx, bool hw_hang);
	s32 err;
	u32 frm_idx;
	u32 frm_num;
	u32 blk_idx;
	u32 blk_num;
	u32 is_earlycb;
	s32 group_id;
	u32 thd_idx;
	u32 task_id;
	u32 task_num;
	u32 task_cnt;
	size_t pkt_ofst[MAX_FRAME_IN_TASK];
	bool is_blk_last;
	bool is_frm_last;
	bool is_task_last;
};

enum mtk_imgsys_cmd {
	IMGSYS_CMD_LOAD = 0,
	IMGSYS_CMD_MOVE,
	IMGSYS_CMD_READ,
	IMGSYS_CMD_WRITE,
	IMGSYS_CMD_POLL,
	IMGSYS_CMD_WAIT,
	IMGSYS_CMD_UPDATE,
	IMGSYS_CMD_ACQUIRE,
	IMGSYS_CMD_TIME,
	IMGSYS_CMD_STOP
};

enum mtk_imgsys_task_pri {
	IMGSYS_PRI_HIGH   = 95,
	IMGSYS_PRI_MEDIUM = 85,
	IMGSYS_PRI_LOW    = 50
};

struct imgsys_event_info {
	int req_fd;
	int req_no;
	int frm_no;
	u64 ts;
	struct swfrm_info_t *frm_info;
	struct cmdq_pkt *pkt;
	struct mtk_imgsys_cb_param *cb_param;
};

struct imgsys_event_history {
	int st;
	struct imgsys_event_info set;
	struct imgsys_event_info wait;
};

struct imgsys_event_table {
	u16 event;	/* cmdq event enum value */
	char dts_name[256];
};

struct command {
	enum mtk_imgsys_cmd opcode;

	union {
		/* For load, move, read */
		struct {
			u32 mask;
			u64 target;
			u64 source;
		} __packed;

		/* For write and poll */
		struct {
			u32 dummy;
			u64 address;
			u32 value;
		} __packed;

		/* For wait and update event */
		struct {
			u32 event;
			u32 action;
		} __packed;
	} u;
} __packed;

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev);
void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamon(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_cmdq_streamoff(struct mtk_imgsys_dev *imgsys_dev);
int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
			 struct swfrm_info_t *frm_info,
			 void (*cmdq_cb)(struct imgsys_cb_data data,
					 u32 uinfo_idx, bool last_task_in_req),
			 void (*cmdq_err_cb)(struct imgsys_cb_data data,
					     u32 fail_uinfo_idx, bool hw_hang));

#endif /* _MTK_IMGSYS_CMDQ_H_ */
