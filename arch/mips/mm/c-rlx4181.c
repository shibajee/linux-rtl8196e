// SPDX-License-Identifier: GPL-2.0
/*
 * c-rlx4181.c: Realtek Lexra RLX4181 CPU cache code.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/of.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/isadep.h>
#include <asm/io.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>

static unsigned int icache_size, dcache_size;		/* Size in bytes */
static unsigned int icache_lsize, dcache_lsize;		/* Size in bytes */


#define _nop() \
do { \
	__asm__ __volatile__("nop"); \
} while (0)


static inline void __rlx4181_flush_dcache_internal(void)
{
	unsigned long cctl;

	cctl = read_c0_rlx4181_cctl();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	write_c0_rlx4181_cctl(cctl & (~RLX4181_CCTL_DINVAL));
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	write_c0_rlx4181_cctl(cctl | (RLX4181_CCTL_DINVAL));
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
}

static inline void __rlx4181_flush_icache_internal(void)
{
	unsigned long cctl;

	cctl = read_c0_rlx4181_cctl();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	write_c0_rlx4181_cctl(cctl & (~RLX4181_CCTL_IINVAL));
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	write_c0_rlx4181_cctl(cctl | (RLX4181_CCTL_IINVAL));
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
	_nop();
}

static void __rlx4181_flush_icache(void)
{
	unsigned long flags;

	local_irq_save(flags);
	__rlx4181_flush_icache_internal();
	local_irq_restore(flags);
}

static void __rlx4181_flush_dcache(void)
{
	unsigned long flags;

	local_irq_save(flags);
	__rlx4181_flush_dcache_internal();
	local_irq_restore(flags);
}

static void rlx4181_flush_icache_range(unsigned long start, unsigned long end)
{
	__rlx4181_flush_icache();
}

static void rlx4181_flush_dcache_range(unsigned long start, unsigned long end)
{
	__rlx4181_flush_dcache();
}

static void rlx4181___flush_cache_all(void)
{
	unsigned long flags;

	local_irq_save(flags);
	__rlx4181_flush_dcache_internal();
	__rlx4181_flush_icache_internal();
	local_irq_restore(flags);
}

static void rlx4181_flush_cache_all(void)
{
	rlx4181___flush_cache_all();
}

static void rlx4181_flush_cache_mm(struct mm_struct *mm)
{
	rlx4181_flush_cache_all();
}

static void rlx4181_flush_cache_range(struct vm_area_struct *vma,
				  unsigned long start, unsigned long end)
{
	rlx4181_flush_cache_all();
}

static void rlx4181_flush_cache_page(struct vm_area_struct *vma,
				 unsigned long addr, unsigned long pfn)
{
	unsigned long kaddr = KSEG0ADDR(pfn << PAGE_SHIFT);
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgdp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;

	/* No ASID => no such page in the cache.  */
	if (cpu_context(smp_processor_id(), mm) == 0)
		return;

	pgdp = pgd_offset(mm, addr);
	pudp = pud_offset(pgdp, addr);
	pmdp = pmd_offset(pudp, addr);
	ptep = pte_offset(pmdp, addr);

	/* Invalid => no such page in the cache.  */
	if (!(pte_val(*ptep) & _PAGE_PRESENT))
		return;

	rlx4181_flush_dcache_range(kaddr, kaddr + PAGE_SIZE);
	rlx4181_flush_icache_range(kaddr, kaddr + PAGE_SIZE);
}

static void local_rlx4181_flush_data_cache_page(void *addr)
{
	__rlx4181_flush_dcache();
}

static void rlx4181_flush_data_cache_page(unsigned long addr)
{
	__rlx4181_flush_dcache();
}

static void rlx4181_flush_kernel_vmap_range(unsigned long vaddr, int size)
{
	rlx4181_flush_cache_all();
}

static void rlx4181_dma_cache_wback_inv(unsigned long start, unsigned long size)
{
	rlx4181_flush_dcache_range(start, start + size);
}

static u32 of_property_read_u32_or_panic(struct device_node *np,
		const char *propname)
{
	u32 out_value;

	if (of_property_read_u32(np, propname, &out_value))
		panic("Unable to get %s from devicetree", propname);
	return out_value;
}

void rlx4181_cache_init(void)
{
	extern void build_clear_page(void);
	extern void build_copy_page(void);
	struct device_node *np;

	np = of_get_cpu_node(0, NULL);
	if (!np)
		panic("Unable to find cpu node in devicetree");

	dcache_size = of_property_read_u32_or_panic(np, "d-cache-size");
	icache_size = of_property_read_u32_or_panic(np, "i-cache-size");
	dcache_lsize = of_property_read_u32_or_panic(np, "d-cache-line-size");
	icache_lsize = of_property_read_u32_or_panic(np, "i-cache-line-size");

	of_node_put(np);

	current_cpu_data.dcache.linesz = dcache_lsize;
	current_cpu_data.icache.linesz = icache_lsize;

	flush_cache_all = rlx4181_flush_cache_all;
	__flush_cache_all = rlx4181___flush_cache_all;
	flush_cache_mm = rlx4181_flush_cache_mm;
	flush_cache_range = rlx4181_flush_cache_range;
	flush_cache_page = rlx4181_flush_cache_page;
	flush_icache_range = rlx4181_flush_icache_range;
	local_flush_icache_range = rlx4181_flush_icache_range;
	__flush_icache_user_range = rlx4181_flush_icache_range;
	__local_flush_icache_user_range = rlx4181_flush_icache_range;

	__flush_kernel_vmap_range = rlx4181_flush_kernel_vmap_range;

	local_flush_data_cache_page = local_rlx4181_flush_data_cache_page;
	flush_data_cache_page = rlx4181_flush_data_cache_page;

	_dma_cache_wback_inv = rlx4181_dma_cache_wback_inv;
	_dma_cache_wback = rlx4181_dma_cache_wback_inv;
	_dma_cache_inv = rlx4181_dma_cache_wback_inv;

	pr_info("Primary instruction cache %dkB, linesize %d bytes.\n",
		icache_size >> 10, icache_lsize);
	pr_info("Primary data cache %dkB, linesize %d bytes.\n",
		dcache_size >> 10, dcache_lsize);

	build_clear_page();
	build_copy_page();
}
