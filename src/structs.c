// Requested feature: posix_memalign
#define _POSIX_C_SOURCE   200809L

#include <string.h>
#include <stdio.h>

#include "structs.h"
#include "vector.h"

const uint32_t garbage_collector_lag = 20;

int region_init(region_t* region, size_t size, size_t align) {
    region->desc = (segment_descriptor_t*)malloc(sizeof(segment_descriptor_t));
    if (!region->desc) {
        return INIT_FAIL;
    }
    region->allocs = NULL;
    region->allocs_zombie = NULL;
    region->align = align;
    region->global_clock = 0;
    region->scheduled_to_delete = 0;
    pthread_mutex_init(&(region->allocs_lock), NULL);
    if (segment_init(region, region->desc, size) != INIT_SUCCESS) {
        free(region->desc);
        return INIT_FAIL;
    }
    return INIT_SUCCESS;
}

void region_destroy(region_t* region) {
    /* Specification guarantees that no transaction is running on this tm when
    tm_destroy is called, so we don't have to clean any transactions here. */

    while (region->allocs) {
        segment_node_t* next = region->allocs->next;
        segment_destroy(region->allocs->desc);
        free(region->allocs);
        region->allocs = next;
    }
    while (region->allocs_zombie) {
        segment_node_t* next = region->allocs_zombie->next;
        segment_destroy(region->allocs_zombie->desc);
        free(region->allocs_zombie);
        region->allocs_zombie = next;
    }
    pthread_mutex_destroy(&(region->allocs_lock));
    segment_destroy(region->desc);
    free(region);
}

int segment_init(region_t* region, segment_descriptor_t* desc, size_t size) {
    size_t align = region->align;
    if (posix_memalign(&(desc->data), align, size) != 0) {
        return INIT_FAIL; 
    }
    size_t fields = size / align;
    desc->w_counters = (uint32_t*)malloc(fields * sizeof(uint32_t));
    if (!desc->w_counters) {
        free(desc->data);
        return INIT_FAIL;
    }
    desc->locks = (atomic_bool*)malloc(fields * sizeof(atomic_bool));
    if (!desc->locks) {
        free(desc->data);
        free(desc->w_counters);
        return INIT_FAIL;
    }
    memset(desc->data, 0, size);
    memset(desc->locks, 0, fields * sizeof(atomic_bool));
    memset(desc->w_counters, 0, fields * sizeof(uint32_t));
    desc->align = align;
    desc->size = size;
    desc->fields = fields;
    return INIT_SUCCESS;
}

void segment_destroy(segment_descriptor_t* desc) {
    free(desc->data);
    free(desc->w_counters);
    free(desc->locks);
    free(desc);
}

int transaction_init(transaction_t* tx, region_t* region, bool is_ro) {
    tx->region = region;
    tx->is_ro = is_ro;
    tx->rv = region->global_clock; /* Sampling global version clock */

    if (!is_ro) {
        tx->read_set = cvector_init(VECTOR_DEFAULT_SIZE);
        if (!tx->read_set) {
            return INIT_FAIL;
        }
        tx->write_set_values = vector_init(VECTOR_DEFAULT_SIZE);
        if (!tx->write_set_values) {
            cvector_destroy(tx->read_set);
            return INIT_FAIL;
        }
        tx->write_set_targets = vector_init(VECTOR_DEFAULT_SIZE);
        if (!tx->write_set_targets) {
            cvector_destroy(tx->read_set);
            vector_destroy(tx->write_set_values);
            return INIT_FAIL;
        }
    }
    return INIT_SUCCESS;
}

void transaction_destroy(transaction_t* tx) {
    if (!(tx->is_ro)) {
        cvector_destroy(tx->read_set);
        vector_destroy(tx->write_set_targets);
        /* Buffers in tx->write_set_values were allocated especially for this transaction */
        vector_deep_destroy(tx->write_set_values);
    }
    free(tx);
}

bool address_in_segment(const segment_descriptor_t* segment, const void* address) {
    return segment->data <= address && address < segment->data + segment->size;
}

segment_node_t* node_find(const region_t* region, const void* address) {
    segment_node_t* allocs = region->allocs;
    while (allocs) {
        if (address_in_segment(allocs->desc, address)) {
            return allocs;
        }
        allocs = allocs->next;
    }
    printf("NODE_FIND WRONG ADDRESS\n"); exit(1); // TODO remove
    /* This address is not in tm region */
    return NULL;
}

segment_descriptor_t* segment_find(const region_t* region, const void* address) {
    if (address_in_segment(region->desc, address)) {
        return region->desc;
    }
    segment_node_t* allocs = region->allocs;
    while (allocs) {
        if (address_in_segment(allocs->desc, address)) {
            return allocs->desc;
        }
        allocs = allocs->next;
    }
    /* This address is not in tm region */
    printf("SEGMENT_FIND WRONG ADDRESS: %p\n", address); exit(1); // TODO remove
    return NULL;
}

/*
 * Removes segments scheduled to remove some time ago.
 *
 * Assumes that caller locked the allocs_lock
 */
void clean_old_segments(region_t* region) {
    segment_node_t* prev = NULL;
    segment_node_t* curr = region->allocs;
    while(curr) {
        if (curr->to_delete && curr->to_delete_from + garbage_collector_lag < region->scheduled_to_delete) {
            /* Node can be removed from allocs */
            if (!prev)
                region->allocs = curr->next;
            else
                prev->next = curr->next;

            /* Add node to allocs zombie */
            // curr->next = region->allocs_zombie;
            // region->allocs_zombie = curr;

            /* Move to next curr */
            curr = prev->next;
        }
        else {
            prev = curr;
            curr = curr->next;
        }
    }
}

/*
 * Schedules segment (node) to be deleted.
 *
 * Assumes that caller locked the alloc_lock
 */
void schedule_to_delete(region_t* region, segment_node_t* node) {
    node->to_delete = true;
    node->to_delete_from = region->scheduled_to_delete;
    region->scheduled_to_delete++;
}

segment_descriptor_t* add_segment(region_t* region, size_t size) {
    segment_descriptor_t* segment_ptr = (segment_descriptor_t*)malloc(sizeof(segment_descriptor_t));
    segment_node_t* node_ptr = (segment_node_t*)malloc(sizeof(segment_node_t));
    if (!segment_ptr || !node_ptr || segment_init(region, segment_ptr, size) != INIT_SUCCESS) {
        free(segment_ptr);
        free(node_ptr);
        return NULL;
    }
    node_ptr->to_delete = false;
    node_ptr->desc = segment_ptr;

    pthread_mutex_lock(&(region->allocs_lock));
    if (!region->allocs) {
        /* First allocated segment */
        node_ptr->next = NULL;
        region->allocs = node_ptr;
    }
    else {
        /* Put at the beginning of allocated linked list */
        node_ptr->next = region->allocs;
        region->allocs = node_ptr;
    }
    pthread_mutex_unlock(&(region->allocs_lock));

    return segment_ptr;
}
