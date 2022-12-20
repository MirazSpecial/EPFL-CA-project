#include <string.h>
#include <stdio.h>

#include "tl2.h"


bool tl2_load(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* buffer) {
    size_t align = segment->align;
    size_t field = find_field(segment, source);
    size_t set_index = vector_find_last(tx->write_set_targets, source);
    
    if (set_index != (size_t)-1) {
        /* This transaction already written in this field */
        memcpy(buffer, tx->write_set_values->data[set_index], align);
    }
    else {
        /* This transaction has not written in this field */
        uint32_t w_count = segment->w_counters[field];
        memcpy(buffer, source, align);
        if (segment->locks[field] == LOCKED || 
            segment->w_counters[field] > w_count || 
            segment->w_counters[field] > tx->rv) {
            atomic_fetch_add(&(tx->region->tx_a_r), 1); // TODO remove
            // printf("tx_a_r\n");
            return false; /* Read value from older snapshot, abort */
        }
    }

    if (!cvector_push_back(tx->read_set, source))
        return false; /* Could not add to read_set, abort */
    return true;
}

bool tl2_load_ro(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* buffer) {
    size_t field = find_field(segment, source);
    memcpy(buffer, source, segment->align);

    if (segment->locks[field] == LOCKED || 
        segment->w_counters[field] > tx->rv) {
        atomic_fetch_add(&(tx->region->tx_a_ro), 1); // TODO remove
        // printf("tx_a_ro\n");
        return false; /* Read value from older snapshot, abort */
    }
    return true;
}


bool tl2_put(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* const target) {
    size_t align = segment->align;
    void* buffer = malloc(align);
    if (!buffer)
        return false; /* Could not allocate buffer, abort */
    memcpy(buffer, source, align);

    if (!vector_push_back(tx->write_set_targets, target) ||
        !vector_push_back(tx->write_set_values, buffer)) {
        free(buffer);
        return false;
    }
    return true;
}

void free_locks(vector_t* locations, region_t* region, size_t n) {
    bool desired_lock_state;
    for (size_t i = 0; i < n; ++i) {
        segment_descriptor_t* segment = segment_find(region, locations->data[i]); 
        size_t field = find_field(segment, locations->data[i]);
        desired_lock_state = LOCKED;
        if (!atomic_compare_exchange_strong(&(segment->locks[field]), &desired_lock_state, FREE)) {
            /* This lock was supposed to be locked, but its free, which is weird
               but doesnt break anything. */
            continue;
        }
    }
}

bool tl2_end(transaction_t* tx) {
    region_t* region = tx->region;

    /* We don't want to lock a field two times, so we need to remove duplicates */
    vector_t* write_set_targets_copy = vector_no_duplicates(tx->write_set_targets);
    if (!write_set_targets_copy)
        return NULL; /* Could not allocate, abort */

    /* Acquire locks in any order */        // TODO maybe more then one loop?
    bool desired_lock_state;
    for (size_t i = 0; i < write_set_targets_copy->size; ++i) {
        segment_descriptor_t* segment = segment_find(region, write_set_targets_copy->data[i]); 
        size_t field = find_field(segment, write_set_targets_copy->data[i]);
        desired_lock_state = FREE;
        if (!atomic_compare_exchange_strong(&(segment->locks[field]), &desired_lock_state, LOCKED)) {
            /* Lock is locked, abort */
            free_locks(write_set_targets_copy, region, i);
            atomic_fetch_add(&(tx->region->tx_a_la), 1); // TODO remove
            // printf("tx_a_la\n")
            return false;
        }
    }
    vector_destroy(write_set_targets_copy);

    /* Increment global version clock */
    uint32_t wv = atomic_fetch_add(&(region->global_clock), 1) + 1;
    
    /* Validate the read set */
    for (size_t i = 0; i < tx->read_set->size; ++i) {
        segment_descriptor_t* segment = segment_find(tx->region, tx->read_set->data[i]); 
        size_t field = find_field(segment, tx->read_set->data[i]);
        bool field_written = vector_find_last(
            tx->write_set_targets, tx->read_set->data[i]) != (size_t)-1;

        if ((!field_written && segment->locks[field] == LOCKED) || 
            segment->w_counters[field] > tx->rv) {
            /* Read value no longer valid, abort */
            free_locks(tx->write_set_targets, region, tx->write_set_targets->size);
            atomic_fetch_add(&(tx->region->tx_a_rsv), 1); // TODO remove
            // printf("tx_a_rsv\n")
            return false;
        }
    }

    // add to history TODO remove
    // write_entry_t we;
    // we.tx_id = tx->id;
    // we.thread = pthread_self();
    // we.rv = tx->rv;
    // we.wv = wv;
    // if (tx->write_set_targets->size == 2) {
    //     we.num1 = find_field(region->desc, tx->write_set_targets->data[0]);
    //     we.num2 = find_field(region->desc, tx->write_set_targets->data[1]);
    //     we.val1 = *((long long*)(tx->write_set_values->data[0]));
    //     we.val2 = *((long long*)(tx->write_set_values->data[1]));
    // }
    // else {
    //     we.num1 = -1111;
    //     we.num2 = -1111;
    //     we.val1 = -1111;
    //     we.val2 = -1111;
    // }
    // history[wv] = we; // TODO remove

    /* Write new values and increase w_count */
    for (size_t i = 0; i < tx->write_set_targets->size; ++i) {
        segment_descriptor_t* segment = segment_find(tx->region, tx->write_set_targets->data[i]); 
        size_t field = find_field(segment, tx->write_set_targets->data[i]);

        memcpy(tx->write_set_targets->data[i], tx->write_set_values->data[i], region->align);
        segment->w_counters[field] = wv; /* Increasing w_count */
    }

    /* Free the locks */
    free_locks(tx->write_set_targets, region, tx->write_set_targets->size);
    
    /* Commit */
    return true;
}
