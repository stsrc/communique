#include <stdio.h>
#include <unistd.h>
#include "events.h"

int main(void)
{
	char event[2] = "a\0";
	int rt;
	int cnt = 0;
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
	while(cnt < 4) {
		rt = event_throw(event);
		if (rt == 1)
			continue;
		printf("proc1 throws event.\n");
		event_check_error(rt, "event_throw");
		cnt++;
	}
	rt = event_unset(event);
	event_check_error(rt, "event_unset in proc1");
	return 0;
}
