/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_IMAGE_H
#define __ASM_IMAGE_H

#define ARM64_IMAGE_MAGIC	"ARM\x64"

#ifdef CONFIG_ARM64_DRTM
#define EFI_IMAGE_INFO_SIZE	40
#else
#define EFI_IMAGE_INFO_SIZE	8
#endif

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
	__le64 res2;
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

#endif /* __ASSEMBLER__ */

#endif /* __ASM_IMAGE_H */
