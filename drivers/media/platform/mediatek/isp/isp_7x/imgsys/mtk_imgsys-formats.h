/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Mediatek ISP imgsys formats
 *
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_IMGSYS_FORMATS_H_
#define _MTK_IMGSYS_FORMATS_H_

struct mtk_imgsys_format {
	u32 format;
	u8 num_planes;
	u32 buffer_size;
	u32 pixels_per_group;
	u32 bytes_per_group[3];
	u32 vertical_sub_sampling[3];
};

extern const struct mtk_imgsys_format mtk_imgsys_format_nv12;

extern const struct mtk_imgsys_format mtk_imgsys_format_nv21;

extern const struct mtk_imgsys_format mtk_imgsys_format_grey;

extern const struct mtk_imgsys_format mtk_imgsys_format_2p012p;

extern const struct mtk_imgsys_format mtk_imgsys_format_2p010p;

extern const struct mtk_imgsys_format mtk_imgsys_format_y8;

extern const struct mtk_imgsys_format mtk_imgsys_format_y16;

extern const struct mtk_imgsys_format mtk_imgsys_format_y32;

extern const struct mtk_imgsys_format mtk_imgsys_format_warp2p;

extern const struct mtk_imgsys_format mtk_imgsys_format_sgbrg10;

extern const struct mtk_imgsys_format mtk_imgsys_format_srggb10;

extern const struct mtk_imgsys_format mtk_imgsys_format_sgrbg10;

extern const struct mtk_imgsys_format mtk_imgsys_format_metai;

extern const struct mtk_imgsys_format mtk_imgsys_format_stt;

extern const struct mtk_imgsys_format mtk_imgsys_format_ctrlmeta;

#endif /* _MTK_IMGSYS_FORMATS_H_ */
