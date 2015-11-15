#include "events.h"

#define SETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x01
#define WAITFOREVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x02
#define	THROWEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x03
#define	UNSETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x04

static inline int event_open()
{
	return open("/dev/events", O_RDWR);
}

int event_set(char name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, SETEVENT, &name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

int event_unset(char name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, UNSETEVENT, &name);
	close(fd);
	if (rt)
		return 1;
	return 0;	
}

int event_throw(char name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, THROWEVENT, &name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

int event_wait(char name)
{
	int rt, fd;
	fd = event_open();
	if (fd < 0)
		return 1;
	rt = ioctl(fd, WAITFOREVENT, &name);
	close(fd);
	if (rt)
		return 1;
	return 0;
}

int event_check_error(int rt, char *string)
{
	if(rt) {
		perror(string);
		exit(rt);
	}
	return 0;
}
