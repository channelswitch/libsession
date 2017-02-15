#define _GNU_SOURCE
#include "symlinks.h"
#include "tty.h"
#include "umountfs.h"
#include "card.h"
#include "message.h"
#include "../protocol.h"
#include "../lib/dri.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdarg.h>
#include <assert.h>

struct data;
struct card_fd;

static void *register_fd(void *user, int fd, unsigned flags,
		void (*f)(void *user1, int event), void *user1);
static void unregister_fd(void *ptr);
static int filter(void *user, char *dev_path, struct stat *stat);
static void sigfd_event(void *user, int event);
static void socket_event(void *user, int event);
static void handle_message(struct data *data, struct message_in *msg);
static int forked_proc(void *user);
static int make_tmpdir(char *dir);
static void notify_umount(void *user);
static void send_header(struct data *data, struct message_header_out *header);
//static void framebuffer_callback(void *user, uint32_t name, int w, int h);
//static void card_wants_resources(void *user, uint64_t unique);
static void remove_outside_ref(struct data *data);

struct data {
	int socket_fd;
	int epoll_fd;

	int sigfd;

	struct symlinks *s;
	struct tty *tty;
	struct umountfs *efs;

	uid_t real_uid;
	gid_t real_gid;

	pid_t child;

	enum {
		SESSION_REF = 1 << 0,
		OUTSIDE_REF = 1 << 1,
		UMOUNTFS_PRESENT_OUTSIDE_NAMESPACE = 1 << 2,
		HAVE_FORKED = 1 << 3,
	} flags;

	/* Only used for storage when calling forked_proc. */
	char *path, **argv;

	char *nothing_mountpoint;
	char *old_dev;
	char *new_dev;

	char *card_device;
	char *tty_device;

	int card_fd;
	struct card *card;
};

struct registered_fd {
	struct data *data;
	int fd;
	void (*f)(void *user, int event);
	void *user1;
};	

int main(int argc, char **argv)
{
	int r = 1;
	int init_failed = 1;
	struct data data;
	data.child = 0;
	data.flags = OUTSIDE_REF;

	char *libsession_debug_socket = getenv("LIBSESSION_DEBUG_SOCKET");
	if(libsession_debug_socket) {
		/* Debug mode */
		data.socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if(data.socket_fd == -1) {
			fprintf(stderr, "Could not create socket: %s.\n",
					strerror(errno));
			return 1;
		}

		struct sockaddr_un addr;
		memset(&addr, 0, sizeof addr);
		if(strlen(libsession_debug_socket) + 1 > sizeof addr.sun_path) {
			fprintf(stderr, "Socket path (\"%s\") too long.\n",
					libsession_debug_socket);
			return 1;
		}
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, libsession_debug_socket);
		if(connect(data.socket_fd, &addr, sizeof addr) == -1) {
			fprintf(stderr, "Error connecting to \"%s\": %s.\n",
					libsession_debug_socket,
					strerror(errno));
			return 1;
		}
	}
	else if(argc == 2) {
		/* Invoked by libsession. Fd is inherited and given as arg. */
		data.socket_fd = atoi(argv[1]);
	}
	else {
		printf("Session-master should normally only be launched by "
				"libsession.\n\nIf you wish to debug "
				"session-master or libsession, first run the "
				"program that\nuses libsession with the "
				"environment variable LIBSESSION_DEBUG_SOCKET "
				"set.\nLibsession will create a Unix Domain "
				"socket where LIBSESSION_DEBUG_SOCKET\npoints "
				"and wait for session-master to connect. "
				"Then run session-master with the\nsame "
				"environment variable and execution will "
				"continue as normal. This lets you\nstart "
				"either program in GDB.\n");
		goto e_args;
	}

	/*
	 * Check that it is a Unix Domain socket. Can't think of how failure
	 * to check this could be exploited, but you never know.
	 */

	struct sockaddr socket_addr;
	socklen_t alen = sizeof socket_addr;
	if(getsockname(data.socket_fd, &socket_addr, &alen) != 0) goto e_args;
	if(socket_addr.sa_family != AF_UNIX) goto e_args;

	/* Epoll stuff */
	data.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if(data.epoll_fd < 0) goto e_epoll;

	/* Register socket fd. */
	void *socket_fd_ptr = register_fd(&data, data.socket_fd, 7,
			socket_event, &data);
	if(!socket_fd_ptr) goto e_register_socket;

	/* Intercept SIGTERM and SIGINT to clean up in /tmp and SIGCHLD to
	 * notify the user of session exit. */
	sigset_t sigfd_signals;
	sigemptyset(&sigfd_signals);
	sigaddset(&sigfd_signals, SIGHUP);
	sigaddset(&sigfd_signals, SIGINT);
	sigaddset(&sigfd_signals, SIGTERM);
	sigaddset(&sigfd_signals, SIGPIPE);
	sigaddset(&sigfd_signals, SIGCHLD);
	data.sigfd = signalfd(-1, &sigfd_signals, SFD_CLOEXEC);
	if(data.sigfd == -1) goto e_sigfd;
	void *sigfd_ptr = register_fd(&data, data.sigfd, 1, sigfd_event, &data);
	if(!sigfd_ptr) goto e_register_sigfd;
	if(sigprocmask(SIG_SETMASK, &sigfd_signals, NULL) == -1)
		goto e_sigblock;

	/* Set UID. */
	uid_t ue, us;
	if(getresuid(&data.real_uid, &ue, &us)) goto e_setuid;
	if(setresuid(0, 0, 0)) goto e_setuid;
	gid_t ge, gs;
	if(getresgid(&data.real_gid, &ge, &gs)) goto e_setuid;
	if(setresgid(0, 0, 0)) goto e_setuid;

	/* Don't want the socket to be inherited by the session. */
	if(fcntl(data.socket_fd, F_SETFD, FD_CLOEXEC)) goto e_fcntl;

	/* Make directories in /tmp. */
	static char template[] = "/tmp/session-XXXXXX";
	char tmpdir[sizeof template];
	strcpy(tmpdir, template);
	if(mkdtemp(tmpdir) == NULL) {
		fprintf(stderr, "Could not create directory in /tmp: %s.\n",
				strerror(errno));
		goto e_mkdtemp;
	}
	if(chmod(tmpdir, 0755) < 0) {
		fprintf(stderr, "Could not set permissions on %s: %s.",
				tmpdir, strerror(errno));
		goto e_chmod;
	}
	static char olddev1[] = "/old_dev";
	char olddev[sizeof tmpdir - 1 + sizeof olddev1 - 1 + 1];
	strcpy(olddev, tmpdir);
	strcat(olddev, olddev1);
	if(make_tmpdir(olddev)) goto e_old_dev;
	data.old_dev = olddev;

	static char newdev1[] = "/new_dev";
	char newdev[sizeof tmpdir - 1 + sizeof newdev1 - 1 + 1];
	strcpy(newdev, tmpdir);
	strcat(newdev, newdev1);
	if(make_tmpdir(newdev)) goto e_new_dev;
	data.new_dev = newdev;

	static char umountfs1[] = "/nothing";
	char umountfs[sizeof tmpdir - 1 + sizeof umountfs1 - 1 + 1];
	strcpy(umountfs, tmpdir);
	strcat(umountfs, umountfs1);
	if(make_tmpdir(umountfs)) goto e_umountfs;
	data.nothing_mountpoint = umountfs;

	/* Make directory in /dev. */
	static char dev_template[] = "/dev/session-XXXXXX";
	char devdir[sizeof dev_template];
	strcpy(devdir, dev_template);
	if(mkdtemp(devdir) == NULL) {
		fprintf(stderr, "Could not create directory in /dev: %s.\n",
				strerror(errno));
		goto e_mkdtemp_dev;
	}
	if(chmod(devdir, 0755) < 0) {
		fprintf(stderr, "Could not set permissions on %s: %s.",
				devdir, strerror(errno));
		goto e_chmod_dev;
	}

	/* Card goes there. */
	static char card_dev[] = "/card0";
	char card_device[sizeof devdir - 1 + sizeof card_dev - 1 + 1];
	strcpy(card_device, devdir);
	strcat(card_device, card_dev);
	data.card_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if(data.card_fd == -1) {
		fprintf(stderr, "Could not open /dev/dri/card0: %s.\n",
				strerror(errno));
		goto e_open_card;
	}
	if(card_new(&data.card, register_fd, unregister_fd, &data,
				data.card_fd,
//				card_wants_resources,
//				framebuffer_callback,
				card_device + 5)) {
		goto e_card_new;
	}
	data.card_device = card_device;

	/* TTY also goes there. */
	static char tty_dev[] = "/tty0";
	char tty_device[sizeof devdir - 1 + sizeof tty_dev - 1 + 1];
	strcpy(tty_device, devdir);
	strcat(tty_device, tty_dev);
	if(tty_new(&data.tty, register_fd, unregister_fd, &data,
				tty_device + 5))
		goto e_tty_new;
	data.tty_device = tty_device;

	/* Symlinks */
	if(symlinks_new(&data.s, newdev, olddev, filter, &data))
		goto e_symlinks_new;

	if(umountfs_new(&data.efs, register_fd, unregister_fd, &data,
				umountfs, notify_umount)) goto e_umountfs_new;
	data.flags |= UMOUNTFS_PRESENT_OUTSIDE_NAMESPACE;

	/* Tell libsession that init succeeded. */
	init_failed = 0;
	struct message_header_out mho;
	mho.type = INIT_SUCCESS;
	send_header(&data, &mho);

	while(data.flags & SESSION_REF || data.flags & OUTSIDE_REF) {
		struct epoll_event ev;
		epoll_wait(data.epoll_fd, &ev, 1, -1);
		struct registered_fd *reg = ev.data.ptr;
		reg->f(reg->user1,
				(ev.events & EPOLLIN ? 1 : 0) |
				(ev.events & EPOLLOUT ? 2 : 0) |
				(ev.events & EPOLLERR ? 4 : 0) |
				(ev.events & EPOLLHUP ? 8 : 0));
	}

	r = 0;

	if(data.flags & UMOUNTFS_PRESENT_OUTSIDE_NAMESPACE) umount(umountfs);
	umountfs_free(data.efs);
e_umountfs_new:
	symlinks_free(data.s);
e_symlinks_new:
	tty_free(data.tty);
e_tty_new:
	card_free(data.card);
e_card_new:
	close(data.card_fd);
e_open_card:
e_chmod_dev:
	rmdir(devdir);
e_mkdtemp_dev:
	rmdir(umountfs);
e_umountfs:
	rmdir(newdev);
e_new_dev:
	rmdir(olddev);
e_old_dev:
e_chmod:
	rmdir(tmpdir);
e_mkdtemp:
e_fcntl:
e_setuid:
e_sigblock:
	unregister_fd(sigfd_ptr);
e_register_sigfd:
	close(data.sigfd);
e_sigfd:
	unregister_fd(socket_fd_ptr);
e_register_socket:
	close(data.epoll_fd);
e_epoll:
	if(init_failed) {
		/* Tell libsession that init failed. */
		mho.type = INIT_FAILURE;
		send_header(&data, &mho);
	}
	close(data.socket_fd);
e_args:
	return r;
}

static void remove_outside_ref(struct data *data)
{
	data->flags &= ~OUTSIDE_REF;
	//TODO FIXME XXX here, we should tell card.c to make outstanding ioctls
	//return somehow.
}

static int make_tmpdir(char *dir)
{
	if(mkdir(dir, 0755)) {
		fprintf(stderr, "Could not create directory %s: %s.",
				dir, strerror(errno));
		return -1;
	}
	return 0;
}

static void sigfd_event(void *user, int event)
{
	struct data *data = user;
	struct signalfd_siginfo ssi;
	read(data->sigfd, &ssi, sizeof ssi);
	if(ssi.ssi_signo == SIGHUP
			|| ssi.ssi_signo == SIGTERM
			|| ssi.ssi_signo == SIGINT) {
		/* Ignore. */
	}
	else if(ssi.ssi_signo == SIGPIPE) {
		remove_outside_ref(data);
	}
	else if(ssi.ssi_signo == SIGCHLD) {
		if(!data->child) return;

		/* There's a race between SIGCHLD and umount if they're from
		 * the same process. Check if FUSE fd is HUPed, and if so,
		 * don't send a message. We could wait, but init reaps the
		 * child anyway. */
		if(umountfs_has_hup(data->efs)) return;

		int child_status;
		int status = waitpid(data->child, &child_status, WNOHANG);
		if(status == data->child && WIFEXITED(child_status)) {
			data->child = 0;
			struct message_header_out header;
			header.type = GOT_SIGCHLD;
			send_header(data, &header);
		}
	}
}

static void socket_event(void *user, int event)
{
	struct data *data = user;
	if(event & 12) {
		/* HUP or ERR */
		remove_outside_ref(data);
	}
	else if(event & 1) {
		/* Readable */
		//TODO struct message_in msg;

		char buf[IN_BUFFER_SIZE];
		struct iovec iov[1];
		iov[0].iov_base = buf;
		iov[0].iov_len = sizeof buf;
		struct msghdr msghdr;
		msghdr.msg_name = NULL;
		msghdr.msg_namelen = 0;
		msghdr.msg_iov = iov;
		msghdr.msg_iovlen = sizeof iov / sizeof iov[0];
		msghdr.msg_control = NULL;
		msghdr.msg_controllen = 0;
		msghdr.msg_flags = 0;
		int status = recvmsg(data->socket_fd, &msghdr, 0);
		if(status < 0) {
			fprintf(stderr, "Error reading from socket: %s.\n",
					strerror(errno));
			goto recv_error;
		}
		if(msghdr.msg_flags & MSG_TRUNC) {
			fprintf(stderr, "Message too long.\n");
			goto recv_error;
		}

struct message_in msg1;memcpy(&msg1, buf, sizeof msg1);
if(msg1.type == MODESET_IOCTL_RESPONSE) {
	struct message_in msg = msg1;
	char *buf1 = buf + sizeof msg1;
	char *buf = buf1;
	int len = status - sizeof msg1;

printf("Session-master: got response to ioctl with retval = %d.\n", msg.modeset_ioctl.return_value);
	card_modeset_ioctl_response(data->card, msg.modeset_ioctl.unique,
			msg.modeset_ioctl.return_value, buf, len);
	goto msg_err;
}

		struct message_in *msg;
		char *buf_p = buf;
		if(message_in_deserialize(&msg, &buf_p, &status)) {
			fprintf(stderr, "Message malformed.\n");
			goto msg_err;
		}

		handle_message(data, msg);

		message_in_free(msg);
msg_err:
recv_error:;
	}
}

static void *register_fd(
		void *user,
		int fd,
		unsigned flags,
		void (*f)(void *user1, int event),
		void *user1)
{
	struct data *data = user;
	struct registered_fd *reg = malloc(sizeof *reg);
	if(!reg) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.", __FILE__,
				__LINE__);
		return NULL;
	}

	reg->data = data;
	reg->fd = fd;
	reg->f = f;
	reg->user1 = user1;
	struct epoll_event ev;
	ev.events =
		(flags & 1 ? EPOLLIN : 0) |
		(flags & 2 ? EPOLLOUT : 0) |
		(flags & 4 ? EPOLLET : 0);
	ev.data.ptr = reg;
	if(epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		fprintf(stderr, "Could not add file descriptor to epoll: %s.",
				strerror(errno));
		free(reg);
		return NULL;
	}

	return reg;
}

static void unregister_fd(void *ptr)
{
	struct registered_fd *reg = ptr;
	epoll_ctl(reg->data->epoll_fd, EPOLL_CTL_DEL, reg->fd, NULL);
	free(reg);
}

static int filter(void *user, char *dev_path, struct stat *stat)
{
	struct data *data = user;
	char *cuse_dev =
		!strcmp(dev_path, "/dev/tty0") ? data->tty_device :
		!strcmp(dev_path, "/dev/dri/card0") ? data->card_device :
		NULL;
	if(cuse_dev) {
		struct stat cuse_stat;
		if(lstat(cuse_dev, &cuse_stat) == -1) return -1;
		stat->st_mode = cuse_stat.st_mode;
		stat->st_rdev = cuse_stat.st_rdev;
		stat->st_uid = data->real_uid;
		stat->st_gid = data->real_gid;
	}
	return 0;
}

static void handle_message(struct data *data, struct message_in *msg)
{
	if(msg->type == RUN_COMMAND) {
		if(data->flags & HAVE_FORKED) {
			fprintf(stderr, "Have already forked.\n");
			return;
		}
		data->flags |= HAVE_FORKED;

		int argc = 0;
		while(msg->cmd.argv[argc]) ++argc;

		data->path = msg->cmd.path;
		data->argv = msg->cmd.argv;
		char stack[8192];
		pid_t pid = clone(forked_proc, stack + 4096, CLONE_NEWNS
				| CLONE_VFORK | CLONE_VM | SIGCHLD, data);
		if(pid == -1) {
			fprintf(stderr, "Could not clone: %s.\n",
					strerror(errno));
			return;
		}

		data->child = pid;
		umount(data->nothing_mountpoint);
		data->flags |= SESSION_REF;
	}
#if 0
	else if(msg->type == RESOURCES_RESPONSE) {
		printf("GOT RESPONSE TO RESOURCES\n");
printf("count_crtcs: %d\n",
		msg->resources.res->count_crtcs);
unsigned i;
for(i = 0; i < msg->resources.res->count_crtcs; ++i) {
	printf("  crtc: %d\n", msg->resources.res->crtc_id_ptr[i]);
}
		card_send_resources(data->card, msg->resources.unique,
				msg->resources.res);
		printf("RESOURCES OUT\n");
	}
#endif
}

static int forked_proc(void *user)
{
	struct data *data = user;
	int status;
	/* Remount /dev. */
	status = mount("/dev", data->old_dev, NULL, MS_MOVE, NULL);
	if(status == -1) {
		fprintf(stderr, "Could not move /dev to %s: %s.\n",
				data->old_dev, strerror(errno));
		goto error;
	}
	status = mount(data->new_dev, "/dev", NULL, MS_BIND, NULL);
	if(status == -1) {
		fprintf(stderr, "Could not bind %s to /dev: %s.\n",
				data->new_dev, strerror(errno));
		goto error;
	}
	/* Don't want to block signals. */
	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);
	/* Stop being root. */
	if(setresgid(data->real_gid, data->real_gid, data->real_gid) == -1) {
		fprintf(stderr, "Setresgid failed: %s.\n", strerror(errno));
		goto error;
	}
	if(setresuid(data->real_uid, data->real_uid, data->real_uid) == -1) {
		fprintf(stderr, "Setresuid failed: %s.\n", strerror(errno));
		goto error;
	}
	execvp(data->path, data->argv);
	fprintf(stderr, "Execvp failed: %s.\n", strerror(errno));
error:
	exit(1);
}

static void notify_umount(void *user)
{
	struct data *data = user;
	data->flags &= ~SESSION_REF;
	struct message_header_out header;
	header.type = GOT_UMOUNT;
	send_header(data, &header);
}

static void send_header(struct data *data, struct message_header_out *header)
{
	if(data->flags & OUTSIDE_REF) {
		send(data->socket_fd, header, sizeof *header, 0);
	}
}

#if 0
static void framebuffer_callback(void *user, uint32_t name, int w, int h)
{
	struct data *data = user;
	struct message_header_out header;
	header.type = FRAMEBUFFER;
	header.framebuffer.gem_name = name;
	header.framebuffer.w = w;
	header.framebuffer.h = h;
	send_header(data, &header);
}

static void card_wants_resources(void *user, uint64_t unique)
{
	struct data *data = user;
	struct message_header_out header;
	header.type = RESOURCES;
	header.resources.unique = unique;
	send_header(data, &header);
	return;
}
#endif

int card_send_modeset_ioctl(void *user, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len)
{
printf("Session-master: actual sending of message to libsession.\n");
printf(" len = %d\n", len);
	struct data *data = user;

	if(!(data->flags & OUTSIDE_REF)) return 1;

	struct message_header_out header;
	header.type = MODESET_IOCTL;
	header.modeset_ioctl.unique = unique;
	header.modeset_ioctl.cmd = cmd;
	header.modeset_ioctl.inarg = inarg;
	header.modeset_ioctl.len = len;

	struct iovec iov[2];
	iov[0].iov_base = &header;
	iov[0].iov_len = sizeof header;
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	struct msghdr hdr;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_iov = iov;
	hdr.msg_iovlen = sizeof iov / sizeof iov[0];
	hdr.msg_control = NULL;
	hdr.msg_controllen = 0;
	hdr.msg_flags = 0;

	ssize_t status = sendmsg(data->socket_fd, &hdr, 0);
	if(status == -1) {
		fprintf(stderr, "Error sending ioctl to user.\n");
		return -1;
	}

	return 0;
}
