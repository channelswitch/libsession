#include <stdint.h>
#include "lib/dri.h"

enum message_type_in {
	RUN_COMMAND,
	RESOURCES_RESPONSE,
};

struct message_in {
	enum message_type_in type;
	union {
		struct {
			char *path;
			char **argv;
		} cmd;
		struct {
			uint32_t unique;
			struct session_dri_resources *res;
		} resources;
	};
};

#define IN_BUFFER_SIZE 16000

enum message_type_out {
	INIT_FAILURE,
	INIT_SUCCESS,
	GOT_SIGCHLD,
	GOT_UMOUNT,
	FRAMEBUFFER,
	RESOURCES,
};

struct message_header_out {
	enum message_type_out type;
	union {
		struct {
			uint32_t gem_name;
			int w;
			int h;
		} framebuffer;
		struct {
			uint32_t unique;
		} resources;
	};
};

#define OUT_BUFFER_SIZE IN_BUFFER_SIZE
