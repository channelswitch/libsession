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
	size_t set_version_actual_size = sizeof(struct drm_set_version);
	void *set_version_actual_ptr = inarg;
	buf += sizeof(struct drm_set_version);
	len -= sizeof(struct drm_set_version);
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
	size_t uniq_actual_size = sizeof(struct drm_unique);
	void *uniq_actual_ptr = inarg;
	buf += sizeof(struct drm_unique);
	len -= sizeof(struct drm_unique);
	inarg = &uniq;

	char uniq_str[4096];
	memcpy(uniq_str, buf, uniq.unique_len);
	char *uniq_str_buf_ptr = buf;
	size_t uniq_str_actual_size = uniq.unique_len;
	void *uniq_str_actual_ptr = uniq.unique;
	buf += uniq.unique_len;
	len -= uniq.unique_len;
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
	size_t version_actual_size = sizeof(struct drm_version);
	void *version_actual_ptr = inarg;
	buf += sizeof(struct drm_version);
	len -= sizeof(struct drm_version);
	inarg = &version;

	char name[4096];
	memcpy(name, buf, version.name_len);
	char *name_buf_ptr = buf;
	size_t name_actual_size = version.name_len;
	void *name_actual_ptr = version.name;
	buf += version.name_len;
	len -= version.name_len;
	version.name = name;

	char date[4096];
	memcpy(date, buf, version.date_len);
	char *date_buf_ptr = buf;
	size_t date_actual_size = version.date_len;
	void *date_actual_ptr = version.date;
	buf += version.date_len;
	len -= version.date_len;
	version.date = date;

	char desc[4096];
	memcpy(desc, buf, version.desc_len);
	char *desc_buf_ptr = buf;
	size_t desc_actual_size = version.desc_len;
	void *desc_actual_ptr = version.desc;
	buf += version.desc_len;
	len -= version.desc_len;
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
	size_t res_actual_size = sizeof(struct drm_mode_card_res);
	void *res_actual_ptr = inarg;
	buf += sizeof(struct drm_mode_card_res);
	len -= sizeof(struct drm_mode_card_res);
	inarg = &res;

	uint32_t fbs[1024];
	memcpy(fbs, buf, res.count_fbs * 4);
	char *fbs_buf_ptr = buf;
	size_t fbs_actual_size = res.count_fbs * 4;
	void *fbs_actual_ptr = (void *)res.fb_id_ptr;
	buf += res.count_fbs * 4;
	len -= res.count_fbs * 4;
	res.fb_id_ptr = (uint64_t)fbs;

	uint32_t crtcs[1024];
	memcpy(crtcs, buf, res.count_crtcs * 4);
	char *crtcs_buf_ptr = buf;
	size_t crtcs_actual_size = res.count_crtcs * 4;
	void *crtcs_actual_ptr = (void *)res.crtc_id_ptr;
	buf += res.count_crtcs * 4;
	len -= res.count_crtcs * 4;
	res.crtc_id_ptr = (uint64_t)crtcs;

	uint32_t connectors[1024];
	memcpy(connectors, buf, res.count_connectors * 4);
	char *connectors_buf_ptr = buf;
	size_t connectors_actual_size = res.count_connectors * 4;
	void *connectors_actual_ptr = (void *)res.connector_id_ptr;
	buf += res.count_connectors * 4;
	len -= res.count_connectors * 4;
	res.connector_id_ptr = (uint64_t)connectors;

	uint32_t encoders[1024];
	memcpy(encoders, buf, res.count_encoders * 4);
	char *encoders_buf_ptr = buf;
	size_t encoders_actual_size = res.count_encoders * 4;
	void *encoders_actual_ptr = (void *)res.encoder_id_ptr;
	buf += res.count_encoders * 4;
	len -= res.count_encoders * 4;
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
	}
	return -1;
}
