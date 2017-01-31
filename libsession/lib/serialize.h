#include <stdint.h>

/* Returns the number of bytes it would take. If ret > buf_sz, the contents
 * of buf are undefined. */
struct message_in;
int serialize_message_in(struct message_in *message, char *buf, int buf_sz);
