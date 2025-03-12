#include "ex_common.h"
#include <math.h>

int width  = 720;
int height = 480;

void mgfx_example_init() {}

static float time = 0.0f;
void mgfx_example_updates(const DrawCtx *ctx) {
    time += 0.05f;
    VkClearColorValue clear = { .float32 = {sin(0.6 * time), sin(0.5f * time), sin(time * 0.2f), 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_clear_image(ctx->cmd, ctx->frame_target->color_attachments[0], &range, &clear);
}

void mgfx_example_shutdown() {}

int main() { mgfx_example_app(); }
