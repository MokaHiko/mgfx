#include "ex_common.h"

#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_memory.h>

void mgfx_example_init() {
}

void mgfx_example_update() {
    mgfx_debug_draw_text(0, 0, "FPS: %.3f", 1.0 / MGFX_TIME_DELTA_TIME);
}

void mgfx_example_shutdown() {
}

int main() { mgfx_example_app(); }
