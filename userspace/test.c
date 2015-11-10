#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <unistd.h>

#define SETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x01
#define WAITFOREVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x02
#define	THROWEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x03

static inline void perror_ioctl(int rt)
{
	if (rt)
		perror("ioctl");
}

int main(void)
{
	int rt;
	pid_t pid;
	int fd = open("/dev/communique", O_RDWR);
	if (fd <= 0) {
		perror("open");
		return 1;
	}
	int test1[2] = {666, 10};
	int test2[2] = {777, 11};
	int test3[2] = {888, 12};
	rt = ioctl(fd, SETEVENT, test1);
	printf("SETEVENT\n");
	perror_ioctl(rt);
	if (rt)
		return 1;
	pid = fork();
	switch(pid) {
	case 0:
		execl("./test2\0", (char *)NULL);
	default:
		break;
	}
	while(1) {
		sleep(2);
		printf("throws test1\n");
		rt = ioctl(fd, THROWEVENT, test1);
		printf("test1 has been thrown\n");
		perror_ioctl(rt);
		sleep(2);
		printf("throws test2\n");
		rt = ioctl(fd, THROWEVENT, test2);
		printf("test2 has been thrown\n");
		perror_ioctl(rt);
		sleep(2);
		printf("throws test3\n");
		rt = ioctl(fd, THROWEVENT, test3);
		printf("test3 has been thrown\n");
		perror_ioctl(rt);
	}
	return 0;
}