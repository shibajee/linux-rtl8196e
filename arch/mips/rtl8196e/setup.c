// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>

#include <asm/reboot.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/bootinfo.h>

#include <asm/mach-rtl8196e/rtl8196e.h>

const char *get_system_type(void)
{
	return "Realtek RTL8196E";
}

void rtl8196e_machine_restart(char *command)
{
	/* Disable all interrupts */
	local_irq_disable();

	/* Use watchdog to reset the system */
	writel(0x10, RTL8196E_CDBR);
	writel(0x00, RTL8196E_WDTCNR);

	for (;;)
		;
}

#define GPIO_A0 BIT(0)
#define GPIO_A1 BIT(1)
#define GPIO_A2 BIT(2)
#define GPIO_A3 BIT(3)
#define GPIO_A4 BIT(4)
#define GPIO_A5 BIT(5)
#define GPIO_A6 BIT(6)
#define GPIO_A7 BIT(7)
#define GPIO_B0 BIT(8)
#define GPIO_B1 BIT(9)
#define GPIO_B2 BIT(10)
#define GPIO_B3 BIT(11)
#define GPIO_B4 BIT(12)
#define GPIO_B5 BIT(13)
#define GPIO_B6 BIT(14)
#define GPIO_B7 BIT(15)

/* Temporary hack until rtl8196e-gpio driver is implemented */
void __init rtl8196e_totolink_n100re_setup_leds(void)
{
	unsigned int gpabcddir, gpabcddata;

	gpabcddir = readl(RTL8196E_GPABCDDIR);
	gpabcddata = readl(RTL8196E_GPABCDDATA);

	writel(gpabcddir | (GPIO_A2 | GPIO_A5 | GPIO_A6), RTL8196E_GPABCDDIR);

	gpabcddata &= ~GPIO_A6; /* Turn on A6 - green PWR */
	gpabcddata |= GPIO_A2;  /* Turn off A2 - green WPS */
	writel(gpabcddata, RTL8196E_GPABCDDATA);
}

void __init *plat_get_fdt(void)
{
	if (fw_passed_dtb)
		return (void *)fw_passed_dtb;

	return NULL;
}

void __init plat_mem_setup(void)
{
	void *dtb;

	_machine_restart = rtl8196e_machine_restart;

	dtb = plat_get_fdt();
	if (!dtb)
		panic("no dtb found");

	__dt_setup_arch(dtb);
}

void __init device_tree_init(void)
{
	unflatten_and_copy_device_tree();

	if (of_machine_is_compatible("totolink,n100re"))
		rtl8196e_totolink_n100re_setup_leds();
}
