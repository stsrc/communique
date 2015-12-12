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
void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("Creating group completions over the limit of glob_compl_cnt_max"
	       " variable.\nModule parameters:\nglob_name_size = 5;\n"
	       "glob_event_cnt_max = 5;\nglob_compl_cnt_max = 4;\n"
	       "glob_proc = 6;\n\n");
	printf("Press enter to start.\n");
	getchar();	
}
void proc1(char **events)
{
	int rt, e0, e1;
	printf("proc1 - event_set\n");
	e0 = event_set(events[0]);
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc1 - event_set");
	event_check_error_exit(e1, "proc1 - event_set");
	sleep(5);
	printf("proc1 - event_throw\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	printf("proc1 - event_unset.\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	sleep(1);
	printf("proc1 - exits.\n");
	exit(0);
}

void proc2(char **events)
{
	int rt, e0, e1;
	printf("proc2 - event_set\n");
	e0 = event_set(events[0]);
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc2 - event_set");
	event_check_error_exit(e1, "proc2 - event_set");
	printf("proc2 - event_wait_group.\n");
	rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc2 - event_wait_group");
	printf("proc2 - event_unset.\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("proc2 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("proc2 - exits.\n");
	exit(0);
}

void proc3(char **events)
{
	int rt, e0, e1;
	printf("proc3 - event_set\n");
	e0 = event_set(events[0]);
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc3 - event_set");
	event_check_error_exit(e1, "proc3 - event_set");
	printf("proc3 - event_wait_group.\n");
	rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc3 - event_wait_group");
	printf("proc3 - event_unset.\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 - exits.\n");
	exit(0);
}

void proc4(char **events)
{
	int rt, e0, e1;
	printf("proc4 - event_set\n");
	e0 = event_set(events[0]);
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc3 - event_set");
	event_check_error_exit(e1, "proc3 - event_set");
	printf("proc4 - event_wait_group.\n");
	rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc4 - event_wait_group");
	printf("proc4 - event_unset.\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc4 - event_unset");
	printf("proc4 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc4 - event_unset");
	printf("proc4 - exits.\n");
	exit(0);
}

void proc5(char **events)
{
	int rt, e0, e1;
	printf("proc3 - event_set\n");
	e0 = event_set(events[0]);
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc3 - event_set");
	event_check_error_exit(e1, "proc3 - event_set");
	sleep(1);
	printf("proc5 - event_wait_group. SHOULD FAIL\n");
	rt = event_wait_group(events, 2);
	event_check_error_exit(rt, "proc5 - event_wait_group, positive fail, proc5 exits");
	if (!rt)
		printf("proc5 - have not failed!\n");
	printf("proc5 - event_unset.\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc5 - event_unset");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc5 - event_unset");
	printf("proc5 - exits.\n");
	exit(0);
}

int main(void)
{
	char *events[] = {"a\0", "b\0"};
	pid_t pid;
	print_test_scenario();
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(events);
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc3(events);
		break;
	default:
		break;
	}	
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc4(events);
		break;
	default:
		break;
	}	
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc5(events);
		break;
	default:
		break;
	}
	proc1(events);	
	return 0;
}
