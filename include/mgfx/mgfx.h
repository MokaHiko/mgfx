#ifndef MGFX_H_
#define MGFX_H_

#include "defines.h"
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
    uint64_t idx;				\
  } name;                                       \

MGFX_HANDLE(vertex_buffer_handle)
MGFX_HANDLE(mgfx_program_handle)
MGFX_HANDLE(mgfx_descriptor_handle)

void vertex_layout_begin(mgfx_vertex_layout* vl);

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size);

void vertex_layout_end(mgfx_vertex_layout* vl);

#endif
