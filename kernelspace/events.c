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
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <asm/io.h>
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
#define WEITINGROUP _IOW(0x8A, 0x05, char __user *)

uint8_t glob_name_size = 2;
uint8_t glob_event_cnt_max = 5;
uint8_t glob_compl_cnt_max = 5;
uint8_t glob_proc = 5;

struct event {
	char *name;
	struct completion *wait[6];
	int8_t s_comp;
	struct list_head element;
	struct mutex lock;
	uint8_t refcount;
	struct spinlock refcount_lock;
	pid_t proc_throws[6];
	pid_t proc_waits[6];
};

struct events {
	const char *driver_name;
	struct cdev *cdev;
	dev_t dev;
	struct class *class;
	struct file_operations fops;
	struct device *device;
	struct list_head event_list;
	struct rw_semaphore rw_lock;
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

/*
 * TODO:
 * -> mutexes/sempahores/reference counts etc.
 * -> group call
 * -> anti-deadlock algorithm
 * -> more flexible completion *wait[tab]
 * -> ctrl+C/INTERRUPTS proof!
 */

/*
 * WARNING - function dynamically allocates memory!
 * Remember to call free when obtained name string will not be used anymore!
 */
char *events_get_name(const char __user *buf)
{
	int rt;
	char *name = kmalloc(sizeof(char) * glob_name_size, GFP_KERNEL);
	if (unlikely(name == NULL))
		return NULL;
	memset(name, 0, sizeof(char)*glob_name_size);
	rt = copy_from_user(name, buf, glob_name_size - 1);
	if (unlikely(rt)) {
		kfree(name);
		return NULL;
	}
	return name;	
}

ssize_t events_search_pid(pid_t *arr, size_t size, pid_t pid)
{
	for (size_t i = 0; i < size; i++) {
		if (pid == arr[i])
			return i;
	}
	return -1;
}

ssize_t events_add_pid(pid_t *arr, ssize_t size, pid_t pid)
{
	ssize_t i;
	i = events_search_pid(arr, size, pid);
	if (i != -1)
		return -1;
	i = events_search_pid(arr, size, 0);
	if (i == -1)
		return -1;
	arr[i] = pid;
	return 0;
}

ssize_t events_remove_pid(pid_t *arr, ssize_t size, pid_t pid)
{
	ssize_t i;
	i = events_search_pid(arr, size, pid);
	if (i == -1)
		return -1;
	arr[i] = 0;
	return 0;
}

int events_non_zero_pid(pid_t *arr, ssize_t size)
{
	int cnt = 0;
	for (ssize_t i = 0; i < size; i++) {
		if (arr[i] != 0)
			cnt++;
	}
	return cnt;
}

struct event *events_search(struct events *cmc, const char *name)
{
	struct event *event = NULL;
	list_for_each_entry(event, &cmc->event_list, element) {
		if (strncmp(event->name, name, strlen(name)) == 0) {
			return event;
		}
	}
	return NULL;
}

struct event *events_get_event(struct events*cmc, const char __user *buf)
{
	struct event *event;
	char *name = events_get_name(buf);
        if (unlikely(name == NULL))
		return NULL;
	event = events_search(cmc, name);
	kfree(name);
	return event;
}

static inline void events_increment_refcount(struct event *event)
{
	spin_lock(&event->refcount_lock);
	event->refcount++;
	spin_unlock(&event->refcount_lock);
}

static inline void events_decrement_refcount(struct event *event)
{
	spin_lock(&event->refcount_lock);
	if (!event->refcount) {
		printk(KERN_EMERG "!!!events_decrement_refcount FAILED!!!!\n");
		spin_unlock(&event->refcount_lock);
		return;
	}
	event->refcount--;
	spin_unlock(&event->refcount_lock);
}

static inline uint8_t events_check_refcount(struct event *event)
{
	uint8_t rt;
	spin_lock(&event->refcount_lock);
	rt = event->refcount;
	spin_unlock(&event->refcount_lock);
	return rt;
}

int events_unset(struct events *cmc, const char __user *buf)
{
	int rt;
	uint8_t refcount;
	struct event *event;
	down_write(&cmc->rw_lock);
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		up_write(&cmc->rw_lock);
		return -EINVAL;
	}
	events_increment_refcount(event);
	rt = mutex_lock_interruptible(&event->lock);
	if (rt) {
		rt = -EINTR;
		goto ret;	//should i go here to mutex_unlock?
	}
	if (event->s_comp) {
		rt = -EAGAIN;
		goto ret;
	}
	refcount = events_check_refcount(event);
	if (refcount != 1) {
		rt = -EAGAIN;
		goto ret;
	}
	rt = events_remove_pid(event->proc_throws, glob_proc, current->pid);
	if (rt) {
		rt = -EINVAL;
		goto ret;
	}
	rt = events_non_zero_pid(event->proc_throws, glob_proc);
	if (rt) {
		rt = 0;
		goto ret;	
	}
	list_del(&event->element);
	kfree(event->name);
	event->name = NULL;
	complete_all(event->wait[0]);
	kfree(event->wait[0]);
	mutex_unlock(&event->lock);
	kfree(event);
	cmc->event_cnt--;
	up_write(&cmc->rw_lock);
	return 0;
ret:
	events_decrement_refcount(event);
	mutex_unlock(&event->lock);
	up_write(&cmc->rw_lock);
	return rt;
}

static inline struct event* events_init_event(char *name)
{
	struct event *event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (unlikely(event == NULL))
		return NULL;
	memset(event, 0, sizeof(struct event));
	event->name = name;
	event->wait[0] = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (unlikely(event->wait[0] == NULL)) {
		kfree(event);
		return NULL;
	}
	events_add_pid(event->proc_throws, glob_proc, current->pid);
	init_completion(event->wait[0]);
	INIT_LIST_HEAD(&event->element);
	mutex_init(&event->lock);
	spin_lock_init(&event->refcount_lock);
	return event;
}

int events_set(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	char *name = events_get_name(buf);
	if (unlikely(name == NULL))
		return -EINVAL;
	down_write(&cmc->rw_lock);
	event = events_search(cmc, name);
	if (event != NULL) {
		rt = events_add_pid(event->proc_throws, glob_proc, current->pid);
		if (rt)
			rt = -EINVAL;
		up_write(&cmc->rw_lock);
		kfree(name);
		return rt;
	}
	cmc->event_cnt++;
	if (cmc->event_cnt >= glob_event_cnt_max)
		goto no_mem;
	event = events_init_event(name);
	if (event == NULL)
		goto no_mem;
	list_add(&event->element, &cmc->event_list);
	up_write(&cmc->rw_lock);
	return 0;
no_mem:
	cmc->event_cnt--;
	kfree(name);
	up_write(&cmc->rw_lock);
	return -ENOMEM;
}

int events_wait(struct events *cmc, const char __user *buf)
{
	int rt;
	int test;
	struct event *event;
	down_read(&cmc->rw_lock);
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		up_read(&cmc->rw_lock);
		return -EINVAL;
	}
	events_increment_refcount(event);
	up_read(&cmc->rw_lock);
	rt = mutex_lock_interruptible(&event->lock);
	if (rt < 0) {
		events_decrement_refcount(event);
		return -EINTR;
	}
	/*
	 * event->wait[0] ALWAYS belongs to "for single event" wait
	 */
	if (!event->s_comp)
		init_completion((struct completion *)event->wait[0]);
	event->s_comp++;
	rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
	mutex_unlock(&event->lock);
	if (rt) {
		events_decrement_refcount(event);
		return -EINVAL;
	}
	/*
	 * LOCK HERE?
	 */
	rt = wait_for_completion_interruptible(event->wait[0]);
	mutex_lock(&event->lock);
	event->s_comp--;
	test = events_remove_pid(event->proc_waits, glob_proc, current->pid);
	if (test)
		debug_message();
	events_decrement_refcount(event);
	mutex_unlock(&event->lock);
	if (unlikely(rt))
		return -EINTR;
	return 0;
}

int events_wait_in_group(struct events *cmc, const char __user *buf)
{
	return 0;
}

int events_throw(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	down_read(&cmc->rw_lock);
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		up_read(&cmc->rw_lock);
		return -EINVAL;
	}
	events_increment_refcount(event);
	up_read(&cmc->rw_lock);
	rt = mutex_lock_interruptible(&event->lock);
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1) {
		mutex_unlock(&event->lock);
		events_decrement_refcount(event);
		return -EPERM;
	}
	if (rt < 0){
		events_decrement_refcount(event);
		return -EINTR;
	}
	if (!event->s_comp) {
		mutex_unlock(&event->lock);
		events_decrement_refcount(event);
		return 0;
	}
	complete_all(event->wait[0]);
	events_decrement_refcount(event);
	mutex_unlock(&event->lock);
	return 0;
}

long events_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rt;
	switch(cmd) {
	case SETEVENT:
		rt = events_set(&cmc, (char __user *)arg);
		return rt;
	case WAITFOREVENT:
		rt = events_wait(&cmc, (char __user *)arg);
		return rt;
	case WEITINGROUP:
		rt = events_wait_in_group(&cmc, (char __user *)arg);
		return rt;
	case THROWEVENT:
		rt = events_throw(&cmc, (char __user *)arg);
		return rt;
	case UNSETEVENT:
		rt = events_unset(&cmc, (char __user *)arg);

		return rt;
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
		 .unlocked_ioctl = events_ioctl
		},
	.device = NULL,
	.event_list = LIST_HEAD_INIT(cmc.event_list),
	.rw_lock = __RWSEM_INITIALIZER(cmc.rw_lock)
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
