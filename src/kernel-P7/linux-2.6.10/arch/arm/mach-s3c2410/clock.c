/* linux/arch/arm/mach-s3c2410/clock.c
 *
 * Copyright (c) 2004 Simtec Electronics
 * Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 Clock control support
 *
 * Based on, and code from linux/arch/arm/mach-versatile/clock.c
 **
 **  Copyright (C) 2004 ARM Limited.
 **  Written by Deep Blue Solutions Limited.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>

#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/hardware/clock.h>
#include <asm/arch/regs-clock.h>

#include "clock.h"

/* clock information */

unsigned long s3c24xx_xtal = 12*1000*1000;	/* default 12MHz */
unsigned long s3c24xx_fclk;
unsigned long s3c24xx_hclk;
unsigned long s3c24xx_pclk;

static LIST_HEAD(clocks);
static DECLARE_MUTEX(clocks_sem);


/* old functions */

void inline s3c2410_clk_enable(unsigned int clocks, unsigned int enable)
{
	unsigned long clkcon;
	unsigned long flags;

	local_irq_save(flags);

	clkcon = __raw_readl(S3C2410_CLKCON);
	clkcon &= ~clocks;

	if (enable)
		clkcon |= clocks;

	/* ensure none of the special function bits set */
	clkcon &= ~(S3C2410_CLKCON_IDLE|S3C2410_CLKCON_POWER);

	__raw_writel(clkcon, S3C2410_CLKCON);

	local_irq_restore(flags);
}

/* enable and disable calls for use with the clk struct */

static int clk_null_enable(struct clk *clk, int enable)
{
	return 0;
}

int s3c2410_clkcon_enable(struct clk *clk, int enable)
{
	s3c2410_clk_enable(clk->ctrlbit, enable);
	return 0;
}

/* Clock API calls */

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p;
	struct clk *clk = ERR_PTR(-ENOENT);
	int idno;

	idno = (dev == NULL) ? -1 : to_platform_device(dev)->id;

	down(&clocks_sem);

	list_for_each_entry(p, &clocks, list) {
		if (p->id == idno &&
		    strcmp(id, p->name) == 0 &&
		    try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

	/* check for the case where a device was supplied, but the
	 * clock that was being searched for is not device specific */

	if (IS_ERR(clk)) {
		list_for_each_entry(p, &clocks, list) {
			if (p->id == -1 && strcmp(id, p->name) == 0 &&
			    try_module_get(p->owner)) {
				clk = p;
				break;
			}
		}
	}

	up(&clocks_sem);
	return clk;
}

void clk_put(struct clk *clk)
{
	module_put(clk->owner);
}

int clk_enable(struct clk *clk)
{
	if (IS_ERR(clk))
		return -EINVAL;

	return (clk->enable)(clk, 1);
}

void clk_disable(struct clk *clk)
{
	if (!IS_ERR(clk))
		(clk->enable)(clk, 0);
}


int clk_use(struct clk *clk)
{
	atomic_inc(&clk->used);
	return 0;
}


void clk_unuse(struct clk *clk)
{
	atomic_dec(&clk->used);
}

unsigned long clk_get_rate(struct clk *clk)
{
	if (IS_ERR(clk))
		return 0;

	if (clk->rate != 0)
		return clk->rate;

	while (clk->parent != NULL && clk->rate == 0)
		clk = clk->parent;

	return clk->rate;
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return -EINVAL;
}

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

EXPORT_SYMBOL(clk_get);
EXPORT_SYMBOL(clk_put);
EXPORT_SYMBOL(clk_enable);
EXPORT_SYMBOL(clk_disable);
EXPORT_SYMBOL(clk_use);
EXPORT_SYMBOL(clk_unuse);
EXPORT_SYMBOL(clk_get_rate);
EXPORT_SYMBOL(clk_round_rate);
EXPORT_SYMBOL(clk_set_rate);
EXPORT_SYMBOL(clk_get_parent);

/* base clocks */

static struct clk clk_f = {
	.name		= "fclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

static struct clk clk_h = {
	.name		= "hclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

static struct clk clk_p = {
	.name		= "pclk",
	.id		= -1,
	.rate		= 0,
	.parent		= NULL,
	.ctrlbit	= 0,
};

/* clocks that could be registered by external code */

struct clk s3c24xx_dclk0 = {
	.name		= "dclk0",
	.id		= -1,
};

struct clk s3c24xx_dclk1 = {
	.name		= "dclk1",
	.id		= -1,
};

struct clk s3c24xx_clkout0 = {
	.name		= "clkout0",
	.id		= -1,
};

struct clk s3c24xx_clkout1 = {
	.name		= "clkout1",
	.id		= -1,
};

struct clk s3c24xx_uclk = {
	.name		= "uclk",
	.id		= -1,
};


/* clock definitions */

static struct clk init_clocks[] = {
	{ .name    = "nand",
	  .id	   = -1,
	  .parent  = &clk_h,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_NAND
	},
	{ .name    = "lcd",
	  .id	   = -1,
	  .parent  = &clk_h,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_LCDC
	},
	{ .name    = "usb-host",
	  .id	   = -1,
	  .parent  = &clk_h,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_USBH
	},
	{ .name    = "usb-device",
	  .id	   = -1,
	  //.parent  = &clk_h,		jassi 
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_USBD
	},
	{ .name    = "timers",
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_PWMT
	},
	{ .name    = "sdi",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_SDI
	},
	{ .name    = "uart",
	  .id	   = 0,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_UART0
	},
	{ .name    = "uart",
	  .id	   = 1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_UART1
	},
	{ .name    = "uart",
	  .id	   = 2,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_UART2
	},
	{ .name    = "gpio",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_GPIO
	},
	{ .name    = "rtc",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_RTC
	},
	{ .name    = "adc",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_ADC
	},
	{ .name    = "i2c",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_IIC
	},
	{ .name    = "iis",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_IIS
	},
	{ .name    = "spi",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .enable  = s3c2410_clkcon_enable,
	  .ctrlbit = S3C2410_CLKCON_SPI
	},
	{ .name    = "watchdog",
	  .id	   = -1,
	  .parent  = &clk_p,
	  .ctrlbit = 0
	}
};

/* initialise the clock system */

int s3c2410_register_clock(struct clk *clk)
{
	clk->owner = THIS_MODULE;
	atomic_set(&clk->used, 0);

	if (clk->enable == NULL)
		clk->enable = clk_null_enable;

	/* add to the list of available clocks */

	down(&clocks_sem);
	list_add(&clk->list, &clocks);
	up(&clocks_sem);

	return 0;
}

/* initalise all the clocks */

int __init s3c2410_init_clocks(void)
{
	struct clk *clkp = init_clocks;
	int ptr;
	int ret;

	printk(KERN_INFO "S3C2410 Clock control, (c) 2004 Simtec Electronics\n");

	/* initialise the main system clocks */

	clk_h.rate = s3c24xx_hclk;
	clk_p.rate = s3c24xx_pclk;
	clk_f.rate = s3c24xx_fclk;

	/* it looks like just setting the register here is not good
	 * enough, and causes the odd hang at initial boot time, so
	 * do all of them indivdually.
	 *
	 * I think disabling the LCD clock if the LCD is active is
	 * very dangerous, and therefore the bootloader should be
	 * careful to not enable the LCD clock if it is not needed.
	 *
	 * and of course, this looks neater
	 */

	s3c2410_clk_enable(S3C2410_CLKCON_NAND, 0);
	s3c2410_clk_enable(S3C2410_CLKCON_USBH, 0);
	s3c2410_clk_enable(S3C2410_CLKCON_USBD, 0);
	s3c2410_clk_enable(S3C2410_CLKCON_ADC, 0);
	s3c2410_clk_enable(S3C2410_CLKCON_IIC, 0);
	s3c2410_clk_enable(S3C2410_CLKCON_SPI, 0);

	/* assume uart clocks are correctly setup */

	/* register our clocks */

	if (s3c2410_register_clock(&clk_f) < 0)
		printk(KERN_ERR "failed to register cpu fclk\n");

	if (s3c2410_register_clock(&clk_h) < 0)
		printk(KERN_ERR "failed to register cpu hclk\n");

	if (s3c2410_register_clock(&clk_p) < 0)
		printk(KERN_ERR "failed to register cpu pclk\n");

	for (ptr = 0; ptr < ARRAY_SIZE(init_clocks); ptr++, clkp++) {
		ret = s3c2410_register_clock(clkp);
		if (ret < 0) {
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       clkp->name, ret);
		}
	}

	return 0;
}


