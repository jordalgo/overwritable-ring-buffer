#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "ring_buffer_lib.h"

typedef struct {
  int a;
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
    printf("Producer pos %lu\n", rng_buf->producer_pos);
    printf("Consumer pos %lu\n", rng_buf->consumer_pos);
    printf("\n");
}

void test_basic()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t c_item = consume(&rng_buf);
    assert(c_item.slot == NULL);
    
    entry_t p_item = reserve(&rng_buf);
    assert(p_item.slot != NULL);
    
    c_item = consume(&rng_buf);
    assert(c_item.slot == NULL);
    
    ((ds_t*)p_item.slot)->a = -1;
    
    commit(&rng_buf, &p_item);
    assert(p_item.slot == NULL);
    
    c_item = consume(&rng_buf);
    assert(c_item.slot != NULL);
    assert(((ds_t*)c_item.slot)->a == -1);

    release(&rng_buf, &c_item);
    assert(c_item.slot == NULL);
}

void test_complex_struct()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(task_t));
    
    entry_t p_item = reserve(&rng_buf);
    assert(p_item.slot != NULL);
    
    memcpy(&((task_t*)p_item.slot)->comm, "hello", 16);
    ((task_t*)p_item.slot)->pid = 123;
    ((task_t*)p_item.slot)->tid = 456;
    commit(&rng_buf, &p_item);
     
    entry_t c_item = consume(&rng_buf);
    assert(c_item.slot != NULL);
    assert(strcmp(((task_t*)c_item.slot)->comm, "hello") == 0);
    assert(((task_t*)c_item.slot)->pid == 123);
    assert(((task_t*)c_item.slot)->tid == 456);
}

void test_destroy()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -1;
    commit(&rng_buf, &p_item1);
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -2;
    commit(&rng_buf, &p_item2);
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -3;
    commit(&rng_buf, &p_item3);
    
    destroy_ring_buf(&rng_buf);
    
    assert(rng_buf.buf == NULL);
    assert(rng_buf.queue == NULL);
    assert(rng_buf.committed.data == NULL);
}

void test_commit_ordering()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -3;
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.slot == NULL);
    
    commit(&rng_buf, &p_item3);
    commit(&rng_buf, &p_item2);
    
    entry_t c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.slot)->a == -3);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.slot)->a == -2);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.slot == NULL);
    
    commit(&rng_buf, &p_item1);
    
    c_item = consume(&rng_buf);
    assert(((ds_t*)c_item.slot)->a == -1);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.slot == NULL);
}

void test_reserve()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -3;
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.slot == NULL);
    
    commit(&rng_buf, &p_item1);
    commit(&rng_buf, &p_item2);
    
    entry_t c_item1 = consume(&rng_buf);
    entry_t c_item2 = consume(&rng_buf);
    
    // Can't consume or reserve
    entry_t c_item3 = consume(&rng_buf);
    assert(c_item3.slot == NULL);
    p_item4 = reserve(&rng_buf);
    assert(p_item4.slot == NULL);
}

void test_overwrite_ordering()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -1;
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -2;
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -3;
    
    commit(&rng_buf, &p_item2);
    commit(&rng_buf, &p_item1);
            
    entry_t c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -2);
    release(&rng_buf, &c_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -4;
    
    // [(x, x), (-1), [-4]]
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -5;
    
    // [[-5], (-1), [-4]]
    
    commit(&rng_buf, &p_item1); 
   // [[-5], (-1), (-4)]
    
    commit(&rng_buf, &p_item3);
    // [(-3), (-1), (-4)]
        
    commit(&rng_buf, &p_item2);
    // [(-3), (-5), (-4)]
        
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -4);
    // [(-3), (-5), [-4]]
    
    p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -6;
    
    commit(&rng_buf, &p_item3);
    // [x, (-5), [-6]]
    
    entry_t c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.slot)->a == -5);
    // [(-6), [-5], [-4]]
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -7;
    commit(&rng_buf, &p_item2);
    
    // [(-7), x, x]
        
    entry_t c_item3 = consume(&rng_buf);
    assert(((ds_t*)c_item3.slot)->a == -7);
    // [x, x, x]
    
    entry_t p_item4 = reserve(&rng_buf);
    assert(p_item4.slot == NULL);
    
    release(&rng_buf, &c_item2);
    // [[], x, x]

    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -8;
    commit(&rng_buf, &p_item2);
    
    // [x, (-8), x]
    
    c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.slot)->a == -8);
    // [x, x, x]
    
    release(&rng_buf, &c_item3);    
    release(&rng_buf, &c_item1);
    
    entry_t c_item4 = consume(&rng_buf);
    assert(c_item4.slot == NULL);
    
    // [[], [], x]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -9;
    // [[], x, x]
        
    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -10;
    // [x, x, x]
        
    commit(&rng_buf, &p_item1);
    // [x, x, (-9)]
    
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -9);
    // [x, x, x]
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.slot == NULL);
    
    commit(&rng_buf, &p_item2);
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -11;
    commit(&rng_buf, &p_item2);
    
    // [x, [-11], x]
    
    release(&rng_buf, &c_item1);
    release(&rng_buf, &c_item2);
    
    // [[], [-11], []]
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.slot != NULL);
    assert(((ds_t*)c_item3.slot)->a == -11);
    
    // [[], x, []]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -12;
    commit(&rng_buf, &p_item1);
    
    p_item2 = reserve(&rng_buf);
    assert(p_item2.slot != NULL);
    ((ds_t*)p_item2.slot)->a = -13;
    commit(&rng_buf, &p_item2);
    
    // [[-13], x, [-12]]
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -14;
    commit(&rng_buf, &p_item1);
    
    // [[-13], [-14], x]
    
    c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.slot)->a == -13);
    
    release(&rng_buf, &c_item3);
    release(&rng_buf, &c_item2);
    
    c_item2 = consume(&rng_buf);
    assert(c_item2.slot != NULL);
    assert(((ds_t*)c_item2.slot)->a == -14);
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.slot == NULL);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -15;
    commit(&rng_buf, &p_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -16;
    commit(&rng_buf, &p_item1);
    
    p_item1 = reserve(&rng_buf);
    assert(p_item1.slot != NULL);
    ((ds_t*)p_item1.slot)->a = -17;
    commit(&rng_buf, &p_item1);
    
    c_item1 = consume(&rng_buf);
    assert(c_item1.slot != NULL);
    assert(((ds_t*)c_item1.slot)->a == -16);
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.slot != NULL);
    assert(((ds_t*)c_item3.slot)->a == -17);
}

void test_longer_buffer()
{
     rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 5, sizeof(ds_t));
    
    entry_t p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -1;
    commit(&rng_buf, &p_item1);
    
    entry_t p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -2;
    commit(&rng_buf, &p_item2);
    
    entry_t p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -3;
    commit(&rng_buf, &p_item3);
    
    entry_t p_item4 = reserve(&rng_buf);
    ((ds_t*)p_item4.slot)->a = -4;
    commit(&rng_buf, &p_item4);
    
    entry_t p_item5 = reserve(&rng_buf);
    ((ds_t*)p_item5.slot)->a = -5;
    commit(&rng_buf, &p_item5);
    
    p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -6;
    commit(&rng_buf, &p_item1);
    
    p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -7;
    commit(&rng_buf, &p_item2);
    
    p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -8;
    commit(&rng_buf, &p_item3);
    
    p_item4 = reserve(&rng_buf);
    ((ds_t*)p_item4.slot)->a = -9;
    commit(&rng_buf, &p_item4);
    
    entry_t c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -5);
    release(&rng_buf, &c_item1);
    
    p_item1 = reserve(&rng_buf);
    ((ds_t*)p_item1.slot)->a = -10;
    
    p_item2 = reserve(&rng_buf);
    ((ds_t*)p_item2.slot)->a = -11;
    
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -7);
    
    p_item3 = reserve(&rng_buf);
    ((ds_t*)p_item3.slot)->a = -12;
    commit(&rng_buf, &p_item3);
    
    p_item4 = reserve(&rng_buf);
    ((ds_t*)p_item4.slot)->a = -13;
    commit(&rng_buf, &p_item4);
    
    entry_t c_item2 = consume(&rng_buf);
    assert(((ds_t*)c_item2.slot)->a == -12);
    
    p_item5 = reserve(&rng_buf);
    ((ds_t*)p_item5.slot)->a = -14;
    commit(&rng_buf, &p_item5);
    
    release(&rng_buf, &c_item1);
    
    p_item4 = reserve(&rng_buf);
    ((ds_t*)p_item4.slot)->a = -15;
    commit(&rng_buf, &p_item4);
    
    c_item1 = consume(&rng_buf);
    assert(((ds_t*)c_item1.slot)->a == -14);
}

int main()
{
    test_basic();
    test_complex_struct();
    test_destroy();
    test_commit_ordering();
    test_reserve();
    test_overwrite_ordering();
    test_longer_buffer();
    
    return 0;
}