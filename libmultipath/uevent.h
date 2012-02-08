#ifndef _UEVENT_H
#define _UEVENT_H

/* environment buffer, the kernel's size in lib/kobject_uevent.c should fit in */
#define HOTPLUG_BUFFER_SIZE		1024
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
	char *envp[HOTPLUG_NUM_ENVP];
};

int uevent_listen(int (*store_uev)(struct uevent *, void * trigger_data),
		  void * trigger_data);
int uevent_get_major(struct uevent *uev);
int uevent_get_minor(struct uevent *uev);

#endif /* _UEVENT_H */
