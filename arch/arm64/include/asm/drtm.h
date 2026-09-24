/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES
 *
 * Definitions from Arm DEN0113 "DRTM Architecture for Arm".
 */
#ifndef __ASM_DRTM_H
#define __ASM_DRTM_H

#include <linux/arm-smccc.h>
#include <linux/bits.h>

/*
 * v1.4B section 3.1 "Introduction to interface functions and data
 * structures". Function IDs are 0x110 - 0x12F, called with SMC64.
 * Offset 0x02 is reserved.
 */
#define ARM_DRTM_SMC_FN_BASE                                      \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64, \
			   ARM_SMCCC_OWNER_STANDARD, 0x110)

/*
 * The spec uses 4KB pages for sizes and alignment throughout, see v1.4B
 * sections 3.3, 3.13, 3.14 and 3.15.
 */
#define ARM_DRTM_PAGE_SIZE			4096

/* v1.4B section 3.2 "DRTM_VERSION". */
#define ARM_DRTM_SMC_VERSION			(ARM_DRTM_SMC_FN_BASE + 0x00)
#define ARM_DRTM_VERSION_MAJOR_MASK		GENMASK_U32(30, 16)
#define ARM_DRTM_VERSION_MINOR_MASK		GENMASK_U32(15, 0)
#define ARM_DRTM_VERSION_MAJOR			1

/* v1.4B section 3.3 "DRTM_FEATURES" */
#define ARM_DRTM_SMC_FEATURES			(ARM_DRTM_SMC_FN_BASE + 0x01)
/* X1 input parameter */
#define ARM_DRTM_FEATURE_SELECTOR		BIT_U64(63)
#define ARM_DRTM_FEATURE_ID_MASK		GENMASK_U64(7, 0)

/*
 * v1.4B section 3.3, Table 6 "Return values for DRTM features". Use with
 * arm_drtm_query_feature() below.
 */

/* v1.4B Table 6 "TPM features" */
#define ARM_DRTM_FEATURE_TPM			0x01
/* Algorithm IDs are defined by TCG, in the kernel they are TPM_ALG_* */
#define ARM_DRTM_TPM_ALG_MASK			GENMASK_U64(15, 0)
#define ARM_DRTM_TPM_HASHING			BIT_U64(32)
#define ARM_DRTM_PCR_SCHEMA_MASK		GENMASK_U64(36, 33)

/*
 * PCR schema numbers. ARM_DRTM_PCR_SCHEMA_MASK is a bitmap indexed by these,
 * and the Launch Features ARM_DRTM_LAUNCH_PCR_SCHEMA_MASK field holds one.
 */
#define ARM_DRTM_PCR_SCHEMA_DEFAULT		0
#define ARM_DRTM_PCR_SCHEMA_AUTHORITIES		1

/* v1.4B Table 6 "Minimum memory requirement", in ARM_DRTM_PAGE_SIZE units */
#define ARM_DRTM_FEATURE_MIN_MEMORY		0x02
#define ARM_DRTM_DLME_DATA_PAGES_MASK		GENMASK_U64(31, 0)
#define ARM_DRTM_NW_DCE_PAGES_MASK		GENMASK_U64(63, 32)

/* v1.4B Table 6 "DMA protection features" */
#define ARM_DRTM_FEATURE_DMA_PROTECTION		0x03
#define ARM_DRTM_DMA_PROTECTION_MASK		GENMASK_U64(7, 0)
/* Only valid if ARM_DRTM_DMA_PROTECTION_REGION is supported */
#define ARM_DRTM_MAX_REGIONS_MASK		GENMASK_U64(23, 8)

/*
 * DMA protection types. ARM_DRTM_DMA_PROTECTION_MASK is a bitmap indexed by
 * these, and the Launch Features ARM_DRTM_LAUNCH_DMA_PROTECTION_MASK field
 * holds one.
 */
#define ARM_DRTM_DMA_PROTECTION_COMPLETE	0
#define ARM_DRTM_DMA_PROTECTION_REGION		1

/*
 * v1.4B Table 6 "Boot PE ID", the value is the PSCI CPU_ON target_cpu
 * encoding of the boot PE
 */
#define ARM_DRTM_FEATURE_BOOT_PE		0x04

/* v1.4B Table 6 "TCB hash features", max hashes for DRTM_SET_TCB_HASH */
#define ARM_DRTM_FEATURE_TCB_HASH		0x05
#define ARM_DRTM_TCB_HASH_COUNT_MASK		GENMASK_U64(7, 0)

/* v1.4B Table 6 "DLME image authentication features" */
#define ARM_DRTM_FEATURE_IMAGE_AUTH		0x06
#define ARM_DRTM_IMAGE_AUTH_SUPPORTED		BIT_U64(0)

/* v1.4B section 3.4 "DRTM_DYNAMIC_LAUNCH" */
#define ARM_DRTM_SMC_DYNAMIC_LAUNCH		(ARM_DRTM_SMC_FN_BASE + 0x04)

/* v1.4B section 3.5 "DRTM_UNPROTECT_MEMORY" */
#define ARM_DRTM_SMC_UNPROTECT_MEMORY		(ARM_DRTM_SMC_FN_BASE + 0x03)

/* v1.4B section 3.6 "DRTM_CLOSE_LOCALITY" */
#define ARM_DRTM_SMC_CLOSE_LOCALITY		(ARM_DRTM_SMC_FN_BASE + 0x05)

/* v1.4B section 3.7 "DRTM_GET_ERROR" */
#define ARM_DRTM_SMC_GET_ERROR			(ARM_DRTM_SMC_FN_BASE + 0x06)

/* v1.4B section 3.8 "DRTM_SET_ERROR" */
#define ARM_DRTM_SMC_SET_ERROR			(ARM_DRTM_SMC_FN_BASE + 0x07)

/*
 * v1.4B section 3.12 "DRTM error code encoding", Table 7. Value passed as
 * the Error Code parameter to DRTM_SET_ERROR and returned by
 * DRTM_GET_ERROR.
 */
#define ARM_DRTM_ERROR_PHASE_MASK		GENMASK_U64(2, 0)
#define ARM_DRTM_ERROR_ID_MASK			GENMASK_U64(10, 3)
#define ARM_DRTM_ERROR_DATA_MASK		GENMASK_U64(63, 11)

/* v1.4B Table 7 "DRTM phase when error occurred" */
#define ARM_DRTM_ERROR_PHASE_NONE		0
#define ARM_DRTM_ERROR_PHASE_DCRTM_FW		1
#define ARM_DRTM_ERROR_PHASE_DCRTM_COPROCESSOR	2
#define ARM_DRTM_ERROR_PHASE_DCE		3
#define ARM_DRTM_ERROR_PHASE_NW_DCE		4
#define ARM_DRTM_ERROR_PHASE_DLME		5

/* v1.4B Table 8 "Definition for error IDs (Bits[10:3])" */
#define ARM_DRTM_ERROR_ID_NONE			0x0
#define ARM_DRTM_ERROR_ID_PARAMETERS		0x2
#define ARM_DRTM_ERROR_ID_SECURE_EXCEPTION	0x3
#define ARM_DRTM_ERROR_ID_TCB_HASH_UNLOCKED	0x4
#define ARM_DRTM_ERROR_ID_TPM			0x5
#define ARM_DRTM_ERROR_ID_SIGNATURE		0x6
#define ARM_DRTM_ERROR_ID_IMAGE_FORMAT		0x7
#define ARM_DRTM_ERROR_ID_VENDOR		0xFF

/*
 * Construct the Table 7 error code. Plain shifts so this is usable from
 * assembly as well as C.
 */
#define ARM_DRTM_MAKE_ERROR(phase, id, data) \
	(((data) << 11) | ((id) << 3) | (phase))

/* v1.4B section 3.9 "DRTM_SET_TCB_HASH" */
#define ARM_DRTM_SMC_SET_TCB_HASH		(ARM_DRTM_SMC_FN_BASE + 0x08)

/* v1.4B section 3.10 "DRTM_LOCK_TCB_HASHES" */
#define ARM_DRTM_SMC_LOCK_TCB_HASHES		(ARM_DRTM_SMC_FN_BASE + 0x09)

/* v1.4B section 3.11 "DRTM_ENABLE_SECURE_INTERRUPTS" */
#define ARM_DRTM_SMC_ENABLE_SECURE_INTERRUPTS	(ARM_DRTM_SMC_FN_BASE + 0x0a)

/*
 * v1.4B Table 9 "Launch Features" field. The 0 constants are for
 * self-documentation of the selected behavior.
 */
#define ARM_DRTM_LAUNCH_HASH_FIRMWARE		0
#define ARM_DRTM_LAUNCH_HASH_TPM		BIT_U32(0)
#define ARM_DRTM_LAUNCH_PCR_SCHEMA_MASK		GENMASK_U32(2, 1)
#define ARM_DRTM_LAUNCH_DMA_PROTECTION_MASK	GENMASK_U32(5, 3)
#define ARM_DRTM_LAUNCH_NO_AUTH			0
#define ARM_DRTM_LAUNCH_AUTH			BIT_U32(6)
#define ARM_DRTM_LAUNCH_KEEP_SECURE_IRQS	0
#define ARM_DRTM_LAUNCH_DISABLE_SECURE_IRQS	BIT_U32(7)

/* v1.4B section 3.18 "Return codes", Table 20 "Return codes and values" */
#define ARM_DRTM_SUCCESS			0
#define ARM_DRTM_NOT_SUPPORTED			-1
#define ARM_DRTM_INVALID_PARAMETERS		-2
#define ARM_DRTM_DENIED				-3
#define ARM_DRTM_NOT_FOUND			-4
#define ARM_DRTM_INTERNAL_ERROR			-5
#define ARM_DRTM_MEM_PROTECT_INVALID		-6
#define ARM_DRTM_COPROCESSOR_ERROR		-7
#define ARM_DRTM_OUT_OF_RESOURCES		-8
#define ARM_DRTM_INVALID_DATA			-9
#define ARM_DRTM_SECONDARY_PE_NOT_OFF		-10
#define ARM_DRTM_ALREADY_CLOSED			-11
#define ARM_DRTM_TPM_ERROR			-12

/* Offsets in struct arm64_drtm_handoff */
#define ARM64_DRTM_HANDOFF_FDT_ADDR_OFFSET	0
#define ARM64_DRTM_HANDOFF_ENABLED_OFFSET	8

#ifndef __ASSEMBLY__

#include <linux/bitfield.h>
#include <linux/build_bug.h>
#include <linux/stddef.h>
#include <linux/types.h>

/* v1.4B section 3.13, Table 9 "DRTM_PARAMETERS definition" */
struct arm_drtm_parameters {
	__le16 revision;
	__le16 reserved;
	__le32 launch_features;
	__le64 dlme_region_address;
	__le64 dlme_region_size;
	__le64 dlme_image_start;
	__le64 dlme_entry_point_offset;
	__le64 dlme_image_size;
	__le64 dlme_data_offset;
	__le64 nw_dce_region_address;
	__le64 nw_dce_region_size;
	__le64 protection_table_address;
	__le64 protection_table_size;
};
static_assert(sizeof(struct arm_drtm_parameters) == 88);

static inline s32 arm_drtm_version(u16 *major, u16 *minor)
{
	struct arm_smccc_res res;
	u32 version;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_VERSION, 0, 0, 0, 0, 0, 0, 0, &res);
	version = res.a0;
	/*
	 * For this command only the output is specified as w0 a 32 bit value.
	 * All others use x0 a 64 bit value. The -ve error and version are
	 * overlayed together with the 32 bit sign bit telling them apart.
	 */
	if (version & BIT(31))
		return version;

	*major = FIELD_GET(ARM_DRTM_VERSION_MAJOR_MASK, version);
	*minor = FIELD_GET(ARM_DRTM_VERSION_MINOR_MASK, version);
	return ARM_DRTM_SUCCESS;
}

static inline s64 arm_drtm_query_function(u64 function_id)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_FEATURES, function_id, 0, 0, 0, 0, 0, 0,
			  &res);
	return res.a0;
}

/*
 * v1.4b Section 3.3.1 says:
 *   A return value of greater than 0 means the feature ID is
 *   implemented, and there are other feature-specific
 *   feature or capability bits available in other return value
 *   registers.
 * And for FEATURE_SELECTOR all defined features return data in other
 * registers, so 0 is not an allowed return.
 */
static inline s64 arm_drtm_query_feature(u64 feature, u64 *value)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_FEATURES,
			  ARM_DRTM_FEATURE_SELECTOR |
				  FIELD_PREP(ARM_DRTM_FEATURE_ID_MASK, feature),
			  0, 0, 0, 0, 0, 0, &res);
	*value = res.a1;
	return res.a0;
}

static inline s64 arm_drtm_unprotect_memory(void)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_UNPROTECT_MEMORY, 0, 0, 0, 0, 0, 0, 0,
			  &res);
	return res.a0;
}

static inline s64
arm_drtm_dynamic_launch(struct arm_drtm_parameters *params_addr)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_DYNAMIC_LAUNCH,
			  (unsigned long)params_addr, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static inline s64 arm_drtm_close_locality(u32 locality)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_CLOSE_LOCALITY, locality, 0, 0, 0, 0, 0,
			  0, &res);
	return res.a0;
}

static inline s64 arm_drtm_get_error(s64 *error_code)
{
	struct arm_smccc_res res;
	s64 status;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_GET_ERROR, 0, 0, 0, 0, 0, 0, 0, &res);
	status = res.a0;
	if (status != ARM_DRTM_SUCCESS)
		return status;

	*error_code = res.a1;
	return ARM_DRTM_SUCCESS;
}

static inline s64 arm_drtm_enable_secure_interrupts(void)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(ARM_DRTM_SMC_ENABLE_SECURE_INTERRUPTS, 0, 0, 0, 0, 0,
			  0, 0, &res);
	return res.a0;
}

/*
 * Since we cannot pass a parameter through the launch to drtm_entry any
 * additional data is written by the stub here.
 */
struct arm64_drtm_handoff {
	__le64 fdt_addr;
	u8 drtm_enabled;
};

static_assert(offsetof(struct arm64_drtm_handoff, fdt_addr) ==
	      ARM64_DRTM_HANDOFF_FDT_ADDR_OFFSET);
static_assert(offsetof(struct arm64_drtm_handoff, drtm_enabled) ==
	      ARM64_DRTM_HANDOFF_ENABLED_OFFSET);

extern struct arm64_drtm_handoff arm64_drtm_handoff;

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_DRTM_H */
