#ifndef _LIBSG_H
#define _LIBSG_H

#define SENSE_BUFF_LEN 32

int sg_read (int sg_fd, unsigned char *, int, unsigned char *, int);

#endif /* _LIBSG_H */
