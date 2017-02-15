#include <stdint.h>

int ioctls_parse_and_process(uint32_t cmd, char *buf, int len, void *user);

/*
 * To be implemented by the caller.
 */
int ioctls_process(void *user, uint32_t cmd, void *arg);
