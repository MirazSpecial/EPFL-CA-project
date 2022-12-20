#pragma once

#include <stdlib.h>
#include <stdbool.h>

#define VECTOR_DEFAULT_SIZE 8 /* Hyperparameter */

/* Vector of void pointers */
struct vector {
    void** data;
    size_t size, size_max;
};
typedef struct vector vector_t;

/* Vector of const void pointers */
struct cvector {
    const void** data;
    size_t size, size_max;
};
typedef struct cvector cvector_t;

vector_t* vector_init(size_t n);
vector_t* vector_copy(const vector_t* vector);
void vector_destroy(vector_t* vector);
void vector_deep_destroy(vector_t* vector);
size_t vector_find_last(const vector_t* vector, const void* element);
bool vector_push_back(vector_t* vector, void* element);
vector_t* vector_no_duplicates(const vector_t* vector);

cvector_t* cvector_init(size_t n);
void cvector_destroy(cvector_t* cvector);
bool cvector_push_back(cvector_t* cvector, const void* element);
