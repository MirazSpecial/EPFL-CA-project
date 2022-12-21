// Requested feature: nrandr48
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>

#include "macros.h"
#include "structs.h"
#include "tm.h"

/*

    1. Lets assume there is one lock that needs to be aquired doing tm_free tm_alloc
    2. Lets assume that transaction does at most one tm_free 

    With those two assumptions, we could segment_destroy() segments previously marked as to_delete,
    in given tm_free and at the end mark his own as to_delete. We can do that, because we know,
    that if given tm_free is running, then all transactions of previous tm_frees already ended.

    UPDATE
    Not really, transaction can do tm_free and then many different things

*/

/*
 * About grader scenario we have around
 * Event:               Number of occurences:
 * tm_read              70,000,000
 * tm_write             1,400,000
 * tm_begin             1,400,000
 * tm_end               1,400,000
 * tm_alloc             300
 * tm_free              300
 * 
 * Typically there are around 7 segments
 *
 * We have to types of transactions (50/50)
 * - big read onlys, reading about 1000 fields
 * - small rw, reading less then 40 writing up to 3 (usually 2)
 * (4 outliners with reading 0 and writing 129)
 * All sizes in read or write are 8
 * 
 * Alocation is always 1048 bytes
 */


/* Constnts */

const char* const_buffer = "aaaabbbbccccddddeeeeffffgggghhhhiiiijjjjkkkkllllmmmmnnnnoooopppp";
const int multi_1_changes = 100000;
const int multi_2_changes = 1000;

/* Basic transactions */

bool trans_write_read(shared_t tm, void* buffer, size_t size, size_t cb_off);
bool trans_write_read_2(shared_t tm, size_t size, size_t off,
                        const void* buffer1, size_t off1,
                        void* buffer2, size_t off2);
// bool trans_write_read_3(shared_t tm, )

/* Single thread scenarios */

void single_1();
void single_2();
void single_3();

/* Multi thread scenarios */

void multi_1(); /*  */
void multi_2();


/* Global */

shared_t global_tm;


int main () {
    srand(time(NULL));

    // multi_1();
    multi_2(10, 2);
    return 0;
}




void* multi_2_worker(void* nums_ptr) {
    unsigned seed = time(NULL) ^ getpid() ^ pthread_self();
    long long* val1 = (long long*)malloc(sizeof(long long));
    long long* val2 = (long long*)malloc(sizeof(long long));
    void* start = tm_start(global_tm);
    size_t align = tm_align(global_tm);
    int nums = *((int*)nums_ptr);

    for (unsigned i = 0; i < multi_2_changes; ++i) {
        int num1 = rand_r(&seed) % nums, num2 = rand_r(&seed) % nums;
        // printf("num1: %d, num2: %d\n", num1, num2);

        tx_t tx = tm_begin(global_tm, false);
        if (tx == invalid_tx)
            continue; /* Transaction invalid, go next */

        if (!tm_read(global_tm, tx, start + align * num1, align, (void*)val1))
            continue; /* Transaction aborted, go next */
        (*val1)--;
        if (!tm_write(global_tm, tx, (void*)val1, align, start + align * num1))
            continue; /* Transaction aborted, go next */

        if (!tm_read(global_tm, tx, start + align * num2, align, (void*)val2))
            continue; /* Transaction aborted, go next */
        (*val2)++;
        if (!tm_write(global_tm, tx, (void*)val2, align, start + align * num2))
            continue; /* Transaction aborted, go next */

        if (!tm_end(global_tm, tx))
            continue; /* Transaction aborted, go next */
    }
    free(val1);
    free(val2);
    return NULL;
}

void multi_2(const size_t nums, const unsigned threads) {
    /*
     * Same as multi_1, but now we have nums numbers in memory and threads threads.
     * Worker thread chooses two numbers at random and transfers money from one to the other.
     */

    global_tm = tm_create(nums * sizeof(long long), sizeof(long long));
    if (global_tm == invalid_shared) {
        printf("multi_1 invalid_shared!\n");
        return;
    } 

    pthread_t handlers[threads];
    // tx_count = 0;
    // tx_aborted = 0;
    size_t nums_arg = nums;

    for (unsigned i = 0; i < threads; i++)
        assert(!pthread_create(&handlers[i], NULL, multi_2_worker, (void*)(&nums_arg)));
    for (unsigned i = 0; i < threads; i++)
        assert(!pthread_join(handlers[i], NULL));
    
    /* Now we need to check is sum is still 0 */

    void* start = tm_start(global_tm);
    size_t align = tm_align(global_tm);
    long long* vals = (long long*)malloc(nums * sizeof(long long));
    tx_t tx = tm_begin(global_tm, true);
    assert(tx != invalid_tx);
    for (size_t i = 0; i < nums; ++i)
        assert(tm_read(global_tm, tx, start + align * i, align, (void*)(vals + i)));
    assert(tm_end(global_tm, tx));

    long long sum = 0;
    for (size_t i = 0; i < nums; ++i)
        sum += vals[i];
    printf(sum == 0 ? "[multi_2] FINAL CORRECT!\n" : "[multi_2] FINAL WRONG!\n");
    printf("Threads: %u, nums: %lu\n", threads, nums);
    printf("Values:   ");
    for (size_t i = 0; i < nums; ++i)
        printf("%lld ", vals[i]);
    printf("\n");
}









void* multi_1_worker(void* unused(null)) {
    unsigned seed = time(NULL) ^ getpid() ^ pthread_self();
    long long* val1 = (long long*)malloc(sizeof(long long));
    long long* val2 = (long long*)malloc(sizeof(long long));
    void* start = tm_start(global_tm);
    size_t align = tm_align(global_tm);

    for (unsigned i = 0; i < multi_1_changes; ++i) {
        int num1 = rand_r(&seed) % 2, num2 = rand_r(&seed) % 2;
        // printf("Worker: %lu, loop: %u, num1: %d, num2: %d\n", pthread_self(), i, num1, num2);
        // num1 = 0, num2 = 1;

        tx_t tx = tm_begin(global_tm, false);
        if (tx == invalid_tx)
            continue; /* Transaction invalid, go next */

        if (!tm_read(global_tm, tx, start + align * num1, align, (void*)val1))
            continue; /* Transaction aborted, go next */
        (*val1)--;
        if (!tm_write(global_tm, tx, (void*)val1, align, start + align * num1))
            continue; /* Transaction aborted, go next */

        if (!tm_read(global_tm, tx, start + align * num2, align, (void*)val2))
            continue; /* Transaction aborted, go next */
        (*val2)++;
        if (!tm_write(global_tm, tx, (void*)val2, align, start + align * num2))
            continue; /* Transaction aborted, go next */

        if (!tm_end(global_tm, tx))
            continue; /* Transaction aborted, go next */
    }
    free(val1);
    free(val2);
    return NULL;
}

void multi_1() {
    /*
     * The idea is, that we have two numbers (initially 0) in tm and we move value
     * between them. So we randomly choose n1 (one of those numbers) and
     * n2 (one of those numbers, possible n1=n2) and transfer 1 from n1 to n2.
     * One such process is a transaction. thread_num threads do many of those 
     * transactions and at the end sum of those two numbers should be equal to zero.
     */

    global_tm = tm_create(64, 8);
    if (global_tm == invalid_shared) {
        printf("multi_1 invalid_shared!\n");
        return;
    } 

    const unsigned threads_num = 3;
    pthread_t handlers[threads_num];
    // tx_count = 0;
    // tx_aborted = 0;

    for (unsigned i = 0; i < threads_num; i++)
        assert(!pthread_create(&handlers[i], NULL, multi_1_worker, NULL));
    for (unsigned i = 0; i < threads_num; i++)
        assert(!pthread_join(handlers[i], NULL));
    
    /* Now we need to check is sum is still 0 */

    void* start = tm_start(global_tm);
    size_t align = tm_align(global_tm);
    long long* val1 = (long long*)malloc(sizeof(long long));
    long long* val2 = (long long*)malloc(sizeof(long long));
    tx_t tx = tm_begin(global_tm, true);
    assert(tx != invalid_tx);
    assert(tm_read(global_tm, tx, start, align, (void*)val1));
    assert(tm_read(global_tm, tx, start + align, align, (void*)val2));
    assert(tm_end(global_tm, tx));

    printf(*val1 + *val2 == 0? "[multi_1] FINAL CORRECT\n" : "[multi_2] FINAL WRONG\n");
    printf("[multi_1] val1: %lld, val2: %lld\n", *val1, *val2);
}











bool trans_write_read(shared_t tm, void* buffer, size_t size, size_t cb_off) {
    /* Copy size bytes with cb_off offset from const_buffer to tm, and read them to buffer */
    assert(size % tm_align(tm) == 0);
    assert(size <= tm_size(tm));

    tx_t tx = tm_begin(tm, false);
    if (tx == invalid_tx)
        return false; /* Transaction aborted */
    if (!tm_write(tm, tx, const_buffer + cb_off, size, tm_start(tm)))
        return false;
    if (!tm_read(tm, tx, tm_start(tm), size, buffer))
        return false;
    if (!tm_end(tm, tx))
        return false;
    return true;
}

bool trans_write_read_2(shared_t tm, size_t size, size_t off, 
                        const void* buffer1, size_t off1,
                        void* buffer2, size_t off2) {
    /* Copy size bytes from buffer1 (offset off1) to tm (offset off) 
       and back to buffer2 (offset off2) */
    assert(size % tm_align(tm) == 0);
    assert(size <= tm_size(tm));
    assert(off % tm_align(tm) == 0);

    tx_t tx = tm_begin(tm, false);
    if (tx == invalid_tx)
        return false; /* Transaction aborted */
    if (!tm_write(tm, tx, buffer1 + off1, size, tm_start(tm) + off))
        return false;
    if (!tm_read(tm, tx, tm_start(tm) + off, size, buffer2 + off2))
        return false;
    if (!tm_end(tm, tx))
        return false;
    return true;
}



void single_1() {
    void* buffer = malloc(64);
    memset(buffer, 0, 64);
    
    shared_t tm = tm_create(800, 8);
    if (tm == invalid_shared) {
        printf("single_1 invalid_shared!\n");
        return;
    } 

    if (!trans_write_read(tm, buffer, 16, 0)) {
        printf("single_1 failed!\n");
        return;
    }

    for (int i = 0; i < 16; ++i) 
        printf("%d ", ((char*)buffer)[i]);
    printf("\n");

    if (!trans_write_read(tm, buffer, 8, 8)) {
        printf("single_1 failed!\n");
        return;
    }

    for (int i = 0; i < 16; ++i) 
        printf("%d ", ((char*)buffer)[i]);
    printf("\n");

    tm_destroy(tm);
    free(buffer);
}

void single_2() {
    void* buffer1 = malloc(64);
    memset(buffer1, 1, 64);
    void* buffer2 = malloc(64);
    memset(buffer2, 2, 64);
    
    shared_t tm = tm_create(800, 8);
    if (tm == invalid_shared) {
        printf("single_2 invalid_shared!\n");
        return;
    } 

    if (!trans_write_read_2(tm, 16, 0, buffer1, 0, buffer2, 0)) {
        printf("single_2 failed!\n");
        return;
    }

    printf("---------- BUFFERS ----------\n");
    for (int i = 0; i < 32; ++i) 
        printf("%d ", ((char*)buffer1)[i]);
    printf("\n");
    for (int i = 0; i < 32; ++i) 
        printf("%d ", ((char*)buffer2)[i]);
    printf("\n");

    if (!trans_write_read_2(tm, 16, 16, buffer2, 8, buffer1, 0)) {
        printf("single_2 failed!\n");
        return;
    }

    printf("---------- BUFFERS ----------\n");
    for (int i = 0; i < 32; ++i) 
        printf("%d ", ((char*)buffer1)[i]);
    printf("\n");
    for (int i = 0; i < 32; ++i) 
        printf("%d ", ((char*)buffer2)[i]);
    printf("\n");

    tm_destroy(tm);
    free(buffer1);
    free(buffer2);
}

void single_3() {
    /* Increase number many times*/
    const int increments = (int)1e6;

    void* buffer1 = malloc(8);
    memset(buffer1, 0, 8);
    void* buffer2 = malloc(8);
    memset(buffer2, 0, 8);

    shared_t tm = tm_create(800, 8);
    if (tm == invalid_shared) {
        printf("single_2 invalid_shared!\n");
        return;
    } 

    for (int i = 0; i < increments; ++i) {
        if (!trans_write_read_2(tm, 8, 0, buffer1, 0, buffer2, 0)) {
            printf("single_2 failed!\n");
            return;
        }
        
        // printf("Final buffer1 value: %lld\n", *(long long*)buffer1);    
        long long num = *(long long*)buffer2;
        num++;
        *(long long*)buffer1 = num;
    }

    printf("Final buffer1 value: %lld\n", *(long long*)buffer1);

    tm_destroy(tm);
    free(buffer1);
    free(buffer2);
}