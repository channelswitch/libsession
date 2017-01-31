#ifndef SESSION_DRI_H
#define SESSION_DRI_H
#include <stdint.h>

struct session_dri_resources {
	uint32_t *fb_id_ptr;
	uint32_t *crtc_id_ptr;
	uint32_t *connector_id_ptr;
	uint32_t *encoder_id_ptr;
	uint32_t count_fbs;
	uint32_t count_crtcs;
	uint32_t count_connectors;
	uint32_t count_encoders;
	uint32_t min_width, max_width;
	uint32_t min_height, max_height;
};

struct session_dri_modeinfo {
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
	uint32_t vrefresh;
	uint32_t flags;
	uint32_t type;
	char name[32];
};

struct session_dri_crtc {
	uint32_t id;
	uint32_t *set_connectors_ptr;
	uint32_t n_connectors;
	uint32_t fb;
	uint32_t x, y;
	uint32_t gamma_size;
	uint32_t mode_valid;
	struct session_dri_modeinfo mode;
};

struct session_dri_connector {
	uint32_t *encoders_ptr;
	struct session_dri_modeinfo *modes_ptr;
	uint32_t *props_ptr;
	uint64_t *prop_values_ptr;
	uint32_t count_modes;
	uint32_t count_props;
	uint32_t count_encoders;
	uint32_t encoder_id;
	uint32_t connector_id;
	uint32_t connector_type;
	uint32_t connector_type_id;
	uint32_t connection;
	uint32_t mm_width, mm_height;
	uint32_t subpixel;
};

struct session_dri_encoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
};
#endif
