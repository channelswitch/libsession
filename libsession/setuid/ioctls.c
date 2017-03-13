/* Automatically generated */
#include "ioctls.h"
#include "../drm.h"
#include "../radeon_drm.h"
#include <linux/fuse.h>
#include <string.h>

static void parse_drm_ioctl_set_version(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;

	if(outsize == 0) goto checkpoint_0;

	ioctls_modeset(user, unique, DRM_IOCTL_SET_VERSION,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_set_version);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_get_unique(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
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

	ioctls_modeset(user, unique, DRM_IOCTL_GET_UNIQUE,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[2];
checkpoint_1:
	if(uniq.unique_len > sizeof(char) * 4096) goto return_error;
	fiov[1].base = (uint64_t)uniq.unique;
	fiov[1].len = uniq.unique_len;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_unique);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 2 ? n_iovs : 0);
	return;

return_error:
	ioctls_return_error(user, unique, DRM_IOCTL_GET_UNIQUE,
			arg, buf0, len0);
}

static void parse_drm_ioctl_version(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
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

	ioctls_modeset(user, unique, DRM_IOCTL_VERSION,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[4];
checkpoint_1:
	if(version.desc_len > sizeof(char) * 4096) goto return_error;
	fiov[3].base = (uint64_t)version.desc;
	fiov[3].len = version.desc_len;
	++n_iovs;
	if(version.date_len > sizeof(char) * 4096) goto return_error;
	fiov[2].base = (uint64_t)version.date;
	fiov[2].len = version.date_len;
	++n_iovs;
	if(version.name_len > sizeof(char) * 4096) goto return_error;
	fiov[1].base = (uint64_t)version.name;
	fiov[1].len = version.name_len;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_version);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 4 ? n_iovs : 0);
	return;

return_error:
	ioctls_return_error(user, unique, DRM_IOCTL_VERSION,
			arg, buf0, len0);
}

static void parse_drm_ioctl_mode_getresources(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
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

	ioctls_modeset(user, unique, DRM_IOCTL_MODE_GETRESOURCES,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[5];
checkpoint_1:
	if(res.count_encoders * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[4].base = (uint64_t)res.encoder_id_ptr;
	fiov[4].len = res.count_encoders * 4;
	++n_iovs;
	if(res.count_connectors * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[3].base = (uint64_t)res.connector_id_ptr;
	fiov[3].len = res.count_connectors * 4;
	++n_iovs;
	if(res.count_crtcs * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[2].base = (uint64_t)res.crtc_id_ptr;
	fiov[2].len = res.count_crtcs * 4;
	++n_iovs;
	if(res.count_fbs * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[1].base = (uint64_t)res.fb_id_ptr;
	fiov[1].len = res.count_fbs * 4;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_card_res);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 5 ? n_iovs : 0);
	return;

return_error:
	ioctls_return_error(user, unique, DRM_IOCTL_MODE_GETRESOURCES,
			arg, buf0, len0);
}

static void parse_drm_ioctl_mode_getconnector(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_get_connector get_connector;
	memcpy(&get_connector, buf, sizeof(struct drm_mode_get_connector));
	buf += sizeof(struct drm_mode_get_connector);
	len -= sizeof(struct drm_mode_get_connector);
	if(get_connector.count_encoders * 4 != 0 && len == 0) goto checkpoint_1;

	if(outsize == 0) goto checkpoint_1;

	ioctls_modeset(user, unique, DRM_IOCTL_MODE_GETCONNECTOR,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[5];
checkpoint_1:
	if(get_connector.count_props * 8 > sizeof(uint64_t) * 1024) goto return_error;
	fiov[4].base = (uint64_t)get_connector.prop_values_ptr;
	fiov[4].len = get_connector.count_props * 8;
	++n_iovs;
	if(get_connector.count_props * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[3].base = (uint64_t)get_connector.props_ptr;
	fiov[3].len = get_connector.count_props * 4;
	++n_iovs;
	if(get_connector.count_modes * sizeof(struct drm_mode_modeinfo) > sizeof(struct drm_mode_modeinfo) * 1024) goto return_error;
	fiov[2].base = (uint64_t)get_connector.modes_ptr;
	fiov[2].len = get_connector.count_modes * sizeof(struct drm_mode_modeinfo);
	++n_iovs;
	if(get_connector.count_encoders * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[1].base = (uint64_t)get_connector.encoders_ptr;
	fiov[1].len = get_connector.count_encoders * 4;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_get_connector);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 5 ? n_iovs : 0);
	return;

return_error:
	ioctls_return_error(user, unique, DRM_IOCTL_MODE_GETCONNECTOR,
			arg, buf0, len0);
}

static void parse_drm_ioctl_mode_getencoder(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;

	if(outsize == 0) goto checkpoint_0;

	ioctls_modeset(user, unique, DRM_IOCTL_MODE_GETENCODER,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_get_encoder);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_info(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_info info;
	char *info_buf_ptr = buf;
	memcpy(&info, buf, sizeof(struct drm_radeon_info));
	buf += sizeof(struct drm_radeon_info);
	len -= sizeof(struct drm_radeon_info);
	if(len == 0) goto checkpoint_1;
	uint32_t value;
	char *value_buf_ptr = buf;
	memcpy(&value, buf, sizeof(uint32_t));

	if(outsize == 0) goto checkpoint_1;

	size_t info_actual_size = sizeof(struct drm_radeon_info);
	void *info_actual_ptr = inarg;
	inarg = &info;

	size_t value_actual_size = sizeof(uint32_t);
	void *value_actual_ptr = (void *)info.value;
	info.value = (uint64_t)&value;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_INFO, inarg);
	memcpy(value_buf_ptr, &value, value_actual_size);
	info.value = (uint64_t)value_actual_ptr;

	memcpy(info_buf_ptr, &info, info_actual_size);
	inarg = info_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_INFO,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[2];
checkpoint_1:
	fiov[1].base = (uint64_t)info.value;
	fiov[1].len = sizeof(uint32_t);
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_info);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 2 ? n_iovs : 0);
}

static void parse_drm_ioctl_get_cap(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;

	if(outsize == 0) goto checkpoint_0;

	ioctls_modeset(user, unique, DRM_IOCTL_GET_CAP,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_get_cap);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_mode_create_dumb(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_create_dumb dumb;
	char *dumb_buf_ptr = buf;
	memcpy(&dumb, buf, sizeof(struct drm_mode_create_dumb));

	if(outsize == 0) goto checkpoint_0;

	size_t dumb_actual_size = sizeof(struct drm_mode_create_dumb);
	void *dumb_actual_ptr = inarg;
	inarg = &dumb;

	int return_value = ioctls_render(user, DRM_IOCTL_MODE_CREATE_DUMB, inarg);
	memcpy(dumb_buf_ptr, &dumb, dumb_actual_size);
	inarg = dumb_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_MODE_CREATE_DUMB,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_create_dumb);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_mode_destroy_dumb(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_destroy_dumb dumb;
	char *dumb_buf_ptr = buf;
	memcpy(&dumb, buf, sizeof(struct drm_mode_destroy_dumb));

	if(outsize == 0) goto checkpoint_0;

	size_t dumb_actual_size = sizeof(struct drm_mode_destroy_dumb);
	void *dumb_actual_ptr = inarg;
	inarg = &dumb;

	int return_value = ioctls_render(user, DRM_IOCTL_MODE_DESTROY_DUMB, inarg);
	memcpy(dumb_buf_ptr, &dumb, dumb_actual_size);
	inarg = dumb_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_MODE_DESTROY_DUMB,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_destroy_dumb);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_mode_addfb(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_fb_cmd fb;
	char *fb_buf_ptr = buf;
	memcpy(&fb, buf, sizeof(struct drm_mode_fb_cmd));

	if(outsize == 0) goto checkpoint_0;

	size_t fb_actual_size = sizeof(struct drm_mode_fb_cmd);
	void *fb_actual_ptr = inarg;
	inarg = &fb;

	int return_value = ioctls_render(user, DRM_IOCTL_MODE_ADDFB, inarg);
	memcpy(fb_buf_ptr, &fb, fb_actual_size);
	inarg = fb_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_MODE_ADDFB,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_fb_cmd);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_mode_rmfb(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_fb_cmd fb;
	char *fb_buf_ptr = buf;
	memcpy(&fb, buf, sizeof(struct drm_mode_fb_cmd));

	if(outsize == 0) goto checkpoint_0;

	size_t fb_actual_size = sizeof(struct drm_mode_fb_cmd);
	void *fb_actual_ptr = inarg;
	inarg = &fb;

	int return_value = ioctls_render(user, DRM_IOCTL_MODE_RMFB, inarg);
	memcpy(fb_buf_ptr, &fb, fb_actual_size);
	inarg = fb_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_MODE_RMFB,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_fb_cmd);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_mode_getcrtc(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_mode_crtc crtc;
	memcpy(&crtc, buf, sizeof(struct drm_mode_crtc));
	buf += sizeof(struct drm_mode_crtc);
	len -= sizeof(struct drm_mode_crtc);
	if(crtc.count_connectors * 4 != 0 && len == 0) goto checkpoint_1;

	if(outsize == 0) goto checkpoint_1;

	ioctls_modeset(user, unique, DRM_IOCTL_MODE_GETCRTC,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[2];
checkpoint_1:
	if(crtc.count_connectors * 4 > sizeof(uint32_t) * 1024) goto return_error;
	fiov[1].base = (uint64_t)crtc.set_connectors_ptr;
	fiov[1].len = crtc.count_connectors * 4;
	++n_iovs;
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_crtc);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 2 ? n_iovs : 0);
	return;

return_error:
	ioctls_return_error(user, unique, DRM_IOCTL_MODE_GETCRTC,
			arg, buf0, len0);
}

static void parse_drm_ioctl_set_master(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	ioctls_modeset(user, unique, DRM_IOCTL_SET_MASTER, arg, NULL, 0);
}

static void parse_drm_ioctl_drop_master(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	ioctls_modeset(user, unique, DRM_IOCTL_DROP_MASTER, arg, NULL, 0);
}

static void parse_drm_ioctl_mode_setgamma(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	ioctls_modeset(user, unique, DRM_IOCTL_MODE_SETGAMMA, arg, NULL, 0);
}

static void parse_drm_ioctl_mode_setcrtc(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	ioctls_modeset(user, unique, DRM_IOCTL_MODE_SETCRTC, arg, NULL, 0);
}

static void parse_drm_ioctl_radeon_gem_info(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_info info;
	char *info_buf_ptr = buf;
	memcpy(&info, buf, sizeof(struct drm_radeon_gem_info));

	if(outsize == 0) goto checkpoint_0;

	size_t info_actual_size = sizeof(struct drm_radeon_gem_info);
	void *info_actual_ptr = inarg;
	inarg = &info;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_INFO, inarg);
	memcpy(info_buf_ptr, &info, info_actual_size);
	inarg = info_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_INFO,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_info);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_gem_create(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_create create;
	char *create_buf_ptr = buf;
	memcpy(&create, buf, sizeof(struct drm_radeon_gem_create));

	if(outsize == 0) goto checkpoint_0;

	size_t create_actual_size = sizeof(struct drm_radeon_gem_create);
	void *create_actual_ptr = inarg;
	inarg = &create;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_CREATE, inarg);
	memcpy(create_buf_ptr, &create, create_actual_size);
	inarg = create_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_CREATE,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_create);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_gem_close(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_gem_close close;
	char *close_buf_ptr = buf;
	memcpy(&close, buf, sizeof(struct drm_gem_close));

	if(outsize == 0) goto checkpoint_0;

	size_t close_actual_size = sizeof(struct drm_gem_close);
	void *close_actual_ptr = inarg;
	inarg = &close;

	int return_value = ioctls_render(user, DRM_IOCTL_GEM_CLOSE, inarg);
	memcpy(close_buf_ptr, &close, close_actual_size);
	inarg = close_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_GEM_CLOSE,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_gem_close);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_gem_mmap(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_mmap mmap;
	char *mmap_buf_ptr = buf;
	memcpy(&mmap, buf, sizeof(struct drm_radeon_gem_mmap));

	if(outsize == 0) goto checkpoint_0;

	size_t mmap_actual_size = sizeof(struct drm_radeon_gem_mmap);
	void *mmap_actual_ptr = inarg;
	inarg = &mmap;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_MMAP, inarg);
	memcpy(mmap_buf_ptr, &mmap, mmap_actual_size);
	inarg = mmap_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_MMAP,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_mmap);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_gem_set_tiling(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_set_tiling tiling;
	char *tiling_buf_ptr = buf;
	memcpy(&tiling, buf, sizeof(struct drm_radeon_gem_set_tiling));

	if(outsize == 0) goto checkpoint_0;

	size_t tiling_actual_size = sizeof(struct drm_radeon_gem_set_tiling);
	void *tiling_actual_ptr = inarg;
	inarg = &tiling;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_SET_TILING, inarg);
	memcpy(tiling_buf_ptr, &tiling, tiling_actual_size);
	inarg = tiling_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_SET_TILING,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_set_tiling);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_gem_busy(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_busy busy;
	char *busy_buf_ptr = buf;
	memcpy(&busy, buf, sizeof(struct drm_radeon_gem_busy));

	if(outsize == 0) goto checkpoint_0;

	size_t busy_actual_size = sizeof(struct drm_radeon_gem_busy);
	void *busy_actual_ptr = inarg;
	inarg = &busy;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_BUSY, inarg);
	memcpy(busy_buf_ptr, &busy, busy_actual_size);
	inarg = busy_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_BUSY,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_busy);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_gem_wait_idle(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;
	struct drm_radeon_gem_wait_idle busy;
	char *busy_buf_ptr = buf;
	memcpy(&busy, buf, sizeof(struct drm_radeon_gem_wait_idle));

	if(outsize == 0) goto checkpoint_0;

	size_t busy_actual_size = sizeof(struct drm_radeon_gem_wait_idle);
	void *busy_actual_ptr = inarg;
	inarg = &busy;

	int return_value = ioctls_render(user, DRM_IOCTL_RADEON_GEM_WAIT_IDLE, inarg);
	memcpy(busy_buf_ptr, &busy, busy_actual_size);
	inarg = busy_actual_ptr;

	ioctls_render_done(user, return_value, unique, DRM_IOCTL_RADEON_GEM_WAIT_IDLE,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_radeon_gem_wait_idle);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

static void parse_drm_ioctl_radeon_cs(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;
	int last_retry = 0;

	if(len == 0) goto checkpoint_0;

	struct drm_radeon_cs cs;
	memcpy(&cs, buf, sizeof cs);
	char *cs_buf_ptr = buf;
	buf += sizeof cs;
	len -= sizeof cs;
	if(cs.num_chunks != 0 && len == 0) goto checkpoint_1;

	uint64_t chunk_ptrs[4096];
	if(cs.num_chunks > sizeof chunk_ptrs / sizeof chunk_ptrs[0]) goto err;
	memcpy(chunk_ptrs, buf, 8 * cs.num_chunks);
	char *chunk_ptrs_buf_ptr = buf;
	buf += 8 * cs.num_chunks;
	len -= 8 * cs.num_chunks;
	if(cs.num_chunks != 0 && len == 0) goto checkpoint_2;

	struct drm_radeon_cs_chunk chunks[
		sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	if(cs.num_chunks > sizeof chunks / sizeof chunks[0]) goto err;
	memcpy(chunks, buf, sizeof chunks[0] * cs.num_chunks);
	char *chunks_buf_ptr = buf;
	buf += sizeof chunks[0] * cs.num_chunks;
	len -= sizeof chunks[0] * cs.num_chunks;

	size_t chunks_data_sz = 0;
	int i;
	for(i = 0; i < cs.num_chunks; ++i) {
		chunks_data_sz += chunks[i].length_dw * 4;
	}
	if(chunks_data_sz != 0 && len == 0) goto checkpoint_3;
	char chunk_data[0x21000];
	if(chunks_data_sz > sizeof chunk_data) goto err;
	memcpy(chunk_data, buf, chunks_data_sz);
	char *chunk_data_buf_ptr = buf;
	buf += chunks_data_sz;
	len -= chunks_data_sz;

	if(!outsize) goto checkpoint_3;

	uint64_t real_chunks = cs.chunks;
	cs.chunks = (uint64_t)&chunk_ptrs;

	uint64_t real_chunk_ptrs[sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	for(i = 0; i < cs.num_chunks; ++i) {
		real_chunk_ptrs[i] = chunk_ptrs[i];
		chunk_ptrs[i] = (uint64_t)&chunks[i];
	}

	uint64_t real_chunk_datas[sizeof chunk_ptrs / sizeof chunk_ptrs[0]];
	unsigned pos = 0;
	for(i = 0; i < cs.num_chunks; ++i) {
		real_chunk_datas[i] = chunks[i].chunk_data;
		chunks[i].chunk_data = (uint64_t)(chunk_data + pos);
		pos += chunks[i].length_dw * 4;
	}

	unsigned real_num_chunks = cs.num_chunks;
	int status = ioctls_render(user, DRM_IOCTL_RADEON_CS, &cs);

	memcpy(chunk_data_buf_ptr, chunk_data, chunks_data_sz);

	for(i = 0; i < cs.num_chunks; ++i) {
		chunks[i].chunk_data = real_chunk_datas[i];
	}
	memcpy(chunks_buf_ptr, chunks, sizeof chunks[0] * real_num_chunks);

	for(i = 0; i < cs.num_chunks; ++i) {
		chunk_ptrs[i] = real_chunk_ptrs[i];
	}
	memcpy(chunk_ptrs_buf_ptr, chunk_ptrs, 8 * real_num_chunks);

	cs.chunks = real_chunks;
	memcpy(cs_buf_ptr, &cs, sizeof cs);

	/* Sizeof cs instead of len0 because of readonly thing. */
	ioctls_render_done(user, status, unique, DRM_IOCTL_RADEON_CS,
			arg, buf0, sizeof cs);
	return;

	struct fuse_ioctl_iovec fiov[2 + 2 * sizeof chunk_ptrs /
		sizeof chunk_ptrs[0]];
checkpoint_3:
	last_retry = 1;
	for(i = 0; i < cs.num_chunks; ++i) {
		unsigned pos = 2 + cs.num_chunks + i;
		fiov[pos].base = chunks[i].chunk_data;
		fiov[pos].len = chunks[i].length_dw * 4;
		++n_iovs;
	}
checkpoint_2:
	for(i = 0; i < cs.num_chunks; ++i) {
		unsigned pos = 2 + i;
		fiov[pos].base = chunk_ptrs[i];
		fiov[pos].len = sizeof chunks[0];
		++n_iovs;
	}
checkpoint_1:
	fiov[1].base = cs.chunks;
	fiov[1].len = 8 * cs.num_chunks;
	++n_iovs;
checkpoint_0:
	fiov[0].base = arg;
	fiov[0].len = sizeof cs;
	++n_iovs;

	ioctls_retry(user, unique, fiov, n_iovs, last_retry);
	return;

err:
	ioctls_return_error(user, unique, DRM_IOCTL_RADEON_CS,
			arg, buf0, len0);
}

static void parse_drm_ioctl_mode_page_flip(void *user, uint64_t unique,
		uint64_t arg, char *buf, int len, int outsize)
{
	void *inarg = (void *)arg;
	char *buf0 = buf;
	int len0 = len;
	int n_iovs = 0;

	if(len == 0) goto checkpoint_0;

	if(outsize == 0) goto checkpoint_0;

	ioctls_modeset(user, unique, DRM_IOCTL_MODE_PAGE_FLIP,
			arg, buf0, len0);
	return;

	struct fuse_ioctl_iovec fiov[1];
checkpoint_0:
	fiov[0].base = (uint64_t)inarg;
	fiov[0].len = sizeof(struct drm_mode_crtc_page_flip);
	++n_iovs;
	ioctls_retry(user, unique, fiov, n_iovs, n_iovs == 1 ? n_iovs : 0);
}

void ioctls_handle(void *user, uint64_t unique, uint32_t cmd,
		uint64_t arg, char *buf, int len, int outsize)
{
	switch(cmd) {
		case DRM_IOCTL_SET_VERSION:
			parse_drm_ioctl_set_version(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_GET_UNIQUE:
			parse_drm_ioctl_get_unique(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_VERSION:
			parse_drm_ioctl_version(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_GETRESOURCES:
			parse_drm_ioctl_mode_getresources(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_GETCONNECTOR:
			parse_drm_ioctl_mode_getconnector(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_GETENCODER:
			parse_drm_ioctl_mode_getencoder(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_INFO:
			parse_drm_ioctl_radeon_info(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_GET_CAP:
			parse_drm_ioctl_get_cap(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_CREATE_DUMB:
			parse_drm_ioctl_mode_create_dumb(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_DESTROY_DUMB:
			parse_drm_ioctl_mode_destroy_dumb(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_ADDFB:
			parse_drm_ioctl_mode_addfb(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_RMFB:
			parse_drm_ioctl_mode_rmfb(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_GETCRTC:
			parse_drm_ioctl_mode_getcrtc(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_SET_MASTER:
			parse_drm_ioctl_set_master(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_DROP_MASTER:
			parse_drm_ioctl_drop_master(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_SETGAMMA:
			parse_drm_ioctl_mode_setgamma(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_SETCRTC:
			parse_drm_ioctl_mode_setcrtc(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_INFO:
			parse_drm_ioctl_radeon_gem_info(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_CREATE:
			parse_drm_ioctl_radeon_gem_create(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_GEM_CLOSE:
			parse_drm_ioctl_gem_close(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_MMAP:
			parse_drm_ioctl_radeon_gem_mmap(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_SET_TILING:
			parse_drm_ioctl_radeon_gem_set_tiling(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_BUSY:
			parse_drm_ioctl_radeon_gem_busy(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_GEM_WAIT_IDLE:
			parse_drm_ioctl_radeon_gem_wait_idle(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_RADEON_CS:
			parse_drm_ioctl_radeon_cs(
				user, unique, arg, buf, len, outsize);
			break;
		case DRM_IOCTL_MODE_PAGE_FLIP:
			parse_drm_ioctl_mode_page_flip(
				user, unique, arg, buf, len, outsize);
			break;
		default:
			ioctls_return_error(user, unique, cmd, arg, buf, len);
	}
}
