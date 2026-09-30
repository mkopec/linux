/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_CAM_META_H__
#define __MTK_CAM_META_H__

/*
 * struct mtk_cam_uapi_meta_rect - rect info
 *
 * @left: The X coordinate of the left side of the rectangle
 * @top:  The Y coordinate of the left side of the rectangle
 * @width:  The width of the rectangle
 * @height: The height of the rectangle
 *
 * rect containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_rect {
	s32 left;
	s32 top;
	u32 width;
	u32 height;
} __packed;

/*
 * struct mtk_cam_uapi_meta_size - size info
 *
 * @width:  The width of the size
 * @height: The height of the size
 *
 * size containing the width and height fields.
 *
 */
struct mtk_cam_uapi_meta_size {
	u32 width;
	u32 height;
} __packed;

/*
 *  A U T O  E X P O S U R E
 */

/*
 *  struct mtk_cam_uapi_ae_hist_cfg - histogram info for AE
 *
 *  @hist_en:    enable bit for current histogram, each histogram can
 *      be 0/1 (disabled/enabled) separately
 *  @hist_opt:   color mode config for current histogram (0/1/2/3/4:
 *      R/G/B/RGB mix/Y)
 *  @hist_bin:   bin mode config for current histogram (1/4: 256/1024 bin)
 *  @hist_y_hi:  ROI Y range high bound for current histogram
 *  @hist_y_low: ROI Y range low bound for current histogram
 *  @hist_x_hi:  ROI X range high bound for current histogram
 *  @hist_x_low: ROI X range low bound for current histogram
 */
struct mtk_cam_uapi_ae_hist_cfg {
	s32 hist_en;
	u8 hist_opt;
	u8 hist_bin;
	u16 hist_y_hi;
	u16 hist_y_low;
	u16 hist_x_hi;
	u16 hist_x_low;
	u16 rsv;
} __packed;

#define MTK_CAM_UAPI_ROI_MAP_BLK_NUM (128 * 128)
/*
 *  struct mtk_cam_uapi_ae_param - parameters for AE configurtion
 *
 *  @pixel_hist_win_cfg_le: window config for le histogram 0~5
 *           separately, uAEHistBin shold be the same
 *           for these 6 histograms
 *  @pixel_hist_win_cfg_se: window config for se histogram 0~5
 *           separately, uAEHistBin shold be the same
 *           for these 6 histograms
 *  @roi_hist_cfg_le : config for roi le histogram 0~3
 *           color mode/enable
 *  @roi_hist_cfg_se : config for roi se histogram 0~3
 *           color mode/enable
 *  @hdr_ratio: in HDR scenario, AE calculated hdr ratio
 *           (LE exp*iso/SE exp*iso*100) for current frame,
 *           default non-HDR scenario ratio=1000
 */
struct mtk_cam_uapi_ae_param {
	struct mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_le[6];
	struct mtk_cam_uapi_ae_hist_cfg pixel_hist_win_cfg_se[6];
	struct mtk_cam_uapi_ae_hist_cfg roi_hist_cfg_le[4];
	struct mtk_cam_uapi_ae_hist_cfg roi_hist_cfg_se[4];
	u8  aai_r1_enable;
	u8  aai_roi_map[MTK_CAM_UAPI_ROI_MAP_BLK_NUM];
	u8  rsv;
	u16 hdr_ratio; /* base 1 x= 1000 */
	u32 act_win_x_start;
	u32 act_win_x_end;
	u32 act_win_y_start;
	u32 act_win_y_end;
} __packed;

/*
 *  A U T O  W H I T E  B A L A N C E
 */

/* Maximum blocks that Mediatek AWB supports */
#define MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM (10)

/*
 *  struct mtk_cam_uapi_awb_param - parameters for AWB configurtion
 *
 *  @stat_en:                  AWB stat enable
 *  @windownum_x:              Number of horizontal AWB windows
 *  @windownum_y:              Number of vertical AWB windows
 *  @lowthreshold_r:           Low threshold of R
 *  @lowthreshold_g:           Low threshold of G
 *  @lowthreshold_b:           Low threshold of B
 *  @highthreshold_r:          High threshold of R
 *  @highthreshold_g:          High threshold of G
 *  @highthreshold_b:          High threshold of B
 *  @lightsrc_lowthreshold_r:  Low threshold of R for light source estimation
 *  @lightsrc_lowthreshold_g:  Low threshold of G for light source estimation
 *  @lightsrc_lowthreshold_b:  Low threshold of B for light source estimation
 *  @lightsrc_highthreshold_r: High threshold of R for light source estimation
 *  @lightsrc_highthreshold_g: High threshold of G for light source estimation
 *  @lightsrc_highthreshold_b: High threshold of B for light source estimation
 *  @pregainlimit_r:           Maximum limit clipping for R color
 *  @pregainlimit_g:           Maximum limit clipping for G color
 *  @pregainlimit_b:           Maximum limit clipping for B color
 *  @pregain_r:                unit module compensation gain for R color
 *  @pregain_g:                unit module compensation gain for G color
 *  @pregain_b:                unit module compensation gain for B color
 *  @valid_datawidth:          valid bits of statistic data
 *  @hdr_support_en:           support HDR mode
 *  @stat_mode:                Output format select <1>sum mode <0>average mode
 *  @error_ratio:              Programmable error pixel count by AWB window size
 *              (base : 256)
 *  @awbxv_win_r:              light area of right bound, the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_l:              light area of left bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_d:              light area of lower bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @awbxv_win_u:              light area of upper bound the size is defined in
 *              MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM
 *  @pregain2_r:               white balance gain of R color
 *  @pregain2_g:               white balance gain of G color
 *  @pregain2_b:               white balance gain of B color
 */
struct mtk_cam_uapi_awb_param {
	u32 stat_en;
	u32 windownum_x;
	u32 windownum_y;
	u32 lowthreshold_r;
	u32 lowthreshold_g;
	u32 lowthreshold_b;
	u32 highthreshold_r;
	u32 highthreshold_g;
	u32 highthreshold_b;
	u32 lightsrc_lowthreshold_r;
	u32 lightsrc_lowthreshold_g;
	u32 lightsrc_lowthreshold_b;
	u32 lightsrc_highthreshold_r;
	u32 lightsrc_highthreshold_g;
	u32 lightsrc_highthreshold_b;
	u32 pregainlimit_r;
	u32 pregainlimit_g;
	u32 pregainlimit_b;
	u32 pregain_r;
	u32 pregain_g;
	u32 pregain_b;
	u32 valid_datawidth;
	u32 hdr_support_en;
	u32 stat_mode;
	u32 format_shift;
	u32 error_ratio;
	u32 postgain_r;
	u32 postgain_g;
	u32 postgain_b;
	u32 postgain2_hi_r;
	u32 postgain2_hi_g;
	u32 postgain2_hi_b;
	u32 postgain2_med_r;
	u32 postgain2_med_g;
	u32 postgain2_med_b;
	u32 postgain2_low_r;
	u32 postgain2_low_g;
	u32 postgain2_low_b;
	s32 awbxv_win_r[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	s32 awbxv_win_l[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	s32 awbxv_win_d[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	s32 awbxv_win_u[MTK_CAM_UAPI_AWB_MAX_LIGHT_AREA_NUM];
	u32 csc_ccm[9];
	u32 acc;
	u32 med_region[4];
	u32 low_region[4];
	u32 pregain2_r;
	u32 pregain2_g;
	u32 pregain2_b;
} __packed;

/*
 * struct mtk_cam_uapi_dgn_param
 *
 *  @gain: digital gain to increase image brightness, 1 x= 1024
 */
struct mtk_cam_uapi_dgn_param {
	u32 gain;
} __packed;

/*
 * struct mtk_cam_uapi_wb_param
 *
 *  @gain_r: white balance gain of R channel
 *  @gain_g: white balance gain of G channel
 *  @gain_b: white balance gain of B channel
 */
struct mtk_cam_uapi_wb_param {
	u32 gain_r;
	u32 gain_g;
	u32 gain_b;
	u32 clip;
} __packed;

/*
 *  A U T O  F O C U S
 */

/*
 * struct mtk_cam_uapi_af_param - af statistic parameters
 *  @roi: AF roi rectangle (in pixel) for AF statistic covered, including
 *    x, y, width, height
 *  @th_sat_g:  green channel pixel value saturation threshold (0~255)
 *  @th_h[3]: horizontal AF filters response threshold (0~50) for H0, H1,
 *    and H2
 *  @th_v:  vertical AF filter response threshold (0~50)
 *  @blk_pixel_xnum: horizontal number of pixel per block
 *  @blk_pixel_ynum: vertical number of pixel per block
 *  @fir_type: to select FIR filter by AF target type (0,1,2,3)
 *  @iir_type: to select IIR filter by AF target type (0,1,2,3)
 *  @data_gain[7]: gamma curve gain for AF source data
 */
struct mtk_cam_uapi_af_param {
	struct mtk_cam_uapi_meta_rect roi;
	u32 th_sat_g;
	u32 th_h[3];
	u32 th_v;
	u32 blk_pixel_xnum;
	u32 blk_pixel_ynum;
	u32 fir_type;
	u32 iir_type;
	u32 data_gain[7];
} __packed;

enum mtk_cam_uapi_flk_hdr_path_control {
	MTKCAM_UAPI_FKLO_HDR_1ST_FRAME = 0,
	MTKCAM_UAPI_FKLO_HDR_2ND_FRAME,
	MTKCAM_UAPI_FKLO_HDR_3RD_FRAME,
} __packed;

/*
 *  struct mtk_cam_uapi_flk_param
 *
 *  @input_bit_sel: maximum pixel value of flicker statistic input
 *  @offset_y: initial position for flicker statistic calculation in y direction
 *  @crop_y: number of rows which will be cropped from bottom
 *  @sgg_val[8]: Simple Gain and Gamma for noise reduction, sgg_val[0] is
 *               gain and sgg_val[1] - sgg_val[7] are gamma table
 *  @noise_thr: the noise threshold of pixel value, pixel value lower than
 *              this value is considered as noise
 *  @saturate_thr: the saturation threshold of pixel value, pixel value
 *                 higher than this value is considered as saturated
 *  @hdr_flk_src: flk source tap point selection
 */
struct mtk_cam_uapi_flk_param {
	u32 input_bit_sel;
	u32 offset_y;
	u32 crop_y;
	u32 sgg_val[8];
	u32 noise_thr;
	u32 saturate_thr;
	u32 hdr_flk_src;
} __packed;

/*
 * struct mtk_cam_uapi_tsf_param
 *
 *  @horizontal_num: block number of horizontal direction
 *  @vertical_num:   block number of vertical direction
 */
struct mtk_cam_uapi_tsf_param {
	u32 horizontal_num;
	u32 vertical_num;
} __packed;

/*
 * struct mtk_cam_uapi_pde_param
 *
 * @pdi_max_size: the max required memory size for pd table
 * @pdo_max_size: the max required memory size for pd point output
 * @pdo_x_size: the pd points out x size
 * @pdo_y_size: the pd points out y size
 * @pd_table_offset: the offset of pd table in the meta_cfg
 */
struct mtk_cam_uapi_pde_param {
	u32 pdi_max_size;
	u32 pdo_max_size;
	u32 pdo_x_size;
	u32 pdo_y_size;
	u32 pd_table_offset;
} __packed;

/*
 * struct mtk_cam_uapi_meta_hw_buf - hardware buffer info
 *
 * @offset: offset from the start of the device memory associated to the
 *    v4l2 meta buffer
 * @size: size of the buffer
 *
 * Some part of the meta buffers are read or written by statistic related
 * hardware DMAs. The hardware buffers may have different size among
 * difference pipeline.
 */
struct mtk_cam_uapi_meta_hw_buf {
	u32 offset;
	u32 size;
} __packed;

/*
 * struct mtk_cam_uapi_pdp_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:     stride value used by
 * @pdo_buf:     The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_pdp_stats {
	struct  mtk_cam_uapi_meta_size stats_src;
	u32   stride;
	struct  mtk_cam_uapi_meta_hw_buf pdo_buf;
} __packed;

/*
 * struct mtk_cam_uapi_cpi_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:     stride value used by
 * @pdo_buf:     The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_cpi_stats {
	struct  mtk_cam_uapi_meta_size stats_src;
	u32   stride;
	struct  mtk_cam_uapi_meta_hw_buf cpio_buf;
} __packed;

/*
 * struct mtk_cam_uapi_mqe_param
 *
 * @mqe_mode:
 */
struct mtk_cam_uapi_mqe_param {
	u32 mqe_mode;
} __packed;

/*
 * struct mtk_cam_uapi_mobc_param
 *
 *
 */
struct mtk_cam_uapi_mobc_param {
	u32 mobc_offst0;
	u32 mobc_offst1;
	u32 mobc_offst2;
	u32 mobc_offst3;
	u32 mobc_gain0;
	u32 mobc_gain1;
	u32 mobc_gain2;
	u32 mobc_gain3;
} __packed;

/*
 * struct mtk_cam_uapi_lsc_param
 *
 *
 */
struct mtk_cam_uapi_lsc_param {
	u32 lsc_ctl1;
	u32 lsc_ctl2;
	u32 lsc_ctl3;
	u32 lsc_lblock;
	u32 lsc_fblock;
	u32 lsc_ratio;
	u32 lsc_tpipe_ofst;
	u32 lsc_tpipe_size;
} __packed;

/*
 * struct mtk_cam_uapi_sgg_param
 *
 *
 */
struct mtk_cam_uapi_sgg_param {
	u32 sgg_pgn;
	u32 sgg_gmrc_1;
	u32 sgg_gmrc_2;
} __packed;

/*
 * struct mtk_cam_uapi_mbn_param
 *
 *
 */
struct mtk_cam_uapi_mbn_param {
	u32 mbn_pow;
	u32 mbn_dir;
	u32 mbn_spar_hei;
	u32 mbn_spar_pow;
	u32 mbn_spar_fac;
	u32 mbn_spar_con1;
	u32 mbn_spar_con0;
} __packed;

/*
 * struct mtk_cam_uapi_cpi_param
 *
 *
 */
struct mtk_cam_uapi_cpi_param {
	u32 cpi_th;
	u32 cpi_pow;
	u32 cpi_dir;
	u32 cpi_spar_hei;
	u32 cpi_spar_pow;
	u32 cpi_spar_fac;
	u32 cpi_spar_con1;
	u32 cpi_spar_con0;
} __packed;

/*
 * struct mtk_cam_uapi_lsci_param
 *
 *
 */
struct mtk_cam_uapi_lsci_param {
	u32 lsci_xsize;
	u32 lsci_ysize;
} __packed;

/*
 * Common stuff for all statistics
 */

#define MTK_CAM_UAPI_MAX_CORE_NUM (2)

/*
 * struct mtk_cam_uapi_pipeline_config - pipeline configuration
 *
 * @num_of_core: The number of isp cores
 */
struct mtk_cam_uapi_pipeline_config {
	u32	num_of_core;
	struct	mtk_cam_uapi_meta_size core_data_size;
	u32	core_pxl_mode_lg2;
} __packed;

/*
 *  A U T O  E X P O S U R E
 */

/* please check the size of MTK_CAM_AE_HIST_MAX_BIN*/
#define MTK_CAM_UAPI_AE_STATS_HIST_MAX_BIN (1024)

/*
 *  A E  A N D   A W B
 */

#define MTK_CAM_UAPI_AAO_BLK_SIZE (32)
#define MTK_CAM_UAPI_AAO_MAX_BLK_X (128)
#define MTK_CAM_UAPI_AAO_MAX_BLK_Y (128)
#define MTK_CAM_UAPI_AAO_MAX_BUF_SIZE (MTK_CAM_UAPI_AAO_BLK_SIZE \
					* MTK_CAM_UAPI_AAO_MAX_BLK_X \
					* MTK_CAM_UAPI_AAO_MAX_BLK_Y)

#define MTK_CAM_UAPI_AHO_BLK_SIZE (3)
#define MTK_CAM_UAPI_AAHO_HIST_SIZE  (6 * 1024 * MTK_CAM_UAPI_AHO_BLK_SIZE \
					+ 14 * 256 * MTK_CAM_UAPI_AHO_BLK_SIZE)
#define MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE  (MTK_CAM_UAPI_MAX_CORE_NUM * \
					MTK_CAM_UAPI_AAHO_HIST_SIZE)

/*
 * struct mtk_cam_uapi_ae_awb_stats - statistics of ae and awb
 *
 * @aao_buf:       The buffer for AAHO statistic hardware output.
 *        The maximum size of the buffer is defined with
 *        MTK_CAM_UAPI_AAO_MAX_BUF_SIZE
 * @aaho_buf:      The buffer for AAHO statistic hardware output.
 *        The maximum size of the buffer is defined with
 *        MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE.
 *
 * This is the AE and AWB statistic returned to user. From  our hardware's
 * point of view, we can't separate the AE and AWB output result, so I use
 * a struct to retutn them.
 */
struct mtk_cam_uapi_ae_awb_stats {
	u32 awb_stat_en_status;
	u32 awb_qbn_acc;
	u32 ae_stat_en_status;
	struct mtk_cam_uapi_meta_hw_buf aao_buf;
	struct mtk_cam_uapi_meta_hw_buf aaho_buf;
} __packed;

/*
 *  A U T O  F O C U S
 */

#define MTK_CAM_UAPI_AFO_BLK_SIZ    (32)
#define MTK_CAM_UAPI_AFO_MAX_BLK_NUM (128 * 128)
#define MTK_CAM_UAPI_AFO_MAX_BUF_SIZE   (MTK_CAM_UAPI_AFO_BLK_SIZ \
						* MTK_CAM_UAPI_AFO_MAX_BLK_NUM)

/*
 * struct mtk_cam_uapi_af_stats - af statistics
 *
 * @blk_num_x: block number of horizontal direction
 * @blk_num_y: block number of vertical direction
 * @afo_buf:    the buffer for AAHO statistic hardware output. The maximum
 *      size of the buffer is defined with
 *      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_af_stats {
	u32 blk_num_x;
	u32 blk_num_y;
	struct mtk_cam_uapi_meta_hw_buf afo_buf;
} __packed;

/*
 *  F L I C K E R
 */

/* FLK's hardware output block size: 64 bits */
#define MTK_CAM_UAPI_FLK_BLK_SIZE (8)

/* Maximum block size (each line) of Mediatek flicker statistic */
#define MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM (6)

/* Maximum height (in pixel) that driver can support */
#define MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT (9000)
#define MTK_CAM_UAPI_FLK_MAX_BUF_SIZE                              \
	(MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM * \
	MTK_CAM_UAPI_FLK_MAX_FRAME_HEIGHT)

/*
 * struct mtk_cam_uapi_flk_stats
 *
 * @flko_buf: the buffer for FLKO statistic hardware output. The maximum
 *         size of the buffer is defined with MTK_CAM_UAPI_FLK_MAX_BUF_SIZE.
 */
struct mtk_cam_uapi_flk_stats {
	struct mtk_cam_uapi_meta_hw_buf flko_buf;
} __packed;

/*
 *  T S F
 */

#define MTK_CAM_UAPI_TSFSO_SIZE (40 * 30 * 3 * 4)

/*
 * struct mtk_cam_uapi_tsf_stats - TSF statistic data
 *
 * @tsfo_buf: The buffer for tsf statistic hardware output. The buffer size
 *        is defined in MTK_CAM_UAPI_TSFSO_SIZE.
 *
 * This output is for Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_tsf_stats {
	struct mtk_cam_uapi_meta_hw_buf tsfo_r1_buf;
	struct mtk_cam_uapi_meta_hw_buf tsfo_r2_buf;
} __packed;

/*
 * struct mtk_cam_uapi_pd_stats - statistics of pd
 *
 * @stats_src:     source width and heitgh of the statistics.
 * @stride:	   stride value used by
 * @pdo_buf:	   The buffer for PD statistic hardware output.
 *
 * This is the PD statistic returned to user.
 */
struct mtk_cam_uapi_pd_stats {
	struct	mtk_cam_uapi_meta_size stats_src;
	u32	stride;
	struct	mtk_cam_uapi_meta_hw_buf pdo_buf;
} __packed;

struct mtk_cam_uapi_timestamp {
	u64 timestamp_buf[128];
} __packed;

/*
 *  T O N E
 */
#define MTK_CAM_UAPI_LTMSO_SIZE ((37 * 12 * 9 + 258) * 8)
#define MTK_CAM_UAPI_TNCSO_SIZE (680 * 510 * 2)
#define MTK_CAM_UAPI_TNCSHO_SIZE (1544)
#define MTK_CAM_UAPI_TNCSBO_SIZE (3888)
#define MTK_CAM_UAPI_TNCSYO_SIZE (68)

/*
 * struct mtk_cam_uapi_ltm_stats - Tone1 statistic data for
 *            Mediatek proprietary algorithm
 *
 * @ltmso_buf:  The buffer for ltm statistic hardware output. The buffer size
 *    is defined in MTK_CAM_UAPI_LTMSO_SIZE.
 * @blk_num_x: block number of horizontal direction
 * @blk_num_y:  block number of vertical direction
 *
 * For Mediatek proprietary algorithm
 */
struct mtk_cam_uapi_ltm_stats {
	struct mtk_cam_uapi_meta_hw_buf ltmso_buf;
	u8  blk_num_x;
	u8  blk_num_y;
	u8  rsv[2];
} __packed;

/*
 * struct mtk_cam_uapi_tnc_stats - Tone2 statistic data for
 *                 Mediatek proprietary algorithm
 *
 * @tncso_buf: The buffer for tnc statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSO_SIZE (680*510*2)
 */
struct mtk_cam_uapi_tnc_stats {
	struct mtk_cam_uapi_meta_hw_buf tncso_buf;
} __packed;

/*
 * struct mtk_cam_uapi_tnch_stats - Tone3 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsho_buf: The buffer for tnch statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSHO_SIZE (1544)
 */
struct mtk_cam_uapi_tnch_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsho_buf;
} __packed;

/*
 * struct mtk_cam_uapi_tncb_stats - Tone4 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsbo_buf: The buffer for tncb statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSBO_SIZE (3888)
 */
struct mtk_cam_uapi_tncb_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsbo_buf;
} __packed;

/*
 * struct mtk_cam_uapi_tncy_stats - Tone3 statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @tncsyo_buf: The buffer for tncy statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_TNCSYO_SIZE (68)
 */
struct mtk_cam_uapi_tncy_stats {
	struct mtk_cam_uapi_meta_hw_buf tncsyo_buf;
} __packed;

/*
 * struct mtk_cam_uapi_act_stats - act statistic data for Mediatek
 *                                  proprietary algorithm
 *
 * @actso_buf: The buffer for tncy statistic hardware output. The buffer size
 *           is defined in MTK_CAM_UAPI_ACTSO_SIZE (768)
 */
#define MTK_CAM_UAPI_ACTSO_SIZE (768)
struct mtk_cam_uapi_act_stats {
	struct mtk_cam_uapi_meta_hw_buf actso_buf;
} __packed;

/*
 * struct mtk_cam_uapi_awb_param_prot - AWB parameters
 *
 * @rot_cos: rotation matrix (cos part)
 * @rot_sin: rotation matrix (sin part)
 *
 */
struct mtk_cam_uapi_awb_param_prot {
	s32 rot_cos;
	s32 rot_sin;
} __packed;

/*
 *  T U N I N G S
 */

/*
 * struct mtk_cam_uapi_bpc_param_prot - BPC parameters
 *
 */
#define MTK_CAM_BPCI_TABLE_SIZE (32)
struct mtk_cam_uapi_bpc_param_prot {
	u32 x_size;
	u32 y_size;
	u32 stride;

	u8  table[MTK_CAM_BPCI_TABLE_SIZE];
} __packed;

/*
 * struct mtk_cam_uapi_drzs8t_param_prot
 *
 *  @tbl_shift: adjust the computed table
 *  @tbl_min: use to limit the min table
 */
struct mtk_cam_uapi_drzs8t_param_prot {
	u32 tbl_shift;
	u32 tbl_min;
} __packed;

/*
 * struct mtk_cam_uapi_lsc_param_prot - LSC parameters
 *
 */
#define MTK_CAM_LSCI_TABLE_SIZE (32768)
struct mtk_cam_uapi_lsc_param_prot {
	u32 x_blk_num;
	u32 y_blk_num;
	u32 x_size;
	u32 y_size;
	u32 stride;
	u8 table[MTK_CAM_LSCI_TABLE_SIZE];
} __packed;

/* *
 * struct mtk_cam_uapi_slk_param_prot - slk tuning setting from userspace
 *
 */
struct mtk_cam_uapi_slk_param_prot {
	u32 center_x;
	u32 center_y;
	u32 radius_0;
	u32 radius_1;
	u32 radius_2;
	u32 gain0;
	u32 gain1;
	u32 gain2;
	u32 gain3;
	u32 gain4;
} __packed;

/*
 * struct mtk_cam_uapi_wb_param_prot - WB parameters
 *
 */
struct mtk_cam_uapi_wb_param_prot {
	u32 debug_info[39];
} __packed;

/*
 * struct mtk_cam_uapi_ltms_param_prot - LTMS parameters
 *
 *  @ratio_x_start: adjusted start point of width related to original width
 *  @ratio_y_start: adjusted start point of height related to original height
 *  @ratio_x_end: adjusted end point of width related to original width
 *  @ratio_y_end: adjusted end point of height related to original height
 */
struct mtk_cam_uapi_ltms_param_prot {
	u32 ltms_gamma_en;
	u32 ratio_x_start;
	u32 ratio_y_start;
	u32 ratio_x_end;
	u32 ratio_y_end;
} __packed;

/*
 * struct mtk_cam_uapi_yuvo_param_prot - YUVO parameters
 *
 *  @drzh2n_fixed_down_ratio: down scale ratio
 */
struct mtk_cam_uapi_yuvo_param_prot {
	u32 drzh2n_fixed_down_ratio;
} __packed;
/* The following sw setting are generated by script */
/*
 * struct mtk_cam_uapi_ccm_param_prot - CCM parameters *
 */
struct mtk_cam_uapi_ccm_param_prot {
	u32 ccm_acc;
} __packed;

/*
 * struct mtk_cam_uapi_drzh2n_param_prot - DRZH2N parameters *
 */
struct mtk_cam_uapi_drzh2n_param_prot {
	u32 drzh2n_vert_tbl_sel;
	u32 drzh2n_hori_tbl_sel;
} __packed;

/*
 * struct mtk_cam_uapi_drzs4n_param_prot - DRZS4N parameters *
 */
struct mtk_cam_uapi_drzs4n_param_prot {
	u32 drzs4n_vert_tbl_sel;
	u32 drzs4n_hori_tbl_sel;
} __packed;

/*
 * struct mtk_cam_uapi_tncs_param_prot - TNCS parameters *
 */
struct mtk_cam_uapi_tncs_param_prot {
	u32 tncs_ggm_lnr;
	u32 tncs_ggm_end_var;
} __packed;

/*
 * Mediatek camera bpc tuning setting
 */
struct mtk_cam_uapi_regmap_raw_bpc {
	u32 bpc_func_con;
	u32 rsv0[49];
};

/*
 * Mediatek camera ccm tuning setting
 */
struct mtk_cam_uapi_regmap_raw_ccm {
	u32 ccm_cnv_1;
	u32 ccm_cnv_2;
	u32 ccm_cnv_3;
	u32 ccm_cnv_4;
	u32 ccm_cnv_5;
	u32 ccm_cnv_6;
};

/*
 * Mediatek camera dm tuning setting
 */
struct mtk_cam_uapi_regmap_raw_dm {
	u32 rsv0;
	u32 dm_intp_nat;
	u32 rsv1[3];
	u32 dm_sl_ctl;
	u32 rsv2;
	u32 dm_nr_str;
	u32 dm_nr_act;
	u32 dm_hf_str;
	u32 dm_hf_act1;
	u32 dm_hf_act2;
	u32 dm_clip;
	u32 rsv3[8];
	u32 dm_ee;
	u32 rsv4[4];
};

/*
 * Mediatek camera g2c tuning setting
 */
struct mtk_cam_uapi_regmap_raw_g2c {
	u32 g2c_conv_0a;
	u32 g2c_conv_0b;
	u32 g2c_conv_1a;
	u32 g2c_conv_1b;
	u32 g2c_conv_2a;
	u32 g2c_conv_2b;
};

#define MTK_CAM_UAPI_GGM_LUT (256)
/*
 * Mediatek camera ggm tuning setting
 */
struct mtk_cam_uapi_regmap_raw_ggm {
	u32 ggm_lut[MTK_CAM_UAPI_GGM_LUT];
	u32 ggm_ctrl;
};

/*
 * Mediatek camera lsc tuning setting
 */
struct mtk_cam_uapi_regmap_raw_lsc {
	u32 lsc_ratio;
};

#define MTK_CAM_UAPI_LTM_CURVE_SIZE_2 (1728)
#define MTK_CAM_UAPI_LTM_CLP_SIZE_2 (108)

/*
 * Mediatek camera ltm tuning setting
 */
struct mtk_cam_uapi_regmap_raw_ltm {
	u32 ltm_ctrl;
	u32 ltm_blk_num;
	u32 ltm_max_div;
	u32 ltm_clip;
	u32 ltm_cfg;
	u32 ltm_clip_th;
	u32 ltm_gain_map;
	u32 ltm_cvnode_grp0;
	u32 ltm_cvnode_grp1;
	u32 ltm_cvnode_grp2;
	u32 ltm_cvnode_grp3;
	u32 ltm_cvnode_grp4;
	u32 ltm_cvnode_grp5;
	u32 ltm_cvnode_grp6;
	u32 ltm_cvnode_grp7;
	u32 ltm_cvnode_grp8;
	u32 ltm_cvnode_grp9;
	u32 ltm_cvnode_grp10;
	u32 ltm_cvnode_grp11;
	u32 ltm_cvnode_grp12;
	u32 ltm_cvnode_grp13;
	u32 ltm_cvnode_grp14;
	u32 ltm_cvnode_grp15;
	u32 ltm_cvnode_grp16;
	u32 ltm_out_str;
	u32 ltm_act_win_x;
	u32 ltm_act_win_y;
	u32 ltmtc_curve[MTK_CAM_UAPI_LTM_CURVE_SIZE_2];
	u32 ltmtc_clp[MTK_CAM_UAPI_LTM_CLP_SIZE_2];
};

/*
 * Mediatek camera ltms tuning setting
 */
struct mtk_cam_uapi_regmap_raw_ltms {
	u32 ltms_max_div;
	u32 ltms_blkhist_lb;
	u32 ltms_blkhist_mb;
	u32 ltms_blkhist_ub;
	u32 ltms_blkhist_int;
	u32 ltms_clip_th_cal;
	u32 ltms_clip_th_lb;
	u32 ltms_clip_th_hb;
	u32 ltms_glbhist_int;
};

/*
 * Mediatek camera obc tuning setting
 */
struct mtk_cam_uapi_regmap_raw_obc {
	u32 obc_dbs;
	u32 obc_gray_bld_0;
	u32 obc_gray_bld_1;
	u32 obc_wbg_rb;
	u32 obc_wbg_g;
	u32 obc_wbig_rb;
	u32 obc_wbig_g;
	u32 obc_obg_rb;
	u32 obc_obg_g;
	u32 obc_offset_r;
	u32 obc_offset_gr;
	u32 obc_offset_gb;
	u32 obc_offset_b;
	u32 rsv1[2];
};

/*
 * Mediatek camera tsfs tuning setting
 */
struct mtk_cam_uapi_regmap_raw_tsfs {
	u32 tsfs_dgain;
};

/*
 * Usage example: To print value of "MTK_CAM_BPC_BPC_FUNC_CON_BPC_BPC_LUT_BIT_EXTEND_EN" in "val"
 * > printf("%x", GET_MTK_CAM(val, MTK_CAM_BPC_BPC_FUNC_CON_BPC_BPC_LUT_BIT_EXTEND_EN));
 */
#define GET_MTK_CAM(val, field) (((val) & field##_MASK) >> \
				 field##_SHIFT)
/*
 * Usage example: To set "val_of_bpc_lut_bit_extend_en" to bits
 * of "MTK_CAM_BPC_BPC_FUNC_CON_BPC_BPC_LUT_BIT_EXTEND_EN" in "val"
 * > val = SET_MTK_CAM(val, MTK_CAM_BPC_BPC_FUNC_CON_BPC_BPC_LUT_BIT_EXTEND_EN,
 * val_of_bpc_lut_bit_extend_en);
 */
#define SET_MTK_CAM(val, field, set_val) (((val) & ~field##_MASK) | \
					  (((set_val) << field##_SHIFT) & field##_MASK))

/*
 * Bit Feild of BPC_FUNC_CON: BPC_EN
 * MTK_CAM_BPC_FUNC_CON_BPC_EN: [31, 31]
 * Enable/disable for BPC Correction
 * 1'd1: enable the function
 * 1'd0: disable the function
 */
#define MTK_CAM_BPC_FUNC_CON_BPC_EN_MASK   0x80000000
#define MTK_CAM_BPC_FUNC_CON_BPC_EN_SHIFT  31

/*
 * Bit Feild of BPC_FUNC_CON: BPC_CT_EN
 * MTK_CAM_BPC_FUNC_CON_BPC_CT_EN: [30, 30]
 * Enable/disable for Cross-Talk compensation
 * 1'd1: enable
 * 1'd0: disable
 */
#define MTK_CAM_BPC_FUNC_CON_BPC_CT_EN_MASK   0x40000000
#define MTK_CAM_BPC_FUNC_CON_BPC_CT_EN_SHIFT  30

/*
 * Bit Feild of BPC_FUNC_CON: BPC_PDC_EN
 * MTK_CAM_BPC_FUNC_CON_BPC_PDC_EN: [29, 29]
 * Enable/disable for PDC correction
 * 1'd1: enable
 * 1'd0: disable
 */
#define MTK_CAM_BPC_FUNC_CON_BPC_PDC_EN_MASK   0x20000000
#define MTK_CAM_BPC_FUNC_CON_BPC_PDC_EN_SHIFT  29

/*
 * Bit Feild of BPC_FUNC_CON: BPC_LUT_EN
 * MTK_CAM_BPC_FUNC_CON_BPC_LUT_EN: [28, 28]
 * Enable table lookup
 * 1'd1:  enable BPC with default table mode
 * 1'd0:  disable BPC with default table mode
 */
#define MTK_CAM_BPC_FUNC_CON_BPC_LUT_EN_MASK   0x10000000
#define MTK_CAM_BPC_FUNC_CON_BPC_LUT_EN_SHIFT  28

/*
 * Bit Feild of BPC_FUNC_CON: BPC_LUT_BIT_EXTEND_EN
 * MTK_CAM_BPC_FUNC_CON_BPC_LUT_BIT_EXTEND_EN: [0, 0]
 * Enable table 24 bits mode
 * 1'd1: Table format to be 24 bits
 * 1'd0: @ the original format, tbale to be 16 bits mode
 */
#define MTK_CAM_BPC_FUNC_CON_BPC_LUT_BIT_EXTEND_EN_MASK   0x00000001
#define MTK_CAM_BPC_FUNC_CON_BPC_LUT_BIT_EXTEND_EN_SHIFT  0

/*
 * Bit Feild of CCM_CNV_1: CCM_CNV_01
 * MTK_CAM_CCM_CNV_1_CCM_CNV_01: [16, 28]
 * matrix 0,1 coefficient
 */
#define MTK_CAM_CCM_CNV_1_CCM_CNV_01_MASK   0x1fff0000
#define MTK_CAM_CCM_CNV_1_CCM_CNV_01_SHIFT  16

/*
 * Bit Feild of CCM_CNV_1: CCM_CNV_00
 * MTK_CAM_CCM_CNV_1_CCM_CNV_00: [0, 12]
 * matrix 0,0 coefficient
 */
#define MTK_CAM_CCM_CNV_1_CCM_CNV_00_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_1_CCM_CNV_00_SHIFT  0

/*
 * Bit Feild of CCM_CNV_2: CCM_CNV_02
 * MTK_CAM_CCM_CNV_2_CCM_CNV_02: [0, 12]
 * matrix 0,2 coefficient
 */
#define MTK_CAM_CCM_CNV_2_CCM_CNV_02_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_2_CCM_CNV_02_SHIFT  0

/*
 * Bit Feild of CCM_CNV_3: CCM_CNV_11
 * MTK_CAM_CCM_CNV_3_CCM_CNV_11: [16, 28]
 * matrix 1,1 coefficient
 */
#define MTK_CAM_CCM_CNV_3_CCM_CNV_11_MASK   0x1fff0000
#define MTK_CAM_CCM_CNV_3_CCM_CNV_11_SHIFT  16

/*
 * Bit Feild of CCM_CNV_3: CCM_CNV_10
 * MTK_CAM_CCM_CNV_3_CCM_CNV_10: [0, 12]
 * matrix 1,0 coefficient
 */
#define MTK_CAM_CCM_CNV_3_CCM_CNV_10_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_3_CCM_CNV_10_SHIFT  0

/*
 * Bit Feild of CCM_CNV_4: CCM_CNV_12
 * MTK_CAM_CCM_CNV_4_CCM_CNV_12: [0, 12]
 * matrix 1,2 coefficient
 */
#define MTK_CAM_CCM_CNV_4_CCM_CNV_12_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_4_CCM_CNV_12_SHIFT  0

/*
 * Bit Feild of CCM_CNV_5: CCM_CNV_21
 * MTK_CAM_CCM_CNV_5_CCM_CNV_21: [16, 28]
 * matrix 2,1 coefficient
 */
#define MTK_CAM_CCM_CNV_5_CCM_CNV_21_MASK   0x1fff0000
#define MTK_CAM_CCM_CNV_5_CCM_CNV_21_SHIFT  16

/*
 * Bit Feild of CCM_CNV_5: CCM_CNV_20
 * MTK_CAM_CCM_CNV_5_CCM_CNV_20: [0, 12]
 * matrix 2,0 coefficient
 */
#define MTK_CAM_CCM_CNV_5_CCM_CNV_20_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_5_CCM_CNV_20_SHIFT  0

/*
 * Bit Feild of CCM_CNV_6: CCM_CNV_22
 * MTK_CAM_CCM_CNV_6_CCM_CNV_22: [0, 12]
 * matrix 2,2 coefficient
 */
#define MTK_CAM_CCM_CNV_6_CCM_CNV_22_MASK   0x00001fff
#define MTK_CAM_CCM_CNV_6_CCM_CNV_22_SHIFT  0

/*
 * Bit Feild of DM_INTP_NAT: DM_L0_OFST
 * MTK_CAM_DM_INTP_NAT_DM_L0_OFST: [12, 19]
 * luma blending LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_INTP_NAT_DM_L0_OFST_MASK   0x000ff000
#define MTK_CAM_DM_INTP_NAT_DM_L0_OFST_SHIFT  12

/*
 * Bit Feild of DM_SL_CTL: DM_SL_Y1
 * MTK_CAM_DM_SL_CTL_DM_SL_Y1: [14, 21]
 * shading link modulation LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_SL_CTL_DM_SL_Y1_MASK   0x003fc000
#define MTK_CAM_DM_SL_CTL_DM_SL_Y1_SHIFT  14

/*
 * Bit Feild of DM_SL_CTL: DM_SL_Y2
 * MTK_CAM_DM_SL_CTL_DM_SL_Y2: [6, 13]
 * shading link modulation LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_SL_CTL_DM_SL_Y2_MASK   0x00003fc0
#define MTK_CAM_DM_SL_CTL_DM_SL_Y2_SHIFT  6

/*
 * Bit Feild of DM_SL_CTL: DM_SL_EN
 * MTK_CAM_DM_SL_CTL_DM_SL_EN: [0, 0]
 * shading link enable
 * 0: disable SL
 * 1: enable SL
 */
#define MTK_CAM_DM_SL_CTL_DM_SL_EN_MASK   0x00000001
#define MTK_CAM_DM_SL_CTL_DM_SL_EN_SHIFT  0

/*
 * Bit Feild of DM_NR_STR: DM_N0_STR
 * MTK_CAM_DM_NR_STR_DM_N0_STR: [10, 14]
 * noise reduction strength
 * 0 ~ 16
 */
#define MTK_CAM_DM_NR_STR_DM_N0_STR_MASK   0x00007c00
#define MTK_CAM_DM_NR_STR_DM_N0_STR_SHIFT  10

/*
 * Bit Feild of DM_NR_ACT: DM_N0_OFST
 * MTK_CAM_DM_NR_ACT_DM_N0_OFST: [24, 31]
 * noise reduction activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_NR_ACT_DM_N0_OFST_MASK   0xff000000
#define MTK_CAM_DM_NR_ACT_DM_N0_OFST_SHIFT  24

/*
 * Bit Feild of DM_HF_STR: DM_HA_STR
 * MTK_CAM_DM_HF_STR_DM_HA_STR: [27, 31]
 * overall high frequency strength
 * 0 ~ 31
 */
#define MTK_CAM_DM_HF_STR_DM_HA_STR_MASK   0xf8000000
#define MTK_CAM_DM_HF_STR_DM_HA_STR_SHIFT  27

/*
 * Bit Feild of DM_HF_STR: DM_H1_GN
 * MTK_CAM_DM_HF_STR_DM_H1_GN: [22, 26]
 * individual high frequency strength
 * 0 ~ 31
 */
#define MTK_CAM_DM_HF_STR_DM_H1_GN_MASK   0x07c00000
#define MTK_CAM_DM_HF_STR_DM_H1_GN_SHIFT  22

/*
 * Bit Feild of DM_HF_STR: DM_H2_GN
 * MTK_CAM_DM_HF_STR_DM_H2_GN: [17, 21]
 * individual high frequency strength
 * 0 ~ 31
 */
#define MTK_CAM_DM_HF_STR_DM_H2_GN_MASK   0x003e0000
#define MTK_CAM_DM_HF_STR_DM_H2_GN_SHIFT  17

/*
 * Bit Feild of DM_HF_STR: DM_H3_GN
 * MTK_CAM_DM_HF_STR_DM_H3_GN: [12, 16]
 * individual high frequency strength
 * 0 ~ 31
 */
#define MTK_CAM_DM_HF_STR_DM_H3_GN_MASK   0x0001f000
#define MTK_CAM_DM_HF_STR_DM_H3_GN_SHIFT  12

/*
 * Bit Feild of DM_HF_ACT1: DM_H1_LWB
 * MTK_CAM_DM_HF_ACT1_DM_H1_LWB: [24, 31]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT1_DM_H1_LWB_MASK   0xff000000
#define MTK_CAM_DM_HF_ACT1_DM_H1_LWB_SHIFT  24

/*
 * Bit Feild of DM_HF_ACT1: DM_H1_UPB
 * MTK_CAM_DM_HF_ACT1_DM_H1_UPB: [16, 23]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT1_DM_H1_UPB_MASK   0x00ff0000
#define MTK_CAM_DM_HF_ACT1_DM_H1_UPB_SHIFT  16

/*
 * Bit Feild of DM_HF_ACT1: DM_H2_LWB
 * MTK_CAM_DM_HF_ACT1_DM_H2_LWB: [8, 15]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT1_DM_H2_LWB_MASK   0x0000ff00
#define MTK_CAM_DM_HF_ACT1_DM_H2_LWB_SHIFT  8

/*
 * Bit Feild of DM_HF_ACT1: DM_H2_UPB
 * MTK_CAM_DM_HF_ACT1_DM_H2_UPB: [0, 7]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT1_DM_H2_UPB_MASK   0x000000ff
#define MTK_CAM_DM_HF_ACT1_DM_H2_UPB_SHIFT  0

/*
 * Bit Feild of DM_HF_ACT2: DM_H3_LWB
 * MTK_CAM_DM_HF_ACT2_DM_H3_LWB: [16, 23]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT2_DM_H3_LWB_MASK   0x00ff0000
#define MTK_CAM_DM_HF_ACT2_DM_H3_LWB_SHIFT  16

/*
 * Bit Feild of DM_HF_ACT2: DM_H3_UPB
 * MTK_CAM_DM_HF_ACT2_DM_H3_UPB: [8, 15]
 * high frequency activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_HF_ACT2_DM_H3_UPB_MASK   0x0000ff00
#define MTK_CAM_DM_HF_ACT2_DM_H3_UPB_SHIFT  8

/*
 * Bit Feild of DM_CLIP: DM_OV_TH
 * MTK_CAM_DM_CLIP_DM_OV_TH: [16, 23]
 * over/undershoot brightness LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_CLIP_DM_OV_TH_MASK   0x00ff0000
#define MTK_CAM_DM_CLIP_DM_OV_TH_SHIFT  16

/*
 * Bit Feild of DM_CLIP: DM_UN_TH
 * MTK_CAM_DM_CLIP_DM_UN_TH: [8, 15]
 * over/undershoot brightness LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_CLIP_DM_UN_TH_MASK   0x0000ff00
#define MTK_CAM_DM_CLIP_DM_UN_TH_SHIFT  8

/*
 * Bit Feild of DM_CLIP: DM_CLIP_TH
 * MTK_CAM_DM_CLIP_DM_CLIP_TH: [0, 7]
 * over/undershoot activity LUT
 * 0 ~ 255
 */
#define MTK_CAM_DM_CLIP_DM_CLIP_TH_MASK   0x000000ff
#define MTK_CAM_DM_CLIP_DM_CLIP_TH_SHIFT  0

/*
 * Bit Feild of DM_EE: DM_HNEG_GN
 * MTK_CAM_DM_EE_DM_HNEG_GN: [5, 9]
 * edge enhancement negative gain
 * 0~16
 */
#define MTK_CAM_DM_EE_DM_HNEG_GN_MASK   0x000003e0
#define MTK_CAM_DM_EE_DM_HNEG_GN_SHIFT  5

/*
 * Bit Feild of DM_EE: DM_HPOS_GN
 * MTK_CAM_DM_EE_DM_HPOS_GN: [0, 4]
 * edge enhancement positive gain
 * 0~16
 */
#define MTK_CAM_DM_EE_DM_HPOS_GN_MASK   0x0000001f
#define MTK_CAM_DM_EE_DM_HPOS_GN_SHIFT  0

/*
 * Bit Feild of G2C_CONV_0A: G2C_CNV_01
 * MTK_CAM_G2C_CONV_0A_G2C_CNV_01: [16, 26]
 * DIP RGB 2 YUV Matrix 0,1 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_0A_G2C_CNV_01_MASK   0x07ff0000
#define MTK_CAM_G2C_CONV_0A_G2C_CNV_01_SHIFT  16

/*
 * Bit Feild of G2C_CONV_0A: G2C_CNV_00
 * MTK_CAM_G2C_CONV_0A_G2C_CNV_00: [0, 10]
 * DIP RGB 2 YUV Matrix 0,0 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_0A_G2C_CNV_00_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_0A_G2C_CNV_00_SHIFT  0

/*
 * Bit Feild of G2C_CONV_0B: G2C_Y_OFST
 * MTK_CAM_G2C_CONV_0B_G2C_Y_OFST: [16, 30]
 * Y offset. Q1.10.0 (mobile) or Q1.14.0 (non-mobile)
 */
#define MTK_CAM_G2C_CONV_0B_G2C_Y_OFST_MASK   0x7fff0000
#define MTK_CAM_G2C_CONV_0B_G2C_Y_OFST_SHIFT  16

/*
 * Bit Feild of G2C_CONV_0B: G2C_CNV_02
 * MTK_CAM_G2C_CONV_0B_G2C_CNV_02: [0, 10]
 * DIP RGB 2 YUV Matrix 0,2 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_0B_G2C_CNV_02_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_0B_G2C_CNV_02_SHIFT  0

/*
 * Bit Feild of G2C_CONV_1A: G2C_CNV_11
 * MTK_CAM_G2C_CONV_1A_G2C_CNV_11: [16, 26]
 * DIP RGB 2 YUV Matrix 1,1 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_1A_G2C_CNV_11_MASK   0x07ff0000
#define MTK_CAM_G2C_CONV_1A_G2C_CNV_11_SHIFT  16

/*
 * Bit Feild of G2C_CONV_1A: G2C_CNV_10
 * MTK_CAM_G2C_CONV_1A_G2C_CNV_10: [0, 10]
 * DIP RGB 2 YUV Matrix 1,0 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_1A_G2C_CNV_10_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_1A_G2C_CNV_10_SHIFT  0

/*
 * Bit Feild of G2C_CONV_1B: G2C_U_OFST
 * MTK_CAM_G2C_CONV_1B_G2C_U_OFST: [16, 29]
 * U offset. Q1.9.0 (mobile) or Q1.13.0 (non-mobile)
 */
#define MTK_CAM_G2C_CONV_1B_G2C_U_OFST_MASK   0x3fff0000
#define MTK_CAM_G2C_CONV_1B_G2C_U_OFST_SHIFT  16

/*
 * Bit Feild of G2C_CONV_1B: G2C_CNV_12
 * MTK_CAM_G2C_CONV_1B_G2C_CNV_12: [0, 10]
 * DIP RGB 2 YUV Matrix 1,2 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_1B_G2C_CNV_12_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_1B_G2C_CNV_12_SHIFT  0

/*
 * Bit Feild of G2C_CONV_2A: G2C_CNV_21
 * MTK_CAM_G2C_CONV_2A_G2C_CNV_21: [16, 26]
 * DIP RGB 2 YUV Matrix 2,1 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_2A_G2C_CNV_21_MASK   0x07ff0000
#define MTK_CAM_G2C_CONV_2A_G2C_CNV_21_SHIFT  16

/*
 * Bit Feild of G2C_CONV_2A: G2C_CNV_20
 * MTK_CAM_G2C_CONV_2A_G2C_CNV_20: [0, 10]
 * DIP RGB 2 YUV Matrix 2,0 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_2A_G2C_CNV_20_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_2A_G2C_CNV_20_SHIFT  0

/*
 * Bit Feild of G2C_CONV_2B: G2C_V_OFST
 * MTK_CAM_G2C_CONV_2B_G2C_V_OFST: [16, 29]
 * V offset. Q1.9.0 (mobile) or Q1.13.0 (non-mobile)
 */
#define MTK_CAM_G2C_CONV_2B_G2C_V_OFST_MASK   0x3fff0000
#define MTK_CAM_G2C_CONV_2B_G2C_V_OFST_SHIFT  16

/*
 * Bit Feild of G2C_CONV_2B: G2C_CNV_22
 * MTK_CAM_G2C_CONV_2B_G2C_CNV_22: [0, 10]
 * DIP RGB 2 YUV Matrix 2,2 coefficient in Q1.1.9
 */
#define MTK_CAM_G2C_CONV_2B_G2C_CNV_22_MASK   0x000007ff
#define MTK_CAM_G2C_CONV_2B_G2C_CNV_22_SHIFT  0

/*
 * Bit Feild of GGM_LUT: GGM_LUT
 * MTK_CAM_GGM_LUT_GGM_LUT: [0, 9]
 * Gamma table entry
 * Do NOT read/write this control register when GGM is
 * enabled (ISP pipeline processing is on-going) or output
 * data of GGM will be gated
 */
#define MTK_CAM_GGM_LUT_GGM_LUT_MASK   0x000003ff
#define MTK_CAM_GGM_LUT_GGM_LUT_SHIFT  0

/*
 * Bit Feild of GGM_CTRL: GGM_LNR
 * MTK_CAM_GGM_CTRL_GGM_LNR: [0, 0]
 * Enable linear output
 */
#define MTK_CAM_GGM_CTRL_GGM_LNR_MASK   0x00000001
#define MTK_CAM_GGM_CTRL_GGM_LNR_SHIFT  0

/*
 * Bit Feild of GGM_CTRL: GGM_END_VAR
 * MTK_CAM_GGM_CTRL_GGM_END_VAR: [1, 10]
 * end point value
 */
#define MTK_CAM_GGM_CTRL_GGM_END_VAR_MASK   0x000007fe
#define MTK_CAM_GGM_CTRL_GGM_END_VAR_SHIFT  1

/*
 * Bit Feild of GGM_CTRL: GGM_RMP_VAR
 * MTK_CAM_GGM_CTRL_GGM_RMP_VAR: [16, 20]
 * 5-bit: can support mapping to 14-bit output, (RMP_VAR+out limiter)/1024
 */
#define MTK_CAM_GGM_CTRL_GGM_RMP_VAR_MASK   0x001f0000
#define MTK_CAM_GGM_CTRL_GGM_RMP_VAR_SHIFT  16

/*
 * Bit Feild of LSC_RATIO: LSC_RA00
 * MTK_CAM_LSC_RATIO_LSC_RA00: [0, 5]
 * Shading ratio
 */
#define MTK_CAM_LSC_RATIO_LSC_RA00_MASK   0x0000003f
#define MTK_CAM_LSC_RATIO_LSC_RA00_SHIFT  0

/*
 * Bit Feild of LTMS_MAX_DIV: LTMS_CLIP_TH_ALPHA_BASE
 * MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE: [0, 9]
 * Divider for Maxvalue
 */
#define MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE_MASK   0x000003ff
#define MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE_SHIFT  0

/*
 * Bit Feild of LTMS_MAX_DIV: LTMS_CLIP_TH_ALPHA_BASE_SHIFT_BIT
 * MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE_SHIFT_BIT: [16, 20]
 * Divider for Maxvalue
 */
#define MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE_SHIFT_BIT_MASK   0x001f0000
#define MTK_CAM_LTMS_MAX_DIV_LTMS_CLIP_TH_ALPHA_BASE_SHIFT_BIT_SHIFT  16

/*
 * Bit Feild of LTMS_BLKHIST_LB: LTMS_BLKHIST_LB
 * MTK_CAM_LTMS_BLKHIST_LB_LTMS_BLKHIST_LB: [0, 17]
 * block histogram low bound,
 * BLKHIST_UB>=BLKHIST_MB>=BLKHIST_LB
 */
#define MTK_CAM_LTMS_BLKHIST_LB_LTMS_BLKHIST_LB_MASK   0x0003ffff
#define MTK_CAM_LTMS_BLKHIST_LB_LTMS_BLKHIST_LB_SHIFT  0

/*
 * Bit Feild of LTMS_BLKHIST_MB: LTMS_BLKHIST_MB
 * MTK_CAM_LTMS_BLKHIST_MB_LTMS_BLKHIST_MB: [0, 17]
 * block histogram middle bound,
 * BLKHIST_UB>=BLKHIST_MB>=BLKHIST_LB
 */
#define MTK_CAM_LTMS_BLKHIST_MB_LTMS_BLKHIST_MB_MASK   0x0003ffff
#define MTK_CAM_LTMS_BLKHIST_MB_LTMS_BLKHIST_MB_SHIFT  0

/*
 * Bit Feild of LTMS_BLKHIST_UB: LTMS_BLKHIST_UB
 * MTK_CAM_LTMS_BLKHIST_UB_LTMS_BLKHIST_UB: [0, 17]
 * block histogram up bound,
 * BLKHIST_UB>=BLKHIST_MB>=BLKHIST_LB
 */
#define MTK_CAM_LTMS_BLKHIST_UB_LTMS_BLKHIST_UB_MASK   0x0003ffff
#define MTK_CAM_LTMS_BLKHIST_UB_LTMS_BLKHIST_UB_SHIFT  0

/*
 * Bit Feild of LTMS_BLKHIST_INT: LTMS_BLKHIST_INT1
 * MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT1: [0, 13]
 * block histogram interval 1
 */
#define MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT1_MASK   0x00003fff
#define MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT1_SHIFT  0

/*
 * Bit Feild of LTMS_BLKHIST_INT: LTMS_BLKHIST_INT2
 * MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT2: [16, 29]
 * block histogram interval 2
 */
#define MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT2_MASK   0x3fff0000
#define MTK_CAM_LTMS_BLKHIST_INT_LTMS_BLKHIST_INT2_SHIFT  16

/*
 * Bit Feild of LTMS_CLIP_TH_CAL: LTMS_CLP_HLTHD
 * MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_HLTHD: [0, 10]
 * control percentage of histogram to calculate clip_th, 10-bits precision.
 */
#define MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_HLTHD_MASK   0x000007ff
#define MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_HLTHD_SHIFT  0

/*
 * Bit Feild of LTMS_CLIP_TH_CAL: LTMS_CLP_STARTBIN
 * MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_STARTBIN: [16, 23]
 * start bin of histogram.
 */
#define MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_STARTBIN_MASK   0x00ff0000
#define MTK_CAM_LTMS_CLIP_TH_CAL_LTMS_CLP_STARTBIN_SHIFT  16

/*
 * Bit Feild of LTMS_CLIP_TH_LB: LTMS_CLP_LB
 * MTK_CAM_LTMS_CLIP_TH_LB_LTMS_CLP_LB: [0, 21]
 * Low bound of clip threshold output.
 */
#define MTK_CAM_LTMS_CLIP_TH_LB_LTMS_CLP_LB_MASK   0x003fffff
#define MTK_CAM_LTMS_CLIP_TH_LB_LTMS_CLP_LB_SHIFT  0

/*
 * Bit Feild of LTMS_CLIP_TH_HB: LTMS_CLP_HB
 * MTK_CAM_LTMS_CLIP_TH_HB_LTMS_CLP_HB: [0, 21]
 * High bound of clip threshold output.
 */
#define MTK_CAM_LTMS_CLIP_TH_HB_LTMS_CLP_HB_MASK   0x003fffff
#define MTK_CAM_LTMS_CLIP_TH_HB_LTMS_CLP_HB_SHIFT  0

/*
 * Bit Feild of LTMS_GLBHIST_INT: LTMS_GLBHIST_INT
 * MTK_CAM_LTMS_GLBHIST_INT_LTMS_GLBHIST_INT: [0, 14]
 * Interval of global histogram
 */
#define MTK_CAM_LTMS_GLBHIST_INT_LTMS_GLBHIST_INT_MASK   0x00007fff
#define MTK_CAM_LTMS_GLBHIST_INT_LTMS_GLBHIST_INT_SHIFT  0

/*
 * Bit Feild of LTMTC_CURVE: LTMTC_TONECURVE_LUT_L
 * MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_L: [0, 13]
 * SRAM_PING_PONG
 * [u8.6-bits]x12x9
 */
#define MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_L_MASK   0x00003fff
#define MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_L_SHIFT  0

/*
 * Bit Feild of LTMTC_CURVE: LTMTC_TONECURVE_LUT_H
 * MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_H: [16, 29]
 * SRAM_PING_PONG
 * [u8.6-bits]x12x9
 */
#define MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_H_MASK   0x3fff0000
#define MTK_CAM_LTMTC_CURVE_LTMTC_TONECURVE_LUT_H_SHIFT  16

/*
 * Bit Feild of LTMTC_CLP: LTMTC_TONECURVE_CLP
 * MTK_CAM_LTMTC_CLP_LTMTC_TONECURVE_CLP: [0, 23]
 * LTM block CT
 */
#define MTK_CAM_LTMTC_CLP_LTMTC_TONECURVE_CLP_MASK   0x00ffffff
#define MTK_CAM_LTMTC_CLP_LTMTC_TONECURVE_CLP_SHIFT  0

/*
 * Bit Feild of LTM_CTRL: LTM_GAMMA_EN
 * MTK_CAM_LTM_CTRL_LTM_GAMMA_EN: [4, 4]
 * Enable gamma domain
 */
#define MTK_CAM_LTM_CTRL_LTM_GAMMA_EN_MASK   0x00000010
#define MTK_CAM_LTM_CTRL_LTM_GAMMA_EN_SHIFT  4

/*
 * Bit Feild of LTM_CTRL: LTM_CURVE_CP_MODE
 * MTK_CAM_LTM_CTRL_LTM_CURVE_CP_MODE: [5, 5]
 * Mode of curve control point, [0]: 33 fixed cp, [1]: 16 XY cp
 */
#define MTK_CAM_LTM_CTRL_LTM_CURVE_CP_MODE_MASK   0x00000020
#define MTK_CAM_LTM_CTRL_LTM_CURVE_CP_MODE_SHIFT  5

/*
 * Bit Feild of LTM_BLK_NUM: LTM_BLK_X_NUM
 * MTK_CAM_LTM_BLK_NUM_LTM_BLK_X_NUM: [0, 4]
 * block X number supports 2~12
 */
#define MTK_CAM_LTM_BLK_NUM_LTM_BLK_X_NUM_MASK   0x0000001f
#define MTK_CAM_LTM_BLK_NUM_LTM_BLK_X_NUM_SHIFT  0

/*
 * Bit Feild of LTM_BLK_NUM: LTM_BLK_Y_NUM
 * MTK_CAM_LTM_BLK_NUM_LTM_BLK_Y_NUM: [8, 12]
 * block Y number supports 2~9
 */
#define MTK_CAM_LTM_BLK_NUM_LTM_BLK_Y_NUM_MASK   0x00001f00
#define MTK_CAM_LTM_BLK_NUM_LTM_BLK_Y_NUM_SHIFT  8

/*
 * Bit Feild of LTM_MAX_DIV: LTM_CLIP_TH_ALPHA_BASE
 * MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE: [0, 9]
 * Divider for Maxvalue
 */
#define MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE_MASK   0x000003ff
#define MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE_SHIFT  0

/*
 * Bit Feild of LTM_MAX_DIV: LTM_CLIP_TH_ALPHA_BASE_SHIFT_BIT
 * MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE_SHIFT_BIT: [16, 20]
 * Divider for Maxvalue
 */
#define MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE_SHIFT_BIT_MASK   0x001f0000
#define MTK_CAM_LTM_MAX_DIV_LTM_CLIP_TH_ALPHA_BASE_SHIFT_BIT_SHIFT  16

/*
 * Bit Feild of LTM_CLIP: LTM_GAIN_TH
 * MTK_CAM_LTM_CLIP_LTM_GAIN_TH: [0, 5]
 * Threshold to clip output gain
 */
#define MTK_CAM_LTM_CLIP_LTM_GAIN_TH_MASK   0x0000003f
#define MTK_CAM_LTM_CLIP_LTM_GAIN_TH_SHIFT  0

/*
 * Bit Feild of LTM_CFG: LTM_ENGINE_EN
 * MTK_CAM_LTM_CFG_LTM_ENGINE_EN: [0, 0]
 * None
 */
#define MTK_CAM_LTM_CFG_LTM_ENGINE_EN_MASK   0x00000001
#define MTK_CAM_LTM_CFG_LTM_ENGINE_EN_SHIFT  0

/*
 * Bit Feild of LTM_CFG: LTM_CG_DISABLE
 * MTK_CAM_LTM_CFG_LTM_CG_DISABLE: [4, 4]
 * None
 */
#define MTK_CAM_LTM_CFG_LTM_CG_DISABLE_MASK   0x00000010
#define MTK_CAM_LTM_CFG_LTM_CG_DISABLE_SHIFT  4

/*
 * Bit Feild of LTM_CFG: LTM_CHKSUM_EN
 * MTK_CAM_LTM_CFG_LTM_CHKSUM_EN: [28, 28]
 * None
 */
#define MTK_CAM_LTM_CFG_LTM_CHKSUM_EN_MASK   0x10000000
#define MTK_CAM_LTM_CFG_LTM_CHKSUM_EN_SHIFT  28

/*
 * Bit Feild of LTM_CFG: LTM_CHKSUM_SEL
 * MTK_CAM_LTM_CFG_LTM_CHKSUM_SEL: [29, 30]
 * None
 */
#define MTK_CAM_LTM_CFG_LTM_CHKSUM_SEL_MASK   0x60000000
#define MTK_CAM_LTM_CFG_LTM_CHKSUM_SEL_SHIFT  29

/*
 * Bit Feild of LTM_CLIP_TH: LTM_CLIP_TH
 * MTK_CAM_LTM_CLIP_TH_LTM_CLIP_TH: [0, 23]
 * clipping threshold, enabled if #define LTM_USE_PREVIOUS_MAXVALUE=1
 */
#define MTK_CAM_LTM_CLIP_TH_LTM_CLIP_TH_MASK   0x00ffffff
#define MTK_CAM_LTM_CLIP_TH_LTM_CLIP_TH_SHIFT  0

/*
 * Bit Feild of LTM_CLIP_TH: LTM_WGT_BSH
 * MTK_CAM_LTM_CLIP_TH_LTM_WGT_BSH: [24, 27]
 * rightward bit shift for weighting data
 */
#define MTK_CAM_LTM_CLIP_TH_LTM_WGT_BSH_MASK   0x0f000000
#define MTK_CAM_LTM_CLIP_TH_LTM_WGT_BSH_SHIFT  24

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_MAP_LOG_EN
 * MTK_CAM_LTM_GAIN_MAP_LTM_MAP_LOG_EN: [0, 0]
 * switch for map log
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_MAP_LOG_EN_MASK   0x00000001
#define MTK_CAM_LTM_GAIN_MAP_LTM_MAP_LOG_EN_SHIFT  0

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_WGT_LOG_EN
 * MTK_CAM_LTM_GAIN_MAP_LTM_WGT_LOG_EN: [1, 1]
 * switch for weight log
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_WGT_LOG_EN_MASK   0x00000002
#define MTK_CAM_LTM_GAIN_MAP_LTM_WGT_LOG_EN_SHIFT  1

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_NONTRAN_MAP_TYPE
 * MTK_CAM_LTM_GAIN_MAP_LTM_NONTRAN_MAP_TYPE: [4, 7]
 * type of nontran map
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_NONTRAN_MAP_TYPE_MASK   0x000000f0
#define MTK_CAM_LTM_GAIN_MAP_LTM_NONTRAN_MAP_TYPE_SHIFT  4

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_TRAN_MAP_TYPE
 * MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_MAP_TYPE: [8, 11]
 * type of tran map
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_MAP_TYPE_MASK   0x00000f00
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_MAP_TYPE_SHIFT  8

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_TRAN_WGT_TYPE
 * MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT_TYPE: [12, 13]
 * type of tran weight
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT_TYPE_MASK   0x00003000
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT_TYPE_SHIFT  12

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_TRAN_WGT
 * MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT: [16, 20]
 * static tran weight
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT_MASK   0x001f0000
#define MTK_CAM_LTM_GAIN_MAP_LTM_TRAN_WGT_SHIFT  16

/*
 * Bit Feild of LTM_GAIN_MAP: LTM_RANGE_SCL
 * MTK_CAM_LTM_GAIN_MAP_LTM_RANGE_SCL: [24, 29]
 * scale of maxTran
 */
#define MTK_CAM_LTM_GAIN_MAP_LTM_RANGE_SCL_MASK   0x3f000000
#define MTK_CAM_LTM_GAIN_MAP_LTM_RANGE_SCL_SHIFT  24

/*
 * Bit Feild of LTM_CVNODE_GRP0: LTM_CVNODE_0
 * MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_0: [0, 11]
 * cvnode-0 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_0_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_0_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP0: LTM_CVNODE_1
 * MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_1: [16, 27]
 * cvnode-1 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_1_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP0_LTM_CVNODE_1_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP1: LTM_CVNODE_2
 * MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_2: [0, 11]
 * cvnode-2 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_2_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_2_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP1: LTM_CVNODE_3
 * MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_3: [16, 27]
 * cvnode-3 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_3_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP1_LTM_CVNODE_3_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP2: LTM_CVNODE_4
 * MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_4: [0, 11]
 * cvnode-4 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_4_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_4_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP2: LTM_CVNODE_5
 * MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_5: [16, 27]
 * cvnode-5 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_5_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP2_LTM_CVNODE_5_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP3: LTM_CVNODE_6
 * MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_6: [0, 11]
 * cvnode-6 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_6_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_6_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP3: LTM_CVNODE_7
 * MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_7: [16, 27]
 * cvnode-7 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_7_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP3_LTM_CVNODE_7_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP4: LTM_CVNODE_8
 * MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_8: [0, 11]
 * cvnode-8 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_8_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_8_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP4: LTM_CVNODE_9
 * MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_9: [16, 27]
 * cvnode-9 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_9_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP4_LTM_CVNODE_9_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP5: LTM_CVNODE_10
 * MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_10: [0, 11]
 * cvnode-10 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_10_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_10_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP5: LTM_CVNODE_11
 * MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_11: [16, 27]
 * cvnode-11 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_11_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP5_LTM_CVNODE_11_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP6: LTM_CVNODE_12
 * MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_12: [0, 11]
 * cvnode-12 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_12_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_12_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP6: LTM_CVNODE_13
 * MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_13: [16, 27]
 * cvnode-13 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_13_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP6_LTM_CVNODE_13_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP7: LTM_CVNODE_14
 * MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_14: [0, 11]
 * cvnode-14 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_14_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_14_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP7: LTM_CVNODE_15
 * MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_15: [16, 27]
 * cvnode-15 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_15_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP7_LTM_CVNODE_15_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP8: LTM_CVNODE_16
 * MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_16: [0, 11]
 * cvnode-16 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_16_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_16_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP8: LTM_CVNODE_17
 * MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_17: [16, 27]
 * cvnode-17 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_17_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP8_LTM_CVNODE_17_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP9: LTM_CVNODE_18
 * MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_18: [0, 11]
 * cvnode-18 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_18_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_18_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP9: LTM_CVNODE_19
 * MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_19: [16, 27]
 * cvnode-19 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_19_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP9_LTM_CVNODE_19_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP10: LTM_CVNODE_20
 * MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_20: [0, 11]
 * cvnode-20 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_20_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_20_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP10: LTM_CVNODE_21
 * MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_21: [16, 27]
 * cvnode-21 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_21_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP10_LTM_CVNODE_21_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP11: LTM_CVNODE_22
 * MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_22: [0, 11]
 * cvnode-22 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_22_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_22_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP11: LTM_CVNODE_23
 * MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_23: [16, 27]
 * cvnode-23 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_23_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP11_LTM_CVNODE_23_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP12: LTM_CVNODE_24
 * MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_24: [0, 11]
 * cvnode-24 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_24_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_24_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP12: LTM_CVNODE_25
 * MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_25: [16, 27]
 * cvnode-25 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_25_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP12_LTM_CVNODE_25_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP13: LTM_CVNODE_26
 * MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_26: [0, 11]
 * cvnode-26 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_26_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_26_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP13: LTM_CVNODE_27
 * MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_27: [16, 27]
 * cvnode-27 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_27_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP13_LTM_CVNODE_27_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP14: LTM_CVNODE_28
 * MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_28: [0, 11]
 * cvnode-28 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_28_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_28_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP14: LTM_CVNODE_29
 * MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_29: [16, 27]
 * cvnode-29 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_29_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP14_LTM_CVNODE_29_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP15: LTM_CVNODE_30
 * MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_30: [0, 11]
 * cvnode-30 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_30_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_30_SHIFT  0

/*
 * Bit Feild of LTM_CVNODE_GRP15: LTM_CVNODE_31
 * MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_31: [16, 27]
 * cvnode-31 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_31_MASK   0x0fff0000
#define MTK_CAM_LTM_CVNODE_GRP15_LTM_CVNODE_31_SHIFT  16

/*
 * Bit Feild of LTM_CVNODE_GRP16: LTM_CVNODE_32
 * MTK_CAM_LTM_CVNODE_GRP16_LTM_CVNODE_32: [0, 11]
 * cvnode-32 for gain map
 */
#define MTK_CAM_LTM_CVNODE_GRP16_LTM_CVNODE_32_MASK   0x00000fff
#define MTK_CAM_LTM_CVNODE_GRP16_LTM_CVNODE_32_SHIFT  0

/*
 * Bit Feild of LTM_OUT_STR: LTM_OUT_STR
 * MTK_CAM_LTM_OUT_STR_LTM_OUT_STR: [0, 4]
 * output strength
 */
#define MTK_CAM_LTM_OUT_STR_LTM_OUT_STR_MASK   0x0000001f
#define MTK_CAM_LTM_OUT_STR_LTM_OUT_STR_SHIFT  0

/*
 * Bit Feild of LTM_ACT_WIN_X: LTM_ACT_WIN_X_START
 * MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_START: [0, 15]
 * Horizontal setting for active window of AE debug
 */
#define MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_START_MASK   0x0000ffff
#define MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_START_SHIFT  0

/*
 * Bit Feild of LTM_ACT_WIN_X: LTM_ACT_WIN_X_END
 * MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_END: [16, 31]
 * Horizontal setting for active window of AE debug
 */
#define MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_END_MASK   0xffff0000
#define MTK_CAM_LTM_ACT_WIN_X_LTM_ACT_WIN_X_END_SHIFT  16

/*
 * Bit Feild of LTM_ACT_WIN_Y: LTM_ACT_WIN_Y_START
 * MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_START: [0, 15]
 * Vertical setting for active window of AE debug
 */
#define MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_START_MASK   0x0000ffff
#define MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_START_SHIFT  0

/*
 * Bit Feild of LTM_ACT_WIN_Y: LTM_ACT_WIN_Y_END
 * MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_END: [16, 31]
 * Vertical setting for active window of AE debug
 */
#define MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_END_MASK   0xffff0000
#define MTK_CAM_LTM_ACT_WIN_Y_LTM_ACT_WIN_Y_END_SHIFT  16

/*
 * Bit Feild of OBC_DBS: OBC_DBS_RATIO
 * MTK_CAM_OBC_DBS_OBC_DBS_RATIO: [0, 4]
 * Ratio of "bias" being eliminated
 */
#define MTK_CAM_OBC_DBS_OBC_DBS_RATIO_MASK   0x0000001f
#define MTK_CAM_OBC_DBS_OBC_DBS_RATIO_SHIFT  0

/*
 * Bit Feild of OBC_DBS: OBC_POSTTUNE_EN
 * MTK_CAM_OBC_DBS_OBC_POSTTUNE_EN: [8, 8]
 * Enable gray-blending and LUT-subtraction processing
 */
#define MTK_CAM_OBC_DBS_OBC_POSTTUNE_EN_MASK   0x00000100
#define MTK_CAM_OBC_DBS_OBC_POSTTUNE_EN_SHIFT  8

/*
 * Bit Feild of OBC_GRAY_BLD_0: OBC_LUMA_MODE
 * MTK_CAM_OBC_GRAY_BLD_0_OBC_LUMA_MODE: [0, 0]
 * Selection between max mode or mean mode for luma computation
 */
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_LUMA_MODE_MASK   0x00000001
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_LUMA_MODE_SHIFT  0

/*
 * Bit Feild of OBC_GRAY_BLD_0: OBC_GRAY_MODE
 * MTK_CAM_OBC_GRAY_BLD_0_OBC_GRAY_MODE: [1, 2]
 * Method of gray value computation
 */
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_GRAY_MODE_MASK   0x00000006
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_GRAY_MODE_SHIFT  1

/*
 * Bit Feild of OBC_GRAY_BLD_0: OBC_NORM_BIT
 * MTK_CAM_OBC_GRAY_BLD_0_OBC_NORM_BIT: [3, 7]
 * Data scale to be normalized to 12-bit
 */
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_NORM_BIT_MASK   0x000000f8
#define MTK_CAM_OBC_GRAY_BLD_0_OBC_NORM_BIT_SHIFT  3

/*
 * Bit Feild of OBC_GRAY_BLD_1: OBC_BLD_MXRT
 * MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_MXRT: [0, 7]
 * (normal and LE)Maximum weight for gray blending
 */
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_MXRT_MASK   0x000000ff
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_MXRT_SHIFT  0

/*
 * Bit Feild of OBC_GRAY_BLD_1: OBC_BLD_LOW
 * MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_LOW: [8, 19]
 * (normal and LE)Luma level below which the gray value is
 * bleneded with the specified maximum weight.
 */
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_LOW_MASK   0x000fff00
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_LOW_SHIFT  8

/*
 * Bit Feild of OBC_GRAY_BLD_1: OBC_BLD_SLP
 * MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_SLP: [20, 31]
 * (normal and LE)Slope of the blending ratio curve between zero and maximum weight.
 */
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_SLP_MASK   0xfff00000
#define MTK_CAM_OBC_GRAY_BLD_1_OBC_BLD_SLP_SHIFT  20

/*
 * Bit Feild of OBC_WBG_RB: OBC_PGN_R
 * MTK_CAM_OBC_WBG_RB_OBC_PGN_R: [0, 12]
 * WB gain for R channel
 */
#define MTK_CAM_OBC_WBG_RB_OBC_PGN_R_MASK   0x00001fff
#define MTK_CAM_OBC_WBG_RB_OBC_PGN_R_SHIFT  0

/*
 * Bit Feild of OBC_WBG_RB: OBC_PGN_B
 * MTK_CAM_OBC_WBG_RB_OBC_PGN_B: [16, 28]
 * WB gain for R channel
 */
#define MTK_CAM_OBC_WBG_RB_OBC_PGN_B_MASK   0x1fff0000
#define MTK_CAM_OBC_WBG_RB_OBC_PGN_B_SHIFT  16

/*
 * Bit Feild of OBC_WBG_G: OBC_PGN_G
 * MTK_CAM_OBC_WBG_G_OBC_PGN_G: [0, 12]
 * WB gain for G channel
 */
#define MTK_CAM_OBC_WBG_G_OBC_PGN_G_MASK   0x00001fff
#define MTK_CAM_OBC_WBG_G_OBC_PGN_G_SHIFT  0

/*
 * Bit Feild of OBC_WBIG_RB: OBC_IVGN_R
 * MTK_CAM_OBC_WBIG_RB_OBC_IVGN_R: [0, 9]
 * Inverse WB gain for R channel
 */
#define MTK_CAM_OBC_WBIG_RB_OBC_IVGN_R_MASK   0x000003ff
#define MTK_CAM_OBC_WBIG_RB_OBC_IVGN_R_SHIFT  0

/*
 * Bit Feild of OBC_WBIG_RB: OBC_IVGN_B
 * MTK_CAM_OBC_WBIG_RB_OBC_IVGN_B: [16, 25]
 * Inverse WB gain for B channel
 */
#define MTK_CAM_OBC_WBIG_RB_OBC_IVGN_B_MASK   0x03ff0000
#define MTK_CAM_OBC_WBIG_RB_OBC_IVGN_B_SHIFT  16

/*
 * Bit Feild of OBC_WBIG_G: OBC_IVGN_G
 * MTK_CAM_OBC_WBIG_G_OBC_IVGN_G: [0, 9]
 * Inverse WB gain for G channel
 */
#define MTK_CAM_OBC_WBIG_G_OBC_IVGN_G_MASK   0x000003ff
#define MTK_CAM_OBC_WBIG_G_OBC_IVGN_G_SHIFT  0

/*
 * Bit Feild of OBC_OBG_RB: OBC_GAIN_R
 * MTK_CAM_OBC_OBG_RB_OBC_GAIN_R: [0, 11]
 * OB gain for R channel
 */
#define MTK_CAM_OBC_OBG_RB_OBC_GAIN_R_MASK   0x00000fff
#define MTK_CAM_OBC_OBG_RB_OBC_GAIN_R_SHIFT  0

/*
 * Bit Feild of OBC_OBG_RB: OBC_GAIN_B
 * MTK_CAM_OBC_OBG_RB_OBC_GAIN_B: [16, 27]
 * OB gain for B channel
 */
#define MTK_CAM_OBC_OBG_RB_OBC_GAIN_B_MASK   0x0fff0000
#define MTK_CAM_OBC_OBG_RB_OBC_GAIN_B_SHIFT  16

/*
 * Bit Feild of OBC_OBG_G: OBC_GAIN_GR
 * MTK_CAM_OBC_OBG_G_OBC_GAIN_GR: [0, 11]
 * OB gain for Gr channel
 */
#define MTK_CAM_OBC_OBG_G_OBC_GAIN_GR_MASK   0x00000fff
#define MTK_CAM_OBC_OBG_G_OBC_GAIN_GR_SHIFT  0

/*
 * Bit Feild of OBC_OBG_G: OBC_GAIN_GB
 * MTK_CAM_OBC_OBG_G_OBC_GAIN_GB: [16, 27]
 * OB gain for Gb channel
 */
#define MTK_CAM_OBC_OBG_G_OBC_GAIN_GB_MASK   0x0fff0000
#define MTK_CAM_OBC_OBG_G_OBC_GAIN_GB_SHIFT  16

/*
 * Bit Feild of OBC_OFFSET_R: OBC_OFST_R
 * MTK_CAM_OBC_OFFSET_R_OBC_OFST_R: [0, 21]
 * OB offset for R channel
 */
#define MTK_CAM_OBC_OFFSET_R_OBC_OFST_R_MASK   0x003fffff
#define MTK_CAM_OBC_OFFSET_R_OBC_OFST_R_SHIFT  0

/*
 * Bit Feild of OBC_OFFSET_GR: OBC_OFST_GR
 * MTK_CAM_OBC_OFFSET_GR_OBC_OFST_GR: [0, 21]
 * OB offset for Gr channel
 */
#define MTK_CAM_OBC_OFFSET_GR_OBC_OFST_GR_MASK   0x003fffff
#define MTK_CAM_OBC_OFFSET_GR_OBC_OFST_GR_SHIFT  0

/*
 * Bit Feild of OBC_OFFSET_GB: OBC_OFST_GB
 * MTK_CAM_OBC_OFFSET_GB_OBC_OFST_GB: [0, 21]
 * OB offset for Gb channel
 */
#define MTK_CAM_OBC_OFFSET_GB_OBC_OFST_GB_MASK   0x003fffff
#define MTK_CAM_OBC_OFFSET_GB_OBC_OFST_GB_SHIFT  0

/*
 * Bit Feild of OBC_OFFSET_B: OBC_OFST_B
 * MTK_CAM_OBC_OFFSET_B_OBC_OFST_B: [0, 21]
 * OB offset for B channel
 */
#define MTK_CAM_OBC_OFFSET_B_OBC_OFST_B_MASK   0x003fffff
#define MTK_CAM_OBC_OFFSET_B_OBC_OFST_B_SHIFT  0

/*
 * Bit Feild of TSFS_DGAIN: TSFS_REGEN_Y_EN
 * MTK_CAM_TSFS_DGAIN_TSFS_REGEN_Y_EN: [0, 0]
 * Digital gain control
 */
#define MTK_CAM_TSFS_DGAIN_TSFS_REGEN_Y_EN_MASK   0x00000001
#define MTK_CAM_TSFS_DGAIN_TSFS_REGEN_Y_EN_SHIFT  0

/*
 * Bit Feild of TSFS_DGAIN: TSFS_GAIN
 * MTK_CAM_TSFS_DGAIN_TSFS_GAIN: [1, 16]
 * Digital gain
 */
#define MTK_CAM_TSFS_DGAIN_TSFS_GAIN_MASK   0x0001fffe
#define MTK_CAM_TSFS_DGAIN_TSFS_GAIN_SHIFT  1

/*
 *  V 4 L 2  M E T A  B U F F E R  L A Y O U T
 */

/*
 *  struct mtk_cam_uapi_meta_raw_stats_cfg
 *
 *  @ae_awb_enable: To indicate if AE and AWB should be enblaed or not. If
 *        it is 1, it means that we enable the following parts of
 *        hardware:
 *        (1) AE/AWB
 *        (2) aao
 *        (3) aaho
 *  @af_enable:     To indicate if AF should be enabled or not. If it is 1,
 *        it means that the AF and afo is enabled.
 *  @dgn_enable:    To indicate if dgn module should be enabled or not.
 *  @flk_enable:    If it is 1, it means flk and flko is enable. If ie is 0,
 *        both flk and flko is disabled.
 *  @tsf_enable:    If it is 1, it means tsfs and tsfso is enable. If ie is 0,
 *        both tsfs and tsfso is disabled.
 *  @wb_enable:     To indicate if wb module should be enabled or not.
 *  @pde_enable:    To indicate if pde module should be enabled or not.
 *  @ae_param:  AE Statistic window config
 *  @awb_param: AWB statistic configuration control
 *  @dgn_param: DGN settings
 *  @flk_param: Flicker statistic configuration
 *  @tsf_param: tsf statistic configuration
 *  @wb_param:  WB settings
 *  @pde_param: pde settings
 */
struct mtk_cam_uapi_meta_raw_stats_cfg {
	s8 ae_awb_enable;
	s8 af_enable;
	s8 dgn_enable;
	s8 flk_enable;
	s8 tsf_enable;
	s8 wb_enable;
	s8 pde_enable;
	u8 rsv;

	struct mtk_cam_uapi_ae_param ae_param;
	struct mtk_cam_uapi_awb_param awb_param;
	struct mtk_cam_uapi_af_param af_param;
	struct mtk_cam_uapi_dgn_param dgn_param;
	struct mtk_cam_uapi_flk_param flk_param;
	struct mtk_cam_uapi_tsf_param tsf_param;
	struct mtk_cam_uapi_wb_param wb_param;
	struct mtk_cam_uapi_pde_param pde_param;

	struct mtk_cam_uapi_prot {
		/* The following top control are generated by script */
		u8 drzh2n_r1_tuning_enable;
		u8 drzh2n_r2_tuning_enable;
		u8 drzh2n_r3_tuning_enable;
		u8 drzh2n_r4_tuning_enable;
		u8 drzh2n_r5_tuning_enable;
		u8 drzh2n_r6_tuning_enable;
		u8 drzh2n_r7_tuning_enable;
		u8 drzh2n_r8_tuning_enable;
		u8 drzs4n_r1_tuning_enable;
		u8 drzs4n_r2_tuning_enable;
		u8 drzs4n_r3_tuning_enable;
		u8 dm_tuning_enable;
		u8 drzs8t_r1_tuning_enable;
		u8 drzs8t_r2_tuning_enable;
		u8 ggm_r1_tuning_enable;
		u8 ggm_r2_tuning_enable;
		u8 ggm_r3_tuning_enable;
		u8 bpc_r1_enable;
		u8 bpc_r2_enable;
		u8 ccm_r1_enable;
		u8 ccm_r2_enable;
		u8 ccm_r3_enable;
		u8 fus_enable;
		u8 g2c_r1_enable;
		u8 g2c_r2_enable;
		u8 g2c_r3_enable;
		u8 hlr_enable;
		u8 lsc_enable;
		u8 ltm_enable;
		u8 ltms_enable;
		u8 obc_r1_enable;
		u8 obc_r2_enable;
		u8 tcy_r1_enable;
		u8 tcy_r2_enable;
		u8 tcy_r3_enable;
		u8 tncs_r1_enable;

		struct mtk_cam_uapi_ccm_param_prot ccm_r1_param;
		struct mtk_cam_uapi_ccm_param_prot ccm_r2_param;
		struct mtk_cam_uapi_ccm_param_prot ccm_r3_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r1_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r2_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r3_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r4_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r5_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r6_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r7_param;
		struct mtk_cam_uapi_drzh2n_param_prot drzh2n_r8_param;
		struct mtk_cam_uapi_drzs4n_param_prot drzs4n_r1_param;
		struct mtk_cam_uapi_drzs4n_param_prot drzs4n_r2_param;
		struct mtk_cam_uapi_drzs4n_param_prot drzs4n_r3_param;
		struct mtk_cam_uapi_tncs_param_prot tncs_param;
		/* script generation done */
		struct mtk_cam_uapi_drzs8t_param_prot drzs8t_r1_param;
		struct mtk_cam_uapi_drzs8t_param_prot drzs8t_r2_param;
		struct mtk_cam_uapi_awb_param_prot awb_param;
		struct mtk_cam_uapi_bpc_param_prot bpc_param;
		struct mtk_cam_uapi_lsc_param_prot lsc_param;
		struct mtk_cam_uapi_slk_param_prot slk_param;
		struct mtk_cam_uapi_wb_param_prot wb_param;
		struct mtk_cam_uapi_ltms_param_prot ltms_param;
		struct mtk_cam_uapi_yuvo_param_prot yuvo_r2_param;
		struct mtk_cam_uapi_yuvo_param_prot yuvo_r4_param;
		/* The following module stuctures are generated by script */
		struct mtk_cam_uapi_regmap_raw_bpc bpc_r1;
		struct mtk_cam_uapi_regmap_raw_bpc bpc_r2;
		struct mtk_cam_uapi_regmap_raw_ccm ccm_r1;
		struct mtk_cam_uapi_regmap_raw_ccm ccm_r2;
		struct mtk_cam_uapi_regmap_raw_ccm ccm_r3;
		struct mtk_cam_uapi_regmap_raw_dm dm_r1;
		u8 rsv1[116];
		struct mtk_cam_uapi_regmap_raw_g2c g2c_r1;
		struct mtk_cam_uapi_regmap_raw_g2c g2c_r2;
		struct mtk_cam_uapi_regmap_raw_g2c g2c_r3;
		struct mtk_cam_uapi_regmap_raw_ggm ggm_r1;
		struct mtk_cam_uapi_regmap_raw_ggm ggm_r2;
		struct mtk_cam_uapi_regmap_raw_ggm ggm_r3;
		u8 rsv2[68];
		struct mtk_cam_uapi_regmap_raw_lsc lsc_r1;
		struct mtk_cam_uapi_regmap_raw_ltm ltm_r1;
		struct mtk_cam_uapi_regmap_raw_ltms ltms_r1;
		struct mtk_cam_uapi_regmap_raw_obc obc_r1;
		struct mtk_cam_uapi_regmap_raw_obc obc_r2;
		u8 rsv3[1420];
		struct mtk_cam_uapi_regmap_raw_tsfs tsfs_r1;
		u8 rsv4[50080];
		/* script generation done */
	} prot __packed;
} __packed;

/*
 * struct mtk_cam_uapi_meta_raw_stats_0 - capture buffer returns from camsys
 *    after the frame is done. The buffer are not be pushed the other
 *    driver such as dip.
 *
 * @ae_awb_stats_enabled: indicate that ae_awb_stats is ready or not in
 *       this buffer
 * @ltm_stats_enabled:    indicate that ltm_stats is ready or not in
 *       this buffer
 * @flk_stats_enabled:    indicate that flk_stats is ready or not in
 *       this buffer
 * @tsf_stats_enabled:    indicate that tsf_stats is ready or not in
 *       this buffer
 * @pde_stats_enabled:    indicate that pde_stats is ready or not in
 *       this buffer
 * @pipeline_config:      the pipeline configuration during processing
 * @pde_stats: the pde module stats
 */
struct mtk_cam_uapi_meta_raw_stats_0 {
	u8 ae_awb_stats_enabled;
	u8 ltm_stats_enabled;
	u8 flk_stats_enabled;
	u8 tsf_stats_enabled;
	u8 tncy_stats_enabled;
	u8 pde_stats_enabled;
	u8 rsv[2];

	struct mtk_cam_uapi_pipeline_config pipeline_config;

	struct mtk_cam_uapi_ae_awb_stats ae_awb_stats;
	struct mtk_cam_uapi_ltm_stats ltm_stats;
	struct mtk_cam_uapi_flk_stats flk_stats;
	struct mtk_cam_uapi_tsf_stats tsf_stats;
	struct mtk_cam_uapi_tncy_stats tncy_stats;
	struct mtk_cam_uapi_pd_stats pde_stats;
	struct mtk_cam_uapi_timestamp timestamp;
} __packed;

/*
 * struct mtk_cam_uapi_meta_raw_stats_1 - statistics before frame done
 *
 * @af_stats_enabled: indicate that lce_stats is ready or not in this buffer
 * @af_stats: AF statistics
 *
 * Any statistic output put in this structure should be careful.
 * The meta buffer needs copying overhead to return the buffer before the
 * all the ISP hardware's processing is finished.
 */
struct mtk_cam_uapi_meta_raw_stats_1 {
	u8 af_stats_enabled;
	u8 af_qbn_r6_enabled;
	u8 rsv[2];
	struct mtk_cam_uapi_af_stats af_stats;
} __packed;

/*
 * struct mtk_cam_uapi_meta_raw_stats_2 - shared statistics buffer
 *
 * @act_stats_enabled:  indicate that act_stats is ready or not in this
 * buffer
 * @act_stats:  act statistics
 *
 * The statistic output in this structure may be pushed to the other
 * driver such as dip.
 *
 */
struct mtk_cam_uapi_meta_raw_stats_2 {
	u8 act_stats_enabled;
	u8 rsv[3];

	struct mtk_cam_uapi_act_stats act_stats;
} __packed;

/*
 * struct mtk_cam_uapi_meta_camsv_stats_0 - capture buffer returns from
 *	 camsys's camsv module after the frame is done. The buffer are
 *	 not be pushed the other driver such as dip.
 *
 * @pd_stats_enabled:	 indicate that pd_stats is ready or not in
 *			 this buffer
 */
struct mtk_cam_uapi_meta_camsv_stats_0 {
	u8   pd_stats_enabled;
	u8   rsv[3];

	struct mtk_cam_uapi_pd_stats pd_stats;
} __packed;

#define MTK_CAM_META_VERSION_MAJOR 2
#define MTK_CAM_META_VERSION_MINOR 3
#define MTK_CAM_META_PLATFORM_NAME "isp71"
#define MTK_CAM_META_CHIP_NAME "mt8188"

#endif /* __MTK_CAM_META_H__ */
