#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * TEST SCENARIO:
 *2 processes - proc1 and proc2 - talking (one sends, second waits)
 */

void proc1(char *event)
{
	int cnt = 0;
	int rt = 0;
	while(cnt < 4) {
		sleep(3);
		printf("proc1 - throws event.\n");
		rt = event_throw(event);
		if (rt == 1)
			continue;
		event_check_error(rt, "proc1 - event_throw");
		cnt++;
	}
	rt = event_unset(event);
	event_check_error_exit(rt, "proc1 - event_unset");
	exit(0);
}

void proc2(char *event)
{
	int rt;
	int cnt = 0;
	while(cnt < 4) {
		printf("proc2 - sleeps on event.\n");
		rt = event_wait(event);
		printf("proc2 - awaken.\n");
		event_check_error(rt, "proc2 - event_wait");
		cnt++;
	}
	rt = event_unset(event);
	event_check_error_exit(rt, "proc2 - event_unset");
	exit(0);
}

int main(void)
{
	char event[2] = "a\0";
	int rt;
	pid_t pid;
	rt = event_set(event);
	event_check_error(rt, "main: event_set");
	pid = fork();
	switch(pid) {
	case 0:
		proc2(event);		
		break;
	default:
		break;
	}
	proc1(event);
	return 0;
}
