#include <stdio.h>
#include <unistd.h>
#include "events.h"
/*
 * TEST SCENARIO:
 * proc2 throws a, waits first for a
 * proc1 throws a, tries to wait for a
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("proc1 and proc2 sets event \"a\"\n");
	printf("proc2 waits as a first for event \"a\"\n");
	printf("proc1 tries to wait for \"a\" what is causing error.\n\n");
}

void proc2(char *event)
{
	int e0, rt;
	printf("proc2 - event_set(\"a\")\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc2 - event_set");
	printf("proc2 - event_wait(e0)\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	printf("proc2 - event_unset(e0)\n");
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
	print_test_scenario();
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(event);
		break;
	default:
		break;
	}

	printf("proc1 - event_set(\"a\")\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc1 - event_set");
	sleep(2);
	printf("proc1 - event_wait(e0) - powinien pojawic sie blad\n");
	rt = event_wait(e0);
	event_check_error(rt, "proc1 - event_wait. Blad prawidlowy.");
	printf("proc1 - event_throw(e0)\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	printf("proc1 - event_unset(e0)\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 exits\n");
	return 0;
}
