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
#include <linux/mutex.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/moduleparam.h>
#include <asm/delay.h>

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

unsigned int glob_name_size = 5;
module_param(glob_name_size, uint, S_IRUGO);
unsigned int glob_event_cnt_max = 5;
module_param(glob_event_cnt_max, uint, S_IRUGO);
unsigned int glob_compl_cnt_max = 5;
module_param(glob_compl_cnt_max, uint, S_IRUGO);
unsigned int glob_proc = 5;
module_param(glob_proc, uint, S_IRUGO);

struct event {
	char *name;
	struct completion **wait;
	char **completed_by;
	unsigned int s_comp;
	unsigned int g_comp;
	struct list_head element;
	struct task_struct **proc_throws;
	struct task_struct **proc_waits;
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
	unsigned int event_cnt;
};

static struct events cmc;
struct event *events_check_if_proc_waits(struct events *cmc, 
					 struct task_struct *task);
ssize_t events_remove_task(struct task_struct **arr, ssize_t size,
			   struct task_struct *task);

void events_diagnose_event(struct event *event)
{
	if (event == NULL) {
		printk("events_diagnose_event: event NULL!\n");
		return;
	}
	printk(KERN_EMERG "name: %s\n", event->name);
	printk(KERN_EMERG "unsigned int s_comp: %u\n", event->s_comp);
	printk(KERN_EMERG "unsigned int g_comp: %d\n", event->g_comp);
	printk(KERN_EMERG "struct completion **wait: %lu\n",
	       (unsigned long)event->wait);
	for (unsigned int i = 0; i < glob_compl_cnt_max; i++) {
		printk(KERN_EMERG "struct completion *wait[%d]: %lu\n",
		       i, (unsigned long)event->wait[i]);
	}
	printk(KERN_EMERG "struct task_struct **proc_throws: %lu\n",
	       (unsigned long)event->proc_throws);
	for(unsigned int i = 0; i < glob_proc; i++) {
		printk(KERN_EMERG "struct task_struct *proc_throws[%d]: %lu\n",
		       i, (unsigned long)event->proc_throws[i]);
	}
	printk(KERN_EMERG "struct task_struct **proc_waits: %lu\n",
	       (unsigned long)event->proc_waits);
	for(unsigned int i = 0; i < glob_proc; i++) {
		printk(KERN_EMERG "struct task_struct *proc_waits[%d]: %lu\n",
		       i, (unsigned long)event->proc_waits[i]);
	}
}

int events_unset(struct file *file, struct events *cmc);

int events_release(struct inode *inode, struct file *file)
{
	struct event *event = NULL;
	mutex_lock(&cmc.lock);
	if (unlikely(file->private_data != NULL)) {
		event = (struct event *)file->private_data;
		if (event->s_comp)
			complete_all(event->wait[0]);
		if (event->g_comp) {
			for(unsigned int i = 1; i < glob_compl_cnt_max; i++) {
				if (event->wait[i] != NULL) {
					if (completion_done(event->wait[i]))
						continue;
					complete_all(event->wait[i]);
					event->completed_by[i] = event->name;
				}
			}
		}
		mutex_unlock(&cmc.lock);
		events_unset(file, &cmc);
	} else {
		mutex_unlock(&cmc.lock);
	}
	return 0;
}

int events_open(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * WARNING - function dynamically allocates memory!
 * Remember to call free when obtained name string will not be used anymore!
 */
char *events_get_name(const char __user *buf)
{
	int rt;
	char *name = kmalloc(sizeof(char) * (glob_name_size + 1), GFP_KERNEL);
	if (unlikely(name == NULL))
		return NULL;
	memset(name, 0, sizeof(char)*(glob_name_size + 1));
	rt = copy_from_user(name, buf, glob_name_size);
	if (unlikely(rt)) {
		kfree(name);
		return NULL;
	}
	return name;	
}

ssize_t events_search_task(struct task_struct **arr, ssize_t size, 
			   struct task_struct *task)
{
	for (ssize_t i = 0; i < size; i++) {
		if (task == arr[i])
			return i;
	}
	return -1;
}

ssize_t events_add_task(struct task_struct **arr, ssize_t size, 
			struct task_struct *task)
{
	ssize_t i;
	i = events_search_task(arr, size, task);
	if (i != -1)
		return -EINVAL;
	i = events_search_task(arr, size, 0);
	if (i == -1)
		return -ENOMEM;
	arr[i] = task;
	return 0;
}

ssize_t events_remove_task(struct task_struct **arr, ssize_t size,
			   struct task_struct *task)
{
	ssize_t i;
	i = events_search_task(arr, size, task);
	if (i == -1)
		return -1;
	arr[i] = NULL;
	return 0;
}

unsigned int events_non_zero_task(struct task_struct **arr, ssize_t size)
{
	unsigned int cnt = 0;
	for (ssize_t i = 0; i < size; i++) {
		if (arr[i] != NULL)
			cnt++;
	}
	return cnt;
}

ssize_t events_search_compl(struct completion **arr, ssize_t size,
			    struct completion *compl)
{
	for (ssize_t i = 0; i < size; i++) {
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
	return i;
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

struct event *events_get_event(struct events *cmc, const char __user *buf)
{
	struct event *event;
	char *name = events_get_name(buf);
        if (unlikely(name == NULL))
		return NULL;
	event = events_search(cmc, name);
	kfree(name);
	return event;
}

int events_unset_check_if_rest_wait(struct events *cmc, 
				   struct task_struct **proc_throws)
{
	struct event *temp;
	for (unsigned int i = 0; i < glob_proc; i++) {
		if (proc_throws[i] == NULL)
			continue;
		else if (proc_throws[i] == current)
			continue;
		temp = events_check_if_proc_waits(cmc, proc_throws[i]);
		if (temp == NULL)
			return 0;
	}
	return 1;
}

int events_unset_check_deadlock(struct events *cmc, struct event *event)
{
	int rt;
	struct completion *temp;
	if (!(event->s_comp || event->g_comp))
		return 0;
	if (event->s_comp) {
		if (completion_done(event->wait[0]))
			return -EAGAIN;
		rt = events_unset_check_if_rest_wait(cmc, event->proc_throws);
		if (rt)
			return -EDEADLK;
	} else if (event->g_comp) {
		for (unsigned int i = 1; i < glob_compl_cnt_max; i++) {
			temp = event->wait[i];
			if (temp == NULL)
				continue;
			else if (completion_done(temp))
				return -EAGAIN;
		}
		rt = events_unset_check_if_rest_wait(cmc, event->proc_throws);
		if (rt)
			return -EDEADLK;
	}
	return 0;
}

static inline int events_unset_check(struct events *cmc, struct event *event)
{
	int rt;
	rt = events_search_task(event->proc_throws, glob_proc, current);
	if (rt == -1)
		return -EACCES;
	do {
		rt = events_unset_check_deadlock(cmc, event);
		if (rt == -EAGAIN) {
			mutex_unlock(&cmc->lock);
			set_current_state(TASK_INTERRUPTIBLE);
			/*
			 * process is going to sleep, because in near future
			 * there will be possibility for event unset.
			 */
			schedule_timeout(1);
			mutex_lock(&cmc->lock);
			/*
			 * shows how many times loops is repeated
			 */
			debug_message(); 
		}
	} while (rt == -EAGAIN);
	return rt;
}

void events_delete_event(struct events *cmc, struct event *event)
{
	list_del(&event->element);
	if (!cmc->event_cnt)
		debug_message();
	cmc->event_cnt--;
	kfree(event->name);
	event->name = NULL;
	kfree(event->wait[0]);
	event->wait[0] = NULL;
	kfree(event->wait);
	event->wait = NULL;
	kfree(event->completed_by);
	event->completed_by = NULL;
	kfree(event->proc_throws);
	event->proc_throws = NULL;
	kfree(event->proc_waits);
	event->proc_waits = NULL;
	kfree(event);
}

int events_unset(struct file *file, struct events *cmc)
{
	int rt;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;
	event = (struct event *)file->private_data;
	if (event == NULL) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	rt = events_unset_check(cmc, event);
	if (rt < 0)
		goto ret;
	events_remove_task(event->proc_throws, glob_proc, current);
	file->private_data = NULL;
	rt = events_non_zero_task(event->proc_throws, glob_proc);
	if (rt > 0) {
		rt = 0;
		goto ret;
	}
	events_delete_event(cmc, event);
	rt = 0;
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
	event->wait = kmalloc(sizeof(struct completion *)*glob_compl_cnt_max,
			      GFP_KERNEL);
	if (unlikely(event->wait == NULL))
		goto err;
	memset(event->wait, 0, sizeof(struct completion *)*glob_compl_cnt_max);
	event->proc_throws = kmalloc(sizeof(struct task_struct *)*glob_proc,
				     GFP_KERNEL);
	if (unlikely(event->proc_throws == NULL))
		goto err;
	memset(event->proc_throws, 0, sizeof(struct task_struct *)*glob_proc);
	event->proc_waits = kmalloc(sizeof(struct task_struct *)*glob_proc,
				    GFP_KERNEL);
	if (unlikely(event->proc_waits == NULL))
		goto err;
	memset(event->proc_waits, 0, sizeof(struct task_struct *)*glob_proc);
	event->wait[0] = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (unlikely(event->wait[0] == NULL))
		goto err;
	event->completed_by = kmalloc(sizeof(char *)*glob_compl_cnt_max, 
				      GFP_KERNEL);
	if (unlikely(event->completed_by == NULL))
		goto err;
	memset(event->completed_by, 0, sizeof(char *)*glob_compl_cnt_max);
	events_add_task(event->proc_throws, glob_proc, current);
	init_completion(event->wait[0]);
	INIT_LIST_HEAD(&event->element);
	return event;
err:
	kfree(event->wait[0]);
	kfree(event->wait);
	kfree(event->proc_throws);
	kfree(event->proc_waits);
	kfree(event);
	return NULL;
}

int events_set(struct file *file, struct events *cmc, const char __user *buf)
{
	int rt;
	struct event *event;
	if (file->private_data != NULL)
		return -EINVAL;
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
		rt = events_add_task(event->proc_throws, glob_proc, current);
		if (rt == -EINVAL) {
			rt = 0;	
		} else if (rt == -ENOMEM) {
			cmc->event_cnt++;
			goto no_mem;
		}
		kfree(name);
		name = NULL;	
	} else {
		cmc->event_cnt++;
		if (cmc->event_cnt > glob_event_cnt_max)
			goto no_mem;
		event = events_init_event(name);
		if (event == NULL)
			goto no_mem;
		list_add(&event->element, &cmc->event_list);
	}	
	file->private_data = (void *)event;
	mutex_unlock(&cmc->lock);
	return 0;
no_mem:
	if (!cmc->event_cnt)
		debug_message();
	cmc->event_cnt--;
	mutex_unlock(&cmc->lock);
	kfree(name);
	return -ENOMEM;
}

struct event *events_check_if_proc_waits(struct events *cmc, struct task_struct *task)
{
	struct event *event = NULL;
	list_for_each_entry(event, &cmc->event_list, element) {
		if (events_search_task(event->proc_waits, glob_proc, task)
		    != -1) {
			return event;
		}
	}
	return NULL;	
}

int events_check_not_deadlock(struct events *cmc, struct task_struct *task, 
			      struct event *event)
{	
	struct event *temp;
	int rt;
	struct task_struct *task_throwing;
	for (unsigned int i = 0; i < glob_proc; i++) {
		task_throwing = event->proc_throws[i];
		if (task_throwing == NULL)
			continue;
		if (task_throwing == task)
			continue;
		rt = events_search_task(event->proc_waits, glob_proc, 
				       task_throwing);
		if (rt != -1) 
			continue;
		temp = events_check_if_proc_waits(cmc, task_throwing);
		if (temp == NULL)
			return 1;
		rt = events_check_not_deadlock(cmc, task, temp);
		if (rt)
			return rt;
	}
	return 0;
}

int events_wait(struct file *file, struct events *cmc)
{
	int rt = 0;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;
	event = (struct event*)file->private_data;
	if (event == NULL) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	/*
	 * without this loop test_single_wait_2 fails.
	 * schedule() is far more better in this situation than udelay(2);
	 */
	while (event->s_comp && completion_done(event->wait[0])) {
		mutex_unlock(&cmc->lock);
		schedule();
		mutex_lock(&cmc->lock);
		/*
		 * shows how many times loop is repeated
		 */
		debug_message();
	}
	if (completion_done(event->wait[0]))
		reinit_completion(event->wait[0]);
	event->s_comp++;
	rt = events_add_task(event->proc_waits, glob_proc, current);
	if (rt) {
		event->s_comp--;
		mutex_unlock(&cmc->lock);
		return -ENOMEM;
	}
	rt = events_check_not_deadlock(cmc, current, event);
	if (!rt) {
		event->s_comp--;
		events_remove_task(event->proc_waits, glob_proc, current);
		mutex_unlock(&cmc->lock);
		return -EDEADLK;
	}
	mutex_unlock(&cmc->lock);
	rt = wait_for_completion_interruptible(event->wait[0]);
	mutex_lock(&cmc->lock);
	if (rt)
		rt = -EINTR;
	if (!event->s_comp)
		debug_message();
	event->s_comp--;
	events_remove_task(event->proc_waits, glob_proc, current);
	mutex_unlock(&cmc->lock);
	return rt;
}

struct wait_group {
	int nbytes;
	const char __user *buf;
};	

struct events_group {
	unsigned int cnt;
	struct completion *comp;
	char *names;
	char **name_tab;
};

void events_del_group_struct(struct events_group *gr)
{
	kfree(gr->names);
	gr->names = NULL;
	kfree(gr->comp);
	gr->comp = NULL;
	kfree(gr->name_tab);
	gr->name_tab = NULL;
}

int events_group_check_not_deadlock(struct events *cmc, struct events_group *gr)
{
	int rt;
	struct event *event;
	for (unsigned int i = 0; i < gr->cnt; i++) {
		event = events_search(cmc, gr->name_tab[i]);
		if (event == NULL)
			return -EINVAL;
		rt = events_check_not_deadlock(cmc, current, event);
		if (rt)
			return rt;
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
			return -EINVAL;
		if (events_gr->cnt + 1 > glob_event_cnt_max) 
			return -ENOMEM;
		events_gr->name_tab[events_gr->cnt] = name;
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
	events_gr->name_tab = kmalloc(sizeof(char *)*glob_event_cnt_max, 
				      GFP_KERNEL);
	if (events_gr->name_tab == NULL)
		return -ENOMEM;
	memset(events_gr->name_tab, 0, sizeof(char *)*glob_event_cnt_max);
	rt = copy_from_user(events_gr->names, wait_group.buf, wait_group.nbytes);
	if (rt)
		return -EAGAIN;
	rt = events_parse_names(cmc, events_gr);
	return rt;
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
	int rt, completed_by = 0;
	for (unsigned int i = 0; i < gr->cnt; i++) {
		event = events_search(cmc, gr->name_tab[i]);
		if (event == NULL)
			continue;
		rt = events_remove_task(event->proc_waits, glob_proc, 
				       current);
		if (rt == -1)
			continue;
		rt = events_remove_compl(event->wait, glob_compl_cnt_max,
				    gr->comp);
		if (rt == -1)
			continue;
		if (event->completed_by[rt] != NULL) {
			rt = strcmp(event->name, event->completed_by[rt]);
			if (!rt)
				completed_by = i + 1;
			event->completed_by[rt] = NULL;
		}
		if (!event->g_comp)
			debug_message();
		event->g_comp--;
	}
	return completed_by;
}

int events_insert_group(struct events *cmc, struct events_group *gr)
{
	struct event *event;
	int rt;
	for (unsigned int i = 0; i < gr->cnt; i++) {
		event = events_search(cmc, gr->name_tab[i]);
		if (event == NULL) {
			events_remove_group(cmc, gr);
			return -EINVAL;
		}
		rt = events_add_task(event->proc_waits, glob_proc, current);	
		if (rt) {
			events_remove_group(cmc, gr);
			return -EINVAL;
		}
		if (event->g_comp + 1 >= glob_compl_cnt_max) {
			events_remove_group(cmc, gr);
			return -ENOMEM;
		}
		event->g_comp++;
		rt = events_add_compl(event->wait, glob_compl_cnt_max, 
				      gr->comp);
		if (rt) {
			if (!event->g_comp)
				debug_message();
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
	rt = events_group_check_not_deadlock(cmc, &events_group);
	if (rt != 1) {
		rt = -EDEADLK;
		events_remove_group(cmc, &events_group);
		goto ret;
	}
	mutex_unlock(&cmc->lock);
	rt = wait_for_completion_interruptible(events_group.comp);
	if (rt)
		rt = -EINTR;
	mutex_lock(&cmc->lock);
	rt = events_remove_group(cmc, &events_group);
ret:
	events_del_group_struct(&events_group);
	mutex_unlock(&cmc->lock);
	return rt;
}

int events_throw(struct file *file, struct events *cmc)
{
	int rt;
	struct event *event;
	rt = mutex_lock_interruptible(&cmc->lock);
	if (rt)
		return -EINTR;

	event = (struct event *)file->private_data;
	if (event == NULL) {
		mutex_unlock(&cmc->lock);
		return -EINVAL;
	}
	rt = events_search_task(event->proc_throws, glob_proc, current);
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
		for(unsigned int i = 1; i < glob_compl_cnt_max; i++) {
			if (event->wait[i] != NULL) {
				if (completion_done(event->wait[i]))
					continue;
				complete_all(event->wait[i]);
				event->completed_by[i] = event->name;
			}
		}
	}
	mutex_unlock(&cmc->lock);
	return 0;
}

long events_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case SETEVENT:
		return events_set(file, &cmc, (char __user *)arg);
	case WAITFOREVENT:
		return events_wait(file, &cmc);
	case WEITINGROUP:
		return events_group_wait(&cmc, (char __user *)arg);
	case THROWEVENT:
		return events_throw(file, &cmc);
	case UNSETEVENT:
		return events_unset(file, &cmc);
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
	.event_list = LIST_HEAD_INIT(cmc.event_list),
};

static int __init events_init(void)
{
	int rt;
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
	if (!cmc->event_cnt) 
		debug_message();
	list_del(&event->element);
	kfree(event->name);
	event->name = NULL;
	for (unsigned int i = 0; i < glob_compl_cnt_max; i++) {
		if (event->wait[i] != NULL) {
			kfree(event->wait[i]);
			event->wait[i] = NULL;
		}
	}
	kfree(event->completed_by);
	kfree(event->wait);
	kfree(event->proc_throws);
	kfree(event->proc_waits);
	kfree(event);
}

static void __exit events_exit(void)
{
	int rt;
	struct event *event = NULL, *temp = NULL;
	device_destroy(cmc.class, cmc.dev);
	cdev_del(cmc.cdev);
	class_destroy(cmc.class);
	unregister_chrdev_region(cmc.dev, 1);
	rt = mutex_lock_interruptible(&cmc.lock);
	list_for_each_entry_safe(event, temp, &cmc.event_list, element) {
		events_diagnose_event(event);
		events_remove_at_exit(&cmc, event);
	}
	mutex_unlock(&cmc.lock);
}

module_init(events_init);
module_exit(events_exit);
