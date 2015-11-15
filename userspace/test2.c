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
	int test1 = 10;
	while(1) {
		printf("wait for 1\n");
		rt = ioctl(fd, WAITFOREVENT, &test1);
		perror_ioctl(rt);
	}
}
