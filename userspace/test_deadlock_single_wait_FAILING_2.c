#include <stdio.h>
#include <unistd.h>
#include "events.h"
/*
 * TEST SCENARIO:
 * proc2 throws a, waits first for a
 * proc1 throws a, tries to wait for a
 */
void proc2(char *event)
{
	int e0, rt;
	printf("proc2 - event_set\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc2 - event_set");
	printf("proc2 - event_wait\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	e0 = event_unset(e0);
	event_check_error_exit(e0, "proc2 - event_unset");
	printf("proc2 exits\n");
	exit(0);	
}

int main(void)
{
	char *event = "a\0";
	pid_t pid;
	int e0, rt;
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(event);
		break;
	default:
		break;
	}

	printf("proc1 - event_set\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc1 - event_set");
	sleep(2);
	printf("proc1 - event_wait - powinien pojawic sie blad\n");
	rt = event_wait(e0);
	event_check_error(rt, "proc1 - event_wait. Blad prawidlowy.");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 exits\n");
	return 0;
}
