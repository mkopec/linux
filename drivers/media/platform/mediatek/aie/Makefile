# SPDX-License-Identifier: GPL-2.0

ifneq ($(filter y m,$(CONFIG_INTERCONNECT_MTK_EXTENSION)),)
ifeq ($(DEVICE_MODULES_PATH), )
ccflags-$(CONFIG_VIDEO_MTK_AIE) += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
else
ccflags-$(CONFIG_VIDEO_MTK_AIE) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
endif
endif

ifneq ($(filter y m,$(CONFIG_REGULATOR_MTK_VMM)),)
ifeq ($(DEVICE_MODULES_PATH), )
ccflags-$(CONFIG_VIDEO_MTK_AIE) += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-vmm/
else
ccflags-$(CONFIG_VIDEO_MTK_AIE) += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/mtk-vmm/
endif
endif

mtk-aie-$(CONFIG_VIDEO_MTK_AIE) += mtk_aie_53.o
mtk-aie-$(CONFIG_VIDEO_MTK_AIE) += mtk_aie_drv.o

obj-$(CONFIG_VIDEO_MTK_AIE) += mtk-aie.o
