#pragma once

#include <stdint.h>

#include "structs.h"

#define DEFAULT_SEGMENT_NUM 65535 

uint32_t get_segment_num(const void* address);
uint64_t get_segment_offset(const void* address);
void* build_virtual_address(uint32_t segment_num, uint64_t segment_offset);
segment_descriptor_t* find_segment(const region_t* region, const void* address);
size_t find_field_number(const segment_descriptor_t* desc, const void* address);
void* get_physical_address(const segment_descriptor_t* desc, const void* address);
