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
	int e0 = event_set(event);
	event_check_error_exit(e0, "event_set");
	sleep(2);
	while(cnt < 4) {
		sleep(1);
		printf("proc1 - throws event.\n");
		rt = event_throw(e0);
		if (rt == 1)
			continue;
		event_check_error(rt, "proc1 - event_throw");
		cnt++;
	}
	printf("proc1 - event unset\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 exits\n");
	exit(0);
}

void proc2(char *event)
{
	int rt, eid;
	int cnt = 0;
	eid = event_set(event);
	event_check_error_exit(eid, "proc2 - event_set");
	while(cnt < 4) {
		printf("proc2 - sleeps on event.\n");
		rt = event_wait(eid);
		printf("proc2 - awaken.\n");
		event_check_error(rt, "proc2 - event_wait");
		cnt++;
	}
	rt = event_unset(eid);
	event_check_error(rt, "proc2 - events_unset");
	printf("proc2 exits\n");
	exit(0);
}

int main(void)
{
	char event[2] = "a\0";
	pid_t pid;
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(event);		
		break;
	default:
		break;
	}
	proc1(event);
	return 0;
}
