#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <linux/kdev_t.h>
#include <libdevmapper.h>

static void usage(char * progname) {
	fprintf(stderr, "usage : %s [-t target type] dev_t\n", progname);
	fprintf(stderr, "where dev_t is either 'major minor' or 'major:minor'\n");
	exit(1);
}

char *
dm_mapname(int major, int minor)
{
	struct dm_task *dmt;
        struct dm_names *names;
        long next = 0;
	char *mapname = NULL;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return NULL;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

        if (!(names = dm_task_get_names(dmt)))
                goto out;
                                                                                
        if (!names->dev) {
                printf("No devices found\n");
                goto out;
        }
                                                                                
        do {
                names = (void *) names + next;
		if ((int) MAJOR(names->dev) == major &&
		    (int) MINOR(names->dev) == minor) {
		    mapname = strdup (names->name);
		    goto out;
		}
                next = names->next;
        } while (next);

out:
	dm_task_destroy(dmt);
	return mapname;
}

int dm_target_type(char *mapname, char *type)
{
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;
	int r = 1;

	if (!(dmt = dm_task_create(DM_DEVICE_STATUS)))
		return 1;

	if (!dm_task_set_name(dmt, mapname))
		goto bad;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto bad;

	if (!type)
		goto good;

	do {
		next = dm_get_next_target(dmt, next, &start, &length,
					  &target_type, &params);
		if (target_type && strcmp(target_type, type))
			goto bad;
	} while (next);

good:
	printf("%s", dm_task_get_name(dmt));
	r = 0;
bad:
	dm_task_destroy(dmt);
	return r;
}

int main(int argc, char **argv)
{
	int c, retval = 0;
	int major, minor;
	char *target_type = NULL;
	char *mapname;

	while ((c = getopt(argc, argv, "t:")) != -1) {
		switch (c) {
		case 't':
			target_type = optarg;
			break;
		default:
			usage(argv[0]);
			return 1;
			break;
		}
	}

	/* sanity check */
	if (optind == argc - 2) {
		major = atoi(argv[argc - 2]);
		minor = atoi(argv[argc - 1]);
	} else if (optind != argc - 1 ||
		   2 != sscanf(argv[argc - 1], "%i:%i", &major, &minor)) {
		usage(argv[0]);
		return 1;
	}

	mapname = dm_mapname(major,minor);
	if (!mapname)
		return 1;

	if (target_type)
		retval = dm_target_type(mapname, target_type);
	else
		printf("%s", mapname);
	
	free(mapname);

	return retval;
}

