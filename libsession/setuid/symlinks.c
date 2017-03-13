#define _XOPEN_SOURCE 700
#include "symlinks.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ftw.h>

#define PATH_BUF 350

static int fill_new_dev(struct symlinks *s);
static int link_dir(struct symlinks *s, char *orig, char *from,
		unsigned depth);
static void clear_new_dev(struct symlinks *s);
static int copy_device(struct symlinks *s, char *dst, char *src);

struct symlinks {
	int (*filter)(
			void *user,
			char *in,
			struct stat *st);
	void *user;
	char *new_dev;
	char *old_dev;
};

static int symlinks(
		struct symlinks *s,
		struct symlinks **s_out,
		char *new_dev,
		char *old_dev,
		int (*filter)(
			void *user,
			char *in,
			struct stat *st),
		void *user)
{
	if(s) goto free;

	s = malloc(sizeof *s);
	if(!s) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n", __FILE__,
				__LINE__);
		goto e_malloc;
	}

	s->filter = filter;
	s->user = user;
	s->new_dev = new_dev;
	s->old_dev = old_dev;

	if(fill_new_dev(s) < 0) goto e_fill_new_dev;

	*s_out = s;
	return 0;

free:
e_fill_new_dev:
	clear_new_dev(s);
	free(s);
e_malloc:
	return -1;
}

int symlinks_new(
		struct symlinks **s_out,
		char *new_dev,
		char *old_dev,
		int (*filter)(
			void *user,
			char *in,
			struct stat *st),
		void *user)
{
	return symlinks(NULL, s_out, new_dev, old_dev, filter, user);
}

void symlinks_free(struct symlinks *s)
{
	if(s) symlinks(s, NULL, NULL, NULL, NULL, NULL);
}

static int try_append(struct symlinks *s, char *buf, char *entry)
{
	if(strlen(buf) + 1 + strlen(entry) + 1 > PATH_BUF) {
		fprintf(stderr, "Pathname too long: %s/%s.\n",
				buf, entry);
		return -1;
	}
	strcat(buf, "/");
	strcat(buf, entry);
	return 0;
}

static int fill_new_dev(struct symlinks *s)
{
	char from[PATH_BUF];
	char orig[PATH_BUF];
	int status;
	snprintf(from, sizeof from, "%s", s->new_dev);
	strcpy(orig, "/dev");
	mode_t prev_mask = umask(0);
	status = link_dir(s, orig, from, 10);
	umask(prev_mask);
	return status;
}

static int link_dir(
		struct symlinks *s,
		char *orig,
		char *from,
		unsigned depth)
{
	int r = -1;

	if(!depth) {
		fprintf(stderr, "/dev is too deep.\n");
		goto e_depth;
	}

	DIR *dir = opendir(orig);
	if(!dir) {
		fprintf(stderr, "Could not open %s: %s.\n", orig,
				strerror(errno));
		goto e_opendir;
	}

	unsigned orig_len = strlen(orig);
	unsigned from_len = strlen(from);

	while(1) {
		/* Read next directory entry. */
		errno = 0;
		struct dirent *entry = readdir(dir);
		if(errno != 0) {
			fprintf(stderr, "Error listing %s: %s.\n", orig,
					strerror(errno));
			goto e_symlink;
		}
		if(!entry) break;

		if(!strcmp(entry->d_name, ".") ||
				!strcmp(entry->d_name, "..")) continue;

		/* Append entry to path names except 'to', because it's needed
		 * for filtering. */
		if(try_append(s, orig, entry->d_name) < 0) goto e_symlink;
		if(try_append(s, from, entry->d_name) < 0) goto e_symlink;

		/* Check if directory or something else. */
		struct stat st;
		int status = lstat(orig, &st);
		if(status < 0) {
			fprintf(stderr, "Could not stat %s: %s.\n", orig,
					strerror(errno));
			goto e_symlink;
		}

		if((st.st_mode & S_IFMT) == S_IFDIR) {
			/* Recurse if directory. */

			status = mkdir(from, 0755);
			if(status < 0) {
				fprintf(stderr, "Could not create directory "
						"%s: %s.\n", from,
						strerror(errno));
				goto e_symlink;
			}
			if(link_dir(s, orig, from, depth - 1) < 0)
				goto e_symlink;
		}
		else {
			status = copy_device(s, from, orig);
			if(status) goto e_symlink;
#if 0
			char buf[PATH_BUF];
			/* to contains dest directory because it wasn't
			 * appended. */

			unsigned rel_start = strlen(buf);
			/* Turns e.g. "tty0" into ".session-XXXXXX/tty". */
			status = s->filter(s->user,
					buf,
					sizeof buf,
					/* Skip "/dev/". */
					orig + 5);
			if(status) goto e_symlink;

			status = copy_device(s, buf, orig + 5);
			if(status) goto e_symlink;
#endif
		}

		orig[orig_len] = '\0';
		from[from_len] = '\0';
	}

	r = 0;

e_symlink:
	closedir(dir);
e_opendir:
e_depth:
	return r;
}

static int copy_device(struct symlinks *s, char *dst, char *src)
{
	struct stat stat;
	if(lstat(src, &stat)) {
		fprintf(stderr, "Could not stat %s: %s.\n",
				src, strerror(errno));
		return -1;
	}

	int status = s->filter(s->user, src, &stat);
	if(status) return -1;

	if((stat.st_mode & S_IFMT) == S_IFBLK ||
			(stat.st_mode & S_IFMT) == S_IFCHR) {
		/* Device files. */
		if(mknod(dst, stat.st_mode, stat.st_rdev) == -1) {
			fprintf(stderr, "Could not create device "
					"file %s: %s.\n", dst,
					strerror(errno));
			return -1;
		}
		if(chown(dst, stat.st_uid, stat.st_gid) == -1) {
			fprintf(stderr, "Could not change ownership of %s: "
					"%s.\n", dst, strerror(errno));
			return -1;
		}
	}
	else {
		/* Sockets, symlinks, other things. */
		char old_dev_path[PATH_BUF];
		if(snprintf(old_dev_path, sizeof old_dev_path, "%s/%s",
					s->old_dev, src + 5) >=
				sizeof old_dev_path) {
			fprintf(stderr, "Path name %s/%s too long.\n",
					s->old_dev, src + 5);
		}

		if(symlink(old_dev_path, dst)) {
			fprintf(stderr, "Could not create symlink from %s "
					"to %s: %s.\n", dst,
					old_dev_path, strerror(errno));
			return -1;
		}
	}
	return 0;
}

static int rm_callback(
		const char *path,
		const struct stat *sb,
		int typeflag,
		struct FTW *ftw)
{
	if(ftw->level == 0) return 0;
	if(typeflag == FTW_DP) rmdir(path);
	else unlink(path);
	return 0;
}

static void clear_new_dev(struct symlinks *s)
{
	nftw(s->new_dev, rm_callback, 1, FTW_DEPTH | FTW_PHYS);
}

char *symlinks_new_dev(struct symlinks *s)
{
	return s->new_dev;
}

char *symlinks_old_dev(struct symlinks *s)
{
	return s->old_dev;
}
