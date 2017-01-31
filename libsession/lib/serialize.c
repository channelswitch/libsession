#include "serialize.h"
#include "../protocol.h"
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>

static int serialize_u32(uint32_t n, char *buf, int buf_sz);
static int serialize_u32_array(uint32_t *array, unsigned n_elems, char *buf,
		int buf_sz);
static int serialize_string(char *str, char *buf, int buf_sz);
static int serialize_argv(char **argv, char *buf, int buf_sz);
static int serialize_dri_resources(struct session_dri_resources *res,
		char *buf, int buf_sz);

int serialize_message_in(struct message_in *message,
		char *buf, int buf_sz)
{
	int r = 0;
	r += serialize_u32(message->type, buf + r, buf_sz - r);
	if(message->type == RUN_COMMAND) {
		r += serialize_string(message->cmd.path, buf + r, buf_sz - r);
		r += serialize_argv(message->cmd.argv, buf + r, buf_sz - r);
	}
	else if(message->type == RESOURCES_RESPONSE) {
		r += serialize_u32(message->resources.unique, buf + r,
				buf_sz - r);
		r += serialize_dri_resources(message->resources.res, buf + r,
				buf_sz - r);
	}
	else {
		assert(0);
	}
	return r;
}

static int serialize_u32(uint32_t n, char *buf, int buf_sz)
{
	if(buf_sz < sizeof n) goto ret;
	unsigned i;
	for(i = 0; i < sizeof n; ++i) {
		buf[i] = (n >> (8 * (sizeof n - 1 - i))) & 0xff;
	}
ret:
	return sizeof n;
}

static int serialize_u32_array(uint32_t *array, unsigned n_elems, char *buf,
		int buf_sz)
{
	int r = 4 * n_elems;
	if(r > buf_sz) goto ret;
	r = 0;
	unsigned i;
	for(i = 0; i < n_elems; ++i) {
		r += serialize_u32(array[i], buf + r, buf_sz - r);
	}
ret:
	return r;
}

static int serialize_string(char *s, char *buf, int buf_sz)
{
	int r = strlen(s) + 1;
	while(buf_sz > 0) {
		*buf = *s;
		if(*s == '\0') break;
		++s;
		++buf;
		--buf_sz;
	}
	return r;
}

static int serialize_argv(char **argv, char *buf, int buf_sz)
{
	int r = 0;
	unsigned n = 0;
	while(argv[n]) ++n;
	r += serialize_u32(n, buf + r, buf_sz - r);
	unsigned i;
	for(i = 0; i < n; ++i) {
		r += serialize_string(argv[i], buf + r, buf_sz - r);
	}
	return r;
}

static int serialize_dri_resources(struct session_dri_resources *res,
		char *buf, int buf_sz)
{
	int r = 0;

	r += serialize_u32(res->count_fbs, buf + r, buf_sz - r);
	r += serialize_u32_array(res->fb_id_ptr, res->count_fbs, buf + r,
			buf_sz - r);
	r += serialize_u32(res->count_crtcs, buf + r, buf_sz - r);
	r += serialize_u32_array(res->crtc_id_ptr, res->count_crtcs, buf + r,
			buf_sz - r);
	r += serialize_u32(res->count_connectors, buf + r, buf_sz - r);
	r += serialize_u32_array(res->connector_id_ptr, res->count_connectors,
			buf + r, buf_sz - r);
	r += serialize_u32(res->count_encoders, buf + r, buf_sz - r);
	r += serialize_u32_array(res->encoder_id_ptr, res->count_encoders,
			buf + r, buf_sz - r);
	r += serialize_u32(res->min_width, buf + r, buf_sz - r);
	r += serialize_u32(res->max_width, buf + r, buf_sz - r);
	r += serialize_u32(res->min_height, buf + r, buf_sz - r);
	r += serialize_u32(res->max_height, buf + r, buf_sz - r);

	return r;
}
