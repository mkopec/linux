// SPDX-License-Identifier: GPL-2.0-only
/*
 * DLME data of ARM DEN 0113 DRTM
 *
 * The launch leaves the DLME data, with the DRTM event log among others,
 * right after the kernel image. Reserve it, and make the event log available
 * to user space at /sys/kernel/security/drtm/binary_measurements, as DEN0113
 * section 4.6.2 expects of the DLME. The kernel appends the events of its own
 * measurements to it.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/security.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tpm_eventlog.h>
#include <linux/unaligned.h>

#include <asm/drtm.h>
#include <asm/early_ioremap.h>

/* Room for the events of the kernel's own measurements */
#define DRTM_LOG_EXTRA		SZ_4K

static phys_addr_t drtm_data_pa __ro_after_init;
static u64 drtm_data_size __ro_after_init;

static DEFINE_MUTEX(drtm_log_lock);
static u8 *drtm_log;
static size_t drtm_log_size;
static size_t drtm_log_room;
static const struct tcg_efi_specid_event_head *drtm_log_specid;

/*
 * Called from arm64_memblock_init(), before memblock allocates. The data is
 * right after the kernel image, drtm_entry checked that.
 */
void __init arm64_drtm_reserve(void)
{
	struct arm_drtm_dlme_data_header *hdr;
	u64 size, used, sub[6];
	bool valid;
	int i;

	if (!arm64_drtm_handoff.drtm_enabled)
		return;

	hdr = early_memremap(__pa_symbol(__drtm_dlme_start), sizeof(*hdr));
	if (!hdr) {
		pr_err("DRTM: can't map the DLME data\n");
		return;
	}

	size = le64_to_cpu(hdr->data_size);
	used = le16_to_cpu(hdr->header_size);
	valid = le16_to_cpu(hdr->version) == 1 && used >= sizeof(*hdr) &&
		size <= SZ_64M;
	sub[0] = le64_to_cpu(hdr->protected_regions_size);
	sub[1] = le64_to_cpu(hdr->address_map_size);
	sub[2] = le64_to_cpu(hdr->event_log_size);
	sub[3] = le64_to_cpu(hdr->tcb_hash_table_size);
	sub[4] = le64_to_cpu(hdr->acpi_tables_size);
	sub[5] = le64_to_cpu(hdr->impdef_size);
	/* Each size is at most 64 MiB, so the sum can't overflow. */
	for (i = 0; i < ARRAY_SIZE(sub); i++) {
		valid &= sub[i] <= SZ_64M;
		used += sub[i];
	}
	early_memunmap(hdr, sizeof(*hdr));

	if (!valid || used > size) {
		pr_err("DRTM: invalid DLME data header\n");
		return;
	}

	drtm_data_pa = __pa_symbol(__drtm_dlme_start);
	drtm_data_size = size;
	memblock_reserve(drtm_data_pa, drtm_data_size);
}

/* Check the TCG2 Spec ID event at the start of the log, and skip it. */
static size_t __init drtm_log_specid_size(const u8 *log, size_t size)
{
	const struct tcg_pcr_event *event = (const void *)log;
	const struct tcg_efi_specid_event_head *specid;
	size_t len;
	u32 i;

	if (size < sizeof(*event) + sizeof(*specid))
		return 0;

	len = sizeof(*event) + event->event_size;
	specid = (const void *)event->event;
	if (len > size || event->event_size < sizeof(*specid) ||
	    memcmp(specid->signature, TCG_SPECID_SIG, sizeof(TCG_SPECID_SIG)) ||
	    specid->num_algs == 0 || specid->num_algs > TPM2_MAX_PCR_BANKS ||
	    event->event_size < struct_size(specid, digest_sizes,
					    specid->num_algs))
		return 0;

	/* The kernel appends events with digests of these sizes. */
	for (i = 0; i < specid->num_algs; i++)
		if (specid->digest_sizes[i].digest_size > TPM2_MAX_DIGEST_SIZE)
			return 0;

	return len;
}

static ssize_t drtm_log_read(struct file *file, char __user *buf,
			     size_t count, loff_t *ppos)
{
	guard(mutex)(&drtm_log_lock);
	return simple_read_from_buffer(buf, count, ppos, drtm_log,
				       drtm_log_size);
}

static const struct file_operations drtm_log_fops = {
	.read = drtm_log_read,
	.llseek = default_llseek,
};

static int __init drtm_log_init(void)
{
	const struct arm_drtm_dlme_data_header *hdr;
	struct dentry *dir, *file;
	size_t off, size;
	const u8 *data;

	if (!drtm_data_size)
		return 0;

	/* The loader may have kept the data out of the linear map. */
	data = memremap(drtm_data_pa, drtm_data_size, MEMREMAP_WB);
	if (!data) {
		pr_err("DRTM: can't map the DLME data\n");
		return -ENOMEM;
	}
	hdr = (const void *)data;
	off = le16_to_cpu(hdr->header_size) +
	      le64_to_cpu(hdr->protected_regions_size) +
	      le64_to_cpu(hdr->address_map_size);
	size = le64_to_cpu(hdr->event_log_size);

	if (!drtm_log_specid_size(data + off, size)) {
		pr_err("DRTM: the event log doesn't start with a Spec ID event\n");
		memunmap((void *)data);
		return 0;
	}

	drtm_log_room = size + DRTM_LOG_EXTRA;
	drtm_log = kmalloc(drtm_log_room, GFP_KERNEL);
	if (!drtm_log) {
		memunmap((void *)data);
		return -ENOMEM;
	}
	memcpy(drtm_log, data + off, size);
	drtm_log_size = size;
	memunmap((void *)data);
	drtm_log_specid = (const void *)((struct tcg_pcr_event *)drtm_log)->event;

	dir = securityfs_create_dir("drtm", NULL);
	if (IS_ERR(dir))
		return PTR_ERR(dir);
	file = securityfs_create_file("binary_measurements", 0440, dir, NULL,
				      &drtm_log_fops);
	return PTR_ERR_OR_ZERO(file);
}
fs_initcall(drtm_log_init);

/**
 * arm64_drtm_log_event() - append an event to the DRTM event log
 * @pcr:	PCR the event was extended into
 * @event_type:	TCG event type
 * @digests:	digests of the event, for at least the banks of the log, each
 *		of TPM2_MAX_DIGEST_SIZE bytes
 * @nr_digests:	number of digests
 * @event:	event data
 * @event_size:	size of the event data
 *
 * Return: 0, -ENODEV without a log, -EINVAL if a digest of the log is
 * missing, or -ENOSPC.
 */
int arm64_drtm_log_event(u32 pcr, u32 event_type,
			 const struct arm64_drtm_digest *digests,
			 unsigned int nr_digests, const void *event,
			 u32 event_size)
{
	const struct tcg_efi_specid_event_algs *alg;
	size_t size = 3 * sizeof(u32) + sizeof(u32) + event_size;
	unsigned int i, j;
	u8 *p;

	guard(mutex)(&drtm_log_lock);
	if (!drtm_log)
		return -ENODEV;

	for (i = 0; i < drtm_log_specid->num_algs; i++)
		size += sizeof(u16) + drtm_log_specid->digest_sizes[i].digest_size;
	if (size > drtm_log_room - drtm_log_size)
		return -ENOSPC;

	p = drtm_log + drtm_log_size;
	put_unaligned_le32(pcr, p);
	put_unaligned_le32(event_type, p + 4);
	put_unaligned_le32(drtm_log_specid->num_algs, p + 8);
	p += 12;

	for (i = 0; i < drtm_log_specid->num_algs; i++) {
		alg = &drtm_log_specid->digest_sizes[i];
		for (j = 0; j < nr_digests; j++)
			if (digests[j].alg_id == alg->alg_id)
				break;
		if (j == nr_digests)
			return -EINVAL;

		put_unaligned_le16(alg->alg_id, p);
		memcpy(p + 2, digests[j].digest, alg->digest_size);
		p += 2 + alg->digest_size;
	}

	put_unaligned_le32(event_size, p);
	memcpy(p + 4, event, event_size);

	drtm_log_size += size;
	return 0;
}
