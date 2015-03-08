#ifndef _DICT_H
#define _DICT_H

#ifndef _VECTOR_H
#include "vector.h"
#endif

void init_keywords(void);
int get_sys_max_fds(int *);
int print_delay_checks(char * buff, int len, void *ptr);

#endif /* _DICT_H */
