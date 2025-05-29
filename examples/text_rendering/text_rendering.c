#include "ex_common.h"

#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_memory.h>

void mgfx_example_init() {}

void mgfx_example_update() {
    static float last_value = 0.0f;
    float cur_value = MGFX_TIME_DELTA_TIME;
    float epsilon = 0.01f;

    if (fabsf(cur_value - last_value) > epsilon) {
        last_value = cur_value;
    }

    mx_ivec2 position = {APP_WIDTH / 2, APP_HEIGHT / 2};

    mgfx_debug_draw_text(position[0], position[1], "apple");
    mgfx_debug_draw_text(position[0], position[1] + 32, "orange");
    mgfx_debug_draw_text(position[0], position[1] + 64, "banana");
    mgfx_debug_draw_text(position[0], position[1] + 96, "mango");
    mgfx_debug_draw_text(position[0], position[1] + 128, "guava");
}

void mgfx_example_shutdown() {}

int main() { mgfx_example_app(); }
