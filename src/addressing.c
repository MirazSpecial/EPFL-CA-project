#include <stdio.h>

#include "addressing.h"


/*
 * Recover segment number from address in memory,
 * segment number is encoded in the first 16 bits of the address,
 * Next 48 bits of the address is the offset in the segment.
 */
uint32_t get_segment_num(const void* address) {
	return ((uint64_t)address) >> 48;
}

/*
 * Recover offset in segment from given address,
 * segment offset is coded by the last 48 bits of the addresss.
 */
uint64_t get_segment_offset(const void* address) {
	return ((((uint64_t)address) << 16) >> 16);
}

/*
 * Build virtual address from given segment number and segment offset.
 * It is assumed, that segment number is not bigger then 65535, 
 * and that segment offset is not bigger then 281474976710655
 */
void* build_virtual_address(uint32_t segment_num, uint64_t segment_offset) {
	return (void*)(((uint64_t)segment_num << 48) | segment_offset);
}

/*
 * Finds segment to which given virtual address belongs, and returns pointer to this segment
 */
segment_descriptor_t* find_segment(const region_t* region, const void* address) {
	uint32_t segment_num = get_segment_num(address);
	if (segment_num == DEFAULT_SEGMENT_NUM) {
		/* The builtin default region segment */
		return region->desc;
	}
	return region->allocs->data[segment_num];
}

/*
 * Find the number of accessed field in given segment
 */
size_t find_field_number(const segment_descriptor_t* desc, const void* address) {
	return (size_t)(get_segment_offset(address) / desc->align);
}

/*
 * Get the real address of memory of given virtual address two 
 * transactional memory 
 */
void* get_physical_address(const segment_descriptor_t* desc, const void* address) {
	uint64_t segment_offset = get_segment_offset(address);
	return desc->data + segment_offset;
}
