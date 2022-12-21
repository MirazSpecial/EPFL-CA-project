// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Internal headers
#include <tm.h>

#include "structs.h"
#include "macros.h"
#include "tl2.h"

shared_t tm_create(size_t size, size_t align) {
    region_t* region = (region_t*) malloc(sizeof(region_t));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    if (region_init(region, size, align) != INIT_SUCCESS) {
        free(region);
        return invalid_shared;
    }
    return (shared_t)region;    
}

void tm_destroy(shared_t shared) {
    region_t* region = (region_t*) shared;
    region_destroy(region);
}

void* tm_start(shared_t shared) {
    region_t* region = (region_t*) shared;
    return region->desc->data;
}

size_t tm_size(shared_t shared) {
    region_t* region = (region_t*) shared;
    return region->desc->size;
}

size_t tm_align(shared_t shared) {
    region_t* region = (region_t*) shared;
    return region->align;
}

tx_t tm_begin(shared_t shared, bool is_ro) {
    transaction_t* tx = malloc(sizeof(transaction_t));
    region_t* region = (region_t*) shared;
    if (unlikely(transaction_init(tx, region, is_ro)) != INIT_SUCCESS) {
        free(tx);
        return invalid_tx;
    }
    
    return (tx_t)tx;
}

bool tm_end(shared_t unused(shared), tx_t tx) {
    if (((transaction_t*)tx)->is_ro) { 
        /* No read_set validation is needed, commit */
        transaction_destroy((transaction_t*)tx);
        return true;
    }
    if (!tl2_end((transaction_t*)tx)) {
        /* Transaction should be aborted */
        transaction_destroy((transaction_t*)tx);
        return false;
    }
    transaction_destroy((transaction_t*)tx);
    return true;
}

bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    region_t* region = (region_t*) shared;
    segment_descriptor_t* segment = segment_find(region, source);

    void* buffer = malloc(size * sizeof(void));
    for (size_t field = 0; field < size / region->align; field++) {
        if (((transaction_t*)tx)->is_ro) {
            if (!tl2_load_ro((transaction_t*)tx, segment, 
                             source + field * region->align, 
                             buffer + field * region->align)) {
                /* Transaction should be aborted */
                transaction_destroy((transaction_t*)tx);
                free(buffer);
                return false;
            }
        }
        else {
            if (!tl2_load((transaction_t*)tx, segment, 
                          source + field * region->align, 
                          buffer + field * region->align)) {
                /* Transaction should be aborted */
                transaction_destroy((transaction_t*)tx);
                free(buffer);
                return false;
            }
        }
    }
    memcpy(target, buffer, size);
    free(buffer);
    return true;
}

bool tm_write(shared_t unused(shared), tx_t tx, void const* source, size_t size, void* target) {
    region_t* region = (region_t*) shared;
    segment_descriptor_t* segment = segment_find(region, target);
    
    for (size_t field = 0; field < size / region->align; field++) {
        if (!tl2_put((transaction_t*)tx, segment,
                     source + field * region->align,
                     target + field * region->align)) {
            /* Transaction should be aborted */
            transaction_destroy((transaction_t*)tx);
            return false;
        }
    }
    return true;
}

alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** unused(target)) {
    region_t* region = (region_t*) shared;

    segment_descriptor_t* segment = add_segment(region, size);

    if (!segment) {
        return nomem_alloc;
    }
    *target = segment->data;
    return success_alloc;
}

bool tm_free(shared_t shared, tx_t unused(tx), void* segment) {
    region_t* region = (region_t*) shared;

    pthread_mutex_lock(&(region->allocs_lock));

    schedule_to_delete(region, node_find(region, segment)); /* Schedules segment to be deleted */
    clean_old_segments(region); /* Deletes segments scheduled in the past */

    pthread_mutex_unlock(&(region->allocs_lock));

    return true;
}
