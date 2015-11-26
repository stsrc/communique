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

uint8_t glob_name_size = 4;
uint8_t glob_event_cnt_max = 5;
uint8_t glob_compl_cnt_max = 5;
uint8_t glob_proc = 5;

struct event {
	char *name;
	struct completion *wait[6];
	int8_t s_comp;
	int8_t g_comp;
	struct list_head element;
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
	struct mutex lock;
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
	arr[i] = NULL;
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
	kfree(event->wait[0]);
	event->wait[0] = NULL;
	kfree(event);
}

int events_unset(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		mutex_unlock(&cmc->lock);
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
	mutex_unlock(&cmc->lock);
	return 0;
ret:
	mutex_unlock(&cmc->lock);
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
	return event;
}

int events_set(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;

	char *name = events_get_name(buf);
	if (unlikely(name == NULL)) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	event = events_search(cmc, name);
	if (event != NULL) {
		rt = events_add_pid(event->proc_throws, glob_proc, current->pid);
		if (rt)
			rt = -EINVAL;
		mutex_unlock(&cmc->lock);
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
	mutex_unlock(&cmc->lock);
	return 0;
no_mem:
	cmc->event_cnt--;
	mutex_unlock(&cmc->lock);
	kfree(name);
	return -ENOMEM;
}

struct event *events_check_if_proc_waits(struct events *cmc, pid_t pid)
{
	struct event *event = NULL;
	list_for_each_entry(event, &cmc->event_list, element) {
		if (events_search_pid(event->proc_waits, glob_proc, pid) != -1) {
			return event;
		}
	}
	return NULL;	
}

int events_check_not_deadlock(struct events *cmc, pid_t current_pid, 
			      struct event *event)
{	
	struct event *temp;
	int rt;
	pid_t pid_throwing;
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
	int rt = 0;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;
	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	if (!event->s_comp)
		init_completion((struct completion *)event->wait[0]);
	event->s_comp++;
	rt = events_add_pid(event->proc_waits, glob_proc, current->pid);
	if (rt) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	rt = events_check_not_deadlock(cmc, current->pid, event);
	if (!rt) {
		mutex_unlock(&cmc->lock);
		return -EDEADLK;
	}
	mutex_unlock(&cmc->lock);
	rt = wait_for_completion_interruptible(event->wait[0]);
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		rt = -EINTR;
	event->s_comp--;
	events_remove_pid(event->proc_waits, glob_proc, current->pid);
	if (!rt)
		mutex_unlock(&cmc->lock);
	return rt;
}

struct wait_group {
	int nbytes;
	const char __user *buf;
};	

struct events_group {
	int cnt;
	struct completion *comp;
	char *names;
	char *name_tab[10];
};

int events_del_group_struct(struct events_group *gr)
{
	if (gr->names != NULL) {
		kfree(gr->names);
		gr->names = NULL;
	}
	if (gr->comp != NULL) {
		kfree(gr->comp);
		gr->comp = NULL;
	}
	return 0;
}

int events_parse_names(struct events *cmc, struct events_group *events_gr)
{
	char *name = NULL;
	struct event *event;
	while((name = strsep(&events_gr->names, "&")) != NULL) {
		event = events_search(cmc, name);
		if (event == NULL)
			return 1;
		if (events_gr->cnt > 9)
			continue;
		events_gr->name_tab[events_gr->cnt] = name;
		printk("event_name = %s\n", 
		       events_gr->name_tab[events_gr->cnt]);
		events_gr->cnt++;
	}
	return 0;
}

int events_group_get_events(struct events *cmc, struct events_group *events_gr, 
			    const char __user *user_buf)
{
	struct wait_group wait_group;
	int rt;
	rt = copy_from_user(&wait_group, user_buf, sizeof(struct wait_group));
	if (rt)
		return -EAGAIN;
	events_gr->names = kmalloc(wait_group.nbytes, GFP_KERNEL);
	if (events_gr->names == NULL)
		return -ENOMEM;
	memset(events_gr->names, 0, wait_group.nbytes);
	rt = copy_from_user(events_gr->names, wait_group.buf, wait_group.nbytes);
	if (rt)
		return -EAGAIN;
	rt = events_parse_names(cmc, events_gr);
	if (rt)
		return -EINVAL;
	return 0;
}

int events_init_completion(struct events_group *gr)
{
	gr->comp = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (gr->comp == NULL)
		return -ENOMEM;
	init_completion(gr->comp);
	return 0;	
}

int events_remove_group(struct events *cmc, struct events_group *gr)
{
	struct event *event;
	int rt;
	debug_message();
	for (int i = 0; i < gr->cnt; i++) {
		event = events_search(cmc, gr->name_tab[i]);
		if (event == NULL)
			continue;
		events_diagnose_event(event);
		debug_message();
		rt = events_remove_pid(event->proc_waits, glob_proc, 
				       current->pid);
		debug_message();
		if (rt == -1)
			continue;
		rt = events_remove_compl(event->wait, glob_compl_cnt_max,
				    gr->comp);
		if (rt == -1)
			continue;
		event->g_comp--;
		if (event->g_comp < 0)
			debug_message();
	}
	return 0;
}

int events_insert_group(struct events *cmc, struct events_group *gr)
{
	struct event *event;
	int rt;
	debug_message();
	for (int i = 0; i < gr->cnt; i++) {
		event = events_search(cmc, gr->name_tab[i]);
		if (event == NULL) {
			events_remove_group(cmc, gr);
			debug_message();
			return -EINVAL;
		}
		events_diagnose_event(event);
		debug_message();
		rt = events_add_pid(event->proc_waits, glob_proc, current->pid);	
		if (rt) {
			events_remove_group(cmc, gr);
			debug_message();
			return -EINVAL;
		}
		debug_message();
		if (event->g_comp + 1 >= glob_compl_cnt_max) {
			events_remove_group(cmc, gr);
			return -ENOMEM;
		}
		event->g_comp++;
		rt = events_add_compl(event->wait, glob_compl_cnt_max, 
				      gr->comp);
		if (rt) {
			event->g_comp--;
			events_remove_group(cmc, gr);
			return -ENOMEM;
		}
	}
	return 0;
}

int events_group_wait(struct events *cmc, const char __user *user_buf)
{
	struct events_group events_group;
	int rt;
	memset(&events_group, 0, sizeof(struct events_group));
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;
	rt = events_group_get_events(cmc, &events_group, user_buf);
	if (rt)
		goto ret;
	rt = events_init_completion(&events_group);
	if (rt)
		goto ret;
	rt = events_insert_group(cmc, &events_group);
	if (rt)
		goto ret;
	//mutex_unlock(&cmc->lock);
	//rt = wait_for_completion_interruptible(events_group.comp);
	//if (rt)
	//	return -EINTR;
	//mutex_lock(&cmc->lock);
	events_remove_group(cmc, &events_group);
	rt = 0;
	goto ret;
ret:
	events_del_group_struct(&events_group);
	mutex_unlock(&cmc->lock);
	return rt;
}

int events_throw(struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;

	event = events_get_event(cmc, buf);
	if (unlikely(event == NULL)) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	rt = events_search_pid(event->proc_throws, glob_proc, current->pid);
	if (rt == -1) {
		mutex_unlock(&cmc->lock);
		return -EPERM;
	}
	if (!event->s_comp && !event->g_comp) {
		mutex_unlock(&cmc->lock);
		return 0;
	}
	if (event->s_comp)
		complete_all(event->wait[0]);
	if (event->g_comp) {
		for(int i = 1; i < glob_compl_cnt_max; i++) {
			if (event->wait[i] != NULL) 
				complete_all(event->wait[i]);
		}
	}
	mutex_unlock(&cmc->lock);
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
	.fops = {.owner = THIS_MODULE,
		 .open = events_open,
		 .release = events_release,
		 .unlocked_ioctl = events_ioctl
		},
	.device = NULL,
	.event_list = LIST_HEAD_INIT(cmc.event_list)
};

static int __init events_init(void)
{
	int rt;
	debug_message();
	mutex_init(&cmc.lock);
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
	cmc->event_cnt--;
	list_del(&event->element);
	kfree(event->name);
	event->name = NULL;
	for (int i = 0; i < glob_compl_cnt_max; i++) {
		if (event->wait[i] != NULL) {
			kfree(event->wait[i]);
			event->wait[i] = NULL;
		}
	}
	kfree(event);
}

static void __exit events_exit(void)
{
	struct event *event = NULL, *temp = NULL;
	debug_message();
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
	mutex_lock_interruptible(&cmc.lock);
	list_for_each_entry_safe(event, temp, &cmc.event_list, element) {
		events_diagnose_event(event);
		events_remove_at_exit(&cmc, event);
	}	
	mutex_unlock(&cmc.lock);
}

module_init(events_init);
module_exit(events_exit);
