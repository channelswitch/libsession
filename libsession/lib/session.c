#define _GNU_SOURCE
#include "session.h"
#include "serialize.h"
#include "ioctls.h"
#include "../protocol.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

static int spawn_master(struct session *s, int *socket_out);
static int wait_for_externally_spawned_master(struct session *s,
		int *socket_out);
static void socket_event(void *user, int event);
static void handle_event(struct session *s, struct message_header_out *header,
		char *payload, unsigned payload_len);
static int child_f(void *user);
static int grandchild_f(void *user);
static int send_message(struct session *s, struct message_in *msg);

struct session {
	void *user;
	void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1);
	void (*unregister_fd)(void *fd_ptr);
	void (*on_exit)(void *user);
	void (*on_sigchld)(void *user);
	void (*pre_exec)(void *user);
	void (*framebuffer_gem)(void *user, uint32_t name, int w, int h);
	void (*resources)(void *user, unsigned sequence);

	int (*modeset_ioctl)(void *user, uint32_t cmd, void *arg);

	int socket_fd;
	void *fd_ptr;
};

static int session(
		struct session *s,
		struct session **s_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void (*on_exit)(void *user),
		void *user,
		struct session_args *args,
		int (*modeset_ioctl)(void *user, uint32_t cmd, void *arg))
{
	if(s) goto free;

	if(args->flags & ~SESSION_ALL_ARG_FLAGS) {
		fprintf(stderr, "Unsupported args in call to session_new.\n");
		goto e_args;
	}

	s = malloc(sizeof *s);
	if(!s) goto e_malloc;

	s->user = user;
	s->register_fd = register_fd;
	s->unregister_fd = unregister_fd;
	s->on_exit = on_exit;

	s->on_sigchld = args->flags & SESSION_ON_SIGCHLD ?
		args->on_sigchld : NULL;
	s->pre_exec = args->flags & SESSION_PRE_EXEC ?
		args->pre_exec : NULL;
	s->framebuffer_gem = args->flags & SESSION_FRAMEBUFFER_GEM ?
		args->framebuffer_gem : NULL;
	s->resources = args->resources;

	s->modeset_ioctl = modeset_ioctl;

	if(spawn_master(s, &s->socket_fd)) goto e_spawn_master;

	s->fd_ptr = s->register_fd(user, s->socket_fd, 1, socket_event, s);
	if(!s->fd_ptr) goto e_register_fd;

	struct message_header_out mho;
	/* This assertion failing means the helper exited without sending
	 * INIT_FAILURE or INIT_SUCCESS, which is a programming error in it. */
	assert(recv(s->socket_fd, &mho, sizeof mho, 0) == sizeof mho);
	if(mho.type == INIT_FAILURE) goto e_init;
	assert(mho.type == INIT_SUCCESS);

	*s_out = s;
	return 0;

free:
e_init:
	if(s->fd_ptr) s->unregister_fd(s->fd_ptr);
e_register_fd:
	close(s->socket_fd);
e_spawn_master:
	free(s);
e_malloc:
e_args:
	return -1;
}

int session_new(
		struct session **s_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void (*on_exit)(void *user),
		void *user,
		struct session_args *args,
		int (*modeset_ioctl)(void *user, uint32_t cmd, void *arg))
{
	return session(NULL, s_out, register_fd, unregister_fd, on_exit, user,
			args, modeset_ioctl);
}

void session_free(struct session *s)
{
	if(s) session(s, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

struct child_data {
	struct session *s;
	int sockets[2];
};

static int spawn_master(
		struct session *s,
		int *socket_out)
{
	if(secure_getenv("LIBSESSION_DEBUG_SOCKET")) {
		return wait_for_externally_spawned_master(s, socket_out);
	}

	int status;
	struct child_data child_data;
	child_data.s = s;
	status = socketpair(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
			child_data.sockets);
	if(status) {
		fprintf(stderr, "Could not create socket pair: %s.\n",
				strerror(errno));
		goto e_socketpair;
	}

	/* Block all signals because of vfork. In session-master, signals are
	 * reset so no need to worry about it. */
	sigset_t set, old_set;
	sigfillset(&set);
	sigprocmask(SIG_SETMASK, &set, &old_set);

	/* This clone call is basically vfork but also tells the kernel not to
	 * send any signal when the process exits. This is good because it
	 * means the user of this shared library doesn't need to know that we
	 * fork. I don't know of any other way to fork and hide the signal from
	 * the user. Supports stacks growing either up or down by putting sp in
	 * the middle! */
	char stack[8192];
	pid_t pid = clone(child_f, stack + 4096, CLONE_VM | CLONE_VFORK | 0,
			&child_data);
	if(pid < 0) {
		fprintf(stderr, "Error forking setuid helper process: %s.\n",
				strerror(errno));
		goto e_vfork;
	}

	/* Parent */
	close(child_data.sockets[1]);
	/* No SIGCHLD doesn't mean we don't need to wait(). Because of double
	 * forking, child will exit immediately so no need for synchronisation.
	 * */
	waitpid(pid, NULL, __WCLONE);

	/* Unblock signals. */
	sigprocmask(SIG_SETMASK, &old_set, NULL);

	*socket_out = child_data.sockets[0];
	return 0;

e_vfork:
	close(child_data.sockets[0]);
	close(child_data.sockets[1]);
e_socketpair:
	return -1;
}

/*
 * For debugging. Create a visible socket at the file specified by the
 * environment variable LIBSESSION_DEBUG_SOCKET and wait for session-master
 * to be launched by someone else.
 */
static int wait_for_externally_spawned_master(struct session *s,
		int *socket_out)
{
	int r = -1;
	char *path = secure_getenv("LIBSESSION_DEBUG_SOCKET");

	int named_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(named_socket_fd == -1) {
		fprintf(stderr, "Error creating a unix socket: %s.\n",
				strerror(errno));
		goto e_socket;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	if(strlen(path) + 1 > sizeof addr.sun_path) {
		fprintf(stderr, "Path to debug socket too long (\"%s\").\n",
				path);
		goto e_strcpy;
	}

	strcpy(addr.sun_path, path);
	int status = bind(named_socket_fd, (void *)&addr, sizeof addr);
	if(status == -1) {
		fprintf(stderr, "Could not create debug socket at \"%s\": "
				"%s.\n", path, strerror(errno));
		goto e_bind;
	}

	status = listen(named_socket_fd, 1);
	if(status == -1) {
		fprintf(stderr, "Error in listen(): %s.\n",
				strerror(errno));
		goto e_listen;
	}

	fprintf(stderr, "Libsession waiting for Session-master to connect on "
			"\"%s\".\n", path);

	int socket_fd = accept(named_socket_fd, NULL, NULL);
	if(socket_fd == -1) {
		fprintf(stderr, "Error in accept(): %s.\n",
				strerror(errno));
		goto e_accept;
	}

	*socket_out = socket_fd;

	r = 0;

e_accept:
e_listen:
	unlink(path);
e_bind:
e_strcpy:
	close(named_socket_fd);
e_socket:
	return r;
}

static int child_f(void *user)
{
	struct child_data *data = user;
	/* Child */
	char stack[2048];
	clone(grandchild_f, stack + 1024,
			CLONE_VM | CLONE_VFORK | 0,
			data);
	_exit(0);
}

static int grandchild_f(void *user)
{
	struct child_data *data = user;
	close(data->sockets[0]);
	/* Remove FD_CLOEXEC on socket fd so session-master gets it. */
	fcntl(data->sockets[1], F_SETFD, 0);
	/* Grandchild */
	if(data->s->pre_exec) data->s->pre_exec(data->s->user);
	char buf[10];
	snprintf(buf, sizeof buf, "%d", data->sockets[1]);
	execlp("session-master", "session-master", buf, NULL);
	_exit(1);
}

static void socket_event(void *user, int event)
{
printf("Libsession: Socket event\n");
	struct session *s = user;

	if(event & 12) hup: {
		/* HUP or ERR */
		fprintf(stderr, "The socket connection to the session master "
				"was broken.\n");
		s->on_exit(s->user);
		s->unregister_fd(s->fd_ptr);
		s->fd_ptr = NULL;
	}
	else if(event & 1) {
		struct message_header_out mho;
		char arg[OUT_BUFFER_SIZE];

		struct iovec iov[2];
		iov[0].iov_base = &mho;
		iov[0].iov_len = sizeof mho;
		iov[1].iov_base = arg;
		iov[1].iov_len = sizeof arg;

		struct msghdr hdr;
		hdr.msg_name = NULL;
		hdr.msg_namelen = 0;
		hdr.msg_iov = iov;
		hdr.msg_iovlen = sizeof iov / sizeof iov[0];
		hdr.msg_control = NULL;
		hdr.msg_controllen = 0;
		hdr.msg_flags = 0;
		int status = recvmsg(s->socket_fd, &hdr, 0);

		if(status == -1) {
			fprintf(stderr, "Error in recvmsg: %s.\n",
					strerror(errno));
			return;
		}
		if(status == 0) goto hup;

		if(status < sizeof mho) {
			fprintf(stderr, "Malformed message from session "
					"master.\n");
			return;
		}

		handle_event(s, &mho, arg, status - sizeof mho);
	}
}

static void handle_event(struct session *s, struct message_header_out *header,
		char *payload, unsigned payload_len)
{
	if(header->type == GOT_SIGCHLD) {
		if(s->on_sigchld) s->on_sigchld(s->user);
	}
	else if(header->type == GOT_UMOUNT) {
		s->on_exit(s->user);
		s->unregister_fd(s->fd_ptr);
		s->fd_ptr = NULL;
	}
	else if(header->type == MODESET_IOCTL) {
		struct message_in msg;
		msg.type = MODESET_IOCTL_RESPONSE;
		msg.modeset_ioctl.unique = header->modeset_ioctl.unique;
		msg.modeset_ioctl.len = header->modeset_ioctl.len;

		struct iovec iov[2];
		iov[0].iov_base = &msg;
		iov[0].iov_len = sizeof msg;
		iov[1].iov_base = payload;
		iov[1].iov_len = payload_len;

		struct msghdr hdr;
		hdr.msg_name = NULL;
		hdr.msg_namelen = 0;
		hdr.msg_iov = iov;
		hdr.msg_iovlen = sizeof iov / sizeof iov[0];
		hdr.msg_control = NULL;
		hdr.msg_controllen = 0;
		hdr.msg_flags = 0;

		msg.modeset_ioctl.return_value = ioctls_parse_and_process(
				header->modeset_ioctl.cmd, payload,
				payload_len, s);
		printf("Libsession: modeset was processed and retval = %d.\n",
				msg.modeset_ioctl.return_value);

		ssize_t status = sendmsg(s->socket_fd, &hdr, 0);
		if(status == -1) {
			fprintf(stderr, "Error sending ioctl to user.\n");
		}
	}
//	else if(header->type == FRAMEBUFFER) {
//		if(s->framebuffer_gem) s->framebuffer_gem(s->user,
//				header->framebuffer.gem_name,
//				header->framebuffer.w,
//				header->framebuffer.h);
//	}
//	else if(header->type == RESOURCES) {
//		s->resources(s->user, header->resources.unique);
//	}
}

int ioctls_process(void *user, uint32_t cmd, void *arg)
{
	struct session *s = user;
	return s->modeset_ioctl(s->user, cmd, arg);
}

static int send_message(struct session *s, struct message_in *msg)
{
	char buf[IN_BUFFER_SIZE];
	int status = serialize_message_in(msg, buf, sizeof buf);
	if(status > sizeof buf) {
		fprintf(stderr, "Message too long.\n");
		return -1;
	}
	status = send(s->socket_fd, buf, status, 0);
	if(status == -1) {
		fprintf(stderr, "Could not send message: %s.\n",
				strerror(errno));
		return -1;
	}
	return 0;
}

int session_start(struct session *s, char *path, char **argv)
{
	struct message_in msg;
	msg.type = RUN_COMMAND;
	msg.cmd.path = path;
	msg.cmd.argv = argv;
	return send_message(s, &msg);
}

//int session_send_dri_resources(struct session *s,
//		struct session_dri_resources *resources,
//		unsigned unique)
//{
//	struct message_in msg;
//	msg.type = RESOURCES_RESPONSE;
//	msg.resources.unique = unique;
//	msg.resources.res = resources;
//	return send_message(s, &msg);
//}
