// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES
 *
 * Support functions for ARM DEN 0113 "DRTM Architecture for Arm"
 */
#include <linux/efi.h>

#include <asm/drtm.h>

struct arm64_drtm_handoff arm64_drtm_handoff __efi_data_handoff;
