#ifndef MAIN_H
#define MAIN_H

#define DAEMON 1
#define MAPGCINT 5

int reconfigure (struct vectors *);
int ev_add_path (char *, struct vectors *);
int ev_remove_path (char *, struct vectors *);
int ev_add_map (char *, int, int, struct vectors *);
int ev_remove_map (char *, struct vectors *);

#endif /* MAIN_H */
