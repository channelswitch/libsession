#include "card.h"
#include <stddef.h>
#include "drm.h"
#include "makejmp.h"
#include <stdint.h>
#include "radeon_drm.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <signal.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <assert.h>
#include <sys/mman.h>
#include <linux/fuse.h>

struct write_iov;

static int initialize_cuse(struct card *c, char *device_name);
static void cuse_process(void *user, int event);
static void timerfd_process(void *user, int event);
static void set_fb_size(struct card *c, unsigned fb, int w, int h);
static int fb_w(struct card *c, unsigned fb);
static int fb_h(struct card *c, unsigned fb);
static void request_context(struct card *c, struct fuse_in_header *ih,
		char *buf);
static int respond_to_dri_ioctl(struct card *c,
		struct fuse_in_header *header_in, void *data);
static void retry(int fd, uint64_t unique, struct fuse_ioctl_iovec *iov_in,
		int iov_in_len, struct fuse_ioctl_iovec *iov_out,
		int iov_out_len);
static void ioctl_out(int fd, uint64_t unique, int return_value, ...);

#define DISABLE_WAIT
#define DEBUG_OUTPUT
#define SCREEN_MESSAGES

enum { FUSE_MMAP = 46 };

struct fuse_mmap_out {
	uint64_t	pointer;
};

struct fuse_mmap_in {
	uint64_t	fh;
	uint64_t	offset;
	uint64_t	length;
	int32_t		prot;
	int32_t		flags;
};


#define BUFSZ 0x21000
#define STACK_SIZE (1 << 20)
#define MAX_CONTEXTS 20

/*
 * Card info
 */

struct framebuffer {
	unsigned fb;
	unsigned name;
	int w, h;
};

struct card {
	int fd, card;

	uint64_t kh;

	enum {
		WAITING_FOR_PAGE_FLIP_EVENT = 1,
		CARD_READABLE = 2,
		CLIENT_POLLING = 4,
	} flags;

	void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1);
	void (*unregister_fd)(void *ptr);
	void *user;

	void *cuse_ptr;
	void *timerfd_ptr;

	int timerfd;
	uint64_t user_data;

	void *card_ptr;

	unsigned n_fbs;
	struct framebuffer *fbs;

	void (*resources)(void *user, uint64_t unique);
	void (*framebuffer)(void *user, uint32_t name, int w, int h);
};

static int card(
		struct card *c,
		struct card **c_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		int fd,
		void (*resources)(void *user, uint64_t unique),
		void (*framebuffer)(void *user, uint32_t name, int w, int h),
		char *device_name)
{
	if(c) goto free;

	c = malloc(sizeof *c);
	if(!c) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.",
				__FILE__, __LINE__);
		goto e_malloc;
	}

	c->fd = open("/dev/cuse", O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if(c->fd < 0) {
		fprintf(stderr, "Could not open /dev/cuse: %s",
				strerror(errno));
		goto e_cuse;
	}

	c->card = fd;

	c->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if(c->timerfd < 0) goto e_timerfd;

	c->flags = 0;

	c->register_fd = register_fd;
	c->unregister_fd = unregister_fd;
	c->user = user;

	c->cuse_ptr = register_fd(user, c->fd, 1, cuse_process, c);
	if(!c->cuse_ptr) goto e_register_cuse;

	c->timerfd_ptr = register_fd(user, c->timerfd, 1, timerfd_process, c);
	if(!c->timerfd_ptr) goto e_register_timerfd;

	/* Self-vsync */
	struct itimerspec its;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 1000000000 / 60;
//XXX
its.it_interval.tv_nsec = 1000000000 / 2;
	its.it_value.tv_sec = its.it_interval.tv_sec;
	its.it_value.tv_nsec = its.it_interval.tv_nsec;
//	timerfd_settime(c->timerfd, 0, &its, NULL);

#ifndef DISABLE_WAIT
	c->card_ptr = register_fd(user, c->card, 5, card_process, c);
	if(!c->card_ptr) goto e_register_card;
#endif

	c->n_fbs = 0;
	c->fbs = NULL;

	c->resources = resources;
	c->framebuffer = framebuffer;

	if(initialize_cuse(c, device_name)) goto e_initialize;

	*c_out = c;
	return 0;

free:
e_initialize:
#ifndef DISABLE_WAIT
	c->unregister_fd(c->card_ptr);
e_register_card:
#endif
	c->unregister_fd(c->timerfd_ptr);
e_register_timerfd:
	c->unregister_fd(c->cuse_ptr);
e_register_cuse:
	close(c->timerfd);
e_timerfd:
	close(c->fd);
e_cuse:
	free(c);
e_malloc:
	return -1;
}

int card_new(
		struct card **c_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		int fd,
		void (*resources)(void *user, uint64_t unique),
		void (*framebuffer)(void *user, uint32_t name, int w, int h),
		char *device_name)
{
	return card(NULL, c_out, register_fd, unregister_fd, user, fd,
			resources, framebuffer, device_name);
}

void card_free(struct card *c)
{
	if(c) card(c, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL);
}

static int initialize_cuse(struct card *c, char *device_name)
{
	struct fuse_in_header header_in;
	struct cuse_init_in init_in;

	struct iovec iov[2] = {
		{ .iov_base = &header_in, .iov_len = sizeof header_in },
		{ .iov_base = &init_in, .iov_len = sizeof init_in }
	};

	int status = readv(c->fd, iov, 2);
	if(status < 0) {
		fprintf(stderr, "Could not read init message from /dev/cuse: "
				"%s.\n", strerror(errno));
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

	static char devinfo[200];
	status = snprintf(devinfo, sizeof devinfo, "DEVNAME=%s", device_name);
	if(status + 1 > sizeof devinfo) {
		fprintf(stderr, "Path too long: %s.\n", device_name);
		goto error;
	}

	struct iovec iov1[3] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &init_out, .iov_len = sizeof init_out },
		{ .iov_base = devinfo, .iov_len = strlen(devinfo) + 1 },
	};
	header_out.len = iov1[0].iov_len + iov1[1].iov_len + iov1[2].iov_len;

	if(writev(c->fd, iov1, 3) == -1) {
		fprintf(stderr, "Error writing init message to /dev/cuse: "
				"%s.\n", strerror(errno));
		goto error;
	}

	return 0;

error:
	return -1;
}

static void timerfd_process(void *user, int event)
{
	struct card *c = user;
	uint64_t expirations;
	read(c->timerfd, &expirations, sizeof expirations);
	//TODO: could loop for expiration times.
	card_vsync(c);
}

static void notify_poll(struct card *c)
{
	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.unique = 0;
	header_out.error = FUSE_NOTIFY_POLL;

	struct fuse_notify_poll_wakeup_out out;
	memset(&out, 0, sizeof out);
	out.kh = c->kh;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &out, .iov_len = sizeof out }
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	writev(c->fd, iov, 2);

	/* Always readable because client hasn't read yet. */
	c->flags |= CARD_READABLE;
	c->flags &= ~CLIENT_POLLING;
}

void card_vsync(struct card *c)
{
	if(c->flags & WAITING_FOR_PAGE_FLIP_EVENT) {
		c->flags &= ~WAITING_FOR_PAGE_FLIP_EVENT;

		if(c->flags & CLIENT_POLLING) notify_poll(c);
		else c->flags |= CARD_READABLE;
	}
}

/*
 * Make FUSE ioctl retrying bearable.
 */

#if 1
static int ioctl_read(struct card *c, struct fuse_in_header *header_in,
		void *data, uintptr_t addr, unsigned len)
{
	/* Ask FUSE for said address */
	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;
	header_out.len = 0;

	struct fuse_ioctl_out out;
	out.result = 0;
	out.flags = FUSE_IOCTL_RETRY;
	out.in_iovs = 1;
	out.out_iovs = 0;

	struct fuse_ioctl_iovec fuse_iovec;

	struct iovec iov[3];

	iov[0].iov_base = &header_out;
	iov[0].iov_len = sizeof header_out;
	header_out.len += iov[0].iov_len;

	iov[1].iov_base = &out;
	iov[1].iov_len = sizeof out;
	header_out.len += iov[1].iov_len;

	iov[2].iov_base = &fuse_iovec;
	iov[2].iov_len = sizeof fuse_iovec;
	header_out.len += iov[2].iov_len;

	fuse_iovec.base = addr;
	fuse_iovec.len = len;

	printf("writev = %ld\n", writev(c->fd, iov, 3));

	/* Wait for response with same unique. */
//	swapjmp(cx->context, c->main_context);


//	return 0;

struct fuse_ioctl_in ioi;
char buf[10000];
	struct iovec iov1[] = {
		{ .iov_base = header_in, .iov_len = sizeof *header_in },
//		{ .iov_base = data, .iov_len = len }
		{ .iov_base = &ioi, .iov_len = sizeof ioi },
		{ .iov_base = buf, .iov_len = sizeof buf }
	};

struct pollfd pfd;
pfd.fd = c->fd; pfd.events = POLLIN;
poll(&pfd, 1, -1);
errno=0;
	int status = readv(c->fd, iov1, sizeof iov1 / sizeof iov1[0]);
printf("IOCTL READ. UNIQUE = %ld, UNIQUE 2 = %ld, status = %d (%s).\n"
		"ioi.cmd = %u, getres = %lu\n", header_out.unique, header_in->unique, status, strerror(errno), ioi.cmd, DRM_IOCTL_MODE_GETRESOURCES);

	memcpy(data, buf, len);
	return status;
}
#else
#define MAX_IOCTL_READS 20
static int ioctl_retry(struct card *c, struct fuse_in_header *header_in,
		unsigned n_iovs, ...)
{
	assert(n_iovs <= MAX_IOCTL_READS);

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;
	header_out.len = 0;

	struct fuse_ioctl_iovec fuse_iovec[MAX_IOCTL_READS];

	va_list args;
	va_start(args, header_in);
	unsigned i;
	for(i = 0; i < n_iovs; ++i) {
		fuse_iovec[i].base = va_arg(args, uint64_t);
		fuse_iovec[i].len = va_arg(args, uint64_t);
	}
	va_end(args);

	struct iovec iov[3];

	iov[0].iov_base = &header_out;
	iov[0].iov_len = sizeof header_out;
	header_out.len += iov[0].iov_len;

	iov[1].iov_base = &out;
	iov[1].iov_len = sizeof out;
	header_out.len += iov[1].iov_len;

	iov[2].iov_base = fuse_iovec;
	iov[2].iov_len = sizeof fuse_iovec[0] * n_iovs;
	header_out.len += iov[2].iov_len;

	return writev(c->fd, iov, sizeof iov / sizeof iov[0]);
}
#endif

/* Each ... is three args: void *src, uintptr_t dst, unsigned len. */
#define MAX_WRITES 10
static int ioctl_write_and_return(struct card *c,
		struct fuse_in_header *header_in,
		int retval, unsigned argc,
		...)
{
//TODO do it in one go
	va_list args;
	unsigned i;

	struct fuse_out_header header_out;
	struct fuse_ioctl_out out;

	struct iovec iov[MAX_WRITES + 2];
	if(argc > MAX_WRITES) return -1;

	if(argc) {
		/* Tell FUSE we want to write the things. */
		memset(&header_out, 0, sizeof header_out);
		header_out.error = 0;
		header_out.unique = header_in->unique;
		header_out.len = 0;

		out.result = 0;
		out.flags = FUSE_IOCTL_RETRY;
		out.in_iovs = 0;
		out.out_iovs = argc;

		struct fuse_ioctl_iovec fuse_iovec[MAX_WRITES];

		iov[0].iov_base = &header_out;
		iov[0].iov_len = sizeof header_out;
		header_out.len += iov[0].iov_len;

		iov[1].iov_base = &out;
		iov[1].iov_len = sizeof out;
		header_out.len += iov[1].iov_len;

		iov[2].iov_base = fuse_iovec;
		iov[2].iov_len = sizeof fuse_iovec[0] * argc;
		header_out.len += iov[2].iov_len;

		va_start(args, argc);
		for(i = 0; i < argc; ++i) {
			va_arg(args, void *);
			fuse_iovec[i].base = va_arg(args, uintptr_t);
			fuse_iovec[i].len = va_arg(args, unsigned);
		}
		va_end(args);

		writev(c->fd, iov, 3);

		/* Wait for response. Same as always, but this time we can
		 * write by appending the data to writev(). */
//		swapjmp(cx->context, c->main_context);

		char buf[BUFSZ];
		struct iovec iov1[2] = {
			{ .iov_base = header_in, .iov_len = sizeof *header_in },
			{ .iov_base = buf, .iov_len = sizeof buf }
		};

		readv(c->fd, iov1, 2);
	}

	/* Now write final response */
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;
	header_out.len = 0;

	out.result = retval;
	out.flags = 0;
	out.in_iovs = 0;
	out.out_iovs = 0;

	iov[0].iov_base = &header_out;
	iov[0].iov_len = sizeof header_out;
	header_out.len += iov[0].iov_len;

	iov[1].iov_base = &out;
	iov[1].iov_len = sizeof out;
	header_out.len += iov[1].iov_len;

	if(argc) {
		va_start(args, argc);
		for(i = 0; i < argc; ++i) {
			iov[2 + i].iov_base = va_arg(args, void *);
			va_arg(args, uintptr_t);
			iov[2 + i].iov_len = va_arg(args, unsigned);
			header_out.len += iov[2 + i].iov_len;
		}
		va_end(args);
	}

	writev(c->fd, iov, 2 + argc);
	return 0;
}

/*
 * Keeping track of framebuffers.
 */

static int add_fb(struct card *c, unsigned fb, unsigned handle)
{
	struct framebuffer *new_fb = realloc(c->fbs,
			sizeof *c->fbs * (c->n_fbs + 1));
	if(!new_fb) goto e_realloc;

	c->fbs = new_fb;

	struct drm_gem_flink flink;
	flink.handle = handle;
	int status = ioctl(c->card, DRM_IOCTL_GEM_FLINK, &flink);
	if(status) goto e_flink;

	new_fb[c->n_fbs].fb = fb;
	new_fb[c->n_fbs].name = flink.name;
	++c->n_fbs;

	return 0;

e_flink:
e_realloc:
	return -1;
}

static void remove_fb(struct card *c, unsigned fb)
{
	unsigned i;
	for(i = 0; i < c->n_fbs; ++i) {
		if(c->fbs[i].fb == fb) goto found;
	}
	return;

found:
	memmove(c->fbs + i, c->fbs + i + 1,
			sizeof *c->fbs * (c->n_fbs - i - 1));
	--c->n_fbs;
}

static uint32_t fb_to_name(struct card *c, unsigned fb)
{
	unsigned i;
	for(i = 0; i < c->n_fbs; ++i) {
		if(c->fbs[i].fb == fb) return c->fbs[i].name;
	}
	return 0;
}

static void set_fb_size(struct card *c, unsigned fb, int w, int h)
{
	unsigned i;
	for(i = 0; i < c->n_fbs; ++i) {
		if(c->fbs[i].fb == fb) {
			c->fbs[i].w = w;
			c->fbs[i].h = h;
		}
	}
}

static int fb_w(struct card *c, unsigned fb)
{
	unsigned i;
	for(i = 0; i < c->n_fbs; ++i) {
		if(c->fbs[i].fb == fb) return c->fbs[i].w;
	}
	return 0;
}

static int fb_h(struct card *c, unsigned fb)
{
	unsigned i;
	for(i = 0; i < c->n_fbs; ++i) {
		if(c->fbs[i].fb == fb) return c->fbs[i].h;
	}
	return 0;
}

/*
 * Here comes the interesting stuff
 */

#ifdef DEBUG_OUTPUT
static void print_mode(struct drm_mode_modeinfo *mode)
{
	printf("  Mode Name: %s\n"
			"    Clock: %d\n"
			"    Display: %dx%d\n"
			"    Sync: X from %d to %d, Y from %d to %d\n"
			"    Total: %dx%d\n"
			"    Horizontal Skew: %d\n"
			"    Vertical Overscan: %d\n"
			"    Refresh Rate: %d\n"
			"    Flags: %d\n"
			"    Type: %d\n",
			mode->name,
			mode->clock,
			mode->hdisplay, mode->vdisplay,
			mode->hsync_start, mode->hsync_end,
			mode->vsync_start, mode->vsync_end,
			mode->htotal, mode->vtotal,
			mode->hskew,
			mode->vscan,
			mode->vrefresh,
			mode->flags,
			mode->type);
}

static void print_array(char *str, uint32_t *array, unsigned sz, unsigned len)
{
	unsigned i;
	printf("%s", str);
	for(i = 0; i < len; ++i) {
		if(i < sz) printf(" %d", array[i]);
		else printf(" -");
	}
	printf("\n");
}
#endif

static int respond_to_dri_ioctl_old(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
	struct fuse_ioctl_in *ioi = data;
printf("IOCTL len = %d\n", header_in->len);
printf("IOCTL in command = %d, flags = %d, arg = %ld\n", ioi->cmd, ioi->flags,
		ioi->arg);

//#define CASE(name) case name: printf("%s\n", #name);
#define CASE(name) case name:

	switch(ioi->cmd) {
		CASE(DRM_IOCTL_VERSION)
		{
			int err = -1;
			struct drm_version vers;
			ioctl_read(c, header_in, &vers, ioi->arg, sizeof vers);


			char *orig_name = vers.name;
			char *orig_date = vers.date;
			char *orig_desc = vers.desc;

			size_t orig_name_len = vers.name_len;
			size_t orig_date_len = vers.date_len;
			size_t orig_desc_len = vers.desc_len;

			char *name = malloc(orig_name_len);
			if(orig_name_len && !name) goto errversion_0;

			char *date = malloc(orig_date_len);
			if(orig_date_len && !date) goto errversion_1;

			char *desc = malloc(orig_desc_len);
			if(orig_desc_len && !desc) goto errversion_2;

			vers.name = name;
			vers.date = date;
			vers.desc = desc;

			int status = ioctl(c->card, DRM_IOCTL_VERSION, &vers);
			if(status < 0) {
				err = status;
				goto errversion_3;
			}

			vers.name = orig_name;
			vers.date = orig_date;
			vers.desc = orig_desc;

			ioctl_write_and_return(c, header_in, status, 4,
					&vers, ioi->arg, sizeof vers,
					name, vers.name, orig_name_len,
					date, vers.date, orig_date_len,
					desc, vers.desc, orig_desc_len);
			err = 0;

errversion_3:		free(desc);
errversion_2:		free(date);
errversion_1:		free(name);
errversion_0:		if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETRESOURCES)
		{
#ifdef SCREEN_MESSAGES
			int r = -1;
			struct drm_mode_card_res res;
			ioctl_read(c, header_in, &res, ioi->arg, sizeof res);
printf("DRM_IOCTL_MODE_GETRESOURCES: unique %lu\n", header_in->unique);
			ioctl_write_and_return(c, header_in, r, 1,
					&res, ioi->arg, sizeof res);
			break;
#else
			int err = -1;
			struct drm_mode_card_res res;
			ioctl_read(c, header_in, &res, ioi->arg, sizeof res);

			uint64_t orig_fbs = res.fb_id_ptr;
			uint64_t orig_crtcs = res.crtc_id_ptr;
			uint64_t orig_connectors = res.connector_id_ptr;
			uint64_t orig_encoders = res.encoder_id_ptr;

			uint32_t orig_fb_size = sizeof(uint32_t) * res.count_fbs;
			uint32_t orig_crtc_size = sizeof(uint32_t) * res.count_crtcs;
			uint32_t orig_connector_size = sizeof(uint32_t) * res.count_connectors;
			uint32_t orig_encoder_size = sizeof(uint32_t) * res.count_encoders;

			void *fbs = malloc(orig_fb_size);
			if(orig_fb_size && !fbs) goto errgetresources_0;

			void *crtcs = malloc(orig_crtc_size);
			if(orig_crtc_size && !crtcs) goto errgetresources_1;

			void *connectors = malloc(orig_connector_size);
			if(orig_connector_size && !connectors) goto errgetresources_2;

			void *encoders = malloc(orig_encoder_size);
			if(orig_encoder_size && !encoders) goto errgetresources_3;

			res.fb_id_ptr = (uintptr_t)fbs;
			res.crtc_id_ptr = (uintptr_t)crtcs;
			res.connector_id_ptr = (uintptr_t)connectors;
			res.encoder_id_ptr = (uintptr_t)encoders;

#ifdef DEBUG_OUTPUT
struct drm_mode_card_res res1 = res;
#endif
			int status = ioctl(c->card, DRM_IOCTL_MODE_GETRESOURCES, &res);
			if(status < 0) {
				err = status;
				goto errgetresources_4;
			}
#ifdef DEBUG_OUTPUT
printf("Resources:\n");
#define RES_ARRAY(Name, name) print_array("  " Name ":", name, res1.count_##name, res.count_##name)
RES_ARRAY("FBs", fbs);
RES_ARRAY("CRTCs", crtcs);
RES_ARRAY("Connectors", connectors);
RES_ARRAY("Encoders", encoders);
printf("  Width: from %d to %d\n  Height: from %d to %d\n",
		res.min_width, res.max_width, res.min_height, res.max_height);
#endif

			res.fb_id_ptr = orig_fbs;
			res.crtc_id_ptr = orig_crtcs;
			res.connector_id_ptr = orig_connectors;
			res.encoder_id_ptr = orig_encoders;

			ioctl_write_and_return(c, header_in, status, 5,
					&res, ioi->arg, sizeof res,
					fbs, orig_fbs, orig_fb_size,
					crtcs, orig_crtcs, orig_crtc_size,
					connectors, orig_connectors, orig_connector_size,
					encoders, orig_encoders, orig_encoder_size);
			err = 0;

errgetresources_4:	free(encoders);
errgetresources_3:	free(connectors);
errgetresources_2:	free(crtcs);
errgetresources_1:	free(fbs);
errgetresources_0:	if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
#endif
		}
		CASE(DRM_IOCTL_GET_CAP)
		{
			struct drm_get_cap cap;
			ioctl_read(c, header_in, &cap, ioi->arg, sizeof cap);
			int status = ioctl(c->card, DRM_IOCTL_GET_CAP, &cap);
			ioctl_write_and_return(c, header_in, status, 1, &cap, ioi->arg, sizeof cap);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETCRTC)
		{
			int err = -1;

			struct drm_mode_crtc crtc, crtc2;
			ioctl_read(c, header_in, &crtc, ioi->arg, sizeof crtc);

			uint32_t *connectors_ptr = malloc(sizeof *connectors_ptr * crtc.count_connectors);
			if(crtc.count_connectors && !connectors_ptr) goto errcrtc_0;

			crtc2 = crtc;
			crtc.set_connectors_ptr = (uintptr_t)connectors_ptr;

#ifdef DEBUG_OUTPUT
struct drm_mode_crtc crtc1 = crtc;
#endif
			int status = ioctl(c->card, DRM_IOCTL_MODE_GETCRTC, &crtc);
#ifdef DEBUG_OUTPUT
printf("CRTC: %d, "
		"Framebuffer: %d\n"
		"  Output Position: %d, %d\n"
		"  Mode valid: %d\n",
		crtc.crtc_id,
		crtc.fb_id,
		crtc.x, crtc.y,
		crtc.mode_valid);
if(crtc.mode_valid) print_mode(&crtc.mode);
print_array("  Connectors:", connectors_ptr, crtc1.count_connectors, crtc.count_connectors);
#endif

			crtc.set_connectors_ptr = crtc2.set_connectors_ptr;

			ioctl_write_and_return(c, header_in, status, 2,
					&crtc, ioi->arg, sizeof crtc,
					connectors_ptr, crtc.set_connectors_ptr, sizeof *connectors_ptr * crtc.count_connectors);

			err = 0;

			free(connectors_ptr);
errcrtc_0:		if(err) ioctl_write_and_return(c, header_in, -1, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETCONNECTOR)
		{
			int err = -1;

			struct drm_mode_get_connector con, con2;
			ioctl_read(c, header_in, &con, ioi->arg, sizeof con);

			uint32_t *encoders;
			uint32_t *props;
			uint64_t *prop_values;
			struct drm_mode_modeinfo *modes;

			unsigned olen_encoders = sizeof *encoders * con.count_encoders;
			unsigned olen_props = sizeof *props * con.count_props;
			unsigned olen_prop_values = sizeof *prop_values * con.count_props;
			unsigned olen_modes = sizeof *modes * con.count_modes;

			encoders = malloc(olen_encoders);
			if(olen_encoders && !encoders) goto errcon_0;

			props = malloc(olen_props);
			if(olen_props && !props) goto errcon_1;

			prop_values = malloc(olen_prop_values);
			if(olen_prop_values && !prop_values) goto errcon_2;

			modes = malloc(olen_modes);
			if(olen_modes && !modes) goto errcon_3;

			con2 = con;
			con.encoders_ptr = (uintptr_t)encoders;
			con.props_ptr = (uintptr_t)props;
			con.prop_values_ptr = (uintptr_t)prop_values;
			con.modes_ptr = (uintptr_t)modes;

#ifdef DEBUG_OUTPUT
struct drm_mode_get_connector con1 = con;
#endif
			int status = ioctl(c->card, DRM_IOCTL_MODE_GETCONNECTOR, &con);
#ifdef DEBUG_OUTPUT
printf("Connector %d:\n"
		"  Encoder: %d\n"
		"  Connector: %d\n"
		"  Connector Type: %d\n"
		"  Connector Type ID: %d\n"
		"  Connection: %d\n"
		"  Size in mm: %dx%d\n"
		"  Subpixel: %d\n"
		"  Number of Modes Supported: %d\n",
		con1.connector_id,
		con.encoder_id,
		con.connector_id,
		con.connector_type,
		con.connector_type_id,
		con.connection,
		con.mm_width, con.mm_height,
		con.subpixel,
		con.count_modes);
{
	int i, max = con.count_modes < con1.count_modes ? con.count_modes : con1.count_modes;
	for(i = 0; i < max; ++i) {
		print_mode(&modes[i]);
	}
}
#endif
//con.count_props = 0 works. Probably fine to ignore all properties.

			con.encoders_ptr = con2.encoders_ptr;
			con.props_ptr = con2.props_ptr;
			con.prop_values_ptr = con2.prop_values_ptr;
			con.modes_ptr = con2.modes_ptr;

			ioctl_write_and_return(c, header_in, status, 5,
					&con, ioi->arg, sizeof con,
					encoders, con.encoders_ptr, olen_encoders,
					props, con.props_ptr, olen_props,
					prop_values, con.prop_values_ptr, olen_prop_values,
					modes, con.modes_ptr, olen_modes);
			err = 0;

			free(modes);
errcon_3:		free(prop_values);
errcon_2:		free(props);
errcon_1:		free(encoders);
errcon_0:		if(err) ioctl_write_and_return(c, header_in, -1, 0);
			break;
		}
		CASE(DRM_IOCTL_SET_MASTER)
		{
			//int status = ioctl(c->card, DRM_IOCTL_SET_MASTER, NULL);
			//ioctl_write_and_return(c, header_in, status, 0);
			//c->flags |= HAVE_MASTER;
			ioctl_write_and_return(c, header_in, 0, 0);
			break;
		}
		CASE(DRM_IOCTL_DROP_MASTER)
		{
			//int status = ioctl(c->card, DRM_IOCTL_DROP_MASTER, NULL);
			//ioctl_write_and_return(c, header_in, status, 0);
			//c->flags &= ~HAVE_MASTER;
			ioctl_write_and_return(c, header_in, 0, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_CREATE_DUMB)
		{
			struct drm_mode_create_dumb dumb;
			ioctl_read(c, header_in, &dumb, ioi->arg, sizeof dumb);
			int status = ioctl(c->card, DRM_IOCTL_MODE_CREATE_DUMB, &dumb);
			ioctl_write_and_return(c, header_in, status, 1, &dumb, ioi->arg, sizeof dumb);
			break;
		}
		CASE(DRM_IOCTL_MODE_ADDFB)
		{
			struct drm_mode_fb_cmd cmd;
			ioctl_read(c, header_in, &cmd, ioi->arg, sizeof cmd);
			int status = ioctl(c->card, DRM_IOCTL_MODE_ADDFB, &cmd);
			add_fb(c, cmd.fb_id, cmd.handle);
			ioctl_write_and_return(c, header_in, status, 1, &cmd, ioi->arg, sizeof cmd);
			break;
		}
		CASE(DRM_IOCTL_MODE_RMFB)
		{
			unsigned int fb;
			ioctl_read(c, header_in, &fb, ioi->arg, sizeof fb);
			int status = ioctl(c->card, DRM_IOCTL_MODE_RMFB, &fb);
			remove_fb(c, fb);
			ioctl_write_and_return(c, header_in, status, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_DESTROY_DUMB)
		{
			struct drm_mode_destroy_dumb dumb;
			ioctl_read(c, header_in, &dumb, ioi->arg, sizeof dumb);
			int status = ioctl(c->card, DRM_IOCTL_MODE_DESTROY_DUMB, &dumb);
			ioctl_write_and_return(c, header_in, status, 0);
			break;
		}
		CASE(DRM_IOCTL_RADEON_INFO)
		{
			struct drm_radeon_info info;
			ioctl_read(c, header_in, &info, ioi->arg, sizeof info);
			uintptr_t client_addr = info.value;
			uint32_t val;
			ioctl_read(c, header_in, &val, client_addr, sizeof val);
			info.value = (uintptr_t)&val;
			int status = ioctl(c->card, DRM_IOCTL_RADEON_INFO, &info);
			ioctl_write_and_return(c, header_in, status, 2, &info, ioi->arg, sizeof info, &val, client_addr, sizeof val);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETENCODER)
		{
			struct drm_mode_get_encoder enc;
			ioctl_read(c, header_in, &enc, ioi->arg, sizeof enc);
#ifdef DEBUG_OUTPUT
struct drm_mode_get_encoder enc1 = enc;
#endif
			int status = ioctl(c->card, DRM_IOCTL_MODE_GETENCODER, &enc);
#ifdef DEBUG_OUTPUT
printf("Encoder %d:\n"
		"  Encoder ID: %d\n"
		"  Encoder Type: %d\n"
		"  CRTC: %d\n"
		"  Possible CRTCs: %d\n"
		"  Possible Clones: %d\n",
		enc1.encoder_id,
		enc.encoder_id,
		enc.encoder_type,
		enc.crtc_id,
		enc.possible_crtcs,
		enc.possible_clones);
#endif
			ioctl_write_and_return(c, header_in, status, 1, &enc, ioi->arg, sizeof enc);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETPROPERTY)
		{
			int err = -1;
			struct drm_mode_get_property prop;
			ioctl_read(c, header_in, &prop, ioi->arg, sizeof prop);

			uint64_t orig_values = prop.values_ptr;
			uint64_t orig_enums = prop.enum_blob_ptr;

			size_t values_size = 0;
			if(prop.count_enum_blobs && prop.flags & DRM_MODE_PROP_BLOB) {
				values_size = sizeof(uint32_t) * prop.count_enum_blobs;
			}
			else if(prop.count_values) {
				values_size = sizeof(uint64_t) * prop.count_values;
			}

			size_t enums_size = 0;
			if(prop.count_enum_blobs && prop.flags & DRM_MODE_PROP_BLOB) {
				enums_size = sizeof(uint32_t) * prop.count_enum_blobs;
			}
			else if(prop.count_enum_blobs && prop.flags & (DRM_MODE_PROP_ENUM | DRM_MODE_PROP_BITMASK)) {
				enums_size = sizeof(struct drm_mode_property_enum) * prop.count_enum_blobs;
			}

			uint64_t *values = malloc(values_size);
			if(values_size && !values) goto errgetproperty_0;

			void *enums = malloc(enums_size);
			if(enums_size && !enums) goto errgetproperty_1;

			prop.values_ptr = (uintptr_t)values;
			prop.enum_blob_ptr = (uintptr_t)enums;

			int status = ioctl(c->card, DRM_IOCTL_MODE_GETPROPERTY, &prop);

			prop.values_ptr = orig_values;
			prop.enum_blob_ptr = orig_enums;

			ioctl_write_and_return(c, header_in, status, 3,
					&prop, ioi->arg, sizeof prop,
					values, orig_values, values_size,
					enums, orig_enums, enums_size);
			err = 0;

			free(enums);
errgetproperty_1:	free(values);
errgetproperty_0:	if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETPROPBLOB)
		{
			int err = -1;
			struct drm_mode_get_blob blob;
			ioctl_read(c, header_in, &blob, ioi->arg, sizeof blob);

			uintptr_t orig_ptr = blob.data;
			size_t orig_size = blob.length;

			void *data = malloc(orig_size);
			if(orig_size && !data) goto errgetpropblob_0;

			int status = ioctl(c->card, DRM_IOCTL_MODE_GETPROPBLOB, &blob);
			if(status < 0) {
				err = status;
				goto errgetpropblob_1;
			}

			blob.data = orig_ptr;

			ioctl_write_and_return(c, header_in, status, 2,
					&blob, ioi->arg, sizeof blob,
					data, orig_ptr, orig_size);
			err = 0;

errgetpropblob_1:	free(data);
errgetpropblob_0:	if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_INFO)
		{
			int err = -1;
			struct drm_radeon_gem_info info;
			ioctl_read(c, header_in, &info, ioi->arg, sizeof info);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_INFO, &info);
			if(status < 0) {
				err = status;
				goto errgeminfo_0;
			}
			ioctl_write_and_return(c, header_in, status, 1, &info, ioi->arg, sizeof info);
			err = 0;
errgeminfo_0:		if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_CREATE)
		{
#if 1
			struct drm_radeon_gem_create create;
			ioctl_read(c, header_in, &create, ioi->arg, sizeof create);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_CREATE, &create);
			ioctl_write_and_return(c, header_in, status, 1, &create, ioi->arg, sizeof create);
			break;
#else
			struct drm_radeon_gem_create create;
			ioctl_read(c, header_in, &create, ioi->arg, sizeof create);
			break;
#endif
		}
		CASE(DRM_IOCTL_RADEON_GEM_MMAP)
		{
			struct drm_radeon_gem_mmap mm;
			ioctl_read(c, header_in, &mm, ioi->arg, sizeof mm);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_MMAP, &mm);
			ioctl_write_and_return(c, header_in, status, 1, &mm, ioi->arg, sizeof mm);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_WAIT_IDLE)
		{
			struct drm_radeon_gem_wait_idle idle;
			ioctl_read(c, header_in, &idle, ioi->arg, sizeof idle);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_WAIT_IDLE, &idle);
			ioctl_write_and_return(c, header_in, status, 1, &idle, ioi->arg, sizeof idle);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_SET_TILING)
		{
			struct drm_radeon_gem_set_tiling tiling;
			ioctl_read(c, header_in, &tiling, ioi->arg, sizeof tiling);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_SET_TILING, &tiling);
			ioctl_write_and_return(c, header_in, status, 1, &tiling, ioi->arg, sizeof tiling);
			break;
		}
		CASE(DRM_IOCTL_MODE_SETPROPERTY)
		{
			struct drm_mode_connector_set_property prop;
			ioctl_read(c, header_in, &prop, ioi->arg, sizeof prop);
			int status = ioctl(c->card, DRM_IOCTL_MODE_SETPROPERTY, &prop);
			ioctl_write_and_return(c, header_in, status, 1, &prop, ioi->arg, sizeof prop);
			break;
		}
		CASE(DRM_IOCTL_MODE_GETFB)
		{
			struct drm_mode_fb_cmd cmd;
			ioctl_read(c, header_in, &cmd, ioi->arg, sizeof cmd);
			int status = ioctl(c->card, DRM_IOCTL_MODE_GETFB, &cmd);
			ioctl_write_and_return(c, header_in, status, 1, &cmd, ioi->arg, sizeof cmd);
			break;
		}
		CASE(DRM_IOCTL_GEM_FLINK)
		{
			struct drm_gem_flink flink;
			ioctl_read(c, header_in, &flink, ioi->arg, sizeof flink);
			int status = ioctl(c->card, DRM_IOCTL_GEM_FLINK, &flink);
printf("flink: handle = %d, name = %d\n", flink.handle, flink.name);
			ioctl_write_and_return(c, header_in, status, 1, &flink, ioi->arg, sizeof flink);
			break;
		}
		CASE(DRM_IOCTL_GEM_OPEN)
		{
			struct drm_gem_open open;
			ioctl_read(c, header_in, &open, ioi->arg, sizeof open);
			int status = ioctl(c->card, DRM_IOCTL_GEM_OPEN, &open);
			ioctl_write_and_return(c, header_in, status, 1, &open, ioi->arg, sizeof open);
			break;
		}
		CASE(DRM_IOCTL_GEM_CLOSE)
		{
			struct drm_gem_close clos;
			ioctl_read(c, header_in, &clos, ioi->arg, sizeof clos);
			int status = ioctl(c->card, DRM_IOCTL_GEM_CLOSE, &clos);
			ioctl_write_and_return(c, header_in, status, 1, &clos, ioi->arg, sizeof clos);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_GET_TILING)
		{
			struct drm_radeon_gem_get_tiling tiling;
			ioctl_read(c, header_in, &tiling, ioi->arg, sizeof tiling);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_GET_TILING, &tiling);
			ioctl_write_and_return(c, header_in, status, 1, &tiling, ioi->arg, sizeof tiling);
			break;
		}
		CASE(DRM_IOCTL_RADEON_CS)
		{
			int err = -1;
			struct drm_radeon_cs cs, cs1;
			ioctl_read(c, header_in, &cs, ioi->arg, sizeof cs);

			uint64_t *chunks_array;
			unsigned chunks_array_size = sizeof *chunks_array * cs.num_chunks;
			chunks_array = malloc(chunks_array_size);
			if(!chunks_array) goto err_cs_0;

			ioctl_read(c, header_in, chunks_array, cs.chunks, chunks_array_size);

			struct drm_radeon_cs_chunk *chunks;
			unsigned chunks_size = sizeof *chunks * cs.num_chunks;
			chunks = malloc(chunks_size);
			if(!chunks_array) goto err_cs_1;

			unsigned i;
			for(i = 0; i < cs.num_chunks; ++i) {
				ioctl_read(c, header_in, &chunks[i], chunks_array[i], sizeof chunks[i]);
			}

			struct {
				void *data;
				unsigned size;
			} *chunk_datas;
			unsigned chunk_datas_size = sizeof *chunk_datas * cs.num_chunks;
			chunk_datas = malloc(chunk_datas_size);
			if(!chunk_datas) goto err_cs_2;

			for(i = 0; i < cs.num_chunks; ++i) {
				chunk_datas[i].size = chunks[i].length_dw * 4;
				chunk_datas[i].data = malloc(chunk_datas[i].size);
				if(!chunk_datas[i].data) goto err_cs_3;
				ioctl_read(c, header_in, chunk_datas[i].data, chunks[i].chunk_data, chunk_datas[i].size);
			}

			for(i = 0; i < cs.num_chunks; ++i) {
				chunks_array[i] = (uintptr_t)&chunks[i];
				chunks[i].chunk_data = (uintptr_t)chunk_datas[i].data;
			}

			cs1 = cs;
			cs1.chunks = (uintptr_t)chunks_array;
			int status = ioctl(c->card, DRM_IOCTL_RADEON_CS, &cs1);

			ioctl_write_and_return(c, header_in, status, 1, &cs, ioi->arg, sizeof cs);

			err = 0;

			i = cs.num_chunks;
			while(i) {
				--i;
				free(chunk_datas[i].data);
			}
err_cs_3:		free(chunk_datas);
err_cs_2:		free(chunks);
err_cs_1:		free(chunks_array);
err_cs_0:		if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_SETGAMMA)
		{
			int err = -1;
			struct drm_mode_crtc_lut lut;
			ioctl_read(c, header_in, &lut, ioi->arg, sizeof lut);

			void *red = malloc(lut.gamma_size);
			if(!red) goto err_gamma_0;
			void *green = malloc(lut.gamma_size);
			if(!green) goto err_gamma_1;
			void *blue = malloc(lut.gamma_size);
			if(!blue) goto err_gamma_2;

			ioctl_read(c, header_in, red, lut.red, lut.gamma_size);
			ioctl_read(c, header_in, green, lut.green, lut.gamma_size);
			ioctl_read(c, header_in, blue, lut.blue, lut.gamma_size);

			lut.red = (uintptr_t)red;
			lut.green = (uintptr_t)green;
			lut.blue = (uintptr_t)blue;

			int status = ioctl(c->card, DRM_IOCTL_MODE_SETGAMMA, &lut);

			ioctl_write_and_return(c, header_in, status, 0);
			err = 0;

			free(blue);
err_gamma_2:		free(green);
err_gamma_1:		free(red);
err_gamma_0:		if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
		}
		CASE(DRM_IOCTL_MODE_SETCRTC)
		{
#if 1
			struct drm_mode_crtc crtc;
			ioctl_read(c, header_in, &crtc, ioi->arg, sizeof crtc);
			set_fb_size(c, crtc.fb_id, crtc.mode.hdisplay,
					crtc.mode.vdisplay);
if(!crtc.mode.hdisplay)goto lol;
//#define D(name) printf(#name ": %d\n", crtc.mode.name)
#define D(name)
D(clock);
D(hdisplay);
D(hsync_start);
D(hsync_end);
D(htotal);
D(hskew);
D(vdisplay);
D(vsync_start);
D(vsync_end);
D(vtotal);
D(vscan);
D(vrefresh);
D(flags);
D(type);
#undef D
//printf("\n");
			c->framebuffer(c->user, fb_to_name(c, crtc.fb_id),
					crtc.mode.hdisplay,
					crtc.mode.vdisplay);
lol:
			ioctl_write_and_return(c, header_in, 0, 0);
			break;
#else
			int err = -1;
			struct drm_mode_crtc crtc;
			ioctl_read(c, header_in, &crtc, ioi->arg, sizeof crtc);
printf("Setting crtc: fb = %d, name = %d\n", crtc.fb_id, fb_to_name(c, crtc.fb_id));

			unsigned connectors_size;
			uint32_t *connectors = NULL;
			if(crtc.count_connectors) {
				connectors_size = sizeof *connectors * crtc.count_connectors;
				connectors = malloc(connectors_size);
				if(!connectors) goto err_setcrtc_0;
				ioctl_read(c, header_in, connectors, crtc.set_connectors_ptr, connectors_size);
			}

			crtc.set_connectors_ptr = (uintptr_t)connectors;
			int status = ioctl(c->card, DRM_IOCTL_MODE_SETCRTC, &crtc);

			ioctl_write_and_return(c, header_in, status, 0);
			err = 0;

			free(connectors);
err_setcrtc_0:		if(err) ioctl_write_and_return(c, header_in, err, 0);
			break;
#endif
		}
		CASE(DRM_IOCTL_WAIT_VBLANK)
		{
#if 1
			printf("Wait for vblank not implemented.\n");
			ioctl_write_and_return(c, header_in, -1, 0);
			break;
#else
			union drm_wait_vblank u;
			ioctl_read(c, header_in, &u, ioi->arg, sizeof u);
			int status = ioctl(c->card, DRM_IOCTL_WAIT_VBLANK, &u);
			ioctl_write_and_return(c, header_in, status, 1, &u, ioi->arg, sizeof u);
			break;
#endif
		}
		CASE(DRM_IOCTL_RADEON_GEM_BUSY)
		{
			struct drm_radeon_gem_busy busy;
			ioctl_read(c, header_in, &busy, ioi->arg, sizeof busy);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_BUSY, &busy);
			ioctl_write_and_return(c, header_in, status, 1, &busy, ioi->arg, sizeof busy);
			break;
		}
		CASE(DRM_IOCTL_RADEON_GEM_OP)
		{
			struct drm_radeon_gem_op op;
			ioctl_read(c, header_in, &op, ioi->arg, sizeof op);
			int status = ioctl(c->card, DRM_IOCTL_RADEON_GEM_OP, &op);
			ioctl_write_and_return(c, header_in, status, 1, &op, ioi->arg, sizeof op);
			break;
		}
		CASE(DRM_IOCTL_MODE_PAGE_FLIP)
		{
#if 1
			struct drm_mode_crtc_page_flip flip;
			ioctl_read(c, header_in, &flip, ioi->arg, sizeof flip);
			if(flip.flags & DRM_MODE_PAGE_FLIP_EVENT) {
				c->user_data = flip.user_data;
				c->flags |= WAITING_FOR_PAGE_FLIP_EVENT;
			}
goto olo;
			c->framebuffer(c->user, fb_to_name(c, flip.fb_id),
					fb_w(c, flip.fb_id),
					fb_h(c, flip.fb_id));
olo:
			ioctl_write_and_return(c, header_in, 0, 0);
			break;
#else
			struct drm_mode_crtc_page_flip flip;
			ioctl_read(c, header_in, &flip, ioi->arg, sizeof flip);
			/* Flip has been requested */
			c->user_data = flip.user_data;
			int status = ioctl(c->card, DRM_IOCTL_MODE_PAGE_FLIP, &flip);
printf("page flip: fb = %d, name = %d\n", flip.fb_id, fb_to_name(c, flip.fb_id));
			ioctl_write_and_return(c, header_in, 0, 1, &flip, ioi->arg, sizeof flip);
			break;
#endif
		}
		/* DRM_IOCTL_RADEON_GEM_USERPTR (not present in my headers) */
		CASE(0xc018646d)
		{
			struct drm_radeon_gem_userptr {
				uint64_t addr;
				uint64_t size;
				uint32_t flags;
				uint32_t handle;
			} userptr;
			ioctl_read(c, header_in, &userptr, ioi->arg,
					sizeof userptr);
			int status = ioctl(c->card, 0xc018646d, &userptr);
			ioctl_write_and_return(c, header_in, status, 1,
					&userptr, ioi->arg,
					sizeof userptr);
			break;
		}
		CASE(DRM_IOCTL_MODE_CURSOR)
		{
			struct drm_mode_cursor cursor;
			ioctl_read(c, header_in, &cursor, ioi->arg, sizeof cursor);
			int status = ioctl(c->card, DRM_IOCTL_MODE_CURSOR, &cursor);
			ioctl_write_and_return(c, header_in, status, 1, &cursor, ioi->arg, sizeof cursor);
			break;
		}
		CASE(DRM_IOCTL_MODE_CURSOR2)
		{
			struct drm_mode_cursor2 cursor2;
			ioctl_read(c, header_in, &cursor2, ioi->arg, sizeof cursor2);
			int status = ioctl(c->card, DRM_IOCTL_MODE_CURSOR2, &cursor2);
			ioctl_write_and_return(c, header_in, status, 1, &cursor2, ioi->arg, sizeof cursor2);
			break;
		}
		CASE(DRM_IOCTL_MODE_MAP_DUMB)
		{
			struct drm_mode_map_dumb dumb;
			ioctl_read(c, header_in, &dumb, ioi->arg, sizeof dumb);
			int status = ioctl(c->card, DRM_IOCTL_MODE_MAP_DUMB, &dumb);
			ioctl_write_and_return(c, header_in, status, 1, &dumb, ioi->arg, sizeof dumb);
			break;
		}
		CASE(DRM_IOCTL_MODE_DIRTYFB)
		{
			struct drm_mode_fb_dirty_cmd dirty;
			ioctl_read(c, header_in, &dirty, ioi->arg, sizeof dirty);
			int status = ioctl(c->card, DRM_IOCTL_MODE_DIRTYFB, &dirty);
			ioctl_write_and_return(c, header_in, status, 1, &dirty, ioi->arg, sizeof dirty);
			break;
		}
		default:
		{
static unsigned first = 1;
if(first) {
#if 0
printf("\nUnknown ioctl: %x\n"
"  Direction:\t%s%s\n"
"  Type:\t\t%d%6$s\n"
"  Number:\t%d (%5$x)\n\n",
//  " (%s%s (%d %d) %x)\n",
ioi->cmd,
(ioi->cmd >> _IOC_DIRSHIFT) & ((1 << _IOC_DIRBITS) - 1) & 2 ? "R" : "",
(ioi->cmd >> _IOC_DIRSHIFT) & _IOC_DIRBITS & 1 ? "W" : "",
(ioi->cmd >> _IOC_TYPESHIFT) & ((1 << _IOC_TYPEBITS) - 1),
//  DRM_IOCTL_BASE,
(ioi->cmd >> _IOC_NRSHIFT) & ((1 << _IOC_NRBITS) - 1),
((ioi->cmd >> _IOC_TYPESHIFT) & ((1 << _IOC_TYPEBITS) - 1)) == DRM_IOCTL_BASE ? " (DRM_IOCTL_BASE)" : " (unknown)");
#else
printf("\nUnknown ioctl: %x\n", ioi->cmd);
#endif
sleep(5);
first = 0;
}
			ioctl_write_and_return(c, header_in, -1, 0);
			break;
		}
	}
#undef CASE
	return 0;
}

/*
 * Simplifying the retrying thing.
 */

#define MAX_SEGMENTS 10

struct retry {
	struct card *c;
	int out;
	char *buf, *buf0;
	size_t buf_len, buf0_len;
	size_t out_len;
	uint64_t unique;
	uint64_t inarg;
	int iov_len;
	struct fuse_ioctl_iovec iov[MAX_SEGMENTS];
	void *segments[MAX_SEGMENTS];
};

static void retry_init(struct retry *r, struct card *c, uint64_t unique,
		struct fuse_ioctl_in *ioi, char *buf)
{
	r->c = c;
	r->out = 0;
	r->buf0 = r->buf = buf;
	r->buf0_len = r->buf_len = ioi->in_size;
	r->out_len = ioi->out_size;
	r->unique = unique;
	r->inarg = ioi->arg;
	r->iov_len = 0;
}

static void retry_read(struct retry *r, void *out, uint64_t ptr, size_t sz)
{
	assert(r->iov_len < sizeof r->iov / sizeof r->iov[0]);
	if(r->out) return;
	if(r->buf_len < sz) {
		r->out = 1;
	}
	else {
		memcpy(out, r->buf, sz);
		r->buf += sz;
		r->buf_len -= sz;
	}
	r->iov[r->iov_len].base = ptr;
	r->iov[r->iov_len].len = sz;
	r->segments[r->iov_len] = out;
	++r->iov_len;
}

static int retry_checkpoint(struct retry *r)
{
	if(!r->out) return 0;

	assert(r->out_len == 0);

	struct fuse_out_header oh;
	oh.error = 0;
	oh.unique = r->unique;

	struct fuse_ioctl_out ioo;
	ioo.result = 0;
	ioo.flags = FUSE_IOCTL_RETRY;
	ioo.in_iovs = r->iov_len;
	ioo.out_iovs = 0;

	struct iovec iov[3];

	iov[0].iov_base = &oh;
	iov[0].iov_len = sizeof oh;
	iov[1].iov_base = &ioo;
	iov[1].iov_len = sizeof ioo;
	iov[2].iov_base = r->iov;
	iov[2].iov_len = sizeof r->iov[0] * r->iov_len;

	oh.len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

	int status = writev(r->c->fd, iov, 3);
	if(status < 0) {
		fprintf(stderr, "Error writing ioctl response to CUSE: %s.\n",
				strerror(errno));
	}

	return 1;
}

static int retry_finish(struct retry *r)
{
	if(!r->out) return 0;

	assert(r->out_len == 0);

	struct fuse_out_header oh;
	oh.error = 0;
	oh.unique = r->unique;

	struct fuse_ioctl_out ioo;
	ioo.result = 0;
	ioo.flags = FUSE_IOCTL_RETRY;
	ioo.in_iovs = r->iov_len;
	ioo.out_iovs = r->iov_len;

	struct iovec iov[4];

	iov[0].iov_base = &oh;
	iov[0].iov_len = sizeof oh;
	iov[1].iov_base = &ioo;
	iov[1].iov_len = sizeof ioo;
	iov[2].iov_base = r->iov;
	iov[2].iov_len = sizeof r->iov[0] * r->iov_len;
	iov[3].iov_base = r->iov;
	iov[3].iov_len = sizeof r->iov[0] * r->iov_len;

	oh.len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len +
		iov[3].iov_len;

	int status = writev(r->c->fd, iov, 4);
	if(status < 0) {
		fprintf(stderr, "Error writing ioctl response to CUSE: %s.\n",
				strerror(errno));
	}

	return 1;
}

static void retry_error(struct retry *r)
{
	struct fuse_out_header oh;
	oh.error = 0;
	oh.unique = r->unique;

	struct fuse_ioctl_out ioo;
	ioo.result = -EIO;
	ioo.flags = 0;
	ioo.in_iovs = 0;
	ioo.out_iovs = 0;

	struct iovec iov[3];

	iov[0].iov_base = &oh;
	iov[0].iov_len = sizeof oh;
	iov[1].iov_base = &ioo;
	iov[1].iov_len = sizeof ioo;
	iov[2].iov_base = r->buf0;
	iov[2].iov_len = r->buf0_len;

	oh.len = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

	int status = writev(r->c->fd, iov, 3);
	if(status < 0) {
		fprintf(stderr, "Error writing ioctl response to CUSE: %s.\n",
				strerror(errno));
	}
}

static void retry_return(struct retry *r, int return_value)
{
	struct fuse_out_header oh;
	oh.error = 0;
	oh.unique = r->unique;

	struct fuse_ioctl_out ioo;
	ioo.result = return_value;
	ioo.flags = 0;
	ioo.in_iovs = 0;
	ioo.out_iovs = 0;

	struct iovec iov[2 + sizeof r->iov / sizeof r->iov[0]];

	iov[0].iov_base = &oh;
	iov[0].iov_len = sizeof oh;
	iov[1].iov_base = &ioo;
	iov[1].iov_len = sizeof ioo;

	oh.len = iov[0].iov_len + iov[1].iov_len;

	size_t out_len = 0;
	int i;
	for(i = 0; i < r->iov_len; ++i) {
		void *ptr = r->segments[i];
		size_t sz = r->iov[i].len;
		iov[2 + i].iov_base = ptr;
		iov[2 + i].iov_len = sz;
		out_len += sz;
	}
	assert(out_len == r->out_len);
	oh.len += out_len;

	int status = writev(r->c->fd, iov, 2 + i);
	if(status < 0) {
		fprintf(stderr, "Error writing ioctl response to CUSE: %s.\n",
				strerror(errno));
	}
}

/*
 * Specific ioctls
 */

static int version(struct retry *r)
{
	struct drm_version vers;
	retry_read(r, &vers, r->inarg, sizeof vers);
	if(retry_checkpoint(r)) return 0;

	char name[4096], date[4096], desc[4096];
	if(vers.name_len > sizeof name
			|| vers.date_len > sizeof date
			|| vers.desc_len > sizeof desc) {
		retry_error(r);
		return 0;
	}

	retry_read(r, name, (uint64_t)vers.name, vers.name_len);
	retry_read(r, date, (uint64_t)vers.date, vers.date_len);
	retry_read(r, desc, (uint64_t)vers.desc, vers.desc_len);
	if(retry_finish(r)) return 0;

	char *name1 = vers.name;
	char *date1 = vers.date;
	char *desc1 = vers.desc;
	vers.name = name;
	vers.date = date;
	vers.desc = desc;
	int status = ioctl(r->c->card, DRM_IOCTL_VERSION, &vers);
	vers.name = name1;
	vers.date = date1;
	vers.desc = desc1;
	printf("Version: %d = \"%s\" \"%s\" \"%s\"\n", name, date, desc);

	retry_return(r, status);
	return 0;
}

static int set_version(struct retry *r)
{
	struct drm_set_version dsv;
	retry_read(r, &dsv, r->inarg, sizeof dsv);
	if(retry_finish(r)) return 0;

	int status = ioctl(r->c->card, DRM_IOCTL_SET_VERSION, &dsv);

	retry_return(r, status);
	return 0;
}

static int get_unique(struct retry *r)
{
	struct drm_unique uniq;
	retry_read(r, &uniq, r->inarg, sizeof uniq);
	if(retry_finish(r)) return 0;

	uniq.unique_len = 0;

	retry_return(r, 0);
	return 0;
}

static int get_resources(struct retry *r)
{
	printf("Get Resources!\n");

	struct drm_mode_card_res res;
	retry_read(r, &res, r->inarg, sizeof res);
	if(retry_checkpoint(r)) return 0;

	size_t fb_size = sizeof(uint32_t) * res.count_fbs;
	size_t crtc_size = sizeof(uint32_t) * res.count_crtcs;
	size_t connector_size = sizeof(uint32_t) * res.count_connectors;
	size_t encoder_size = sizeof(uint32_t) * res.count_encoders;

	uint32_t fbs[1024], crtcs[1024], connectors[1024], encoders[1024];

	if(fb_size > sizeof fbs
			|| crtc_size > sizeof crtcs
			|| connector_size > sizeof connectors
			|| encoder_size > sizeof encoders) {
		retry_error(r);
		return 0;
	}

	retry_read(r, fbs, res.fb_id_ptr, fb_size);
	retry_read(r, crtcs, res.crtc_id_ptr, crtc_size);
	retry_read(r, connectors, res.connector_id_ptr, connector_size);
	retry_read(r, encoders, res.encoder_id_ptr, encoder_size);
	if(retry_finish(r)) return 0;

	retry_error(r);
	return 0;
}

/*
 * Device actions
 */

static int respond_to_dri_ioctl(struct card *c,
		struct fuse_in_header *ih, void *data)
{
	struct retry retry, *r;
	struct fuse_ioctl_in *ioi = data;
	retry_init(&retry, c, ih->unique, ioi, (char *)data + sizeof *ioi);
	r = &retry;
	switch(ioi->cmd) {
		case DRM_IOCTL_SET_VERSION: return set_version(r);
		case DRM_IOCTL_GET_UNIQUE: return get_unique(r);
		case DRM_IOCTL_VERSION: return version(r);
		case DRM_IOCTL_MODE_GETRESOURCES: return get_resources(r);
		default:
			retry_error(r);
			printf("Unknown ioctl.\n");

#define CASE(name) if(ioi->cmd == name) printf("The ioctl was " #name ".\n");
		CASE(DRM_IOCTL_VERSION)
		CASE(DRM_IOCTL_MODE_GETRESOURCES)
		CASE(DRM_IOCTL_GET_CAP)
		CASE(DRM_IOCTL_MODE_GETCRTC)
		CASE(DRM_IOCTL_MODE_GETCONNECTOR)
		CASE(DRM_IOCTL_SET_MASTER)
		CASE(DRM_IOCTL_DROP_MASTER)
		CASE(DRM_IOCTL_MODE_CREATE_DUMB)
		CASE(DRM_IOCTL_MODE_ADDFB)
		CASE(DRM_IOCTL_MODE_RMFB)
		CASE(DRM_IOCTL_MODE_DESTROY_DUMB)
		CASE(DRM_IOCTL_RADEON_INFO)
		CASE(DRM_IOCTL_MODE_GETENCODER)
		CASE(DRM_IOCTL_MODE_GETPROPERTY)
		CASE(DRM_IOCTL_MODE_GETPROPBLOB)
		CASE(DRM_IOCTL_RADEON_GEM_INFO)
		CASE(DRM_IOCTL_RADEON_GEM_CREATE)
		CASE(DRM_IOCTL_RADEON_GEM_MMAP)
		CASE(DRM_IOCTL_RADEON_GEM_WAIT_IDLE)
		CASE(DRM_IOCTL_RADEON_GEM_SET_TILING)
		CASE(DRM_IOCTL_MODE_SETPROPERTY)
		CASE(DRM_IOCTL_MODE_GETFB)
		CASE(DRM_IOCTL_GEM_FLINK)
		CASE(DRM_IOCTL_GEM_OPEN)
		CASE(DRM_IOCTL_GEM_CLOSE)
		CASE(DRM_IOCTL_RADEON_GEM_GET_TILING)
		CASE(DRM_IOCTL_RADEON_CS)
		CASE(DRM_IOCTL_MODE_SETGAMMA)
		CASE(DRM_IOCTL_MODE_SETCRTC)
		CASE(DRM_IOCTL_WAIT_VBLANK)
		CASE(DRM_IOCTL_RADEON_GEM_BUSY)
		CASE(DRM_IOCTL_RADEON_GEM_OP)
		CASE(DRM_IOCTL_MODE_PAGE_FLIP)
		CASE(0xc018646d)
		CASE(DRM_IOCTL_MODE_CURSOR)
		CASE(DRM_IOCTL_MODE_CURSOR2)
		CASE(DRM_IOCTL_MODE_MAP_DUMB)
		CASE(DRM_IOCTL_MODE_DIRTYFB)
		CASE(DRM_IOCTL_VERSION)
#undef CASE
	}

	return 0;
}

static int respond_to_dri_open(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
	//struct fuse_open_in *in = data;

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;

	struct fuse_open_out out;
	memset(&out, 0, sizeof out);
	out.fh = 1;
	out.open_flags = 0;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &out, .iov_len = sizeof out }
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	return writev(c->fd, iov, 2);
}

static int respond_to_dri_release(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
	//struct fuse_release_in *in = data;

	//if(c->flags & HAVE_MASTER) ioctl(c->card, DRM_IOCTL_DROP_MASTER, NULL);
	//c->flags &= ~HAVE_MASTER;

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;

	struct iovec iov[1] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out }
	};
	header_out.len = iov[0].iov_len;

	return writev(c->fd, iov, 1);
}

static int respond_to_dri_mmap(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
	struct fuse_mmap_in *mmap_in = data;

	void *map = mmap(
			NULL,
			mmap_in->length,
			mmap_in->prot,
			mmap_in->flags,
			c->card,
			mmap_in->offset);

	assert(map != MAP_FAILED);

	struct fuse_mmap_out mmap_out;
	mmap_out.pointer = (uint64_t)map;

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &mmap_out, .iov_len = sizeof mmap_out },
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	int status = writev(c->fd, iov, 2);
	if(status < 0) {
		printf("error :(\n");
	}

//TODO uncomment
//	munmap(map, mmap_in->length);

	return status;
}

static int respond_to_dri_poll(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
	struct fuse_poll_in *in = data;
	c->kh = in->kh;

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;

	struct fuse_poll_out out;
	memset(&out, 0, sizeof out);

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &out, .iov_len = sizeof out }
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	if(c->flags & CARD_READABLE) {
		out.revents = POLLIN;
	}
	else {
		out.revents = 0;
		c->flags |= CLIENT_POLLING;
	}

	return writev(c->fd, iov, 2);
}

static int respond_to_dri_read(struct card *c,
		struct fuse_in_header *header_in, void *data)
{
//	struct fuse_read_in *in = data;

	struct fuse_out_header header_out;
	memset(&header_out, 0, sizeof header_out);
	header_out.error = 0;
	header_out.unique = header_in->unique;

//TODO: what if c->flags & CARD_READABLE == 0?

	assert(c->flags & CARD_READABLE);
	c->flags &= ~CARD_READABLE;

	struct drm_event_vblank vblank;
	vblank.base.length = sizeof vblank;
	vblank.base.type = DRM_EVENT_FLIP_COMPLETE;
	vblank.user_data = c->user_data;
//TODO: not this
	vblank.tv_sec = 0;
	vblank.tv_usec = 0;
	vblank.sequence = 0;
	vblank.reserved = 0;

	struct iovec iov[2] = {
		{ .iov_base = &header_out, .iov_len = sizeof header_out },
		{ .iov_base = &vblank, .iov_len = sizeof vblank },
	};
	header_out.len = iov[0].iov_len + iov[1].iov_len;

	int status = writev(c->fd, iov, 2);

	return status;
}

static void cuse_process(void *user, int event)
{
	struct card *c = user;

	struct fuse_in_header header_in;
	char buf[BUFSZ];
	struct iovec iov[2] = {
		{ .iov_base = &header_in, .iov_len = sizeof header_in },
		{ .iov_base = buf, .iov_len = sizeof buf }
	};

	int status = readv(c->fd, iov, 2);
	if(status < 0 && errno == EAGAIN) {
		return;
	}
	if(status < 0) {
		fprintf(stderr, "Could not read message from /dev/cuse: %s.\n",
				strerror(errno));
		return;
	}

	request_context(c, &header_in, buf);
}

void card_send_resources(struct card *c, uint64_t unique,
		struct session_dri_resources *resources)
{
printf("!!! SENDING RESOURCES NOT IMPLEMENTET !!!\n");
}

static void request_context(struct card *c, struct fuse_in_header *ih,
		char *buf)
{
	if(ih->opcode == FUSE_OPEN) {
		if(respond_to_dri_open(c, ih, buf) < 0) goto err;
	}
	else if(ih->opcode == FUSE_RELEASE) {
		if(respond_to_dri_release(c, ih, buf) < 0) goto err;
	}
	else if(ih->opcode == FUSE_IOCTL) {
		if(respond_to_dri_ioctl(c, ih, buf) < 0) goto err;
	}
	else if(ih->opcode == FUSE_MMAP) {
		if(respond_to_dri_mmap(c, ih, buf) < 0) goto err;
	}
	else if(ih->opcode == FUSE_POLL) {
		if(respond_to_dri_poll(c, ih, buf) < 0) goto err;
	}
	else if(ih->opcode == FUSE_READ) {
		if(respond_to_dri_read(c, ih, buf) < 0) goto err;
	}
	else {
		fprintf(stderr, "Unsupported FUSE operation: %d\n",
				ih->opcode);
	}
	return;
err:
	assert(0);
}