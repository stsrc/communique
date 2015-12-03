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

int main(void)
{
	int rt, i;
	char *events[] = {"1\0", "2\0", "3\0", "4\0", "5\0", "6\0", "7\0", "8\0"};
	for (i = 1; i < 8; i++) {
		printf("proc1 - event_set.\n");
		rt = event_set(events[i - 1]);
		event_check_error(rt, "proc1 - event_set");
		if (rt)
			break;
	}
	printf("proc1 stoped at %d event\n", i - 1);
	for (int temp = 1; temp < 8; temp++) {
		printf("proc1 - event_unset.\n");
		rt = event_unset(events[temp - 1]);
		event_check_error_exit(rt, "proc1 - event_unset");
	}
	return 0;
}
