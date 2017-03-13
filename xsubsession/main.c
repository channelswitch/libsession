#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <session.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glu.h>

struct data;
static void *register_fd(void *user, int fd, unsigned flags,
		void (*f)(void *user1, int event), void *user1);
static void unregister_fd(void *ptr);
static void exit_callback(void *user);
static void on_sigchld(void *user);
static void framebuffer(void *user, uint32_t name, int w, int h);
static void x_event(void *user, int event);
static GLuint name_to_texture(struct data *r, uint32_t name, int w, int h);
static void draw(struct data *data);
static void resources(void *user, unsigned sequence);
static int modeset_ioctl(void *user, uint32_t cmd, void *arg);

struct fb {
	uint32_t name;
	EGLImage image;
	GLuint texture;
};

struct data {
	int w, h;

	struct session *s;

	int epoll_fd;
	unsigned to_exit;
	Display *x_display;
	EGLDisplay egl_display;
	EGLContext egl_context;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

	enum {
		HAVE_FB = 1 << 0,
	} flags;

	unsigned n_fbs;
	struct fb *fbs;

	GLint fb_texture;
};

struct registered_fd {
	struct data *data;
	int fd;
	void (*f)(void *user, int event);
	void *user1;
};	

int main(int argc, char **argv)
{
	int r = 1;

	static struct option long_options[] = {
		{ "resolution", required_argument, NULL, 'r' },
		{ NULL, 0, NULL, 0 }
	};

	struct data data;
	data.flags = 0;
	data.to_exit = 0;
	data.w = 800;
	data.h = 600;
	while(1) {
		int index;
		int c = getopt_long(argc, argv, "r:", long_options, &index);
		if(c == -1) break;

		if(c == 'r') {
			/* Resolution */
			int scan_len;
			if(sscanf(optarg, "%dx%d%n",
						&data.w, &data.h,
						&scan_len) != 2
					|| scan_len != strlen(optarg)
					|| data.w < 1 || data.h < 1) {
				fprintf(stderr, "%s is not a valid "
						"resolution. It must have the "
						"form WxH.\n", optarg);
				goto e_args;
			}
		}
		else {
			goto e_args;
		}
	}

	int use_login_shell = 0;
	char pw_buf[1000];
	struct passwd upw;
	char *default_argv[2];

	if(argc - optind < 1) {
		struct passwd *result;
		use_login_shell = 1;
		int status = getpwuid_r(getuid(), &upw, pw_buf, sizeof pw_buf,
				&result);
		if(!result) {
			fprintf(stderr, "Could not get login shell: %s.\n",
					strerror(status));
			goto e_args;
		}

		default_argv[0] = upw.pw_shell;
		default_argv[1] = NULL;
	}

	data.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if(data.epoll_fd < 0) goto e_epoll_create;

	data.x_display = XOpenDisplay(NULL);
	if(!data.x_display) {
		fprintf(stderr, "Could not open X display.\n");
		goto e_open_display;
	}

	data.egl_display = eglGetDisplay(data.x_display);
	if(data.egl_display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Error opening EGL display.\n");
		goto e_egl_display;
	}

	int major, minor;
	if(!eglInitialize(data.egl_display, &major, &minor)) {
		fprintf(stderr, "Error in eglInitialize.\n");
		goto e_egl_init;
	}

	EGLint egl_attributes[] = {
		EGL_ALPHA_SIZE, 8,
		EGL_BIND_TO_TEXTURE_RGB, EGL_TRUE,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE,
	};
	EGLConfig egl_config;
	int n_configs;
	if(!eglChooseConfig(data.egl_display, egl_attributes, &egl_config, 1,
				&n_configs)) {
		fprintf(stderr, "Error choosing EGL config: %d.\n",
				eglGetError());
		goto e_choose_egl_config;
	}

	VisualID x_visual_id = 0;
	if(!eglGetConfigAttrib(data.egl_display, egl_config,
				EGL_NATIVE_VISUAL_ID, (EGLint *)&x_visual_id)) {
		fprintf(stderr, "Error getting X visual.\n");
		goto e_get_x_visual;
	}

	XVisualInfo like_this, *visual_info;
	like_this.visualid = x_visual_id;
	int num_visuals;
	visual_info = XGetVisualInfo(data.x_display, VisualIDMask, &like_this,
			&num_visuals);
	if(!visual_info || !num_visuals) {
		fprintf(stderr, "Error getting visual info.\n");
		goto e_get_visual_info;
	}

	Colormap colormap = XCreateColormap(data.x_display,
			DefaultRootWindow(data.x_display), visual_info->visual,
			AllocNone);

	XSetWindowAttributes x_window_attributes = {
		.colormap = colormap,
		.event_mask = ExposureMask | KeyPressMask,
	};

	Window x_window = XCreateWindow(data.x_display,
			DefaultRootWindow(data.x_display),
			0, 0, data.w, data.h, 0, visual_info->depth,
			InputOutput, visual_info->visual,
			CWColormap | CWEventMask,
			&x_window_attributes);

	XStoreName(data.x_display, x_window, "xsubsession");

	EGLSurface egl_surface = eglCreateWindowSurface(data.egl_display,
			egl_config, x_window, NULL);
	if(egl_surface == EGL_NO_SURFACE) {
		fprintf(stderr, "Error from eglCreateWindowSurface(): %d.\n",
				eglGetError());
		goto e_egl_surface;
	}

	if(!eglBindAPI(EGL_OPENGL_API)) {
		fprintf(stderr, "Error in eglBindAPI(EGL_OPENGL_API).\n");
		goto e_bind_api;
	}

	data.egl_context = eglCreateContext(data.egl_display, egl_config,
			EGL_NO_CONTEXT, NULL);
	if(data.egl_context == EGL_NO_CONTEXT) {
		fprintf(stderr, "Error from eglCreateContext(): %d.\n",
				eglGetError());
		goto e_egl_context;
	}

	data.glEGLImageTargetTexture2DOES = (void *)eglGetProcAddress(
			"glEGLImageTargetTexture2DOES");
	if(!data.glEGLImageTargetTexture2DOES) {
		fprintf(stderr, "Could not get addres for "
				"glEGLImageTargetTexture2DOES.\n");
		goto e_get_proc_address;
	}

	eglMakeCurrent(data.egl_display, egl_surface, egl_surface,
			data.egl_context);

	void *x_ptr;
	x_ptr = register_fd(&data, ConnectionNumber(data.x_display), 1,
			x_event, &data);
	if(!x_ptr) goto e_register_x;

	data.n_fbs = 0;
	data.fbs = NULL;

	struct session *session;
	struct session_args args;
	args.flags = SESSION_ON_SIGCHLD
		| SESSION_FRAMEBUFFER_GEM;
	args.on_sigchld = on_sigchld;
	args.framebuffer_gem = framebuffer;
	args.resources = resources;
	if(session_new(&session, register_fd, unregister_fd,
				exit_callback, &data,
				&args, modeset_ioctl)) goto e_session;
	data.s = session;

	session_start(session,
			use_login_shell ? upw.pw_shell : argv[optind],
			use_login_shell ? default_argv : argv + optind);

	XMapWindow(data.x_display, x_window);

	x_event(&data, 1);
	while(!data.to_exit) {
		while(1) {
			struct epoll_event ev;
			int status = epoll_wait(data.epoll_fd, &ev, 1, 0);
			if(!status) break;
			struct registered_fd *reg = ev.data.ptr;
			reg->f(reg->user1,
					(ev.events & EPOLLIN ? 1 : 0) |
					(ev.events & EPOLLOUT ? 2 : 0) |
					(ev.events & EPOLLERR ? 4 : 0) |
					(ev.events & EPOLLHUP ? 8 : 0));
		}
		draw(&data);
		eglSwapBuffers(data.egl_display, egl_surface);
	}

	r = 0;

	session_free(session);
e_session:
	//TODO: actually free textures
	free(data.fbs);
	unregister_fd(x_ptr);
e_register_x:
e_get_proc_address:
	eglDestroyContext(data.egl_display, data.egl_context);
e_egl_context:
e_bind_api:
	eglDestroySurface(data.egl_display, egl_surface);
e_egl_surface:
	XDestroyWindow(data.x_display, x_window);
	XFreeColormap(data.x_display, colormap);
	XFree(visual_info);
e_get_visual_info:
e_get_x_visual:
e_choose_egl_config:
	eglTerminate(data.egl_display);
e_egl_init:
e_egl_display:
	XCloseDisplay(data.x_display);
e_open_display:
	close(data.epoll_fd);
e_epoll_create:
e_args:
	return r;
}

static void *register_fd(
		void *user,
		int fd,
		unsigned flags,
		void (*f)(void *user1, int event),
		void *user1)
{
	struct data *data = user;
	struct registered_fd *reg = malloc(sizeof *reg);
	if(!reg) {
		fprintf(stderr, "Malloc returned NULL at %s: %d.", __FILE__,
				__LINE__);
		return NULL;
	}

	reg->data = data;
	reg->fd = fd;
	reg->f = f;
	reg->user1 = user1;
	struct epoll_event ev;
	ev.events =
		(flags & 1 ? EPOLLIN : 0) |
		(flags & 2 ? EPOLLOUT : 0) |
		(flags & 4 ? EPOLLET : 0);
	ev.data.ptr = reg;
	if(epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		fprintf(stderr, "Could not add file descriptor to epoll: "
				"%s.", strerror(errno));
		free(reg);
		return NULL;
	}

	return reg;
}

static void unregister_fd(void *ptr)
{
	struct registered_fd *reg = ptr;
	epoll_ctl(reg->data->epoll_fd, EPOLL_CTL_DEL, reg->fd, NULL);
	free(reg);
}

static void exit_callback(void *user)
{
	struct data *data = user;
	data->to_exit = 1;
}

static void on_sigchld(void *user)
{
	struct data *data = user;
	data->to_exit = 1;
}

static void framebuffer(void *user, uint32_t name, int w, int h)
{
	struct data *data = user;
	data->flags |= HAVE_FB;
	data->fb_texture = name_to_texture(data, name, w, h);
}

static void x_event(void *user, int event)
{
	struct data *data = user;
	while(XPending(data->x_display)) {
		XEvent xev;
		XNextEvent(data->x_display, &xev);
	}
}

static GLuint name_to_texture(struct data *r, uint32_t name, int w, int h)
{
	unsigned i;
	for(i = 0; i < r->n_fbs; ++i) {
		if(r->fbs[i].name == name) return r->fbs[i].texture;
	}

	// vdisplay = 1920, vtotal = 2200, hdisplay = 1080, htotal = 1125
	EGLAttrib attributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
		EGL_WIDTH, w,
		EGL_HEIGHT, h,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_STRIDE_MESA, w, // w för kmscube
		EGL_NONE
	};
	EGLImage image;
	image = eglCreateImage(
			r->egl_display,
			r->egl_context,
			EGL_DRM_BUFFER_MESA,
			(EGLClientBuffer)(uint64_t)name,
			attributes);

	if(image == EGL_NO_IMAGE) {
		fprintf(stderr, "Error in eglCreateImage().\n");
		goto e_egl_create_image;
	}

	GLuint texture;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	r->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	struct fb *new_fbs = realloc(r->fbs, sizeof *r->fbs * (r->n_fbs + 1));
	if(!new_fbs) {
		fprintf(stderr, "Null from realloc at %s: %d.\n",
				__FILE__, __LINE__);
		goto e_realloc;
	}
	r->fbs = new_fbs;

	struct fb *fb = &r->fbs[r->n_fbs];
	++r->n_fbs;

	fb->name = name;
	fb->image = image;
	fb->texture = texture;

	return fb->texture;

e_realloc:
	eglDestroyImage(r->egl_display, image);
e_egl_create_image:
	return 0;
}

static void draw(struct data *data)
{
	int width = data->w, height = data->h;

	glClearColor(0.0, 0.0, 0.0, 0.0);

	glViewport(0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, 1, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -10.0);
	static GLfloat verts[][2] = {
		{ -1, -1 },
		{  1, -1 },
		{ -1,  1 },
		{  1, -1 },
		{  1,  1 },
		{ -1,  1 },
	};
	static GLfloat texcoords[][2] = {
		{ 1, 1 },
		{ 0, 1 },
		{ 1, 0 },
		{ 0, 1 },
		{ 0, 0 },
		{ 1, 0 },
	};

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if(data->flags & HAVE_FB) {
		glBindTexture(GL_TEXTURE_2D, data->fb_texture);
		glEnable(GL_TEXTURE_2D);

		glVertexPointer(2, GL_FLOAT, 0, verts);
		glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glDrawArrays(GL_TRIANGLES, 0, sizeof verts / sizeof verts[0]);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

/*
 * Screen stuff
 */

#if 0
static uint32_t fb_ids[] = {
};

static uint32_t crtc_ids[] = {
	29, 31, 33, 35, 37, 39
};

static uint32_t connector_ids[] = {
	41, 43, 45, 47
};

static uint32_t encoder_ids[] = {
	40, 42, 44, 46, 48
};

static struct session_dri_resources res = {
#define A(name) \
	.count_##name##s = sizeof name##_ids / sizeof name##_ids[0], \
	.name##_id_ptr = name##_ids
	A(fb),
	A(crtc),
	A(connector),
	A(encoder),
#undef A
	.min_width = 0,
	.max_width = 16384,
	.min_height = 0,
	.max_height = 16384,
};
#endif

static void resources(void *user, unsigned sequence)
{
//	struct data *data = user;
//	session_send_dri_resources(data->s, &res, sequence);
}

#include "../libsession/drm.h"
#include "../libsession/radeon_drm.h"

static void fill_array(void *dst, size_t elem_sz, uint32_t *count, void *src,
		int src_count);
#define FILL(dst, count, src) fill_array(dst, sizeof src[0], &count, src, \
		sizeof src / sizeof src[0])

static int modeset_ioctl(void *user, uint32_t cmd, void *arg)
{
	if(cmd == DRM_IOCTL_SET_VERSION) {
		struct drm_set_version *sv = arg; // 1 4 2 43
		printf("di major = %d\n", sv->drm_di_major);
		printf("di minor = %d\n", sv->drm_di_minor);
		printf("dd major = %d\n", sv->drm_dd_major);
		printf("dd minor = %d\n", sv->drm_dd_minor);
		/* TODO: Check out version <= in version or whatever */
		sv->drm_di_major = 1;
		sv->drm_di_minor = 4;
		sv->drm_dd_major = 2;
		sv->drm_dd_minor = 43;
		return 0;
	}
	else if(cmd == DRM_IOCTL_VERSION) {
//XXX horrible horrible hack to work around libdrm doing fstat on cuse fd and
// seeing the wrong numbers
static int i;++i;//printf("VERSION: %d\n", i);
		char radeon[] = "radeon";
		char r600[] = "r600";
		char *name = i >= 9 ? r600 : radeon;

		struct drm_version *v = arg;
		v->version_major = 2;
		v->version_minor = 43;
		v->version_patchlevel = 0;
		char date[] = "20080528";
		char desc[] = "ATI Radeon";
		strncpy(v->name, name, v->name_len);
		v->name_len = strlen(name);
		strncpy(v->date, date, v->date_len);
		v->date_len = strlen(date);
		strncpy(v->desc, desc, v->desc_len);
		v->desc_len = strlen(desc);
	}
	else if(cmd == DRM_IOCTL_GET_UNIQUE) {
		struct drm_unique *unique = arg;
		//static char unique_str[] = "pci:0000:01:00.0";
		static char unique_str[] = "";
		strncpy(unique->unique, unique_str, unique->unique_len);
		unique->unique_len = strlen(unique_str);
	}
	else if(cmd == DRM_IOCTL_MODE_GETRESOURCES) {
		struct drm_mode_card_res *res = arg;
		static uint32_t fbs[] = { };
		static uint32_t crtcs[] = { 29 };//, 31, 33, 35, 37, 39 };
		static uint32_t connectors[] = { 45 };
		static uint32_t encoders[] = { 44 };
//printf("sizes in: %d %d %d %d\n", res->count_fbs, res->count_crtcs, res->count_connectors, res->count_encoders);
#define COPY_ARRAY(name) memcpy((void *)res->name##_id_ptr, name##s, 4 * (res->count_##name##s > (sizeof name##s / sizeof name##s[0]) ? (sizeof name##s / sizeof name##s[0]) : res->count_##name##s)); res->count_##name##s = (sizeof name##s / sizeof name##s[0]);
		COPY_ARRAY(fb);
		COPY_ARRAY(crtc);
		COPY_ARRAY(connector);
		COPY_ARRAY(encoder);
		res->min_width = 0;
		res->max_width = 16384;
		res->min_height = 0;
		res->max_height = 16384;
//printf("sizes out: %d %d %d %d\n", res->count_fbs, res->count_crtcs, res->count_connectors, res->count_encoders);
	}
	else if(cmd == DRM_IOCTL_MODE_GETCONNECTOR) {
		struct drm_mode_get_connector *c = arg;
		c->encoder_id = 44;
		c->connector_type = 11;
		c->connector_type_id = 1;
		c->connection = 1;
		c->mm_width = 600;
		c->mm_height = 340;
		c->subpixel = 1;
		uint32_t encs[] = { 44 };
		FILL((void *)c->encoders_ptr, c->count_encoders, encs);
		//uint32_t props[] = { 1, 2, 18, 22, 23, 24, 26, 20, 25, 27 };
		uint32_t props[] = {};
		FILL((void *)c->props_ptr, c->count_props, props);
		//uint64_t prop_values[] = { 89, 0, 1, 0, 0, 0, 0, 0, 2, 0 };
		uint64_t prop_values[] = {};
		FILL((void *)c->prop_values_ptr, c->count_props, prop_values);
		struct drm_mode_modeinfo modes[] = {
			{
				.name = "800x600",
				.clock = 40000,
				.hdisplay = 800, .vdisplay = 600,
				.hsync_start = 840, .vsync_start = 601,
				.hsync_end = 968, .vsync_end = 605,
				.htotal = 1056, .vtotal = 628,
				.hskew = 0, .vscan = 0,
				.vrefresh = 60,
				.flags = 5,
				.type = 64,
			},
		};
		FILL((void *)c->modes_ptr, c->count_modes, modes);
	}
	else if(cmd == DRM_IOCTL_MODE_GETENCODER) {
		struct drm_mode_get_encoder *e = arg;
		e->encoder_type = 2;
		e->crtc_id = 29;
		e->possible_crtcs = 63;
		e->possible_clones = 0;
	}
	else if(cmd == DRM_IOCTL_GET_CAP) {
		struct drm_get_cap *cap = arg;
//		if(cap->capability == 5) cap->value = 3;
		if(cap->capability == DRM_CAP_PRIME) {
			cap->value = 0;
		}
		else if(cap->capability == DRM_CAP_DUMB_PREFERRED_DEPTH) {
			cap->value = 24;
		}
		else if(cap->capability == DRM_CAP_DUMB_BUFFER) {
			cap->value = 1;
		}
		else {
			printf("unknown DRM_IOCTL_GET_CAP\n");
			return -1;
		}
printf("DRM_IOCTL_MODE_CREATE_DUMB = %lu\n", DRM_IOCTL_MODE_CREATE_DUMB);
	}
	else if(cmd == DRM_IOCTL_MODE_GETCRTC) {
		struct drm_mode_crtc *crtc = arg;
		uint32_t connectors[] = { };
		FILL((void *)crtc->set_connectors_ptr, crtc->count_connectors, connectors);
		crtc->fb_id = 0; // TODO change?
		crtc->x = 0;
		crtc->y = 0;
		crtc->gamma_size = 256;
		crtc->mode_valid = 0;
//		crtc->
	}
	else if(cmd == DRM_IOCTL_SET_MASTER) {
		return 0;
	}
	else if(cmd == DRM_IOCTL_DROP_MASTER) {
		return 0;
	}
	else if(cmd == DRM_IOCTL_MODE_SETGAMMA) {
		return 0;
	}
	else if(cmd == DRM_IOCTL_MODE_SETCRTC) {
		return 0;
	}
	else {
		switch(cmd) {
#define CASE(name) case name: printf("XSUBSESSION UNHANDLED " #name "!\n"); break;
			CASE(DRM_IOCTL_SET_VERSION)
			CASE(DRM_IOCTL_GET_UNIQUE)
			CASE(DRM_IOCTL_VERSION)
			CASE(DRM_IOCTL_MODE_GETRESOURCES)
			CASE(DRM_IOCTL_GET_CAP)
			CASE(DRM_IOCTL_MODE_GETCRTC)
			CASE(DRM_IOCTL_MODE_GETCONNECTOR)
			CASE(DRM_IOCTL_SET_MASTER)
			CASE(DRM_IOCTL_DROP_MASTER)
			CASE(DRM_IOCTL_MODE_CREATE_DUMB)
			CASE(DRM_IOCTL_MODE_ADDFB)
			CASE(DRM_IOCTL_MODE_RMFB)
			CASE(DRM_IOCTL_MODE_DESTROY_DUMB)
			CASE(DRM_IOCTL_RADEON_INFO)
			CASE(DRM_IOCTL_MODE_GETENCODER)
			CASE(DRM_IOCTL_MODE_GETPROPERTY)
			CASE(DRM_IOCTL_MODE_GETPROPBLOB)
			CASE(DRM_IOCTL_RADEON_GEM_INFO)
			CASE(DRM_IOCTL_RADEON_GEM_CREATE)
			CASE(DRM_IOCTL_RADEON_GEM_MMAP)
			CASE(DRM_IOCTL_RADEON_GEM_WAIT_IDLE)
			CASE(DRM_IOCTL_RADEON_GEM_SET_TILING)
			CASE(DRM_IOCTL_MODE_SETPROPERTY)
			CASE(DRM_IOCTL_MODE_GETFB)
			CASE(DRM_IOCTL_GEM_FLINK)
			CASE(DRM_IOCTL_GEM_OPEN)
			CASE(DRM_IOCTL_GEM_CLOSE)
			CASE(DRM_IOCTL_RADEON_GEM_GET_TILING)
			CASE(DRM_IOCTL_RADEON_CS)
			CASE(DRM_IOCTL_MODE_SETGAMMA)
			CASE(DRM_IOCTL_MODE_SETCRTC)
			CASE(DRM_IOCTL_WAIT_VBLANK)
			CASE(DRM_IOCTL_RADEON_GEM_BUSY)
			CASE(DRM_IOCTL_RADEON_GEM_OP)
			CASE(DRM_IOCTL_MODE_PAGE_FLIP)
			CASE(0xc018646d)
			CASE(DRM_IOCTL_MODE_CURSOR)
			CASE(DRM_IOCTL_MODE_CURSOR2)
			CASE(DRM_IOCTL_MODE_MAP_DUMB)
			CASE(DRM_IOCTL_MODE_DIRTYFB)
#undef CASE
			default:
				printf("IOCTL unknown!!!!!\n");
		}
		return -1;
	}

	return 0;
}

static void fill_array(void *dst, size_t elem_sz, uint32_t *count, void *src,
		int src_count)
{
	size_t total_sz;
	if(*count < src_count) total_sz = elem_sz * *count;
	else total_sz = elem_sz * src_count;
	memcpy(dst, src, total_sz);
	*count = src_count;
}
