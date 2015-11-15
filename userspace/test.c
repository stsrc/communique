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
	pid_t pid;
	int cnt = 0;
	int rt;
	int fd = open("/dev/communique", O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	char test = 'a';
	printf("ioctl SETEVENT proc 1\n");
	rt = ioctl(fd, SETEVENT, &test);
	ioctl_test(rt);
	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	}
	switch(pid) {
	case 0:
		rt = execl("./test2", NULL);
		if (rt) {
			printf("PROBLEMS!\n");
			close(fd);
			exit(1);
		}
		break;
	default:
		break;
	}
	sleep(3);
	while(cnt < 5) {
		sleep(3);
		printf("ioctl THROWEVENT proc 1\n");
		rt = ioctl(fd, THROWEVENT, &test);
		ioctl_test(rt);	
		cnt++;	
	}
	printf("ioctl UNSETEVENT proc 1\n");
	rt = ioctl(fd, UNSETEVENT, &test);
	ioctl_test(rt);
	close(fd);	
	return 0;
}
