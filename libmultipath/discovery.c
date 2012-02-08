/*
 * Copyright (c) 2004, 2005, 2006 Christophe Varoqui
 * Copyright (c) 2005 Stefan Bader, IBM
 * Copyright (c) 2005 Mike Anderson
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <checkers.h>
#include <libprio.h>

#include "vector.h"
#include "memory.h"
#include "util.h"
#include "structs.h"
#include "config.h"
#include "blacklist.h"
#include "callout.h"
#include "debug.h"
#include "propsel.h"
#include "sg_include.h"
#include "sysfs.h"
#include "discovery.h"

struct path *
store_pathinfo (vector pathvec, vector hwtable, char * devname, int flag)
{
	struct path * pp;

	pp = alloc_path();

	if (!pp)
		return NULL;

	if(safe_sprintf(pp->dev, "%s", devname)) {
		condlog(0, "pp->dev too small");
		goto out;
	}
	if (pathinfo(pp, hwtable, flag))
		goto out;

	if (store_path(pathvec, pp))
		goto out;

	return pp;
out:
	free_path(pp);
	return NULL;
}

static int
path_discover (vector pathvec, struct config * conf, char * devname, int flag)
{
	char path[FILE_NAME_SIZE];
	struct path * pp;

	if (!devname)
		return 0;

	if (filter_devnode(conf->blist_devnode, conf->elist_devnode,
			   devname) > 0)
		return 0;

	if(safe_sprintf(path, "%s/block/%s/device", sysfs_path,
			devname)) {
		condlog(0, "path too small");
		return 1;
	}

	if (strncmp(devname,"cciss",5) && !filepresent(path)) {
		condlog(4, "path %s not present", path);
		return 0;
	}

	pp = find_path_by_dev(pathvec, devname);

	if (!pp) {
		pp = store_pathinfo(pathvec, conf->hwtable,
				    devname, flag);
		return (pp ? 0 : 1);
	}
	return pathinfo(pp, conf->hwtable, flag);
}

int
path_discovery (vector pathvec, struct config * conf, int flag)
{
	DIR *blkdir;
	struct dirent *blkdev;
	struct stat statbuf;
	char devpath[PATH_MAX];
	char *devptr;
	int r = 0;

	if (!(blkdir = opendir("/sys/block")))
		return 1;

	strcpy(devpath,"/sys/block");
	while ((blkdev = readdir(blkdir)) != NULL) {
		if ((strcmp(blkdev->d_name,".") == 0) ||
		    (strcmp(blkdev->d_name,"..") == 0))
			continue;

		devptr = devpath + 10;
		*devptr = '\0';
		strcat(devptr,"/");
		strcat(devptr,blkdev->d_name);
		if (stat(devpath, &statbuf) < 0)
			continue;

		if (S_ISDIR(statbuf.st_mode) == 0)
			continue;

		condlog(4, "Discover device %s", devpath);

		r += path_discover(pathvec, conf, blkdev->d_name, flag);
	}
	closedir(blkdir);
	condlog(4, "Discovery status %d", r);
	return r;
}


/*
 * the daemon can race udev upon path add,
 * not multipath(8), ran by udev
 */
#if DAEMON
#define WAIT_MAX_SECONDS 5
#define WAIT_LOOP_PER_SECOND 5

static int
wait_for_file (char * filename)
{
	int loop;
	struct stat stats;
	
	loop = WAIT_MAX_SECONDS * WAIT_LOOP_PER_SECOND;
	
	while (--loop) {
		if (stat(filename, &stats) == 0)
			return 0;

		if (errno != ENOENT)
			return 1;

		usleep(1000 * 1000 / WAIT_LOOP_PER_SECOND);
	}
	return 1;
}
#else
static int
wait_for_file (char * filename)
{
	return 0;
}
#endif

#define declare_sysfs_get_str(fname) \
extern int \
sysfs_get_##fname (struct sysfs_device * dev, char * buff, size_t len) \
{ \
	char *attr; \
	char attr_path[SYSFS_PATH_SIZE]; \
\
	if (safe_sprintf(attr_path, "%s/%s/%s", sysfs_path, dev->devpath, #fname)) \
		return 1; \
\
	if (wait_for_file(attr_path)) \
		return 1; \
\
	attr = sysfs_attr_get_value(dev->devpath, #fname); \
	if (!attr) \
		return 1; \
	if (strlcpy(buff, attr, len) != strlen(attr)) \
		return 2; \
	strchop(buff); \
	return 0; \
}

declare_sysfs_get_str(devtype);
declare_sysfs_get_str(cutype);
declare_sysfs_get_str(vendor);
declare_sysfs_get_str(model);
declare_sysfs_get_str(rev);

int
sysfs_get_dev (struct sysfs_device * dev, char * buff, size_t len)
{
	char *attr;

	attr = sysfs_attr_get_value(dev->devpath, "dev");
	if (!attr) {
		condlog(3, "%s: no 'dev' attribute in sysfs", dev->kernel);
		return 1;
	}
	if (strlcpy(buff, attr, len) != strlen(attr)) {
		condlog(3, "%s: overflow in 'dev' attribute", dev->kernel);
		return 2;
	}
	return 0;
}

int
sysfs_get_size (struct sysfs_device * dev, unsigned long long * size)
{
	char *attr;
	int r;

	attr = sysfs_attr_get_value(dev->devpath, "size");
	if (!attr)
		return 1;

	r = sscanf(attr, "%llu\n", size);

	if (r != 1)
		return 1;

	return 0;
}
	
int
sysfs_get_fc_nodename (struct sysfs_device * dev, char * node,
		       unsigned int host, unsigned int channel,
		       unsigned int target)
{
	char attr_path[SYSFS_PATH_SIZE], *attr;

	if (safe_sprintf(attr_path, 
			 "/class/fc_transport/target%i:%i:%i",
			 host, channel, target)) {
		condlog(0, "attr_path too small");
		return 1;
	}

	attr = sysfs_attr_get_value(attr_path, "node_name");
	if (attr) {
		strlcpy(node, attr, strlen(attr));
		return 0;
	}

	return 1;
}
	
/*
 * udev might be slow creating node files : wait
 */
static int
opennode (char * dev, int mode)
{
	char devpath[FILE_NAME_SIZE], *ptr;

	if (safe_sprintf(devpath, "%s/%s", conf->udev_dir, dev)) {
		condlog(0, "devpath too small");
		return -1;
	}
	/*
	 * Translate '!' into '/'
	 */
	ptr = devpath;
	while ((ptr = strchr(ptr, '!'))) {
		*ptr = '/';
		ptr++;
	}
	return open(devpath, mode);
}

extern int
devt2devname (char *devname, int devname_len, char *devt)
{
	FILE *fd;
	unsigned int tmpmaj, tmpmin, major, minor;
	char dev[FILE_NAME_SIZE];
	char block_path[FILE_NAME_SIZE];
	struct stat statbuf;

	if (sscanf(devt, "%u:%u", &major, &minor) != 2) {
		condlog(0, "Invalid device number %s", devt);
		return 1;
	}

	if (devname_len > sizeof(dev))
		devname_len = sizeof(dev);

	if ((fd = fopen("/proc/partitions", "r")) < 0) {
		condlog(0, "Cannot open /proc/partitions");
		return 1;
	}

	while (!feof(fd)) {
		int r = fscanf(fd,"%u %u %*d %s",&tmpmaj, &tmpmin, dev);
		if (!r) {
			r = fscanf(fd,"%*s\n");
			continue;
		}
		if (r != 3)
			continue;

		if ((major == tmpmaj) && (minor == tmpmin)) {
			if (snprintf(block_path, sizeof(block_path),
				     "/sys/block/%s", dev) >= sizeof(block_path)) {
				condlog(0, "device name %s is too long",dev);
				fclose(fd);
				return 1;
			}
			break;
		}
	}
	fclose(fd);

	if (strncmp(block_path,"/sys/block", 10)) {
		condlog(3, "No device found for %u:%u", major, minor);
		return 1;
	}

	if (stat(block_path, &statbuf) < 0) {
		condlog(0, "No sysfs entry for %s", block_path);
		return 1;
	}

	if (S_ISDIR(statbuf.st_mode) == 0) {
		condlog(0, "sysfs entry %s is not a directory", block_path);
		return 1;
	}
	strncpy(devname, dev, devname_len);

	return 0;
}

int
do_inq(int sg_fd, int cmddt, int evpd, unsigned int pg_op,
       void *resp, int mx_resp_len)
{
	unsigned char inqCmdBlk[INQUIRY_CMDLEN] =
		{ INQUIRY_CMD, 0, 0, 0, 0, 0 };
	unsigned char sense_b[SENSE_BUFF_LEN];
	struct sg_io_hdr io_hdr;

	if (cmddt)
		inqCmdBlk[1] |= 2;
	if (evpd)
		inqCmdBlk[1] |= 1;
	inqCmdBlk[2] = (unsigned char) pg_op;
	inqCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
	inqCmdBlk[4] = (unsigned char) (mx_resp_len & 0xff);
	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (inqCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_b);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = mx_resp_len;
	io_hdr.dxferp = resp;
	io_hdr.cmdp = inqCmdBlk;
	io_hdr.sbp = sense_b;
	io_hdr.timeout = DEF_TIMEOUT;

	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0)
		return -1;

	/* treat SG_ERR here to get rid of sg_err.[ch] */
	io_hdr.status &= 0x7e;
	if ((0 == io_hdr.status) && (0 == io_hdr.host_status) &&
	    (0 == io_hdr.driver_status))
		return 0;
	if ((SCSI_CHECK_CONDITION == io_hdr.status) ||
	    (SCSI_COMMAND_TERMINATED == io_hdr.status) ||
	    (SG_ERR_DRIVER_SENSE == (0xf & io_hdr.driver_status))) {
		if (io_hdr.sbp && (io_hdr.sb_len_wr > 2)) {
			int sense_key;
			unsigned char * sense_buffer = io_hdr.sbp;
			if (sense_buffer[0] & 0x2)
				sense_key = sense_buffer[1] & 0xf;
			else
				sense_key = sense_buffer[2] & 0xf;
			if(RECOVERED_ERROR == sense_key)
				return 0;
		}
	}
	return -1;
}

static int
get_serial (char * str, int maxlen, int fd)
{
	int len = 0;
	char buff[MX_ALLOC_LEN + 1] = {0};

	if (fd < 0)
		return 1;

	if (0 == do_inq(fd, 0, 1, 0x80, buff, MX_ALLOC_LEN)) {
		len = buff[3];
		if (len >= maxlen)
			return 1;
		if (len > 0) {
			memcpy(str, buff + 4, len);
			str[len] = '\0';
		}
		return 0;
	}
	return 1;
}

static int
get_inq (char *dev, char * vendor, char * product, char * rev, int fd)
{
	unsigned char buff[MX_ALLOC_LEN + 1] = {0};
	int len = 0;

	if (fd < 0)
		return 1;

	memset(buff, 0, MX_ALLOC_LEN);
	if (0 != do_inq(fd, 0, 0, 0, buff, 36))
		return 1;

	/* Check peripheral qualifier */
	if ((buff[0] >> 5) != 0) {
		int pqual = (buff[0] >> 5);
		switch (pqual) {
		case 1:
			condlog(3, "%s: INQUIRY failed, LU not connected", dev);
			break;
		case 3:
			condlog(3, "%s: INQUIRY failed, LU not supported", dev);
			break;
		default:
			condlog(3, "%s: INQUIRY failed, Invalid PQ %x", dev, pqual);
			break;
		}

		return 1;
	}

	len = buff[4] + 4;

	if (len < 8) {
		condlog(3, "%s: INQUIRY response too short (len %d)",
			dev, len);
		return 1;
	}

	memcpy(vendor, buff + 8, 8);
	vendor[8] = '\0';
	strchop(vendor);
	memcpy(product, buff + 16, 16);
	product[16] = '\0';
	strchop(product);
	memcpy(rev, buff + 32, 4);
	rev[4] = '\0';
	strchop(rev);
	return 0;
}

static int
scsi_sysfs_pathinfo (struct path * pp, struct sysfs_device * parent)
{
	char attr_path[FILE_NAME_SIZE];

	if (sysfs_get_vendor(parent, pp->vendor_id, SCSI_VENDOR_SIZE))
		return 1;

	condlog(3, "%s: vendor = %s", pp->dev, pp->vendor_id);

	if (sysfs_get_model(parent, pp->product_id, SCSI_PRODUCT_SIZE))
		return 1;

	condlog(3, "%s: product = %s", pp->dev, pp->product_id);

	if (sysfs_get_rev(parent, pp->rev, SCSI_REV_SIZE))
		return 1;

	condlog(3, "%s: rev = %s", pp->dev, pp->rev);

	/*
	 * set the hwe configlet pointer
	 */
	pp->hwe = find_hwe(conf->hwtable, pp->vendor_id, pp->product_id, pp->rev);

	/*
	 * host / bus / target / lun
	 */
	basenamecpy(parent->devpath, attr_path, FILE_NAME_SIZE);

	sscanf(attr_path, "%i:%i:%i:%i",
			&pp->sg_id.host_no,
			&pp->sg_id.channel,
			&pp->sg_id.scsi_id,
			&pp->sg_id.lun);
	condlog(3, "%s: h:b:t:l = %i:%i:%i:%i",
			pp->dev,
			pp->sg_id.host_no,
			pp->sg_id.channel,
			pp->sg_id.scsi_id,
			pp->sg_id.lun);

	/*
	 * target node name
	 */
	if(!sysfs_get_fc_nodename(parent, pp->tgt_node_name,
				 pp->sg_id.host_no,
				 pp->sg_id.channel,
				 pp->sg_id.scsi_id)) {
		condlog(3, "%s: tgt_node_name = %s",
			pp->dev, pp->tgt_node_name);
	}

	return 0;
}

static int
ccw_sysfs_pathinfo (struct path * pp, struct sysfs_device * parent)
{
	char attr_path[FILE_NAME_SIZE];
	char attr_buff[FILE_NAME_SIZE];

	sprintf(pp->vendor_id, "IBM");

	condlog(3, "%s: vendor = %s", pp->dev, pp->vendor_id);

	if (sysfs_get_devtype(parent, attr_buff, FILE_NAME_SIZE))
		return 1;

	if (!strncmp(attr_buff, "3370", 4)) {
		sprintf(pp->product_id,"S/390 DASD FBA");
	} else if (!strncmp(attr_buff, "9336", 4)) {
		sprintf(pp->product_id,"S/390 DASD FBA");
	} else {
		sprintf(pp->product_id,"S/390 DASD ECKD");
	}

	condlog(3, "%s: product = %s", pp->dev, pp->product_id);

	/*
	 * set the hwe configlet pointer
	 */
	pp->hwe = find_hwe(conf->hwtable, pp->vendor_id, pp->product_id, NULL);

	/*
	 * host / bus / target / lun
	 */
	basenamecpy(parent->devpath, attr_path, FILE_NAME_SIZE);
	pp->sg_id.lun = 0;
	sscanf(attr_path, "%i.%i.%x",
			&pp->sg_id.host_no,
			&pp->sg_id.channel,
			&pp->sg_id.scsi_id);
	condlog(3, "%s: h:b:t:l = %i:%i:%i:%i",
			pp->dev,
			pp->sg_id.host_no,
			pp->sg_id.channel,
			pp->sg_id.scsi_id,
			pp->sg_id.lun);

	return 0;
}

static int
cciss_sysfs_pathinfo (struct path * pp, struct sysfs_device * dev)
{
	char attr_path[FILE_NAME_SIZE];

	/*
	 * host / bus / target / lun
	 */
	basenamecpy(dev->devpath, attr_path, FILE_NAME_SIZE);
	pp->sg_id.lun = 0;
	pp->sg_id.channel = 0;
	sscanf(attr_path, "cciss!c%id%i",
			&pp->sg_id.host_no,
			&pp->sg_id.scsi_id);
	condlog(3, "%s: h:b:t:l = %i:%i:%i:%i",
			pp->dev,
			pp->sg_id.host_no,
			pp->sg_id.channel,
			pp->sg_id.scsi_id,
			pp->sg_id.lun);
	return 0;
}

static int
common_sysfs_pathinfo (struct path * pp, struct sysfs_device *dev)
{
	char *attr;

	attr = sysfs_attr_get_value(dev->devpath, "dev");
	if (!attr) {
		condlog(3, "%s: no 'dev' attribute in sysfs", pp->dev);
		return 1;
	}
	strlcpy(pp->dev_t, attr, BLK_DEV_SIZE);

	condlog(3, "%s: dev_t = %s", pp->dev, pp->dev_t);

	if (sysfs_get_size(dev, &pp->size))
		return 1;

	condlog(3, "%s: size = %llu", pp->dev, pp->size);

	return 0;
}

struct sysfs_device *sysfs_device_from_path(struct path *pp)
{
	char sysdev[FILE_NAME_SIZE];

	strlcpy(sysdev,"/block/", FILE_NAME_SIZE);
	strlcat(sysdev,pp->dev, FILE_NAME_SIZE);

	return sysfs_device_get(sysdev);
}

extern int
sysfs_pathinfo(struct path * pp)
{
	struct sysfs_device *parent;

	pp->sysdev = sysfs_device_from_path(pp);
	if (!pp->sysdev) {
		condlog(1, "%s: failed to get sysfs information", pp->dev);
		return 1;
	}

	if (common_sysfs_pathinfo(pp, pp->sysdev))
		return 1;

	parent = sysfs_device_get_parent(pp->sysdev);
	if (!parent)
		parent = pp->sysdev;

	if (!strncmp(pp->dev,"cciss",5))
		strcpy(parent->subsystem,"cciss");

	condlog(3, "%s: subsystem = %s", pp->dev, parent->subsystem);

	if (!strncmp(parent->subsystem, "scsi",4))
		pp->bus = SYSFS_BUS_SCSI;
	if (!strncmp(parent->subsystem, "ccw",3))
		pp->bus = SYSFS_BUS_CCW;
	if (!strncmp(parent->subsystem, "cciss",5))
		pp->bus = SYSFS_BUS_CCISS;

	if (pp->bus == SYSFS_BUS_UNDEF)
		return 0;
	else if (pp->bus == SYSFS_BUS_SCSI) {
		if (scsi_sysfs_pathinfo(pp, parent))
			return 1;
	} else if (pp->bus == SYSFS_BUS_CCW) {
		if (ccw_sysfs_pathinfo(pp, parent))
			return 1;
	} else if (pp->bus == SYSFS_BUS_CCISS) {
		if (cciss_sysfs_pathinfo(pp, pp->sysdev))
			return 1;
	}
	return 0;
}

static int
scsi_ioctl_pathinfo (struct path * pp, int mask)
{
	if (mask & DI_SERIAL) {
		get_serial(pp->serial, SERIAL_SIZE, pp->fd);
		condlog(3, "%s: serial = %s", pp->dev, pp->serial);
	}

	return 0;
}

static int
cciss_ioctl_pathinfo (struct path * pp, int mask)
{
	int ret;

	if (mask & DI_SYSFS) {
		ret = get_inq(pp->dev, pp->vendor_id, pp->product_id, pp->rev, pp->fd);
		if (ret)
			return ret;

		condlog(3, "%s: vendor = %s", pp->dev, pp->vendor_id);
		condlog(3, "%s: product = %s", pp->dev, pp->product_id);
		condlog(3, "%s: revision = %s", pp->dev, pp->rev);
		/*
		 * set the hwe configlet pointer
		 */
		pp->hwe = find_hwe(conf->hwtable, pp->vendor_id,
				   pp->product_id, pp->rev);

	}
	return 0;
}

static int
get_state (struct path * pp)
{
	struct checker * c = &pp->checker;

	if (!pp->mpp)
		return 0;

	if (!checker_selected(c)) {
		select_checker(pp);
		if (!checker_selected(c))
			return 1;
		checker_set_fd(c, pp->fd);
		if (checker_init(c, &pp->mpp->mpcontext))
			return 1;
	}
	pp->state = checker_check(c);
	condlog(3, "%s: state = %i", pp->dev, pp->state);
	if (pp->state == PATH_DOWN && strlen(checker_message(c)))
		condlog(3, "%s: checker msg is \"%s\"",
			pp->dev, checker_message(c));
	return 0;
}

static int
get_prio (struct path * pp)
{
	if (!pp)
		return 0;

	if (!pp->prio) {
		select_prio(pp);
		if (!pp->prio)
			return 1;
	}
	pp->priority = prio_getprio(pp->prio, pp);
	if (pp->priority < 0) {
		condlog(3, "%s: %s prio error", pp->dev, prio_name(pp->prio));
		pp->priority = PRIO_UNDEF;
		return 1;
	}
	condlog(3, "%s: %s prio = %u",
		pp->dev, prio_name(pp->prio), pp->priority);
	return 0;
}

static int
get_uid (struct path * pp)
{
	char buff[CALLOUT_MAX_SIZE];

	if (!pp->getuid)
		select_getuid(pp);

	if (apply_format(pp->getuid, &buff[0], pp)) {
		condlog(0, "error formatting uid callout command");
		memset(pp->wwid, 0, WWID_SIZE);
	} else if (execute_program(buff, pp->wwid, WWID_SIZE)) {
		condlog(3, "error calling out %s", buff);
		memset(pp->wwid, 0, WWID_SIZE);
		return 1;
	}
	condlog(3, "%s: uid = %s (callout)", pp->dev ,pp->wwid);
	return 0;
}

extern int
pathinfo (struct path *pp, vector hwtable, int mask)
{
	condlog(3, "%s: mask = 0x%x", pp->dev, mask);

	/*
	 * fetch info available in sysfs
	 */
	if (mask & DI_SYSFS && sysfs_pathinfo(pp))
		return 1;

	/*
	 * fetch info not available through sysfs
	 */
	if (pp->fd < 0)
		pp->fd = opennode(pp->dev, O_RDONLY);

	if (pp->fd < 0) {
		condlog(4, "Couldn't open node for %s: %s",
			pp->dev, strerror(errno));
		goto blank;
	}

	if (pp->bus == SYSFS_BUS_SCSI &&
	    scsi_ioctl_pathinfo(pp, mask))
		goto blank;

	if (pp->bus == SYSFS_BUS_CCISS &&
	    cciss_ioctl_pathinfo(pp, mask))
		goto blank;

	if (mask & DI_CHECKER && get_state(pp))
		goto blank;

	 /*
	  * Retrieve path priority, even for PATH_DOWN paths if it has never
	  * been successfully obtained before.
	  */
	if (mask & DI_PRIO &&
	    (pp->state != PATH_DOWN || pp->priority == PRIO_UNDEF)) {
		if( !strlen(pp->wwid))
                        get_uid(pp);
		get_prio(pp);
	}

	if (mask & DI_WWID && !strlen(pp->wwid))
		get_uid(pp);

#ifndef DAEMON
	close(pp->fd);
	pp->fd = -1;
#endif
	return 0;

blank:
	/*
	 * Recoverable error, for example faulty or offline path
	 */
	memset(pp->wwid, 0, WWID_SIZE);
	pp->state = PATH_DOWN;
#ifndef DAEMON
	if (pp->fd > 0){
		close(pp->fd);
		pp->fd = -1;
	}
#endif
	return 0;
}
