#include <stdint.h>
#include "lib/dri.h"

enum message_type_in {
	RUN_COMMAND,
	MODESET_IOCTL_RESPONSE,
};

struct message_in {
	enum message_type_in type;
	union {
		struct {
			char *path;
			char **argv;
		} cmd;
		struct {
			uint64_t unique;
			uint32_t len;
			int32_t return_value;
		} modeset_ioctl;
	};
};

#define IN_BUFFER_SIZE 16000

enum message_type_out {
	INIT_FAILURE,
	INIT_SUCCESS,
	GOT_SIGCHLD,
	GOT_UMOUNT,
	MODESET_IOCTL,
};

struct message_header_out {
	enum message_type_out type;
	union {
		struct {
			uint64_t unique;
			uint64_t inarg;
			uint32_t cmd;
			uint32_t len;
		} modeset_ioctl;
	};
};

#define OUT_BUFFER_SIZE IN_BUFFER_SIZE
