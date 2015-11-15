#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <uapi/asm-generic/ioctl.h>
#include <linux/completion.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <asm/barrier.h>
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

uint8_t BUF_MAX = 5;

struct communique {
	const char *name;
	struct cdev *cdev;
	dev_t dev;
	struct class *class;
	struct file_operations fops;
	struct device *device;
	struct list_head event_list;
};

static struct communique cmc;

struct event {
	char *name;
	struct completion waiting;
	struct list_head list_element;
	pid_t p_emits[16];
	int p_emits_cnt;
	pid_t p_waits[16];
	int p_waits_cnt;
};

void communique_destroy(struct communique *cmc)
{
	struct list_head *pos = NULL, *temp = NULL;
	struct event *event = NULL;
	list_for_each_safe(pos, temp, &cmc->event_list){
		event = list_entry(pos, struct event, list_element);
		complete_all(&event->waiting);
		list_del(pos);
		kfree(event->name);
		kfree(event);
	}	
}

int communique_search_pid(pid_t *tab, pid_t proc)
{	
	for(int i = 0; i < 16; i++) {
		if (tab[i] == proc)
			return i;
	}
	return -1;
}

int communique_release(struct inode *inode_s, struct file *file_s)
{
	return 0;
}

int communique_open(struct inode *inode_s, struct file *file_s)
{
	return 0;
}

struct event * communique_search_by_name(struct communique *cmc, char *name)
{
	struct list_head *pos = NULL;
	struct event *event = NULL;
	list_for_each(pos, &cmc->event_list) {
		event = list_entry(pos, struct event, list_element);
		if (strncmp(event->name, name, strlen(name)) == 0)
			return event;
	}
	return NULL;
}

int communique_delete_event(struct event *event)
{
	int i = 0;
	if (event->p_waits_cnt)
		return -EAGAIN;
	i = communique_search_pid(event->p_emits, current->pid);
	if (i < 0)
		return -EINVAL;
	event->p_emits_cnt--;
	if (event->p_emits_cnt) {
		event->p_emits[i] = 0;
	} else {
		list_del(&event->list_element);	
		if (event->name != NULL)
			kfree(event->name);
		kfree(event);
	}
	return 0;
}

char *communique_get_name(const char __user *buf)
{
	int rt;
	char *name = kmalloc(BUF_MAX, GFP_KERNEL);
	if (name == NULL)
		return NULL;
	memset(name, 0, BUF_MAX);
	rt = copy_from_user(name, buf, BUF_MAX);
	if (rt) {
		kfree(name);
		return NULL;
	}
	return name;
}

int communique_set(struct communique *cmc, const char __user *buf)
{
	struct event *temp = NULL;
	char *name = communique_get_name(buf);
	if (name == NULL)
		return -ENOMEM;
	temp = communique_search_by_name(cmc, name);
	if (temp == NULL) {
		temp = kmalloc(sizeof(struct event), GFP_KERNEL);
		if (temp == NULL) {
			kfree(name);
			return -ENOMEM;
		}
		memset(temp, 0, sizeof(struct event));
		init_completion(&(temp->waiting));
		temp->name = name;
		INIT_LIST_HEAD(&temp->list_element);
		list_add(&temp->list_element, &cmc->event_list);
	} else {
		kfree(name);
		if (communique_search_pid(temp->p_emits, current->pid) != -1)
			return -EINVAL;
	}
	temp->p_emits[temp->p_emits_cnt] = current->pid;	
	temp->p_emits_cnt++;
	return 0;
}

int communique_unset(struct communique *cmc, const char __user *buf)
{
	int rt;
	struct event *temp = NULL;
	char *name = communique_get_name(buf);
	if (name == NULL)
		return -ENOMEM;
	temp = communique_search_by_name(cmc, name);
	kfree(name);
	if (temp == NULL) {
		return -EINVAL;
	}
	rt = communique_delete_event(temp);
	return rt;	
}

int communique_check_deadlocks(struct communique *cmc, struct event *event)
{
	return 0;
}

struct event *communique_get_name_and_search(struct communique *cmc,
					     const char __user *buf)
{
	struct event *event = NULL;
	char *name = communique_get_name(buf);
	if (name == NULL)
		return NULL;
	event = communique_search_by_name(cmc, name);
	kfree(name);
	return event;
}

int communique_wait(struct communique *cmc, const char __user *buf)
{
	int rt = 0;
	struct event *event = communique_get_name_and_search(cmc, buf);
	if (event == NULL)
		return -EINVAL;
	rt = wait_for_completion_interruptible(&(event->waiting));
	if (rt) 
		return -EINTR;
	return 0;
}

int communique_throw(struct communique *cmc, const char __user *buf)
{
	int rt;
	struct event *event = communique_get_name_and_search(cmc, buf);
	if (event == NULL)
		return -EINVAL;
	rt = communique_search_pid(event->p_emits, current->pid);
	if (rt < 0)
		return -EINVAL;
	complete_all(&(event->waiting));
	mb();
	reinit_completion(&(event->waiting));
	return 0;
}

long communique_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case SETEVENT:
		return communique_set(&cmc, (char __user *)arg);
	case WAITFOREVENT:
		return communique_wait(&cmc, (char __user *)arg);
	case THROWEVENT:
		return communique_throw(&cmc, (char __user *)arg);
	case UNSETEVENT:
		return communique_unset(&cmc, (char __user *)arg);
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
	.device = NULL,
	.event_list = LIST_HEAD_INIT(cmc.event_list)
};

static int __init communique_init(void)
{
	int rt;
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
	communique_destroy(&cmc);
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
}

module_init(communique_init);
module_exit(communique_exit);
