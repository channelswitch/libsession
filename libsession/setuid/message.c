#include "message.h"
#include "../protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int deserialize_u32(uint32_t *out, char **buf, int *buf_sz);
static int deserialize_u32_array(uint32_t **array_out, unsigned n_elems,
		char **buf, int *buf_sz);
static void free_u32_array(uint32_t *array);
static int deserialize_string(char **out, char **buf, int *buf_sz);
static void free_string(char *s);
static int deserialize_argv(char ***out, char **buf, int *buf_sz);
static void free_argv(char **argv);
static int resources_deserialize(struct session_dri_resources **res_out,
		char **buf, int *buf_sz);
static void resources_free(struct session_dri_resources *res);

static int message_in(struct message_in *m, struct message_in **m_p,
		char **buf, int *buf_sz)
{
	if(m) goto free;

	m = malloc(sizeof *m);
	if(!m) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n",
				__FILE__, __LINE__);
		goto e_malloc;
	}

	uint32_t type;
	if(deserialize_u32(&type, buf, buf_sz)) goto e_type;
	m->type = type;

	if(m->type == RUN_COMMAND) {
		if(deserialize_string(&m->cmd.path, buf, buf_sz)) goto e_r1;
		if(deserialize_argv(&m->cmd.argv, buf, buf_sz)) goto e_r2;
	}
	else if(m->type == RESOURCES_RESPONSE) {
		if(deserialize_u32(&m->resources.unique, buf, buf_sz)) {
			goto e_resources_sequence;
		}
		if(resources_deserialize(&m->resources.res, buf, buf_sz)) {
			goto e_resources_res;
		}
	}
	else {
		goto e_type;
	}

	*m_p = m;
	return 0;

free:
	if(m->type == RUN_COMMAND) {
		free_argv(m->cmd.argv);
e_r1:
		free_string(m->cmd.path);
e_r2:;
	}
	else if(m->type == RESOURCES_RESPONSE) {
		resources_free(m->resources.res);
e_resources_res:
e_resources_sequence:;
	}
e_type:
	free(m);
e_malloc:
	return -1;
}

int message_in_deserialize(struct message_in **m_p, char **buf, int *buf_sz)
{
	return message_in(NULL, m_p, buf, buf_sz);
}

void message_in_free(struct message_in *m)
{
	if(m) message_in(m, NULL, NULL, 0);
}

/*
 * Int
 */

static int deserialize_u32(uint32_t *out, char **buf, int *buf_sz)
{
	if(*buf_sz < sizeof *out) return -1;
	unsigned i;
	for(i = 0; i < sizeof *out; ++i) {
		*out <<= 8;
		*out |= *(*buf)++;
		--*buf_sz;
	}
	return 0;
}

/*
 * Arrays
 */

static int u32_array(uint32_t *array, uint32_t **array_out, unsigned n_elems,
		char **buf, int *buf_sz)
{
	if(array) goto free;

	array = malloc(sizeof *array * n_elems);
	if(!array) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n", __FILE__,
				__LINE__);
		goto e_malloc;
	}

	unsigned i;
	for(i = 0; i < n_elems; ++i) {
		if(deserialize_u32(array + i, buf, buf_sz)) goto e_deserialize;
	}

	*array_out = array;
	return 0;

free:
e_deserialize:
	free(array);
e_malloc:
	return -1;
}

static int deserialize_u32_array(uint32_t **array_out, unsigned n_elems,
		char **buf, int *buf_sz)
{
	return u32_array(NULL, array_out, n_elems, buf, buf_sz);
}

static void free_u32_array(uint32_t *array)
{
	if(array) u32_array(array, NULL, 0, NULL, NULL);
}

/*
 * String
 */

static int string(char *s, char **out, char **buf, int *buf_sz)
{
	if(s) goto free;

	int len = 0;
	while(1) {
		if(len >= *buf_sz) goto e_len;
		if((*buf)[len++] == '\0') break;
	}

	s = malloc(len);
	if(!s) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n", __FILE__,
				__LINE__);
		goto e_malloc;
	}

	strcpy(s, *buf);

	*buf += len;
	*buf_sz -= len;
	*out = s;
	return 0;

free:
	free(s);
e_malloc:
e_len:
	return -1;
}

static int deserialize_string(char **out, char **buf, int *buf_sz)
{
	return string(NULL, out, buf, buf_sz);
}

static void free_string(char *s)
{
	if(s) string(s, NULL, NULL, NULL);
}

/*
 * Argv
 */

static int argv(char **argv, char ***out, char **buf, int *buf_sz)
{
	if(argv) goto free;

	uint32_t n_strings;
	if(deserialize_u32(&n_strings, buf, buf_sz)) goto e_n;

	argv = malloc(sizeof *argv * (n_strings + 1));
	if(!argv) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n",
				__FILE__, __LINE__);
		goto e_malloc;
	}

	unsigned i;
	for(i = 0; i < n_strings; ++i) {
		if(deserialize_string(&argv[i], buf, buf_sz)) goto e_strings;
	}

	argv[n_strings] = NULL;

	*out = argv;
	return 0;

free:
	i = 0;
	while(argv[i]) ++i;
	while(i--) {
		free_string(argv[i]);
e_strings:;
	}
	free(argv);
e_malloc:
e_n:
	return -1;
}

static int deserialize_argv(char ***out, char **buf, int *buf_sz)
{
	return argv(NULL, out, buf, buf_sz);
}

static void free_argv(char **a)
{
	if(a) argv(a, NULL, NULL, NULL);
}

/*
 * Resources
 */

static int resources(struct session_dri_resources *res,
		struct session_dri_resources **res_out,
		char **buf, int *buf_sz)
{
	if(res) goto free;

	res = malloc(sizeof *res);
	if(!res) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.\n", __FILE__,
				__LINE__);
		goto e_malloc;
	}

	if(deserialize_u32(&res->count_fbs, buf, buf_sz)) goto e_count_fbs;
	if(deserialize_u32_array(&res->fb_id_ptr, res->count_fbs, buf,
				buf_sz)) goto e_fb_id_ptr;

	if(deserialize_u32(&res->count_crtcs, buf, buf_sz)) goto e_count_crtcs;
	if(deserialize_u32_array(&res->crtc_id_ptr, res->count_crtcs, buf,
				buf_sz)) goto e_crtc_id_ptr;

	if(deserialize_u32(&res->count_connectors, buf,
				buf_sz)) goto e_count_connectors;
	if(deserialize_u32_array(&res->connector_id_ptr, res->count_connectors,
				buf, buf_sz)) goto e_connector_id_ptr;

	if(deserialize_u32(&res->count_encoders, buf,
				buf_sz)) goto e_count_encoders;
	if(deserialize_u32_array(&res->encoder_id_ptr, res->count_encoders,
				buf, buf_sz)) goto e_encoder_id_ptr;

	if(deserialize_u32(&res->min_width, buf, buf_sz)) goto e_min_width;
	if(deserialize_u32(&res->max_width, buf, buf_sz)) goto e_max_width;
	if(deserialize_u32(&res->min_height, buf, buf_sz)) goto e_min_height;
	if(deserialize_u32(&res->max_height, buf, buf_sz)) goto e_max_height;

	*res_out = res;
	return 0;

free:
e_max_height:
e_min_height:
e_max_width:
e_min_width:
	free_u32_array(res->encoder_id_ptr);
e_encoder_id_ptr:
e_count_encoders:
	free_u32_array(res->connector_id_ptr);
e_connector_id_ptr:
e_count_connectors:
	free_u32_array(res->crtc_id_ptr);
e_crtc_id_ptr:
e_count_crtcs:
	free_u32_array(res->fb_id_ptr);
e_fb_id_ptr:
e_count_fbs:
	free(res);
e_malloc:
	return -1;
}

static int resources_deserialize(struct session_dri_resources **res_out,
		char **buf, int *buf_sz)
{
	return resources(NULL, res_out, buf, buf_sz);
}

static void resources_free(struct session_dri_resources *res)
{
	if(res) resources(res, NULL, NULL, NULL);
}

