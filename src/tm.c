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
    // region->txs = 0; region->txs_ro = 0; region->tx_a_r = 0; region->tx_a_rsv = 0; region->tx_a_la = 0; region->tx_a_ro = 0; region->tx_s = 0; // TODO remove
    return (shared_t)region;    
}

void tm_destroy(shared_t shared) {
    region_t* region = (region_t*) shared;
    // printf("[tm_destroy] txs: %u, txs_ro: %u, txs_s: %u, tx_a_r: %u, tx_a_ro: %u, tx_a_rsv: %u, tx_a_la: %u\n", 
    //     region->txs, region->txs_ro, region->tx_s, region->tx_a_r, region->tx_a_ro, region->tx_a_rsv, region->tx_a_la);
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

//TODO
alloc_t tm_alloc(shared_t unused(shared), tx_t unused(tx), size_t unused(size), void** unused(target)) {
    printf("ERROR - tm_alloc - NOT YES READY\n");
    return nomem_alloc;
}

//TODO
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(segment)) {
    printf("ERROR - tm_alloc - NOT YES READY\n");
    return false;
}





// alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
//     // We allocate the dynamic segment such that its words are correctly
//     // aligned. Moreover, the alignment of the 'next' and 'prev' pointers must
//     // be satisfied. Thus, we use align on max(align, struct segment_node*).
//     size_t align = ((struct region*) shared)->align;
//     align = align < sizeof(struct segment_node*) ? sizeof(void*) : align;

//     struct segment_node* sn;
//     if (unlikely(posix_memalign((void**)&sn, align, sizeof(struct segment_node) + size) != 0)) // Allocation failed
//         return nomem_alloc;

//     // Insert in the linked list
//     sn->prev = NULL;
//     sn->next = ((struct region*) shared)->allocs;
//     if (sn->next) sn->next->prev = sn;
//     ((struct region*) shared)->allocs = sn;

//     void* segment = (void*) ((uintptr_t) sn + sizeof(struct segment_node));
//     memset(segment, 0, size);
//     *target = segment;
//     return success_alloc;
// }

// bool tm_free(shared_t shared, tx_t unused(tx), void* segment) {
//     struct segment_node* sn = (struct segment_node*) ((uintptr_t) segment - sizeof(struct segment_node));

//     // Remove from the linked list
//     if (sn->prev) sn->prev->next = sn->next;
//     else ((struct region*) shared)->allocs = sn->next;
//     if (sn->next) sn->next->prev = sn->prev;

//     free(sn);
//     return true;
// }
