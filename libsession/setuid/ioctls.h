/*
 * Not automatically generated
 */
#include <stddef.h>
#include <stdint.h>

struct fuse_ioctl_iovec;

void handle_ioctl(void *user, uint64_t unique, uint64_t cmd, uint64_t inarg,
		char *buf, int len, int outsize);

/* To be implemented by the user. */
void retry_ioctl(void *user, uint64_t unique, struct fuse_ioctl_iovec *fiov,
		int fiov_len, int last_retry);
void return_ioctl_error(void *user, uint64_t unique, char *buf, int len);
void send_modeset_ioctl_to_user(void *user, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len);
