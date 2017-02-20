/* Automatically generated */
#include "ioctls.h"
#include "../drm.h"
#include "../radeon_drm.h"
#include <linux/fuse.h>
#include <string.h>

static void parse_drm_ioctl_set_version(void *user, uint64_t unique,
		uint64_t inarg, char *buf, int len, int outsize)
{
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	if(len == 0) goto checkpoint_0;

	if(outsize == 0) goto checkpoint_0;

	send_modeset_ioctl_to_user(user, unique, DRM_IOCTL_SET_VERSION,
		inarg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_set_version);
	++n_iovs;
	retry_ioctl(user, unique, fiov, n_iovs, n_iovs == 1);
}

static void parse_drm_ioctl_get_unique(void *user, uint64_t unique,
		uint64_t inarg, char *buf, int len, int outsize)
{
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	if(len == 0) goto checkpoint_0;
	struct drm_unique uniq;
	memcpy(&uniq, buf, sizeof(struct drm_unique));
	buf += sizeof(struct drm_unique);
	len -= sizeof(struct drm_unique);
	if(uniq.unique_len != 0 && len == 0) goto checkpoint_1;

	if(outsize == 0) goto checkpoint_1;

	send_modeset_ioctl_to_user(user, unique, DRM_IOCTL_GET_UNIQUE,
		inarg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[2];
checkpoint_1:
	if(uniq.unique_len > 4096) goto return_error;
	fiov[1].base = (uint64_t)uniq.unique;
	fiov[1].len = uniq.unique_len;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_unique);
	++n_iovs;
	retry_ioctl(user, unique, fiov, n_iovs, n_iovs == 2);
	return;

return_error:
	return_ioctl_error(user, unique, buf0, len0);
}

static void parse_drm_ioctl_version(void *user, uint64_t unique,
		uint64_t inarg, char *buf, int len, int outsize)
{
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	if(len == 0) goto checkpoint_0;
	struct drm_version version;
	memcpy(&version, buf, sizeof(struct drm_version));
	buf += sizeof(struct drm_version);
	len -= sizeof(struct drm_version);
	if(version.name_len != 0 && len == 0) goto checkpoint_1;

	if(outsize == 0) goto checkpoint_1;

	send_modeset_ioctl_to_user(user, unique, DRM_IOCTL_VERSION,
		inarg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[4];
checkpoint_1:
	if(version.desc_len > 4096) goto return_error;
	fiov[3].base = (uint64_t)version.desc;
	fiov[3].len = version.desc_len;
	++n_iovs;
	if(version.date_len > 4096) goto return_error;
	fiov[2].base = (uint64_t)version.date;
	fiov[2].len = version.date_len;
	++n_iovs;
	if(version.name_len > 4096) goto return_error;
	fiov[1].base = (uint64_t)version.name;
	fiov[1].len = version.name_len;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_version);
	++n_iovs;
	retry_ioctl(user, unique, fiov, n_iovs, n_iovs == 4);
	return;

return_error:
	return_ioctl_error(user, unique, buf0, len0);
}

static void parse_drm_ioctl_mode_getresources(void *user, uint64_t unique,
		uint64_t inarg, char *buf, int len, int outsize)
{
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	if(len == 0) goto checkpoint_0;
	struct drm_mode_card_res res;
	memcpy(&res, buf, sizeof(struct drm_mode_card_res));
	buf += sizeof(struct drm_mode_card_res);
	len -= sizeof(struct drm_mode_card_res);
	if(res.count_fbs * 4 != 0 && len == 0) goto checkpoint_1;

	if(outsize == 0) goto checkpoint_1;

	send_modeset_ioctl_to_user(user, unique, DRM_IOCTL_MODE_GETRESOURCES,
		inarg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[5];
checkpoint_1:
	if(res.count_encoders * 4 > 1024) goto return_error;
	fiov[4].base = (uint64_t)res.encoder_id_ptr;
	fiov[4].len = res.count_encoders * 4;
	++n_iovs;
	if(res.count_connectors * 4 > 1024) goto return_error;
	fiov[3].base = (uint64_t)res.connector_id_ptr;
	fiov[3].len = res.count_connectors * 4;
	++n_iovs;
	if(res.count_crtcs * 4 > 1024) goto return_error;
	fiov[2].base = (uint64_t)res.crtc_id_ptr;
	fiov[2].len = res.count_crtcs * 4;
	++n_iovs;
	if(res.count_fbs * 4 > 1024) goto return_error;
	fiov[1].base = (uint64_t)res.fb_id_ptr;
	fiov[1].len = res.count_fbs * 4;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_card_res);
	++n_iovs;
	retry_ioctl(user, unique, fiov, n_iovs, n_iovs == 5);
	return;

return_error:
	return_ioctl_error(user, unique, buf0, len0);
}

void handle_ioctl(void *user, uint64_t unique, uint64_t cmd,
		uint64_t inarg, char *buf, int len, int outsize)
{
	switch(cmd) {
		case DRM_IOCTL_SET_VERSION:
			parse_drm_ioctl_set_version(
				user, unique, inarg, buf, len, outsize);
			break;
		case DRM_IOCTL_GET_UNIQUE:
			parse_drm_ioctl_get_unique(
				user, unique, inarg, buf, len, outsize);
			break;
		case DRM_IOCTL_VERSION:
			parse_drm_ioctl_version(
				user, unique, inarg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_GETRESOURCES:
			parse_drm_ioctl_mode_getresources(
				user, unique, inarg, buf, len, outsize);
			break;
		default:
			return_ioctl_error(user, unique, buf, len);
	}
}
