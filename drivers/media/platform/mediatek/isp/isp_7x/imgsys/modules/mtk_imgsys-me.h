/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_ME_H_
#define _MTK_IMGSYS_ME_H_

#include "../mtk_imgsys-dev.h"

/**
 * imgsys_me_set_initial_value - Set the initial values for the motion engine device.
 * @imgsys_dev: Pointer to the mtk_imgsys_dev structure representing the ISP Pass 2
 * image processing device.
 *
 * This function initializes the necessary values for the motion engine device to
 * ensure it is ready for operation. It should be called during the initialization
 * phase of the ISP Pass 2 image processing device.
 */
void imgsys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);

/**
 * imgsys_me_uninit - Uninitialize the motion engine subsystem.
 * @imgsys_dev: Pointer to the mtk_imgsys_dev structure representing the ISP Pass 2
 * image processing device.
 *
 * This function performs cleanup and deinitialization of the motion engine device.
 */
void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev);

/**
 * imgsys_me_debug_dump - Dump debug information for the motion engine.
 * @imgsys_dev: Pointer to the mtk_imgsys_dev structure representing the ISP Pass 2
 * image processing device.
 * @engine    : The ID of the motion engine to dump debug information for.
 *
 * This function retrieves and prints debug information related to the motion engine
 * device. It is useful for diagnosing issues and understanding the internal state
 * of the engine.
 */
void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine);

/**
 * imgsys_me_ndd_dump - Dump the frame information for debugging.
 * @imgsys_dev: Pointer to the mtk_imgsys_dev structure representing the ISP Pass 2
 * image processing device.
 * @frm_dump_info: Pointer to the imgsys_ndd_frm_dump_info structure containing
 * frame dump information.
 *
 * This function dumps the NDD (Normal Data Dump) frame information for debugging
 * purposes. It provides insights into the frame data processed by the ISP Pass 2
 * image processing device, which can help in identifying issues related to frame
 * handling.
 */
void imgsys_me_ndd_dump(struct mtk_imgsys_dev *imgsys_dev,
			struct imgsys_ndd_frm_dump_info *frm_dump_info);

#endif /* _MTK_IMGSYS_ME_H_ */
