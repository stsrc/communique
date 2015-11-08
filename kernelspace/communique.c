#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h> 
#include <linux/device.h>
#include <linux/err.h>
#include <linux/keyboard.h>
#include <linux/reboot.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>

#include <linux/kernel.h>
#include <uapi/asm-generic/ioctl.h>

MODULE_LICENSE("GPL");

#ifdef DEBUG
#define debug_message() printk(KERN_EMERG "DEBUG %s %d \n", __FUNCTION__, \
			       __LINE__)
#else
#define debug_message()
#endif

#define SETEVENT _IOW('|', 0x70, char __user *)
#define WAITFOREVENT _IOW('|', 0x71, char __user *)
#define THROWEVENT _IOW('|', 0x72, char __user *)

struct communique {
	const char *name;
	struct cdev *cdev;
	dev_t dev;
	struct class *class;
	struct file_operations fops;
	struct device *device;
};

struct process_relation {
	pid_t pid;
	uint8_t throws;
	uint8_t reacts;
	uint8_t sleeps;
};

int communique_release(struct inode *inode_s, struct file *file_s)
{
	debug_message();
	return 0;
}


int communique_open(struct inode *inode_s, struct file *file_s)
{
	debug_message();
	return 0;
}

int communique_cp_prs(int *to, const char __user *from, uint8_t n)
{
	debug_message();

	int rt;
	char temp[3 * sizeof(int)];
	if (n > sizeof(temp)) 
		return -EFAULT;
	rt = copy_from_user(temp, from, n);
	if (rt)
		return -EAGAIN;
	sscanf(temp, "%d%d%d", to, (to + 4), (to + 8));
	
	printk(KERN_EMERG "DEBUG: %d, %d, %d\n", to[0], to[1], to[2]);

	return 0;
}

int communique_register(const char __user *args)
{
	debug_message();
	int rt = 0;
	int data[3];
	rt = communique_cp_prs(data, args, 3);
	return 0;
}

int communique_sleep(const char __user *args)
{
	debug_message();
	return -EDEADLK;
}

int communique_throw(const char __user *args)
{
	debug_message();
	return 1;
}

long communique_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	debug_message();
	printk(KERN_EMERG "SETEVENT: %lu, WAITFOREVENT: %lu, THROWEVENT: %lu\n"
	       , SETEVENT, WAITFOREVENT, THROWEVENT);
	printk(KERN_EMERG "command: %lu\n", (unsigned long)cmd);
	switch(cmd) {
	case SETEVENT:
		return communique_register((char __user *)arg);
	case WAITFOREVENT:
		return communique_sleep((char __user *)arg);
	case THROWEVENT:
		return communique_throw((char __user *)arg);
	default:
		return -EINVAL;
	}

}

static struct communique cmc = {
	.name = "communique",
	.cdev = NULL,
	.dev = 0,
	.class = NULL,
	.fops = {.open = communique_open,
		 .release = communique_release,
		 .unlocked_ioctl = communique_ioctl,
		 .compat_ioctl = communique_ioctl
		},
	.device = NULL
};

static int __init communique_init(void)
{
	int rt;
	debug_message();	
	rt = alloc_chrdev_region(&cmc.dev, 0, 1, cmc.name);
	if (rt)
		return rt;

	cmc.class = class_create(THIS_MODULE, cmc.name);
	if (IS_ERR(cmc.class)) {
		rt = PTR_ERR(cmc.class);
		goto err;
	}

	cmc.cdev = cdev_alloc();
	if (!cmc.cdev) {
		rt = -ENOMEM;
		goto err;
	}

	cdev_init(cmc.cdev, &cmc.fops);
	rt = cdev_add(cmc.cdev, cmc.dev, 1);
	if (rt) {
		kfree(cmc.cdev);
		cmc.cdev = NULL;
		goto err;
	}

	cmc.device = device_create(cmc.class, NULL, cmc.dev, NULL, cmc.name);
	if (IS_ERR(cmc.device)) {
		rt = PTR_ERR(cmc.device);
		goto err;
	}
	return 0;
err:	
	if (cmc.cdev) {
		cdev_del(cmc.cdev);		
		cmc.cdev = NULL;
	}
	if (cmc.class) {
		class_destroy(cmc.class);
		cmc.class = NULL;	
	}
	unregister_chrdev_region(cmc.dev, 1);
	return rt;
}

static void __exit communique_exit(void)
{
	debug_message();
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
}

module_init(communique_init);
module_exit(communique_exit);
