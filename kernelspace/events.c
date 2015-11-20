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
	int8_t g_comp;
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
 * -> remove EAGAIN from unset.
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

static inline int events_check_unset(struct event *event)
{
	int rt;
	uint8_t refcount;
	if (event->s_comp)
		return -EAGAIN;
	refcount = events_check_refcount(event);
	if (refcount != 1)
		return -EAGAIN;
	rt = events_remove_pid(event->proc_throws, glob_proc, current->pid);
	if (rt)
		return -EINVAL;
	rt = events_non_zero_pid(event->proc_throws, glob_proc);
	if (rt) 
		return 0;
	return 1;
}

int events_unset(struct events *cmc, const char __user *buf)
{
	int rt;
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
	rt = events_check_unset(event);
	if (rt != 1)
		goto ret;
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
	mutex_unlock(&event->lock);
	events_decrement_refcount(event);
	if (unlikely(rt))
		return -EINTR;
	return 0;
}

struct wait_group {
	int nbytes;
	char *events;
};

void print_wait_group(struct wait_group *wait)
{
	printk(KERN_EMERG "struct size: %d", (int)sizeof(struct wait_group));
	printk(KERN_EMERG "int nbytes addr: %lu; char *events addr: %lu\n",
	       (unsigned long)&wait->nbytes, (unsigned long)&wait->events);
	printk(KERN_EMERG "nbytes = %d, events = %lu\n", wait->nbytes,
	       (unsigned long)wait->events);
}

/*
 * returned pointer must be freed outside!
 */
const char *events_group_get_events(const char __user *user_buf)
{
	struct wait_group wait_group;
	char *buf;
	int rt;
	rt = copy_from_user(&wait_group, user_buf, sizeof(struct wait_group));
	if (rt)
		return NULL;
	print_wait_group(&wait_group);
	buf = kmalloc(wait_group.nbytes, GFP_KERNEL);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, wait_group.nbytes);
	rt = copy_from_user(buf, wait_group.events, wait_group.nbytes);
	if (rt) {
		kfree(buf);
		return NULL;
	}
	return (const char *)buf;
}

int events_check_events_existances(struct events *cmc, const char *buf)
{
	char *temp = (char *)buf;
	const char *name = NULL;
	struct event *event;
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		if (event == NULL)
			return 1;
	}
	return 0;
}

/*
 * returned pointer must be freed outside!
 */
struct completion *events_new_completion(void)
{
	struct completion *compl = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (compl == NULL)
		return NULL;
	init_completion(compl);
	return compl;
}

int events_insert_completion(struct events *cmc, const char *buf, 
			     struct completion *completion)
{
	int rt;
	char *temp = (char *)buf;
	const char *name = NULL;
	struct event *event;
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		mutex_lock(&event->lock);
		rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
		if (rt) {
			/*what if it fails in the middle?
			 * Nearly impossible though...
			 */
			debug_message();
			mutex_unlock(&event->lock);
			return -1;
		}
		if (event->g_comp + 1 > glob_compl_cnt_max) {
			debug_message();
			mutex_unlock(&event->lock);
			return -1;
		}
		event->g_comp++;
		event->wait[event->g_comp] = completion;
		mutex_unlock(&event->lock);
	}
	return 0;
}

int events_group_wait(struct events *cmc, const char __user *user_buf)
{
	int rt;
	struct completion *completion;
	const char *buf = events_group_get_events(user_buf);
	if (buf == NULL)
		return -EAGAIN;
	down_read(&cmc->rw_lock);
	printk(KERN_EMERG "buffer contains: %s\n", buf);
	rt = events_check_events_existances(cmc, buf);
	if (rt) {
		rt = -EINVAL;
		goto ret;
	}
	completion = events_new_completion();
	if (completion == NULL) {
		rt = -ENOMEM;
		goto ret;
	}
	rt = events_insert_completion(cmc, buf, completion);
	rt = wait_for_completion_interruptible(completion);
ret:
	up_read(&cmc->rw_lock);
	kfree(buf);
	return rt;
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
	if (rt < 0){
		events_decrement_refcount(event);
		return -EINTR;
	}
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1) {
		mutex_unlock(&event->lock);
		events_decrement_refcount(event);
		return -EPERM;
	}
	if (!event->s_comp) {
		mutex_unlock(&event->lock);
		events_decrement_refcount(event);
		return 0;
	}
	for(int i = 0; i <= event->g_comp; i++)
		complete_all(event->wait[i]);
	event->g_comp = 0;
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
		rt = events_group_wait(&cmc, (char __user *)arg);
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
