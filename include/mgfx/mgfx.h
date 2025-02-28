#ifndef MGFX_H_
#define MGFX_H_

#include <mx/mx.h>

typedef MX_API struct {
	char name[256];
	void* nwh;
} mgfx_init_info;

/*@brief Initializes the MGFX graphics system.*/
/**/
/**/
/*@param info Pointer to an `mgfx_init_info` struct containing initialization parameters.*/
/*@returns MGFX_SUCCESS (0) if successful, or an error code otherwise.*/
MX_API int mgfx_init(const mgfx_init_info* info);

MX_API void mgfx_frame();

MX_API void mgfx_shutdown();

MX_API void mgfx_reset(uint32_t width, uint32_t height);

static const uint16_t mgfx_invalid_handle = UINT16_MAX;
#define MGFX_INVALID_HANDLE ((uint16_t)mgfx_invalid_handle)

#define MGFX_HANDLE(name)                       \
  typedef MX_API struct name {                  \
    uint16_t idx;				\
  } name;                                       \

MGFX_HANDLE(mgfx_vbh)

#endif
