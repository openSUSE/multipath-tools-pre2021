/*
 * sysfs.h
 */

#ifndef _LIBMULTIPATH_SYSFS_H
#define _LIBMULTIPATH_SYSFS_H

#include "list.h"

#define PATH_SIZE 512
#define NAME_SIZE 128

#define dbg(format, arg...) do {} while (0)

struct sysfs_device {
	struct list_head node;			/* for device cache */
	struct sysfs_device *parent;		/* already cached parent*/
	char devpath[PATH_SIZE];
	char subsystem[NAME_SIZE];		/* $class, $bus, drivers, module */
	char kernel[NAME_SIZE];			/* device instance name */
	char kernel_number[NAME_SIZE];
	char driver[NAME_SIZE];			/* device driver name */
};

int sysfs_init(char *path, size_t len);
void sysfs_cleanup(void);
void sysfs_device_set_values(struct sysfs_device *dev, const char *devpath,
			     const char *subsystem, const char *driver);
struct sysfs_device *sysfs_device_get(const char *devpath);
struct sysfs_device *sysfs_device_get_parent(struct sysfs_device *dev);
struct sysfs_device *sysfs_device_get_parent_with_subsystem(struct sysfs_device *dev, const char *subsystem);
char *sysfs_attr_get_value(const char *devpath, const char *attr_name);
int sysfs_resolve_link(char *path, size_t size);

#endif
