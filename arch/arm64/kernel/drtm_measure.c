// SPDX-License-Identifier: GPL-2.0-only
/*
 * Measurements of the DLME configuration for ARM DEN 0113 DRTM
 *
 * The dynamic launch measures the kernel image only. DEN0113 section 4.6.2
 * requires the DLME to validate any code or data outside of the DLME region
 * before it relies on it, and reserves PCRs 19 to 22 and TPM locality 2 for
 * its own measurements. Measure the kernel command line, the device tree and
 * the initramfs into PCR 19 at locality 2, then close locality 2, after
 * which neither PCR 19 nor the PCRs of the launch can change until the next
 * launch.
 *
 * The TPM may only appear late, e.g. a firmware TPM driver built as a module.
 * The digests are computed early, while the DMA protection of the launch is
 * in place and before the initramfs is unpacked, and extended when the TPM
 * that recorded the launch registers, before user space can access it.
 */

#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/libfdt.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of_fdt.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tpm.h>
#include <linux/tpm_eventlog.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>

#include <asm/drtm.h>

#define DRTM_DLME_PCR		19
#define DRTM_HASH_CHUNK		SZ_64K

enum drtm_dlme_event {
	DRTM_EV_CMDLINE,
	DRTM_EV_DTB,
	DRTM_EV_INITRD,
	DRTM_NR_EVENTS,
};

static const char * const drtm_event_names[DRTM_NR_EVENTS] = {
	[DRTM_EV_CMDLINE] = "command line",
	[DRTM_EV_DTB] = "device tree",
	[DRTM_EV_INITRD] = "initramfs",
};

/* Event data of the events in the DRTM event log */
static const char * const drtm_event_data[DRTM_NR_EVENTS] = {
	[DRTM_EV_CMDLINE] = "Linux command line",
	[DRTM_EV_DTB] = "Linux device tree",
	[DRTM_EV_INITRD] = "Linux initramfs",
};

/* The banks that can be extended, the TPM doesn't report its banks yet. */
static const u16 drtm_algs[] = {
	TPM_ALG_SHA1, TPM_ALG_SHA256, TPM_ALG_SHA384, TPM_ALG_SHA512,
};

static u8 drtm_digests[DRTM_NR_EVENTS][ARRAY_SIZE(drtm_algs)]
		      [SHA512_DIGEST_SIZE] __ro_after_init;

static struct {
	struct sha1_ctx sha1;
	struct sha256_ctx sha256;
	struct sha384_ctx sha384;
	struct sha512_ctx sha512;
} drtm_hash_ctx __initdata;

/* Hash data once for all banks, in chunks that stay in the cache. */
static void __init drtm_hash(const u8 *data, size_t len,
			     u8 out[][SHA512_DIGEST_SIZE])
{
	size_t n;

	sha1_init(&drtm_hash_ctx.sha1);
	sha256_init(&drtm_hash_ctx.sha256);
	sha384_init(&drtm_hash_ctx.sha384);
	sha512_init(&drtm_hash_ctx.sha512);

	for (; len; data += n, len -= n) {
		n = min_t(size_t, len, DRTM_HASH_CHUNK);
		sha1_update(&drtm_hash_ctx.sha1, data, n);
		sha256_update(&drtm_hash_ctx.sha256, data, n);
		sha384_update(&drtm_hash_ctx.sha384, data, n);
		sha512_update(&drtm_hash_ctx.sha512, data, n);
		cond_resched();
	}

	sha1_final(&drtm_hash_ctx.sha1, out[0]);
	sha256_final(&drtm_hash_ctx.sha256, out[1]);
	sha384_final(&drtm_hash_ctx.sha384, out[2]);
	sha512_final(&drtm_hash_ctx.sha512, out[3]);
}

/*
 * A dynamic launch resets PCR 17 to 0 and extends it, which only locality 4
 * can do. Its initial value is all ones, so a different value identifies the
 * TPM that recorded the launch. The launch may only extend some of the banks,
 * which stay 0 in the others.
 */
static bool drtm_is_launch_tpm(struct tpm_chip *chip)
{
	struct tpm_digest digest;
	u16 size;
	int i;

	if (!(chip->flags & TPM_CHIP_FLAG_TPM2))
		return false;

	for (i = 0; i < chip->nr_allocated_banks; i++) {
		memset(&digest, 0, sizeof(digest));
		digest.alg_id = chip->allocated_banks[i].alg_id;
		size = chip->allocated_banks[i].digest_size;
		if (tpm_pcr_read(chip, 17, &digest))
			continue;

		if (memchr_inv(digest.digest, 0xff, size) &&
		    memchr_inv(digest.digest, 0, size))
			return true;
	}

	return false;
}

static void drtm_log(enum drtm_dlme_event ev)
{
	struct arm64_drtm_digest digests[ARRAY_SIZE(drtm_algs)];
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(drtm_algs); i++) {
		digests[i].alg_id = drtm_algs[i];
		digests[i].digest = drtm_digests[ev][i];
	}

	rc = arm64_drtm_log_event(DRTM_DLME_PCR, IPL, digests,
				  ARRAY_SIZE(digests), drtm_event_data[ev],
				  strlen(drtm_event_data[ev]) + 1);
	if (rc && rc != -ENODEV)
		pr_warn("DRTM: failed to log the %s: %d\n",
			drtm_event_names[ev], rc);
}

static int drtm_extend(struct tpm_chip *chip)
{
	struct tpm_digest *digests;
	int ev, i, j, rc = 0;

	digests = kcalloc(chip->nr_allocated_banks, sizeof(*digests),
			  GFP_KERNEL);
	if (!digests)
		return -ENOMEM;

	for (ev = 0; ev < DRTM_NR_EVENTS; ev++) {
		for (i = 0; i < chip->nr_allocated_banks; i++) {
			u16 alg = chip->allocated_banks[i].alg_id;

			for (j = 0; j < ARRAY_SIZE(drtm_algs); j++)
				if (drtm_algs[j] == alg)
					break;
			if (j == ARRAY_SIZE(drtm_algs)) {
				pr_err("DRTM: can't measure into the %#06x bank\n",
				       alg);
				rc = -EOPNOTSUPP;
				goto out;
			}

			digests[i].alg_id = alg;
			memcpy(digests[i].digest, drtm_digests[ev][j],
			       chip->allocated_banks[i].digest_size);
		}

		rc = tpm_pcr_extend_locality(chip, TPM_LOCALITY_2,
					     DRTM_DLME_PCR, digests);
		if (rc) {
			pr_err("DRTM: failed to measure the %s: %d\n",
			       drtm_event_names[ev], rc);
			goto out;
		}

		drtm_log(ev);
	}

out:
	kfree(digests);
	return rc;
}

static int drtm_tpm_notify(struct notifier_block *nb, unsigned long event,
			   void *data)
{
	static DEFINE_MUTEX(lock);
	static bool done;
	struct tpm_chip *chip = data;
	s64 status;
	int rc;

	if (event != TPM_CHIP_ADD)
		return NOTIFY_DONE;

	guard(mutex)(&lock);
	if (done || !drtm_is_launch_tpm(chip))
		return NOTIFY_DONE;
	done = true;

	rc = drtm_extend(chip);
	if (!rc)
		pr_info("DRTM: measured the DLME configuration into PCR %d of %s\n",
			DRTM_DLME_PCR, dev_name(&chip->dev));

	/*
	 * Close locality 2 even if the measurements failed, a PCR 19 that
	 * misses them shows that.
	 */
	status = arm_drtm_close_locality(TPM_LOCALITY_2);
	if (status != ARM_DRTM_SUCCESS)
		pr_err("DRTM: failed to close locality 2: %lld\n", status);

	return NOTIFY_OK;
}

static struct notifier_block drtm_tpm_nb = {
	.notifier_call = drtm_tpm_notify,
};

/*
 * Before rootfs_initcall unpacks the initramfs, and before
 * DRTM_UNPROTECT_MEMORY lets devices change any of the measured data.
 */
static int __init drtm_measure_dlme(void)
{
	const u8 *initrd = NULL;
	size_t initrd_size = 0;
	int ev;

	if (!arm64_drtm_handoff.drtm_enabled)
		return 0;

	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) && initrd_start) {
		initrd = (const u8 *)initrd_start;
		initrd_size = initrd_end - initrd_start;
	}

	drtm_hash((const u8 *)boot_command_line, strlen(boot_command_line),
		  drtm_digests[DRTM_EV_CMDLINE]);
	drtm_hash(initial_boot_params,
		  initial_boot_params ? fdt_totalsize(initial_boot_params) : 0,
		  drtm_digests[DRTM_EV_DTB]);
	drtm_hash(initrd, initrd_size, drtm_digests[DRTM_EV_INITRD]);

	for (ev = 0; ev < DRTM_NR_EVENTS; ev++)
		pr_info("DRTM: %s sha256 %*phN\n", drtm_event_names[ev],
			SHA256_DIGEST_SIZE, drtm_digests[ev][1]);

	return tpm_register_chip_notifier(&drtm_tpm_nb);
}
fs_initcall_sync(drtm_measure_dlme);
