/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2022 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_IF_H__
#define __MTK_CAM_SENINF_IF_H__

int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int *pixelmode);

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int pixelmode);

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg);

int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *pixelrate);

int mtk_cam_seninf_calc_pixelrate(struct device *dev, s64 width, s64 height, s64 hblank,
				  s64 vblank, int fps_n, int fps_d, s64 sensor_pixel_rate);

int mtk_cam_seninf_dump(struct v4l2_subdev *sd);

unsigned int
mtk_cam_seninf_get_vc_feature(struct v4l2_subdev *sd, unsigned int pad);

void
mtk_cam_seninf_set_secure(struct v4l2_subdev *sd, int enable, unsigned int sec_info_addr);
#endif
