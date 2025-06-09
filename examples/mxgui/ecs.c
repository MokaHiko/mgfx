#include "ecs.h"
#include "mx/mx.h"

#include <mx/mx_darray.h>
#include <mx/mx_hash.h>
#include <mx/mx_log.h>

#include <mx/mx_string.h>

const static uint32_t INVALID_COMPONENT_IDX = -1;
static mx_darray_t(mx_actor) s_actors = NULL;

mx_map_t(mx_component_id, mx_component_container, 32) s_components_info;
mx_map_find_fn(component_info_find, mx_component_id, mx_component_container);
mx_map_insert_fn(component_info_insert, mx_component_id, mx_component_container);

MX_NO_DISCARD static inline mx_component_container* container_find(mx_component_id id) {
    mx_component_container* ccontainer = component_info_find(&s_components_info, id);

    if (!ccontainer) {
        MX_LOG_ERROR("[MX] Unregistered type %d", id);
        return NULL;
    }

    return ccontainer;
};

MX_NO_DISCARD static inline uint32_t
actor_find_index(const mx_component_container* cc, mx_actor actor, mx_component_id id) {
    if (!cc) {
        return -1;
    };

    uint32_t count = MX_DARRAY_COUNT(&cc->actor_to_index);

    if (count < actor.id) {
        return MX_INVALID_ACTOR_ID;
    }

    uint32_t idx = cc->actor_to_index[actor.id];

    if (idx >= MX_DARRAY_COUNT(&cc->darray)) {
        return -1;
    }

    return idx;
};

int mx_ecs_init() {
    s_actors = MX_DARRAY_CREATE(mx_actor, MX_DEFAULT_ALLOCATOR);
    return MX_SUCCESS;
};

mx_component_id component_id_from_type_impl(const char* name) {
    mx_str name_string = {name};
    return mx_murmur_hash(name, mx_strlen(name_string), 0);
};

void component_declare_impl(mx_component_id id, mx_component_container* container) {
    if (component_info_find(&s_components_info, id)) {
        MX_LOG_WARN("[MX] Component(%u) already registered!", id);
        return;
    }

    component_info_insert(&s_components_info, id, container);
    MX_LOG_TRACE("%s component (%u) registered", container->name, id);
};

mx_actor mx_actor_spawn_impl(const mx_actor_spawn_opts* opts) {
    mx_darray_push(&s_actors,
                   mx_actor,
                   {
                       .id = 0,
                       .archetype = 0,
                   });

    return (mx_actor){.id = MX_DARRAY_COUNT(&s_actors) - 1};
};

mx_darray_t(uint8_t) mx_query_view_impl(mx_component_id id) {
    mx_component_container* container = component_info_find(&s_components_info, id);

    if (!container) {
        return NULL;
    }

    return container->darray;
}

mx_ecs_iter mx_ecs_query_iter_impl(mx_component_id id) {
    mx_component_container* container = component_info_find(&s_components_info, id);

    if (!container) {
        return (mx_ecs_iter){0};
    }

    return (mx_ecs_iter){
        .container = container,
    };
}

mx_bool mx_iter_next(mx_ecs_iter* it) {
    if (!it->container) {
        return MX_FALSE;
    }

    if (it->idx >= it->container->elem_count) {
        return MX_FALSE;
    }

    it->component = &it->container->darray[it->idx * it->container->elem_size];
    it->actor = it->container->index_to_actor[it->idx];
    it->idx++;

    return MX_TRUE;
}

void* mx_actor_set_impl(mx_actor actor, mx_component_id id, void* data) {
    mx_component_container* cc = container_find(id);

    if (!cc) {
        return NULL;
    };

    if (actor.id >= cc->elem_count) {
        mx_darray_resize(&cc->actor_to_index, actor.id + 1);
        mx_darray_resize(&cc->index_to_actor, actor.id + 1);
    }

    uint32_t component_idx = actor_find_index(cc, actor, id);

    if (component_idx != INVALID_COMPONENT_IDX) {
        memcpy(cc->darray + component_idx * cc->elem_size, data,cc->elem_size);
    }
    else {
		component_idx = MX_DARRAY_COUNT(&cc->darray);
		cc->actor_to_index[actor.id] = component_idx;
		cc->index_to_actor[component_idx] = actor;

		MX_DARRAY_ADD(&cc->darray, data);
		++cc->elem_count;
    }

    return cc->darray + component_idx * cc->elem_size;
};

void* mx_actor_find_impl(mx_actor actor, mx_component_id id) {
    mx_component_container* cc = container_find(id);

    if (!cc) {
        return NULL;
    }

    uint32_t idx = actor_find_index(cc, actor, id);

    if (idx == MX_INVALID_ACTOR_ID) {
        return NULL;
    }

    return &cc->darray[idx * cc->elem_size];
};

void mx_actor_remove_impl(mx_actor actor, mx_component_id id) {
    mx_component_container* cc = container_find(id);

    if (!cc) {
        return;
    }

    uint32_t idx_to_remove = actor_find_index(cc, actor, id);

    if (idx_to_remove == MX_INVALID_ACTOR_ID) {
        return;
    }

    // Copy end element to remove idx
    memcpy(cc->darray + idx_to_remove * cc->elem_size,
           cc->darray + (cc->elem_count - 1) * cc->elem_size,
           cc->elem_size);

    // Swap moved element with removed idx
    mx_actor end = cc->index_to_actor[cc->elem_count - 1];
    cc->actor_to_index[end.id] = idx_to_remove;
    cc->index_to_actor[idx_to_remove] = end;

    cc->actor_to_index[actor.id] = MX_INVALID_ACTOR_ID;

    // Resize container
    --cc->elem_count;
    mx_darray_resize(&cc->darray, cc->elem_count);
}

void mx_ecs_shutdown() {
    // delete all actors
    // delete all components
};
