#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * Scenariusz testu:
   Weryfikacja glob_compl_cnt_max: 1 zdarzenie moze miec tylko glob_compl_cnt_max - 1 
 * oczekiwań grupowych. Utworzenie jednego procesu rzucającego i nadmiarowej grupy 
 * procesów oczekujących grupowo.
 *
 *    Parametry modulu do testow:
 *	glob_name_size = 5;
 *	glob_event_cnt_max = 5;
 *	glob_compl_cnt_max = 4;
 *	glob_proc = 6;
 */

void proc1(char **events)
{
	int rt;
	sleep(5);
	printf("proc1 - event_throw\n");
	rt = event_throw(events[0]);
	event_check_error_exit(rt, "proc1 - event_throw");
	sleep(3);
	printf("proc1 - event_unset.\n");
	rt = event_unset(events[1]);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 - event_unset.\n");
	rt = event_unset(events[0]);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 - exits.\n");
	exit(0);
}

void proc2(char **events)
{	
	printf("proc2 - event_wait_group.\n");
	int rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc2 - event_wait_group");
	printf("proc2 - exits.\n");
	exit(0);
}

void proc3(char **events)
{
	printf("proc3 - event_wait_group.\n");
	int rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc3 - event_wait_group");
	printf("proc3 - exits.\n");
	exit(0);
}

void proc4(char **events)
{
	printf("proc4 - event_wait_group.\n");
	int rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc4 - event_wait_group");
	printf("proc4 - exits.\n");
	exit(0);
}

void proc5(char **events)
{
	sleep(1);
	printf("proc5 - event_wait_group. SHOULD FAIL\n");
	int rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc5 - event_wait_group, positive fail, proc5 exits");
	printf("proc5 - have not failed!\n");
	exit(0);
}

int main(void)
{
	char *events[] = {"a\0", "b\0"};
	int rt;
	pid_t pid;
	printf("proc1 - event_set.\n");
	rt = event_set(events[0]);
	event_check_error_exit(rt, "proc1 - event_set");
	printf("proc1 - event_set.\n");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc1 - event_set");
	pid = fork();
	switch(pid) {
	case 0:
		proc2(events);
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3(events);
		break;
	default:
		break;
	}	
	pid = fork();
	switch(pid) {
	case 0:
		proc4(events);
		break;
	default:
		break;
	}	
	pid = fork();
	switch(pid) {
	case 0:
		proc5(events);
		break;
	default:
		break;
	}
	proc1(events);	
	return 0;
}
