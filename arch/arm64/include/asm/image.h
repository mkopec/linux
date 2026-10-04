/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_IMAGE_H
#define __ASM_IMAGE_H

#define ARM64_IMAGE_MAGIC	"ARM\x64"

#ifdef CONFIG_ARM64_DRTM
#define EFI_IMAGE_INFO_SIZE	40
#else
#define EFI_IMAGE_INFO_SIZE	8
#endif

/*
 * struct arm64_drtm_image_desc, see below. "DRTM" read as a little endian u32.
 */
#define ARM64_DRTM_IMAGE_DESC_MAGIC	0x4d545244
#define ARM64_DRTM_IMAGE_DESC_VERSION	1
#define ARM64_DRTM_IMAGE_DESC_SIZE	40

#define ARM64_IMAGE_FLAG_BE_SHIFT		0
#define ARM64_IMAGE_FLAG_PAGE_SIZE_SHIFT	(ARM64_IMAGE_FLAG_BE_SHIFT + 1)
#define ARM64_IMAGE_FLAG_PHYS_BASE_SHIFT \
					(ARM64_IMAGE_FLAG_PAGE_SIZE_SHIFT + 2)
#define ARM64_IMAGE_FLAG_BE_MASK		0x1
#define ARM64_IMAGE_FLAG_PAGE_SIZE_MASK		0x3
#define ARM64_IMAGE_FLAG_PHYS_BASE_MASK		0x1

#define ARM64_IMAGE_FLAG_LE			0
#define ARM64_IMAGE_FLAG_BE			1
#define ARM64_IMAGE_FLAG_PAGE_SIZE_4K		1
#define ARM64_IMAGE_FLAG_PAGE_SIZE_16K		2
#define ARM64_IMAGE_FLAG_PAGE_SIZE_64K		3
#define ARM64_IMAGE_FLAG_PHYS_BASE		1

#ifndef __ASSEMBLER__

#define arm64_image_flag_field(flags, field) \
				(((flags) >> field##_SHIFT) & field##_MASK)

/*
 * struct arm64_image_header - arm64 kernel image header
 * See Documentation/arch/arm64/booting.rst for details
 *
 * @code0:		Executable code, or
 *   @mz_header		  alternatively used for part of MZ header
 * @code1:		Executable code
 * @text_offset:	Image load offset
 * @image_size:		Effective Image size
 * @flags:		kernel flags
 * @drtm_desc:		offset of struct arm64_drtm_image_desc, or 0
 * @reserved:		reserved
 * @magic:		Magic number
 * @reserved5:		reserved, or
 *   @pe_header:	  alternatively used for PE COFF offset
 */

struct arm64_image_header {
	__le32 code0;
	__le32 code1;
	__le64 text_offset;
	__le64 image_size;
	__le64 flags;
	__le64 drtm_desc;
	__le64 res3;
	__le64 res4;
	__le32 magic;
	__le32 res5;
};

/*
 * This struct is filled in by the linker, see EFI_IMAGE_INFO.
 * Any change requires updating arch/arm64/kernel/vmlinux.lds.S
 */
struct efi_image_info {
	u64 code_size;
#ifdef CONFIG_ARM64_DRTM
	u64 drtm_measured_start;
	u64 dlme_measured_size;
	u64 drtm_entry;
	u64 arm64_drtm_handoff;
#endif
};
static_assert(sizeof(struct efi_image_info) == EFI_IMAGE_INFO_SIZE);

/*
 * Describes the DEN0113 DLME layout of an Image built with CONFIG_ARM64_DRTM so
 * that a loader outside of the kernel build, such as u-root, can launch it.
 * Unlike struct efi_image_info this is a versioned experimental format: it is
 * found through arm64_image_header::drtm_desc and is identified by its magic
 * and version. All offsets are relative to the start
 * of Image, see arch/arm64/kernel/vmlinux.lds.S for the layout.
 *
 * The descriptor is emitted by the linker in the kernel's endianness, loaders
 * only accept little endian Images.
 */
struct arm64_drtm_image_desc {
	__le32 magic;
	__le32 version;
	__le64 measured_start;
	__le64 measured_size;
	__le64 entry;
	__le64 handoff;
};
static_assert(sizeof(struct arm64_drtm_image_desc) ==
	      ARM64_DRTM_IMAGE_DESC_SIZE);

#endif /* __ASSEMBLER__ */

#endif /* __ASM_IMAGE_H */
