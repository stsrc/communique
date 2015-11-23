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
	struct completion *unset;
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
 * -> anti-deadlock algorithm
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

static inline int events_check_unset(struct event *event)
{
	int rt;
	if (event->s_comp || event->g_comp)	
		return -EAGAIN;
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
	rt = mutex_lock_interruptible(&event->lock);
	if (rt) {
		rt = -EINTR;
		goto ret;	//should i go here to mutex_unlock?
	}
	if (event->unset != NULL) {
		rt = 0;
		goto ret;
	} 
	rt = events_check_unset(event);
	if (rt == -EAGAIN) {
		while(rt == -EAGAIN) {
			event->unset = events_new_completion();
			if (event->unset == NULL) {
				rt = -ENOMEM;
				goto ret;
			}
			mutex_unlock(&event->lock);
			up_write(&cmc->rw_lock);
			wait_for_completion(event->unset);
			down_write(&cmc->rw_lock);
			rt = mutex_lock_interruptible(&event->lock);
			rt = events_check_unset(event);
			while(spin_is_locked(&event->unset->wait.lock));
			kfree(event->unset);
			event->unset = NULL;
		}
	}
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
	event->unset = NULL;
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
	if (event == NULL) {
		debug_message();
		printk("DEREFERENCING event == NULL!!!\n");
		return -EINVAL;
	}
	debug_message();
	for (int i = 0; i < glob_proc; i++) {
		debug_message();
		mutex_lock(&event->lock);
		debug_message();
		pid_throwing = event->proc_throws[i];
		debug_message();
		mutex_unlock(&event->lock);
		debug_message();
		if (pid_throwing == 0)
			continue;
		if (pid_throwing == current_pid)
			continue;
		debug_message();
		mutex_lock(&event->lock);
		debug_message();
		rt = events_search_pid(event->proc_waits, glob_proc, 
				       pid_throwing);
		debug_message();
		mutex_unlock(&event->lock);
		debug_message();
		if (rt != -1)
			continue;
		debug_message();
		temp = events_check_if_proc_waits(cmc, pid_throwing);
		debug_message();
		if (temp == NULL)
			return 1;
		debug_message();
		rt = events_check_not_deadlock(cmc, current_pid, temp);
		debug_message();
		if (rt)
			return 1;
		else
			continue;
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
	rt = mutex_lock_interruptible(&event->lock);
	up_read(&cmc->rw_lock);
	if (rt < 0)
		return -EINTR;
	if (!event->s_comp)
		init_completion((struct completion *)event->wait[0]);
	event->s_comp++;
	rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
	if (rt)
		return -EINVAL;
	mutex_unlock(&event->lock);
	down_write(&cmc->rw_lock);
	rt = events_check_not_deadlock(cmc, current->pid, event);
	up_write(&cmc->rw_lock);
	if (!rt)
		return -EDEADLK;
	rt = wait_for_completion_interruptible(event->wait[0]);
	mutex_lock(&event->lock);
	event->s_comp--;
	if (event->unset)
		complete(event->unset);
	events_remove_pid(event->proc_waits, glob_proc, current->pid);
	if (rt != -EINTR)
		mutex_unlock(&event->lock);
	if (unlikely(rt))
		return -EINTR;
	return 0;
}

struct wait_group {
	int nbytes;
	char *events;
};

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
		if (event == NULL) {
			debug_message();
			printk("DEREFERENCING event == NULL!!!\n");
			continue;
		}
		mutex_lock_interruptible(&event->lock);
		events_remove_pid(event->proc_waits, glob_proc, current->pid);	
		rt = events_remove_compl(event->wait, glob_compl_cnt_max,
					 completion);	
		if (rt != -1) {
			event->g_comp--;
			if (event->unset)
				complete(event->unset);
		}
		mutex_unlock(&event->lock);
	}
	kfree(temp);
	return 0;
}
//MUTUAL EXCLUSIONS HERE. ARE THEY OK?
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
		if (event == NULL) {
			debug_message();
			printk("DEREFERENCING event == NULL!!!\n");
			continue;
		}
		mutex_lock(&event->lock);
		rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
		if (rt == -1) {
			/*
			 * event->pid_waits is full or bad events (input
			 * string contains the same event more than one)
			 */
			mutex_unlock(&event->lock);
			events_remove_completion(cmc, buf, completion);
			kfree(temp);
			return -ENOMEM;
		}	
		rt = events_add_compl(event->wait, glob_compl_cnt_max, completion);
		if (rt) {
			mutex_unlock(&event->lock);
			events_remove_completion(cmc, buf, completion);
			kfree(temp);
			return -ENOMEM;

		}
		event->g_comp++;	
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
	pid_t current_pid = current->pid;
	char *temp = kmalloc(strlen(buf)+1, GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	strcpy(temp, buf);
	while ((name = strsep(&temp, "&")) != NULL) {
		debug_message();
		event = events_search(cmc, name);
		if (unlikely(event == NULL)) {
			debug_message();
			kfree(temp);
			return -EINVAL;
		}
		debug_message();
		rt = events_check_not_deadlock(cmc, current_pid, event);
		if (rt)
			break;
	}
	kfree(temp);
	rt = 1;
	return rt;
}	

int events_group_wait(struct events *cmc, const char __user *user_buf)
{
	int rt;
	struct completion *completion;
	const char *buf = events_group_get_events(user_buf);
	if (buf == NULL)
		return -ENOMEM;
	down_write(&cmc->rw_lock);//testing purpose, i don't want anything to change
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
	printk(KERN_EMERG "buf = %s\n", buf);
	rt = events_group_check_not_deadlock(cmc, buf);
	if (rt <= 0) {
		rt = -EDEADLK;
		events_remove_completion(cmc, buf, completion);
		while(spin_is_locked(&completion->wait.lock));
		kfree(completion);
		goto ret;
	}
	up_write(&cmc->rw_lock);
	rt = wait_for_completion_interruptible(completion);
	if (rt)
		rt = -EINTR;
	down_write(&cmc->rw_lock);
	events_remove_completion(cmc, buf, completion);
	up_write(&cmc->rw_lock);
	while (spin_is_locked(&completion->wait.lock));
	kfree(completion);
	kfree(buf);
	return rt;
ret:
	up_write(&cmc->rw_lock);
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
	rt = mutex_lock_interruptible(&event->lock);
	up_read(&cmc->rw_lock);
	if (rt < 0)
		return -EINTR;
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1) {
		mutex_unlock(&event->lock);
		return -EPERM;
	}
	if (!event->s_comp && !event->g_comp) {
		mutex_unlock(&event->lock);
		return 0;
	}
	if (event->s_comp)
		complete_all(event->wait[0]);
	for(int i = 1; i <= event->g_comp; i++)
		complete_all(event->wait[i]);
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
	events_delete_event(event);
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
