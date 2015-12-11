#include "events.h"

#define SETEVENT _IOW(0x8A, 0x01, sizeof(char *))
#define WAITFOREVENT _IOW(0x8A, 0x02, sizeof(char *))
#define THROWEVENT _IOW(0x8A, 0x03, sizeof(char *))
#define UNSETEVENT _IOW(0x8A, 0x04, sizeof(char *))
#define WAITINGROUP _IOW(0x8A, 0x05, sizeof(char *))


static inline int event_open()
{
	return open("/dev/events", O_RDWR);
}

int event_get_name_len()
{
	int fd;
	char value[16];
	int rt;	
	memset(value, 0, sizeof(value));
	fd = open("/sys/module/events/parameters/glob_name_size\0", O_RDONLY);
	if (fd < 0)
		return fd;
	rt = read(fd, value, sizeof(value));
	if (rt < 0) {
		errno = EAGAIN;
		return -EAGAIN;
	}
	rt = (int)strtol(value, NULL, 10); 	
	close(fd);
	return rt;
}

int event_check_name(char *name) 
{
	int max_len = event_get_name_len();
	if (max_len < 0)
		return max_len;
	else if (max_len < strlen(name)) {
		errno = EINVAL;
		return -EINVAL;
	}
	return 0;
}

int event_set(char *name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return fd;
	rt = event_check_name(name);
	if (rt)
		return rt;
	rt = ioctl(fd, SETEVENT, name);
	if (!rt)
		rt = fd;
	else
		close(fd);
	return rt;
}

int event_unset(int event_num)
{
	int rt;	
	rt = ioctl(event_num, UNSETEVENT);
	while (rt && (errno == EAGAIN)) {
		sleep(1);
		rt = ioctl(event_num, UNSETEVENT);
	}
	close(event_num);
	return rt;
}

inline int event_throw(int event_num)
{
	return ioctl(event_num, THROWEVENT);
}

inline int event_wait(int event_num)
{
	return ioctl(event_num, WAITFOREVENT);
}

struct wait_group {
	int nbytes;
	char *events;
};

int event_wait_group(char **events, int events_cnt)
{
	struct wait_group wait_group;
	int rt = 0, fd = 0, cnt = 0;
	int max_length = 0;
	int length = 0;
	fd = event_open();
	if (fd < 0)
		return fd;
	max_length = event_get_name_len();
	if (max_length < 0)
		return max_length;
	for (int i = 0; i < events_cnt; i++) {
		length = strlen(events[i]);
		if (length > max_length) {
			errno = EINVAL;
			return -EINVAL;
		}
		cnt += length + 1;
	}
	wait_group.nbytes = cnt;
	wait_group.events = malloc(cnt);
	if (wait_group.events == NULL) {
		close(fd);
		return -ENOMEM;
	}
	memset(wait_group.events, 0, cnt);
	for (int i = 0; i < events_cnt; i++) {
		strcat(wait_group.events, events[i]);
		if (i != events_cnt - 1)
			strcat(wait_group.events, "&");
	}
	rt = ioctl(fd, WAITINGROUP, &wait_group);
	free(wait_group.events);
	close(fd);
	return rt;
}

int event_check_error_exit(int rt, char *string)
{
	if(rt < 0) {
		perror(string);
		exit(rt);
	}
	return 0;
}

int event_check_error(int rt, char *string)
{
	if(rt < 0)
		perror(string);
	return 0;
}
