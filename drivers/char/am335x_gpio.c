#include <linux/init.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <mach/gpio.h>
#include <plat/mux.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#define GPIO_IOC_MAGIC					'G'
#define IOCTL_GPIO_SETOUTPUT			_IOW(GPIO_IOC_MAGIC, 0, int)                   
#define IOCTL_GPIO_SETINPUT				_IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE				_IOW(GPIO_IOC_MAGIC, 2, int) 
#define IOCTL_GPIO_GETVALUE				_IOR(GPIO_IOC_MAGIC, 3, int)
#define GPIO_IOC_MAXNR					3

struct am335x_gpio_arg{
        int pin;
        int data;
};
 
struct am335x_gpio_node{
        int pin;
        struct am335x_gpio_node *next;
};

static int gpio_major = 0;
static int gpio_minor = 0;
static struct class *gpio_class;
static struct class_device *class_dev;
static struct cdev gpio_cdev;
static struct am335x_gpio_node *head;

static int is_required(int pinnum)
{	
	struct am335x_gpio_node *pnode = head->next;
	struct am335x_gpio_node *rnode = head;

	while(pnode != NULL)
	{
		if(pnode->pin == pinnum)
			return 0;
		else
		{
			rnode = pnode;
			pnode = pnode->next;
		}
	}
	printk("New GPIO Registered!\n");
	pnode = (struct am335x_gpio_node *)kmalloc(sizeof(struct am335x_gpio_node), GFP_KERNEL);
	if(pnode != NULL)
	{
		rnode->next = pnode;
		pnode->pin = pinnum;
		pnode->next = NULL;
	}
	else
		printk("Can't Kmalloc...\n");
	
	return 1;

}

static int gpio_open(struct inode *inode,struct file *filp)
{
	return 0;
}

static int gpio_release(struct inode *inode,struct file *filp)
{
	return 0;
}


static long gpio_ioctl(struct file *filp,  unsigned int cmd,  unsigned long arg)
{
	int err = 0;

	
	struct am335x_gpio_arg *parg = (struct am335x_gpio_arg *)arg;  
	

	if(is_required(parg->pin))
	{
		err = gpio_request(parg->pin, "am335x_gpio");
		if(err != 0)
		{	
			printk("Can't request this gpio!\n");
			gpio_free(parg->pin);
		}				
	}
	switch (cmd) {
	case IOCTL_GPIO_SETOUTPUT:
			
			gpio_direction_output(parg->pin, parg->data);
		break;	
	case IOCTL_GPIO_SETINPUT:

			gpio_direction_input(parg->pin);
		break;
	case IOCTL_GPIO_SETVALUE:

			gpio_set_value(parg->pin, parg->data);
		break;
	case IOCTL_GPIO_GETVALUE:

			parg->data = gpio_get_value(parg->pin) ? 1 : 0;
		break;
	default :
			printk(KERN_WARNING "Unsuported Ioctl Cmd\n");
	}
	
	return 0;
	
}

struct file_operations gpio_fops = {
	.owner =    THIS_MODULE,
	.open =     gpio_open,
	.unlocked_ioctl =    gpio_ioctl,
	.release =  gpio_release,
};


static int __init am335x_gpio_init(void)
{
	int result;
	dev_t dev = 0;


	result = alloc_chrdev_region(&dev, gpio_minor, 1,"am335x_gpio");
	gpio_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", gpio_major);
		return result;
	}
	printk("AM335X GPIO Driver Registered.\n");

	cdev_init(&gpio_cdev, &gpio_fops);
	gpio_cdev.owner = THIS_MODULE;
	gpio_cdev.ops = &gpio_fops;
	result = cdev_add (&gpio_cdev, dev, 1);

	gpio_class = class_create(THIS_MODULE, "am335x_gpio");
	if (IS_ERR(gpio_class)) {
        	printk(KERN_WARNING "Class_create faild\n");
		cdev_del(&gpio_cdev);
		unregister_chrdev_region(dev, 0);
		return result;
	}
	class_dev = device_create(gpio_class,NULL,dev,0,"am335x_gpio");

	head = (struct am335x_gpio_node *)kmalloc(sizeof(struct am335x_gpio_node), GFP_KERNEL);
	if(head == NULL)
	{
		return -1;
	}
	else  
    {  
        head->pin = 0;  
        head->next = NULL;    
    }  
	return 0;
}

static void __exit am335x_gpio_exit(void)
{
	dev_t dev = 0;
	struct am335x_gpio_node *pnode = head->next;
	struct am335x_gpio_node *rnode = NULL;
	
	while(pnode != NULL )
    {
		rnode = pnode->next;
        kfree(pnode);
        pnode = rnode;
    }
	kfree(head);
	
	dev = MKDEV(gpio_major, gpio_minor);

	class_unregister(class_dev);
	class_destroy(gpio_class);
	unregister_chrdev_region(dev, 0); 
	cdev_del(&gpio_cdev);
}


module_init(am335x_gpio_init);
module_exit(am335x_gpio_exit);


MODULE_DESCRIPTION("Driver for AM335X GPIO");
MODULE_AUTHOR("lxp@hzmct.com");
MODULE_LICENSE("GPL");




