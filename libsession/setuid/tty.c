#include "tty.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/fuse.h>
#include <sys/uio.h>

static void tty_event(void *user1, int event);
static int tty_init(struct tty *t, char *devnode);

#define BUFSZ 0x21000

struct tty {
	void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1);
	void (*unregister_fd)(void *fd_ptr);
	void *user;

	int fd;
	void *fd_ptr;
};

int tty(
		struct tty *t,
		struct tty **t_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		char *devnode)
{
	if(t) goto free;

	t = malloc(sizeof *t);
	if(!t) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.",
				__FILE__, __LINE__);
		goto e_malloc_t;
	}

	t->register_fd = register_fd;
	t->unregister_fd = unregister_fd;
	t->user = user;

	t->fd = open("/dev/cuse", O_RDWR | O_CLOEXEC);
	if(t->fd < 0) {
		fprintf(stderr, "Could not open /dev/cuse: %s.%s",
				strerror(errno), errno == ENOENT ?
				" Perhaps try \"modprobe cuse\"?" : "");
		goto e_open_dev_cuse;
	}

	t->fd_ptr = t->register_fd(t->user, t->fd, 1, tty_event, t);
	if(!t->fd_ptr) goto e_register_fd;

	/* Do CUSE initialization so the device will be present immediately. */
	if(tty_init(t, devnode)) goto e_init;

	*t_out = t;
	return 0;

free:
e_init:
	t->unregister_fd(t->fd_ptr);
e_register_fd:
	close(t->fd);
e_open_dev_cuse:
	free(t);
e_malloc_t:
	return -1;
}

int tty_new(
		struct tty **t_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		char *devnode)
{
	return tty(NULL, t_out, register_fd, unregister_fd, user, devnode);
}

void tty_free(struct tty *t)
{
	if(t) tty(t, NULL, NULL, NULL, NULL, NULL);
}

static int tty_init(struct tty *t, char *devnode)
{
	struct fuse_in_header header_in;
	struct cuse_init_in init_in;

	struct iovec iov[2] = {
		{ .iov_base = &header_in, .iov_len = sizeof header_in },
		{ .iov_base = &init_in, .iov_len = sizeof init_in }
	};

	int status = readv(t->fd, iov, 2);
	if(status < 0) {
		fprintf(stderr, "Could not read message from /dev/cuse: %s.\n",
				strerror(errno));
		goto error;
	}

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in.unique;

	struct cuse_init_out init_out;
	memset(&init_out, 0, sizeof init_out);
	init_out.major = FUSE_KERNEL_VERSION;
	init_out.minor = FUSE_KERNEL_MINOR_VERSION;
	init_out.unused = 0;
	init_out.flags = CUSE_UNRESTRICTED_IOCTL;
	init_out.max_read = BUFSZ;
	init_out.max_write = BUFSZ;
	init_out.dev_major = 0;
	init_out.dev_minor = 0;

	char devinfo2[200];
	status = snprintf(devinfo2, sizeof devinfo2, "DEVNAME=%s", devnode);
	if(status + 1 > sizeof devinfo2) {
		fprintf(stderr, "Path too long: %s.\n", devnode);
		goto error;
	}

	struct iovec iov1[3] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &init_out, .iov_len = sizeof init_out },
		{ .iov_base = devinfo2, .iov_len = strlen(devinfo2) + 1 }
	};
	header_out.len = iov1[0].iov_len + iov1[1].iov_len + iov1[2].iov_len;

	if(writev(t->fd, iov1, 3) == -1) {
		fprintf(stderr, "Could not write to /dev/cuse: %s.\n",
				strerror(errno));
		goto error;
	}

	return 0;

error:
	return -1;
}

/*
 * TTY (Xorg sends KDSETMODE. Just ignore it.)
 */

static int respond_to_tty_open(struct tty *t, struct fuse_in_header *header,
		void *data)
{
	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header->unique;

	struct fuse_open_out out;
	memset(&out, 0, sizeof out);
	out.fh = 1; // Can be anything as far as I can tell
	out.open_flags = 0;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &out, .iov_len = sizeof out }
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	return writev(t->fd, iov, 2);
}

static int respond_to_tty_release(struct tty *t, struct fuse_in_header *header,
		void *data)
{
	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header->unique;

	struct iovec iov[1] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out }
	};
	header_out.len = iov[0].iov_len;

	return writev(t->fd, iov, 1);
}

static int respond_to_tty_ioctl(struct tty *t, struct fuse_in_header *header,
		void *data)
{
	// Ignore everything and return success

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header->unique;

	struct fuse_ioctl_out out;
	out.result = 0;
	out.flags = 0; 
	out.in_iovs = 0;
	out.out_iovs = 0;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &out, .iov_len = sizeof out }
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	return writev(t->fd, iov, 2);
}

static int process_tty(struct tty *t)
{
	char buf[BUFSZ];
	struct fuse_in_header header_in;

	struct iovec iov[2] = {
		{ .iov_base = &header_in, .iov_len = sizeof header_in },
		{ .iov_base = buf, .iov_len = sizeof buf }
	};

	int status = readv(t->fd, iov, 2);
	if(status < 0) {
		fprintf(stderr, "Could not read message from /dev/cuse: %s.\n",
				strerror(errno));
		goto err0;
	}

	switch(header_in.opcode) {
		case FUSE_OPEN:
			if(respond_to_tty_open(t, &header_in, buf) < 0)
				goto err0;
			break;
		case FUSE_RELEASE:
			if(respond_to_tty_release(t, &header_in, buf) < 0)
				goto err0;
			break;
		case FUSE_IOCTL:
			if(respond_to_tty_ioctl(t, &header_in, buf) < 0)
				goto err0;
			break;
		default:
			fprintf(stderr, "libsession: unknown FUSE opcode: "
					"%d.\n", header_in.opcode);
			break;
	}

	return 0;

err0:	return -1;
}

static void tty_event(void *user1, int event)
{
	struct tty *t = user1;
	process_tty(t);
}
