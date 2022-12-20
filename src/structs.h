#pragma once

// // Requested feature: pthread_rwlock_t
// #ifndef _GNU_SOURCE
// #define _GNU_SOURCE
// #endif
// #ifndef __USE_XOPEN2K
// #define __USE_XOPEN2K
// #endif

#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "macros.h"
#include "vector.h"

#define INIT_SUCCESS 0
#define INIT_FAIL 1

#define find_field(segment, data_ptr) (((data_ptr) - (segment)->data) / (segment)->align) 

#define FREE 0
#define LOCKED 1

// atomic_uint tx_count; // TODO remove
// atomic_uint tx_aborted;
// struct write_entry { // TODO remove
//     unsigned tx_id;
//     unsigned rv, wv;
//     long unsigned thread;
//     size_t num1, num2;
//     long long val1, val2;
// };
// typedef struct write_entry write_entry_t;
// write_entry_t history[(size_t)1e6]; // TODO remove


struct segment_descriptor {
    size_t size;                /* Size in bytes */
    size_t align;               /* Alginment in segment */
    size_t fields;              /* Number of fields in the segment (size/align) */
    void* data;                 /* Pointer to tm */
    atomic_bool* locks;         /* Locks for segment's fiels */
    uint32_t* w_counters;       /* Local counters of tm fields */
};
typedef struct segment_descriptor segment_descriptor_t;

struct segment_node {
    segment_descriptor_t* desc;
    struct segment_node* prev;
    struct segment_node* next;
};
typedef struct segment_node segment_node_t;

struct region {
    // atomic_uint txs, txs_ro, tx_s, tx_a_r, tx_a_ro, tx_a_rsv, tx_a_la;
    atomic_uint global_clock;
    segment_descriptor_t* desc;
    segment_node_t* allocs;
    size_t align;               
};
typedef struct region region_t;

/*
 * I decided to tread tx_t as a location in memory
 */
struct transaction {
    // unsigned id; //TODO remove
    region_t* region;
    bool is_ro;
    uint32_t rv;                /* Read version of global clock */
    cvector_t* read_set;          /* Set of locations read by tx in tm */
    vector_t* write_set_targets; /* Set of locations writen to by tx in tm */
    vector_t* write_set_values; /* Set of locations of values written */
};
typedef struct transaction transaction_t;

int region_init(region_t* region, size_t size, size_t align);
void region_destroy(region_t* region);

int segment_init(region_t* region, segment_descriptor_t* desc, size_t size);
void segment_destroy(segment_descriptor_t* desc);

int transaction_init(transaction_t* tx, region_t* region, bool is_ro);
void transaction_destroy(transaction_t* tx);

segment_descriptor_t* segment_find(const region_t* region, const void* address);
