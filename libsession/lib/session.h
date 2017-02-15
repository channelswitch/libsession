#include <stdint.h>
#include "dri.h"

struct session;
struct session_args;
struct session_dri_resources;
int session_new(
		struct session **s_out,
		/* Callback to register an fd for polling. Flags are 1 for
		 * readable, 2 for writable and 4 for edge triggered. The
		 * function f is to be called whenever that happens to the
		 * fd. */
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			/* Event: 1 - readable, 2 - writable, 4 - error. */
			void (*f)(void *user1, int event),
			void *user1),
		/* Called with the pointer returned by register_fd to free
		 * it. */
		void (*unregister_fd)(void *fd_ptr),
		/* Called when the last process in the session, including
		 * daemons, exits. */
		void (*on_exit)(void *user),
		/* User pointer sent to register_fd and error and other
		 * callbacks. */
		void *user,
		/* Other args. Can not be NULL. */
		struct session_args *args,
		/* Callback for every modeset ioctl in the session. Structs
		 * the same as in drm.h, drm_mode.h etc. Pointers automatically
		 * translated. */
		int (*modeset_ioctl)(void *user, uint32_t cmd, void *arg));//,
		/* Setup (CRTCs, connectors, etc) for the default card. */
//		struct session_dri_resources *resources);
void session_free(struct session *s);

/* Other args, all optional. */
enum {
	SESSION_ON_SIGCHLD = 1 << 0,
	SESSION_PRE_EXEC = 1 << 1,
	SESSION_FRAMEBUFFER_GEM = 1 << 2,
	SESSION_MODESETTING = 1 << 3,
	SESSION_ALL_ARG_FLAGS = (1 << 4) - 1,
};
struct session_args {
	unsigned flags;
	/* Called when the immediate child of the session master, the process
	 * you specified, exits. Not called if that is the last process in the
	 * session. It's fine to call session_free() and even exit while there
	 * are still processes in the session. They and the session master will
	 * continue to run in the background, though consider that fds may get
	 * HUPed. */
	void (*on_sigchld)(void *user);
	/* Called after (double) vfork but before exec("session-master"). Here
	 * is where you do dup2()ing and setsid() if you want to change the
	 * controlling terminal for the session. Also setuid, cgroups,
	 * namespaces, etc. */
	void (*pre_exec)(void *user);
	/* Called with a GEM name when there is a new framebuffer. */
	void (*framebuffer_gem)(void *user, uint32_t name, int w, int h);
	/* Called when the session requests resources. Use session_send_dri_
	 * resources to respond. */
	void (*resources)(void *user, unsigned unique);
};

/* Start the session by running this command. Only call once. Argv is a NULL-
 * terminated array of NULL-terminated strings, like argv from main. */
int session_start(struct session *s, char *command, char **argv);

/* Call after resources() callback, not neccesarily inside the callback.
 * Resources argument needs only remain valid for duration of call. */
int session_send_dri_resources(struct session *s,
		struct session_dri_resources *resources,
		unsigned sequence);
