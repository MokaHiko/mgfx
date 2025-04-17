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

    mgfx_debug_draw_text(0, 0, "delta time: %.2f s", last_value * 1000.0f);
    mgfx_debug_draw_text(0, 100, "fps : %u", (uint32_t)(1.0f / last_value));

    mgfx_debug_draw_text(0, 150, "MY BALLS!!");
    mgfx_debug_draw_text(0, 250, "YOOO THAT'S CRAZY!");
}

void mgfx_example_shutdown() {}

int main() { mgfx_example_app(); }
