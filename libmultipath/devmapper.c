/*
 * snippets copied from device-mapper dmsetup.c
 * Copyright (c) 2004, 2005 Christophe Varoqui
 * Copyright (c) 2005 Kiyoshi Ueda, NEC
 * Copyright (c) 2005 Patrick Caulfield, Redhat
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <libdevmapper.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "checkers.h"
#include "vector.h"
#include "structs.h"
#include "debug.h"
#include "memory.h"
#include "devmapper.h"
#include "config.h"

#include "log_pthread.h"
#include <sys/types.h>
#include <time.h>

#define MAX_WAIT 5
#define LOOPS_PER_SEC 5

#define UUID_PREFIX "mpath-"
#define UUID_PREFIX_LEN 6

static void
dm_write_log (int level, const char *file, int line, const char *f, ...)
{
	va_list ap;
	int thres;

	if (level > 6)
		level = 6;

	thres = (conf) ? conf->verbosity : 0;
	if (thres <= 3 || level > thres)
		return;

	va_start(ap, f);
	if (!logsink) {
		time_t t = time(NULL);
		struct tm *tb = localtime(&t);
		char buff[16];

		strftime(buff, sizeof(buff), "%b %d %H:%M:%S", tb);
		buff[sizeof(buff)-1] = '\0';

		fprintf(stdout, "%s | ", buff);
		fprintf(stdout, "libdevmapper: %s(%i): ", file, line);
		vfprintf(stdout, f, ap);
		fprintf(stdout, "\n");
	} else {
		condlog(level, "libdevmapper: %s(%i): ", file, line);
		log_safe(level + 3, f, ap);
	}
	va_end(ap);

	return;
}

extern void
dm_init(void) {
	dm_log_init(&dm_write_log);
	dm_log_init_verbose(conf ? conf->verbosity + 3 : 0);
}

static int
dm_libprereq (void)
{
	char version[64];
	int v[3];
	int minv[3] = {1, 2, 8};

	dm_get_library_version(version, sizeof(version));
	condlog(3, "libdevmapper version %s", version);
	sscanf(version, "%d.%d.%d ", &v[0], &v[1], &v[2]);

	if ((v[0] > minv[0]) ||
	    ((v[0] ==  minv[0]) && (v[1] > minv[1])) ||
	    ((v[0] == minv[0]) && (v[1] == minv[1]) && (v[2] >= minv[2])))
		return 0;
	condlog(0, "libdevmapper version must be >= %d.%.2d.%.2d",
		minv[0], minv[1], minv[2]);
	return 1;
}

static int
dm_drvprereq (char * str)
{
	int r = 2;
	struct dm_task *dmt;
	struct dm_versions *target;
	struct dm_versions *last_target;
	int minv[3] = {1, 0, 3};
	unsigned int *v;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST_VERSIONS)))
		return 3;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt)) {
		condlog(0, "Can not communicate with kernel DM");
		goto out;
	}
	target = dm_task_get_versions(dmt);

	do {
		last_target = target;
		if (!strncmp(str, target->name, strlen(str))) {
			r = 1;
			break;
		}
		target = (void *) target + target->next;
	} while (last_target != target);

	if (r == 2) {
		condlog(0, "DM multipath kernel driver not loaded");
		goto out;
	}
	v = target->version;
	if ((v[0] > minv[0]) ||
	    ((v[0] == minv[0]) && (v[1] > minv[1])) ||
	    ((v[0] == minv[0]) && (v[1] == minv[1]) && (v[2] >= minv[2]))) {
		r = 0;
		goto out;
	}
	condlog(0, "DM multipath kernel driver must be >= %u.%.2u.%.2u",
		minv[0], minv[1], minv[2]);
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_prereq (void)
{
	if (dm_libprereq())
		return 1;
	return dm_drvprereq(TGT_MPATH);
}

static int
dm_simplecmd (int task, const char *name, int no_flush) {
	int r = 0;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, name))
		goto out;

	dm_task_no_open_count(dmt);
	dm_task_skip_lockfs(dmt);	/* for DM_DEVICE_RESUME */
#ifdef LIBDM_API_FLUSH
	if (no_flush)
		dm_task_no_flush(dmt);		/* for DM_DEVICE_SUSPEND/RESUME */
#endif

	r = dm_task_run (dmt);

	out:
	dm_task_destroy (dmt);
	return r;
}

extern int
dm_simplecmd_flush (int task, const char *name) {
	return dm_simplecmd(task, name, 0);
}

extern int
dm_simplecmd_noflush (int task, const char *name) {
	return dm_simplecmd(task, name, 1);
}

extern int
dm_addmap (int task, const char *target, struct multipath *mpp, char * params,
	   int use_uuid, int ro) {
	int r = 0;
	struct dm_task *dmt;
	char *prefixed_uuid = NULL;

	if (!(dmt = dm_task_create (task)))
		return 0;

	if (!dm_task_set_name (dmt, mpp->alias))
		goto addout;

	if (!dm_task_add_target (dmt, 0, mpp->size, target, params))
		goto addout;

	if (ro)
		dm_task_set_ro(dmt);

	if (use_uuid && mpp->wwid){
		prefixed_uuid = MALLOC(UUID_PREFIX_LEN + strlen(mpp->wwid) + 1);
		if (!prefixed_uuid) {
			condlog(0, "cannot create prefixed uuid : %s\n",
				strerror(errno));
			goto addout;
		}
		sprintf(prefixed_uuid, UUID_PREFIX "%s", mpp->wwid);
		if (!dm_task_set_uuid(dmt, prefixed_uuid))
			goto freeout;
	}

	if (mpp->attribute_flags & (1 << ATTR_MODE) &&
	    !dm_task_set_mode(dmt, mpp->mode))
		goto freeout;
	if (mpp->attribute_flags & (1 << ATTR_UID) &&
	    !dm_task_set_uid(dmt, mpp->uid))
		goto freeout;
	if (mpp->attribute_flags & (1 << ATTR_GID) &&
	    !dm_task_set_gid(dmt, mpp->gid))
		goto freeout;
	condlog(4, "%s: addmap [0 %llu %s %s]\n", mpp->alias, mpp->size,
		target, params);

	dm_task_no_open_count(dmt);

	r = dm_task_run (dmt);

	freeout:
	if (prefixed_uuid)
		FREE(prefixed_uuid);

	addout:
	dm_task_destroy (dmt);

	return r;
}

static int
_dm_addmap_create (struct multipath *mpp, char * params, int ro) {
	int r;
	r = dm_addmap(DM_DEVICE_CREATE, TGT_MPATH, mpp, params, 1, ro);
	/*
	 * DM_DEVICE_CREATE is actually DM_DEV_CREATE + DM_TABLE_LOAD.
	 * Failing the second part leaves an empty map. Clean it up.
	 */
	if (!r && dm_map_present(mpp->alias)) {
		condlog(3, "%s: failed to load map (a path might be in use)",
			mpp->alias);
		dm_flush_map(mpp->alias);
	}
	return r;
}

#define ADDMAP_RW 0
#define ADDMAP_RO 1

extern int
dm_addmap_create (struct multipath *mpp, char *params) {
	return _dm_addmap_create(mpp, params, ADDMAP_RW);
}

extern int
dm_addmap_create_ro (struct multipath *mpp, char *params) {
	return _dm_addmap_create(mpp, params, ADDMAP_RO);
}

extern int
dm_addmap_reload (struct multipath *mpp, char *params) {
	return dm_addmap(DM_DEVICE_RELOAD, TGT_MPATH, mpp, params, 0, ADDMAP_RW);
}

extern int
dm_addmap_reload_ro (struct multipath *mpp, char *params) {
	return dm_addmap(DM_DEVICE_RELOAD, TGT_MPATH, mpp, params, 0, ADDMAP_RO);
}

extern int
dm_map_present (const char * str)
{
	int r = 0;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, str))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	if (info.exists)
		r = 1;
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_get_map(char * name, unsigned long long * size, char * outparams)
{
	int r = 1;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params = NULL;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		return 1;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (size)
		*size = length;

	if (outparams) {
		if (snprintf(outparams, PARAMS_SIZE, "%s", params) <= PARAMS_SIZE)
			r = 0;
	} else
		r = 0;
out:
	dm_task_destroy(dmt);
	return r;
}

static int
dm_get_prefixed_uuid(const char *name, char *uuid)
{
	struct dm_task *dmt;
	const char *uuidtmp;
	int r = 1;

	dmt = dm_task_create(DM_DEVICE_INFO);
	if (!dmt)
		return 1;

	if (!dm_task_set_name (dmt, name))
		goto uuidout;

	if (!dm_task_run(dmt))
		goto uuidout;

	uuidtmp = dm_task_get_uuid(dmt);
	if (uuidtmp)
		strcpy(uuid, uuidtmp);
	else
		uuid[0] = '\0';

	r = 0;
uuidout:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_get_uuid(char *name, char *uuid)
{
	char uuidtmp[WWID_SIZE];

	if (dm_get_prefixed_uuid(name, uuidtmp))
		return 1;

	if (!strncmp(uuidtmp, UUID_PREFIX, UUID_PREFIX_LEN))
		strcpy(uuid, uuidtmp + UUID_PREFIX_LEN);
	else
		strcpy(uuid, uuidtmp);

	return 0;
}

/*
 * returns:
 *    0 : if both uuids end with same suffix which starts with UUID_PREFIX
 *    1 : otherwise
 */
int
dm_compare_uuid(const char* mapname1, const char* mapname2)
{
	char *p1, *p2;
	char uuid1[WWID_SIZE], uuid2[WWID_SIZE];

	if (dm_get_prefixed_uuid(mapname1, uuid1))
		return 1;

	if (dm_get_prefixed_uuid(mapname2, uuid2))
		return 1;

	p1 = strstr(uuid1, UUID_PREFIX);
	p2 = strstr(uuid2, UUID_PREFIX);
	if (p1 && p2 && !strcmp(p1, p2))
		return 0;

	return 1;
}

extern int
dm_get_status(char * name, char * outstatus)
{
	int r = 1;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type;
	char *status;

	if (!(dmt = dm_task_create(DM_DEVICE_STATUS)))
		return 1;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &status);

	if (snprintf(outstatus, PARAMS_SIZE, "%s", status) <= PARAMS_SIZE)
		r = 0;
out:
	if (r)
		condlog(0, "%s: error getting map status string", name);

	dm_task_destroy(dmt);
	return r;
}

/*
 * returns:
 *    1 : match
 *    0 : no match
 *   -1 : empty map
 */
extern int
dm_type(const char * name, char * type)
{
	int r = 0;
	struct dm_task *dmt;
	void *next = NULL;
	uint64_t start, length;
	char *target_type = NULL;
	char *params;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	/* Fetch 1st target */
	next = dm_get_next_target(dmt, next, &start, &length,
				  &target_type, &params);

	if (!target_type)
		r = -1;
	else if (!strcmp(target_type, type))
		r = 1;

out:
	dm_task_destroy(dmt);
	return r;
}

static int
dm_dev_t (const char * mapname, char * dev_t, int len)
{
	int r = 1;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	r = info.open_count;
	if (snprintf(dev_t, len, "%i:%i", info.major, info.minor) > len)
		    goto out;

	r = 0;
out:
	dm_task_destroy(dmt);
	return r;
}

int
dm_get_opencount (const char * mapname)
{
	int r = -1;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	r = info.open_count;
out:
	dm_task_destroy(dmt);
	return r;
}

int
dm_get_major (char * mapname)
{
	int r = -1;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	r = info.major;
out:
	dm_task_destroy(dmt);
	return r;
}

int
dm_get_minor (char * mapname)
{
	int r = -1;
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	r = info.minor;
out:
	dm_task_destroy(dmt);
	return r;
}

extern int
dm_flush_map (const char * mapname)
{
	int r;

	if (!dm_map_present(mapname))
		return 0;

	if (dm_type(mapname, TGT_MPATH) <= 0)
		return 0; /* nothing to do */

	if (dm_remove_partmaps(mapname))
		return 1;

	if (dm_get_opencount(mapname)) {
		condlog(2, "%s: map in use", mapname);
		return 1;
	}

	r = dm_simplecmd_flush(DM_DEVICE_REMOVE, mapname);

	if (r) {
		condlog(4, "multipath map %s removed", mapname);
		return 0;
	}
	return 1;
}

extern int
dm_suspend_and_flush_map (const char * mapname)
{
	int r, s;

	if (!dm_map_present(mapname))
		return 0;

	if (dm_type(mapname, TGT_MPATH) <= 0)
		return 0; /* nothing to do */

	s = dm_queue_if_no_path((char *)mapname, 0);
	if (!s)
		s = dm_simplecmd_flush(DM_DEVICE_SUSPEND, mapname);

	if (dm_remove_partmaps(mapname))
		goto resume;

	if (dm_get_opencount(mapname)) {
		condlog(2, "%s: map in use", mapname);
		goto resume;
	}

	r = dm_simplecmd_flush(DM_DEVICE_REMOVE, mapname);
	if (!r) {
		s = 1;
		goto resume;
	}

	condlog(4, "multipath map %s removed", mapname);
	return 0;
resume:
	if (s)
		dm_simplecmd_noflush(DM_DEVICE_RESUME, mapname);
	return 1;
}

extern int
dm_flush_maps (void)
{
	int r = 0;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;

	if (!(dmt = dm_task_create (DM_DEVICE_LIST)))
		return 0;

	dm_task_no_open_count(dmt);

	if (!dm_task_run (dmt))
		goto out;

	if (!(names = dm_task_get_names (dmt)))
		goto out;

	if (!names->dev)
		goto out;

	do {
		r |= dm_suspend_and_flush_map(names->name);
		next = names->next;
		names = (void *) names + next;
	} while (next);

	out:
	dm_task_destroy (dmt);
	return r;
}

int
dm_message(char * mapname, char * message)
{
	int r = 1;
	struct dm_task *dmt;

	if (!(dmt = dm_task_create(DM_DEVICE_TARGET_MSG)))
		return 1;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	if (!dm_task_set_sector(dmt, 0))
		goto out;

	if (!dm_task_set_message(dmt, message))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	r = 0;
out:
	if (r)
		condlog(0, "DM message failed [%s]", message);

	dm_task_destroy(dmt);
	return r;
}

int
dm_fail_path(char * mapname, char * path)
{
	char message[32];

	if (snprintf(message, 32, "fail_path %s\n", path) > 32)
		return 1;

	return dm_message(mapname, message);
}

int
dm_reinstate_path(char * mapname, char * path)
{
	char message[32];

	if (snprintf(message, 32, "reinstate_path %s\n", path) > 32)
		return 1;

	return dm_message(mapname, message);
}

int
dm_queue_if_no_path(char *mapname, int enable)
{
	char *message;

	if (enable)
		message = "queue_if_no_path\n";
	else
		message = "fail_if_no_path\n";

	return dm_message(mapname, message);
}

int
dm_set_pg_timeout(char *mapname, int timeout_val)
{
	char message[24];

	if (snprintf(message, 24, "set_pg_timeout %d", timeout_val) >= 24)
		return 1;
	return dm_message(mapname, message);
}

static int
dm_groupmsg (char * msg, char * mapname, int index)
{
	char message[32];

	if (snprintf(message, 32, "%s_group %i\n", msg, index) > 32)
		return 1;

	return dm_message(mapname, message);
}

int
dm_switchgroup(char * mapname, int index)
{
	return dm_groupmsg("switch", mapname, index);
}

int
dm_enablegroup(char * mapname, int index)
{
	return dm_groupmsg("enable", mapname, index);
}

int
dm_disablegroup(char * mapname, int index)
{
	return dm_groupmsg("disable", mapname, index);
}

int
dm_get_maps (vector mp)
{
	struct multipath * mpp;
	int r = 1;
	int info;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;

	if (!mp)
		return 1;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 1;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!(names = dm_task_get_names(dmt)))
		goto out;

	if (!names->dev) {
		r = 0; /* this is perfectly valid */
		goto out;
	}

	do {
		info = dm_type(names->name, TGT_MPATH);

		if (info <= 0)
			goto next;

		mpp = alloc_multipath();

		if (!mpp)
			goto out;

		mpp->alias = STRDUP(names->name);

		if (!mpp->alias)
			goto out1;

		if (info > 0) {
			if (dm_get_map(names->name, &mpp->size, mpp->params))
				goto out1;

			if (dm_get_status(names->name, mpp->status))
				goto out1;

			dm_get_uuid(names->name, mpp->wwid);
			dm_get_info(names->name, &mpp->dmi);
		}

		if (!vector_alloc_slot(mp))
			goto out1;

		vector_set_slot(mp, mpp);
		mpp = NULL;
next:
		next = names->next;
		names = (void *) names + next;
	} while (next);

	r = 0;
	goto out;
out1:
	free_multipath(mpp, KEEP_PATHS);
out:
	dm_task_destroy (dmt);
	return r;
}

extern int
dm_get_name(char *uuid, char *name)
{
	vector vec;
	struct multipath *mpp;
	int i, rc = 0;

	vec = vector_alloc();

	if (!vec)
		return 0;

	if (dm_get_maps(vec)) {
		goto out;
	}

	vector_foreach_slot(vec, mpp, i) {
		if (!strcmp(uuid, mpp->wwid)) {
			strcpy(name, mpp->alias);
			rc=1;
			break;
		}
	}
out:
	vector_foreach_slot(vec, mpp, i) {
		free_multipath(mpp, KEEP_PATHS);
	}
	vector_free(vec);
	return rc;
}

int
dm_geteventnr (char *name)
{
	struct dm_task *dmt;
	struct dm_info info;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info)) {
		info.event_nr = 0;
		goto out;
	}

	if (!info.exists) {
		info.event_nr = 0;
		goto out;
	}

out:
	dm_task_destroy(dmt);

	return info.event_nr;
}

char *
dm_mapname(int major, int minor)
{
	char * response = NULL;
	const char *map;
	struct dm_task *dmt;
	int r;
	int loop = MAX_WAIT * LOOPS_PER_SEC;

	if (!(dmt = dm_task_create(DM_DEVICE_STATUS)))
		return NULL;

	if (!dm_task_set_major(dmt, major) ||
	    !dm_task_set_minor(dmt, minor))
		goto bad;

	dm_task_no_open_count(dmt);

	/*
	 * device map might not be ready when we get here from
	 * daemon uev_trigger -> uev_add_map
	 */
	while (--loop) {
		r = dm_task_run(dmt);

		if (r)
			break;

		usleep(1000 * 1000 / LOOPS_PER_SEC);
	}

	if (!r) {
		condlog(0, "%i:%i: timeout fetching map name", major, minor);
		goto bad;
	}

	map = dm_task_get_name(dmt);
	if (map && strlen(map))
		response = STRDUP((char *)dm_task_get_name(dmt));

	dm_task_destroy(dmt);
	return response;
bad:
	dm_task_destroy(dmt);
	condlog(0, "%i:%i: error fetching map name", major, minor);
	return NULL;
}

int
dm_remove_partmaps (const char * mapname)
{
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	char params[PARAMS_SIZE];
	unsigned long long size;
	char dev_t[32];
	int r = 1;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 1;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!(names = dm_task_get_names(dmt)))
		goto out;

	if (!names->dev) {
		r = 0; /* this is perfectly valid */
		goto out;
	}

	if (dm_dev_t(mapname, &dev_t[0], 32))
		goto out;

	do {
		if (
		    /*
		     * if devmap target is "linear"
		     */
		    (dm_type(names->name, TGT_PART) > 0) &&

		    /*
		     * and both uuid end with same suffix starting
		     * at UUID_PREFIX
		     */
		    (!dm_compare_uuid(names->name, mapname)) &&

		    /*
		     * and we can fetch the map table from the kernel
		     */
		    !dm_get_map(names->name, &size, &params[0]) &&

		    /*
		     * and the table maps over the multipath map
		     */
		    strstr(params, dev_t)
		   ) {
			/*
			 * then it's a kpartx generated partition.
			 * remove it.
			 */
			/*
			 * if the opencount is 0 maybe some other
			 * partitions depend on it.
			 */
			if (dm_get_opencount(names->name)) {
				dm_remove_partmaps(names->name);
				if (dm_get_opencount(names->name)) {
					condlog(2, "%s: map in use",
						names->name);
					goto out;
				}
			}
			condlog(4, "partition map %s removed",
				names->name);
			dm_simplecmd_flush(DM_DEVICE_REMOVE, names->name);
		}

		next = names->next;
		names = (void *) names + next;
	} while (next);

	r = 0;
out:
	dm_task_destroy (dmt);
	return r;
}

static struct dm_info *
alloc_dminfo (void)
{
	return MALLOC(sizeof(struct dm_info));
}

int
dm_get_info (char * mapname, struct dm_info ** dmi)
{
	int r = 1;
	struct dm_task *dmt = NULL;

	if (!mapname)
		return 1;

	if (!*dmi)
		*dmi = alloc_dminfo();

	if (!*dmi)
		return 1;

	if (!(dmt = dm_task_create(DM_DEVICE_INFO)))
		goto out;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, *dmi))
		goto out;

	r = 0;
out:
	if (r) {
		memset(*dmi, 0, sizeof(struct dm_info));
		FREE(*dmi);
		*dmi = NULL;
	}

	if (dmt)
		dm_task_destroy(dmt);

	return r;
}

int
dm_rename_partmaps (char * old, char * new)
{
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	char buff[PARAMS_SIZE];
	unsigned long long size;
	char dev_t[32];
	int r = 1;

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 1;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!(names = dm_task_get_names(dmt)))
		goto out;

	if (!names->dev) {
		r = 0; /* this is perfectly valid */
		goto out;
	}

	if (dm_dev_t(old, &dev_t[0], 32))
		goto out;

	do {
		if (
		    /*
		     * if devmap target is "linear"
		     */
		    (dm_type(names->name, TGT_PART) > 0) &&

		    /*
		     * and the multipath mapname and the part mapname start
		     * the same
		     */
		    !strncmp(names->name, old, strlen(old)) &&

		    /*
		     * and we can fetch the map table from the kernel
		     */
		    !dm_get_map(names->name, &size, &buff[0]) &&

		    /*
		     * and the table maps over the multipath map
		     */
		    strstr(buff, dev_t)
		   ) {
				/*
				 * then it's a kpartx generated partition.
				 * Rename it.
				 */
				snprintf(buff, PARAMS_SIZE, "%s%s",
					 new, names->name + strlen(old));
				dm_rename(names->name, buff);
				condlog(4, "partition map %s renamed",
					names->name);
		   }

		next = names->next;
		names = (void *) names + next;
	} while (next);

	r = 0;
out:
	dm_task_destroy (dmt);
	return r;
}

int
dm_rename (char * old, char * new)
{
	int r = 0;
	struct dm_task *dmt;

	if (dm_rename_partmaps(old, new))
		return r;

	if (!(dmt = dm_task_create(DM_DEVICE_RENAME)))
		return r;

	if (!dm_task_set_name(dmt, old))
		goto out;

	if (!dm_task_set_newname(dmt, new))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	r = 1;
out:
	dm_task_destroy(dmt);
	return r;
}

void dm_reassign_deps(char *table, char *dep, char *newdep)
{
	char *p, *n;
	char newtable[PARAMS_SIZE];

	strcpy(newtable, table);
	p = strstr(newtable, dep);
	n = table + (p - newtable);
	strcpy(n, newdep);
	n += strlen(newdep);
	p += strlen(dep);
	strcat(n, p);
}

int dm_reassign_table(const char *name, char *old, char *new)
{
	int r, modified = 0;
	uint64_t start, length;
	struct dm_task *dmt, *reload_dmt;
	char *target, *params = NULL;
	char buff[PARAMS_SIZE];
	void *next = NULL;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		return 0;

	if (!dm_task_set_name(dmt, name))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;
	if (!(reload_dmt = dm_task_create(DM_DEVICE_RELOAD)))
		goto out;
	if (!dm_task_set_name(reload_dmt, name))
		goto out_reload;

	do {
		next = dm_get_next_target(dmt, next, &start, &length,
					  &target, &params);
		memset(buff, 0, PARAMS_SIZE);
		strcpy(buff, params);
		if (strcmp(target, TGT_MPATH) && strstr(params, old)) {
			condlog(3, "%s: replace target %s %s",
				name, target, buff);
			dm_reassign_deps(buff, old, new);
			condlog(3, "%s: with target %s %s",
				name, target, buff);
			modified++;
		}
		dm_task_add_target(reload_dmt, start, length, target, buff);
	} while (next);

	if (modified) {
		dm_task_no_open_count(reload_dmt);

		if (!dm_task_run(reload_dmt)) {
			condlog(3, "%s: failed to reassign targets", name);
			goto out_reload;
		}
		dm_simplecmd_noflush(DM_DEVICE_RESUME, name);
	}
	r = 1;

out_reload:
	dm_task_destroy(reload_dmt);
out:
	dm_task_destroy(dmt);
	return r;
}


/*
 * Reassign existing device-mapper table(s) to not use
 * the block devices but point to the multipathed
 * device instead
 */
int dm_reassign(const char *mapname)
{
	struct dm_deps *deps;
	struct dm_task *dmt;
	struct dm_info info;
	struct dm_names *names;
	char **dm_deps = NULL;
	unsigned next = 0;
	char dev_t[32];
	int r = 0, i;

	if (!(dmt = dm_task_create(DM_DEVICE_DEPS)))
		return 0;

	if (!dm_task_set_name(dmt, mapname))
		goto out;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!dm_task_get_info(dmt, &info))
		goto out;

	if (!(deps = dm_task_get_deps(dmt)))
		goto out;

	if (!info.exists)
		goto out;

	dm_deps = MALLOC((deps->count + 1) * sizeof(char *));
	for (i = 0; i < deps->count; i++) {
		dm_deps[i] = MALLOC(32);
		sprintf(dm_deps[i], "%d:%d",
			major(deps->device[i]),
			minor(deps->device[i]));
		condlog(3, "%s: found dep %s", mapname, dm_deps[i]);
	}
	dm_deps[i] = NULL;

	dm_task_destroy (dmt);

	if (dm_dev_t(mapname, &dev_t[0], 32))
		goto out;

	/* Fetch all maps */
	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 1;

	dm_task_no_open_count(dmt);

	if (!dm_task_run(dmt))
		goto out;

	if (!(names = dm_task_get_names(dmt)))
		goto out;

	if (!names->dev) {
		r = 0; /* this is perfectly valid */
		goto out;
	}

	do {
		/* Skip this map and any partitions on it */
		if (!strncmp(names->name, mapname, strlen(mapname)))
			goto next_name;
		i = 0;
		while (dm_deps && dm_deps[i]) {
			dm_reassign_table(names->name, dm_deps[i], dev_t);
			i++;
		}
	next_name:
		next = names->next;
		names = (void *) names + next;
	} while (next);

	for (i = 0; dm_deps && dm_deps[i] != NULL; i++)
		FREE(dm_deps[i]);
	FREE(dm_deps);

	r = 0;
out:
	dm_task_destroy (dmt);
	return r;
}
