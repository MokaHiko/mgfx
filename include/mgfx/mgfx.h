#ifndef MGFX_H_
#define MGFX_H_

#include <mx/mx.h>

#define MGFX_SUCCESS ((uint16_t)0)

typedef MX_API struct {
	char name[256];
	void* nwh;
} mgfx_init_info;

MX_API int mgfx_init(const mgfx_init_info* info);

MX_API void mgfx_shutdown();

static const uint16_t mgfx_invalid_handle = UINT16_MAX;
#define MGFX_INVALID_HANDLE ((uint16_t)mgfx_invalid_handle)

#define MGFX_HANDLE(name)                       \
  typedef MX_API struct {                       \
    uint16_t idx;				\
  } name;                                       \

MGFX_HANDLE(mgfx_vb)

#endif
