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
	pid_t proc_throws[6]; //O really 6? not 5?
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
struct completion *events_new_completion(void);

static inline void generate_oops(void)
{
	*(int *)NULL = 0;
}

void events_diagnose_event(struct event *event)
{
	if (event == NULL) {
		printk("events_diagnose_event: event NULL!\n");
		return;
	}
	printk(KERN_EMERG "name: %s\n", event->name);
	printk(KERN_EMERG "int8_t s_comp: %d\n", event->s_comp);
	printk(KERN_EMERG "int8_t g_comp: %d\n", event->g_comp);
	for (int i = 0; i < glob_compl_cnt_max; i++) {
		printk(KERN_EMERG "struct completion *wait[%d]: %lu\n",
		       i, (unsigned long)event->wait[i]);
	}
	for(int i = 0; i < glob_proc; i++) {
		printk(KERN_EMERG "pid_t proc_throws[%d]: %lu\n",
		       i, (unsigned long)event->proc_throws[i]);
	}
	for(int i = 0; i < glob_proc; i++) {
		printk(KERN_EMERG "pid_t proc_waits[%d]: %lu\n",
		       i, (unsigned long)event->proc_waits[i]);
	}
}

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
 * -semaphores
 * -group_wait
 * -anti-deadlock for group_wait
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

ssize_t events_search_compl(struct completion **arr, size_t size,
			    struct completion *compl)
{
	for (size_t i = 0; i < size; i++) {
		if (compl == arr[i])
			return i;
	}
	return -1;
}

ssize_t events_add_compl(struct completion **arr, ssize_t size,
			 struct completion *compl)
{
	ssize_t i;
	i = events_search_compl(arr, size, compl);
	if (i != -1)
		return -1;
	i = events_search_compl(arr, size, 0);
	if (i == -1)
		return -1;
	arr[i] = compl;
	return 0;
}

ssize_t events_remove_compl(struct completion **arr, ssize_t size,
			    struct completion *compl)
{
	ssize_t i;
	i = events_search_compl(arr, size, compl);
	if (i == -1)
		return -1;
	arr[i] = 0;
	return 0;
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

static inline int events_check_unset(struct event *event)
{
	int rt;
	if (event->s_comp || event->g_comp)	
		return -EAGAIN;
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1)
		return -EACCES;
	rt = 0;
	return rt;
}

void events_delete_event(struct events *cmc, struct event *event)
{
	list_del(&event->element);
	cmc->event_cnt--;
	kfree(event->name);
	event->name = NULL;
	while (spin_is_locked(&event->wait[0]->wait.lock));
	kfree(event->wait[0]);
	//rest?
	mutex_unlock(&event->lock);
	kfree(event);
}

int events_unset(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		return -EINVAL;
	}
	rt = events_check_unset(event);
	if (rt < 0)
		goto ret;
	events_remove_pid(event->proc_throws, glob_proc, current->pid);
	rt = events_non_zero_pid(event->proc_throws, glob_proc);
	if (rt) {
		rt = 0;
		goto ret;
	}
	events_delete_event(cmc, event);
	return 0;
ret:
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
	return event;
}

int events_set(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	char *name = events_get_name(buf);
	if (unlikely(name == NULL))
		return -EINVAL;
	event = events_search(cmc, name);
	if (event != NULL) {
		rt = events_add_pid(event->proc_throws, glob_proc, current->pid);
		if (rt)
			rt = -EINVAL;
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
	return 0;
no_mem:
	cmc->event_cnt--;
	kfree(name);
	return -ENOMEM;
}

struct event *events_check_if_proc_waits(struct events *cmc, pid_t pid)
{
	struct event *event = NULL;
	list_for_each_entry(event, &cmc->event_list, element) {
		if (event == NULL) {
			debug_message();
			printk("DEREFERENCING event == NULL!!!\n");
			return NULL;
		}
		mutex_lock(&event->lock);
		if (events_search_pid(event->proc_waits, glob_proc, pid) != -1) {
			mutex_unlock(&event->lock);
			return event;
		}
		mutex_unlock(&event->lock);
	}
	return NULL;	
}

int events_check_not_deadlock(struct events *cmc, pid_t current_pid, 
			      struct event *event)
{	
	struct event *temp;
	int rt;
	pid_t pid_throwing;
	if (event == NULL) 
		return -EINVAL;
	for (int i = 0; i < glob_proc; i++) {
		pid_throwing = event->proc_throws[i];
		if (pid_throwing == 0)
			continue;
		if (pid_throwing == current_pid)
			continue;
		rt = events_search_pid(event->proc_waits, glob_proc, 
				       pid_throwing);
		if (rt != -1) 
			continue;
		temp = events_check_if_proc_waits(cmc, pid_throwing);
		if (temp == NULL)
			return 1;
		rt = events_check_not_deadlock(cmc, current_pid, temp);
		if (rt)
			return rt;
	}
	return 0;
}

int events_wait(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		up_read(&cmc->rw_lock);
		return -EINVAL;
	}
	if (!event->s_comp)
		init_completion((struct completion *)event->wait[0]);
	event->s_comp++;
	rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
	if (rt)
		return -EINVAL;
	rt = events_check_not_deadlock(cmc, current->pid, event);
	if (!rt)
		return -EDEADLK;
	rt = wait_for_completion_interruptible(event->wait[0]);
	event->s_comp--;
	events_remove_pid(event->proc_waits, glob_proc, current->pid);
	return 0;
}
	
int events_group_wait(struct events *cmc, const char __user *user_buf)
{
	return 0;
}

int events_throw(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) 
		return -EINVAL;
	if (rt < 0)
		return -EINTR;
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1) {
		return -EPERM;
	}
	if (!event->s_comp && !event->g_comp) 
		return 0;
	if (event->s_comp)
		complete_all(event->wait[0]);
	for(int i = 1; i <= event->g_comp; i++)
		complete_all(event->wait[i]);
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
	debug_message();
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

void events_remove_at_exit(struct events *cmc, struct event *event)
{
	//i dont know
	cmc->event_cnt--;
}

static void __exit events_exit(void)
{
	struct event *event = NULL, *temp = NULL;
	debug_message();
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
	list_for_each_entry_safe(event, temp, &cmc.event_list, element) {
		events_diagnose_event(event);
		events_remove_at_exit(&cmc, event);
	}
}

module_init(events_init);
module_exit(events_exit);
