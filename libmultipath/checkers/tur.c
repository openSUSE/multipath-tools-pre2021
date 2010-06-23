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
#include "../libmultipath/uevent.h"

#define TUR_CMD_LEN 6
#define HEAVY_CHECK_COUNT       10

#define MSG_TUR_UP	"tur checker reports path is up"
#define MSG_TUR_DOWN	"tur checker reports path is down"
#define MSG_TUR_GHOST	"tur checker reports path is in standby state"
#define MSG_TUR_RUNNING	"tur checker still running"
#define MSG_TUR_TIMEOUT	"tur checker timed out"
#define MSG_TUR_FAILED	"tur checker failed to initialize"

struct tur_checker_context {
	int state;
	int running;
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t active;
};

int libcheck_init (struct checker * c)
{
	struct tur_checker_context *ct;

	ct = malloc(sizeof(struct tur_checker_context));
	if (!ct)
		return 1;
	memset(ct, 0, sizeof(struct tur_checker_context));

	ct->state = PATH_UNCHECKED;
	pthread_cond_init(&ct->active, NULL);
	pthread_mutex_init(&ct->lock, NULL);
	c->context = ct;

	return 0;
}

void libcheck_free (struct checker * c)
{
	if (c->context) {
		struct tur_checker_context *ct = c->context;

		pthread_mutex_destroy(&ct->lock);
		pthread_cond_destroy(&ct->active);
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
	int state, signal_thread = 1;

	condlog(3, "tur checker starting up");

	/* TUR checker start up */
	pthread_mutex_lock(&ct->lock);
	ct->state = PATH_PENDING;
	pthread_mutex_unlock(&ct->lock);

	state = tur_check(c);

	/* TUR checker done */
	pthread_mutex_lock(&ct->lock);
	/* Paranoia check: Another thread might have been started */
	if (ct->thread == pthread_self())
		ct->state = state;
	else
		signal_thread = 0;
	pthread_mutex_unlock(&ct->lock);
	if (signal_thread)
		pthread_cond_signal(&ct->active);

	condlog(3, "tur checker finished, state %s",
		checker_state_name(state));
	return ((void *)0);
}


void tur_timeout(struct timespec *tsp)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	tsp->tv_sec = now.tv_sec;
	tsp->tv_nsec = now.tv_usec * 1000;
	tsp->tv_nsec += 1000000; /* 1 millisecond */
}

extern int
libcheck_check (struct checker * c)
{
	struct tur_checker_context *ct = c->context;
	struct timespec tsp;
	pthread_attr_t attr;
	int tur_status, r;


	if (!ct)
		return PATH_UNCHECKED;

	if (c->sync)
		return tur_check(c);

	/*
	 * Async mode
	 */
	r = pthread_mutex_lock(&ct->lock);
	if (r != 0) {
		condlog(2, "tur mutex lock failed with %d", r);
		MSG(c, MSG_TUR_FAILED);
		return PATH_WILD;
	}

	if (ct->running) {
		/* TUR checker running */
		if (ct->state == PATH_PENDING ||
		    ct->state == PATH_UNCHECKED) {
			if (ct->running > c->async_timeout) {
				condlog(3, "abort tur checker on timeout");
				ct->running = 0;
				ct->thread = 0;
				MSG(c, MSG_TUR_TIMEOUT);
				tur_status = PATH_DOWN;
			} else {
				condlog(3, "tur checker still not finished");
				ct->running++;
				MSG(c, MSG_TUR_RUNNING);
				tur_status = PATH_PENDING;
			}
		} else {
			/* TUR checker done */
			ct->running = 0;
			tur_status = ct->state;
			ct->state = PATH_UNCHECKED;
		}
		pthread_mutex_unlock(&ct->lock);
	} else {
		/* Start new TUR checker */
		tur_timeout(&tsp);
		setup_thread_attr(&attr, 32 * 1024, 1);
		r = pthread_create(&ct->thread, &attr, tur_thread, c);
		if (r) {
			pthread_mutex_unlock(&ct->lock);
			condlog(2, "tur thread creation failure %d", r);
			MSG(c, MSG_TUR_FAILED);
			return PATH_WILD;
		}
		pthread_attr_destroy(&attr);
		r = pthread_cond_timedwait(&ct->active, &ct->lock, &tsp);
		ct->running = 1;
		tur_status = ct->state;
		pthread_mutex_unlock(&ct->lock);
		if (r == ETIMEDOUT) {
			condlog(3, "tur checker still running");
			tur_status = PATH_PENDING;
		} else if (r != 0) {
			condlog(2, "tur checker failed waiting with %d", r);
			MSG(c, MSG_TUR_FAILED);
			tur_status = PATH_WILD;
		} else {
			ct->running = 0;
		}
	}

	return tur_status;
}
