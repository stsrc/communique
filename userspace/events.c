#include "events.h"

#define SETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x01
#define WAITFOREVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x02
#define	THROWEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x03
#define	UNSETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x04
#define WAITINGROUP (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x05 

static inline int event_open()
{
	return open("/dev/events", O_RDWR);
}

int event_set(char *name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, SETEVENT, name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

int event_unset(char *name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, UNSETEVENT, name);
	while (rt && (errno == EAGAIN)) {
		sleep(1);
		rt = ioctl(fd, UNSETEVENT, name);
	}
	close(fd);
	if (rt)
		return 1;
	return 0;	
}

int event_throw(char *name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, THROWEVENT, name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

int event_wait(char *name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, WAITFOREVENT, name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

struct wait_group {
	int nbytes;
	char *events;
};

int event_wait_group(char **events, int events_cnt)
{
	struct wait_group wait_group;

	int rt, fd, cnt = 0;
	fd = event_open();
	if (fd < 0)
		return 1;
	for (int i = 0; i < events_cnt; i++)
		cnt += strlen(events[i]) + 1;
	wait_group.nbytes = cnt;
	wait_group.events = malloc(cnt);
	if (wait_group.events == NULL) {
		close(fd);
		return 1;
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
	if (rt)
		return 1;
	return 0;
}

int event_check_error_exit(int rt, char *string)
{
	if(rt) {
		perror(string);
		exit(rt);
	}
	return 0;
}

int event_check_error(int rt, char *string)
{
	if(rt)
		perror(string);
	return 0;
}
