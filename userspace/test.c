#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <unistd.h>

#define SETEVENT (1 << 30) | (sizeof(int *) << 16) | ('|' << 8) | 0x70
#define WAITFOREVENT (1 << 30) | (sizeof(int *) << 16) | ('|' << 8) | 0x71
#define	THROWEVENT (1 << 30) | (sizeof(int *) << 16) | ('|' << 8) | 0x72

int main(void)
{
	int rt;
	int fd = open("/dev/communique", O_RDWR);
	int test[3] = {665, 666, 667};
	rt = ioctl(fd, 50, test);
	printf("ioctl with bad request: %d\n", rt);
	rt = ioctl(fd, SETEVENT, test);
	printf("ioctl with SETEVENT: %d, should be 0\n", rt);
	rt = ioctl(fd, WAITFOREVENT, test);
	printf("ioctl with WAITFOREVENT: %d, should be -35\n", rt);
	rt = ioctl(fd, THROWEVENT, test);
	printf("ioctl with THROWEVENT: %d, should be 1\n", rt);
	close(fd);
	printf("SETEVENT %lu, WAITFOREVENT %lu, THROWEVENT %lu\n", SETEVENT,
	       WAITFOREVENT, THROWEVENT);
	return 0;
}
