/*
 * Not automatically generated
 */
#include <stddef.h>
#include <stdint.h>

struct fuse_ioctl_iovec;

void ioctls_handle(void *user, uint64_t unique, uint32_t cmd, uint64_t inarg,
		char *buf, int len, int outsize);

/* To be implemented by the user. */
void ioctls_retry(void *user, uint64_t unique, struct fuse_ioctl_iovec *fiov,
		int fiov_len, int last_retry);
void ioctls_return_error(void *user, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len);
void ioctls_modeset(void *user, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len);
int ioctls_render(void *user, uint32_t cmd, void *inarg);
void ioctls_render_done(void *user, int retv, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len);
