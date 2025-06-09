#ifndef MX_ECS_H_
#define MX_ECS_H_

#include <mx/mx.h>
#include <mx/mx_darray.h>

typedef uint32_t mx_component_id;

#define MX_INVALID_ACTOR_ID 0xFFFFFFFF
typedef struct MX_API mx_actor {
    uint32_t id;        // Unique id
    uint32_t archetype; // Hash of components
} mx_actor;

typedef struct MX_API mx_actor_info {
    const char* name;
} mx_actor_info;

MX_API MX_NO_DISCARD int mx_ecs_init();

typedef struct mx_component_container {
    const char* name;

    uint32_t elem_size;
    uint32_t elem_count;

    mx_darray_t(uint32_t) actor_to_index;
    mx_darray_t(mx_actor) index_to_actor;

    mx_darray_t(uint8_t) darray;
} mx_component_container;

typedef struct MX_API mx_actor_spawn_opts {
    const char* name;
    mx_actor parent;
} mx_actor_spawn_opts;

MX_API MX_NO_DISCARD mx_actor mx_actor_spawn_impl(const mx_actor_spawn_opts* opts);
#define mx_actor_spawn(...) mx_actor_spawn_impl(&(mx_actor_spawn_opts)__VA_ARGS__)

MX_API void mx_actor_destroy(mx_actor actor);

MX_API MX_NO_DISCARD mx_component_id component_id_from_type_impl(const char* name);
#define component_id_from_type(type) component_id_from_type_impl(#type)

MX_API void component_declare_impl(mx_component_id id, mx_component_container* container);
#define mx_component_declare(type)                                                       \
    component_declare_impl(                                                              \
        component_id_from_type_impl(#type),                                              \
        &(mx_component_container){                                                       \
            .name = #type,                                                               \
            .elem_size = sizeof(type),                                                   \
            .actor_to_index = MX_DARRAY_CREATE(uint32_t, MX_DEFAULT_ALLOCATOR),          \
            .index_to_actor = MX_DARRAY_CREATE(mx_actor, MX_DEFAULT_ALLOCATOR),          \
            .darray = (uint8_t*)MX_DARRAY_CREATE(type, MX_DEFAULT_ALLOCATOR)});

MX_API void*
mx_actor_set_impl(mx_actor actor, mx_component_id id, void* data);
#define mx_actor_set(actor_id, type, ...)                                                \
    (type*)mx_actor_set_impl(actor_id, component_id_from_type(type), &(type)__VA_ARGS__)

MX_API void mx_actor_remove_impl(mx_actor actor, mx_component_id id);
#define mx_actor_remove(actor_id, type)                                                  \
    mx_actor_remove_impl(actor_id, component_id_from_type(type))

// @ returns a pointer to the component of the matching id and null otherwise.
MX_API MX_NO_DISCARD void* mx_actor_find_impl(mx_actor actor, mx_component_id id);
#define mx_actor_find(actor_id, type)                                                    \
    (type*)mx_actor_find_impl(actor_id, component_id_from_type(type))

#define mx_query_view(type)                                                              \
    (mx_darray_t(type)) mx_query_view_impl(component_id_from_type(type))
mx_darray_t(uint8_t) mx_query_view_impl(mx_component_id id);

typedef struct MX_API mx_ecs_iter {
    mx_actor actor;
    void* component;

    mx_component_container* container;
    uint32_t idx;
} mx_ecs_iter;

MX_API MX_NO_DISCARD mx_ecs_iter mx_ecs_query_iter_impl(mx_component_id id);
#define mx_ecs_query_iter(type) mx_ecs_query_iter_impl(component_id_from_type(type))

MX_API MX_NO_DISCARD mx_bool mx_iter_next(mx_ecs_iter* it);

MX_API void mx_ecs_shutdown();

#endif
