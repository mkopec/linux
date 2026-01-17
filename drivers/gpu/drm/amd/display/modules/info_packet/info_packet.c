/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */

#include "mod_info_packet.h"
#include "core_types.h"
#include "dc_types.h"
#include "mod_shared.h"
#include "mod_freesync.h"
#include "dc.h"

enum vsc_packet_revision {
	vsc_packet_undefined = 0,
	//01h = VSC SDP supports only 3D stereo.
	vsc_packet_rev1 = 1,
	//02h = 3D stereo + PSR.
	vsc_packet_rev2 = 2,
	//03h = 3D stereo + PSR2.
	vsc_packet_rev3 = 3,
	//04h = 3D stereo + PSR/PSR2 + Y-coordinate.
	vsc_packet_rev4 = 4,
	//05h = 3D stereo + PSR/PSR2 + Y-coordinate + Pixel Encoding/Colorimetry Format
	vsc_packet_rev5 = 5,
	//06h = 3D stereo + PR + Y-coordinate
	vsc_packet_rev6 = 6,
	//07h = 3D stereo + PR + Y-coordinate + Pixel Encoding/Colorimetry Format
	vsc_packet_rev7 = 7,
};

#define HDMI_INFOFRAME_TYPE_EMP    0x7F
#define HDMI_INFOFRAME_TYPE_VENDOR 0x81
#define HDMI_INFOFRAME_LENGTH_MASK 0x1F
#define HF_VSIF_VERSION  1
#define HF_VSIF_3D_BIT   0
#define HF_VSIF_ALLM_BIT 1

// VTEM Byte Offset
#define VTEM_PB0		0
#define VTEM_PB1		1
#define VTEM_PB2		2
#define VTEM_PB3		3
#define VTEM_PB4		4
#define VTEM_PB5		5
#define VTEM_PB6		6

#define VTEM_ORG_ID          1
#define VTEM_DATA_SET_TAG    1
#define VTEM_DATA_SET_LENGTH 4

#define VTEM_M_CONST    0
#define VTEM_FVA_FACTOR 0

#define VTEM_BRR_MASK_UPPER 0x03
#define VTEM_BRR_MASK_LOWER 0xFF

/* VTEM Byte Offset */
#define VTEM_PB0 0
#define VTEM_PB1 1
#define VTEM_PB2 2
#define VTEM_PB3 3
#define VTEM_PB4 4
#define VTEM_PB5 5
#define VTEM_PB6 6

#define VTEM_MD0 7
#define VTEM_MD1 8
#define VTEM_MD2 9
#define VTEM_MD3 10

/* Extended Metadata Packet */
/* Header */
#define EMP_LAST_BIT  6
#define EMP_FIRST_BIT 7
/* PB0 */
#define EMP_SNC_BIT 1
#define EMP_VFR_BIT 2
#define EMP_AFR_BIT 3
#define EMP_DST_BIT 4
#define EMP_END_BIT 6
#define EMP_NEW_BIT 7
/* PB7 = MD0 */
#define VTEM_VRR_BIT     0
#define VTEM_M_CONST_BIT 1
#define VTEM_FVA_BIT     4
/* MD1 Base_Vfront */
/* MD2 */
#define VTEM_BRR_UPPER_BIT 0
#define VTEM_RB_BIT        2
/* MD3 BRR Lower */


enum ColorimetryRGBDP {
	ColorimetryRGB_DP_sRGB               = 0,
	ColorimetryRGB_DP_AdobeRGB           = 3,
	ColorimetryRGB_DP_P3                 = 4,
	ColorimetryRGB_DP_CustomColorProfile = 5,
	ColorimetryRGB_DP_ITU_R_BT2020RGB    = 6,
};
enum ColorimetryYCCDP {
	ColorimetryYCC_DP_ITU601        = 0,
	ColorimetryYCC_DP_ITU709        = 1,
	ColorimetryYCC_DP_AdobeYCC      = 5,
	ColorimetryYCC_DP_ITU2020YCC    = 6,
	ColorimetryYCC_DP_ITU2020YCbCr  = 7,
};

/* Helper function to set VSC packet colorimetry data */
void set_vsc_packet_colorimetry_data(
		const struct dc_stream_state *stream,
		struct dc_info_packet *info_packet,
		enum dc_color_space cs,
		enum color_transfer_func tf)
{
	/* Set VSC SDP fields for pixel encoding and colorimetry format from DP 1.3 specs
	 * Data Bytes DB 18~16
	 * Bits 3:0 (Colorimetry Format)        |  Bits 7:4 (Pixel Encoding)
	 * ----------------------------------------------------------------------------------------------------
	 * 0x0 = sRGB                           |  0 = RGB
	 * 0x1 = RGB Wide Gamut Fixed Point
	 * 0x2 = RGB Wide Gamut Floating Point
	 * 0x3 = AdobeRGB
	 * 0x4 = DCI-P3
	 * 0x5 = CustomColorProfile
	 * (others reserved)
	 * ----------------------------------------------------------------------------------------------------
	 * 0x0 = ITU-R BT.601                   |  1 = YCbCr444
	 * 0x1 = ITU-R BT.709
	 * 0x2 = xvYCC601
	 * 0x3 = xvYCC709
	 * 0x4 = sYCC601
	 * 0x5 = AdobeYCC601
	 * 0x6 = ITU-R BT.2020 Y'cC'bcC'rc
	 * 0x7 = ITU-R BT.2020 Y'C'bC'r
	 * (others reserved)
	 * ----------------------------------------------------------------------------------------------------
	 * 0x0 = ITU-R BT.601                   |  2 = YCbCr422
	 * 0x1 = ITU-R BT.709
	 * 0x2 = xvYCC601
	 * 0x3 = xvYCC709
	 * 0x4 = sYCC601
	 * 0x5 = AdobeYCC601
	 * 0x6 = ITU-R BT.2020 Y'cC'bcC'rc
	 * 0x7 = ITU-R BT.2020 Y'C'bC'r
	 * (others reserved)
	 * ----------------------------------------------------------------------------------------------------
	 * 0x0 = ITU-R BT.601                   |  3 = YCbCr420
	 * 0x1 = ITU-R BT.709
	 * 0x2 = xvYCC601
	 * 0x3 = xvYCC709
	 * 0x4 = sYCC601
	 * 0x5 = AdobeYCC601
	 * 0x6 = ITU-R BT.2020 Y'cC'bcC'rc
	 * 0x7 = ITU-R BT.2020 Y'C'bC'r
	 * (others reserved)
	 * ----------------------------------------------------------------------------------------------------
	 * 0x0 =DICOM Part14 Grayscale          |  4 = Yonly
	 * Display Function
	 * (others reserved)
	 */
	unsigned int pixelEncoding = 0;
	unsigned int colorimetryFormat = 0;

	/* Set Pixel Encoding */
	switch (stream->timing.pixel_encoding) {
	case PIXEL_ENCODING_RGB:
		pixelEncoding = 0x0;  /* RGB = 0h */
		break;
	case PIXEL_ENCODING_YCBCR444:
		pixelEncoding = 0x1;  /* YCbCr444 = 1h */
		break;
	case PIXEL_ENCODING_YCBCR422:
		pixelEncoding = 0x2;  /* YCbCr422 = 2h */
		break;
	case PIXEL_ENCODING_YCBCR420:
		pixelEncoding = 0x3;  /* YCbCr420 = 3h */
		break;
	default:
		pixelEncoding = 0x0;  /* default RGB = 0h */
		break;
	}

	/* Set Colorimetry format based on pixel encoding */
	switch (stream->timing.pixel_encoding) {
	case PIXEL_ENCODING_RGB:
		if ((cs == COLOR_SPACE_SRGB) ||
				(cs == COLOR_SPACE_SRGB_LIMITED))
			colorimetryFormat = ColorimetryRGB_DP_sRGB;
		else if (cs == COLOR_SPACE_ADOBERGB)
			colorimetryFormat = ColorimetryRGB_DP_AdobeRGB;
		else if ((cs == COLOR_SPACE_2020_RGB_FULLRANGE) ||
				(cs == COLOR_SPACE_2020_RGB_LIMITEDRANGE))
			colorimetryFormat = ColorimetryRGB_DP_ITU_R_BT2020RGB;
		break;

	case PIXEL_ENCODING_YCBCR444:
	case PIXEL_ENCODING_YCBCR422:
	case PIXEL_ENCODING_YCBCR420:
		/* Note: xvYCC probably not supported correctly here on DP since colorspace translation
		 * loses distinction between BT601 vs xvYCC601 in translation
		 */
		if (cs == COLOR_SPACE_YCBCR601)
			colorimetryFormat = ColorimetryYCC_DP_ITU601;
		else if (cs == COLOR_SPACE_YCBCR709)
			colorimetryFormat = ColorimetryYCC_DP_ITU709;
		else if (cs == COLOR_SPACE_ADOBERGB)
			colorimetryFormat = ColorimetryYCC_DP_AdobeYCC;
		else if (cs == COLOR_SPACE_2020_YCBCR_LIMITED)
			colorimetryFormat = ColorimetryYCC_DP_ITU2020YCbCr;

		if (cs == COLOR_SPACE_2020_YCBCR_LIMITED && tf == TRANSFER_FUNC_GAMMA_22)
			colorimetryFormat = ColorimetryYCC_DP_ITU709;
		break;

	default:
		colorimetryFormat = ColorimetryRGB_DP_sRGB;
		break;
	}

	info_packet->sb[16] = (pixelEncoding << 4) | colorimetryFormat;

	/* Set color depth */
	switch (stream->timing.display_color_depth) {
	case COLOR_DEPTH_666:
		/* NOTE: This is actually not valid for YCbCr pixel encoding to have 6 bpc
		 *       as of DP1.4 spec, but value of 0 probably reserved here for potential future use.
		 */
		info_packet->sb[17] = 0;
		break;
	case COLOR_DEPTH_888:
		info_packet->sb[17] = 1;
		break;
	case COLOR_DEPTH_101010:
		info_packet->sb[17] = 2;
		break;
	case COLOR_DEPTH_121212:
		info_packet->sb[17] = 3;
		break;
	/*case COLOR_DEPTH_141414: -- NO SUCH FORMAT IN DP SPEC */
	case COLOR_DEPTH_161616:
		info_packet->sb[17] = 4;
		break;
	default:
		info_packet->sb[17] = 0;
		break;
	}

	/* all YCbCr are always limited range */
	if ((cs == COLOR_SPACE_SRGB_LIMITED) ||
			(cs == COLOR_SPACE_2020_RGB_LIMITEDRANGE) ||
			(pixelEncoding != 0x0)) {
		info_packet->sb[17] |= 0x80; /* DB17 bit 7 set to 1 for CEA timing. */
	}

	/* Content Type (Bits 2:0)
	 *  0 = Not defined.
	 *  1 = Graphics.
	 *  2 = Photo.
	 *  3 = Video.
	 *  4 = Game.
	 */
	info_packet->sb[18] = 0;
}

void mod_build_vsc_infopacket(const struct dc_stream_state *stream,
		struct dc_info_packet *info_packet,
		enum dc_color_space cs,
		enum color_transfer_func tf)
{
	unsigned int vsc_packet_revision = vsc_packet_undefined;
	unsigned int i;
	bool stereo3dSupport = false;

	if (stream->timing.timing_3d_format != TIMING_3D_FORMAT_NONE && stream->view_format != VIEW_3D_FORMAT_NONE) {
		vsc_packet_revision = vsc_packet_rev1;
		stereo3dSupport = true;
	}

	/* VSC packet set to 4 for PSR-SU, or 2 for PSR1 */
	if (stream->link->psr_settings.psr_version == DC_PSR_VERSION_SU_1)
		vsc_packet_revision = vsc_packet_rev4;
	else if (stream->link->replay_settings.config.replay_supported)
		vsc_packet_revision = vsc_packet_rev4;
	else if (stream->link->psr_settings.psr_version == DC_PSR_VERSION_1)
		vsc_packet_revision = vsc_packet_rev2;

	/* Update to revision 5 for extended colorimetry support */
	if (stream->use_vsc_sdp_for_colorimetry)
		vsc_packet_revision = vsc_packet_rev5;

	/* Check for Panel Replay (highest priority) */
	if (stream->link->replay_settings.config.replay_version == DC_VESA_PANEL_REPLAY)
		vsc_packet_revision = stream->use_vsc_sdp_for_colorimetry ? vsc_packet_rev7 : vsc_packet_rev6;

	/* VSC packet not needed based on the features
	 * supported by this DP display
	 */
	if (vsc_packet_revision == vsc_packet_undefined)
		return;

	if (vsc_packet_revision == vsc_packet_rev6) {
		/* Secondary-data Packet ID = 0*/
		info_packet->hb0 = 0x00;
		/* 07h - Packet Type Value indicating Video
		 * Stream Configuration packet
		 */
		info_packet->hb1 = 0x07;
		/* 06h = VSC SDP supporting 3D stereo + PR
		 */
		info_packet->hb2 = 0x06;
		/* 0Eh = VSC SDP supporting 3D stereo + PR
		 * (HB2 = 06h), with Y-coordinate of first scan
		 * line of the SU region
		 */
		info_packet->hb3 = 0x10;

		for (i = 0; i < 28; i++)
			info_packet->sb[i] = 0;

		info_packet->valid = true;
	}

	if (vsc_packet_revision == vsc_packet_rev4) {
		/* Secondary-data Packet ID = 0*/
		info_packet->hb0 = 0x00;
		/* 07h - Packet Type Value indicating Video
		 * Stream Configuration packet
		 */
		info_packet->hb1 = 0x07;
		/* 04h = VSC SDP supporting 3D stereo + PSR/PSR2 + Y-coordinate
		 * (applies to eDP v1.4 or higher).
		 */
		info_packet->hb2 = 0x04;
		/* 0Eh = VSC SDP supporting 3D stereo + PSR2
		 * (HB2 = 04h), with Y-coordinate of first scan
		 * line of the SU region
		 */
		info_packet->hb3 = 0x0E;

		for (i = 0; i < 28; i++)
			info_packet->sb[i] = 0;

		info_packet->valid = true;
	}

	if (vsc_packet_revision == vsc_packet_rev2) {
		/* Secondary-data Packet ID = 0*/
		info_packet->hb0 = 0x00;
		/* 07h - Packet Type Value indicating Video
		 * Stream Configuration packet
		 */
		info_packet->hb1 = 0x07;
		/* 02h = VSC SDP supporting 3D stereo and PSR
		 * (applies to eDP v1.3 or higher).
		 */
		info_packet->hb2 = 0x02;
		/* 08h = VSC packet supporting 3D stereo + PSR
		 * (HB2 = 02h).
		 */
		info_packet->hb3 = 0x08;

		for (i = 0; i < 28; i++)
			info_packet->sb[i] = 0;

		info_packet->valid = true;
	}

	if (vsc_packet_revision == vsc_packet_rev1) {

		info_packet->hb0 = 0x00;	// Secondary-data Packet ID = 0
		info_packet->hb1 = 0x07;	// 07h = Packet Type Value indicating Video Stream Configuration packet
		info_packet->hb2 = 0x01;	// 01h = Revision number. VSC SDP supporting 3D stereo only
		info_packet->hb3 = 0x01;	// 01h = VSC SDP supporting 3D stereo only (HB2 = 01h).

		info_packet->valid = true;
	}

	if (stereo3dSupport) {
		/* ==============================================================================================================|
		 * A. STEREO 3D
		 * ==============================================================================================================|
		 * VSC Payload (1 byte) From DP1.2 spec
		 *
		 * Bits 3:0 (Stereo Interface Method Code)  |  Bits 7:4 (Stereo Interface Method Specific Parameter)
		 * -----------------------------------------------------------------------------------------------------
		 * 0 = Non Stereo Video                     |  Must be set to 0x0
		 * -----------------------------------------------------------------------------------------------------
		 * 1 = Frame/Field Sequential               |  0x0: L + R view indication based on MISC1 bit 2:1
		 *                                          |  0x1: Right when Stereo Signal = 1
		 *                                          |  0x2: Left when Stereo Signal = 1
		 *                                          |  (others reserved)
		 * -----------------------------------------------------------------------------------------------------
		 * 2 = Stacked Frame                        |  0x0: Left view is on top and right view on bottom
		 *                                          |  (others reserved)
		 * -----------------------------------------------------------------------------------------------------
		 * 3 = Pixel Interleaved                    |  0x0: horiz interleaved, right view pixels on even lines
		 *                                          |  0x1: horiz interleaved, right view pixels on odd lines
		 *                                          |  0x2: checker board, start with left view pixel
		 *                                          |  0x3: vertical interleaved, start with left view pixels
		 *                                          |  0x4: vertical interleaved, start with right view pixels
		 *                                          |  (others reserved)
		 * -----------------------------------------------------------------------------------------------------
		 * 4 = Side-by-side                         |  0x0: left half represents left eye view
		 *                                          |  0x1: left half represents right eye view
		 */
		switch (stream->timing.timing_3d_format) {
		case TIMING_3D_FORMAT_HW_FRAME_PACKING:
		case TIMING_3D_FORMAT_SW_FRAME_PACKING:
		case TIMING_3D_FORMAT_TOP_AND_BOTTOM:
		case TIMING_3D_FORMAT_TB_SW_PACKED:
			info_packet->sb[0] = 0x02; // Stacked Frame, Left view is on top and right view on bottom.
			break;
		case TIMING_3D_FORMAT_DP_HDMI_INBAND_FA:
		case TIMING_3D_FORMAT_INBAND_FA:
			info_packet->sb[0] = 0x01; // Frame/Field Sequential, L + R view indication based on MISC1 bit 2:1
			break;
		case TIMING_3D_FORMAT_SIDE_BY_SIDE:
		case TIMING_3D_FORMAT_SBS_SW_PACKED:
			info_packet->sb[0] = 0x04; // Side-by-side
			break;
		default:
			info_packet->sb[0] = 0x00; // No Stereo Video, Shall be cleared to 0x0.
			break;
		}

	}

	/* 05h = VSC SDP supporting 3D stereo, PSR2, and Pixel Encoding/Colorimetry Format indication.
	 *   Added in DP1.3, a DP Source device is allowed to indicate the pixel encoding/colorimetry
	 *   format to the DP Sink device with VSC SDP only when the DP Sink device supports it
	 *   (i.e., VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED bit in the DPRX_FEATURE_ENUMERATION_LIST
	 *   register (DPCD Address 02210h, bit 3) is set to 1).
	 *   (Requires VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED bit set to 1 in DPCD 02210h. This
	 *   DPCD register is exposed in the new Extended Receiver Capability field for DPCD Rev. 1.4
	 *   (and higher). When MISC1. bit 6. is Set to 1, a Source device uses a VSC SDP to indicate
	 *   the Pixel Encoding/Colorimetry Format and that a Sink device must ignore MISC1, bit 7, and
	 *   MISC0, bits 7:1 (MISC1, bit 7. and MISC0, bits 7:1 become "don't care").)
	 */
	if (vsc_packet_revision == vsc_packet_rev5) {
		/* Secondary-data Packet ID = 0 */
		info_packet->hb0 = 0x00;
		/* 07h - Packet Type Value indicating Video Stream Configuration packet */
		info_packet->hb1 = 0x07;
		/* 05h = VSC SDP supporting 3D stereo, PSR2, and Pixel Encoding/Colorimetry Format indication. */
		info_packet->hb2 = 0x05;
		/* 13h = VSC SDP supporting 3D stereo, + PSR2, + Pixel Encoding/Colorimetry Format indication (HB2 = 05h). */
		info_packet->hb3 = 0x13;

		info_packet->valid = true;

		set_vsc_packet_colorimetry_data(stream, info_packet, cs, tf);
	}

	if (vsc_packet_revision == vsc_packet_rev7) {
		/* Secondary-data Packet ID = 0 */
		info_packet->hb0 = 0x00;
		/* 07h - Packet Type Value indicating Video Stream Configuration packet */
		info_packet->hb1 = 0x07;
		/* 07h = VSC SDP supporting 3D stereo, PR, and Pixel Encoding/Colorimetry Format indication. */
		info_packet->hb2 = 0x07;
		/* 13h = VSC SDP supporting 3D stereo, + PR, + Pixel Encoding/Colorimetry Format indication (HB2 = 07h). */
		info_packet->hb3 = 0x13;

		info_packet->valid = true;

		set_vsc_packet_colorimetry_data(stream, info_packet, cs, tf);
	}
}

static bool is_hdmi_vic_mode(const struct dc_stream_state *stream)
{
	if (stream->timing.hdmi_vic == 0)
		return false;

	if (stream->timing.h_total < 3840 ||
	    stream->timing.v_total < 2160)
		return false;

	/* 3D/ALLM forces HDMI VIC -> CTA VIC translation */
	if (stream->view_format != VIEW_3D_FORMAT_NONE)
		return false;

	if (stream->link->local_sink->edid_caps.allm)
		return false;

	return true;
}

/**
 *  mod_build_hf_vsif_infopacket - Prepare HDMI Vendor Specific info frame.
 *                                 Follows HDMI Spec to build up Vendor Specific info frame
 *                                 Conforms to h14b-vsif or hf-vsif based on the capabilities
 *
 *  @stream:      contains data we may need to construct VSIF (i.e. timing_3d_format, etc.)
 *  @info_packet: output structure where to store VSIF
 */
void mod_build_hf_vsif_infopacket(const struct dc_stream_state *stream,
		struct dc_info_packet *info_packet)
{
		bool hdmi_vic_mode = false;
		bool allm = false;
		bool stereo = false;
		uint8_t checksum = 0;
		uint8_t offset = 0;
		uint8_t i = 0;
		uint8_t length = 5;
		uint32_t oui = HDMI_IEEE_OUI;
		enum dc_timing_3d_format format;

		info_packet->valid = false;

		allm = stream->link->local_sink->edid_caps.allm;
		format = stream->view_format == VIEW_3D_FORMAT_NONE ?
			 TIMING_3D_FORMAT_NONE :
			 stream->timing.timing_3d_format;
		stereo = format != TIMING_3D_FORMAT_NONE;
		hdmi_vic_mode = is_hdmi_vic_mode(stream);

		if (!stereo && !hdmi_vic_mode && !allm)
			return;

		if (allm)
			oui = HDMI_FORUM_IEEE_OUI;

		info_packet->sb[1] = oui & 0xFF;
		info_packet->sb[2] = (oui >> 8) & 0xFF;
		info_packet->sb[3] = (oui >> 16) & 0xFF;

		if (oui == HDMI_FORUM_IEEE_OUI) {
			offset = 2;
			length += 2;
			info_packet->sb[4] = HF_VSIF_VERSION;
			info_packet->sb[5] = stereo << HF_VSIF_3D_BIT;
			info_packet->sb[5] |= allm << HF_VSIF_ALLM_BIT;
		}

		if (stereo) {
			info_packet->sb[4 + offset] = (2 << 5);

			switch (format) {
			case TIMING_3D_FORMAT_HW_FRAME_PACKING:
			case TIMING_3D_FORMAT_SW_FRAME_PACKING:
				info_packet->sb[5 + offset] = (0x0 << 4);
				break;

			case TIMING_3D_FORMAT_SIDE_BY_SIDE:
			case TIMING_3D_FORMAT_SBS_SW_PACKED:
				info_packet->sb[5 + offset] = (0x8 << 4);
				++length;
				break;

			case TIMING_3D_FORMAT_TOP_AND_BOTTOM:
			case TIMING_3D_FORMAT_TB_SW_PACKED:
				info_packet->sb[5 + offset] = (0x6 << 4);
				break;

			default:
				break;
			}

		/* Doesn't need the offset as it can't be used with hf-vsif */
		} else if (hdmi_vic_mode) {
			info_packet->sb[4] = (1 << 5);
			info_packet->sb[5] = stream->timing.hdmi_vic;
		}

		info_packet->hb0 = HDMI_INFOFRAME_TYPE_VENDOR;
		info_packet->hb1 = 0x01;
		info_packet->hb2 = length & HDMI_INFOFRAME_LENGTH_MASK;

		checksum += info_packet->hb0;
		checksum += info_packet->hb1;
		checksum += info_packet->hb2;

		for (i = 1; i <= length; i++)
			checksum += info_packet->sb[i];

		info_packet->sb[0] = (uint8_t) (0x100 - checksum);

		info_packet->valid = true;
}

static void build_vtem_infopacket_header(struct dc_info_packet *infopacket)
{
	uint8_t pb0 = 0;

	/* might need logic in the future */
	pb0 |= 0 << EMP_SNC_BIT;
	pb0 |= 1 << EMP_VFR_BIT;
	pb0 |= 0 << EMP_AFR_BIT;
	pb0 |= 0 << EMP_DST_BIT;
	pb0 |= 0 << EMP_END_BIT;
	pb0 |= 1 << EMP_NEW_BIT;

	infopacket->hb0 = HDMI_INFOFRAME_TYPE_EMP;
	infopacket->hb1 = (1 << EMP_FIRST_BIT) | (1 << EMP_LAST_BIT);
	infopacket->hb2 = 0; // sequence

	infopacket->sb[VTEM_PB0] = pb0;
	infopacket->sb[VTEM_PB2] = VTEM_ORG_ID;
	infopacket->sb[VTEM_PB4] = VTEM_DATA_SET_TAG;
	infopacket->sb[VTEM_PB6] = VTEM_DATA_SET_LENGTH;
}

static void build_vtem_infopacket_data(const struct dc_stream_state *stream,
		const struct mod_vrr_params *vrr,
		struct dc_info_packet *infopacket)
{
	unsigned int hblank = 0;
	unsigned int brr = 0;
	bool vrr_active = false;
	bool rb = false;

	vrr_active = vrr->state == VRR_STATE_ACTIVE_VARIABLE ||
		     vrr->state == VRR_STATE_ACTIVE_FIXED;

	infopacket->sb[VTEM_MD0] = VTEM_M_CONST << VTEM_M_CONST_BIT;
	infopacket->sb[VTEM_MD0] |= VTEM_FVA_FACTOR << VTEM_FVA_BIT;
	infopacket->sb[VTEM_MD0] |= vrr_active << VTEM_VRR_BIT;

	infopacket->sb[VTEM_MD1] = 0;
	infopacket->sb[VTEM_MD2] = 0;
	infopacket->sb[VTEM_MD3] = 0;

	if (!vrr_active || is_hdmi_vic_mode(stream))
		return;
	/*
	 * In accordance with CVT 1.2 and CVT 2.1:
	 * Reduced Blanking standard defines a fixed value of
	 * 160 for hblank, further reduced to 80 in RB2. RB3 uses
	 * fixed hblank of 80 pixels + up to 120 additional pixels
	 * in 8-pixel steps.
	 */
	hblank = stream->timing.h_total - stream->timing.h_addressable;
	rb = (hblank >= 80 && hblank <= 200 && hblank % 8 == 0);
	brr = div_u64(mod_freesync_calc_nominal_field_rate(stream), 1000000);

	if (brr > VTEM_BRR_MAX) {
		infopacket->valid = false;
		return;
	}

	infopacket->sb[VTEM_MD1] = (uint8_t) stream->timing.v_front_porch;
	infopacket->sb[VTEM_MD2] = rb << VTEM_RB_BIT;
	infopacket->sb[VTEM_MD2] |= (brr >> 8) & VTEM_BRR_MASK_UPPER;
	infopacket->sb[VTEM_MD3] = brr & VTEM_BRR_MASK_LOWER;
}

void mod_build_vtem_infopacket(const struct dc_stream_state *stream,
		const struct mod_vrr_params *vrr,
		struct dc_info_packet *infopacket)
{
	infopacket->valid = true;
	build_vtem_infopacket_header(infopacket);
	build_vtem_infopacket_data(stream, vrr, infopacket);
}

void mod_build_adaptive_sync_infopacket(const struct dc_stream_state *stream,
		enum adaptive_sync_type asType,
		const struct AS_Df_params *param,
		struct dc_info_packet *info_packet)
{
	info_packet->valid = false;

	memset(info_packet, 0, sizeof(struct dc_info_packet));

	switch (asType) {
	case ADAPTIVE_SYNC_TYPE_DP:
		if (stream != NULL)
			mod_build_adaptive_sync_infopacket_v2(stream, param, info_packet);
		break;
	case ADAPTIVE_SYNC_TYPE_PCON_ALLOWED:
	case ADAPTIVE_SYNC_TYPE_EDP:
		if (stream && stream->link->replay_settings.config.replay_supported &&
			stream->link->replay_settings.config.replay_version == DC_VESA_PANEL_REPLAY)
			mod_build_adaptive_sync_infopacket_v2(stream, param, info_packet);
		else
			mod_build_adaptive_sync_infopacket_v1(info_packet);
		break;
	case ADAPTIVE_SYNC_TYPE_NONE:
	case ADAPTIVE_SYNC_TYPE_PCON_NOT_ALLOWED:
	case ADAPTIVE_SYNC_TYPE_HDMI:
	default:
		break;
	}
}

void mod_build_adaptive_sync_infopacket_v1(struct dc_info_packet *info_packet)
{
	info_packet->valid = true;
	// HEADER {HB0, HB1, HB2, HB3} = {00, Type, Version, Length}
	info_packet->hb0 = 0x00;
	info_packet->hb1 = 0x22;
	info_packet->hb2 = AS_SDP_VER_1;
	info_packet->hb3 = 0x00;
}

void mod_build_adaptive_sync_infopacket_v2(const struct dc_stream_state *stream,
		const struct AS_Df_params *param,
		struct dc_info_packet *info_packet)
{
	info_packet->valid = true;
	// HEADER {HB0, HB1, HB2, HB3} = {00, Type, Version, Length}
	info_packet->hb0 = 0x00;
	info_packet->hb1 = 0x22;
	info_packet->hb2 = AS_SDP_VER_2;
	info_packet->hb3 = AS_DP_SDP_LENGTH;

	if (param) {
		//Payload
		info_packet->sb[0] = param->supportMode; //1: AVT; 0: FAVT
		info_packet->sb[1] = (stream->timing.v_total & 0x00FF);
		info_packet->sb[2] = (stream->timing.v_total & 0xFF00) >> 8;
		//info_packet->sb[3] = 0x00; Target RR, not use fot AVT
		info_packet->sb[4] = (param->increase.support << 6 | param->decrease.support << 7);
		info_packet->sb[5] = param->increase.frame_duration_hex;
		info_packet->sb[6] = param->decrease.frame_duration_hex;
	}
}

