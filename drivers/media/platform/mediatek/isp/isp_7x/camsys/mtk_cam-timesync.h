/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __ARCHCOUNTER_TIMESYNC__
#define __ARCHCOUNTER_TIMESYNC__

u64 mtk_cam_timesync_to_monotonic(u64 hwclock);
u64 mtk_cam_timesync_to_boot(u64 hwclock);

#endif
