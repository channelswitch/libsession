/* Automatically generated */
#include "ioctls.h"
#include <stdlib.h>
#include <string.h>
#include "../drm.h"
#include "../radeon_drm.h"

static int parse_drm_ioctl_set_version(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_set_version set_version;
	memcpy(&set_version, buf, sizeof(struct drm_set_version));
	char *set_version_buf_ptr = buf;
	buf += sizeof(struct drm_set_version);
	len -= sizeof(struct drm_set_version);
	size_t set_version_actual_size = sizeof(struct drm_set_version);
	void *set_version_actual_ptr = inarg;
	inarg = &set_version;

	int return_value = ioctls_process(user, DRM_IOCTL_SET_VERSION, inarg);

	memcpy(set_version_buf_ptr, &set_version, set_version_actual_size);
	inarg = set_version_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_get_unique(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_unique uniq;
	memcpy(&uniq, buf, sizeof(struct drm_unique));
	char *uniq_buf_ptr = buf;
	buf += sizeof(struct drm_unique);
	len -= sizeof(struct drm_unique);
	size_t uniq_actual_size = sizeof(struct drm_unique);
	void *uniq_actual_ptr = inarg;
	inarg = &uniq;

	char uniq_str[4096];
	memcpy(uniq_str, buf, uniq.unique_len);
	char *uniq_str_buf_ptr = buf;
	buf += uniq.unique_len;
	len -= uniq.unique_len;
	size_t uniq_str_actual_size = uniq.unique_len;
	void *uniq_str_actual_ptr = uniq.unique;
	uniq.unique = uniq_str;

	int return_value = ioctls_process(user, DRM_IOCTL_GET_UNIQUE, inarg);

	memcpy(uniq_str_buf_ptr, uniq_str, uniq_str_actual_size);
	uniq.unique = uniq_str_actual_ptr;

	memcpy(uniq_buf_ptr, &uniq, uniq_actual_size);
	inarg = uniq_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_version(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_version version;
	memcpy(&version, buf, sizeof(struct drm_version));
	char *version_buf_ptr = buf;
	buf += sizeof(struct drm_version);
	len -= sizeof(struct drm_version);
	size_t version_actual_size = sizeof(struct drm_version);
	void *version_actual_ptr = inarg;
	inarg = &version;

	char name[4096];
	memcpy(name, buf, version.name_len);
	char *name_buf_ptr = buf;
	buf += version.name_len;
	len -= version.name_len;
	size_t name_actual_size = version.name_len;
	void *name_actual_ptr = version.name;
	version.name = name;

	char date[4096];
	memcpy(date, buf, version.date_len);
	char *date_buf_ptr = buf;
	buf += version.date_len;
	len -= version.date_len;
	size_t date_actual_size = version.date_len;
	void *date_actual_ptr = version.date;
	version.date = date;

	char desc[4096];
	memcpy(desc, buf, version.desc_len);
	char *desc_buf_ptr = buf;
	buf += version.desc_len;
	len -= version.desc_len;
	size_t desc_actual_size = version.desc_len;
	void *desc_actual_ptr = version.desc;
	version.desc = desc;

	int return_value = ioctls_process(user, DRM_IOCTL_VERSION, inarg);

	memcpy(desc_buf_ptr, desc, desc_actual_size);
	version.desc = desc_actual_ptr;

	memcpy(date_buf_ptr, date, date_actual_size);
	version.date = date_actual_ptr;

	memcpy(name_buf_ptr, name, name_actual_size);
	version.name = name_actual_ptr;

	memcpy(version_buf_ptr, &version, version_actual_size);
	inarg = version_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_mode_getresources(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_mode_card_res res;
	memcpy(&res, buf, sizeof(struct drm_mode_card_res));
	char *res_buf_ptr = buf;
	buf += sizeof(struct drm_mode_card_res);
	len -= sizeof(struct drm_mode_card_res);
	size_t res_actual_size = sizeof(struct drm_mode_card_res);
	void *res_actual_ptr = inarg;
	inarg = &res;

	uint32_t fbs[1024];
	memcpy(fbs, buf, res.count_fbs * 4);
	char *fbs_buf_ptr = buf;
	buf += res.count_fbs * 4;
	len -= res.count_fbs * 4;
	size_t fbs_actual_size = res.count_fbs * 4;
	void *fbs_actual_ptr = (void *)res.fb_id_ptr;
	res.fb_id_ptr = (uint64_t)fbs;

	uint32_t crtcs[1024];
	memcpy(crtcs, buf, res.count_crtcs * 4);
	char *crtcs_buf_ptr = buf;
	buf += res.count_crtcs * 4;
	len -= res.count_crtcs * 4;
	size_t crtcs_actual_size = res.count_crtcs * 4;
	void *crtcs_actual_ptr = (void *)res.crtc_id_ptr;
	res.crtc_id_ptr = (uint64_t)crtcs;

	uint32_t connectors[1024];
	memcpy(connectors, buf, res.count_connectors * 4);
	char *connectors_buf_ptr = buf;
	buf += res.count_connectors * 4;
	len -= res.count_connectors * 4;
	size_t connectors_actual_size = res.count_connectors * 4;
	void *connectors_actual_ptr = (void *)res.connector_id_ptr;
	res.connector_id_ptr = (uint64_t)connectors;

	uint32_t encoders[1024];
	memcpy(encoders, buf, res.count_encoders * 4);
	char *encoders_buf_ptr = buf;
	buf += res.count_encoders * 4;
	len -= res.count_encoders * 4;
	size_t encoders_actual_size = res.count_encoders * 4;
	void *encoders_actual_ptr = (void *)res.encoder_id_ptr;
	res.encoder_id_ptr = (uint64_t)encoders;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_GETRESOURCES, inarg);

	memcpy(encoders_buf_ptr, encoders, encoders_actual_size);
	res.encoder_id_ptr = (uint64_t)encoders_actual_ptr;

	memcpy(connectors_buf_ptr, connectors, connectors_actual_size);
	res.connector_id_ptr = (uint64_t)connectors_actual_ptr;

	memcpy(crtcs_buf_ptr, crtcs, crtcs_actual_size);
	res.crtc_id_ptr = (uint64_t)crtcs_actual_ptr;

	memcpy(fbs_buf_ptr, fbs, fbs_actual_size);
	res.fb_id_ptr = (uint64_t)fbs_actual_ptr;

	memcpy(res_buf_ptr, &res, res_actual_size);
	inarg = res_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_mode_getconnector(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_mode_get_connector get_connector;
	memcpy(&get_connector, buf, sizeof(struct drm_mode_get_connector));
	char *get_connector_buf_ptr = buf;
	buf += sizeof(struct drm_mode_get_connector);
	len -= sizeof(struct drm_mode_get_connector);
	size_t get_connector_actual_size = sizeof(struct drm_mode_get_connector);
	void *get_connector_actual_ptr = inarg;
	inarg = &get_connector;

	uint32_t encoders[1024];
	memcpy(encoders, buf, get_connector.count_encoders * 4);
	char *encoders_buf_ptr = buf;
	buf += get_connector.count_encoders * 4;
	len -= get_connector.count_encoders * 4;
	size_t encoders_actual_size = get_connector.count_encoders * 4;
	void *encoders_actual_ptr = (void *)get_connector.encoders_ptr;
	get_connector.encoders_ptr = (uint64_t)encoders;

	struct drm_mode_modeinfo modes[1024];
	memcpy(modes, buf, get_connector.count_modes * sizeof(struct drm_mode_modeinfo));
	char *modes_buf_ptr = buf;
	buf += get_connector.count_modes * sizeof(struct drm_mode_modeinfo);
	len -= get_connector.count_modes * sizeof(struct drm_mode_modeinfo);
	size_t modes_actual_size = get_connector.count_modes * sizeof(struct drm_mode_modeinfo);
	void *modes_actual_ptr = (void *)get_connector.modes_ptr;
	get_connector.modes_ptr = (uint64_t)modes;

	uint32_t props[1024];
	memcpy(props, buf, get_connector.count_props * 4);
	char *props_buf_ptr = buf;
	buf += get_connector.count_props * 4;
	len -= get_connector.count_props * 4;
	size_t props_actual_size = get_connector.count_props * 4;
	void *props_actual_ptr = (void *)get_connector.props_ptr;
	get_connector.props_ptr = (uint64_t)props;

	uint64_t prop_values[1024];
	memcpy(prop_values, buf, get_connector.count_props * 8);
	char *prop_values_buf_ptr = buf;
	buf += get_connector.count_props * 8;
	len -= get_connector.count_props * 8;
	size_t prop_values_actual_size = get_connector.count_props * 8;
	void *prop_values_actual_ptr = (void *)get_connector.prop_values_ptr;
	get_connector.prop_values_ptr = (uint64_t)prop_values;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_GETCONNECTOR, inarg);

	memcpy(prop_values_buf_ptr, prop_values, prop_values_actual_size);
	get_connector.prop_values_ptr = (uint64_t)prop_values_actual_ptr;

	memcpy(props_buf_ptr, props, props_actual_size);
	get_connector.props_ptr = (uint64_t)props_actual_ptr;

	memcpy(modes_buf_ptr, modes, modes_actual_size);
	get_connector.modes_ptr = (uint64_t)modes_actual_ptr;

	memcpy(encoders_buf_ptr, encoders, encoders_actual_size);
	get_connector.encoders_ptr = (uint64_t)encoders_actual_ptr;

	memcpy(get_connector_buf_ptr, &get_connector, get_connector_actual_size);
	inarg = get_connector_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_mode_getencoder(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_mode_get_encoder encoder;
	memcpy(&encoder, buf, sizeof(struct drm_mode_get_encoder));
	char *encoder_buf_ptr = buf;
	buf += sizeof(struct drm_mode_get_encoder);
	len -= sizeof(struct drm_mode_get_encoder);
	size_t encoder_actual_size = sizeof(struct drm_mode_get_encoder);
	void *encoder_actual_ptr = inarg;
	inarg = &encoder;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_GETENCODER, inarg);

	memcpy(encoder_buf_ptr, &encoder, encoder_actual_size);
	inarg = encoder_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_get_cap(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_get_cap cap;
	memcpy(&cap, buf, sizeof(struct drm_get_cap));
	char *cap_buf_ptr = buf;
	buf += sizeof(struct drm_get_cap);
	len -= sizeof(struct drm_get_cap);
	size_t cap_actual_size = sizeof(struct drm_get_cap);
	void *cap_actual_ptr = inarg;
	inarg = &cap;

	int return_value = ioctls_process(user, DRM_IOCTL_GET_CAP, inarg);

	memcpy(cap_buf_ptr, &cap, cap_actual_size);
	inarg = cap_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_mode_getcrtc(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_mode_crtc crtc;
	memcpy(&crtc, buf, sizeof(struct drm_mode_crtc));
	char *crtc_buf_ptr = buf;
	buf += sizeof(struct drm_mode_crtc);
	len -= sizeof(struct drm_mode_crtc);
	size_t crtc_actual_size = sizeof(struct drm_mode_crtc);
	void *crtc_actual_ptr = inarg;
	inarg = &crtc;

	uint32_t connectors[1024];
	memcpy(connectors, buf, crtc.count_connectors * 4);
	char *connectors_buf_ptr = buf;
	buf += crtc.count_connectors * 4;
	len -= crtc.count_connectors * 4;
	size_t connectors_actual_size = crtc.count_connectors * 4;
	void *connectors_actual_ptr = (void *)crtc.set_connectors_ptr;
	crtc.set_connectors_ptr = (uint64_t)connectors;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_GETCRTC, inarg);

	memcpy(connectors_buf_ptr, connectors, connectors_actual_size);
	crtc.set_connectors_ptr = (uint64_t)connectors_actual_ptr;

	memcpy(crtc_buf_ptr, &crtc, crtc_actual_size);
	inarg = crtc_actual_ptr;

	return return_value;
}

static int parse_drm_ioctl_set_master(char *buf, int len, void *user)
{
	void *inarg = NULL;

	int return_value = ioctls_process(user, DRM_IOCTL_SET_MASTER, inarg);

	return return_value;
}

static int parse_drm_ioctl_drop_master(char *buf, int len, void *user)
{
	void *inarg = NULL;

	int return_value = ioctls_process(user, DRM_IOCTL_DROP_MASTER, inarg);

	return return_value;
}

static int parse_drm_ioctl_mode_setgamma(char *buf, int len, void *user)
{
	void *inarg = NULL;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_SETGAMMA, inarg);

	return return_value;
}

static int parse_drm_ioctl_mode_setcrtc(char *buf, int len, void *user)
{
	void *inarg = NULL;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_SETCRTC, inarg);

	return return_value;
}

static int parse_drm_ioctl_mode_page_flip(char *buf, int len, void *user)
{
	void *inarg = NULL;

	struct drm_mode_crtc_page_flip flip;
	memcpy(&flip, buf, sizeof(struct drm_mode_crtc_page_flip));
	char *flip_buf_ptr = buf;
	buf += sizeof(struct drm_mode_crtc_page_flip);
	len -= sizeof(struct drm_mode_crtc_page_flip);
	size_t flip_actual_size = sizeof(struct drm_mode_crtc_page_flip);
	void *flip_actual_ptr = inarg;
	inarg = &flip;

	int return_value = ioctls_process(user, DRM_IOCTL_MODE_PAGE_FLIP, inarg);

	memcpy(flip_buf_ptr, &flip, flip_actual_size);
	inarg = flip_actual_ptr;

	return return_value;
}

int ioctls_parse_and_process(uint32_t cmd, char *buf, int len, void *user)
{
	switch(cmd) {
		case DRM_IOCTL_SET_VERSION:
			return parse_drm_ioctl_set_version(buf, len, user);
		case DRM_IOCTL_GET_UNIQUE:
			return parse_drm_ioctl_get_unique(buf, len, user);
		case DRM_IOCTL_VERSION:
			return parse_drm_ioctl_version(buf, len, user);
		case DRM_IOCTL_MODE_GETRESOURCES:
			return parse_drm_ioctl_mode_getresources(buf, len, user);
		case DRM_IOCTL_MODE_GETCONNECTOR:
			return parse_drm_ioctl_mode_getconnector(buf, len, user);
		case DRM_IOCTL_MODE_GETENCODER:
			return parse_drm_ioctl_mode_getencoder(buf, len, user);
		case DRM_IOCTL_GET_CAP:
			return parse_drm_ioctl_get_cap(buf, len, user);
		case DRM_IOCTL_MODE_GETCRTC:
			return parse_drm_ioctl_mode_getcrtc(buf, len, user);
		case DRM_IOCTL_SET_MASTER:
			return parse_drm_ioctl_set_master(buf, len, user);
		case DRM_IOCTL_DROP_MASTER:
			return parse_drm_ioctl_drop_master(buf, len, user);
		case DRM_IOCTL_MODE_SETGAMMA:
			return parse_drm_ioctl_mode_setgamma(buf, len, user);
		case DRM_IOCTL_MODE_SETCRTC:
			return parse_drm_ioctl_mode_setcrtc(buf, len, user);
		case DRM_IOCTL_MODE_PAGE_FLIP:
			return parse_drm_ioctl_mode_page_flip(buf, len, user);
	}
	return -1;
}
