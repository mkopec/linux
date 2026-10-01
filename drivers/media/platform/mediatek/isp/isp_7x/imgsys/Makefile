# SPDX-License-Identifier: GPL-2.0
#

mtk_imgsys_util-objs := \
mtk_imgsys-debug.o \
mtk_imgsys-dev.o \
mtk_imgsys-sys.o \
mtk_imgsys-v4l2.o \
mtk_imgsys-cmdq.o \
mtk_imgsys-of.o \
mtk_imgsys-formats.o \
mtk_imgsys-scp.o

mtk_imgsys_hw_isp-objs := \
modules/mtk_imgsys-dip.o \
modules/mtk_imgsys-traw.o \
modules/mtk_imgsys-pqdip.o \
modules/mtk_imgsys-wpe.o \
modules/mtk_imgsys-adl.o \
modules/mtk_imgsys-me.o

obj-$(CONFIG_VIDEO_MTK_ISP_71_IMGSYS) += mtk_imgsys_util.o
obj-$(CONFIG_VIDEO_MTK_ISP_71_IMGSYS) += mtk_imgsys_hw_isp.o
