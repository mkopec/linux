/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Microsoft Corporation
 */

#ifndef __TPM_FTPM_TEE_H__
#define __TPM_FTPM_TEE_H__

#include <linux/tee_drv.h>
#include <linux/tpm.h>
#include <linux/uuid.h>

/* The TAFs ID implemented in this TA */
#define FTPM_OPTEE_TA_SUBMIT_COMMAND  (0)
#define FTPM_OPTEE_TA_EMULATE_PPI     (1)
#define FTPM_OPTEE_TA_REQUEST_LOCALITY    (2)
#define FTPM_OPTEE_TA_RELINQUISH_LOCALITY (3)

/* GlobalPlatform TEE Client API result of a denied locality */
#define FTPM_TEE_ERROR_ACCESS_DENIED  0xFFFF0001

/* max. buffer size supported by fTPM  */
#define MAX_COMMAND_SIZE       4096
#define MAX_RESPONSE_SIZE      4096

/**
 * struct ftpm_tee_private - fTPM's private data
 * @chip:     struct tpm_chip instance registered with tpm framework.
 * @session:  fTPM TA session identifier.
 * @ctx:      TEE context handler.
 * @shm:      Memory pool shared with fTPM TA in TEE.
 */
struct ftpm_tee_private {
	struct tpm_chip *chip;
	u32 session;
	struct tee_context *ctx;
	struct tee_shm *shm;
};

#endif /* __TPM_FTPM_TEE_H__ */
