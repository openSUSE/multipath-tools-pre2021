#ifndef _ALIAS_H
#define _ALIAS_H

int valid_alias(char *alias);
char *get_user_friendly_alias(char *wwid, char *file, char *prefix,
			      int bindings_readonly);
int get_user_friendly_wwid(char *alias, char *buff, char *file);
char *use_existing_alias (char *wwid, char *file, char *alias_old,
		char *prefix, int bindings_read_only);
struct config;
int check_alias_settings(const struct config *);
#endif /* _ALIAS_H */
