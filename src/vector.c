#include <string.h>
#include <stdio.h>

#include "vector.h"

vector_t* vector_init(size_t n) {
    vector_t* vector_ptr = (vector_t*)malloc(sizeof(vector_t));
    if (!vector_ptr)
        return NULL;
    void** buffer = (void**)malloc(n * sizeof(void*));
    if (!buffer) {
        free(vector_ptr);
        return NULL;
    }
    vector_ptr->data = buffer;
    vector_ptr->size = 0;
    vector_ptr->size_max = n;
    return vector_ptr;
}

cvector_t* cvector_init(size_t n) {
    cvector_t* cvector_ptr = (cvector_t*)malloc(sizeof(cvector_t));
    if (!cvector_ptr)
        return NULL;
    const void** buffer = (const void**)malloc(n * sizeof(const void*));
    if (!buffer) {
        free(cvector_ptr);
        return NULL;
    }
    cvector_ptr->data = buffer;
    cvector_ptr->size = 0;
    cvector_ptr->size_max = n;
    return cvector_ptr;
}

vector_t* vector_copy(const vector_t* vector) {
    vector_t* copy = vector_init(vector->size_max);
    if (!copy)
        return NULL;
    for (size_t i = 0; i < vector->size; ++i) {
        vector_push_back(copy, vector->data[i]);
    }
    return copy;
}

void vector_destroy(vector_t* vector) {
    free(vector->data);
    free(vector);
}

void cvector_destroy(cvector_t* cvector) {
    free(cvector->data);
    free(cvector);
}

void vector_deep_destroy(vector_t* vector) {
    for (size_t i = 0; i < vector->size; ++i) {
        free(vector->data[i]);
    }
    vector_destroy(vector);
}

size_t vector_find_last(const vector_t* vector, const void* element) {
    for (size_t i = vector->size - 1; i != (size_t)-1; i--) {
        if (vector->data[i] == element)
            return i; 
    }
    return (size_t)-1; /* Element not in the vector */
}

bool vector_push_back(vector_t* vector, void* element) {
    if (vector->size == vector->size_max) {
        /* Resizing vector */
        vector->size_max *= 2;
        void** buffer = (void**)malloc(vector->size_max * sizeof(void*));
        if (!buffer)
            return false; /* Failed to add element to vector */
        
        for (size_t i = 0; i < vector->size; ++i)
            buffer[i] = vector->data[i];
        free(vector->data);
        vector->data = buffer;
    }
    vector->data[vector->size] = element;
    vector->size++;
    return true; /* Element added to vector */
}

bool cvector_push_back(cvector_t* cvector, const void* element) {
    if (cvector->size == cvector->size_max) {
        /* Resizing cvector */
        cvector->size_max *= 2;
        const void** buffer = (const void **)malloc(cvector->size_max * sizeof(const void*));
        if (!buffer)
            return false; /* Failed to add element to cvector */
        
        for (size_t i = 0; i < cvector->size; ++i)
            buffer[i] = cvector->data[i];
        free(cvector->data);
        cvector->data = buffer;
    }
    cvector->data[cvector->size] = element;
    cvector->size++;
    return true; /* Element added to vector */
}

/* Function used for compering elements in vector_sort */
int vector_sort_cmp (const void* e1, const void* e2) {
   return (*(void**)e1 - *(void**)e2);
}

void vector_sort(vector_t* vector) {
    qsort(vector->data, vector->size, sizeof(void*), vector_sort_cmp);
}

vector_t* vector_no_duplicates(const vector_t* vector) {
    vector_t* copy = vector_copy(vector);
    if (!copy || copy->size == 0)
        return copy;
    vector_sort(copy);

    size_t last_unique = 0;
    for (size_t i = 1; i < copy->size; ++i) {
        if (copy->data[i] != copy->data[last_unique]) {
            last_unique++;
            copy->data[last_unique] = copy->data[i];
        }
    }
    copy->size = last_unique + 1;
    return copy;
}
