/*
 * (C) Copyright HUAWEI Technology Corp. 2017, All Rights Reserved.
 *
 * main.c
 *
 * Prioritizer for device mapper multipath, where the corresponding priority
 * values of specific paths are provided by a latency algorithm. And the
 * latency algorithm is dependent on arguments.
 *
 * The principle of the algorithm as follows:
 * 1. By sending a certain number "io_num" of read IOs to the current path
 *    continuously, the IOs' average latency can be calculated.
 * 2. According to the average latency of each path and the weight value
 *    "latency_interval", the priority "rc" of each path can be provided.
 *
 * Author(s): Yang Feng <philip.yang@huawei.com>
 *            Zou Ming <zouming.zouming@huawei.com>
 *            Guan Junxiong <guanjunxiong@huawei.com>
 *
 * This file is released under the GPL version 2, or any later version.
 */
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "debug.h"
#include "prio.h"
#include "structs.h"
#include "../checkers/libsg.h"

#define THRES_USEC_VALUE        120000000LL    /*unit: us, =120s*/

#define MAX_IO_NUM              200
#define MIN_IO_NUM              2

#define MAX_LATENCY_INTERVAL    60            /*unit: s*/
#define MIN_LATENCY_INTERVAL    1             /*unit: us, or ms, or s*/

#define DEFAULT_PRIORITY        0

#define MAX_CHAR_SIZE           30

#define CHAR_USEC               "us"
#define CHAR_MSEC               "ms"
#define CHAR_SEC                "s"

enum interval_type {
    INTERVAL_USEC,
    INTERVAL_MSEC,
    INTERVAL_SEC,
    INTERVAL_INVALID
};

/* interval_unit_str and interval_unit_type keep the same assignment sequence */
static const char *interval_unit_str[MAX_CHAR_SIZE] = {
    CHAR_USEC, CHAR_MSEC, CHAR_SEC
};
static const int interval_unit_type[] = {
    INTERVAL_USEC, INTERVAL_MSEC, INTERVAL_SEC
};

#define USEC_PER_SEC      1000000LL
#define USEC_PER_MSEC     1000LL
#define USEC_PER_USEC     1LL

static const int conversion_ratio[] = {
    [INTERVAL_USEC]		= USEC_PER_USEC,
    [INTERVAL_MSEC]     = USEC_PER_MSEC,
    [INTERVAL_SEC]		= USEC_PER_SEC,
    [INTERVAL_INVALID]	= 0
};

static long long path_latency[MAX_IO_NUM];

static inline long long timeval_to_us(const struct timespec *tv)
{
	return ((long long) tv->tv_sec * USEC_PER_SEC) + (tv->tv_nsec >> 10);
}

static int do_readsector0(int fd, unsigned int timeout)
{
	unsigned char buf[4096];
	unsigned char sbuf[SENSE_BUFF_LEN];
	int ret;

	ret = sg_read(fd, &buf[0], 4096, &sbuf[0],
		      SENSE_BUFF_LEN, timeout);

	return ret;
}

int check_args_valid(int io_num, long long latency_interval, int type)
{
    if ((io_num < MIN_IO_NUM) || (io_num > MAX_IO_NUM))
    {
        condlog(0, "args io_num is more than the valid values range");
        return 0;
    }

    /* s:[1, 60], ms:[1, 60000], us:[1, 60000000] */
    if ((latency_interval < MIN_LATENCY_INTERVAL) || (latency_interval > (MAX_LATENCY_INTERVAL * USEC_PER_SEC / conversion_ratio[type])))
    {
        condlog(0, "args latency_interval is more than the valid values range");
        return 0;
    }

    return 1;
}

static int get_interval_type(char *type)
{
    int index;

    for (index = 0; index < sizeof(interval_unit_str)/MAX_CHAR_SIZE; index++)
    {
        if (strcmp(type, interval_unit_str[index]) == 0)
        {
            return interval_unit_type[index];
        }
    }

    return INTERVAL_INVALID;
}

long long get_conversion_ratio(int type)
{
    return conversion_ratio[type];
}

/* In multipath.conf, args form: io_num|latency_interval. For example,
*  args is "20|10ms", this function can get 20, 10.
*/
static int get_interval_and_ionum(char *args,
                                        int *ionum,
                                        long long *interval)
{
    char source[MAX_CHAR_SIZE];
    char vertica = '|';
    char *endstrbefore = NULL;
    char *endstrafter = NULL;
    int type;
    unsigned int size = strlen(args);
    long long ratio;

    if ((args == NULL) || (ionum == NULL) || (interval == NULL))
    {
        condlog(0, "args string is NULL");
        return 0;
    }

    if ((size < 1) || (size > MAX_CHAR_SIZE-1))
    {
        condlog(0, "args string's size is too long");
        return 0;
    }

    memcpy(source, args, size+1);

    if (!isdigit(source[0]))
    {
        condlog(0, "args io_num string's first char is not digit");
        return 0;
    }

    *ionum = (int)strtoul(source, &endstrbefore, 10);
    if (endstrbefore[0] != vertica)
    {
        condlog(0, "segmentation char is invalid");
        return 0;
    }

    if (!isdigit(endstrbefore[1]))
    {
        condlog(0, "args latency_interval string's first char is not digit");
        return 0;
    }

    *interval = (long long)strtol(&endstrbefore[1], &endstrafter, 10);
    type = get_interval_type(endstrafter);
    if (type == INTERVAL_INVALID)
    {
        condlog(0, "args latency_interval type is invalid");
        return 0;
    }

    if (check_args_valid(*ionum, *interval, type) == 0)
    {
        return 0;
    }

	ratio = get_conversion_ratio(type);
    *interval *= (long long)ratio;

    return 1;
}

long long calc_standard_deviation(long long *path_latency, int size, long long avglatency)
{
    int index;
    long long total = 0;

    for (index = 0; index < size; index++)
    {
        total += (path_latency[index] - avglatency) * (path_latency[index] - avglatency);
    }

    total /= (size-1);

    return (long long)sqrt((double)total);
}

int getprio (struct path *pp, char *args, unsigned int timeout)
{
    int rc, temp;
    int index = 0;
    int io_num;
    long long latency_interval;
    long long avglatency;
    long long standard_deviation;
    long long toldelay = 0;
    long long before, after;
    struct timespec tv;

	if (pp->fd < 0)
		return -1;

    if (get_interval_and_ionum(args, &io_num, &latency_interval) == 0)
    {
        condlog(0, "%s: get path_latency args fail", pp->dev);
        return DEFAULT_PRIORITY;
    }

    memset(path_latency, 0, sizeof(path_latency));

    temp = io_num;
    while (temp-- > 0)
    {
        (void)clock_gettime(CLOCK_MONOTONIC, &tv);
        before = timeval_to_us(&tv);

        if (do_readsector0(pp->fd, timeout) == 2)
        {
            condlog(0, "%s: path down", pp->dev);
            return -1;
        }

        (void)clock_gettime(CLOCK_MONOTONIC, &tv);
        after = timeval_to_us(&tv);

        path_latency[index] = after - before;
        toldelay += path_latency[index++];
    }

    avglatency = toldelay/(long long)io_num;
    condlog(4, "%s: average latency is (%lld)", pp->dev, avglatency);

    if (avglatency > THRES_USEC_VALUE)
    {
        condlog(0, "%s: average latency (%lld) is more than thresold", pp->dev, avglatency);
        return DEFAULT_PRIORITY;
    }

    /* warn the user if the latency_interval set is smaller than (2 * standard deviation), or equal */
    standard_deviation = calc_standard_deviation(path_latency, index, avglatency);
    if (latency_interval <= (2 * standard_deviation))
        condlog(3, "%s: args latency_interval set is smaller than 2 * standard deviation (%lld us), or equal",
            pp->dev, standard_deviation);

	rc = (int)(THRES_USEC_VALUE - (avglatency/(long long)latency_interval));
    return rc;
}
