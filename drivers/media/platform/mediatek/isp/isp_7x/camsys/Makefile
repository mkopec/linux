# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 MediaTek Inc.

mtk-cam-isp-objs := mtk_cam.o mtk_cam-raw.o mtk_cam-pool.o \
		    mtk_cam-video.o mtk_cam-ctrl.o \
		    mtk_cam-seninf-route.o mtk_cam-seninf-drv.o \
		    mtk_cam-debug.o \
		    mtk_cam-raw_debug.o \
		    mtk_cam-feature.o mtk_cam-timesync.o

mtk-cam-plat-util-objs :=  mtk_cam-plat-util.o

include $(src)/mtk_csi_phy_2_0/Makefile

obj-$(CONFIG_VIDEO_MTK_ISP_71_CAMSYS) += mtk-cam-plat-util.o
obj-$(CONFIG_VIDEO_MTK_ISP_71_CAMSYS) += mtk-cam-isp.o
ccflags-y += -DISP7_1
