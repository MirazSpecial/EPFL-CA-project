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
    region->allocs = vector_init(1024);
    if (!region->allocs) {
        free(region->desc);
        return INIT_FAIL;
    }
    region->allocs_frees = 0;
    region->align = align;
    region->global_clock = 0;
    pthread_mutex_init(&(region->allocs_lock), NULL);
    if (segment_init(region, region->desc, size) != INIT_SUCCESS) {
        free(region->desc);
        vector_destroy(region->allocs);
        return INIT_FAIL;
    }
    return INIT_SUCCESS;
}

void region_destroy(region_t* region) {
    /* Specification guarantees that no transaction is running on this tm when
    tm_destroy is called, so we don't have to clean any transactions here. */

    for (size_t i = 0; i < region->allocs->size; ++i) {
        segment_destroy(region->allocs->data[i]);
    }
    vector_destroy(region->allocs);
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
    desc->to_delete = false;
    return INIT_SUCCESS;
}

void segment_destroy(segment_descriptor_t* desc) {
    if (desc) {
        free(desc->data);
        free(desc->w_counters);
        free(desc->locks);
        free(desc);
    }
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

uint32_t add_segment(region_t* region, size_t size) {
    segment_descriptor_t* segment_ptr = (segment_descriptor_t*)malloc(sizeof(segment_descriptor_t));
    if (!segment_ptr || segment_init(region, segment_ptr, size) != INIT_SUCCESS) {
        free(segment_ptr);
        return -1;
    }

    pthread_mutex_lock(&(region->allocs_lock));
    uint32_t segment_num = region->allocs->size;
    vector_push_back(region->allocs, segment_ptr);
    pthread_mutex_unlock(&(region->allocs_lock));

    return segment_num;
}

/*
 * Schedules a segment to be deleted. Assumes that allocs_lock is locked by caller
 */
void schedule_to_delete(region_t* region, segment_descriptor_t* desc) {
    region->allocs_frees++;
    desc->to_delete = true;
}

/*
 * Destroys all segments with flag 'to_delete' set on. Assumes
 * that allocs_lock is locked by caller
 */
void delete_old_segments(region_t* region) {
    for (size_t i = 0; i < region->allocs->size; ++i) {
        segment_descriptor_t* desc = region->allocs->data[i]; 
        if (desc->to_delete)
            segment_destroy(desc);
    }
}