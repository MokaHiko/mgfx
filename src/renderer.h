#ifndef MGFX_RENDERER_H_
#define MGFX_RENDERER_H_

#include <mx/mx.h>
#include "mgfx/defines.h"

typedef struct buffer {
    uint64_t allocation; // VmaAllocation
    uint64_t handle;     // VkBuffer
    uint32_t usage;      // VkBufferUsageFlags
} buffer;

extern void buffer_create(size_t size, uint32_t usage, uint32_t flags, buffer* buffer);

#endif
