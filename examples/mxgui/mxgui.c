#include "mxgui/mxgui.h"

#include "mgfx/defines.h"
#include "mgfx/mgfx.h"
#include "mx/mx_math_types.h"

#define CLAY_IMPLEMENTATION
#include <clay/clay.h>
// TODO: Remove
#include <stdlib.h>

#include <mx/mx_darray.h>
#include <mx/mx_math.h>
#include <mx/mx_math_mtx.h>
#include <mx/mx_memory.h>

static void clay_error_callback(Clay_ErrorData errorText) {}

const Clay_Color COLOR_LIGHT = (Clay_Color){224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color){168, 66, 28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138, 50, 255};

typedef struct mxgui_vertex {
  mx_vec3 position;
  mx_vec2 uv;
} mxgui_vertex;

mxgui_init_info s_settings;

mgfx_sh s_texured_quad_vsh, s_texured_quad_fsh;
mgfx_ph s_texured_quad_ph;
mgfx_uh s_texture_uh;

mx_darray_t(mxgui_vertex) s_textured_quad_vertices;
mx_darray_t(uint32_t) s_textured_quad_indices;

static void draw_rectangle(Clay_BoundingBox bb, Clay_Color color) {
  mx_darray_clear(&s_textured_quad_vertices);
  mx_darray_clear(&s_textured_quad_indices);

  // NDC ALLIGNED TOP LEFT 0: Bottom-left
  mx_darray_push(&s_textured_quad_vertices, mxgui_vertex,
                 {.position = {0.0f, -1.0f, 1.0f}, .uv = {0.0f, 1.0f}});

  // NDC ALLIGNED TOP LEFT 1: Bottom-right
  mx_darray_push(&s_textured_quad_vertices, mxgui_vertex,
                 {.position = {1.0f, -1.0f, 1.0f}, .uv = {1.0f, 1.0f}});

  // NDC ALLIGNED TOP LEFT 2: Top-right
  mx_darray_push(&s_textured_quad_vertices, mxgui_vertex,
                 {.position = {1.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}});

  // NDC ALLIGNED TOP LEFT 3: Top-left
  mx_darray_push(&s_textured_quad_vertices, mxgui_vertex,
                 {.position = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});

  // mx_vec3 offset = {bb.x, -bb.y, 1.0f};
  // mx_vec3 scale = {bb.width, bb.height, 1.0f};
  mx_vec3 offset = {0, 0, 0.0f};
  mx_vec3 scale = {bb.width, bb.height, 1.0f};
  for (size_t vertidx = 0; vertidx < MX_DARRAY_COUNT(&s_textured_quad_vertices);
       vertidx++) {
    s_textured_quad_vertices[vertidx].position =
        mx_vec3_mul(s_textured_quad_vertices[vertidx].position, scale);

    s_textured_quad_vertices[vertidx].position =
        mx_vec3_add(s_textured_quad_vertices[vertidx].position, offset);
  }

  mgfx_vertex_layout mxgui_vl = {0};
  mgfx_vertex_layout_begin(&mxgui_vl);
  mgfx_vertex_layout_add(&mxgui_vl, MGFX_VERTEX_ATTRIBUTE_POSITION,
                         sizeof(mx_vec3));
  mgfx_vertex_layout_add(&mxgui_vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD0,
                         sizeof(mx_vec2));
  mgfx_vertex_layout_end(&mxgui_vl);

  mgfx_transient_vb tvb = {.vl = &mxgui_vl};
  mgfx_transient_vertex_buffer_allocate(
      s_textured_quad_vertices,
      MX_DARRAY_COUNT(&s_textured_quad_vertices) * sizeof(mxgui_vertex), &tvb);

  mgfx_transient_buffer tib = {0};

  mx_darray_push(&s_textured_quad_indices, uint32_t, {0});
  mx_darray_push(&s_textured_quad_indices, uint32_t, {3});
  mx_darray_push(&s_textured_quad_indices, uint32_t, {2});
  mx_darray_push(&s_textured_quad_indices, uint32_t, {2});
  mx_darray_push(&s_textured_quad_indices, uint32_t, {1});
  mx_darray_push(&s_textured_quad_indices, uint32_t, {0});

  mgfx_transient_index_buffer_allocate(
      s_textured_quad_indices,
      MX_DARRAY_COUNT(&s_textured_quad_indices) * sizeof(uint32_t), &tib);

  mgfx_bind_transient_vertex_buffer(tvb);
  mgfx_bind_transient_index_buffer(tib);
  mgfx_bind_descriptor(s_texture_uh);
  mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, s_texured_quad_ph);
}

void mxgui_init(mxgui_init_info settings) {
  s_settings = settings;

  size_t clay_required_memory = Clay_MinMemorySize();
  Clay_Arena clayMemoryArena = (Clay_Arena){
      .memory = malloc(clay_required_memory),
      .capacity = clay_required_memory,
  };

  Clay_Initialize(
      clayMemoryArena,
      (Clay_Dimensions){.width = s_settings.width, .height = s_settings.height},
      (Clay_ErrorHandler){.errorHandlerFunction = clay_error_callback,
                          .userData = NULL});

  s_texured_quad_vsh = mgfx_shader_create("mxgui.vert.spv");
  s_texured_quad_fsh = mgfx_shader_create("mxgui.frag.spv");
  s_texured_quad_ph = mgfx_program_create_graphics(
      s_texured_quad_vsh, s_texured_quad_fsh,
      (mgfx_graphics_create_info){.name = "textured_quad_program"});

  s_texture_uh = mgfx_uniform_create("u_texture",
                                     MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER);
  mgfx_set_texture(s_texture_uh, MGFX_WHITE_TH);

  s_textured_quad_vertices =
      MX_DARRAY_CREATE(mxgui_vertex, MX_DEFAULT_ALLOCATOR);
  s_textured_quad_indices = MX_DARRAY_CREATE(uint32_t, MX_DEFAULT_ALLOCATOR);
}

// Example measure text function
static inline Clay_Dimensions MeasureText(Clay_StringSlice text,
                                          Clay_TextElementConfig* config,
                                          uintptr_t userData) {
  // Clay_TextElementConfig contains members such as fontId, fontSize,
  // letterSpacing etc Note: Clay_String->chars is not guaranteed to be null
  // terminated
  return (Clay_Dimensions){
      .width = text.length *
               config->fontSize, // <- this will only work for monospace fonts,
                                 // see the renderers/ directory for more
                                 // advanced text measurement
      .height = config->fontSize};
}

// Layout config is just a struct that can be declared statically, or inline
Clay_ElementDeclaration sidebarItemConfig = (Clay_ElementDeclaration){
    .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                          .height = CLAY_SIZING_FIXED(50)}},
    .backgroundColor = COLOR_ORANGE};

// Re-useable components are just normal functions
void side_bar_item_component() {
  CLAY(sidebarItemConfig) {
    // children go here...
  }
}

void mxgui_render() {
  Clay_SetLayoutDimensions(
      (Clay_Dimensions){s_settings.width, s_settings.height});

  //     // Optional: Update internal pointer position for handling mouseover /
  //     click / touch events - needed for scrolling & debug tools
  //     Clay_SetPointerState((Clay_Vector2) { mousePositionX, mousePositionY },
  //     isMouseDown);
  //     // Optional: Update internal pointer position for handling mouseover /
  //     click / touch events - needed for scrolling and debug tools
  //     Clay_UpdateScrollContainers(true, (Clay_Vector2) { mouseWheelX,
  //     mouseWheelY }, deltaTime);

  // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
  Clay_BeginLayout();

  // An example of laying out a UI with a fixed width sidebar and flexible width
  // main content
  CLAY({.id = CLAY_ID("OuterContainer"),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = CLAY_PADDING_ALL(16),
                   .childGap = 16},
        .backgroundColor = {250, 250, 255, 255}}) {

    CLAY({.id = CLAY_ID("SideBar"),
          .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {.width = CLAY_SIZING_FIXED(300),
                                .height = CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(16),
                     .childGap = 16},
          .backgroundColor = COLOR_LIGHT}) {

      CLAY({.id = CLAY_ID("ProfilePictureOuter"),
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0)},
                       .padding = CLAY_PADDING_ALL(16),
                       .childGap = 16,
                       .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
            .backgroundColor = COLOR_RED}) {
        // CLAY({.id = CLAY_ID("ProfilePicture"),
        //       .layout = {.sizing = {.width = CLAY_SIZING_FIXED(60),
        //                             .height = CLAY_SIZING_FIXED(60)}},
        //       .image = {.imageData = &profilePicture}}) {}
        CLAY_TEXT(CLAY_STRING("Clay - UI Library"),
                  CLAY_TEXT_CONFIG(
                      {.fontSize = 24, .textColor = {255, 255, 255, 255}}));
      }

      // Standard C code like loops etc work inside components
      for (int i = 0; i < 5; i++) {

        side_bar_item_component();
      }

      CLAY({.id = CLAY_ID("MainContent"),
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                  .height = CLAY_SIZING_GROW(0)}},
            .backgroundColor = COLOR_LIGHT}) {}
    }
  }

  // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
  Clay_RenderCommandArray render_commands = Clay_EndLayout();

  // More comprehensive rendering examples can be found in the renderers/
  // directory

  const real_t right = (real_t)s_settings.width;
  const real_t top = (real_t)s_settings.height;

  mx_mat4 mxgui_proj =
      mx_ortho(0.0f, s_settings.width, -s_settings.height, 0.0f, -1.0f, 1.0f);
  mgfx_set_proj(mxgui_proj.val);
  mgfx_set_view(MX_MAT4_IDENTITY.val);
  mgfx_set_transform(MX_MAT4_IDENTITY.val);

  for (int i = 0; i < render_commands.length; i++) {
    Clay_RenderCommand* render_command = &render_commands.internalArray[i];

    switch (render_command->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      draw_rectangle(render_command->boundingBox,
                     render_command->renderData.rectangle.backgroundColor);
      return;
    }
      // ... Implement handling of other command types
    case CLAY_RENDER_COMMAND_TYPE_NONE:
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
    case CLAY_RENDER_COMMAND_TYPE_TEXT:
    case CLAY_RENDER_COMMAND_TYPE_IMAGE:
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
      break;
    }
  }
}

void mxgui_shutdown() {
  mgfx_uniform_destroy(s_texture_uh);
  mgfx_program_destroy(s_texured_quad_ph);

  mgfx_shader_destroy(s_texured_quad_fsh);
  mgfx_shader_destroy(s_texured_quad_vsh);

  MX_DARRAY_DESTROY(&s_textured_quad_vertices, MX_DEFAULT_ALLOCATOR);
  MX_DARRAY_DESTROY(&s_textured_quad_indices, MX_DEFAULT_ALLOCATOR);

  // TODO: clean up clay
}
