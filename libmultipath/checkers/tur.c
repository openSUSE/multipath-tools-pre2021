/*
 * Some code borrowed from sg-utils.
 *
 * Copyright (c) 2004 Christophe Varoqui
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include "checkers.h"

#include "../libmultipath/debug.h"
#include "../libmultipath/sg_include.h"

#define TUR_CMD_LEN 6
#define HEAVY_CHECK_COUNT       10

#define MSG_TUR_UP	"tur checker reports path is up"
#define MSG_TUR_DOWN	"tur checker reports path is down"
#define MSG_TUR_GHOST	"tur checker reports path is in standby state"
#define MSG_TUR_RUNNING	"tur checker still running"
#define MSG_TUR_FAILED	"tur checker failed to initialize"

struct tur_checker_context {
	int state;
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t running;
};

int libcheck_init (struct checker * c)
{
	struct tur_checker_context *ct;

	ct = malloc(sizeof(struct tur_checker_context));
	if (!ct)
		return 1;
	memset(ct, 0, sizeof(struct tur_checker_context));

	ct->state = PATH_UNCHECKED;
	pthread_cond_init(&ct->running, NULL);
	pthread_mutex_init(&ct->lock, NULL);
	c->context = ct;

	return 0;
}

void libcheck_free (struct checker * c)
{
	if (c->context) {
		struct tur_checker_context *ct = c->context;

		pthread_mutex_destroy(&ct->lock);
		pthread_cond_destroy(&ct->running);
		free(c->context);
	}
	return;
}

int
tur_check (struct checker * c)
{
	struct sg_io_hdr io_hdr;
	unsigned char turCmdBlk[TUR_CMD_LEN] = { 0x00, 0, 0, 0, 0, 0 };
	unsigned char sense_buffer[32];
	int retry_tur = 5;

 retry:
	memset(&io_hdr, 0, sizeof (struct sg_io_hdr));
	memset(&sense_buffer, 0, 32);
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof (turCmdBlk);
	io_hdr.mx_sb_len = sizeof (sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.cmdp = turCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = DEF_TIMEOUT;
	io_hdr.pack_id = 0;
	if (ioctl(c->fd, SG_IO, &io_hdr) < 0) {
		MSG(c, MSG_TUR_DOWN);
		return PATH_DOWN;
	}
	if ((io_hdr.status & 0x7e) == 0x18) {
		/*
		 * SCSI-3 arrays might return
		 * reservation conflict on TUR
		 */
		MSG(c, MSG_TUR_UP);
		return PATH_UP;
	}
	if (io_hdr.info & SG_INFO_OK_MASK) {
		int key = 0, asc, ascq;

		switch (io_hdr.host_status) {
		case DID_OK:
		case DID_NO_CONNECT:
		case DID_BAD_TARGET:
		case DID_ABORT:
		case DID_TRANSPORT_DISRUPTED:
		case DID_TRANSPORT_FAILFAST:
			break;
		default:
			/* Driver error, retry */
			if (--retry_tur)
				goto retry;
			break;
		}
		if (io_hdr.sb_len_wr > 3) {
			if (io_hdr.sbp[0] == 0x72 || io_hdr.sbp[0] == 0x73) {
				key = io_hdr.sbp[1] & 0x0f;
				asc = io_hdr.sbp[2];
				ascq = io_hdr.sbp[3];
			} else if (io_hdr.sb_len_wr > 13 &&
				   ((io_hdr.sbp[0] & 0x7f) == 0x70 ||
				    (io_hdr.sbp[0] & 0x7f) == 0x71)) {
				key = io_hdr.sbp[2] & 0x0f;
				asc = io_hdr.sbp[12];
				ascq = io_hdr.sbp[13];
			}
		}
		if (key == 0x6) {
			/* Unit Attention, retry */
			if (--retry_tur)
				goto retry;
		}
		else if (key == 0x2) {
			/* Not Ready */
			/* Note: Other ALUA states are either UP or DOWN */
			if( asc == 0x04 && ascq == 0x0b){
				/*
				 * LOGICAL UNIT NOT ACCESSIBLE,
				 * TARGET PORT IN STANDBY STATE
				 */
				MSG(c, MSG_TUR_GHOST);
				return PATH_GHOST;
			}
		}
		MSG(c, MSG_TUR_DOWN);
		return PATH_DOWN;
	}
	MSG(c, MSG_TUR_UP);
	return PATH_UP;
}

void *tur_thread(void *ctx)
{
	struct checker *c = ctx;
	struct tur_checker_context *ct = c->context;
	int state;

	condlog(3, "tur checker starting up");

	/* TUR checker start up */
	pthread_mutex_lock(&ct->lock);
	ct->state = PATH_PENDING;

	state = tur_check(c);

	/* TUR checker done */
	ct->state = state;
	pthread_mutex_unlock(&ct->lock);
	pthread_cond_signal(&ct->running);

	condlog(3, "tur checker finished, state %d", state);
	return ((void *)0);
}


void tur_timeout(struct timespec *tsp)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	tsp->tv_sec = now.tv_sec;
	tsp->tv_nsec = now.tv_usec * 1000;
	tsp->tv_nsec += 100000; /* Wait for 100 msec */
}

extern int
libcheck_check (struct checker * c)
{
	struct tur_checker_context *ct = c->context;

	if (!ct)
		return PATH_UNCHECKED;

	if (c->sync)
		return tur_check(c);
	else {
		struct timespec tsp;
		int tur_status, r;

		/*
		 * Async mode
		 */
		r = pthread_mutex_trylock(&ct->lock);
		if (r == EBUSY) {
			condlog(3, "tur checker still not finished");
			MSG(c, MSG_TUR_RUNNING);
			return PATH_PENDING;
		} else if (r != 0) {
			condlog(3, "tur mutex lock failed with %d", r);
			MSG(c, MSG_TUR_FAILED);
			return PATH_WILD;
		}
		if (ct->state != PATH_UNCHECKED) {
			r = ct->state;
			ct->state = PATH_UNCHECKED;
			pthread_mutex_unlock(&ct->lock);
			return r;
		}

		tur_timeout(&tsp);
		r = pthread_create(&ct->thread, NULL, tur_thread, c);
		if (r) {
			pthread_mutex_unlock(&ct->lock);
			condlog(3, "tur thread creation failure %d", r);
			MSG(c, MSG_TUR_FAILED);
			return PATH_WILD;
		}
		r = pthread_cond_timedwait(&ct->running, &ct->lock, &tsp);
		pthread_mutex_unlock(&ct->lock);
		if (r == ETIMEDOUT) {
			condlog(3, "tur checker still running");
			tur_status = PATH_PENDING;
		} else if (r != 0) {
			condlog(3, "tur checker failed waiting with %d", r);
			MSG(c, MSG_TUR_FAILED);
			tur_status = PATH_WILD;
		} else
			tur_status = ct->state;

		return tur_status;
	}
}
