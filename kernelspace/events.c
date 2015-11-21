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
	uint8_t refcount;
	struct spinlock refcount_lock;
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

static inline void generate_oops(void)
{
	*(int *)NULL = 0;
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
 * -> anti-deadlock algorithm in un-set too!
 * -> group call - WHAT IF THERE IS NO ONE OF EVENT?!?!
 *  -> event_throw - meaby change position of closing rw_lock and mutex_lock?
 * -> check semaphores (is it really mutual exclusions
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

static inline void events_increment_refcount(struct event *event)
{
	spin_lock(&event->refcount_lock);
	//event->refcount++;
	spin_unlock(&event->refcount_lock);
}

static inline void events_decrement_refcount(struct event *event)
{
	spin_lock(&event->refcount_lock);
//	if (!event->refcount) {
//		generate_oops();
//		spin_unlock(&event->refcount_lock);
//		return;
//	}
//	event->refcount--;
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
	if (event->s_comp || event->g_comp) {
		debug_message();
		printk(KERN_EMERG "event name: %s\n", event->name);
		printk(KERN_EMERG "event->g_comp = %d\n", event->g_comp);
		printk(KERN_EMERG "event->s_comp = %d\n", event->s_comp);

		return -EAGAIN;
	}
//	refcount = events_check_refcount(event);
//	if (refcount != 1)
//		return -EAGAIN;
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1)
		return -EACCES;
	return rt;
}

void events_delete_event(struct event *event)
{
	list_del(&event->element);
	kfree(event->name);
	event->name = NULL;
	while (spin_is_locked(&event->wait[0]->wait.lock));
	kfree(event->wait[0]);
	mutex_unlock(&event->lock);
	kfree(event);
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
	if (rt < 0)
		goto ret;
	events_remove_pid(event->proc_throws, glob_proc, current->pid);
	rt = events_non_zero_pid(event->proc_throws, glob_proc);
	if (rt) {
		rt = 0;
		goto ret;
	}
	events_delete_event(event);
	cmc->event_cnt--;
	up_write(&cmc->rw_lock);
	return 0;
ret:
	events_decrement_refcount(event);
	if (rt != -EINTR) //is it okay? check in a while
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
		rt = mutex_lock_interruptible(&event->lock);
		if (rt) {
			rt = -EINTR;
			up_write(&cmc->rw_lock);
			kfree(name);
			return rt;
		}
		rt = events_add_pid(event->proc_throws, glob_proc, current->pid);
		mutex_unlock(&event->lock);
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

int events_check_if_proc_waits(struct events *cmc, pid_t pid, 
			       struct event **event)
{
	int rt = -1;
	struct event *temp = NULL;
	list_for_each_entry(temp, &cmc->event_list, element) {
		if ((*event != NULL) && (*event != temp))
			continue;
		mutex_lock(&temp->lock);
		if (events_search_pid(temp->proc_waits, glob_proc, pid) != -1) {
			mutex_unlock(&temp->lock);
			if (rt == -1) {
				*event = temp;
				rt = 0;
			} else {
				rt = 1;
			}
		}
		mutex_unlock(&temp->lock);
	}
	return rt;
}

int events_check_not_deadlock(struct events *cmc, pid_t current_pid, 
			      struct event *event)
{	
	struct event *temp = NULL;
	int rt, more_events = 1;
	for (int i = 0; i < glob_proc; i++) {
		if (event->proc_throws[i] == 0)
			continue;
		if (event->proc_throws[i] == current_pid)
			continue;
		while (more_events > 0) {
			more_events = events_check_if_proc_waits
				      (cmc, event->proc_throws[i], &temp);
			if (temp == NULL)
				return 1;
			rt = events_check_not_deadlock(cmc, current_pid, temp);
			if (rt)
				return 1;
		}
	}	
	return 0;
}

int events_wait(struct events *cmc, const char __user *buf)
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
	rt = mutex_lock_interruptible(&event->lock);
	up_read(&cmc->rw_lock);
	if (rt < 0) {
		events_decrement_refcount(event);
		return -EINTR;
	}

	if (!event->s_comp)
		init_completion((struct completion *)event->wait[0]);
	event->s_comp++;
	rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
	if (rt) {
		events_decrement_refcount(event);
		return -EINVAL;
	}
	mutex_unlock(&event->lock);
	down_write(&cmc->rw_lock);
	rt = events_check_not_deadlock(cmc, current->pid, event);
	up_write(&cmc->rw_lock);
	if (!rt) {
		events_remove_pid(event->proc_waits, glob_proc, current->pid);
		event->s_comp--;
		events_decrement_refcount(event);
		return -EDEADLK;
	}
	rt = wait_for_completion_interruptible(event->wait[0]);
	mutex_lock(&event->lock);
	event->s_comp--;
	events_remove_pid(event->proc_waits, glob_proc, current->pid);
	if (rt != -EINTR)
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
	char *temp = kmalloc(strlen(buf)+1, GFP_KERNEL);
	if (temp == NULL)
		return -1;
	strcpy(temp, buf);
	const char *name = NULL;
	struct event *event;
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		if (event == NULL) {
			kfree(temp);
			return 1;
		}
	}
	kfree(temp);
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

int events_remove_completion(struct events *cmc, const char *buf, 
			     struct completion *completion)
{
	int rt;
	const char *name = NULL;
	struct event *event;
	char *temp = kmalloc(strlen(buf)+1, GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	strcpy(temp, buf);
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		mutex_lock_interruptible(&event->lock);
		event->g_comp--;
		rt = events_remove_pid(event->proc_waits, glob_proc, current->pid);	
		rt = events_remove_compl(event->wait, glob_compl_cnt_max,
					 completion);	
		mutex_unlock(&event->lock);
	}
	kfree(temp);
	return 0;
}

int events_insert_completion(struct events *cmc, const char *buf, 
			     struct completion *completion)
{
	int rt;
	const char *name = NULL;
	struct event *event;
	char *temp = kmalloc(strlen(buf)+1, GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	strcpy(temp, buf);
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		mutex_lock_interruptible(&event->lock);
		rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
		if (rt) {
			kfree(temp);
			generate_oops();
			mutex_unlock(&event->lock);
			events_remove_completion(cmc, buf, completion);
			return -1;
		}
		if (event->g_comp + 1 > glob_compl_cnt_max) {
			kfree(temp);
			mutex_unlock(&event->lock);
			events_remove_completion(cmc, buf, completion);
			return -1;
		}
		event->g_comp++;
		rt = events_add_compl(event->wait, glob_compl_cnt_max, completion);
		if (rt) {
			events_remove_pid(event->proc_waits, glob_proc, 
					  current->pid);
			event->g_comp--;
			kfree(temp);
			mutex_unlock(&event->lock);
			return -1;
		}
		mutex_unlock(&event->lock);
	}
	kfree(temp);
	return 0;
}

int events_group_check_not_deadlock(struct events *cmc, const char *buf)
{
	int rt = 0;
	const char *name = NULL;
	struct event *event;
	pid_t current_pid;
	char *temp = kmalloc(strlen(buf)+1, GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	strcpy(temp, buf);
	while ((name = strsep(&temp, "&")) != NULL) {
		event = events_search(cmc, name);
		if (unlikely(event == NULL)) {
			generate_oops();
			return -EINVAL;
		}
		rt = events_check_not_deadlock(cmc, current_pid, event);
		if (rt)
			break;
	}
	kfree(temp);
	return rt;
}	

int events_group_wait(struct events *cmc, const char __user *user_buf)
{
	int rt;
	struct completion *completion;
	const char *buf = events_group_get_events(user_buf);
	if (buf == NULL)
		return -ENOMEM;
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
	if (rt < -1) {
		rt = -ENOMEM;
		goto ret;
	}
	rt = events_group_check_not_deadlock(cmc, buf);
	if (!rt) {
		rt = -EDEADLK;
		events_remove_completion(cmc, buf, completion);
		while(spin_is_locked(&completion->wait.lock));
		kfree(completion);
		goto ret;
	}
	up_read(&cmc->rw_lock);
	rt = wait_for_completion_interruptible(completion);
	if (rt)
		rt = -EINTR;
	down_read(&cmc->rw_lock);
	events_remove_completion(cmc, buf, completion);
	up_read(&cmc->rw_lock);
	while (spin_is_locked(&completion->wait.lock));
	kfree(completion);
	kfree(buf);
	return rt;
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
	rt = mutex_lock_interruptible(&event->lock);
	up_read(&cmc->rw_lock);
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
	if (!event->s_comp && !event->g_comp) {
		mutex_unlock(&event->lock);
		events_decrement_refcount(event);
		return 0;
	}
	if (event->s_comp)
		complete_all(event->wait[0]);
	for(int i = 1; i <= event->g_comp; i++)
		complete_all(event->wait[i]);
	mutex_unlock(&event->lock);
	events_decrement_refcount(event);

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
