// SPDX-License-Identifier: GPL-2.0
/*
 * Realtek RTL8196E SoC timer driver.
 *
 * Timer0 (32bit): Used as clocksource
 * Timer1 (32bit): Used as clock event device
 */

#include <linux/init.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/sched_clock.h>
#include <linux/of_clk.h>
#include <linux/io.h>

#include "timer-of.h"

/* Timer registers */
#define TCCNR			0x10
#define TCIR			0x14
#define TC0_DATA		0x00
#define TC1_DATA		0x04
#define TC0_CNT			0x08
#define TC1_CNT			0x0C

/* TCCNR register bits */
#define TCCNR_TC0_EN_BIT		BIT(31)
#define TCCNR_TC1_EN_BIT		BIT(29)
#define TCCNR_TC0_MODE_BIT		BIT(30)
#define TCCNR_TC1_MODE_BIT		BIT(28)

/* TCIR register bits */
#define TCIR_TC0_IE_BIT		BIT(31)
#define TCIR_TC1_IE_BIT		BIT(30)
#define TCIR_TC0_IP_BIT		BIT(29)
#define TCIR_TC1_IP_BIT		BIT(28)

/* Forward declaration */
static struct timer_of to;

static void __iomem *base;

#define RTL8196E_TIMER_MODE_COUNTER	0
#define RTL8196E_TIMER_MODE_TIMER	1

static void rtl8196e_set0_enable_bit(int enabled)
{
	u16 tccnr;

	tccnr = readl(base + TCCNR);
	tccnr &= ~(TCCNR_TC0_EN_BIT);

	if (enabled)
		tccnr |= TCCNR_TC0_EN_BIT;

	writel(tccnr, base + TCCNR);
}

static void rtl8196e_set1_enable_bit(int enabled)
{
	u16 tccnr;

	tccnr = readl(base + TCCNR);
	tccnr &= ~(TCCNR_TC1_EN_BIT);

	if (enabled)
		tccnr |= TCCNR_TC1_EN_BIT;

	writel(tccnr, base + TCCNR);
}

static void rtl8196e_set0_mode_bit(int mode)
{
	u16 tccnr;

	tccnr = readl(base + TCCNR);
	tccnr &= ~(TCCNR_TC0_MODE_BIT);

	if (mode)
		tccnr |= TCCNR_TC0_MODE_BIT;

	writel(tccnr, base + TCCNR);
}

static void rtl8196e_set1_mode_bit(int mode)
{
	u16 tccnr;

	tccnr = readl(base + TCCNR);
	tccnr &= ~(TCCNR_TC1_MODE_BIT);

	if (mode)
		tccnr |= TCCNR_TC1_MODE_BIT;

	writel(tccnr, base + TCCNR);
}

static irqreturn_t rtl8196e_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *cd = dev_id;
	int status;

	status = readl(base + TCIR);
	writel(status, base + TCIR); /* Clear all interrupts */

	cd->event_handler(cd);

	return IRQ_HANDLED;
}

static int rtl8196e_clockevent_set_next(unsigned long evt,
				       struct clock_event_device *cd)
{
	rtl8196e_set1_enable_bit(0);
	writel(evt, base + TC1_DATA);
	writel(evt, base + TC1_CNT);
	rtl8196e_set1_enable_bit(1);
	return 0;
}

static int rtl8196e_set_state_periodic(struct clock_event_device *cd)
{
	unsigned long period = timer_of_period(to_timer_of(cd));

	rtl8196e_set1_enable_bit(0);
	rtl8196e_set1_mode_bit(RTL8196E_TIMER_MODE_TIMER);

	/* This timer should reach zero each jiffy */
	writel(period, base + TC1_DATA);
	writel(period, base + TC1_CNT);

	rtl8196e_set1_enable_bit(1);
	return 0;
}

static int rtl8196e_set_state_oneshot(struct clock_event_device *cd)
{
	rtl8196e_set1_enable_bit(0);
	rtl8196e_set1_mode_bit(RTL8196E_TIMER_MODE_COUNTER);
	return 0;
}

static int rtl8196e_set_state_shutdown(struct clock_event_device *cd)
{
	rtl8196e_set1_enable_bit(0);
	return 0;
}

static void rtl8196e_timer_init_hw(void)
{
	/* Disable all timers */
	writel(0, base + TCCNR);

	/* Clear and disable all timer interrupts */
	writel(0xf0, base + TCIR);

	/* Reset all timers timeouts */
	writel(0, base + TC0_DATA);
	writel(0, base + TC1_DATA);

	/* Reset all counters */
	writel(0, base + TC0_CNT);
	writel(0, base + TC1_CNT);
}

static u64 notrace rtl8196e_timer_sched_read(void)
{
	return ~readl(base + TC0_CNT);
}

static int rtl8196e_start_clksrc(void)
{
	/* We use Timer0 as a clocksource (monotonic counter). */
	writel(0xFFFFFFFF, base + TC0_DATA);
	writel(0xFFFFFFFF, base + TC0_CNT);

	rtl8196e_set0_mode_bit(RTL8196E_TIMER_MODE_TIMER);
	rtl8196e_set0_enable_bit(1);

	sched_clock_register(rtl8196e_timer_sched_read, 32, timer_of_rate(&to));

	return clocksource_mmio_init(base + TC0_CNT, "rtl8196e-clksrc",
				     timer_of_rate(&to), 500, 32,
				     clocksource_mmio_readl_down);
}

static struct timer_of to = {
	.flags = TIMER_OF_BASE | TIMER_OF_CLOCK | TIMER_OF_IRQ,

	.clkevt = {
			.name = "rtl8196e_tick",
			.rating = 200,
			.features = CLOCK_EVT_FEAT_ONESHOT |
				    CLOCK_EVT_FEAT_PERIODIC,
			.set_next_event = rtl8196e_clockevent_set_next,
			.cpumask = cpu_possible_mask,
			.set_state_periodic = rtl8196e_set_state_periodic,
			.set_state_oneshot = rtl8196e_set_state_oneshot,
			.set_state_shutdown = rtl8196e_set_state_shutdown,
	},

	.of_irq = {
			.handler = rtl8196e_timer_interrupt,
			.flags = IRQF_TIMER,
	},
};

static int __init rtl8196e_timer_init(struct device_node *node)
{
	int ret;

	ret = timer_of_init(node, &to);
	if (ret)
		return ret;

	base = timer_of_base(&to);

	rtl8196e_timer_init_hw();

	ret = rtl8196e_start_clksrc();
	if (ret) {
		pr_err("Failed to register clocksource\n");
		return ret;
	}

	clockevents_config_and_register(&to.clkevt, timer_of_rate(&to), 100,
					0xffffffff);

	/* Enable interrupts for Timer3. Disable interrupts for others */
	writel(TCIR_TC1_IE_BIT, base + TCIR);

	return 0;
}

TIMER_OF_DECLARE(rtl8196e_timer, "realtek,rtl8196e-timer", rtl8196e_timer_init);
