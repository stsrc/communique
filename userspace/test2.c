#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

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
	int fd = open("/dev/communique", O_RDWR);
	if (fd <= 0) {
		perror("open");
		return 1;
	}
	int test1[2] = {666, 10};
	int test2[2] = {777, 11};
	int test3[2] = {888, 12};
	while(1) {
		printf("wait for 1\n");
		rt = ioctl(fd, WAITFOREVENT, &(test1[1]));
		perror_ioctl(rt);
		printf("catched 1\n");
		printf("wait for 2\n");
		rt = ioctl(fd, WAITFOREVENT, &(test2[1]));
		perror_ioctl(rt);
		printf("catched 2\n");
		printf("wait for 3\n");
		rt = ioctl(fd, WAITFOREVENT, &(test3[1]));
		perror_ioctl(rt);
		printf("catched 3\n");
	}
}
