#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "../libcheckers/checkers.h"

#include <vector.h>
#include <structs.h>
#include <callout.h>
#include <debug.h>
#include "libprio.h"

int prio_command(struct path * pp)
{
	char buff[CALLOUT_MAX_SIZE];
	char prio[16], *ptr;
	int priority;

	if (!pp->getprio)
		return PRIO_UNDEF;

	if (apply_format(pp->getprio, &buff[0], pp)) {
		condlog(0, "error formatting prio callout command");
		return PRIO_UNDEF;
	} else if (execute_program(buff, prio, 16)) {
		condlog(0, "error calling out %s", buff);
		return PRIO_UNDEF;
	}

	condlog(3, "%s: priority %s", pp->dev, prio);
	priority = strtoul(prio, &ptr, 10);
	if (prio == ptr)
		return PRIO_UNDEF;

	return priority;
}
