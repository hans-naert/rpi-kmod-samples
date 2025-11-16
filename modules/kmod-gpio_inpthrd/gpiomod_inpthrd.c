/*
 * Basic Linux Kernel module using GPIO interrupts and kthread.
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
#include <linux/gpio.h>
#include <linux/interrupt.h> 
#include <linux/kthread.h>
#include <linux/delay.h>

/* Define LEDs */
static struct gpio leds[] = {
		{  4 + 512, GPIOF_OUT_INIT_LOW, "LED 1" },
		{ 25 + 512, GPIOF_OUT_INIT_LOW, "LED 2" },
};

/* Define BUTTONs */
static struct gpio buttons[] = {
		{ 17 + 512, GPIOF_IN, "BUTTON 1" },	// Blink LED 1 faster
		{ 18 + 512, GPIOF_IN, "BUTTON 2" },	// Blink LED 1 slower
};

/* IRQs assigned to BUTTONs */
static int button_irqs[] = { -1, -1 };

/* Initial blink delay */
static int blink_delay = 1000;

/* Task handle to identify thread */ 
static struct task_struct *ts = NULL;

/* 
 * Thread to blink LED 2 
 */
static int led_thread(void *data)
{
	printk(KERN_INFO "%s\n", __func__);

	// loop until killed ...
	while(!kthread_should_stop()) {

		gpio_set_value(leds[1].gpio, !gpio_get_value(leds[1].gpio));

		mdelay(blink_delay);
	}

	return 0;
}

/* 
 * Interrupt handler for the BUTTONs 
 */
static irqreturn_t button_isr(int irq, void *data)
{
	// indicate button press
	gpio_set_value(leds[0].gpio, 1);

	if(irq == button_irqs[0]) {
			// make LED blink faster
			blink_delay = 500;
	}
	else if(irq == button_irqs[1]) {
			// make LED blink slower
			blink_delay = 1500;
	}

	printk("Delay: %d\n", blink_delay);

	// remove button press indicator
	gpio_set_value(leds[0].gpio, 0);

	return IRQ_HANDLED;
}

/*
 * Module init function
 */
static int __init gpiomod_init(void)
{
	int ret = 0;
	int i;

	printk(KERN_INFO "GPIO Interrupts init\n");
	
	// register LED gpios
	for (i = 0; i < ARRAY_SIZE(leds); i++) {
		ret = gpio_request(leds[i].gpio, leds[i].label);
		if (ret) {
			printk(KERN_ERR "Unable to request GPIO %d: %d\n", leds[i].gpio, ret);
			goto err_free_leds;
		}
		gpio_direction_output(leds[i].gpio, (leds[i].flags & GPIOF_OUT_INIT_LOW) ? 0 : 1);
	}
	
	// register BUTTON gpios
	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		ret = gpio_request(buttons[i].gpio, buttons[i].label);
		if (ret) {
			printk(KERN_ERR "Unable to request GPIO %d: %d\n", buttons[i].gpio, ret);
			goto err_free_buttons;
		}
		gpio_direction_input(buttons[i].gpio);
	}

	printk(KERN_INFO "Current button1 value: %d\n", gpio_get_value(buttons[0].gpio));
	
	ret = gpio_to_irq(buttons[0].gpio);

	if(ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto err_free_buttons;
	}

	button_irqs[0] = ret;

	printk(KERN_INFO "Successfully requested BUTTON1 IRQ # %d\n", button_irqs[0]);

	/* trigger on rising edge, don't accept IRQs while already handling an IRQ */
	ret = request_irq(button_irqs[0], button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#button1", NULL);

	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto err_free_buttons;
	}

	ret = gpio_to_irq(buttons[1].gpio);

	if(ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto err_free_buttons;
	}
		
	button_irqs[1] = ret;

	printk(KERN_INFO "Successfully requested BUTTON2 IRQ # %d\n", button_irqs[1]);

	/* trigger on rising edge, don't accept IRQs while already handling an IRQ */
	ret = request_irq(button_irqs[1], button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#button2", NULL);

	if(ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	ts = kthread_create(led_thread, NULL, "led_thread");

	if(ts) {
		wake_up_process(ts);
    }
	else {
		printk(KERN_ERR "Unable to create thread\n");
		goto fail3;
	}
			
	return 0;

// cleanup what has been setup so far
fail3:
	free_irq(button_irqs[0], NULL);

err_free_buttons:
	// Free any button GPIOs that were successfully requested
	while (--i >= 0) {
		gpio_free(buttons[i].gpio);
	}
	i = ARRAY_SIZE(leds);

err_free_leds:
	// Free any LED GPIOs that were successfully requested
	while (--i >= 0) {
		gpio_free(leds[i].gpio);
	}

	return ret;	
}

/*
 * Module exit function
 */
static void __exit gpiomod_exit(void)
{
	int i;

	printk(KERN_INFO "GPIO Interrupts exit\n");

	// terminate thread
	if(ts) {
		kthread_stop(ts);
	}

	// free irqs
	free_irq(button_irqs[0], NULL);
	free_irq(button_irqs[1], NULL);
	
	// turn all LEDs off
	for(i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_set_value(leds[i].gpio, 0); 
	}
	
	// unregister GPIOs
	for(i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_free(leds[i].gpio);
	}
	for(i = 0; i < ARRAY_SIZE(buttons); i++) {
		gpio_free(buttons[i].gpio);
	}
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wendler");
MODULE_DESCRIPTION("Basic Linux Kernel module using GPIO interrupts and kthread.");

module_init(gpiomod_init);
module_exit(gpiomod_exit);
