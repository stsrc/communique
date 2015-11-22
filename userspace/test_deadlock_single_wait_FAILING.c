#include <stdio.h>
#include <unistd.h>
#include "events.h"

void proc2(char **events)
{
	int rt;
	printf("proc2 - event_set\n");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc2 - event_set FAILURE");
	printf("proc2 - event_wait\n");
	rt = event_wait(events[0]);
	event_check_error_exit(rt, "proc2 - event_wait FAILURE");
	printf("proc2 - exits, but should not\n");
	exit(0);	
}

void proc3(char **events)
{
	int rt;
	printf("proc3 - event_set\n");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc3 - event_set FAIULRE");
	printf("proc3 - event_wait\n");
	rt = event_wait(events[1]);
	event_check_error_exit(rt, "proc3 - event_wait FAILURE");
	printf("proc3 - exits, but should not\n");
	exit(0);	
}

int main(void)
{
	char *events[] = {"a\0", "b\0", "c\0"};
	pid_t pid;
	printf("proc1 - exits, event_set\n");
	int rt = event_set(events[0]);
	event_check_error_exit(rt, "proc1 - event_set. SHOULD NOT FAIL!");
	pid = fork();
	switch(pid) {
	case 0:
		proc2(events);
		break;
	default:
		break;
	}
	sleep(1);
	pid = fork();
	switch (pid) {
	case 0:
		proc3(events);
		break;
	default:
		break;
	}
	sleep(1);
	printf("proc1 - event_wait - should fail\n");
	rt = event_wait(events[2]);
	event_check_error_exit(rt, "proc1 - event_wait SHOULD FAIL!");
	printf("PROC3 NO FAILURE - WHOOOP WHOOP ITS WRONG\n");
}
