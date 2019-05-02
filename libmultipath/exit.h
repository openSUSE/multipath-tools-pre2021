#ifndef _EXIT_H
#define _EXIT_H

void set_should_exit_fn(int(*fn)(void));
int should_exit(void);
#endif /* _EXIT_H */
