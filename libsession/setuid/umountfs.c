#include "umountfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <linux/fuse.h>
#include <assert.h>
#include <poll.h>

#define IN_BUF 0x21000
#define WANTED_FUSE_MAJOR 7
#define WANTED_FUSE_MINOR 23

#define UID 0
#define GID 0
#define ROOTMODE 040755

static void fuse_event(void *user, int event);
static void fuse_readable(struct umountfs *e);
static int fuse_mount(struct umountfs *e, char *mountpoint);
static int read_fuse_message(int fd, struct fuse_in_header *hdr,
		void *payload, unsigned payload_len);
static int write_fuse_message(int fd, struct fuse_out_header *hdr,
		void *payload, unsigned payload_len);

#define rmsg(payload) read_fuse_message(\
		e->fuse_fd, &ih, &payload, sizeof payload)
#define wmsg(payload) { oh.unique = ih.unique; write_fuse_message(\
		e->fuse_fd, &oh, &payload, sizeof payload); }

struct umountfs {
	int fuse_fd;
	void *fuse_ptr;
	void (*unregister_fd)(void *fd_ptr);
	void (*notify_umount)(void *user);
	void *user;
};

static int umountfs(
		struct umountfs *e,
		struct umountfs **e_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		char *mountpoint,
		void (*notify_umount)(void *user))
{
	if(e) goto free;

	e = malloc(sizeof *e);
	if(!e) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n", __FILE__,
				__LINE__);
		goto e_malloc;
	}

	e->unregister_fd = unregister_fd;
	e->notify_umount = notify_umount;
	e->user = user;

	e->fuse_fd = open("/dev/fuse", O_RDWR | O_CLOEXEC);
	if(e->fuse_fd < 0) {
		fprintf(stderr, "Could not open /dev/fuse: %s.\n",
				strerror(errno));
		goto e_open;
	}

	/* NOTE: /dev/fuse MUST be added to epoll AFTER mount(2). Otherwise,
	 * polling doesn't work. I spent a day on this. */
	if(fuse_mount(e, mountpoint)) goto e_mount;

	e->fuse_ptr = register_fd(user, e->fuse_fd, 1, fuse_event, e);
	if(!e->fuse_ptr) goto e_register_fd;

	*e_out = e;
	return 0;

free:
	if(e->fuse_ptr) e->unregister_fd(e->fuse_ptr);
e_register_fd:
	/* Only unmount on error in umountfs_new because the mountpoint may not
	 * be present there otherwise. */
	if(!e) umount(mountpoint);
e_mount:
	close(e->fuse_fd);
e_open:
	free(e);
e_malloc:
	return -1;
}

int umountfs_new(
		struct umountfs **e_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		char *mountpoint,
		void (*notify_umount)(void *user))
{
	return umountfs(NULL, e_out, register_fd, unregister_fd, user,
			mountpoint, notify_umount);
}

void umountfs_free(struct umountfs *e)
{
	if(e) umountfs(e, NULL, NULL, NULL, NULL, NULL, NULL);
}

static int fuse_mount(struct umountfs *e, char *mountpoint)
{
	int status;
	char options[100];
	snprintf(options, sizeof options,
			"fd=%d,"
			"rootmode=%o,"
			"user_id=%u,"
			"group_id=%u",
			e->fuse_fd,
			ROOTMODE,
			UID,
			GID);
	status = mount("nothing", mountpoint, "fuse", 0, options);
	if(status == -1) {
		fprintf(stderr, "Could not mount umountfs on %s: %s.\n",
				mountpoint, strerror(errno));
		goto e_mount;
	}

	/* FUSE initialization. */
	struct fuse_in_header ih;
	struct fuse_init_in ii;
	struct fuse_out_header oh;
	struct fuse_init_out io;
	struct iovec iov[2];
	iov[0].iov_base = &ih;
	iov[0].iov_len = sizeof ih;
	iov[1].iov_base = &ii;
	iov[1].iov_len = sizeof ii;
	if(readv(e->fuse_fd, iov, sizeof iov / sizeof iov[0]) == -1) {
		fprintf(stderr, "Error reading init request from FUSE: "
				"%s.\n", strerror(errno));
		goto e_fuse_init;
	}

	if(ih.opcode != FUSE_INIT) {
		fprintf(stderr, "Wrong initial message from FUSE.\n");
		goto e_fuse_init;
	}

	if(ii.major < WANTED_FUSE_MAJOR || (ii.major == WANTED_FUSE_MAJOR &&
				ii.minor < WANTED_FUSE_MINOR)) {
		fprintf(stderr, "Fuse kernel version (%u.%u) too old. At least "
				"%u.%u required.\n", ii.major, ii.minor,
				WANTED_FUSE_MAJOR, WANTED_FUSE_MINOR);
		goto e_fuse_init;
	}

	/* The fuse header tells me to do this, but it's probably never gonna
	 * be used, judging by the source code. */
	if(ii.major > WANTED_FUSE_MAJOR) {
		oh.error = 0;
		memset(&io, 0, sizeof io);
		io.major = WANTED_FUSE_MAJOR;
		io.minor = WANTED_FUSE_MINOR;
		wmsg(io);
		rmsg(ii);
		if(ii.major != WANTED_FUSE_MAJOR ||
				ii.minor != WANTED_FUSE_MINOR)
			goto e_fuse_init;
	}

	oh.error = 0;
	memset(&io, 0, sizeof io);
	io.major = WANTED_FUSE_MAJOR;
	io.minor = WANTED_FUSE_MINOR;
	io.max_readahead = ii.max_readahead;
	io.max_write = 0x20000;

	wmsg(io);

	return 0;

e_fuse_init:
	umount(mountpoint);
e_mount:
	return -1;
}

static void fuse_event(void *user, int event)
{
	struct umountfs *e = user;
	if(event & 12) {
		/* Error or HUP. */
		e->notify_umount(e->user);
		e->unregister_fd(e->fuse_ptr);
		e->fuse_ptr = NULL;
	}
	else if(event & 1) {
		/* Readable. */
		fuse_readable(e);
	}
}

static void fuse_readable(struct umountfs *e)
{
	struct fuse_in_header ih;
	char buf[IN_BUF];

	struct iovec iov[2];
	iov[0].iov_base = &ih;
	iov[0].iov_len = sizeof ih;
	iov[1].iov_base = buf;
	iov[1].iov_len = sizeof buf;
	if(readv(e->fuse_fd, iov, sizeof iov / sizeof iov[0]) == -1) {
		/* ENODEV probably means umount between poll and read. */
		if(errno != ENODEV) {
			fprintf(stderr, "Error reading from FUSE: %s.\n",
					strerror(errno));
		}
		return;
	}

	struct fuse_out_header oh;
	oh.unique = ih.unique;
	oh.error = 0;

	void *arg = buf;
	if(ih.opcode == FUSE_GETATTR) {
		struct fuse_attr_out ao = {};
		ao.attr.ino = 1;
		ao.attr.mode = ROOTMODE;
		ao.attr.nlink = FUSE_ROOT_ID;
		ao.attr.uid = UID;
		ao.attr.gid = GID;
		wmsg(ao);
	}
	else if(ih.opcode == FUSE_OPENDIR) {
		struct fuse_open_out oo = {};
		/* We can assume this refers to the root directory because a
		 * lookup is required otherwise and they all return ENOENT. */
		assert(ih.nodeid == 1);
		oo.fh = 1;
		wmsg(oo);
	}
	else if(ih.opcode == FUSE_READDIR) {
		struct fuse_read_in *ri = arg;
		assert(ri->fh = 1);
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else if(ih.opcode == FUSE_RELEASEDIR) {
		struct fuse_release_in *ri = arg;
		assert(ri->fh = 1);
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else if(ih.opcode == FUSE_LOOKUP) {
		oh.error = -ENOENT;
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else if(ih.opcode == FUSE_CREATE
			|| ih.opcode == FUSE_SYMLINK
			|| ih.opcode == FUSE_MKNOD
			|| ih.opcode == FUSE_MKDIR) {
		oh.error = -EACCES;
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else if(ih.opcode == FUSE_ACCESS
			|| ih.opcode == FUSE_FLUSH) {
		assert(ih.nodeid == 1);
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else if(ih.opcode == FUSE_STATFS) {
		struct fuse_statfs_out so = {};
		wmsg(so);
	}
	else if(ih.opcode == FUSE_FSYNCDIR) {
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
	else {
		oh.error = -ENOSYS;
		oh.unique = ih.unique;
		oh.len = sizeof oh;
		write(e->fuse_fd, &oh, sizeof oh);
	}
}

static int read_fuse_message(int fd, struct fuse_in_header *hdr,
		void *payload, unsigned payload_len)
{
	struct iovec iov[2];
	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof *hdr;
	iov[1].iov_base = payload;
	iov[1].iov_len = payload_len;
	hdr->len = iov[0].iov_len + iov[1].iov_len;
	int status = readv(fd, iov, sizeof iov / sizeof iov[1]);
	if(status == -1) {
		fprintf(stderr, "Error reading from FUSE: %s.\n",
				strerror(errno));
		return -1;
	}
	return status;
}

static int write_fuse_message(int fd, struct fuse_out_header *hdr,
		void *payload, unsigned payload_len)
{
	struct iovec iov[2];
	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof *hdr;
	iov[1].iov_base = payload;
	iov[1].iov_len = payload_len;
	hdr->len = iov[0].iov_len + iov[1].iov_len;
	int status = writev(fd, iov, sizeof iov / sizeof iov[1]);
	if(status == -1) {
		fprintf(stderr, "Error writing to FUSE: %s.\n",
				strerror(errno));
		return -1;
	}
	return status;
}

unsigned umountfs_has_hup(struct umountfs *e)
{
	struct pollfd fd;
	fd.fd = e->fuse_fd;
	fd.events = 0;
	poll(&fd, 1, 0);
	return fd.revents ? 1 : 0;
}
