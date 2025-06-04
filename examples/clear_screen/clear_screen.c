#include "ex_common.h"

#include <mx/mx_math.h>

void mgfx_example_init() {}

void mgfx_example_update() {
    mgfx_set_view_clear(
        MGFX_DEFAULT_VIEW_TARGET,
        (float[4]){mx_cos(MGFX_TIME), 1.0f, 0.5f * mx_sin(MGFX_TIME), 1.0f});

    mgfx_debug_draw_text(APP_WIDTH / 1.75, APP_HEIGHT / 2, "Hello MGFX");
}

void mgfx_example_shutdown() {}

int main() { mgfx_example_app(); }
