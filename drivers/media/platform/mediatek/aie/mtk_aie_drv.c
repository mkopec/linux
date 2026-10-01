// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include "mtk_aie.h"
#include <linux/delay.h>
#include <linux/firmware.h>

static const unsigned int fd_wdma_en[FD_LOOP_NUM][OUTPUT_WDMA_WRA_NUM] = {
	{ 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 0, 1, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 1, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 0, 1, 0 },
	{ 1, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 0, 0, 0 },
	{ 1, 0, 1, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 },
	{ 1, 1, 1, 1 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 },
	{ 1, 0, 1, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 },
	{ 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 1, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }
};

static const unsigned int out_stride_size[FD_LOOP_NUM][OUTPUT_WDMA_WRA_NUM] = {
	{ 1, 0, 0, 0 }, { 1, 0, 2, 0 }, { 1, 0, 2, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 2, 2 }, { 1, 1, 2, 2 }, { 1, 0, 0, 0 }, { 1, 0, 2, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 2, 0 }, { 1, 1, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 3, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 2, 0 }, { 1, 0, 2, 0 },
	{ 1, 0, 0, 0 }, { 1, 1, 2, 2 }, { 1, 1, 2, 2 }, { 1, 0, 0, 0 },
	{ 1, 0, 2, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 2, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 },
	{ 1, 1, 1, 1 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 0, 0, 0 }, { 3, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 2, 0 },
	{ 1, 0, 2, 0 }, { 1, 0, 0, 0 }, { 1, 1, 2, 2 }, { 1, 1, 2, 2 },
	{ 1, 0, 0, 0 }, { 1, 0, 2, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 2, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 3, 0, 0, 0 }
};

static const unsigned int fd_ker_rdma_size[FD_LOOP_NUM][KERNEL_RDMA_RA_NUM] = {
	{ 240, 240 },	{ 1168, 1168 }, { 1168, 1168 }, { 272, 272 },
	{ 2320, 2320 }, { 2080, 2080 }, { 1040, 1040 }, { 4624, 4624 },
	{ 3104, 3104 }, { 9232, 9232 }, { 4624, 4624 }, { 4128, 4128 },
	{ 1040, 1040 }, { 4624, 4624 }, { 4624, 4624 }, { 1552, 1552 },
	{ 4624, 4624 }, { 4624, 4624 }, { 4128, 4128 }, { 1040, 1040 },
	{ 1040, 1040 }, { 528, 528 },	{ 4160, 4160 }, { 4160, 4160 },
	{ 2080, 2080 }, { 2080, 2080 }, { 2080, 2080 }, { 1040, 1040 },
	{ 0, 0 },	{ 240, 240 },	{ 1168, 1168 }, { 1168, 1168 },
	{ 272, 272 },	{ 2320, 2320 }, { 2080, 2080 }, { 1040, 1040 },
	{ 4624, 4624 }, { 3104, 3104 }, { 9232, 9232 }, { 4624, 4624 },
	{ 4128, 4128 }, { 1040, 1040 }, { 4624, 4624 }, { 4624, 4624 },
	{ 1552, 1552 }, { 4624, 4624 }, { 4624, 4624 }, { 4128, 4128 },
	{ 1040, 1040 }, { 1040, 1040 }, { 528, 528 },	{ 4160, 4160 },
	{ 4160, 4160 }, { 2080, 2080 }, { 2080, 2080 }, { 2080, 2080 },
	{ 1040, 1040 }, { 0, 0 },	{ 240, 240 },	{ 1168, 1168 },
	{ 1168, 1168 }, { 272, 272 },	{ 2320, 2320 }, { 2080, 2080 },
	{ 1040, 1040 }, { 4624, 4624 }, { 3104, 3104 }, { 9232, 9232 },
	{ 4624, 4624 }, { 4128, 4128 }, { 1040, 1040 }, { 4624, 4624 },
	{ 4624, 4624 }, { 1552, 1552 }, { 4624, 4624 }, { 4624, 4624 },
	{ 4128, 4128 }, { 1040, 1040 }, { 1040, 1040 }, { 528, 528 },
	{ 4160, 4160 }, { 4160, 4160 }, { 2080, 2080 }, { 2080, 2080 },
	{ 2080, 2080 }, { 1040, 1040 }, { 0, 0 }
};

static const unsigned int fd_out_stride2_in[FD_LOOP_NUM] = {
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned int fd_stride[FD_LOOP_NUM] = {
	2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const unsigned int fd_maxpool[FD_LOOP_NUM] = {
	0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned int out_2size[FD_LOOP_NUM] = {
	0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1,
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned int in_ch_pack[FD_LOOP_NUM] = {
	1,  16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0,  1,	16, 16, 16, 16, 16, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 0,	1,  16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0
};

static const unsigned int outlayer[FD_LOOP_NUM] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
};

static const unsigned int out_ch_pack[FD_LOOP_NUM] = {
	16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 16, 16, 16, 32, 32, 32, 32, 32, 32, 0,  16, 16, 16, 16, 16, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 16, 16, 16, 32, 32, 32,
	32, 32, 32, 0,	16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 16, 16, 16, 32, 32, 32, 32, 32, 32, 0
};

static const unsigned int anchor_en_num[FD_LOOP_NUM] = {
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
};

/* [loop][ch][output_index] */
static const signed int fd_rdma_en[FD_LOOP_NUM][INPUT_WDMA_WRA_NUM][2] = {
	{ { 99, 99 }, { 99, 99 }, { 99, 99 }, { -1, -1 } },
	{ { 0, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 1, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 1, 0 }, { 2, 0 }, { -1, -1 }, { -1, -1 } },
	{ { 3, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 1, 2 }, { 2, 2 }, { 4, 2 }, { 4, 3 } },
	{ { 5, 0 }, { 5, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 6, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 5, 0 }, { 5, 1 }, { 7, 0 }, { -1, -1 } },
	{ { 8, 0 }, { 8, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 9, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 5, 2 }, { 5, 3 }, { 7, 2 }, { 10, 2 } },
	{ { 11, 0 }, { 11, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 12, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 13, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 11, 0 }, { 11, 1 }, { 14, 0 }, { -1, -1 } },
	{ { 15, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 16, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 11, 0 }, { 11, 1 }, { 14, 0 }, { 17, 0 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { 18, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 19, 0 }, { 22, 0 }, { 22, 1 }, { 25, 0 } },
	{ { 99, 99 }, { 99, 99 }, { 99, 99 }, { -1, -1 } },
	{ { 29, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 30, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 30, 0 }, { 31, 0 }, { -1, -1 }, { -1, -1 } },
	{ { 32, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 30, 2 }, { 31, 2 }, { 33, 2 }, { 33, 3 } },
	{ { 34, 0 }, { 34, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 35, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 34, 0 }, { 34, 1 }, { 36, 0 }, { -1, -1 } },
	{ { 37, 0 }, { 37, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 38, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 34, 2 }, { 34, 3 }, { 36, 2 }, { 39, 2 } },
	{ { 40, 0 }, { 40, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 41, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 42, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 40, 0 }, { 40, 1 }, { 43, 0 }, { -1, -1 } },
	{ { 44, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 45, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 40, 0 }, { 40, 1 }, { 43, 0 }, { 46, 0 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 47, 0 }, { 47, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 48, 0 }, { 51, 0 }, { 51, 1 }, { 54, 0 } },
	{ { 99, 99 }, { 99, 99 }, { 99, 99 }, { -1, -1 } },
	{ { 58, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 59, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 59, 0 }, { 60, 0 }, { -1, -1 }, { -1, -1 } },
	{ { 61, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 59, 2 }, { 60, 2 }, { 62, 2 }, { 62, 3 } },
	{ { 63, 0 }, { 63, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 64, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 63, 0 }, { 63, 1 }, { 65, 0 }, { -1, -1 } },
	{ { 66, 0 }, { 66, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 67, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 63, 2 }, { 63, 3 }, { 65, 2 }, { 68, 2 } },
	{ { 69, 0 }, { 69, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 70, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 71, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 69, 0 }, { 69, 1 }, { 72, 0 }, { -1, -1 } },
	{ { 73, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 74, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 69, 0 }, { 69, 1 }, { 72, 0 }, { 75, 0 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 76, 0 }, { 76, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 77, 0 }, { 80, 0 }, { 80, 1 }, { 83, 0 } }
};

static const unsigned int attr_wdma_en[ATTR_LOOP_NUM][OUTPUT_WDMA_WRA_NUM] = {
	{ 1, 0, 1, 0 }, { 1, 0, 1, 0 }, { 1, 0, 0, 0 }, { 1, 1, 1, 1 },
	{ 1, 1, 1, 1 }, { 1, 0, 1, 0 }, { 1, 1, 0, 0 }, { 1, 0, 1, 0 },
	{ 1, 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 1, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }
};

static const unsigned int
	attr_ker_rdma_size[ATTR_LOOP_NUM][KERNEL_RDMA_RA_NUM] = {
		{ 240, 240 },	{ 1168, 1168 }, { 272, 272 },	{ 2320, 2320 },
		{ 2080, 2080 }, { 9232, 9232 }, { 3104, 3104 }, { 9232, 9232 },
		{ 4128, 4128 }, { 1040, 1040 }, { 4624, 4624 }, { 4624, 4624 },
		{ 1552, 1552 }, { 4624, 4624 }, { 4624, 4624 }, { 4128, 4128 },
		{ 9232, 9232 }, { 272, 272 },	{ 9232, 9232 }, { 2320, 2320 },
		{ 144, 144 },	{ 9232, 9232 }, { 272, 272 },	{ 9232, 9232 },
		{ 2320, 2320 }, { 144, 144 }
	};

static const unsigned int attr_out_stride2_as_in[ATTR_LOOP_NUM] = {
	0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned int attr_fd_stride[ATTR_LOOP_NUM] = { /* H */
							    2, 1, 1, 1, 1, 1, 1,
							    1, 1, 1, 1, 1, 1, 1,
							    1, 1, 1, 1, 1, 1, 1,
							    1, 1, 1, 1, 1
};

static const unsigned int attr_fd_maxpool[ATTR_LOOP_NUM] = { /* L */
							     1, 0, 0, 0, 0, 0,
							     0, 0, 0, 0, 0, 0,
							     0, 0, 0, 0, 0, 0,
							     0, 0, 0, 0, 0, 0,
							     0, 0
};

static const unsigned int attr_out_2size[ATTR_LOOP_NUM] = { /* O */
							    1, 1, 0, 1, 1, 1, 0,
							    1, 0, 0, 0, 0, 0, 0,
							    0, 0, 0, 0, 0, 0, 0,
							    0, 0, 0, 0, 0
};

/* [loop][ch][output_index] */
static const signed int attr_rdma_en[ATTR_LOOP_NUM][INPUT_WDMA_WRA_NUM][2] = {
	{ { 99, 99 }, { 99, 99 }, { 99, 99 }, { -1, -1 } },
	{ { 0, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 0, 0 }, { 1, 0 }, { -1, -1 }, { -1, -1 } },
	{ { 2, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 0, 2 }, { 1, 2 }, { 3, 2 }, { 3, 3 } },
	{ { 4, 0 }, { 4, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 4, 0 }, { 4, 1 }, { 5, 0 }, { -1, -1 } },
	{ { 6, 0 }, { 6, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 4, 2 }, { 4, 3 }, { 5, 2 }, { 7, 2 } },
	{ { 8, 0 }, { 8, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 9, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 10, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 8, 0 }, { 8, 1 }, { 11, 0 }, { -1, -1 } },
	{ { 12, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 13, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 8, 0 }, { 8, 1 }, { 11, 0 }, { 14, 0 } },
	{ { 15, 0 }, { 15, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 16, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 15, 0 }, { 15, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 18, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 19, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 15, 0 }, { 15, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 21, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 15, 0 }, { 15, 1 }, { -1, -1 }, { -1, -1 } },
	{ { 23, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } },
	{ { 24, 0 }, { -1, -1 }, { -1, -1 }, { -1, -1 } }
};

static const unsigned int attr_wdma_size[ATTR_LOOP_NUM][OUTPUT_WDMA_WRA_NUM] = {
	{ 16384, 0, 4096, 0 },
	{ 16384, 0, 4096, 0 },
	{ 16384, 0, 0, 0 },
	{ 16384, 16384, 4096, 4096 },
	{ 8192, 8192, 2048, 2048 },
	{ 8192, 0, 2048, 0 },
	{ 8192, 8192, 0, 0 },
	{ 8192, 0, 2048, 0 },
	{ 2048, 2048, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 2048, 2048, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 1024, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 2048, 0, 0, 0 },
	{ 1024, 0, 0, 0 },
	{ 0, 0, 0, 0 }
};

static const unsigned int fld_step_align_size[FLD_STEP_NUM][FLD_MAX_FRAME] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6528 },
	{ 1536, 1280, 1280, 1280, 1280, 1280, 1280, 1280, 1280, 1280, 1280,
	  1280, 1280, 1280, 1280 },
	{ 5376, 5376, 5376, 5376, 5376, 5376, 5376, 5376, 5376, 5376, 5376,
	  5376, 5376, 5376, 5376 },
	{ 307200, 307200, 307200, 307200, 307200, 307200, 307200, 307200,
	  307200, 307200, 307200, 307200, 307200, 307200, 307200 },
	{ 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064,
	  8064, 8064, 8064, 8064 },
	{ 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064, 8064,
	  8064, 8064, 8064, 8064 }
};

static const unsigned int fld_face_info_0[FLD_MAX_FRAME] = {
	FLD_INFO_0_FACE_0,  FLD_INFO_0_FACE_1,	FLD_INFO_0_FACE_2,
	FLD_INFO_0_FACE_3,  FLD_INFO_0_FACE_4,	FLD_INFO_0_FACE_5,
	FLD_INFO_0_FACE_6,  FLD_INFO_0_FACE_7,	FLD_INFO_0_FACE_8,
	FLD_INFO_0_FACE_9,  FLD_INFO_0_FACE_10, FLD_INFO_0_FACE_11,
	FLD_INFO_0_FACE_12, FLD_INFO_0_FACE_13, FLD_INFO_0_FACE_14
};

static const unsigned int fld_face_info_1[FLD_MAX_FRAME] = {
	FLD_INFO_1_FACE_0,  FLD_INFO_1_FACE_1,	FLD_INFO_1_FACE_2,
	FLD_INFO_1_FACE_3,  FLD_INFO_1_FACE_4,	FLD_INFO_1_FACE_5,
	FLD_INFO_1_FACE_6,  FLD_INFO_1_FACE_7,	FLD_INFO_1_FACE_8,
	FLD_INFO_1_FACE_9,  FLD_INFO_1_FACE_10, FLD_INFO_1_FACE_11,
	FLD_INFO_1_FACE_12, FLD_INFO_1_FACE_13, FLD_INFO_1_FACE_14
};

static const unsigned int fld_face_info_2[FLD_MAX_FRAME] = {
	FLD_INFO_2_FACE_0,  FLD_INFO_2_FACE_1,	FLD_INFO_2_FACE_2,
	FLD_INFO_2_FACE_3,  FLD_INFO_2_FACE_4,	FLD_INFO_2_FACE_5,
	FLD_INFO_2_FACE_6,  FLD_INFO_2_FACE_7,	FLD_INFO_2_FACE_8,
	FLD_INFO_2_FACE_9,  FLD_INFO_2_FACE_10, FLD_INFO_2_FACE_11,
	FLD_INFO_2_FACE_12, FLD_INFO_2_FACE_13, FLD_INFO_2_FACE_14
};

static u32 aie_cmb_u16(u16 low, u16 high)
{
	return ((u32)high << 16) | low;
}

static u32 aie_cmb_stride(u16 low, u16 high)
{
	return ((u32)high << 16) | (low & 0x000F);
}

static inline u16 dif_x(const struct aie_enq_info *aie_cfg)
{
	return (u16)(aie_cfg->src_roi.x2 - aie_cfg->src_roi.x1);
}

static inline u16 dif_y(const struct aie_enq_info *aie_cfg)
{
	return (u16)(aie_cfg->src_roi.y2 - aie_cfg->src_roi.y1);
}

static inline void set_cmb_cfg(u32 *tbl, u16 index, u16 stride)
{
	if (!tbl)
		tbl[index] = aie_cmb_u16(tbl[index], stride);
}

static inline void set_cmbst_cfg(u32 *tbl, u16 index, u16 stride)
{
	if (!tbl)
		tbl[index] = aie_cmb_stride(tbl[index], stride);
}

static int aie_imem_alloc(struct mtk_aie_dev *fd, u32 size,
			  struct imem_buf_info *bufinfo)
{
	struct device *dev = fd->dev;
	void *va = NULL;
	dma_addr_t dma_handle = 0;

	if (size == 0) {
		dev_dbg(fd->dev, "%s: size(%d)\n", __func__, size);
		return -EINVAL;
	}

	fd->fd_mem_size += size;

	va = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
	if (!va || dma_handle == 0)
		return -ENOMEM;

	bufinfo->va = va;
	bufinfo->pa = dma_handle;
	bufinfo->size = size;

	dev_dbg(fd->dev,
		"%s: vAddr(0x%p) pAddr(0x%pad) size(%d)\n",
		__func__,
		va,
		&dma_handle,
		size
	);

	return 0;
}

static void aie_imem_free(struct mtk_aie_dev *fd, struct imem_buf_info *bufinfo)
{
	dev_dbg(fd->dev,
		"%s: vAddr(0x%p) pAddr(0x%pad) size(%d)\n",
		__func__,
		bufinfo->va,
		&bufinfo->pa,
		bufinfo->size
	);

	if (bufinfo->va)
		dma_free_coherent(fd->dev,
				  bufinfo->size,
				  bufinfo->va,
				  bufinfo->pa
		);
}

static void aie_init_table(struct mtk_aie_dev *fd, u16 pym_width,
			   u16 pym_height)
{
	int i = 0;
	struct aie_static_info *pstv = &fd->st_info;

	pstv->inf_elm[PYM2_START_LOOP].img_width = pym_width / 4;
	pstv->inf_elm[PYM2_START_LOOP].img_height = pym_height / 4;

	pstv->inf_elm[PYM1_START_LOOP].img_width = pym_width / 2;
	pstv->inf_elm[PYM1_START_LOOP].img_height = pym_height / 2;

	pstv->inf_elm[PYM0_START_LOOP].img_width = pym_width;
	pstv->inf_elm[PYM0_START_LOOP].img_height = pym_height;

	for (i = 0; i < FD_LOOP_NUM; i++) {
		if (i != PYM2_START_LOOP && i != PYM1_START_LOOP && i != PYM0_START_LOOP) {
			if (fd_out_stride2_in[i] == 1) {
				pstv->inf_elm[i].img_width =
					pstv->inf_elm[i - 1].stride2_out_width;
				pstv->inf_elm[i].img_height =
					pstv->inf_elm[i - 1].stride2_out_height;
			} else {
				pstv->inf_elm[i].img_width =
					pstv->inf_elm[i - 1].out_width;
				pstv->inf_elm[i].img_height =
					pstv->inf_elm[i - 1].out_height;
			}
		}

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1) {
			pstv->inf_elm[i].out_width =
				(pstv->inf_elm[i].img_width - 1) / (2 * fd_maxpool[i]) + 1;
			pstv->inf_elm[i].out_height =
				(pstv->inf_elm[i].img_height - 1) / (2 * fd_maxpool[i]) + 1;
		} else {
			pstv->inf_elm[i].out_width =
				(pstv->inf_elm[i].img_width - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
			pstv->inf_elm[i].out_height =
				(pstv->inf_elm[i].img_height - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) + 1;
		}

		pstv->inf_elm[i].stride2_out_width =
			((pstv->inf_elm[i].out_width - 1) / 2 + 1) * out_2size[i];
		pstv->inf_elm[i].stride2_out_height =
			((pstv->inf_elm[i].out_height - 1) / 2 + 1) * out_2size[i];

		if (outlayer[i] == 1) {
			pstv->inf_elm[i].out_xsize_plus_1 =
				pstv->inf_elm[i].out_width * out_ch_pack[i] * 2;
			pstv->inf_elm[i].out_stride =
				round_up(pstv->inf_elm[i].out_xsize_plus_1 * anchor_en_num[i], 16);
			pstv->inf_elm[i].out_xsize_plus_1_stride2 =
				((pstv->inf_elm[i].out_width - 1) / 2 + 1) *
				out_ch_pack[i] * 2 * out_2size[i];
		} else {
			pstv->inf_elm[i].out_xsize_plus_1 =
				pstv->inf_elm[i].out_width * out_ch_pack[i];
			pstv->inf_elm[i].out_stride =
				round_up(pstv->inf_elm[i].out_xsize_plus_1, 16);
			pstv->inf_elm[i].out_xsize_plus_1_stride2 =
				((pstv->inf_elm[i].out_width - 1) / 2 + 1) *
				out_ch_pack[i] * out_2size[i];
		}

		pstv->inf_elm[i].out_stride_stride2 =
				round_up(pstv->inf_elm[i].out_xsize_plus_1_stride2, 16);

		if (out_2size[i] == 1)
			pstv->inf_elm[i].out_ysize_plus_1_stride2 =
				(pstv->inf_elm[i].out_height - 1) / 2 + 1;
		else
			pstv->inf_elm[i].out_ysize_plus_1_stride2 =
				pstv->inf_elm[i].out_height;

		if (fd_wdma_en[i][0]) {
			if (i == RPN2_LOOP_NUM || i == RPN1_LOOP_NUM || i == RPN0_LOOP_NUM)
				pstv->inf_elm[i].fd_wdma_size[0] =
					RESULT_SIZE;
			else
				pstv->inf_elm[i].fd_wdma_size[0] =
					pstv->inf_elm[i].out_height *
					pstv->inf_elm[i].out_stride;
		}

		if (outlayer[i] == 1) {
			if (fd_wdma_en[i][1])
				pstv->inf_elm[i].fd_wdma_size[1] =
					pstv->inf_elm[i].fd_wdma_size[0];
			if (fd_wdma_en[i][2])
				pstv->inf_elm[i].fd_wdma_size[2] =
					pstv->inf_elm[i].fd_wdma_size[0];
			if (fd_wdma_en[i][3])
				pstv->inf_elm[i].fd_wdma_size[3] =
					pstv->inf_elm[i].fd_wdma_size[0];
		} else if (i == RPN2_LOOP_NUM || i == RPN1_LOOP_NUM || i == RPN0_LOOP_NUM) {
			pstv->inf_elm[i].fd_wdma_size[0] = RESULT_SIZE;
		} else {
			if (fd_wdma_en[i][1])
				pstv->inf_elm[i].fd_wdma_size[1] =
					pstv->inf_elm[i].out_height *
					pstv->inf_elm[i].out_stride;
			if (fd_wdma_en[i][2])
				pstv->inf_elm[i].fd_wdma_size[2] =
					pstv->inf_elm[i].out_ysize_plus_1_stride2 *
					pstv->inf_elm[i].out_stride_stride2;
			if (fd_wdma_en[i][3])
				pstv->inf_elm[i].fd_wdma_size[3] =
					pstv->inf_elm[i].out_ysize_plus_1_stride2 *
					pstv->inf_elm[i].out_stride_stride2;
		}

		if (in_ch_pack[i] == 1)
			pstv->inf_elm[i].input_xsize_plus_1 =
				round_up(pstv->inf_elm[i].img_width, 8);
		else
			pstv->inf_elm[i].input_xsize_plus_1 =
				pstv->inf_elm[i].img_width * in_ch_pack[i];
	}
}

static void aie_update_table(struct mtk_aie_dev *fd, u16 pym_width,
			     u16 pym_height)
{
	int i = 0;
	struct aie_static_info *pstv = &fd->st_info;

	pstv->inf_elm[PYM2_START_LOOP].img_width = pym_width / 4;
	pstv->inf_elm[PYM2_START_LOOP].img_height = pym_height / 4;

	pstv->inf_elm[PYM1_START_LOOP].img_width = pym_width / 2;
	pstv->inf_elm[PYM1_START_LOOP].img_height = pym_height / 2;

	pstv->inf_elm[PYM0_START_LOOP].img_width = pym_width;
	pstv->inf_elm[PYM0_START_LOOP].img_height = pym_height;

	for (i = 0; i < FD_LOOP_NUM; i++) {
		if (i != PYM2_START_LOOP && i != PYM1_START_LOOP &&
		    i != PYM0_START_LOOP) {
			if (fd_out_stride2_in[i] == 1) {
				pstv->inf_elm[i].img_width =
					pstv->inf_elm[i - 1].stride2_out_width;
				pstv->inf_elm[i].img_height =
					pstv->inf_elm[i - 1].stride2_out_height;
			} else {
				pstv->inf_elm[i].img_width =
					pstv->inf_elm[i - 1].out_width;
				pstv->inf_elm[i].img_height =
					pstv->inf_elm[i - 1].out_height;
			}
		}

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1) {
			pstv->inf_elm[i].out_width =
				(pstv->inf_elm[i].img_width - 1) /
					(2 * fd_maxpool[i]) +
				1;
			pstv->inf_elm[i].out_height =
				(pstv->inf_elm[i].img_height - 1) /
					(2 * fd_maxpool[i]) +
				1;
		} else {
			pstv->inf_elm[i].out_width =
				(pstv->inf_elm[i].img_width - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) +
				1;
			pstv->inf_elm[i].out_height =
				(pstv->inf_elm[i].img_height - 1) /
					(fd_stride[i] + 2 * fd_maxpool[i]) +
				1;
		}

		pstv->inf_elm[i].stride2_out_width =
			((pstv->inf_elm[i].out_width - 1) / 2 + 1) *
			out_2size[i];
		pstv->inf_elm[i].stride2_out_height =
			((pstv->inf_elm[i].out_height - 1) / 2 + 1) *
			out_2size[i];

		if (outlayer[i] == 1) {
			pstv->inf_elm[i].out_xsize_plus_1 =
				pstv->inf_elm[i].out_width *
				out_ch_pack[i] * 2;
			pstv->inf_elm[i].out_stride =
				round_up(pstv->inf_elm[i].out_xsize_plus_1 * anchor_en_num[i], 16);
			pstv->inf_elm[i].out_xsize_plus_1_stride2 =
				((pstv->inf_elm[i].out_width - 1) / 2 +
				 1) *
				out_ch_pack[i] * 2 * out_2size[i];
		} else {
			pstv->inf_elm[i].out_xsize_plus_1 =
				pstv->inf_elm[i].out_width *
				out_ch_pack[i];
			pstv->inf_elm[i].out_stride =
				round_up(pstv->inf_elm[i].out_xsize_plus_1, 16);
			pstv->inf_elm[i].out_xsize_plus_1_stride2 =
				((pstv->inf_elm[i].out_width - 1) / 2 +
				 1) *
				out_ch_pack[i] * out_2size[i];
		}

		pstv->inf_elm[i].out_stride_stride2 =
			round_up(pstv->inf_elm[i].out_xsize_plus_1_stride2, 16);

		if (out_2size[i] == 1)
			pstv->inf_elm[i].out_ysize_plus_1_stride2 =
				(pstv->inf_elm[i].out_height - 1) / 2 + 1;
		else
			pstv->inf_elm[i].out_ysize_plus_1_stride2 =
				pstv->inf_elm[i].out_height;

		if (in_ch_pack[i] == 1)
			pstv->inf_elm[i].input_xsize_plus_1 =
				round_up(pstv->inf_elm[i].img_width, 8);
		else
			pstv->inf_elm[i].input_xsize_plus_1 =
				pstv->inf_elm[i].img_width * in_ch_pack[i];
	}
}

static void aie_update_buf_params(struct mtk_aie_dev *fd, u16 max_img_width,
			      u16 max_img_height)
{
	u8 i = 0, j = 0;
	struct aie_static_info *pstv = &fd->st_info;

	fd->base_para->max_img_width = max_img_width;
	fd->base_para->max_img_height = max_img_height;
	fd->fd_dma_max_size = 0;
	fd->fd_dma_rst_max_size = 0;
	fd->fd_fd_kernel_size = 0;
	fd->fd_attr_kernel_size = 0;
	fd->fd_attr_dma_max_size = 0;
	fd->fd_attr_dma_rst_max_size = 0;

	/* FDMODE Dram Buffer Size */
	fd->fd_rs_cfg_size = 4 * fd->variant->rs_cfg_size * 2;
	fd->fd_fd_cfg_size = 4 * fd->variant->fd_cfg_size * FD_LOOP_NUM;
	fd->fd_yuv2rgb_cfg_size = 4 * fd->variant->y2r_cfg_size;

	/* ATTRMODE Dram Buffer Size */
	fd->attr_fd_cfg_size = 4 * fd->variant->fd_cfg_size * ATTR_LOOP_NUM;
	fd->attr_yuv2rgb_cfg_size = 4 * fd->variant->y2r_cfg_size;

	/* HW Output Buffer Size */
	fd->rs_pym_out_size[0] = fd->base_para->max_pyramid_width *
				 fd->base_para->max_pyramid_height;
	fd->rs_pym_out_size[1] = fd->rs_pym_out_size[0] / 2;
	fd->rs_pym_out_size[2] = fd->rs_pym_out_size[0] / 4;

	/* FDMODE Dram Buffer Size */
	for (i = 0; i < FD_LOOP_NUM; i++) {
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (fd_wdma_en[i][j]) {
				if ((i == RPN2_LOOP_NUM || i == RPN1_LOOP_NUM ||
				     i == RPN0_LOOP_NUM) && j == 0) {
					fd->fd_dma_rst_max_size +=
						pstv->inf_elm[i]
							.fd_wdma_size[j];
				} else {
					fd->fd_dma_max_size +=
						pstv->inf_elm[i]
							.fd_wdma_size[j];
				}
			}
		}
	}

	for (i = 0; i < FD_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			if (fd_ker_rdma_size[i][j])
				fd->fd_fd_kernel_size += fd_ker_rdma_size[i][j];
		}
	}

	/* ATTRMODE Dram Buffer Size */
	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (attr_wdma_en[i][j]) {
				if ((i == AGE_OUT_RGS || i == GENDER_OUT_RGS ||
				     i == INDIAN_OUT_RGS || i == RACE_OUT_RGS) && j == 0) {
					fd->fd_attr_dma_rst_max_size +=
						ATTR_OUT_SIZE *
						MAX_ENQUE_FRAME_NUM;
				} else {
					fd->fd_attr_dma_max_size +=
						attr_wdma_size[i][j];
				}
			}
		}
	}

	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++)
			fd->fd_attr_kernel_size += attr_ker_rdma_size[i][j];
	}

	/* FD Pose secure result output buffer: result size * 3 loops */
	fd->fd_dma_rst_max_size += RESULT_SIZE * 3;

	if (fd->variant->fld_enable) {
		/* fld size */
		fd->fld_step_size = 0;
		for (i = 0; i < FLD_STEP_NUM; i++)
			for (j = 0; j < FLD_MAX_FRAME; j++)
				fd->fld_step_size += fld_step_align_size[i][j];

		fd->fld_out_size = FLD_OUTPUT_SIZE * FLD_MAX_FRAME;
	}
}

static int aie_alloc_dram_buf(struct mtk_aie_dev *fd)
{
	int ret = -EINVAL;
	u8 i = 0;
	u32 alloc_size = 0;

	/* RS DRAM */
	alloc_size = fd->fd_rs_cfg_size;
	dev_dbg(fd->dev, "RS CFG:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->rs_cfg_data);
	if (ret)
		goto dma_alloc_fail;
	/* FD MODE */
	fd->base_para->fd_rs_cfg_pa = fd->rs_cfg_data.pa;
	fd->base_para->fd_rs_cfg_va = fd->rs_cfg_data.va;

	/* FD DRAM */
	alloc_size =
		fd->fd_fd_cfg_size + fd->attr_fd_cfg_size * MAX_ENQUE_FRAME_NUM;
	dev_dbg(fd->dev, "FD CFG:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_cfg_data);
	if (ret)
		goto dma_alloc_fail;
	/* FD MODE */
	fd->base_para->fd_fd_cfg_pa = fd->fd_cfg_data.pa;
	fd->base_para->fd_fd_cfg_va = fd->fd_cfg_data.va;
	/* ATTR MODE */
	fd->base_para->attr_fd_cfg_pa[0] =
		fd->base_para->fd_fd_cfg_pa + fd->fd_fd_cfg_size;
	fd->base_para->attr_fd_cfg_va[0] =
		fd->base_para->fd_fd_cfg_va + fd->fd_fd_cfg_size;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->base_para->attr_fd_cfg_pa[i] =
			fd->base_para->attr_fd_cfg_pa[i - 1] +
			fd->attr_fd_cfg_size;
		fd->base_para->attr_fd_cfg_va[i] =
			fd->base_para->attr_fd_cfg_va[i - 1] +
			fd->attr_fd_cfg_size;
	}

	/* YUV2RGB DRAM */
	alloc_size = fd->fd_yuv2rgb_cfg_size +
		     fd->attr_yuv2rgb_cfg_size * MAX_ENQUE_FRAME_NUM;
	dev_dbg(fd->dev, "YUV2RGB CFG:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->yuv2rgb_cfg_data);
	if (ret)
		goto dma_alloc_fail;
	/* FD MODE */
	fd->base_para->fd_yuv2rgb_cfg_pa = fd->yuv2rgb_cfg_data.pa;
	fd->base_para->fd_yuv2rgb_cfg_va = fd->yuv2rgb_cfg_data.va;

	/* ATTR MODE */
	fd->base_para->attr_yuv2rgb_cfg_pa[0] =
		fd->base_para->fd_yuv2rgb_cfg_pa + fd->fd_yuv2rgb_cfg_size;
	fd->base_para->attr_yuv2rgb_cfg_va[0] =
		fd->base_para->fd_yuv2rgb_cfg_va + fd->fd_yuv2rgb_cfg_size;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->base_para->attr_yuv2rgb_cfg_pa[i] =
			fd->base_para->attr_yuv2rgb_cfg_pa[i - 1] +
			fd->attr_yuv2rgb_cfg_size;
		fd->base_para->attr_yuv2rgb_cfg_va[i] =
			fd->base_para->attr_yuv2rgb_cfg_va[i - 1] +
			fd->attr_yuv2rgb_cfg_size;
	}

	return ret;
dma_alloc_fail:
	aie_imem_free(fd, &fd->fd_cfg_data);
	aie_imem_free(fd, &fd->rs_cfg_data);

	return ret;
}

static int aie_alloc_output_buf(struct mtk_aie_dev *fd)
{
	int ret = -EINVAL;
	u32 alloc_size = 0;
	int i, j, pa_off = 0, va_off = 0;

	for (i = 0; i < PYM_NUM; i++)
		alloc_size += fd->rs_pym_out_size[i] * 3;
	dev_dbg(fd->dev, "RS OUT:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->rs_output_hw);
	if (ret)
		return ret;

	for (i = 0; i < PYM_NUM; i++) {
		for (j = 0; j < COLOR_NUM; j++) {
			fd->base_para->rs_pym_rst_pa[i][j] =
				fd->rs_output_hw.pa + pa_off;
			pa_off += fd->rs_pym_out_size[i];

			fd->base_para->rs_pym_rst_va[i][j] =
				fd->rs_output_hw.va + va_off;
			va_off += fd->rs_pym_out_size[i];
		}
	}

	return ret;
}

static void aie_alloc_normal(struct mtk_aie_dev *fd, int start, int end)
{
	int i = 0, j = 0;
	int pi = 0, pj = 0;
	struct aie_static_info *pstv = &fd->st_info;

	if (start <= 0 || end <= start) {
		dev_err(fd->dev, "%s: start = %d, end = %d\n", __func__, start, end);
		return;
	}

	pi = start - 1;
	pj = 0;
	for (i = start; i < end + 1; i++) {
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_pa[i][j] =
					fd->dma_para->fd_out_hw_pa[pi][pj] +
					pstv->inf_elm[pi].fd_wdma_size[pj];
				pi = i;
				pj = j;
			}
		}
	}
}

static int aie_alloc_fddma_buf(struct mtk_aie_dev *fd)
{
	int ret = -EINVAL;
	u32 alloc_size = 0;

	alloc_size = fd->fd_dma_max_size;
	dev_dbg(fd->dev, "FD DMA:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_dma_hw);
	if (ret)
		goto dma_alloc_fail;
	alloc_size = fd->fd_fd_kernel_size + fd->fd_attr_kernel_size;
	dev_dbg(fd->dev, "FD KERNEL:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_kernel_hw);
	if (ret)
		goto dma_alloc_fail;

	alloc_size = fd->fd_attr_dma_max_size;
	dev_dbg(fd->dev, "ATTR DMA:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_attr_dma_hw);
	if (ret)
		goto dma_alloc_fail;

	alloc_size = fd->fd_dma_rst_max_size + fd->fd_attr_dma_rst_max_size;
	dev_dbg(fd->dev, "RESULT DMA:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_dma_result_hw);
	if (ret)
		goto dma_alloc_fail;

	return 0;

dma_alloc_fail:
	aie_imem_free(fd, &fd->fd_attr_dma_hw);
	aie_imem_free(fd, &fd->fd_kernel_hw);
	aie_imem_free(fd, &fd->fd_dma_hw);

	return ret;
}

static int aie_alloc_fld_buf(struct mtk_aie_dev *fd)
{
	int ret = -EINVAL;
	u32 alloc_size = 0;

	alloc_size = fd->fld_step_size;
	dev_dbg(fd->dev, "FLD STEP:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_fld_step_data);
	if (ret)
		return ret;

	alloc_size = fd->fld_out_size;
	dev_dbg(fd->dev, "FLD OUT:");
	ret = aie_imem_alloc(fd, alloc_size, &fd->fd_fld_out_hw);
	if (ret)
		goto fld_step;

	return 0;
fld_step:
	aie_imem_free(fd, &fd->fd_fld_step_data);

	return ret;
}

static void aie_arrange_fddma_buf(struct mtk_aie_dev *fd)
{
	void *current_va = NULL;
	dma_addr_t current_pa = 0;
	struct aie_static_info *pstv = &fd->st_info;
	u8 i = 0, j = 0;

	/* 0~18 */
	fd->dma_para->fd_out_hw_pa[0][0] = fd->fd_dma_hw.pa;
	aie_alloc_normal(fd, 1, 18);

	/* 19~27 */
	fd->dma_para->fd_out_hw_pa[19][0] =
		fd->dma_para->fd_out_hw_pa[18][1] +
		pstv->inf_elm[18].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[19][1] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		pstv->inf_elm[19].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[20][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		2 * pstv->inf_elm[20].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[20][1] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		3 * pstv->inf_elm[20].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[21][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		4 * pstv->inf_elm[21].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[22][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		pstv->inf_elm[19].fd_wdma_size[0] +
		pstv->inf_elm[19].fd_wdma_size[1] +
		pstv->inf_elm[20].fd_wdma_size[0] +
		pstv->inf_elm[20].fd_wdma_size[1] +
		pstv->inf_elm[21].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[22][1] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		pstv->inf_elm[22].fd_wdma_size[0] +
		pstv->inf_elm[22].fd_wdma_size[2] +
		pstv->inf_elm[23].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[22][2] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		pstv->inf_elm[22].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[22][3] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		pstv->inf_elm[22].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][0] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		2 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][1] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		2 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][2] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		3 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][3] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		3 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[24][0] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		4 * pstv->inf_elm[24].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[24][1] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		4 * pstv->inf_elm[24].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[25][0] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		pstv->inf_elm[22].fd_wdma_size[1] +
		pstv->inf_elm[22].fd_wdma_size[3] +
		pstv->inf_elm[23].fd_wdma_size[1] +
		pstv->inf_elm[23].fd_wdma_size[3] +
		pstv->inf_elm[24].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[25][1] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		pstv->inf_elm[25].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[26][0] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		2 * pstv->inf_elm[26].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[26][1] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		3 * pstv->inf_elm[26].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[27][0] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		4 * pstv->inf_elm[27].out_xsize_plus_1;

	/* 29~47 */
	fd->dma_para->fd_out_hw_pa[29][0] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		pstv->inf_elm[25].fd_wdma_size[0] +
		pstv->inf_elm[25].fd_wdma_size[1] +
		pstv->inf_elm[26].fd_wdma_size[0] +
		pstv->inf_elm[26].fd_wdma_size[1] +
		pstv->inf_elm[27].fd_wdma_size[0];
	aie_alloc_normal(fd, 30, 47);

	/* 48~56 */
	fd->dma_para->fd_out_hw_pa[48][0] =
		fd->dma_para->fd_out_hw_pa[47][1] +
		pstv->inf_elm[47].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[48][1] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		pstv->inf_elm[48].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[49][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		2 * pstv->inf_elm[49].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[49][1] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		3 * pstv->inf_elm[49].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[50][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		4 * pstv->inf_elm[50].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[51][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		pstv->inf_elm[48].fd_wdma_size[0] +
		pstv->inf_elm[48].fd_wdma_size[1] +
		pstv->inf_elm[49].fd_wdma_size[0] +
		pstv->inf_elm[49].fd_wdma_size[1] +
		pstv->inf_elm[50].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[51][1] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		pstv->inf_elm[51].fd_wdma_size[0] +
		pstv->inf_elm[51].fd_wdma_size[2] +
		pstv->inf_elm[52].fd_wdma_size[0] +
		pstv->inf_elm[52].fd_wdma_size[2] +
		pstv->inf_elm[53].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[51][2] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		pstv->inf_elm[51].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[51][3] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		pstv->inf_elm[51].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][0] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		2 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][1] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		2 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][2] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		3 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][3] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		3 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[53][0] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		4 * pstv->inf_elm[53].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[53][1] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		4 * pstv->inf_elm[53].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[54][0] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		pstv->inf_elm[51].fd_wdma_size[1] +
		pstv->inf_elm[51].fd_wdma_size[3] +
		pstv->inf_elm[52].fd_wdma_size[1] +
		pstv->inf_elm[52].fd_wdma_size[3] +
		pstv->inf_elm[53].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[54][1] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		pstv->inf_elm[54].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[55][0] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		2 * pstv->inf_elm[55].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[55][1] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		3 * pstv->inf_elm[55].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[56][0] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		4 * pstv->inf_elm[56].out_xsize_plus_1;

	/* 58~76 */
	fd->dma_para->fd_out_hw_pa[58][0] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		pstv->inf_elm[54].fd_wdma_size[0] +
		pstv->inf_elm[54].fd_wdma_size[1] +
		pstv->inf_elm[55].fd_wdma_size[0] +
		pstv->inf_elm[55].fd_wdma_size[1] +
		pstv->inf_elm[56].fd_wdma_size[0];
	aie_alloc_normal(fd, 59, 76);

	/* 77~85 */
	fd->dma_para->fd_out_hw_pa[77][0] =
		fd->dma_para->fd_out_hw_pa[76][1] +
		pstv->inf_elm[76].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[77][1] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		pstv->inf_elm[77].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[78][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		2 * pstv->inf_elm[78].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[78][1] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		3 * pstv->inf_elm[78].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[79][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		4 * pstv->inf_elm[79].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[80][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		pstv->inf_elm[77].fd_wdma_size[0] +
		pstv->inf_elm[77].fd_wdma_size[1] +
		pstv->inf_elm[78].fd_wdma_size[0] +
		pstv->inf_elm[78].fd_wdma_size[1] +
		pstv->inf_elm[79].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[80][1] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		pstv->inf_elm[80].fd_wdma_size[0] +
		pstv->inf_elm[80].fd_wdma_size[2] +
		pstv->inf_elm[81].fd_wdma_size[0] +
		pstv->inf_elm[81].fd_wdma_size[2] +
		pstv->inf_elm[82].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[80][2] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		pstv->inf_elm[80].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[80][3] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		pstv->inf_elm[80].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][0] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		2 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][1] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		2 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][2] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		3 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][3] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		3 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[82][0] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		4 * pstv->inf_elm[82].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[82][1] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		4 * pstv->inf_elm[82].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[83][0] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		pstv->inf_elm[80].fd_wdma_size[1] +
		pstv->inf_elm[80].fd_wdma_size[3] +
		pstv->inf_elm[81].fd_wdma_size[1] +
		pstv->inf_elm[81].fd_wdma_size[3] +
		pstv->inf_elm[82].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[83][1] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		pstv->inf_elm[83].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[84][0] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		2 * pstv->inf_elm[84].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[84][1] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		3 * pstv->inf_elm[84].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[85][0] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		4 * pstv->inf_elm[85].out_xsize_plus_1;

	/* VA : except 28, 57, 86 */
	/* 0~86 */
	fd->dma_para->fd_out_hw_va[0][0] = fd->fd_dma_hw.va;
	for (i = 1; i < FD_LOOP_NUM; i++) {
		if (i == RPN2_LOOP_NUM || i == RPN1_LOOP_NUM ||
		    i == RPN0_LOOP_NUM)
			continue;
		for (j = 0; j < 4; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_va[i][j] =
					fd->fd_dma_hw.va +
					fd->dma_para->fd_out_hw_pa[i][j] -
					fd->fd_dma_hw.pa;
			}
		}
	}

	current_pa = fd->dma_para->fd_out_hw_pa[83][0] +
		    pstv->inf_elm[83].fd_wdma_size[0] +
		    pstv->inf_elm[83].fd_wdma_size[1] +
		    pstv->inf_elm[84].fd_wdma_size[0] +
		    pstv->inf_elm[84].fd_wdma_size[1] +
		    pstv->inf_elm[85].fd_wdma_size[0];
	current_va = fd->dma_para->fd_out_hw_va[83][0] +
		    pstv->inf_elm[83].fd_wdma_size[0] +
		    pstv->inf_elm[83].fd_wdma_size[1] +
		    pstv->inf_elm[84].fd_wdma_size[0] +
		    pstv->inf_elm[84].fd_wdma_size[1] +
		    pstv->inf_elm[85].fd_wdma_size[0];

	dev_dbg(fd->dev,
		"%s: current VA = %p PA = 0x%pad\n",
		__func__,
		current_va,
		&current_pa
	);
}

static void aie_arrange_kernel_buf(struct mtk_aie_dev *fd)
{
	void *current_va = NULL;
	dma_addr_t current_pa = 0;
	u8 i = 0, j = 0;

	current_pa = fd->fd_kernel_hw.pa;
	current_va = fd->fd_kernel_hw.va;

	for (i = 0; i < FD_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			if (fd_ker_rdma_size[i][j]) {
				fd->dma_para->fd_kernel_pa[i][j] = current_pa;
				fd->dma_para->fd_kernel_va[i][j] = current_va;
				current_pa += fd_ker_rdma_size[i][j];
				current_va += fd_ker_rdma_size[i][j];
			}
		}
	}

	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			fd->dma_para->attr_kernel_pa[i][j] = current_pa;
			fd->dma_para->attr_kernel_va[i][j] = current_va;
			current_pa += attr_ker_rdma_size[i][j];
			current_va += attr_ker_rdma_size[i][j];
		}
	}

	dev_dbg(fd->dev,
		"%s: current VA = %p PA = 0x%pad\n",
		__func__,
		current_va,
		&current_pa
	);
}

static void aie_arrange_attrdma_buf(struct mtk_aie_dev *fd)
{
	void *current_va = NULL;
	dma_addr_t current_pa = 0;
	u8 i = 0, j = 0;

	current_pa = fd->fd_attr_dma_hw.pa;
	current_va = fd->fd_attr_dma_hw.va;

	/* attribute mode */
	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (attr_wdma_en[i][j]) {
				fd->dma_para->attr_out_hw_pa[i][j] = current_pa;
				fd->dma_para->attr_out_hw_va[i][j] = current_va;
				current_pa += attr_wdma_size[i][j];
				current_va += attr_wdma_size[i][j];
			}
		}
	}

	dev_dbg(fd->dev,
		"%s: current VA = %p PA = 0x%pad\n",
		__func__,
		current_va,
		&current_pa
	);
}

static void aie_arrange_result_dma_buf(struct mtk_aie_dev *fd)
{
	void *currentresult_va = NULL;
	dma_addr_t currentresult_pa = 0;
	u8 i = 0;
	struct aie_static_info *pstv = &fd->st_info;

	currentresult_pa = fd->fd_dma_result_hw.pa;
	currentresult_va = fd->fd_dma_result_hw.va;

	fd->dma_para->fd_out_hw_pa[RPN2_LOOP_NUM][0] = currentresult_pa;
	fd->dma_para->fd_out_hw_va[RPN2_LOOP_NUM][0] = currentresult_va;
	currentresult_pa += pstv->inf_elm[RPN2_LOOP_NUM].fd_wdma_size[0];
	currentresult_va += pstv->inf_elm[RPN2_LOOP_NUM].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[RPN1_LOOP_NUM][0] = currentresult_pa;
	fd->dma_para->fd_out_hw_va[RPN1_LOOP_NUM][0] = currentresult_va;
	currentresult_pa += pstv->inf_elm[RPN1_LOOP_NUM].fd_wdma_size[0];
	currentresult_va += pstv->inf_elm[RPN1_LOOP_NUM].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[RPN0_LOOP_NUM][0] = currentresult_pa;
	fd->dma_para->fd_out_hw_va[RPN0_LOOP_NUM][0] = currentresult_va;
	currentresult_pa += pstv->inf_elm[RPN0_LOOP_NUM].fd_wdma_size[0];
	currentresult_va += pstv->inf_elm[RPN0_LOOP_NUM].fd_wdma_size[0];

	fd->dma_para->attr_out_hw_pa[AGE_OUT_RGS][0] = currentresult_pa;
	fd->dma_para->attr_out_hw_va[AGE_OUT_RGS][0] = currentresult_va;
	currentresult_pa += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentresult_va += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[GENDER_OUT_RGS][0] = currentresult_pa;
	fd->dma_para->attr_out_hw_va[GENDER_OUT_RGS][0] = currentresult_va;
	currentresult_pa += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentresult_va += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[INDIAN_OUT_RGS][0] = currentresult_pa;
	fd->dma_para->attr_out_hw_va[INDIAN_OUT_RGS][0] = currentresult_va;
	currentresult_pa += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentresult_va += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	fd->dma_para->attr_out_hw_pa[RACE_OUT_RGS][0] = currentresult_pa;
	fd->dma_para->attr_out_hw_va[RACE_OUT_RGS][0] = currentresult_va;
	currentresult_pa += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;
	currentresult_va += ATTR_OUT_SIZE * MAX_ENQUE_FRAME_NUM;

	/* need to prepare 10 buffers to store 10 times result */
	fd->dma_para->age_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[AGE_OUT_RGS][0];
	fd->dma_para->age_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[AGE_OUT_RGS][0];
	fd->dma_para->gender_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[GENDER_OUT_RGS][0];
	fd->dma_para->gender_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[GENDER_OUT_RGS][0];
	fd->dma_para->is_indian_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[INDIAN_OUT_RGS][0];
	fd->dma_para->is_indian_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[INDIAN_OUT_RGS][0];
	fd->dma_para->race_out_hw_pa[0] =
		fd->dma_para->attr_out_hw_pa[RACE_OUT_RGS][0];
	fd->dma_para->race_out_hw_va[0] =
		fd->dma_para->attr_out_hw_va[RACE_OUT_RGS][0];

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		fd->dma_para->age_out_hw_pa[i] =
			fd->dma_para->age_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->age_out_hw_va[i] =
			fd->dma_para->age_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->gender_out_hw_pa[i] =
			fd->dma_para->gender_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->gender_out_hw_va[i] =
			fd->dma_para->gender_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->is_indian_out_hw_pa[i] =
			fd->dma_para->is_indian_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->is_indian_out_hw_va[i] =
			fd->dma_para->is_indian_out_hw_va[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->race_out_hw_pa[i] =
			fd->dma_para->race_out_hw_pa[i - 1] + ATTR_OUT_SIZE;
		fd->dma_para->race_out_hw_va[i] =
			fd->dma_para->race_out_hw_va[i - 1] + ATTR_OUT_SIZE;
	}

	memset(fd->fd_dma_result_hw.va, 0, fd->fd_dma_result_hw.size);

	dev_dbg(fd->dev,
		"%s: current VA = %p PA = 0x%pad\n",
		__func__,
		currentresult_va,
		&currentresult_pa
	);
}

static void aie_arrange_fld_buf(struct mtk_aie_dev *fd)
{
	u8 i = 0, j = 0;
	unsigned int offset = 0;

	for (i = 0; i < FLD_STEP_NUM; i++) {
		for (j = 0; j < FLD_MAX_FRAME; j++) {
			fd->fld_para->fld_step_va[i][j] =
				fd->fd_fld_step_data.va + offset;
			fd->fld_para->fld_step_pa[i][j] =
				fd->fd_fld_step_data.pa + offset;
			offset += fld_step_align_size[i][j];
		}
	}

	for (i = 0, offset = 0; i < FLD_MAX_FRAME; i++) {
		fd->fld_para->fld_output_va[i] = fd->fd_fld_out_hw.va + offset;
		fd->fld_para->fld_output_pa[i] = fd->fd_fld_out_hw.pa + offset;
		offset += FLD_OUTPUT_SIZE;
	}
}

static void aie_update_fddma_buf(struct mtk_aie_dev *fd)
{
	struct aie_static_info *pstv = &fd->st_info;
	u8 i = 0, j = 0;

	/* 19~27 */
	fd->dma_para->fd_out_hw_pa[19][0] =
		fd->dma_para->fd_out_hw_pa[18][1] +
		pstv->inf_elm[18].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[19][1] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		pstv->inf_elm[19].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[20][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		2 * pstv->inf_elm[20].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[20][1] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		3 * pstv->inf_elm[20].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[21][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		4 * pstv->inf_elm[21].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[22][0] =
		fd->dma_para->fd_out_hw_pa[19][0] +
		pstv->inf_elm[19].fd_wdma_size[0] +
		pstv->inf_elm[19].fd_wdma_size[1] +
		pstv->inf_elm[20].fd_wdma_size[0] +
		pstv->inf_elm[20].fd_wdma_size[1] +
		pstv->inf_elm[21].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[22][1] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		pstv->inf_elm[22].fd_wdma_size[0] +
		pstv->inf_elm[22].fd_wdma_size[2] +
		pstv->inf_elm[23].fd_wdma_size[0] +
		pstv->inf_elm[23].fd_wdma_size[2] +
		pstv->inf_elm[24].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[22][2] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		pstv->inf_elm[22].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[22][3] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		pstv->inf_elm[22].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][0] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		2 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][1] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		2 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][2] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		3 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[23][3] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		3 * pstv->inf_elm[23].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[24][0] =
		fd->dma_para->fd_out_hw_pa[22][0] +
		4 * pstv->inf_elm[24].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[24][1] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		4 * pstv->inf_elm[24].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[25][0] =
		fd->dma_para->fd_out_hw_pa[22][1] +
		pstv->inf_elm[22].fd_wdma_size[1] +
		pstv->inf_elm[22].fd_wdma_size[3] +
		pstv->inf_elm[23].fd_wdma_size[1] +
		pstv->inf_elm[23].fd_wdma_size[3] +
		pstv->inf_elm[24].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[25][1] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		pstv->inf_elm[25].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[26][0] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		2 * pstv->inf_elm[26].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[26][1] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		3 * pstv->inf_elm[26].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[27][0] =
		fd->dma_para->fd_out_hw_pa[25][0] +
		4 * pstv->inf_elm[27].out_xsize_plus_1;

	/* 48~56 */
	fd->dma_para->fd_out_hw_pa[48][0] =
		fd->dma_para->fd_out_hw_pa[47][1] +
		pstv->inf_elm[47].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[48][1] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		pstv->inf_elm[48].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[49][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		2 * pstv->inf_elm[49].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[49][1] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		3 * pstv->inf_elm[49].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[50][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		4 * pstv->inf_elm[50].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[51][0] =
		fd->dma_para->fd_out_hw_pa[48][0] +
		pstv->inf_elm[48].fd_wdma_size[0] +
		pstv->inf_elm[48].fd_wdma_size[1] +
		pstv->inf_elm[49].fd_wdma_size[0] +
		pstv->inf_elm[49].fd_wdma_size[1] +
		pstv->inf_elm[50].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[51][1] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		pstv->inf_elm[51].fd_wdma_size[0] +
		pstv->inf_elm[51].fd_wdma_size[2] +
		pstv->inf_elm[52].fd_wdma_size[0] +
		pstv->inf_elm[52].fd_wdma_size[2] +
		pstv->inf_elm[53].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[51][2] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		pstv->inf_elm[51].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[51][3] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		pstv->inf_elm[51].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][0] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		2 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][1] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		2 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][2] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		3 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[52][3] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		3 * pstv->inf_elm[52].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[53][0] =
		fd->dma_para->fd_out_hw_pa[51][0] +
		4 * pstv->inf_elm[53].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[53][1] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		4 * pstv->inf_elm[53].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[54][0] =
		fd->dma_para->fd_out_hw_pa[51][1] +
		pstv->inf_elm[51].fd_wdma_size[1] +
		pstv->inf_elm[51].fd_wdma_size[3] +
		pstv->inf_elm[52].fd_wdma_size[1] +
		pstv->inf_elm[52].fd_wdma_size[3] +
		pstv->inf_elm[53].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[54][1] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		pstv->inf_elm[54].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[55][0] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		2 * pstv->inf_elm[55].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[55][1] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		3 * pstv->inf_elm[55].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[56][0] =
		fd->dma_para->fd_out_hw_pa[54][0] +
		4 * pstv->inf_elm[56].out_xsize_plus_1;
	/* 77~85 */
	fd->dma_para->fd_out_hw_pa[77][0] =
		fd->dma_para->fd_out_hw_pa[76][1] +
		pstv->inf_elm[76].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[77][1] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		pstv->inf_elm[77].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[78][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		2 * pstv->inf_elm[78].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[78][1] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		3 * pstv->inf_elm[78].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[79][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		4 * pstv->inf_elm[79].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[80][0] =
		fd->dma_para->fd_out_hw_pa[77][0] +
		pstv->inf_elm[77].fd_wdma_size[0] +
		pstv->inf_elm[77].fd_wdma_size[1] +
		pstv->inf_elm[78].fd_wdma_size[0] +
		pstv->inf_elm[78].fd_wdma_size[1] +
		pstv->inf_elm[79].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[80][1] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		pstv->inf_elm[80].fd_wdma_size[0] +
		pstv->inf_elm[80].fd_wdma_size[2] +
		pstv->inf_elm[81].fd_wdma_size[0] +
		pstv->inf_elm[81].fd_wdma_size[2] +
		pstv->inf_elm[82].fd_wdma_size[0];
	fd->dma_para->fd_out_hw_pa[80][2] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		pstv->inf_elm[80].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[80][3] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		pstv->inf_elm[80].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][0] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		2 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][1] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		2 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][2] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		3 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[81][3] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		3 * pstv->inf_elm[81].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[82][0] =
		fd->dma_para->fd_out_hw_pa[80][0] +
		4 * pstv->inf_elm[82].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[82][1] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		4 * pstv->inf_elm[82].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[83][0] =
		fd->dma_para->fd_out_hw_pa[80][1] +
		pstv->inf_elm[80].fd_wdma_size[1] +
		pstv->inf_elm[80].fd_wdma_size[3] +
		pstv->inf_elm[81].fd_wdma_size[1] +
		pstv->inf_elm[81].fd_wdma_size[3] +
		pstv->inf_elm[82].fd_wdma_size[1];
	fd->dma_para->fd_out_hw_pa[83][1] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		pstv->inf_elm[83].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[84][0] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		2 * pstv->inf_elm[84].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[84][1] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		3 * pstv->inf_elm[84].out_xsize_plus_1;
	fd->dma_para->fd_out_hw_pa[85][0] =
		fd->dma_para->fd_out_hw_pa[83][0] +
		4 * pstv->inf_elm[85].out_xsize_plus_1;

	/* VA : except 28, 57, 86 */
	/* 0~86 */
	fd->dma_para->fd_out_hw_va[0][0] = fd->fd_dma_hw.va;
	for (i = 1; i < FD_LOOP_NUM; i++) {
		if (i == RPN2_LOOP_NUM || i == RPN1_LOOP_NUM ||
		    i == RPN0_LOOP_NUM)
			continue;
		for (j = 0; j < 4; j++) {
			if (fd_wdma_en[i][j]) {
				fd->dma_para->fd_out_hw_va[i][j] =
					fd->fd_dma_hw.va +
					fd->dma_para->fd_out_hw_pa[i][j] -
					fd->fd_dma_hw.pa;
			}
		}
	}
}

static void aie_free_dram_buf(struct mtk_aie_dev *fd)
{
	aie_imem_free(fd, &fd->rs_cfg_data);
	aie_imem_free(fd, &fd->fd_cfg_data);
	aie_imem_free(fd, &fd->yuv2rgb_cfg_data);
}

static void aie_free_output_buf(struct mtk_aie_dev *fd)
{
	aie_imem_free(fd, &fd->rs_output_hw);
}

static void aie_free_fddma_buf(struct mtk_aie_dev *fd)
{
	aie_imem_free(fd, &fd->fd_dma_hw);
	aie_imem_free(fd, &fd->fd_kernel_hw);
	aie_imem_free(fd, &fd->fd_attr_dma_hw);
	aie_imem_free(fd, &fd->fd_dma_result_hw);
}

static void aie_free_fld_buf(struct mtk_aie_dev *fd)
{
	aie_imem_free(fd, &fd->fd_fld_step_data);
	aie_imem_free(fd, &fd->fd_fld_out_hw);
}

static int aie_copy_fw(struct mtk_aie_dev *fd, const char *name, void *buf,
		       unsigned int size)
{
	int ret = -EINVAL;
	const struct firmware *fw = NULL;

	ret = request_firmware(&fw, name, fd->dev);
	if (ret == 0) {
		if (size >= fw->size)
			memcpy(buf, fw->data, fw->size);
		else
			ret = -EINVAL;
	}

	release_firmware(fw);

	return ret;
}

static int aie_load_fw(struct mtk_aie_dev *fd)
{
	u8 i = 0, j = 0;
	int ret = -EINVAL;
	char name[128] = {};
	char *sel_folder = NULL;
	char *mp_fw30_folder = "aie_mp_fw";
	char *mp_fw31_folder = "aie_mp_fw31";

	if (fd->variant->hw_version == 30)
		sel_folder = mp_fw30_folder;
	else if (fd->variant->hw_version == 31)
		sel_folder = mp_fw31_folder;
	else
		return -EINVAL;

	ret = sprintf(name, "%s/config/aie_fd_fd_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd,
			  name,
			  fd->base_para->fd_fd_cfg_va,
			  fd->fd_fd_cfg_size
		);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_fd_rs_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd,
			  name,
			  fd->base_para->fd_rs_cfg_va,
			  fd->fd_rs_cfg_size
		);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_fd_yuv2rgb_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd,
			  name,
			  fd->base_para->fd_yuv2rgb_cfg_va,
			  fd->fd_yuv2rgb_cfg_size
		);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_attr_fd_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd,
			  name,
			  fd->base_para->attr_fd_cfg_va[0],
			  fd->attr_fd_cfg_size
		);
	if (ret)
		return ret;

	ret = sprintf(name, "%s/config/aie_attr_yuv2rgb_config.bin", sel_folder);
	if (ret < 0)
		return ret;

	ret = aie_copy_fw(fd,
			  name,
			  fd->base_para->attr_yuv2rgb_cfg_va[0],
			  fd->attr_yuv2rgb_cfg_size
		);
	if (ret)
		return ret;

	for (i = 1; i < MAX_ENQUE_FRAME_NUM; i++) {
		memcpy(fd->base_para->attr_fd_cfg_va[i],
		       fd->base_para->attr_fd_cfg_va[0], fd->attr_fd_cfg_size);
		memcpy(fd->base_para->attr_yuv2rgb_cfg_va[i],
		       fd->base_para->attr_yuv2rgb_cfg_va[0],
		       fd->attr_yuv2rgb_cfg_size);
	}

	for (i = 0; i < FD_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			if (fd_ker_rdma_size[i][j]) {
				ret = sprintf(name,
					      "%s/kernel/aie_fd_kernel_bias_loop%02d_%d.bin",
					      sel_folder,
					      i,
					      j
					);
				if (ret < 0)
					return ret;

				ret = aie_copy_fw(fd,
						  name,
						  fd->dma_para->fd_kernel_va[i][j],
						  fd_ker_rdma_size[i][j]
					);
				if (ret)
					return ret;
			}
		}
	}

	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			ret = sprintf(name,
				      "%s/kernel/aie_attr_kernel_bias_loop%02d_%d.bin",
				      sel_folder,
				      i,
				      j
				);
			if (ret < 0)
				return ret;

			ret = aie_copy_fw(fd,
					  name,
					  fd->dma_para->attr_kernel_va[i][j],
					  attr_ker_rdma_size[i][j]
				);
			if (ret)
				return ret;
		}
	}

	if (fd->variant->fld_enable) {
		ret = sprintf(name, "%s/config/aie_fld_blink_weight_forest14.bin", sel_folder);
		if (ret < 0)
			return ret;

		ret = aie_copy_fw(fd,
				  name,
				  fd->fld_para->fld_step_va[FLD_STEP_BLINK][14],
				  fld_step_align_size[FLD_STEP_BLINK][14]
			);
		if (ret)
			return ret;

		for (j = 0; j < FLD_MAX_FRAME; j++) {
			ret = sprintf(name,
				      "%s/config/aie_fld_cv_forest%02d_iom3.bin",
				      sel_folder,
				      j
				);
			if (ret < 0)
				return ret;

			ret = aie_copy_fw(fd,
					  name,
					  fd->fld_para->fld_step_va[FLD_STEP_CV][j],
					  fld_step_align_size[FLD_STEP_CV][j]
				);
			if (ret)
				return ret;
		}

		for (j = 0; j < FLD_MAX_FRAME; j++) {
			ret = sprintf(name,
				      "%s/config/aie_fld_fp_forest%02d_om45.bin",
				      sel_folder,
				      j
				);
			if (ret < 0)
				return ret;

			ret = aie_copy_fw(fd,
					  name,
					  fd->fld_para->fld_step_va[FLD_STEP_FP][j],
					  fld_step_align_size[FLD_STEP_FP][j]
				);
			if (ret)
				return ret;
		}

		for (j = 0; j < FLD_MAX_FRAME; j++) {
			ret = sprintf(name,
				      "%s/config/aie_fld_leafnode_forest%02d.bin",
				      sel_folder,
				      j
				);
			if (ret < 0)
				return ret;

			ret = aie_copy_fw(fd,
					  name,
					  fd->fld_para->fld_step_va[FLD_STEP_LEAF][j],
					  fld_step_align_size[FLD_STEP_LEAF][j]
				);
			if (ret)
				return ret;
		}

		for (j = 0; j < FLD_MAX_FRAME; j++) {
			ret = sprintf(name,
				      "%s/config/aie_fld_tree_forest%02d_km02.bin",
				      sel_folder,
				      j
				);
			if (ret < 0)
				return ret;
			ret = aie_copy_fw(fd,
					  name,
					  fd->fld_para->fld_step_va[FLD_STEP_KM02][j],
					  fld_step_align_size[FLD_STEP_KM02][j]
				);
			if (ret)
				return ret;
		}

		for (j = 0; j < FLD_MAX_FRAME; j++) {
			ret = sprintf(name,
				      "%s/config/aie_fld_tree_forest%02d_km13.bin",
				      sel_folder,
				      j
				);
			if (ret < 0)
				return ret;
			ret = aie_copy_fw(fd,
					  name,
					  fd->fld_para->fld_step_va[FLD_STEP_KM13][j],
					  fld_step_align_size[FLD_STEP_KM13][j]
				);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static void aie_reset_output_buf(struct mtk_aie_dev *fd,
				 struct aie_enq_info *aie_cfg)
{
	if (aie_cfg->sel_mode == FDMODE) {
		memset(fd->rs_output_hw.va, 0, fd->rs_output_hw.size);
		memset(fd->dma_para->fd_out_hw_va[RPN0_LOOP_NUM][0], 0,
		       RESULT_SIZE);
		memset(fd->dma_para->fd_out_hw_va[RPN1_LOOP_NUM][0], 0,
		       RESULT_SIZE);
		memset(fd->dma_para->fd_out_hw_va[RPN2_LOOP_NUM][0], 0,
		       RESULT_SIZE);
	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		memset(fd->base_para->rs_pym_rst_va[0][0], 0,
		       fd->rs_pym_out_size[0]);
		memset(fd->base_para->rs_pym_rst_va[0][1], 0,
		       fd->rs_pym_out_size[0]);
		memset(fd->base_para->rs_pym_rst_va[0][2], 0,
		       fd->rs_pym_out_size[0]);
	} else if (aie_cfg->sel_mode == FLDMODE) {
		if (fd->variant->fld_enable)
			memset(fd->fld_para->fld_output_va[0], 0,
			       FLD_MAX_FRAME * FLD_OUTPUT_SIZE);
	}
}

static int aie_update_cfg(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int crop_width = 0;
	int crop_height = 0;

	crop_width = aie_cfg->src_img_width;
	crop_height = aie_cfg->src_img_height;

	if (aie_cfg->en_roi) {
		crop_width = dif_x(aie_cfg) + 1;
		crop_height = dif_y(aie_cfg) + 1;
	}

	if (crop_width == 0 || crop_height == 0) {
		dev_err(fd->dev, "AIE error:crop size is wrong");
		return -EINVAL;
	}

	if (aie_cfg->en_padding) {
		crop_width = crop_width + aie_cfg->src_padding.right +
			     aie_cfg->src_padding.left;
		crop_height = crop_height + aie_cfg->src_padding.up +
			      aie_cfg->src_padding.down;
	}

	if (aie_cfg->sel_mode == FDMODE) {
		fd->base_para->sel_mode = aie_cfg->sel_mode;
		fd->base_para->crop_width = crop_width;
		fd->base_para->crop_height = crop_height;
		fd->base_para->src_img_addr = aie_cfg->src_img_addr;
		fd->base_para->src_img_addr_uv = aie_cfg->src_img_addr_uv;
		fd->base_para->img_width = aie_cfg->src_img_width;
		fd->base_para->img_height = aie_cfg->src_img_height;
		fd->base_para->src_img_fmt = aie_cfg->src_img_fmt;
		fd->base_para->rotate_degree = aie_cfg->rotate_degree;
	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		fd->attr_para->sel_mode[fd->attr_para->w_idx] =
			aie_cfg->sel_mode;
		fd->attr_para->crop_width[fd->attr_para->w_idx] = crop_width;
		fd->attr_para->crop_height[fd->attr_para->w_idx] = crop_height;
		fd->attr_para->src_img_addr[fd->attr_para->w_idx] =
			aie_cfg->src_img_addr;
		fd->attr_para->src_img_addr_uv[fd->attr_para->w_idx] =
			aie_cfg->src_img_addr_uv;
		fd->attr_para->img_width[fd->attr_para->w_idx] =
			aie_cfg->src_img_width;
		fd->attr_para->img_height[fd->attr_para->w_idx] =
			aie_cfg->src_img_height;
		fd->attr_para->src_img_fmt[fd->attr_para->w_idx] =
			aie_cfg->src_img_fmt;
		fd->attr_para->rotate_degree[fd->attr_para->w_idx] =
			aie_cfg->rotate_degree;
	}

	return 0;
}

static int aie_config_y2r(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg,
			  int mode)
{
	u32 img_addr = 0;
	u32 img_addr_UV = 0;
	u32 img_off = 0;
	u32 img_off_uv = 0;
	u32 *yuv2rgb_cfg = NULL;
	u32 srcbuf, srcbuf_UV = 0;
	u16 xmag_0 = 0, ymag_0 = 0;
	u16 pym0_out_w = 0;
	u16 pym0_out_h = 0;
	u16 stride_pym0_out_w = 0;
	u16 sr_crp_w = 0;
	u16 sr_crp_h = 0;
	u16 y1_stride = 0;

	if (!aie_cfg->en_roi) {
		img_off = 0;
		img_off_uv = 0;
	} else {
		if (aie_cfg->src_img_fmt == FMT_MONO ||
		    aie_cfg->src_img_fmt == FMT_YUV_2P ||
		    aie_cfg->src_img_fmt == FMT_YVU_2P) {
			y1_stride = aie_cfg->src_img_stride * aie_cfg->src_roi.y1;
			img_off = y1_stride + aie_cfg->src_roi.x1;
			img_off_uv = y1_stride + aie_cfg->src_roi.x1;
		} else if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
			   aie_cfg->src_img_fmt == FMT_YUV420_1P) {
			y1_stride = aie_cfg->src_img_stride * aie_cfg->src_roi.y1;
			img_off = y1_stride + aie_cfg->src_roi.x1;
			img_off_uv = y1_stride / 2 + aie_cfg->src_roi.x1;
		} else if (aie_cfg->src_img_fmt == FMT_YUYV ||
			   aie_cfg->src_img_fmt == FMT_YVYU ||
			   aie_cfg->src_img_fmt == FMT_UYVY ||
			   aie_cfg->src_img_fmt == FMT_VYUY) {
			y1_stride = aie_cfg->src_img_stride * aie_cfg->src_roi.y1;
			img_off = y1_stride + aie_cfg->src_roi.x1 * 2;
			img_off_uv = y1_stride + aie_cfg->src_roi.x1 * 2;
		} else {
			dev_err(fd->dev,
				"AIE error: Unsupport input format %d",
				aie_cfg->src_img_fmt
				);
			return -EINVAL;
		}
	}

	img_addr = aie_cfg->src_img_addr + img_off;
	img_addr_UV = aie_cfg->src_img_addr_uv + img_off_uv;

	srcbuf = img_addr;
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P ||
	    aie_cfg->src_img_fmt == FMT_YUV_2P ||
	    aie_cfg->src_img_fmt == FMT_YVU_2P)
		srcbuf_UV = img_addr_UV;
	else
		srcbuf_UV = 0;

	if (mode == FDMODE) {
		sr_crp_w = fd->base_para->crop_width;
		sr_crp_h = fd->base_para->crop_height;
		yuv2rgb_cfg = (u32 *)fd->base_para->fd_yuv2rgb_cfg_va;
		pym0_out_w = fd->base_para->pyramid_width;
	} else if (mode == ATTRIBUTEMODE) {
		sr_crp_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		sr_crp_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
		yuv2rgb_cfg =
			(u32 *)fd->base_para
				->attr_yuv2rgb_cfg_va[fd->attr_para->w_idx];
		pym0_out_w = ATTR_MODE_PYRAMID_WIDTH;
	}

	pym0_out_h = pym0_out_w * sr_crp_h / sr_crp_w;

	if (pym0_out_w != 0) {
		xmag_0 = 512 * sr_crp_w / pym0_out_w;
		ymag_0 = xmag_0;
	} else {
		xmag_0 = 0;
		ymag_0 = 0;
	}

	yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] =
		(yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] & 0xFFFFFFF8) |
		((aie_cfg->src_img_fmt) & 0x7);
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P) { /* for match patten */
		yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] =
			(yuv2rgb_cfg[Y2R_SRC_DST_FORMAT] & 0xFFFFFFF8) |
			((0x3) & 0x7);
	}
	yuv2rgb_cfg[Y2R_IN_W_H] = (yuv2rgb_cfg[Y2R_IN_W_H] & 0xF800F800) |
				  ((sr_crp_w << 16) & 0x7FF0000) |
				  (sr_crp_h & 0x7FF);
	yuv2rgb_cfg[Y2R_OUT_W_H] = (yuv2rgb_cfg[Y2R_OUT_W_H] & 0xF800F800) |
				   ((pym0_out_w << 16) & 0x7FF0000) |
				   (pym0_out_h & 0x7FF);

	if (aie_cfg->src_img_fmt == FMT_YUV_2P ||
	    aie_cfg->src_img_fmt == FMT_YVU_2P) { /* 2 plane */
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x11;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_cmb_u16(dif_x(aie_cfg), dif_y(aie_cfg));
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(dif_x(aie_cfg), dif_y(aie_cfg));
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_cmb_u16(sr_crp_w - 1, sr_crp_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] =
				aie_cmb_u16(sr_crp_w - 1, sr_crp_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x1;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x1;
	} else if (aie_cfg->src_img_fmt == FMT_MONO) {
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x01;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_cmb_u16(dif_x(aie_cfg), dif_y(aie_cfg));
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(dif_x(aie_cfg), dif_y(aie_cfg));
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_cmb_u16(sr_crp_w - 1, sr_crp_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] =
				aie_cmb_u16(sr_crp_w - 1, sr_crp_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
	} else if (aie_cfg->src_img_fmt == FMT_YUYV ||
		   aie_cfg->src_img_fmt == FMT_YVYU ||
		   aie_cfg->src_img_fmt == FMT_UYVY ||
		   aie_cfg->src_img_fmt == FMT_VYUY) { /* 1 plane */
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x1;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_cmb_u16(2 * (dif_x(aie_cfg) + 1) - 1,
								    dif_y(aie_cfg)
							);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(2 * (dif_x(aie_cfg) + 1) - 1,
								    dif_y(aie_cfg)
							);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_cmb_u16(2 * sr_crp_w - 1, sr_crp_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(2 * sr_crp_w - 1, sr_crp_h - 1);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x3;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x3;
	}

	/* AIE3.0 */
	if (aie_cfg->src_img_fmt == FMT_YUV420_2P ||
	    aie_cfg->src_img_fmt == FMT_YUV420_1P) {
		yuv2rgb_cfg[Y2R_RA0_RA1_EN] =
			(yuv2rgb_cfg[Y2R_RA0_RA1_EN] & 0xFFFFFFEE) | 0x11;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] = aie_cmb_u16(dif_x(aie_cfg), dif_y(aie_cfg));
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(dif_x(aie_cfg),
								    dif_y(aie_cfg) / 2
							);
		} else {
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE0] =
				aie_cmb_u16(sr_crp_w - 1, sr_crp_h - 1);
			yuv2rgb_cfg[Y2R_IN_X_Y_SIZE1] = aie_cmb_u16(sr_crp_w - 1,
								    sr_crp_h / 2 - 1
							);
		}
		yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE0_BUS_SIZE0] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;
		yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] =
			(yuv2rgb_cfg[Y2R_IN_STRIDE1_BUS_SIZE1] & 0xFFF0) |
			((aie_cfg->src_img_stride << 16) & 0xFFFF0000) | 0x0;

		yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] =
			(yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] & 0xFFFFFFFE) | 0x01;
		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] = aie_cmb_u16(0, dif_x(aie_cfg));
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] = aie_cmb_u16(0, dif_y(aie_cfg));
		} else {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] =
				aie_cmb_u16(0, sr_crp_w - 1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] =
				aie_cmb_u16(0, sr_crp_h - 1);
		}
	} else {
		yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] =
			(yuv2rgb_cfg[Y2R_CO2_FMT_MODE_EN] & 0xFFFFFFFE);

		if (aie_cfg->en_roi) {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] = aie_cmb_u16(0, dif_x(aie_cfg));
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] = aie_cmb_u16(0, dif_y(aie_cfg));
		} else {
			yuv2rgb_cfg[Y2R_CO2_CROP_X] =
				aie_cmb_u16(0, sr_crp_w - 1);
			yuv2rgb_cfg[Y2R_CO2_CROP_Y] =
				aie_cmb_u16(0, sr_crp_h - 1);
		}
	}

	stride_pym0_out_w = round_up(pym0_out_w, 8);

	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE0] =
		aie_cmb_u16(pym0_out_w - 1, pym0_out_h - 1);
	set_cmb_cfg(yuv2rgb_cfg, Y2R_OUT_STRIDE0_BUS_SIZE0, stride_pym0_out_w);
	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE1] =
		aie_cmb_u16(pym0_out_w - 1, pym0_out_h - 1);
	set_cmb_cfg(yuv2rgb_cfg, Y2R_OUT_STRIDE1_BUS_SIZE1, stride_pym0_out_w);
	yuv2rgb_cfg[Y2R_OUT_X_Y_SIZE2] =
		aie_cmb_u16(pym0_out_w - 1, pym0_out_h - 1);
	set_cmb_cfg(yuv2rgb_cfg, Y2R_OUT_STRIDE2_BUS_SIZE2, stride_pym0_out_w);

	if (aie_cfg->en_padding) {
		yuv2rgb_cfg[Y2R_PADDING_EN_UP_DOWN] =
			1 | ((aie_cfg->src_padding.up << 4) & 0x1FF0) |
			((aie_cfg->src_padding.down << 16) & 0x01FF0000);
		yuv2rgb_cfg[Y2R_PADDING_RIGHT_LEFT] =
			(aie_cfg->src_padding.right & 0x01FF) |
			((aie_cfg->src_padding.left << 16) & 0x01FF0000);
	} else {
		yuv2rgb_cfg[Y2R_PADDING_EN_UP_DOWN] = 0;
		yuv2rgb_cfg[Y2R_PADDING_RIGHT_LEFT] = 0;
	}

	yuv2rgb_cfg[Y2R_IN_0] = srcbuf;
	yuv2rgb_cfg[Y2R_IN_1] = srcbuf_UV;

	yuv2rgb_cfg[Y2R_OUT_0] = (u32)fd->base_para->rs_pym_rst_pa[0][0];
	yuv2rgb_cfg[Y2R_OUT_1] = (u32)fd->base_para->rs_pym_rst_pa[0][1];
	yuv2rgb_cfg[Y2R_OUT_2] = (u32)fd->base_para->rs_pym_rst_pa[0][2];

	yuv2rgb_cfg[Y2R_X_Y_MAG] = (xmag_0 & 0x3FFF) |
				   ((ymag_0 << 16) & 0x3FFF0000);

	if (sr_crp_w >= pym0_out_w) { /* down scale AIE1.0 by FRZ */
		yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] =
			(yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] & 0x00100070);
		yuv2rgb_cfg[Y2R_SRZ_HORI_STEP] = 0;
		yuv2rgb_cfg[Y2R_SRZ_VERT_STEP] = 0;
	} else { /* SRZ */
		/* 0: FDRZ for down scaling */
		/* 1: SRZ for up scaling */
		yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] =
			(yuv2rgb_cfg[Y2R_RS_SEL_SRZ_EN] & 0x00100070) | SRZ_BIT;
		yuv2rgb_cfg[Y2R_SRZ_HORI_STEP] =
			((sr_crp_w - 1) << 15) / (pym0_out_w - 1);
		yuv2rgb_cfg[Y2R_SRZ_VERT_STEP] =
			((sr_crp_h - 1) << 15) / (pym0_out_h - 1);
	}

	if (fd->variant->hw_version == 31) {
		yuv2rgb_cfg[Y2R_CON_IN_BA_MSB] = (u32)0x02020202;
		yuv2rgb_cfg[Y2R_CON_OUT_BA_MSB] = (u32)0x02020202;
	}

	return 0;
}

static int aie_config_rs(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	u32 *rs_cfg = NULL;
	u32 *rs_tbl[2] = { NULL, NULL };
	u16 xmag_0 = 0, ymag_0 = 0;
	u16 pym_out_w[3] = { 0, 0, 0 };
	u16 pym_out_h[3] = { 0, 0, 0 };
	u16 round_w = 0;
	u16 sr_crp_w = 0;
	u16 sr_crp_h = 0;
	int i = 0;

	if (aie_cfg->sel_mode == FDMODE) {
		sr_crp_w = fd->base_para->crop_width;
		sr_crp_h = fd->base_para->crop_height;
	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		sr_crp_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		sr_crp_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
	}

	rs_cfg = (u32 *)fd->base_para->fd_rs_cfg_va;

	pym_out_w[0] = fd->base_para->pyramid_width;
	pym_out_w[1] = pym_out_w[0] >> 1;
	pym_out_w[2] = pym_out_w[1] >> 1;

	pym_out_h[0] = pym_out_w[0] * sr_crp_h / sr_crp_w;
	pym_out_h[1] = pym_out_h[0] >> 1;
	pym_out_h[2] = pym_out_h[1] >> 1;

	for (i = 0; i < 2; i++) {
		rs_tbl[i] = rs_cfg + fd->variant->rs_cfg_size * i;

		rs_tbl[i][RS_IN_0] = (u32)fd->base_para->rs_pym_rst_pa[i][0];
		rs_tbl[i][RS_IN_1] = (u32)fd->base_para->rs_pym_rst_pa[i][1];
		rs_tbl[i][RS_IN_2] = (u32)fd->base_para->rs_pym_rst_pa[i][2];

		rs_tbl[i][RS_OUT_0] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][0];
		rs_tbl[i][RS_OUT_1] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][1];
		rs_tbl[i][RS_OUT_2] =
			(u32)fd->base_para->rs_pym_rst_pa[i + 1][2];

		rs_tbl[i][RS_INPUT_W_H] =
			(rs_tbl[i][RS_INPUT_W_H] & 0xF800F800) |
			(pym_out_h[i] & 0x7FF) |
			((pym_out_w[i] << 16) & 0x7FF0000);
		rs_tbl[i][RS_OUTPUT_W_H] =
			(rs_tbl[i][RS_OUTPUT_W_H] & 0xF800F800) |
			(pym_out_h[i + 1] & 0x7FF) |
			((pym_out_w[i + 1] << 16) & 0x7FF0000);
		rs_tbl[i][RS_IN_X_Y_SIZE0] =
			aie_cmb_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		rs_tbl[i][RS_IN_X_Y_SIZE1] =
			aie_cmb_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		rs_tbl[i][RS_IN_X_Y_SIZE2] =
			aie_cmb_u16(pym_out_w[i] - 1, pym_out_h[i] - 1);
		set_cmb_cfg(rs_tbl[i], RS_IN_STRIDE0, pym_out_w[i]);
		set_cmb_cfg(rs_tbl[i], RS_IN_STRIDE1, pym_out_w[i]);
		set_cmb_cfg(rs_tbl[i], RS_IN_STRIDE2, pym_out_w[i]);
		rs_tbl[i][RS_OUT_X_Y_SIZE0] = aie_cmb_u16(pym_out_w[i + 1] - 1,
							  pym_out_h[i + 1] - 1
						);
		rs_tbl[i][RS_OUT_X_Y_SIZE1] = aie_cmb_u16(pym_out_w[i + 1] - 1,
							  pym_out_h[i + 1] - 1
						);
		rs_tbl[i][RS_OUT_X_Y_SIZE2] = aie_cmb_u16(pym_out_w[i + 1] - 1,
							  pym_out_h[i + 1] - 1
						);

		if (i == 0)
			round_w = pym_out_w[i + 1];
		else
			round_w = round_up(pym_out_w[i + 1], 8);

		set_cmb_cfg(rs_tbl[i], RS_OUT_STRIDE0, round_w);
		set_cmb_cfg(rs_tbl[i], RS_OUT_STRIDE1, round_w);
		set_cmb_cfg(rs_tbl[i], RS_OUT_STRIDE2, round_w);

		xmag_0 = 512 * pym_out_w[i] / pym_out_w[i + 1];
		ymag_0 = xmag_0;

		rs_tbl[i][RS_X_Y_MAG] = (xmag_0 & 0x3FFF) |
					((ymag_0 << 16) & 0x3FFF0000);

		if (fd->variant->hw_version == 31) {
			rs_tbl[i][RS_CON_IN_BA_MSB] = (u32)0x02020202;
			rs_tbl[i][RS_CON_OUT_BA_MSB] = (u32)0x02020202;
		}
	}

	return 0;
}

static int aie_config_network(struct mtk_aie_dev *fd,
			      struct aie_enq_info *aie_cfg)
{
	u16 conv_width = 0;
	u16 conv_height = 0;
	u8 i = 0;
	u8 j = 0;
	u8 uch = 0;
	u8 uloop = 0;
	u16 fd_xsize[4] = { 0, 0, 0, 0 };
	void *fd_cfg = NULL;
	u32 *fd_cur_cfg = NULL;
	u32 *fd_cur_set = NULL;
	u16 pyramid0_out_w = 0;
	u16 pyramid0_out_h = 0;
	u16 pyramid1_out_h = 0;
	u16 pyramid2_out_h = 0;
	u16 input_height = 0;
	u16 out_height = 0;
	u16 out_ysize_plus_1 = 0;
	u16 out_ysize_plus_1_stride2 = 0;
	u32 sr_crp_w = 0;
	u32 sr_crp_h = 0;
	struct aie_static_info *pstv = &fd->st_info;
	u32 cal_x = 0;
	u32 cal_y = 0;

	if (aie_cfg->sel_mode == FDMODE) {
		sr_crp_w = fd->base_para->crop_width;
		sr_crp_h = fd->base_para->crop_height;
	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		sr_crp_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
		sr_crp_h = fd->attr_para->crop_height[fd->attr_para->w_idx];
	}

	pyramid0_out_w = fd->base_para->pyramid_width;
	pyramid0_out_h = pyramid0_out_w * sr_crp_h / sr_crp_w;

	pyramid1_out_h = pyramid0_out_h / 2;
	pyramid2_out_h = pyramid1_out_h / 2;

	fd_cfg = fd->base_para->fd_fd_cfg_va;

	for (i = 0; i < FD_LOOP_NUM; i++) {
		fd_cur_cfg = (u32 *)fd_cfg + fd->variant->fd_cfg_size * i;
		fd_cur_cfg[FD_INPUT_ROTATE] =
			(fd_cur_cfg[FD_INPUT_ROTATE] & 0xFFFF0FFF) |
			((aie_cfg->rotate_degree << 12) & 0x3000);

		if (i == 0)
			input_height = pyramid2_out_h;
		else if (i == (RPN2_LOOP_NUM + 1))
			input_height = pyramid1_out_h;
		else if (i == (RPN1_LOOP_NUM + 1))
			input_height = pyramid0_out_h;
		else
			if (fd_out_stride2_in[i] == 0)
				input_height = out_height;
			else
				input_height = (out_height + 1) / 2;

		if (fd_maxpool[i] == 1 && fd_stride[i] == 1)
			out_height =
				DIV_ROUND_UP(input_height, 2 * fd_maxpool[i]);
		else
			out_height = DIV_ROUND_UP(input_height, fd_stride[i] + 2 * fd_maxpool[i]);

		if (i == RPN0_LOOP_NUM || i == RPN1_LOOP_NUM ||
		    i == RPN2_LOOP_NUM) {
			conv_width = fd->base_para->img_width;
			conv_height = fd->base_para->img_height;
			fd_xsize[0] = pstv->inf_elm[i].img_width * 2 * 16 *
					      anchor_en_num[i] -
				      1;
			fd_xsize[3] = pstv->inf_elm[i].img_width * 2 * 32 *
					      anchor_en_num[i] - 1;
			fd_xsize[2] = fd_xsize[3];
			fd_xsize[1] = fd_xsize[2];
		} else {
			conv_width = DIV_ROUND_UP(pstv->inf_elm[i].img_width, fd_stride[i]);
			conv_height = DIV_ROUND_UP(input_height, fd_stride[i]);

			fd_xsize[3] = pstv->inf_elm[i].input_xsize_plus_1 - 1;
			fd_xsize[2] = fd_xsize[3];
			fd_xsize[1] = fd_xsize[2];
			fd_xsize[0] = fd_xsize[1];
		}

		fd_cur_cfg[FD_CONV_WIDTH_MOD6] =
			(fd_cur_cfg[FD_CONV_WIDTH_MOD6] & 0xFF8FFFFF) |
			(((conv_width % 6) << 20) & 0x00700000);
		fd_cur_cfg[FD_CONV_IMG_W_H] =
			aie_cmb_u16(conv_height, conv_width);

		fd_cur_cfg[FD_IN_IMG_W_H] = aie_cmb_u16(input_height, pstv->inf_elm[i].img_width);
		fd_cur_cfg[FD_OUT_IMG_W_H] = aie_cmb_u16(out_height, pstv->inf_elm[i].out_width);

		if (fd_rdma_en[i][0][0] != -1) {
			for (j = 0; j < 4; j++) {
				fd_cur_cfg[FD_IN_X_Y_SIZE0 + 2 * j] =
					aie_cmb_u16(fd_xsize[j], input_height - 1);
				set_cmbst_cfg(fd_cur_cfg,
					      FD_IN_STRIDE0_BUS_SIZE0 + 2 * j,
					      fd_xsize[j] + 1
				);
			}
		}

		out_ysize_plus_1 = out_height - 1;
		out_ysize_plus_1_stride2 = (out_height + 1) / 2 - 1;

		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			fd_cur_set = fd_cur_cfg + 2 * j;
			if (!fd_wdma_en[i][j])
				continue;

			if (out_stride_size[i][j] == 1) {
				fd_cur_set[FD_OUT_X_Y_SIZE0] =
					aie_cmb_u16(pstv->inf_elm[i].out_xsize_plus_1 - 1,
						    out_ysize_plus_1
					);
				set_cmbst_cfg(fd_cur_set,
					      FD_OUT_STRIDE0_BUS_SIZE0 + 2 * j,
					      pstv->inf_elm[i].out_stride
				);
			} else if (out_stride_size[i][j] == 2) {
				fd_cur_set[FD_OUT_X_Y_SIZE0] =
					aie_cmb_u16(pstv->inf_elm[i].out_xsize_plus_1_stride2 - 1,
						    out_ysize_plus_1_stride2
					);
				set_cmbst_cfg(fd_cur_set,
					      FD_OUT_STRIDE0_BUS_SIZE0,
					      pstv->inf_elm[i].out_stride_stride2
				);
			}
		}

		if (i == RPN0_LOOP_NUM || i == RPN1_LOOP_NUM || i == RPN2_LOOP_NUM)
			set_cmb_cfg(fd_cur_cfg, FD_RPN_SET + 2 * j, fd->base_para->rpn_anchor_thrd);

		if (i == RPN0_LOOP_NUM) {
			cal_x = ((sr_crp_w << 10) * 100 /
				 (int)fd->base_para->pyramid_width) >>
				10;
			cal_y = cal_x * 512 / 100;
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				((cal_y << 4) & 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
					(aie_cfg->src_roi.x1 -
					 aie_cfg->src_padding.left) |
					(aie_cfg->src_roi.y1 -
					 aie_cfg->src_padding.up)
						<< 16;
			}
		} else if (i == RPN1_LOOP_NUM) {
			cal_x = ((sr_crp_w << 10) * 100 /
				 (int)fd->base_para->pyramid_width) >>
				10;
			cal_y = cal_x * 2 * 512 / 100;
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				((cal_y << 4) & 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
					(aie_cfg->src_roi.x1 -
					 aie_cfg->src_padding.left) |
					(aie_cfg->src_roi.y1 -
					 aie_cfg->src_padding.up)
						<< 16;
			}
		} else if (i == RPN2_LOOP_NUM) {
			cal_x = ((sr_crp_w << 10) * 100 /
				 (int)fd->base_para->pyramid_width) >>
				10;
			cal_y = cal_x * 4 * 512 / 100;
			fd_cur_cfg[FD_IMAGE_COORD] =
				(fd_cur_cfg[FD_IMAGE_COORD] & 0xF) |
				((cal_y << 4) & 0x7FFF0);
			fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] = 0;
			if (aie_cfg->en_roi) {
				fd_cur_cfg[FD_IMAGE_COORD_XY_OFST] =
					(aie_cfg->src_roi.x1 -
					 aie_cfg->src_padding.left) |
					(aie_cfg->src_roi.y1 -
					 aie_cfg->src_padding.up)
						<< 16;
			}
		}

		/* IN_FM_BASE_ADR */
		if (i == 0) {
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[2][2]);
		} else if (i == (RPN2_LOOP_NUM + 1)) {
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[1][2]);
		} else if (i == (RPN1_LOOP_NUM + 1)) {
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][2]);
		} else {
			for (j = 0; j < INPUT_WDMA_WRA_NUM; j++) {
				if (fd_rdma_en[i][j][0] != -1) {
					uloop = fd_rdma_en[i][j][0];
					uch = fd_rdma_en[i][j][1];
					fd_cur_cfg[FD_IN_0 + j] =
						(u32)(fd->dma_para->fd_out_hw_pa
							      [uloop][uch]);
				}
			}
		}

		/* OUT_FM_BASE_ADR */
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (fd_wdma_en[i][j])
				fd_cur_cfg[FD_OUT_0 + j] =
					(u32)(fd->dma_para->fd_out_hw_pa[i][j]);
		}

		/* KERNEL_BASE_ADR */
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			if (fd_ker_rdma_size[i][j])
				fd_cur_cfg[FD_KERNEL_0 + j] =
					(u32)(fd->dma_para->fd_kernel_pa[i][j]);
		}

		if (fd->variant->hw_version == 31) {
			fd_cur_cfg[FD_CON_IN_BA_MSB] = (u32)0x02020202;
			fd_cur_cfg[FD_CON_OUT_BA_MSB] = (u32)0x02020202;
			fd_cur_cfg[FD_CON_KERNEL_BA_MSB] = (u32)0x00000202;
		}
	}

	return 0;
}

static int aie_config_attr_network(struct mtk_aie_dev *fd,
				   struct aie_enq_info *aie_cfg)
{
	bool is_regression_loop = false;
	void *fd_cfg = NULL;
	u32 *fd_cur_cfg = NULL;
	u16 fd_input_ht = 0, fd_output_ht = 0;
	u16 fd_out_y[4] = { 0, 0, 0, 0 };
	u8 i = 0, j = 0;
	u8 uloop = 0, uch = 0, uidx = 0;
	u16 pyramid0_out_w = 0, pyramid0_out_h = 0;
	int fd_conv_ht = 0;
	u16 sr_crp_w = 0;
	u16 sr_crp_h = 0;

	sr_crp_w = fd->attr_para->crop_width[fd->attr_para->w_idx];
	sr_crp_h = fd->attr_para->crop_height[fd->attr_para->w_idx];

	pyramid0_out_w = ATTR_MODE_PYRAMID_WIDTH;
	pyramid0_out_h = pyramid0_out_w * sr_crp_h / sr_crp_w;

	fd_cfg = fd->base_para->attr_fd_cfg_va[fd->attr_para->w_idx];

	for (i = 0; i < ATTR_LOOP_NUM; i++) {
		fd_cur_cfg = (u32 *)fd_cfg + fd->variant->fd_cfg_size * i;
		fd_cur_cfg[FD_INPUT_ROTATE] =
			(fd_cur_cfg[FD_INPUT_ROTATE] & 0xFFFF0FFF) |
			((aie_cfg->rotate_degree << 12) & 0x3000);
		if (i == 0)
			fd_input_ht = pyramid0_out_h;
		else
			if (attr_out_stride2_as_in[i] == 0)
				fd_input_ht = fd_output_ht;
			else if (attr_out_stride2_as_in[i] == 1)
				fd_input_ht = (fd_output_ht + 1) / 2;

		fd_output_ht = DIV_ROUND_UP(fd_input_ht,
					    attr_fd_stride[i] +
					    2 * attr_fd_maxpool[i]
				);
		fd_conv_ht = DIV_ROUND_UP(fd_input_ht, attr_fd_stride[i]);

		fd_cur_cfg[FD_CONV_IMG_W_H] =
			(fd_cur_cfg[FD_CONV_IMG_W_H] & 0xFFFF0000) |
			(fd_conv_ht & 0xFFFF);
		fd_cur_cfg[FD_IN_IMG_W_H] =
			(fd_cur_cfg[FD_IN_IMG_W_H] & 0xFFFF0000) |
			(fd_input_ht & 0xFFFF);
		fd_cur_cfg[FD_OUT_IMG_W_H] =
			(fd_cur_cfg[FD_OUT_IMG_W_H] & 0xFFFF0000) |
			(fd_output_ht & 0xFFFF);
		set_cmb_cfg(fd_cur_cfg, FD_IN_X_Y_SIZE0, fd_input_ht - 1);
		set_cmb_cfg(fd_cur_cfg, FD_IN_X_Y_SIZE1, fd_input_ht - 1);
		set_cmb_cfg(fd_cur_cfg, FD_IN_X_Y_SIZE2, fd_input_ht - 1);
		set_cmb_cfg(fd_cur_cfg, FD_IN_X_Y_SIZE3, fd_input_ht - 1);

		is_regression_loop = (i == AGE_OUT_RGS || i == GENDER_OUT_RGS ||
				    i == INDIAN_OUT_RGS || i == RACE_OUT_RGS);

		if (is_regression_loop) {
			fd_out_y[0] = 0;
			fd_out_y[1] = 0;
			fd_out_y[2] = 0;
			fd_out_y[3] = 0;
		} else {
			fd_out_y[0] = fd_output_ht - 1;
			fd_out_y[1] = fd_output_ht - 1;
			if (attr_out_2size[i] == 0) {
				fd_out_y[2] = fd_output_ht - 1;
				fd_out_y[3] = fd_output_ht - 1;
			} else {
				fd_out_y[2] = (fd_output_ht + 1) / 2 - 1;
				fd_out_y[3] = (fd_output_ht + 1) / 2 - 1;
			}
		}

		for (j = 0; j < 4; j++)
			set_cmb_cfg(fd_cur_cfg, FD_OUT_X_Y_SIZE0 + 2 * j, fd_out_y[j]);

		/* IN_FM_BASE_ADR */
		if (i == 0) {
			fd_cur_cfg[FD_IN_0] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][0]);
			fd_cur_cfg[FD_IN_1] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][1]);
			fd_cur_cfg[FD_IN_2] =
				(u32)(fd->base_para->rs_pym_rst_pa[0][2]);
		} else {
			for (j = 0; j < INPUT_WDMA_WRA_NUM; j++) {
				if (attr_rdma_en[i][j][0] != -1) {
					uloop = attr_rdma_en[i][j][0];
					uch = attr_rdma_en[i][j][1];
					fd_cur_cfg[FD_IN_0 + j] =
						(u32)(fd->dma_para->attr_out_hw_pa
							      [uloop][uch]);
				}
			}
		}

		/* OUT_FM_BASE_ADR */
		for (j = 0; j < OUTPUT_WDMA_WRA_NUM; j++) {
			if (attr_wdma_en[i][j]) {
				uidx = fd->attr_para->w_idx;
				if (i == AGE_OUT_RGS && j == 0)
					fd_cur_cfg[FD_OUT_0 + j] =
						(u32)(fd->dma_para->age_out_hw_pa
							      [uidx]);
				else if (i == GENDER_OUT_RGS && j == 0)
					fd_cur_cfg[FD_OUT_0 + j] =
						(u32)(fd->dma_para
							      ->gender_out_hw_pa
								      [uidx]);
				else if (i == INDIAN_OUT_RGS && j == 0)
					fd_cur_cfg[FD_OUT_0 + j] =
						(u32)(fd->dma_para
							      ->is_indian_out_hw_pa
								      [uidx]);
				else if (i == RACE_OUT_RGS && j == 0)
					fd_cur_cfg[FD_OUT_0 + j] =
						(u32)(fd->dma_para
							      ->race_out_hw_pa
								      [uidx]);
				else
					fd_cur_cfg[FD_OUT_0 + j] =
						(u32)(fd->dma_para
							      ->attr_out_hw_pa
								      [i][j]);
			}
		}

		/* KERNEL_BASE_ADR */
		for (j = 0; j < KERNEL_RDMA_RA_NUM; j++) {
			fd_cur_cfg[FD_KERNEL_0 + j] =
				(u32)(fd->dma_para->attr_kernel_pa[i][j]);
		}

		if (fd->variant->hw_version == 31) {
			fd_cur_cfg[FD_CON_IN_BA_MSB] = (u32)0x02020202;
			fd_cur_cfg[FD_CON_OUT_BA_MSB] = (u32)0x02020202;
			fd_cur_cfg[FD_CON_KERNEL_BA_MSB] = (u32)0x00000202;
		}
	}
	return 0;
}

static int aie_config_dram(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int ret = -EINVAL;

	if (aie_cfg->sel_mode == FDMODE) {
		ret = aie_config_y2r(fd, aie_cfg, aie_cfg->sel_mode);
		if (ret)
			return ret;

		ret = aie_config_rs(fd, aie_cfg);
		if (ret)
			return ret;

		ret = aie_config_network(fd, aie_cfg);
		if (ret)
			return ret;

	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		ret = aie_config_y2r(fd, aie_cfg, aie_cfg->sel_mode);
		if (ret)
			return ret;

		ret = aie_config_attr_network(fd, aie_cfg);
		if (ret)
			return ret;
	}

	return ret;
}

void aie_reset(struct mtk_aie_dev *fd)
{
	writel(0x30000, fd->fd_base + AIE_START_REG);
	writel(0x0, fd->fd_base + AIE_START_REG);
}

int aie_init(struct mtk_aie_dev *fd, struct user_init *user_init)
{
	int ret = -ENOMEM;
	int i = 0, j = 0;

	if (fd->fd_state & STATE_INIT) {
		dev_err(fd->dev, "%s fd state: %d\n", __func__, fd->fd_state);
		return -EINVAL;
	}

	fd->fd_state &= ~STATE_INIT;
	fd->fd_mem_size = 0;

	fd->base_para = kmalloc(sizeof(*fd->base_para), GFP_KERNEL);
	if (!fd->base_para)
		goto kmalloc_fail;

	fd->attr_para = kmalloc(sizeof(*fd->attr_para), GFP_KERNEL);
	if (!fd->attr_para)
		goto kmalloc_fail;

	fd->dma_para = kmalloc(sizeof(*fd->dma_para), GFP_KERNEL);
	if (!fd->dma_para)
		goto kmalloc_fail;

	if (fd->variant->fld_enable) {
		fd->fld_para =
			kmalloc(sizeof(*fd->fld_para), GFP_KERNEL);
		if (!fd->fld_para)
			goto kmalloc_fail;
	}

	fd->base_para->rpn_anchor_thrd =
		(signed short)(user_init->feature_threshold & 0x0000FFFF);
	fd->base_para->pyramid_width = user_init->pyramid_width;
	fd->base_para->pyramid_height = user_init->pyramid_height;
	fd->base_para->max_pyramid_width = user_init->pyramid_width;
	fd->base_para->max_pyramid_height = user_init->pyramid_height;

	fd->base_para->fd_fd_cfg_va = NULL;
	fd->base_para->fd_rs_cfg_va = NULL;
	fd->base_para->fd_yuv2rgb_cfg_va = NULL;
	for (i = 0; i < MAX_ENQUE_FRAME_NUM; i++)
		fd->base_para->attr_fd_cfg_va[i] = NULL;
	for (i = 0; i < MAX_ENQUE_FRAME_NUM; i++)
		fd->base_para->attr_yuv2rgb_cfg_va[i] = NULL;
	for (i = 0; i < PYM_NUM; i++)
		for (j = 0; j < COLOR_NUM; j++)
			fd->base_para->rs_pym_rst_va[i][j] = NULL;

	memset(&fd->st_info, 0, sizeof(struct aie_static_info));
	aie_init_table(fd, fd->base_para->max_pyramid_width,
		       fd->base_para->max_pyramid_height);
	aie_update_buf_params(fd, user_init->max_img_width,
			  user_init->max_img_height);
	ret = aie_alloc_dram_buf(fd);
	if (ret)
		goto free_all;

	ret = aie_alloc_output_buf(fd);
	if (ret)
		goto free_all;

	ret = aie_alloc_fddma_buf(fd);
	if (ret)
		goto free_all;

	if (fd->variant->fld_enable) {
		ret = aie_alloc_fld_buf(fd);
		if (ret)
			goto free_all;
	}

	aie_arrange_fddma_buf(fd);
	aie_arrange_kernel_buf(fd);
	aie_arrange_attrdma_buf(fd);
	aie_arrange_result_dma_buf(fd);

	if (fd->variant->fld_enable)
		aie_arrange_fld_buf(fd);

	ret = aie_load_fw(fd);
	if (ret) {
		dev_err(fd->dev, "Failed to load aie fw\n");
		goto free_all;
	}

	fd->attr_para->r_idx = 0;
	fd->attr_para->w_idx = 0;

	fd->fd_state |= STATE_INIT;

	dev_info(fd->dev, "%s: fd_mem_size(%d)\n", __func__, fd->fd_mem_size);

	return ret;

free_all:
	aie_free_dram_buf(fd);
	aie_free_output_buf(fd);
	aie_free_fddma_buf(fd);
	if (fd->variant->fld_enable)
		aie_free_fld_buf(fd);

kmalloc_fail:
	kfree(fd->base_para);
	kfree(fd->attr_para);
	kfree(fd->dma_para);
	kfree(fd->fld_para);

	dev_err(fd->dev, "Failed to init aie\n");

	return ret;
}

void aie_uninit(struct mtk_aie_dev *fd)
{
	fd->fd_state &= ~STATE_INIT;

	aie_free_dram_buf(fd);
	aie_free_output_buf(fd);
	aie_free_fddma_buf(fd);

	if (fd->variant->fld_enable)
		aie_free_fld_buf(fd);

	kfree(fd->base_para);
	kfree(fd->attr_para);
	kfree(fd->dma_para);
	kfree(fd->fld_para);
}

int aie_prepare(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int ret = -EINVAL;

	if (fd->variant->fld_enable) {
		if (aie_cfg->sel_mode == FLDMODE) { /* FLD don't need to prepare buf */
			dev_dbg(fd->dev, "FLD, Mode: %d", aie_cfg->sel_mode);
			return 0;
		}
	}

	memset(&fd->reg_cfg, 0, sizeof(struct aie_reg_cfg));

	if (aie_cfg->pyramid_base_width == 0) {
		fd->base_para->pyramid_width = fd->base_para->max_pyramid_width;
		fd->base_para->pyramid_height =
			fd->base_para->max_pyramid_height;
		fd->base_para->number_of_pyramid = 3;
	} else {
		if (aie_cfg->pyramid_base_width >
			    fd->base_para->max_pyramid_width ||
		    aie_cfg->pyramid_base_height >
			    fd->base_para->max_pyramid_height ||
		    aie_cfg->number_of_pyramid > 3 ||
		    aie_cfg->number_of_pyramid <= 0) {
			dev_err(fd->dev,
				"err: base w: %d h: %d num: %d\n",
				aie_cfg->pyramid_base_width,
				aie_cfg->pyramid_base_height,
				aie_cfg->number_of_pyramid
			);
			dev_err(fd->dev,
				"err: max w: %d h: %d\n",
				fd->base_para->max_pyramid_width,
				fd->base_para->max_pyramid_height
			);

			return ret;
		}

		fd->base_para->pyramid_height =
			fd->base_para->max_pyramid_height;
		fd->base_para->number_of_pyramid = aie_cfg->number_of_pyramid;
		if (aie_cfg->pyramid_base_width !=
		    fd->base_para->pyramid_width) {
			dev_dbg(fd->dev,
				"pre: %d cur: %d num: %d\n",
				fd->base_para->pyramid_width,
				aie_cfg->pyramid_base_width,
				fd->base_para->number_of_pyramid
			);
			fd->base_para->pyramid_width =
				aie_cfg->pyramid_base_width;
			aie_update_table(fd, fd->base_para->pyramid_width,
					 fd->base_para->pyramid_height);
			aie_update_fddma_buf(fd);
		}
	}

	if (aie_cfg->src_img_width > fd->base_para->max_img_width ||
	    aie_cfg->src_img_height > fd->base_para->max_img_height) {
		dev_err(fd->dev,
			"AIE error: Enque Size error Src_WD: %d Src_HT: %d\n",
			aie_cfg->src_img_width,
			aie_cfg->src_img_height
		);

		dev_err(fd->dev,
			"AIE error: MAX_Src_WD: %d MAX_Src_HT: %d\n",
			fd->base_para->max_img_width,
			fd->base_para->max_img_height
		);
		return ret;
	}

	aie_reset_output_buf(fd, aie_cfg);

	fd->reg_cfg.fd_mode = aie_cfg->sel_mode;
	if (aie_cfg->sel_mode == FDMODE) {
		fd->reg_cfg.rs_adr = (u32)fd->base_para->fd_rs_cfg_pa;
		fd->reg_cfg.yuv2rgb_adr = (u32)fd->base_para->fd_yuv2rgb_cfg_pa;
		fd->reg_cfg.fd_adr = (u32)fd->base_para->fd_fd_cfg_pa +
							 fd->variant->fd_cfg_size * 4 *
							 FD_LOOP_NUM / 3 *
							 (3 - aie_cfg->number_of_pyramid);

	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		fd->reg_cfg.yuv2rgb_adr =
			(u32)fd->base_para->attr_yuv2rgb_cfg_pa[fd->attr_para->w_idx];
		fd->reg_cfg.fd_adr =
			(u32)fd->base_para->attr_fd_cfg_pa[fd->attr_para->w_idx];
	} else {
		dev_err(fd->dev, "AIE error, Mode: %d", aie_cfg->sel_mode);
		return ret;
	}

	ret = aie_update_cfg(fd, aie_cfg);
	if (ret)
		return ret;

	ret = aie_config_dram(fd, aie_cfg);
	if (ret)
		return ret;

	if (aie_cfg->sel_mode == ATTRIBUTEMODE)
		fd->attr_para->w_idx =
			(fd->attr_para->w_idx + 1) % MAX_ENQUE_FRAME_NUM;

	return ret;
}

void aie_execute(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	unsigned int loop_num = 0;
	unsigned int loop_reg_val = 0;
	unsigned int i = 0;

	if (aie_cfg->sel_mode == FDMODE) {
		writel(0x0, fd->fd_base + AIE_START_REG);
		writel(0x00000111, fd->fd_base + AIE_ENABLE_REG);
		loop_num = FD_LOOP_NUM / 3 * (aie_cfg->number_of_pyramid);
		loop_reg_val = (loop_num << 8) |
			       (aie_cfg->number_of_pyramid - 1);
		writel(loop_reg_val, fd->fd_base + AIE_LOOP_REG);
		writel(0x1, fd->fd_base + AIE_INT_EN_REG);
		writel(fd->reg_cfg.rs_adr,
		       fd->fd_base + AIE_RS_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.fd_adr,
		       fd->fd_base + AIE_FD_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.yuv2rgb_adr,
		       fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_REG);

		if (fd->variant->hw_version == 31) {
			writel(0x00000002,
			       fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_MSB);
			writel(0x00000002,
			       fd->fd_base + AIE_RS_CON_BASE_ADR_MSB);
			writel(0x00000002,
			       fd->fd_base + AIE_FD_CON_BASE_ADR_MSB);
		}

		writel(0x1, fd->fd_base + AIE_START_REG);
	} else if (aie_cfg->sel_mode == ATTRIBUTEMODE) {
		writel(0x0, fd->fd_base + AIE_START_REG);
		writel(0x00000101, fd->fd_base + AIE_ENABLE_REG);
		writel(0x00001A00, fd->fd_base + AIE_LOOP_REG);
		writel(0x1, fd->fd_base + AIE_INT_EN_REG);
		writel(fd->reg_cfg.rs_adr,
		       fd->fd_base + AIE_RS_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.fd_adr,
		       fd->fd_base + AIE_FD_CON_BASE_ADR_REG);
		writel(fd->reg_cfg.yuv2rgb_adr,
		       fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_REG);

		if (fd->variant->hw_version == 31) {
			writel(0x00000002,
			       fd->fd_base + AIE_YUV2RGB_CON_BASE_ADR_MSB);
			writel(0x00000002,
			       fd->fd_base + AIE_RS_CON_BASE_ADR_MSB);
			writel(0x00000002,
			       fd->fd_base + AIE_FD_CON_BASE_ADR_MSB);
		}

		writel(0x1, fd->fd_base + AIE_START_REG);
	} else if (aie_cfg->sel_mode == FLDMODE) {
		if (fd->variant->fld_enable) {
			writel(0x10, fd->fd_base + AIE_START_REG);
			writel(0x00011111, fd->fd_base + AIE_DMA_CTL_REG);
			writel(0x01111111, fd->fd_base + FLD_EN);
			writel(0x1, fd->fd_base + AIE_INT_EN_REG);
			for (i = 0; i < aie_cfg->fld_face_num; i++) {
				writel(aie_cfg->src_img_addr,
				       fd->fd_base + FLD_BASE_ADDR_FACE_0 +
					       i * 0x4);
				writel(aie_cfg->fld_input[i].fld_in_crop_x1
						       << 16 |
					       aie_cfg->fld_input[i]
						       .fld_in_crop_y1,
				       fd->fd_base + fld_face_info_0[i]);
				writel(aie_cfg->fld_input[i].fld_in_crop_x2
						       << 16 |
					       aie_cfg->fld_input[i]
						       .fld_in_crop_y2,
				       fd->fd_base + fld_face_info_1[i]);
				writel(aie_cfg->fld_input[i].fld_in_rip << 4 |
					       aie_cfg->fld_input[i].fld_in_rop,
				       fd->fd_base + fld_face_info_2[i]);
			}

			writel(aie_cfg->fld_face_num << 28 | FLD_FOREST << 16 |
				       FLD_POINT,
			       fd->fd_base + FLD_MODEL_PARA1);
			writel(13 << 16 | 0xfe9,
			       fd->fd_base + FLD_MODEL_PARA14);

			writel(aie_cfg->src_img_width << 16 |
				       aie_cfg->src_img_height,
			       fd->fd_base + FLD_SRC_WD_HT);

			/*input settings*/
			writel(0x007c003f, fd->fd_base + FLD_PL_IN_SIZE_0);
			writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_0);
			writel(0x007c003f, fd->fd_base + FLD_PL_IN_SIZE_1);
			writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_1);
			writel(0x0016003f, fd->fd_base + FLD_PL_IN_SIZE_2_0);
			writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_0);
			writel(0x0013003f, fd->fd_base + FLD_PL_IN_SIZE_2_1);
			writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_1);
			writel(0x0013003f, fd->fd_base + FLD_PL_IN_SIZE_2_2);
			writel(0x0040000f, fd->fd_base + FLD_PL_IN_STRIDE_2_2);
			writel(0x00a6001f, fd->fd_base + FLD_PL_IN_SIZE_3);
			writel(0x0020000f, fd->fd_base + FLD_PL_IN_STRIDE_3);

			/*output setting*/
			writel((2400 * aie_cfg->fld_face_num - 1) << 16 | 127,
			       fd->fd_base + FLD_SH_IN_SIZE_0);
			writel(0x0010000f, fd->fd_base + FLD_SH_IN_STRIDE_0);
			writel(fd->fld_para->fld_output_pa[0],
			       fd->fd_base + FLD_TR_OUT_BASE_ADDR_0);
			writel((aie_cfg->fld_face_num - 1) << 16 | 0x6f,
			       fd->fd_base + FLD_TR_OUT_SIZE_0);
			writel(0x0070000f, fd->fd_base + FLD_TR_OUT_STRIDE_0);
			writel(fd->fld_para->fld_output_pa[0],
			       fd->fd_base + FLD_PP_OUT_BASE_ADDR_0);
			writel((aie_cfg->fld_face_num - 1) << 16 | 0x6f,
			       fd->fd_base + FLD_PP_OUT_SIZE_0);
			writel(0x0070000f, fd->fd_base + FLD_PP_OUT_STRIDE_0);

			/*cv score*/
			writel(0x00000001, fd->fd_base + FLD_BS_BIAS);
			writel(0x0000b835,
			       fd->fd_base + FLD_CV_FM_RANGE_0); // 8E8
			writel(0xffff5cba,
			       fd->fd_base + FLD_CV_FM_RANGE_1); // 8EC
			writel(0x00005ed5,
			       fd->fd_base + FLD_CV_PM_RANGE_0); // 8F0
			writel(0xffff910d,
			       fd->fd_base + FLD_CV_PM_RANGE_1); // 8F4
			writel(0x0000031e, fd->fd_base + FLD_BS_RANGE_0); // 8F8
			writel(0xfffffcae, fd->fd_base + FLD_BS_RANGE_1); // 8FC

			/* 6 steps */
			writel(fd->fld_para->fld_step_pa[FLD_STEP_BLINK][14],
			       fd->fd_base + FLD_BS_IN_BASE_ADDR_14);

			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][0],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_0);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][1],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_1);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][2],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_2);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][3],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_3);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][4],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_4);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][5],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_5);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][6],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_6);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][7],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_7);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][8],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_8);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][9],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_9);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][10],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_10);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][11],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_11);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][12],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_12);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][13],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_13);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_CV][14],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_14);

			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][0],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_0);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][1],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_1);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][2],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_2);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][3],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_3);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][4],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_4);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][5],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_5);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][6],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_6);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][7],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_7);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][8],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_8);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][9],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_9);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][10],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_10);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][11],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_11);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][12],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_12);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][13],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_13);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_FP][14],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_14);

			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][0],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_0);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][1],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_1);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][2],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_2);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][3],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_3);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][4],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_4);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][5],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_5);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][6],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_6);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][7],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_7);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][8],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_8);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][9],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_9);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][10],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_10);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][11],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_11);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][12],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_12);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][13],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_13);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_LEAF][14],
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_14);

			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][0],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_0);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][1],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_1);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][2],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_2);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][3],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_3);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][4],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_4);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][5],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_5);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][6],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_6);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][7],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_7);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][8],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_8);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][9],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_9);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][10],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_10);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][11],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_11);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][12],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_12);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][13],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_13);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM02][14],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_14);

			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][0],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_0);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][1],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_1);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][2],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_2);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][3],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_3);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][4],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_4);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][5],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_5);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][6],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_6);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][7],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_7);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][8],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_8);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][9],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_9);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][10],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_10);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][11],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_11);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][12],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_12);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][13],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_13);
			writel(fd->fld_para->fld_step_pa[FLD_STEP_KM13][14],
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_14);

			/* */
			writel(0x22222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_0_8_15_MSB);

			writel(0x22222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_1_8_15_MSB);

			writel(0x22222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_2_8_15_MSB);

			writel(0x22222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_PL_IN_BASE_ADDR_3_8_15_MSB);

			writel(0x22222222,
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_SH_IN_BASE_ADDR_8_15_MSB);

			writel(0x02000000,
			       fd->fd_base + FLD_BS_IN_BASE_ADDR_8_15_MSB);

			writel(0x22222222,
			       fd->fd_base + FLD_BASE_ADDR_FACE_0_7_MSB);
			writel(0x02222222,
			       fd->fd_base + FLD_BASE_ADDR_FACE_8_14_MSB);
			writel(0x00000002,
			       fd->fd_base + FLD_TR_OUT_BASE_ADDR_0_MSB);
			writel(0x00000002,
			       fd->fd_base + FLD_PP_OUT_BASE_ADDR_0_MSB);

			/*fld mode + trigger start*/
			writel(0x11, fd->fd_base + AIE_START_REG);
		}
	}
}

void aie_irqhandle(struct mtk_aie_dev *fd)
{
	writel(0x0, fd->fd_base + AIE_START_REG);

	/* interrupt read clear */
	readl(fd->fd_base + AIE_INT_REG);
}

static u16 aie_get_hi16(unsigned int value)
{
	return (value & 0xFFFF0000) >> 16;
}

static u16 aie_get_lo16(unsigned int value)
{
	return value & 0xFFFF;
}

static signed short aie_refine_s16_value(signed short value)
{
	s16 result = 0;

	if ((value & 0x200) >> 9)
		result = (value | 0xFE00);
	else
		result = value;

	return result;
}

/* return aie_cfg to user space */
void aie_get_fd_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	void *fd_pym_result[PYM_NUM] = { NULL, NULL, NULL };
	unsigned int *pto12 = NULL;
	unsigned int i = 0, j = 0;
	struct FDRESULT *prst = NULL;
	signed short landmark = 0;
	struct aie_enq_info *tmp_aie_cfg = NULL;
	u32 fd_result_hw = 0, fd_result_1_hw = 0;
	u32 fd_total_num = 0;
	u32 fd_pyramid_num[PYM_NUM] = { 0, 0, 0 };

	aie_cfg->sel_mode = fd->base_para->sel_mode;
	aie_cfg->rotate_degree = fd->base_para->rotate_degree;
	aie_cfg->src_img_addr = fd->base_para->src_img_addr;
	aie_cfg->src_img_addr_uv = fd->base_para->src_img_addr_uv;
	aie_cfg->src_img_width = fd->base_para->img_width;
	aie_cfg->src_img_height = fd->base_para->img_height;
	aie_cfg->src_img_fmt = fd->base_para->src_img_fmt;
	aie_cfg->fd_version = FD_VERSION;
	aie_cfg->attr_version = ATTR_VERSION;

	aie_cfg->irq_status = readl(fd->fd_base + AIE_INT_EN_REG);

	fd_result_hw = fd->reg_cfg.hw_result;
	fd_result_1_hw = fd->reg_cfg.hw_result1;
	fd_total_num = fd_result_hw & 0xFFF;
	fd_pyramid_num[0] = (fd_result_hw & 0xFFF0000) >> 16;
	fd_pyramid_num[1] = fd_result_1_hw & 0xFFF;
	fd_pyramid_num[2] = (fd_result_1_hw & 0xFFF0000) >> 16;

	if (fd_total_num == 0)
		goto nothing_out;

	tmp_aie_cfg =  aie_cfg;

	tmp_aie_cfg->fd_out.fd_total_num = fd_total_num;
	tmp_aie_cfg->fd_out.fd_pyramid0_num = fd_pyramid_num[0];
	tmp_aie_cfg->fd_out.fd_pyramid1_num = fd_pyramid_num[1];
	tmp_aie_cfg->fd_out.fd_pyramid2_num = fd_pyramid_num[2];

	switch (tmp_aie_cfg->number_of_pyramid) {
	case 1:
		fd_pym_result[2] = fd->dma_para->fd_out_hw_va[RPN0_LOOP_NUM][0];
		break;
	case 2:
		fd_pym_result[1] = fd->dma_para->fd_out_hw_va[RPN0_LOOP_NUM][0];
		fd_pym_result[2] = fd->dma_para->fd_out_hw_va[RPN1_LOOP_NUM][0];
		break;
	case 3:
		fd_pym_result[0] = fd->dma_para->fd_out_hw_va[RPN0_LOOP_NUM][0];
		fd_pym_result[1] = fd->dma_para->fd_out_hw_va[RPN1_LOOP_NUM][0];
		fd_pym_result[2] = fd->dma_para->fd_out_hw_va[RPN2_LOOP_NUM][0];
		break;
	default:
		dev_err(fd->dev, "Wrong number_of_pyramid\n");
		goto nothing_out;
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < fd_pyramid_num[i]; j++) {
			if (i == 0)
				prst = &tmp_aie_cfg->fd_out.PYRAMID0_RESULT;
			else if (i == 1)
				prst = &tmp_aie_cfg->fd_out.PYRAMID1_RESULT;
			else if (i == 2)
				prst = &tmp_aie_cfg->fd_out.PYRAMID2_RESULT;

			pto12 = (unsigned int *)fd_pym_result[i] + 12 * j;

			prst->anchor_x0[j] = aie_get_lo16(*(pto12 + 0));
			prst->anchor_y0[j] = aie_get_hi16(*(pto12 + 0));
			prst->anchor_x1[j] = aie_get_lo16(*(pto12 + 1));
			prst->anchor_y1[j] = aie_get_hi16(*(pto12 + 1));

			if (prst->anchor_x1[j] == 0 ||
			    prst->anchor_y1[j] == 0) {
				dev_err(fd->dev,
					"wrong coordinate: i=%d j=%d M:%d %d %d %d\n",
					i,
					j,
					prst->anchor_x0[j],
					prst->anchor_x1[j],
					prst->anchor_y0[j],
					prst->anchor_y1[j]
				);
				goto nothing_out;
			}

			/* ROP result at 1st run */
			landmark = (*(pto12 + 2) & 0x3FF);
			prst->rop_landmark_score0[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 2) & 0xFFC00) >> 10);
			prst->rop_landmark_score1[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 2) & 0x3FF00000) >> 20);
			prst->rop_landmark_score2[j] =
				aie_refine_s16_value(landmark);

			prst->anchor_score[j] =
				aie_refine_s16_value(*(pto12 + 9) & 0x3FF);

			/* RIP result at 1st run */
			landmark = ((*(pto12 + 9) & 0xFFC00) >> 10);
			prst->rip_landmark_score0[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 9) & 0x3FF00000) >> 20);
			prst->rip_landmark_score1[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 9) & 0xC0000000) >> 30) |
				   ((*(pto12 + 10) & 0xFF) << 2);
			prst->rip_landmark_score2[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 10) & 0x3FF00) >> 8);
			prst->rip_landmark_score3[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 10) & 0xFFC0000) >> 18);
			prst->rip_landmark_score4[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 10) & 0xF0000000) >> 28) |
				   ((*(pto12 + 11) & 0x3F) << 4);
			prst->rip_landmark_score5[j] =
				aie_refine_s16_value(landmark);
			landmark = ((*(pto12 + 11) & 0xFFC0) >> 6);
			prst->rip_landmark_score6[j] =
				aie_refine_s16_value(landmark);
			prst->face_result_index[j] =
				((*(pto12 + 11) & 0xFFF0000) >> 16);
			prst->anchor_index[j] =
				((*(pto12 + 11) & 0x70000000) >> 28);

			prst->fd_partial_result = fd_pyramid_num[i];
		}
	}
	return;
nothing_out:
	// Ensure that user mode does not receive an inappropriate result structure
	memset(&aie_cfg->fd_out, 0, sizeof(struct fd_result));
}

void aie_get_attr_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	u32 *attr_race_result = NULL, *attr_gender_result = NULL;
	u32 *attr_age_result = NULL, *attr_is_indian_result = NULL;

	aie_cfg->sel_mode = fd->attr_para->sel_mode[fd->attr_para->r_idx];
	aie_cfg->rotate_degree =
		fd->attr_para->rotate_degree[fd->attr_para->r_idx];
	aie_cfg->src_img_addr =
		fd->attr_para->src_img_addr[fd->attr_para->r_idx];
	aie_cfg->src_img_addr_uv =
		fd->attr_para->src_img_addr_uv[fd->attr_para->r_idx];
	aie_cfg->src_img_width = fd->attr_para->img_width[fd->attr_para->r_idx];
	aie_cfg->src_img_height =
		fd->attr_para->img_height[fd->attr_para->r_idx];
	aie_cfg->src_img_fmt = fd->attr_para->src_img_fmt[fd->attr_para->r_idx];
	aie_cfg->fd_version = FD_VERSION;
	aie_cfg->attr_version = ATTR_VERSION;

	aie_cfg->irq_status = readl(fd->fd_base + AIE_INT_EN_REG);

	/* 64 feature * 32 bytes */
	attr_age_result =
		(u32 *)fd->dma_para->age_out_hw_va[fd->attr_para->r_idx];
	attr_gender_result =
		(u32 *)fd->dma_para->gender_out_hw_va[fd->attr_para->r_idx];
	attr_is_indian_result =
		(u32 *)fd->dma_para->is_indian_out_hw_va[fd->attr_para->r_idx];
	attr_race_result =
		(u32 *)fd->dma_para->race_out_hw_va[fd->attr_para->r_idx];

	aie_cfg->attr_out.MERGED_AGE_RESULT.RESULT[0] =
		aie_get_lo16(*attr_age_result);
	aie_cfg->attr_out.MERGED_AGE_RESULT.RESULT[1] =
		aie_get_hi16(*attr_age_result);

	aie_cfg->attr_out.MERGED_GENDER_RESULT.RESULT[0] =
		aie_get_lo16(*attr_gender_result);
	aie_cfg->attr_out.MERGED_GENDER_RESULT.RESULT[1] =
		aie_get_hi16(*attr_gender_result);

	aie_cfg->attr_out.MERGED_IS_INDIAN_RESULT.RESULT[0] =
		aie_get_lo16(*attr_is_indian_result);
	aie_cfg->attr_out.MERGED_IS_INDIAN_RESULT.RESULT[1] =
		aie_get_hi16(*attr_is_indian_result);

	aie_cfg->attr_out.MERGED_RACE_RESULT.RESULT[0] =
		aie_get_lo16(*attr_race_result);
	aie_cfg->attr_out.MERGED_RACE_RESULT.RESULT[1] =
		aie_get_hi16(*attr_race_result);
	aie_cfg->attr_out.MERGED_RACE_RESULT.RESULT[2] =
		aie_get_lo16(*(attr_race_result + 1));

	fd->attr_para->r_idx = (fd->attr_para->r_idx + 1) % MAX_ENQUE_FRAME_NUM;
}

void aie_get_fld_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg)
{
	int i = 0, j = 0;
	u16 *out_parsing = NULL;
	u8 fld_rlt[FLD_MAX_FRAME][FLD_OUTPUT_SIZE];

	aie_cfg->irq_status = readl(fd->fd_base + AIE_INT_EN_REG);

	memcpy(fld_rlt, fd->fld_para->fld_output_va[0], sizeof(fld_rlt));

	for (j = 0; j < aie_cfg->fld_face_num; j++) {
		out_parsing = (unsigned short *)&fld_rlt[j][0];
		for (i = 0; i < FLD_CUR_LANDMARK; i++) {
			aie_cfg->fld_out[j].fld_landmark[i].x = *out_parsing;
			aie_cfg->fld_out[j].fld_landmark[i].y =
				*(out_parsing + 1);

			if (i % 2)
				out_parsing = out_parsing + 6;
			else
				out_parsing = out_parsing + 2;
		}
		out_parsing = (unsigned short *)&fld_rlt[j][0];
		if (FLD_CUR_LANDMARK % 2)
			out_parsing =
				out_parsing + ((FLD_CUR_LANDMARK + 1) / 2) * 8;
		else
			out_parsing = out_parsing + (FLD_CUR_LANDMARK / 2) * 8;

		aie_cfg->fld_out[j].fld_out_rop = *out_parsing;
		aie_cfg->fld_out[j].fld_out_rip = *(out_parsing + 1);
		aie_cfg->fld_out[j].confidence = *(out_parsing + 2);
		aie_cfg->fld_out[j].blinkscore = *(out_parsing + 3);
	}
}
