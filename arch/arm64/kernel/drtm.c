// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES
 *
 * Support functions for ARM DEN 0113 "DRTM Architecture for Arm"
 */
#include <linux/bug.h>
#include <linux/efi.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/printk.h>

#include <asm/drtm.h>

struct arm64_drtm_handoff arm64_drtm_handoff __efi_data_handoff;

static int __init arm64_drtm_unprotect_memory(void)
{
	s64 status;

	if (!arm64_drtm_handoff.drtm_enabled)
		return 0;

	/*
	 * Currently Linux can only fully support a DRTM implementation that
	 * relies only the SMMU. This should be called directly after attaching
	 * a driver to every SMMU but before binding any drivers that want to
	 * use devices attached to the SMMU. Our boot flow does not have a way
	 * to do that, so for now call it here.
	 *
	 * For SMMU based implementations their UNPROTECT_MEMORY is probably a
	 * NOP since touching the SMMU here is forbidden, but they may still do
	 * something interesting so be sure to call it at least.
	 *
	 * The stub excludes the most likely case of non-SMMU by only using
	 * ARM_DRTM_DMA_PROTECTION_COMPLETE, which is defined as:
	 *    Complete DMA protection is hardware-based enforcement at the SMMU
	 *    that blocks all DMA from Non- secure devices."
	 *
	 * For now if people have such systems they cannot include built-in
	 * drivers for any DMA devices, those have to be modules to be ordered
	 * after this.
	 */
	/*
	 * The initramfs is unpacked asynchronously. Devices mustn't be able
	 * to change it before it has been unpacked.
	 */
	wait_for_initramfs();

	status = arm_drtm_unprotect_memory();
	WARN(status != ARM_DRTM_SUCCESS,
	     "DRTM: failed to unprotect memory (x0=%lld)", status);
	if (status == ARM_DRTM_SUCCESS)
		pr_info("DRTM: launch completed\n");
	return 0;
}
late_initcall_sync(arm64_drtm_unprotect_memory);
