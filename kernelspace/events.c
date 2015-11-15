#include <linux/errno.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <uapi/asm-generic/ioctl.h>
#include <linux/completion.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");

/*
 * NOTES: flex_array
 */

#ifdef DEBUG
#define debug_message() printk(KERN_EMERG "DEBUG %s %d \n", __FUNCTION__, \
			       __LINE__)
#else
#define debug_message()
#endif

#define SETEVENT _IOW(0x8A, 0x01, char __user *)
#define WAITFOREVENT _IOW(0x8A, 0x02, char __user *)
#define THROWEVENT _IOW(0x8A, 0x03, char __user *)
#define UNSETEVENT _IOW(0x8A, 0x04, char __user *)

uint8_t glob_name_size = 2;
uint8_t glob_event_cnt_max = 5;

struct event {
	char *name;
	struct completion waiting;
	struct list_head element;
};

struct events {
	const char *driver_name;
	struct cdev *cdev;
	dev_t dev;
	struct class *class;
	struct file_operations fops;
	struct device *device;
	struct list_head event_list;
	struct event *event;
	uint8_t event_cnt;
};

static struct events cmc;

int events_release(struct inode *inode, struct file *file)
{
	return 0;
}

int events_open(struct inode *inode, struct file *file)
{
	return 0;
}

int events_unset(struct events *cmc, const char __user *buf)
{
	struct event *event;
	event = cmc->event;
	kfree(event->name);
	return 0;
}

int events_set(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	char *name = kmalloc(sizeof(char) * glob_name_size, GFP_KERNEL);
	if (name == NULL)
		return -ENOMEM;
	rt = copy_from_user(name, buf, glob_name_size);
	if (rt) {
		kfree(name);
		return -EAGAIN;
	}
	event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (event == NULL) {
		kfree(name);
		return -ENOMEM;
	}
	cmc->event_cnt++;
	event->name = name;
	INIT_LIST_HEAD(&event->element);
	list_add(&event->element, &cmc->event_list);
	cmc->event = event;
	return 0;
}

int events_wait(struct events *cmc, const char __user *buf)
{
	struct event *event;
	int rt;
	event = cmc->event;
	init_completion(&event->waiting);
	rt = wait_for_completion_interruptible(&event->waiting);
	if (rt)
		return -EINTR;
	return 0;
}

int events_throw(struct events *cmc, const char __user *buf)
{
	struct event *event;
	event = cmc->event;
	complete_all(&event->waiting);
	return 0;
}

long events_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case SETEVENT:
		return events_set(&cmc, (char __user *)arg);
	case WAITFOREVENT:
		return events_wait(&cmc, (char __user *)arg);
	case THROWEVENT:
		return events_throw(&cmc, (char __user *)arg);
	case UNSETEVENT:
		return events_unset(&cmc, (char __user *)arg);
	default:
		return -EINVAL;
	}
}

static struct events cmc = {
	.driver_name = "events",
	.cdev = NULL,
	.dev = 0,
	.class = NULL,
	.fops = {.open = events_open,
		 .release = events_release,
		 .unlocked_ioctl = events_ioctl,
		 .compat_ioctl = events_ioctl
		},
	.device = NULL,
	.event_list = LIST_HEAD_INIT(cmc.event_list),
	.event_cnt = 0
};

static int __init events_init(void)
{
	int rt;
	rt = alloc_chrdev_region(&cmc.dev, 0, 1, cmc.driver_name);
	if (rt)
		return rt;

	cmc.class = class_create(THIS_MODULE, cmc.driver_name);
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

	cmc.device = device_create(cmc.class, NULL, cmc.dev, NULL, 
				   cmc.driver_name);
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

static void __exit events_exit(void)
{
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
}

module_init(events_init);
module_exit(events_exit);
