#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * Scenariusz testu:
 * glob_event_cnt_max: 1 proces probuje utworzyc za duzo zdarzen.
 *
 *    Parametry modulu do testow:
 *	glob_name_size = 5;
 *	glob_event_cnt_max = 5;
 *	glob_compl_cnt_max = 5;
 *	glob_proc = 5;
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n1 process tries to make to many events.\n");
	printf("Number of events is limited by glob_event_cnt_max variable.\n\n");
	printf("Module parameters:\nglob_name_size = 5;\n"
	       "glob_event_cnt_max = 5;\nglob_compl_cnt_max = 5;\n"
	       "glob_proc = 5;\n\n");
	printf("Press enter to start.\n");
	getchar();
}

int main(void)
{
	int rt, i, e[8];
	char *events[] = {"1\0", "2\0", "3\0", "4\0", "5\0", "6\0", "7\0", "8\0"};
	print_test_scenario();
	for (i = 1; i < 8; i++) {
		printf("proc1 - event_set.\n");
		e[i-1] = event_set(events[i - 1]);
		event_check_error(e[i-1], "proc1 - event_set");
		if (e[i-1] < 0)
			break;
	}
	printf("proc1 stoped at %dth event\n", i - 1);
	for (int temp = 1; temp < i; temp++) {
		printf("proc1 - event_unset.\n");
		rt = event_unset(e[temp - 1]);
		event_check_error_exit(rt, "proc1 - event_unset");
	}
	return 0;
}
