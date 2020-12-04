/*
 * Basic Linux Kernel module using a tasklet to blink LEDs.
 *
 * Author:
 * 	Stefan Wendler (devnull@kaltpost.de)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>	
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h> 

#include "query_ioctl.h"


#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static int status = 1, dignity = 3, ego = 5;
 
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    query_arg_t q;
 
    switch (cmd)
    {
        case QUERY_GET_VARIABLES:
            q.status = status;
            q.dignity = dignity;
            q.ego = ego;
            if (copy_to_user((query_arg_t *)arg, &q, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            break;
        case QUERY_CLR_VARIABLES:
            status = 0;
            dignity = 0;
            ego = 0;
            break;
        case QUERY_SET_VARIABLES:
            if (copy_from_user(&q, (query_arg_t *)arg, sizeof(query_arg_t)))
            {
                return -EACCES;
            }
            status = q.status;
            dignity = q.dignity;
            ego = q.ego;
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};

/* Define pins, directin and inital state of GPIOs for LEDs */
static struct gpio leds[] = {
		{  4, GPIOF_OUT_INIT_LOW, "LED 1" },
		{ 25, GPIOF_OUT_INIT_LOW, "LED 2" },
		{ 24, GPIOF_OUT_INIT_LOW, "LED 3" },
};

/* Tasklet to blink the LEDs */
static void blink_tasklet(unsigned long data)
{
	int i;

	printk(KERN_INFO "%s\n", __func__);

	printk("Tasklet started\n");

	for(i = 0; i < ARRAY_SIZE(leds); i++) {

		if(i - 1 >= 0) {
			gpio_set_value(leds[i - 1].gpio, 0); 
		}
		gpio_set_value(leds[i].gpio, 1); 
		mdelay(500);
	}

	gpio_set_value(leds[i - 1].gpio, 0); 

	printk("Tasklet ended\n");	
}

DECLARE_TASKLET(tl_descr, blink_tasklet, 0L);

/*
 * Module init function
 */
static int __init gpiomod_init(void)
{
	int ret = 0;

	printk(KERN_INFO "%s\n", __func__);

	// register, turn on
	ret = gpio_request_array(leds, ARRAY_SIZE(leds));

	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
		return ret;
	}

	tasklet_schedule(&tl_descr);

	return ret;
}

/*
 * Module exit function
 */
static void __exit gpiomod_exit(void)
{
	int i;

	printk(KERN_INFO "%s\n", __func__);

	tasklet_kill(&tl_descr);

	// turn all off
	for(i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_set_value(leds[i].gpio, 0); 
	}
	
	// unregister
	gpio_free_array(leds, ARRAY_SIZE(leds));
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wendler");
MODULE_DESCRIPTION("Basic Linux Kernel module using a tasklet to blink LEDs");

module_init(gpiomod_init);
module_exit(gpiomod_exit);
