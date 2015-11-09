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
#include <linux/rbtree.h>


MODULE_LICENSE("GPL");

#ifdef DEBUG
#define debug_message() printk(KERN_EMERG "DEBUG %s %d \n", __FUNCTION__, \
			       __LINE__)
#else
#define debug_message()
#endif

#define SETEVENT _IOW(0x8A, 0x01, char __user *)
#define WAITFOREVENT _IOW(0x8A, 0x02, char __user *)
#define THROWEVENT _IOW(0x8A, 0x03, char __user *)

struct communique {
	const char *name;
	struct cdev *cdev;
	dev_t dev;
	struct class *class;
	struct file_operations fops;
	struct device *device;
	struct rb_root proc_rel_tree;
};

struct proc_rel {
	pid_t pid;
	uint8_t event_to_throw;
	uint8_t sleeps;
	struct rb_node node;
};

/*
 * functions implemented specially for rbtree
 */
struct proc_rel *communique_rb_search(struct rb_root *root, int event)
{
	struct rb_node *node = root->rb_node;
	while (node) {
		struct proc_rel *data = rb_entry(node, struct proc_rel, node);
		if (event < data->event_to_throw)
			node = node->rb_left;
		else if (event > data->event_to_throw)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

int communique_rb_insert(struct rb_root *root, struct proc_rel *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	while (*new) {
		struct proc_rel *this = rb_entry(*new, struct proc_rel, node);
		parent = *new;
		if (data->event_to_throw < this->event_to_throw)
			new = &((*new)->rb_left);
		else if (data->event_to_throw > this->event_to_throw)
			new = &((*new)->rb_right);
		else
			return -EINVAL;
	}
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);
	return 0;
}

/*
 * drivers functions
 */

int communique_release(struct inode *inode_s, struct file *file_s)
{
	return 0;
}


int communique_open(struct inode *inode_s, struct file *file_s)
{
	return 0;
}

static inline int communique_cp_prs(int *to, const char __user *from, uint8_t n)
{
	int rt;
	rt = copy_from_user((char *)to, from, n * sizeof(int));
	if (rt)
		return -EAGAIN;
	return 0;
}

static struct communique cmc;

int communique_register(const char __user *args)
{
	int rt = 0;
	int data[2];
	struct proc_rel *temp = NULL;
	rt = communique_cp_prs(data, args, 2);
	if (rt)
		return rt;
	temp = kmalloc(sizeof(struct proc_rel), GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	temp->pid = data[0];
	temp->event_to_throw = data[1];
	communique_rb_insert(&cmc.proc_rel_tree, temp);
	return 0;
}

void debug_pr(struct proc_rel *temp)
{
	printk(KERN_EMERG "pid: %d, event: %d\n", temp->pid, 
	       temp->event_to_throw);
}

int communique_sleep(const char __user *args)
{
	struct proc_rel *temp = NULL;
	int rt;
	int event;
	rt = communique_cp_prs(&event, args, 1);
	if (rt)
		return rt;
	temp = communique_rb_search(&cmc.proc_rel_tree, event);
	if (temp == NULL)
		return -EINVAL;
	else
		debug_pr(temp);
	return 0;
}

int communique_throw(const char __user *args)
{
	struct proc_rel *temp = NULL;
	int rt;
	int data[2];
	rt = communique_cp_prs(data, args, 2);
	if (rt)
		return rt;
	temp = communique_rb_search(&cmc.proc_rel_tree, data[1]);
	if ((data[0] != temp->pid) || (temp == NULL))
		return -EACCES;
	else
		debug_pr(temp);
	return 0;
}

long communique_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
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
	.device = NULL,
	.proc_rel_tree = RB_ROOT 
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
