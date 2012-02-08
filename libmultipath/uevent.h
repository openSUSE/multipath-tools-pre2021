#ifndef _UEVENT_H
#define _UEVENT_H

/* environment buffer, the kernel's size in lib/kobject_uevent.c should fit in */
#define HOTPLUG_BUFFER_SIZE		2048
#define HOTPLUG_NUM_ENVP		32
#define OBJECT_SIZE			512

#ifndef NETLINK_KOBJECT_UEVENT
#define NETLINK_KOBJECT_UEVENT		15
#endif

struct uevent {
	struct list_head node;
	char buffer[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
	char *devpath;
	char *action;
	char *kernel;
	long seqnum;
	char *envp[HOTPLUG_NUM_ENVP];
};

void setup_thread_attr(pthread_attr_t *attr, size_t stacksize,
		       int detached);

int uevent_listen(void);
int uevent_dispatch(int (*store_uev)(struct uevent *, void * trigger_data),
		    void * trigger_data);
int uevent_get_major(struct uevent *uev);
int uevent_get_minor(struct uevent *uev);
int uevent_get_disk_ro(struct uevent *uev);
char *uevent_get_dm_name(struct uevent *uev);
char *uevent_get_env(struct uevent *uev, const char *env);
long sysfs_get_seqnum(void);
int uevent_wait_for_seqnum(long seqnum, unsigned int timeout);

#endif /* _UEVENT_H */
