#include <stdio.h>
#include <unistd.h>
#include "events.h"

int main(void)
{
	char event = 'a';
	int rt;
	pid_t pid;
	rt = event_set(event);
	event_check_error(rt, "event_set");
	pid = fork();
	switch(pid) {
	case 0:
		rt = execl("./proc2", "proc2", (char *)NULL);
		if (rt) {
			perror("execl");
			return 1;
		}
		break;
	default:
		break;
	}
	sleep(3);
	while(1) {
		sleep(2);
		printf("Process 1 throws event\n");
		rt = event_throw(event);
		event_check_error(rt, "event_throw");
	}
	return 0;
}
