#pragma once

#include "structs.h"

/*
 * load exactly 'segment->align' bytes from source (tm) (or write set) to buffer (lm)
 * performing all the other tl2 loading operations
 * 
 * if true then not abort,
 * false to abort  
 */
bool tl2_load(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* buffer);

/*
 * load exactly 'segment->align' bytes from source (tm) to buffer (lm)
 * dont put address to read_set as this is part of read only transaction!
 *
 * true for success, false to abort
 */
bool tl2_load_ro(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* buffer);

/* 
 * We were supposed to put exactly 'segment->align' bytes from source (lm) 
 * to target, we don't do that, we put it to created buffer (added to tx->write_set_values)
 *
 * true for success, falst to aborts
 */
bool tl2_put(transaction_t* tx, segment_descriptor_t* segment, const void* source, void* const target);

/*
 * Try to end given transaction
 *
 * true for success, false if aborted
 */
bool tl2_end(transaction_t* tx);
