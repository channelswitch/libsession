/* A completely empty FUSE filesystem that notifies you of when it is
 * unmounted. This is used to detect when the last process in a mountpoint
 * namespace exits. The filesystem will not automatically unmount itself upon
 * umountfs_free since you may have moved it. You must ensure it is unmounted
 * before umountfs_free is called. */

struct umountfs;
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
		void (*notify_umount)(void *user));
void umountfs_free(struct umountfs *e);

unsigned umountfs_has_hup(struct umountfs *e);
