#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "ring_buffer_lib.h"

typedef struct {
  int a;
  unsigned long b;
} ds_t;

typedef struct {
    char comm[16];
    unsigned long pid;
    unsigned long tid;
} task_t;

void print_rng_buf_state(rng_buf_t* rng_buf) {
    int i = 0;
    printf("Queue positions:\n");
    for (i = 0; i < rng_buf->nr_entries; i++) {
        printf("%d ", rng_buf->queue[i]);
    }
    printf("\n");
    printf("Values:\n");
    for (i = 0; i < rng_buf->nr_entries; i++) {
        if (rng_buf->queue[i] == -1) {
            printf("R ");
        } else {
            ds_t* ds = (ds_t*)&rng_buf->buf[rng_buf->queue[i] * rng_buf->entry_size];
            printf("%d ", ds->a);
        }
    }
    printf("\n");
    printf("Producer pos %d\n", rng_buf->producer_pos);
    printf("Consumer pos %d\n", rng_buf->consumer_pos);
    printf("\n");
}

void test_basic()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t c_item = consume(&rng_buf);
    assert(c_item.ds == NULL);
    
    entry_t p_item = reserve(&rng_buf);
    assert(p_item.ds != NULL);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds == NULL);
    
    ((ds_t*)p_item.ds)->a = -1;
    ((ds_t*)p_item.ds)->b = 1;
    
    commit(&rng_buf, &p_item);
    assert(p_item.ds == NULL);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t*)c_item.ds)->a == -1);
    assert(((ds_t*)c_item.ds)->b == 1);

    release(&rng_buf, &c_item);
    assert(c_item.ds == NULL);
}

void test_commit_ordering()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.ds)->a = -1;
    ((ds_t*)p_item1.ds)->b = 1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.ds)->a = -2;
    ((ds_t*)p_item2.ds)->b = 2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.ds)->a = -3;
    ((ds_t*)p_item3.ds)->b = 3;
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.ds == NULL);
    
    commit(&rng_buf, &p_item3);
    commit(&rng_buf, &p_item2);
    
    entry_t c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.ds)->a == -3);
    assert(((ds_t*)c_item.ds)->b == 3);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.ds)->a == -2);
    assert(((ds_t*)c_item.ds)->b == 2);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds == NULL);
    
    commit(&rng_buf, &p_item1);
    
    c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.ds)->a == -1);
    assert(((ds_t*)c_item.ds)->b == 1);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds == NULL);
}

void test_reserve()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.ds)->a = -1;
    ((ds_t*)p_item1.ds)->b = 1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.ds)->a = -2;
    ((ds_t*)p_item2.ds)->b = 2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.ds)->a = -3;
    ((ds_t*)p_item3.ds)->b = 3;
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.ds == NULL);
    
    commit(&rng_buf, &p_item1);
    commit(&rng_buf, &p_item2);
    
    entry_t c_item1 = consume(&rng_buf);
    entry_t c_item2 = consume(&rng_buf);
    
    // Can't consume or reserve
    entry_t c_item3 = consume(&rng_buf);
    assert(c_item3.ds == NULL);
    p_item4 = reserve(&rng_buf);
    assert(p_item4.ds == NULL);
}

void test_overwrite_ordering()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.ds)->a = -1;
    ((ds_t*)p_item1.ds)->b = 1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.ds)->a = -2;
    ((ds_t*)p_item2.ds)->b = 2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.ds)->a = -3;
    ((ds_t*)p_item3.ds)->b = 3;
    
    commit(&rng_buf, &p_item2);
    commit(&rng_buf, &p_item1);
            
    entry_t c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.ds)->a == -2);
    assert(((ds_t*)c_item1.ds)->b == 2);
    release(&rng_buf, &c_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -4;
    ((ds_t*)p_item1.ds)->b = 4;
    
    // [(x, x), (-1, -1), [-4, 4]]
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -5;
    ((ds_t*)p_item2.ds)->b = 5;
    
    // [[-5, -5], (-1, -1), [-4, 4]]
    
    commit(&rng_buf, &p_item1); 
   // [[-5, -5], (-1, -1), (-4, 4)]
    
    commit(&rng_buf, &p_item3);
    // [(-3, -3), (-1, -1), (-4, -4)]
        
    commit(&rng_buf, &p_item2);
    // [(-3, 3), (-5, 5), (-4, 4)]
        
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.ds)->a == -4);
    assert(((ds_t*)c_item1.ds)->b == 4);
    // [(-3, 3), (-5, 5), [-4, -4]]
    
    p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.ds)->a = -6;
    ((ds_t*)p_item3.ds)->b = 6;
    
    commit(&rng_buf, &p_item3);
    // [x, (-5, 5), [-6, -6]]
    
    entry_t c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.ds)->a == -5);
    assert(((ds_t*)c_item2.ds)->b == 5);
    // [(-6, -6), [-5, 5], [-4, -4]]
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -7;
    ((ds_t*)p_item2.ds)->b = 7;
    commit(&rng_buf, &p_item2);
    
    // [(-7, -7), x, x]
        
    entry_t c_item3 = consume(&rng_buf);
    assert(((ds_t*)c_item3.ds)->a == -7);
    assert(((ds_t*)c_item3.ds)->b == 7);
    // [x, x, x]
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.ds == NULL);
    
    release(&rng_buf, &c_item2);
    // [[], x, x]

    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -8;
    ((ds_t*)p_item2.ds)->b = 8;
    commit(&rng_buf, &p_item2);
    
    // [x, (-8, -8), x]
    
    c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.ds)->a == -8);
    assert(((ds_t*)c_item2.ds)->b == 8);
    // [x, x, x]
    
    release(&rng_buf, &c_item3);    
    release(&rng_buf, &c_item1);
    
    entry_t c_item4 = consume(&rng_buf);
    assert(c_item4.ds == NULL);
    
    // [[], [], x]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -9;
    ((ds_t*)p_item1.ds)->b = 9;
    // [[], x, x]
        
    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -10;
    ((ds_t*)p_item2.ds)->b = 10;
    // [x, x, x]
        
    commit(&rng_buf, &p_item1);
    // [x, x, (-9, 9)]
    
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.ds)->a == -9);
    assert(((ds_t*)c_item1.ds)->b == 9);
    // [x, x, x]
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.ds == NULL);
    
    commit(&rng_buf, &p_item2);
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -11;
    ((ds_t*)p_item2.ds)->b = 11;
    commit(&rng_buf, &p_item2);
    
    // [x, [-11, 11], x]
    
    release(&rng_buf, &c_item1);
    release(&rng_buf, &c_item2);
    
    // [[], [-11, 11], []]
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.ds != NULL);
    assert(((ds_t*)c_item3.ds)->a == -11);
    assert(((ds_t*)c_item3.ds)->b == 11);
    
    // [[], x, []]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -12;
    ((ds_t*)p_item1.ds)->b = 12;
    commit(&rng_buf, &p_item1);
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.ds != NULL);
    ((ds_t*)p_item2.ds)->a = -13;
    ((ds_t*)p_item2.ds)->b = 13;
    commit(&rng_buf, &p_item2);
    
    // [[-13, 13], x, [-12, 12]]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -14;
    ((ds_t*)p_item1.ds)->b = 14;
    commit(&rng_buf, &p_item1);
    
    // [[-13, 13], [-14, 14], x]
    
    c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.ds)->a == -13);
    assert(((ds_t*)c_item2.ds)->b == 13);
    
    release(&rng_buf, &c_item3);
    release(&rng_buf, &c_item2);
    
    c_item2 = consume(&rng_buf);
    assert(c_item2.ds != NULL);
    assert(((ds_t*)c_item2.ds)->a == -14);
    assert(((ds_t*)c_item2.ds)->b == 14);
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.ds == NULL);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -15;
    ((ds_t*)p_item1.ds)->b = 15;
    commit(&rng_buf, &p_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -16;
    ((ds_t*)p_item1.ds)->b = 16;
    commit(&rng_buf, &p_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.ds != NULL);
    ((ds_t*)p_item1.ds)->a = -17;
    ((ds_t*)p_item1.ds)->b = 17;
    commit(&rng_buf, &p_item1);
    
    c_item1 = consume(&rng_buf);
    assert(c_item1.ds != NULL);
    assert(((ds_t*)c_item1.ds)->a == -16);
    assert(((ds_t*)c_item1.ds)->b == 16);
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.ds != NULL);
    assert(((ds_t*)c_item3.ds)->a == -17);
    assert(((ds_t*)c_item3.ds)->b == 17);
}

int main()
{
    test_basic();
    test_commit_ordering();
    test_reserve();
    test_overwrite_ordering();
    
    return 0;
}