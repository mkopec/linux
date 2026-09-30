/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Hung-Wen Hsieh <hung-wen.hsieh@mediatek.com>
 */

#ifndef __UFBC_DEF_H__
#define __UFBC_DEF_H__

/**
 * Describe Mediatek UFBC (Universal Frame Buffer Compression) buffer header.
 * Mediatek UFBC supports 1 plane Bayer and 2 planes Y/UV image formats.
 * Caller must follow the formulation to calculate the bit stream buffer size
 * and length table buffer size.
 *
 * Header Size
 *
 * Fixed size of 4096 bytes. Reserved bytes will be used by Mediatek
 * ISP driver. Caller SHOULD NOT edit it.
 *
 * Bit Stream Size
 *
 *  @code
 *    // for each plane
 *    size = ((width + 63) / 64) * 64;      // width must be aligned to 64 pixel
 *    size = (size * bits_per_pixel + 7) / 8; // convert to bytes
 *    size = size * height;
 *  @endcode
 *
 * Table Size
 *
 *  @code
 *    // for each plane
 *    size = (width + 63) / 64;
 *    size = size * height;
 *  @endcode
 *
 * And the memory layout should be followed as
 *
 *  @code
 *           Bayer                  YUV2P
 *    +------------------+  +------------------+
 *    |      Header      |  |      Header      |
 *    +------------------+  +------------------+
 *    |                  |  |     Y Plane      |
 *    | Bayer Bit Stream |  |    Bit Stream    |
 *    |                  |  |                  |
 *    +------------------+  +------------------+
 *    |   Length Table   |  |     UV Plane     |
 *    +------------------+  |    Bit Stream    |
 *                          |                  |
 *                          +------------------+
 *                          |     Y Plane      |
 *                          |   Length Table   |
 *                          +------------------+
 *                          |     UV Plane     |
 *                          |   Length Table   |
 *                          +------------------+
 *  @endcode
 *
 *  @note Caller has responsibility to fill all the fields according the
 *        real buffer layout.
 */

struct ufbc_buf_header {
	/** Describe image resolution, unit in pixel. */
	u32 width;

	/** Describe image resolution, unit in pixel. */
	u32 height;

	/** Describe UFBC data plane count, UFBC supports maximum 2 planes. */
	u32 plane_count;

	/** Describe the original image data bits per pixel of the given plane. */
	u32 bits_per_pixel[3];

	/**
	 * Describe the offset of the given plane bit stream data in bytes,
	 * including header size.
	 */
	u32 bitstream_offset[3];

	/** Describe the bit stream data size in bytes of the given plane. */
	u32 bitstream_size[3];

	/** Describe the encoded data size in bytes of the given plane. */
	u32 bitstream_data_size[3];

	/**
	 * Describe the offset of length table of the given plane, including
	 * header size.
	 */
	u32 table_offset[3];

	/** Describe the length table size of the given plane */
	u32 table_size[3];

	/** Describe the total buffer size, including buffer header. */
	u32 buffer_size;
};

struct ufbc_buffer_header {
	union {
		struct ufbc_buf_header header;
		u8 reserved[4096];
	};
};

#endif // __UFBC_DEF_H__
