#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * Scenariusz testu:
 * glob_name_size: 1 proces tworzy zdarzenie o zbyt dlugiej nazwie.
 * Nie zostalo to zaimplementowane.
 *
 *    Parametry modulu do testow:
 *	glob_name_size = 5;
 *	glob_event_cnt_max = 5;
 *	glob_compl_cnt_max = 5;
 *	glob_proc = 5;
 */

int main(void)
{
	int rt;
	char *event = "wwwwwwwwwwwwwwwwwww\0";
	printf("proc1: event_set. Should fail.\n");
	rt = event_set(event);
	event_check_error_exit(rt, "proc1: event_set. Positive fail\n");
	printf("proc1: event_set have not failed!\n");
	printf("proc1: event_unset.\n");
	event_unset(event);
	printf("proc1: exits.\n");
	return 0;
}
