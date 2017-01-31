/*
 * Copy /dev.
 *
 * new_dev - Copies of /dev nodes go here. Must stay valid until symlinks_free.
 * old_dev - Things other than devices become links to here. Example: /dev/log
 *   will turn into ln -s old_dev/log new_dev/log. Must also stay valid.
 * filter - a callback that allows the user to redirect devices.
 *   in - the name of the device node to create a link of
 *   out - what it is to link to in the original /dev directory.
 *   just do strcpy(out, in) plus length checks in the common case.
 */
struct symlinks;
struct stat;
int symlinks_new(
		struct symlinks **s_out,
		char *new_dev,
		char *old_dev,
		int (*filter)(
			void *user,
			char *dev_path,
			struct stat *stat),
		void *user);
void symlinks_free(struct symlinks *s);

char *symlinks_symdir(struct symlinks *s);
char *symlinks_devdir(struct symlinks *s);
