// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>

#include <asm/setup.h>
#include <asm/mach-rtl8196e/rtl8196e.h>

void __init prom_init(void)
{
	setup_8250_early_printk_port((unsigned long)RTL8196E_UART0_BASE, 2,
				     10000);
}

void __init prom_free_prom_memory(void)
{
}
