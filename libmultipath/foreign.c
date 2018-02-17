/*
  Copyright (c) 2018 Martin Wilck, SUSE Linux GmbH

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
  USA.
*/

#include <sys/sysmacros.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <glob.h>
#include <dlfcn.h>
#include <libudev.h>
#include "vector.h"
#include "debug.h"
#include "util.h"
#include "foreign.h"
#include "structs.h"
#include "structs_vec.h"
#include "print.h"

static vector foreigns;

#define get_dlsym(foreign, sym, lbl)					\
	do {								\
		foreign->sym =	dlsym(foreign->handle, #sym);		\
		if (foreign->sym == NULL) {				\
			condlog(0, "%s: symbol \"%s\" not found in \"%s\"", \
				__func__, #sym, foreign->name);		\
			goto lbl;					\
		}							\
	} while(0)

static void free_foreign(struct foreign **fgn)
{
	if (fgn == NULL || *fgn == NULL)
		return;
	if ((*fgn)->context != NULL)
		(*fgn)->cleanup((*fgn)->context);
	if ((*fgn)->handle != NULL)
		dlclose((*fgn)->handle);
	free(*fgn);
}

void cleanup_foreign(void)
{
	struct foreign *fgn;
	int i;

	vector_foreach_slot(foreigns, fgn, i)
		free_foreign(&fgn);
	vector_free(foreigns);
	foreigns = NULL;
}

int init_foreign(const char *multipath_dir)
{
	char pathbuf[PATH_MAX];
	static const char base[] = "libforeign-";
	static const char suffix[] = ".so";
	glob_t globbuf;
	int ret = -EINVAL, r, i;

	if (foreigns != NULL) {
		condlog(0, "%s: already initialized", __func__);
		return -EEXIST;
	}
	foreigns = vector_alloc();

	if (snprintf(pathbuf, sizeof(pathbuf), "%s/%s*%s",
		     multipath_dir, base, suffix) >= sizeof(pathbuf)) {
		condlog(1, "%s: path length overflow", __func__);
		goto err;
	}

	condlog(4, "%s: looking for %s\n", __func__, pathbuf);
	memset(&globbuf, 0, sizeof(globbuf));
	r = glob(pathbuf, 0, NULL, &globbuf);

	if (r == GLOB_NOMATCH) {
		condlog(3, "%s: no foreign multipath libraries found",
			__func__);
		globfree(&globbuf);
		return 0;
	} else if (r != 0) {
		char *msg;

		if (errno != 0) {
			ret = -errno;
			msg = strerror(errno);
		} else {
			ret = -1;
			msg = (r == GLOB_ABORTED ? "read error" :
			       "out of memory");
		}
		condlog(0, "%s: search for foreign libraries failed: %d (%s)",
			__func__, r, msg);
		globfree(&globbuf);
		goto err;
	}

	for (i = 0; i < globbuf.gl_pathc; i++) {
		char *msg, *fn;
		struct foreign *fgn;
		int len, namesz;

		fn = strrchr(globbuf.gl_pathv[i], '/');
		if (fn == NULL)
			fn = globbuf.gl_pathv[i];
		else
			fn++;

		len = strlen(fn);
		if (len <= sizeof(base) + sizeof(suffix) - 2) {
			condlog(0, "%s: internal error: filename too short: %s",
				__func__, globbuf.gl_pathv[i]);
			continue;
		}

		condlog(4, "%s: found %s", __func__, fn);

		namesz = len + 3 - sizeof(base) - sizeof(suffix);
		fgn = malloc(sizeof(*fgn) + namesz);
		if (fgn == NULL)
			continue;
		memset(fgn, 0, sizeof(*fgn));

		strlcpy((char*)fgn + sizeof(*fgn), fn + sizeof(base) - 1,
			namesz);
		fgn->name = (const char*)fgn + sizeof(*fgn);

		fgn->handle = dlopen(globbuf.gl_pathv[i], RTLD_NOW|RTLD_LOCAL);
		msg = dlerror();
		if (fgn->handle == NULL) {
			condlog(1, "%s: failed to open %s: %s", __func__,
				fn, msg);
			free_foreign(&fgn);
			continue;
		}

		get_dlsym(fgn, init, dl_err);
		get_dlsym(fgn, cleanup, dl_err);
		get_dlsym(fgn, add, dl_err);
		get_dlsym(fgn, change, dl_err);
		get_dlsym(fgn, delete, dl_err);
		get_dlsym(fgn, delete_all, dl_err);
		get_dlsym(fgn, check, dl_err);
		get_dlsym(fgn, lock, dl_err);
		get_dlsym(fgn, unlock, dl_err);
		get_dlsym(fgn, get_multipaths, dl_err);
		get_dlsym(fgn, release_multipaths, dl_err);
		get_dlsym(fgn, get_paths, dl_err);
		get_dlsym(fgn, release_paths, dl_err);

		fgn->context = fgn->init(LIBMP_FOREIGN_API, fgn->name);
		if (fgn->context == NULL) {
			condlog(0, "%s: init() failed for %s", __func__, fn);
			free_foreign(&fgn);
			continue;
		}

		if (vector_alloc_slot(foreigns) == NULL) {
			free_foreign(&fgn);
			continue;
		}
		vector_set_slot(foreigns, fgn);
		condlog(3, "foreign library \"%s\" loaded successfully", fgn->name);

		continue;

	dl_err:
		free_foreign(&fgn);
	}
	globfree(&globbuf);

	return 0;
err:
	cleanup_foreign();
	return ret;
}

int add_foreign(struct udev_device *udev)
{
	struct foreign *fgn;
	dev_t dt;
	int j;

	if (udev == NULL) {
		condlog(1, "%s called with NULL udev", __func__);
		return FOREIGN_ERR;
	}
	dt = udev_device_get_devnum(udev);
	vector_foreach_slot(foreigns, fgn, j) {
		int r = fgn->add(fgn->context, udev);

		if (r == FOREIGN_CLAIMED) {
			condlog(3, "%s: foreign \"%s\" claims device %d:%d",
				__func__, fgn->name, major(dt), minor(dt));
			return r;
		} else if (r == FOREIGN_OK) {
			condlog(4, "%s: foreign \"%s\" owns device %d:%d",
				__func__, fgn->name, major(dt), minor(dt));
			return r;
		} else if (r != FOREIGN_IGNORED) {
			condlog(1, "%s: unexpected return value %d from \"%s\"",
				__func__, r, fgn->name);
		}
	}
	return FOREIGN_IGNORED;
}

int change_foreign(struct udev_device *udev)
{
	struct foreign *fgn;
	int j;
	dev_t dt;

	if (udev == NULL) {
		condlog(1, "%s called with NULL udev", __func__);
		return FOREIGN_ERR;
	}
	dt = udev_device_get_devnum(udev);
	vector_foreach_slot(foreigns, fgn, j) {
		int r = fgn->change(fgn->context, udev);

		if (r == FOREIGN_OK) {
			condlog(4, "%s: foreign \"%s\" completed %d:%d",
				__func__, fgn->name, major(dt), minor(dt));
			return r;
		} else if (r != FOREIGN_IGNORED) {
			condlog(1, "%s: unexpected return value %d from \"%s\"",
				__func__, r, fgn->name);
		}
	}
	return FOREIGN_IGNORED;
}

int delete_foreign(struct udev_device *udev)
{
	struct foreign *fgn;
	int j;
	dev_t dt;

	if (udev == NULL) {
		condlog(1, "%s called with NULL udev", __func__);
		return FOREIGN_ERR;
	}
	dt = udev_device_get_devnum(udev);
	vector_foreach_slot(foreigns, fgn, j) {
		int r = fgn->delete(fgn->context, udev);

		if (r == FOREIGN_OK) {
			condlog(3, "%s: foreign \"%s\" deleted device %d:%d",
				__func__, fgn->name, major(dt), minor(dt));
			return r;
		} else if (r != FOREIGN_IGNORED) {
			condlog(1, "%s: unexpected return value %d from \"%s\"",
				__func__, r, fgn->name);
		}
	}
	return FOREIGN_IGNORED;
}

int delete_all_foreign(void)
{
	struct foreign *fgn;
	int j;

	vector_foreach_slot(foreigns, fgn, j) {
		int r;

		r = fgn->delete_all(fgn->context);
		if (r != FOREIGN_IGNORED && r != FOREIGN_OK) {
			condlog(1, "%s: unexpected return value %d from \"%s\"",
				__func__, r, fgn->name);
		}
	}
	return FOREIGN_OK;
}

void check_foreign(void)
{
	struct foreign *fgn;
	int j;

	vector_foreach_slot(foreigns, fgn, j) {
		fgn->check(fgn->context);
	}
}

/* Call this after get_path_layout */
void foreign_path_layout(void)
{
	struct foreign *fgn;
	int i;

	vector_foreach_slot(foreigns, fgn, i) {
		const struct _vector *vec;

		fgn->lock(fgn->context);
		vec = fgn->get_paths(fgn->context);
		if (vec != NULL) {
			_get_path_layout(vec, LAYOUT_RESET_NOT);
		}
		fgn->release_paths(fgn->context, vec);
		fgn->unlock(fgn->context);
	}
}

/* Call this after get_multipath_layout */
void foreign_multipath_layout(void)
{
	struct foreign *fgn;
	int i;

	vector_foreach_slot(foreigns, fgn, i) {
		const struct _vector *vec;

		fgn->lock(fgn->context);
		pthread_cleanup_push(fgn->unlock, fgn->context);
		vec = fgn->get_multipaths(fgn->context);
		if (vec != NULL) {
			_get_multipath_layout(vec, LAYOUT_RESET_NOT);
		}
		fgn->release_multipaths(fgn->context, vec);
		pthread_cleanup_pop(1);
	}
}

int snprint_foreign_topology(char *buf, int len, int verbosity)
{
	struct foreign *fgn;
	int i;
	char *c = buf;

	vector_foreach_slot(foreigns, fgn, i) {
		const struct _vector *vec;
		const struct gen_multipath *gm;
		int j;

		fgn->lock(fgn->context);
		pthread_cleanup_push(fgn->unlock, fgn->context);

		vec = fgn->get_multipaths(fgn->context);
		if (vec != NULL) {
			vector_foreach_slot(vec, gm, j) {

				c += _snprint_multipath_topology(gm, c,
								 buf + len - c,
								 verbosity);
				if (c >= buf + len - 1)
					break;
			}
			if (c >= buf + len - 1)
				break;
		}
		fgn->release_multipaths(fgn->context, vec);
		pthread_cleanup_pop(1);
	}

	return c - buf;
}

void print_foreign_topology(int verbosity)
{
	int buflen = MAX_LINE_LEN * MAX_LINES;
	char *buf = NULL, *tmp = NULL;

	buf = malloc(buflen);
	buf[0] = '\0';
	while (buf != NULL) {
		char *c = buf;

		c += snprint_foreign_topology(buf, buflen,
						   verbosity);
		if (c < buf + buflen - 1)
			break;

		buflen *= 2;
		tmp = buf;
		buf = realloc(buf, buflen);
	}

	if (buf == NULL && tmp != NULL)
		buf = tmp;

	if (buf != NULL) {
		printf("%s", buf);
		free(buf);
	}
}

int snprint_foreign_paths(char *buf, int len, const char *style, int pretty)
{
	struct foreign *fgn;
	int i;
	char *c = buf;

	vector_foreach_slot(foreigns, fgn, i) {
		const struct _vector *vec;
		const struct gen_path *gp;
		int j;

		fgn->lock(fgn->context);
		pthread_cleanup_push(fgn->unlock, fgn->context);

		vec = fgn->get_paths(fgn->context);
		if (vec != NULL) {
			vector_foreach_slot(vec, gp, j) {
				c += _snprint_path(gp, c, buf + len - c,
						   style, pretty);
				if (c >= buf + len - 1)
					break;
			}
			if (c >= buf + len - 1)
				break;
		}
		fgn->release_paths(fgn->context, vec);
		pthread_cleanup_pop(1);
	}

	return c - buf;
}

int snprint_foreign_multipaths(char *buf, int len,
			       const char *style, int pretty)
{
	struct foreign *fgn;
	int i;
	char *c = buf;

	vector_foreach_slot(foreigns, fgn, i) {
		const struct _vector *vec;
		const struct gen_multipath *gm;
		int j;

		fgn->lock(fgn->context);
		pthread_cleanup_push(fgn->unlock, fgn->context);

		vec = fgn->get_multipaths(fgn->context);
		if (vec != NULL) {
			vector_foreach_slot(vec, gm, j) {
				c += _snprint_multipath(gm, c, buf + len - c,
							style, pretty);
				if (c >= buf + len - 1)
					break;
			}
			if (c >= buf + len - 1)
				break;
		}
		fgn->release_multipaths(fgn->context, vec);
		pthread_cleanup_pop(1);
	}

	return c - buf;
}
