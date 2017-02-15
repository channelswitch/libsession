#include <stdint.h>
#include <session/dri.h>
struct card;
//struct session_dri_resources;
/*
 * All callbacks reentrant.
 */
int card_new(
		struct card **c_out,
		void *(*register_fd)(
			void *user,
			int fd,
			unsigned flags,
			void (*f)(void *user1, int event),
			void *user1),
		void (*unregister_fd)(void *fd_ptr),
		void *user,
		int fd,
//		void (*request_resources)(void *user, uint64_t unique),
//		void (*framebuffer)(void *user, uint32_t name, int w, int h),
		char *device_name);
void card_free(struct card *c);

//void card_vsync(struct card *c);
//void card_send_resources(struct card *c, uint64_t unique,
//		struct session_dri_resources *resources);

/*
 * To be implemented by the user. Called whenever modeset ioctl happens.
 */
int card_send_modeset_ioctl(void *user, uint64_t unique, uint32_t cmd,
		uint64_t inarg, char *buf, int len);

/*
 * Call this function to respond to the above
 */
void card_modeset_ioctl_response(struct card *c, uint64_t unique,
		int return_value, char *buf, int len);
