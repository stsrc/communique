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
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
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
	struct rw_semaphore rb_mutex;
};

static struct communique cmc;

struct proc_rel {
	pid_t owner_pid;
	pid_t user_pid;
	uint8_t event_to_throw;
	struct rb_node node;
	struct completion waiting;
	spinlock_t lock;
};

/*
 * functions implemented specially for rbtree
 */
struct proc_rel *communique_rb_search(struct rb_root *root, int event)
{
	struct rb_node *node;
	down_read(&(cmc.rb_mutex));
	/*down_read may put the calling process into an uninterruptible sleep!*/
       	node = root->rb_node;
	while (node) {
		struct proc_rel *data = rb_entry(node, struct proc_rel, node);
		if (event < data->event_to_throw) {
			node = node->rb_left;
		} else if (event > data->event_to_throw) {
			node = node->rb_right;
		} else {
			up_read(&(cmc.rb_mutex));
			return data;
		}
	}
	up_read(&(cmc.rb_mutex));
	return NULL;
}

int communique_rb_insert(struct rb_root *root, struct proc_rel *data)
{
	struct rb_node **new = NULL, *parent = NULL;
	down_read(&(cmc.rb_mutex));
	new = &(root->rb_node);
	while (*new) {
		struct proc_rel *this = rb_entry(*new, struct proc_rel, node);
		parent = *new;
		if (data->event_to_throw < this->event_to_throw) {
			new = &((*new)->rb_left);
		} else if (data->event_to_throw > this->event_to_throw) {
			new = &((*new)->rb_right);
		} else {
			up_read(&(cmc.rb_mutex));
			return -EINVAL;
		}
	}
	up_read(&(cmc.rb_mutex));
	down_write(&(cmc.rb_mutex));
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);
	up_write(&(cmc.rb_mutex));
	return 0;
}

int communique_pr_remove(struct rb_root *root, int event)
{
	struct proc_rel *ret = communique_rb_search(root, event);
	if (ret == NULL)
		return -EINVAL;
	down_write(&(cmc.rb_mutex));
	rb_erase(&(ret->node), root);
	up_write(&(cmc.rb_mutex));
	kfree(ret);
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

static inline int communique_copy_parse(int *to, const char __user *from, 
				        uint8_t n)
{
	int rt;
	rt = copy_from_user((char *)to, from, n * sizeof(int));
	if (rt)
		return -EAGAIN;
	return 0;
}

int communique_set(const char __user *args)
{
	int rt = 0;
	int event;
	struct proc_rel *temp = NULL;
	struct task_struct *task = NULL;
	rt = communique_copy_parse(&event, args, 1);
	if (rt)
		return rt;
	temp = kmalloc(sizeof(struct proc_rel), GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	/*
	 * TODO: remember, that pid may be re-used by another process
	 * if this process will be terminated. It's temporary assign!
	 */
	task = current;
	temp->owner_pid = task->pid;
	temp->user_pid = 0;
	temp->event_to_throw = event;
	spin_lock_init(&(temp->lock));
	rt = communique_rb_insert(&cmc.proc_rel_tree, temp);
	if (rt) {
		kfree(temp);
		return -EINVAL; 
	}
	init_completion(&(temp->waiting));
	return 0;
}

void debug_pr(struct proc_rel *temp)
{
	printk(KERN_EMERG "pid: %d, current pid: %d, event: %d\n", 
	       (int)temp->owner_pid, (int)current->pid, temp->event_to_throw);
}

int communique_check_wait(struct proc_rel *temp)
{
	spin_lock_irq(&(temp->lock));
	if (temp->user_pid)
		return -EAGAIN;
	return 0;
}

void communique_set_wait(struct proc_rel *temp)
{
	//spinlock!
	temp->user_pid = current->pid;
}

int communique_wait(const char __user *args)
{
	struct proc_rel *temp = NULL;
	int rt;
	int event;
	rt = communique_copy_parse(&event, args, 1);
	if (rt)
		return rt;
	temp = communique_rb_search(&cmc.proc_rel_tree, event);
	if (temp == NULL)
		return -EINVAL;
	spin_lock_irq(&(temp->lock));
	rt = communique_check_wait(temp);
	if (rt)
		return rt;
	communique_set_wait(temp);
	spin_unlock_irq(&(temp->lock));
	rt = wait_for_completion_interruptible(&(temp->waiting));
	if (!rt)
		return -EINTR;
	return 0;
}

int communique_check_throw(struct proc_rel *temp)
{	
	//it only disables hw irqs... what about sw??
	
	if (current->pid != temp->owner_pid) {
		debug_pr(temp);
		return -EACCES;
	}
	return 0;
}

int communique_throw(const char __user *args)
{
	struct proc_rel *temp = NULL;
	int rt;
	int event;
	rt = communique_copy_parse(&event, args, 1);
	if (rt)
		return -EAGAIN;
	temp = communique_rb_search(&cmc.proc_rel_tree, event);
	if (temp == NULL)
		return -EINVAL;
	rt = communique_check_throw(temp);
	if (rt)
		return rt;
	complete_all(&(temp->waiting));
	reinit_completion(&(temp->waiting));
	temp->user_pid = 0;
	return 0;
}

long communique_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case SETEVENT:
		return communique_set((char __user *)arg);
	case WAITFOREVENT:
		return communique_wait((char __user *)arg);
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
	init_rwsem(&(cmc.rb_mutex));
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
