#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>

/* P8.8 = GPIO2.3 */

#define LED 			3

/* gpio setup */

#define GPIO2_BASE_START	0x481AC000 
#define GPIO2_BASE_STOP		0x481ACFFF 
#define GPIO_DATAIN		0x138
#define GPIO_DATAOUT		0x13C
#define GPIO_SETDATAOUT		0x194
#define GPIO_CLEARDATAOUT	0x190
#define GPIO_OE			0x134

/* control mode */

#define CONTROL_MODE_START	0x44E10000
#define CONTROL_MODE_STOP	0x44E11FFF

#define P8_7_MODE_OFFSET 	0x890
#define INPUT_PULL_DOWN		0X27 



static dev_t dev;
static struct cdev my_cdev;
static struct device *device;
static struct class *class;
static char *msg = NULL;
static void __iomem *io;
static unsigned long temp;

static int dev_open(struct inode *, struct file *);
static int dev_close(struct inode *, struct file *);
static ssize_t dev_read(struct file*, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);


static void gpio_init(void)
{	
	/* set mode ouput & input function for p8.7 */

	io = (unsigned int *)ioremap(CONTROL_MODE_START, CONTROL_MODE_STOP - CONTROL_MODE_START);
	if(io == NULL)
	{
		printk(KERN_ALERT "Can not map to to virtual addr control mode\n");
		return -ENOMEM;
	}
	iowrite32(INPUT_PULL_DOWN, (io + P8_7_MODE_OFFSET)); 
	
	/* setup gpio */
	
	io = ioremap(GPIO2_BASE_START, GPIO2_BASE_STOP - GPIO2_BASE_START);
	if(io == NULL)
	{
		printk(KERN_ALERT "Can not map to to virtual addr gpio setup\n");
		return -ENOMEM;
	}
	
	/* set mode output led */
	/* save state of this port */
	iowrite32(~0u, (io + GPIO_OE));
	temp = ioread32(io + GPIO_OE);	
	
	/* enable output at pin LED */
	temp &= ~(1 << LED);
	iowrite32(temp, (io + GPIO_OE));

}

static void __set_value_led(int value)
{
		temp = ioread32(io + GPIO_DATAOUT);
		
		if(value)
		{
			printk(KERN_INFO "set pin high\n");
			temp |= ( 1 << LED);
			iowrite32(temp , (io + GPIO_SETDATAOUT));
		}
		else
		{
			printk(KERN_INFO "set pin low\n");
			temp |= ( 1 << LED);
			iowrite32(temp ,(io + GPIO_CLEARDATAOUT));
		}
}

static unsigned int __get_value_led(void)
{
	return (*((unsigned int*)(io + GPIO_DATAOUT)))&(1<<LED);
}


static struct file_operations fops =
{
	.open = dev_open,
	.release = dev_close,
	.read = dev_read,
	.write = dev_write
};

static int dev_open(struct inode *inode, struct file *fp)
{
	
	printk(KERN_INFO "Handle opend event \n");
	return 0;
}

static int dev_close(struct inode *inode, struct file *fp)
{
	printk(KERN_INFO "Driver: close()\n");
	return 0;
}

static ssize_t dev_read(struct file *fp, char __user *buf, size_t len, loff_t *off)
{
	char led_value;
	short count;

	led_value = __get_value_led();
	msg[0] = led_value;
	len = 1;

	count = copy_to_user(buf, msg, len);
	printk("gpio_led=%d\n", __get_value_led());;
	return 0;
}

static ssize_t dev_write(struct file *fp, const char __user *buf, size_t len, loff_t *off)
{
	short count;

	memset(msg, 0, strlen(msg));
	count = copy_from_user(msg, buf, len);

	if (msg[0] == '1')
	{
		__set_value_led(1);
	}
	else if (msg[0] == '0')
	{
		__set_value_led(0);
	}
	else
	{
		printk(KERN_INFO "Unknown command, 1 or 0\n");
	}
	printk(KERN_INFO "Driver: write()\n");
	return len;
}

static int __init bbb_led_init(void)
{
	gpio_init();
	__get_value_led(1);
	
	int ret;
	printk(KERN_INFO "Registered bbb_led!\n");
	ret = alloc_chrdev_region(&dev, 0, 1, "blink");
	if (ret < 0)
	{
		printk(KERN_INFO "Failed to alloc_chrdev_region!\n");
		return -EFAULT;
	}

	class = class_create(THIS_MODULE, "blink_class");
	if (IS_ERR(class))
	{
		printk(KERN_INFO "Failed to create class device!\n");
		unregister_chrdev_region(dev, 1);
		return -EFAULT;
	}

	devive = device_create(class, NULL, dev, NULL, "blink_led");
	if (IS_ERR(device))
	{
		printk(KERN_INFO "Failed to create the device!\n");
		class_destroy(class);
		return -EFAULT;
	}

	cdev_init(&my_cdev, &fops);
	ret = cdev_add(&my_cdev, dev, 1);
	if (ret < 0)
	{
		printk(KERN_INFO "Failed to add cdev!\n");
		device_destroy(class, dev);
		return ret;
	}

	msg = (char *)kmalloc(32, GFP_KERNEL);
	if (msg != NULL)
	{
		printk(KERN_INFO "malloc allocator address: 0x%p\n", msg);
	}

}

static void __exit bbb_led_exit(void)
{
	__set_value_led(0);
	if (msg)
	{
		kfree(msg);
	}
	cdev_del(&my_cdev);
	device_destroy(class, dev);
	class_destroy(class);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "Unregisterd chrdev\n");
}

module_init(bbb_led_init);
module_exit(bbb_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KhoeVV");
MODULE_DESCRIPTION("BBB led blink");


