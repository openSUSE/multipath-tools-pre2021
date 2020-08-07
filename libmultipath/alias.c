/*
 * Copyright (c) 2005 Christophe Varoqui
 * Copyright (c) 2005 Benjamin Marzinski, Redhat
 */
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "debug.h"
#include "uxsock.h"
#include "alias.h"
#include "file.h"
#include "vector.h"
#include "checkers.h"
#include "structs.h"
#include "config.h"
#include "util.h"
#include "errno.h"


/*
 * significant parts of this file were taken from iscsi-bindings.c of the
 * linux-iscsi project.
 * Copyright (C) 2002 Cisco Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * See the file COPYING included with this distribution for more details.
 */

int
valid_alias(char *alias)
{
	if (strchr(alias, '/') != NULL)
		return 0;
	return 1;
}


static int
format_devname(char *name, int id, int len, char *prefix)
{
	int pos;
	int prefix_len = strlen(prefix);

	memset(name,0, len);
	strcpy(name, prefix);
	for (pos = len - 1; pos >= prefix_len; pos--) {
		id--;
		name[pos] = 'a' + id % 26;
		if (id < 26)
			break;
		id /= 26;
	}
	memmove(name + prefix_len, name + pos, len - pos);
	name[prefix_len + len - pos] = '\0';
	return (prefix_len + len - pos);
}

static int
scan_devname(char *alias, char *prefix)
{
	char *c;
	int i, n = 0;

	if (!prefix || strncmp(alias, prefix, strlen(prefix)))
		return -1;

	if (strlen(alias) == strlen(prefix))
		return -1;

	if (strlen(alias) > strlen(prefix) + 7)
		/* id of 'aaaaaaaa' overflows int */
		return -1;

	c = alias + strlen(prefix);
	while (*c != '\0' && *c != ' ' && *c != '\t') {
		if (*c < 'a' || *c > 'z')
			return -1;
		i = *c - 'a';
		n = ( n * 26 ) + i;
		if (n < 0)
			return -1;
		c++;
		n++;
	}

	return n;
}

static int
lookup_binding(FILE *f, char *map_wwid, char **map_alias, char *prefix)
{
	char buf[LINE_MAX];
	unsigned int line_nr = 0;
	int id = 1;
	int biggest_id = 1;
	int smallest_bigger_id = INT_MAX;

	*map_alias = NULL;

	rewind(f);
	while (fgets(buf, LINE_MAX, f)) {
		char *c, *alias, *wwid;
		int curr_id;

		line_nr++;
		c = strpbrk(buf, "#\n\r");
		if (c)
			*c = '\0';
		alias = strtok(buf, " \t");
		if (!alias) /* blank line */
			continue;
		curr_id = scan_devname(alias, prefix);
		if (curr_id == id)
			id++;
		if (curr_id > biggest_id)
			biggest_id = curr_id;
		if (curr_id > id && curr_id < smallest_bigger_id)
			smallest_bigger_id = curr_id;
		wwid = strtok(NULL, " \t");
		if (!wwid){
			condlog(3,
				"Ignoring malformed line %u in bindings file",
				line_nr);
			continue;
		}
		if (strcmp(wwid, map_wwid) == 0){
			condlog(3, "Found matching wwid [%s] in bindings file."
				" Setting alias to %s", wwid, alias);
			*map_alias = strdup(alias);
			if (*map_alias == NULL)
				condlog(0, "Cannot copy alias from bindings "
					"file : %s", strerror(errno));
			return 0;
		}
	}
	condlog(3, "No matching wwid [%s] in bindings file.", map_wwid);
	if (id < 0) {
		condlog(0, "no more available user_friendly_names");
		return 0;
	}
	if (id < smallest_bigger_id)
		return id;
	return biggest_id + 1;
}

static int
rlookup_binding(FILE *f, char *buff, char *map_alias, char *prefix)
{
	char line[LINE_MAX];
	unsigned int line_nr = 0;

	buff[0] = '\0';

	while (fgets(line, LINE_MAX, f)) {
		char *c, *alias, *wwid;

		line_nr++;
		c = strpbrk(line, "#\n\r");
		if (c)
			*c = '\0';
		alias = strtok(line, " \t");
		if (!alias) /* blank line */
			continue;
		wwid = strtok(NULL, " \t");
		if (!wwid){
			condlog(3,
				"Ignoring malformed line %u in bindings file",
				line_nr);
			continue;
		}
		if (strlen(wwid) > WWID_SIZE - 1) {
			condlog(3,
				"Ignoring too large wwid at %u in bindings file", line_nr);
			continue;
		}
		if (strcmp(alias, map_alias) == 0){
			condlog(3, "Found matching alias [%s] in bindings file."
				"\nSetting wwid to %s", alias, wwid);
			strncpy(buff, wwid, WWID_SIZE);
			buff[WWID_SIZE - 1] = '\0';
			return 0;
		}
	}
	condlog(3, "No matching alias [%s] in bindings file.", map_alias);

	return -1;
}

static char *
allocate_binding(int fd, char *wwid, int id, char *prefix)
{
	char buf[LINE_MAX];
	off_t offset;
	char *alias, *c;
	int i;

	if (id < 0) {
		condlog(0, "Bindings file full. Cannot allocate new binding");
		return NULL;
	}

	i = format_devname(buf, id, LINE_MAX, prefix);
	c = buf + i;
	snprintf(c,LINE_MAX - i, " %s\n", wwid);
	buf[LINE_MAX - 1] = '\0';

	offset = lseek(fd, 0, SEEK_END);
	if (offset < 0){
		condlog(0, "Cannot seek to end of bindings file : %s",
			strerror(errno));
		return NULL;
	}
	if (write(fd, buf, strlen(buf)) != strlen(buf)){
		condlog(0, "Cannot write binding to bindings file : %s",
			strerror(errno));
		/* clear partial write */
		if (ftruncate(fd, offset))
			condlog(0, "Cannot truncate the header : %s",
				strerror(errno));
		return NULL;
	}
	c = strchr(buf, ' ');
	if (c)
		*c = '\0';
	alias = strdup(buf);
	if (alias == NULL)
		condlog(0, "cannot copy new alias from bindings file : %s",
			strerror(errno));
	else
		condlog(3, "Created new binding [%s] for WWID [%s]", alias,
			wwid);
	return alias;
}

char *
use_existing_alias (char *wwid, char *file, char *alias_old,
		char *prefix, int bindings_read_only)
{
	char *alias = NULL;
	int id = 0;
	int fd, can_write;
	char buff[WWID_SIZE];
	FILE *f;

	fd = open_file(file, &can_write, BINDINGS_FILE_HEADER);
	if (fd < 0)
		return NULL;

	f = fdopen(fd, "r");
	if (!f) {
		condlog(0, "cannot fdopen on bindings file descriptor");
		close(fd);
		return NULL;
	}
	/* lookup the binding. if it exsists, the wwid will be in buff
	 * either way, id contains the id for the alias
	 */
	rlookup_binding(f, buff, alias_old, prefix);

	if (strlen(buff) > 0) {
		/* if buff is our wwid, it's already
		 * allocated correctly
		 */
		if (strcmp(buff, wwid) == 0)
			alias = STRDUP(alias_old);
		else {
			alias = NULL;
			condlog(0, "alias %s already bound to wwid %s, cannot reuse",
				alias_old, buff);
		}
		goto out;
	}

	id = lookup_binding(f, wwid, &alias, NULL);
	if (alias) {
		condlog(3, "Use existing binding [%s] for WWID [%s]",
			alias, wwid);
		goto out;
	}

	/* allocate the existing alias in the bindings file */
	id = scan_devname(alias_old, prefix);
	if (id <= 0)
		goto out;

	if (fflush(f) != 0) {
		condlog(0, "cannot fflush bindings file stream : %s",
			strerror(errno));
		goto out;
	}

	if (can_write && !bindings_read_only) {
		alias = allocate_binding(fd, wwid, id, prefix);
		condlog(0, "Allocated existing binding [%s] for WWID [%s]",
			alias, wwid);
	}

out:
	fclose(f);
	return alias;
}

char *
get_user_friendly_alias(char *wwid, char *file, char *prefix,
			int bindings_read_only)
{
	char *alias;
	int fd, id;
	FILE *f;
	int can_write;

	if (!wwid || *wwid == '\0') {
		condlog(3, "Cannot find binding for empty WWID");
		return NULL;
	}

	fd = open_file(file, &can_write, BINDINGS_FILE_HEADER);
	if (fd < 0)
		return NULL;

	f = fdopen(fd, "r");
	if (!f) {
		condlog(0, "cannot fdopen on bindings file descriptor : %s",
			strerror(errno));
		close(fd);
		return NULL;
	}

	id = lookup_binding(f, wwid, &alias, prefix);
	if (id < 0) {
		fclose(f);
		return NULL;
	}

	if (fflush(f) != 0) {
		condlog(0, "cannot fflush bindings file stream : %s",
			strerror(errno));
		free(alias);
		fclose(f);
		return NULL;
	}

	if (!alias && can_write && !bindings_read_only && id)
		alias = allocate_binding(fd, wwid, id, prefix);

	fclose(f);
	return alias;
}

int
get_user_friendly_wwid(char *alias, char *buff, char *file)
{
	int fd, unused;
	FILE *f;

	if (!alias || *alias == '\0') {
		condlog(3, "Cannot find binding for empty alias");
		return -1;
	}

	fd = open_file(file, &unused, BINDINGS_FILE_HEADER);
	if (fd < 0)
		return -1;

	f = fdopen(fd, "r");
	if (!f) {
		condlog(0, "cannot fdopen on bindings file descriptor : %s",
			strerror(errno));
		close(fd);
		return -1;
	}

	rlookup_binding(f, buff, alias, NULL);
	if (!strlen(buff)) {
		fclose(f);
		return -1;
	}

	fclose(f);
	return 0;
}

struct binding {
	char *alias;
	char *wwid;
};

static void _free_binding(struct binding *bdg)
{
	free(bdg->wwid);
	free(bdg->alias);
	free(bdg);
}

/*
 * Perhaps one day we'll implement this more efficiently, thus use
 * an abstract type.
 */
typedef struct _vector Bindings;

static void free_bindings(Bindings *bindings)
{
	struct binding *bdg;
	int i;

	vector_foreach_slot(bindings, bdg, i)
		_free_binding(bdg);
	vector_reset(bindings);
}

enum {
	BINDING_EXISTS,
	BINDING_CONFLICT,
	BINDING_ADDED,
	BINDING_DELETED,
	BINDING_NOTFOUND,
	BINDING_ERROR,
};

static int add_binding(Bindings *bindings, const char *alias, const char *wwid)
{
	struct binding *bdg;
	int i, cmp = 0;

	/* Keep the bindings array sorted by alias */
	vector_foreach_slot(bindings, bdg, i) {
		if ((cmp = strcmp(bdg->alias, alias)) >= 0)
			break;
	}

	/* Check for exact match */
	if (i < VECTOR_SIZE(bindings) && cmp == 0)
		return strcmp(bdg->wwid, wwid) ?
			BINDING_CONFLICT : BINDING_EXISTS;

	bdg = calloc(1, sizeof(*bdg));
	if (bdg) {
		bdg->wwid = strdup(wwid);
		bdg->alias = strdup(alias);
		if (bdg->wwid && bdg->alias &&
		    vector_insert_slot(bindings, i, bdg))
			return BINDING_ADDED;
	}

	_free_binding(bdg);
	return BINDING_ERROR;
}

static int write_bindings_file(const Bindings *bindings, int fd)
{
	struct binding *bnd;
	char line[LINE_MAX];
	int i;

	if (write(fd, BINDINGS_FILE_HEADER, sizeof(BINDINGS_FILE_HEADER) - 1)
	    != sizeof(BINDINGS_FILE_HEADER) - 1)
		return -1;

	vector_foreach_slot(bindings, bnd, i) {
		int len;

		len = snprintf(line, sizeof(line), "%s %s\n",
			       bnd->alias, bnd->wwid);

		if (len >= sizeof(line)) {
			condlog(1, "%s: line overflow", __func__);
			return -1;
		}

		if (write(fd, line, len) != len)
			return -1;
	}
	return 0;
}

static int fix_bindings_file(const struct config *conf,
			     const Bindings *bindings)
{
	int rc;
	long fd;
	char tempname[PATH_MAX];

	if (safe_sprintf(tempname, "%s.XXXXXX", conf->bindings_file))
		return -1;
	if ((fd = mkstemp(tempname)) == -1) {
		condlog(1, "%s: mkstemp: %m", __func__);
		return -1;
	}
	pthread_cleanup_push(close_fd, (void*)fd);
	rc = write_bindings_file(bindings, fd);
	pthread_cleanup_pop(1);
	if (rc == -1) {
		condlog(1, "failed to write new bindings file %s",
			tempname);
		unlink(tempname);
		return rc;
	}
	if ((rc = rename(tempname, conf->bindings_file)) == -1)
		condlog(0, "%s: rename: %m", __func__);
	else
		condlog(1, "updated bindings file %s", conf->bindings_file);
	return rc;
}

static int _check_bindings_file(const struct config *conf, FILE *file,
				 Bindings *bindings)
{
	int rc = 0;
	unsigned int linenr = 0;
	char *line = NULL;
	size_t line_len = 0;
	ssize_t n;

	pthread_cleanup_push(free, line);
	while ((n = getline(&line, &line_len, file)) >= 0) {
		char *c, *alias, *wwid, *mpe_wwid;

		linenr++;
		c = strpbrk(line, "#\n\r");
		if (c)
			*c = '\0';
		alias = strtok(line, " \t");
		if (!alias) /* blank line */
			continue;
		wwid = strtok(NULL, " \t");
		if (!wwid) {
			condlog(1, "invalid line %d in bindings file, missing WWID",
				linenr);
			continue;
		}
		c = strtok(NULL, " \t");
		if (c)
			/* This is non-fatal */
			condlog(1, "invalid line %d in bindings file, extra args \"%s\"",
				linenr, c);

		mpe_wwid = get_mpe_wwid(conf->mptable, alias);
		if (mpe_wwid && strcmp(mpe_wwid, wwid)) {
			condlog(0, "ERROR: alias \"%s\" for WWID %s in bindings file "
				"on line %u conflicts with multipath.conf entry for %s",
				alias, wwid, linenr, mpe_wwid);
			rc = -1;
			continue;
		}

		switch (add_binding(bindings, alias, wwid)) {
		case BINDING_CONFLICT:
			condlog(0, "ERROR: multiple bindings for alias \"%s\" in "
				"bindings file on line %u, discarding binding to WWID %s",
				alias, linenr, wwid);
			rc = -1;
			break;
		case BINDING_EXISTS:
			condlog(2, "duplicate line for alias %s in bindings file on line %u",
				alias, linenr);
			break;
		case BINDING_ERROR:
			condlog(2, "error adding binding %s -> %s",
				alias, wwid);
			break;
		default:
			break;
		}
	}
	pthread_cleanup_pop(1);
	return rc;
}

/*
 * check_alias_settings(): test for inconsistent alias configuration
 *
 * It's a fatal configuration error if the same alias is assigned to
 * multiple WWIDs. In the worst case, it can cause data corruption
 * by mangling devices with different WWIDs into the same multipath map.
 * This function tests the configuration from multipath.conf and the
 * bindings file for consistency, drops inconsistent multipath.conf
 * alias settings, and rewrites the bindings file if necessary, dropping
 * conflicting lines (if user_friendly_names is on, multipathd will
 * fill in the deleted lines with a newly generated alias later).
 * Note that multipath.conf is not rewritten. Use "multipath -T" for that.
 *
 * Returns: 0 in case of success, -1 if the configuration was bad
 * and couldn't be fixed.
 */
int check_alias_settings(const struct config *conf)
{
	FILE *file;
	int can_write;
	int rc = 0, i;
	Bindings bindings = {.allocated = 0, };
	struct mpentry *mpe;

	pthread_cleanup_push_cast(free_bindings, &bindings);
	vector_foreach_slot(conf->mptable, mpe, i) {
		if (!mpe->wwid || !mpe->alias)
			continue;
		if (add_binding(&bindings, mpe->alias, mpe->wwid) ==
		    BINDING_CONFLICT) {
			condlog(0, "ERROR: alias \"%s\" bound to multiple wwids in multipath.conf, "
				"discarding binding to %s",
				mpe->alias, mpe->wwid);
			free(mpe->alias);
			mpe->alias = NULL;
		}
	}
	/* This clears the bindings */
	pthread_cleanup_pop(1);

	pthread_cleanup_push_cast(free_bindings, &bindings);
	file = fdopen(open_file(conf->bindings_file, &can_write,
				BINDINGS_FILE_HEADER), "r");
	if (file == NULL) {
		if (errno != ENOENT)
			condlog(1, "failed to fdopen %s: %m",
				conf->bindings_file);
	} else {
		pthread_cleanup_push_cast(fclose, file);
		rc = _check_bindings_file(conf, file, &bindings);
		pthread_cleanup_pop(1);
		if (rc == -1 && can_write && !conf->bindings_read_only)
			rc = fix_bindings_file(conf, &bindings);
		else if (rc == -1)
			condlog(0, "ERROR: bad settings in read-only in bindings file %s",
				conf->bindings_file);
	}
	pthread_cleanup_pop(1);
	return rc;
}
