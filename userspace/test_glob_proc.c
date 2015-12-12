#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * Scenariusz testu:
 * Weryfikacja glob_proc: ograniczona liczba procesów może rzucać 
 * zdarzenie/oczekiwać na zdarzenie.
 * Celem sprawdzenia zostanie utworzony 1 proces sterujący oraz 
 * nadmiarowa grupa procesów rejestrujacych sie.
 *
 *    Parametry modulu do testow:
 *	glob_name_size = 5;
 *	glob_event_cnt_max = 5;
 *	glob_compl_cnt_max = 5;
 *	glob_proc = 3;
 */

void proc1(char *event)
{
	int rt, e0;
	printf("proc1 - event_set.\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc1 - event_set");
	sleep(5);
	printf("proc1 - event_throw.\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	printf("proc1 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 - exits.\n");
	sleep(1);
	exit(0);
}

void proc2(char *event)
{
	printf("proc2 - event_set.\n");
	int rt, e0;
	e0 = event_set(event);
	event_check_error_exit(e0, "proc2 - event_set");	
	printf("proc2 - event_wait.\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	printf("proc2 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("proc2 - exits.\n");
	exit(0);
}

void proc3(char *event)
{
	int rt, e0;
	printf("proc3 - event_set.\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc3 - event_set");
	printf("proc3 - event_wait.\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc3 - event_wait");
	printf("proc3 - event_unset.\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 - exits.\n");
	exit(0);
}

void proc4(char *event)
{
	int e0;
	sleep(1);
	printf("proc4 - event_set. SHOULD FAIL\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc4 - event_set. Positive fail, proc4 exits");
	printf("proc4 - have not failed!\n");
	exit(0);
}

int main(void)
{
	char event[2] = "a\0";
	pid_t pid;
	pid = fork();
	switch(pid) {
	case 0:
		proc2(event);
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3(event);
		break;
	default:
		break;
	}	
	pid = fork();
	switch(pid) {
	case 0:
		proc4(event);
		break;
	default:
		break;
	}	
	proc1(event);	
	return 0;
}
