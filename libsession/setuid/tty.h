struct tty;
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
		char *devnode);
void tty_free(struct tty *t);
