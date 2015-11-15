#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>
#include <stdlib.h>

#define SETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x01
#define WAITFOREVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x02
#define	THROWEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x03
#define	UNSETEVENT (1 << 30) | (sizeof(char *) << 16) | (0x8A << 8) | 0x04

void ioctl_test(int rt)
{
	if (rt) {
		perror("ioctl");
		exit(1);
	}
}

int main(void)
{
	int rt;
	int cnt = 0;
	int fd = open("/dev/communique", O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	char test[5] = "aaaa\0";
	printf("ioctl SETEVENT proc 2\n");
	rt = ioctl(fd, SETEVENT, test);
	ioctl_test(rt);
	while(cnt < 5) {
		printf("ioctl WAITEVENT proc 2\n");
		rt = ioctl(fd, WAITFOREVENT, test);
		printf("DUPA DUPA DUPA CHUJ\n");
		ioctl_test(rt);
		cnt++;
	}
	sleep(5);
	printf("ioctl UNSETEVENT proc 2\n");
	rt = ioctl(fd, UNSETEVENT, test);
	ioctl_test(rt);
	close(fd);
	return 0;
}
