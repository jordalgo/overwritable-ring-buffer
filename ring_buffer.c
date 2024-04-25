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

void debug_rb_ds(rng_buf_t * rng_buf) {
    for (int i = 0; i < rng_buf->entries; ++i) {
        printf("entry idx %d. A: %d B: %lu\n", i, ((ds_t *)rng_buf->buf + i)->a, ((ds_t *)rng_buf->buf + i)->b);
    }
    printf("consumer_pos: %d, producer_pos: %d\n", rng_buf->consumer_pos, rng_buf->producer_pos);
    printf("nr_can_consume: %d\n", rng_buf->nr_can_consume);
}

void debug_rb_task(rng_buf_t * rng_buf) {
    for (int i = 0; i < rng_buf->entries; ++i) {
        printf("entry idx %d. comm: %s pid: %lu tid: %lu\n", i, ((task_t *)rng_buf->buf + i)->comm, ((task_t *)rng_buf->buf + i)->pid, ((task_t *)rng_buf->buf + i)->tid);
    }
    printf("consumer_pos: %d, producer_pos: %d\n", rng_buf->consumer_pos, rng_buf->producer_pos);
    printf("nr_can_consume: %d\n", rng_buf->nr_can_consume);
}

void test_basic()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    ds_t item1;
    item1.a = 1;
    item1.b = 2;
    
    ds_t item2;
    item2.a = 3;
    item2.b = 4;
    
    ds_t item3;
    item3.a = 5;
    item3.b = 6;
    
    entry_t c_item = consume(&rng_buf);
    assert(c_item.ds == NULL);
    
    assert(publish(&rng_buf, &item1) == 0);
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 1);
    assert(((ds_t *)c_item.ds)->b == 2);
    
    publish(&rng_buf, &item2);
    
    entry_t c_item2 = consume(&rng_buf);
    assert(c_item2.ds != NULL);
    assert(((ds_t *)c_item2.ds)->a == 3);
    assert(((ds_t *)c_item2.ds)->b == 4);

    entry_t c_item3 = consume(&rng_buf);
    assert(c_item3.ds == NULL);
    
    destroy_ring_buf(&rng_buf);
    
    init_ring_buf(&rng_buf, 5, sizeof(task_t));
    
    task_t task1;
    task1.pid = 30;
    task1.tid = 10;
    
    char comm[16] = "tomato";
    memcpy(task1.comm, comm, sizeof(comm));
    
    assert(publish(&rng_buf, &task1) == 0);
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(strcmp(((task_t *)c_item.ds)->comm, task1.comm) == 0);
    assert(((task_t *)c_item.ds)->pid == 30);
    assert(((task_t *)c_item.ds)->tid == 10);
}

void test_2()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
 
    ds_t item1;   
    item1.a = 1;
    item1.b = 2;
    
    ds_t item2;
    item2.a = 3;
    item2.b = 4;
    
    ds_t item3;
    item3.a = 5;
    item3.b = 6;
    
    ds_t item4;
    item4.a = 6;
    item4.b = 7;
    
    publish(&rng_buf, &item1);
    publish(&rng_buf, &item2);
    publish(&rng_buf, &item3);
    
    entry_t c_item = consume(&rng_buf);
    
    publish(&rng_buf, &item4);
    
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 1);
    assert(((ds_t *)c_item.ds)->b == 2);
    
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 5);
    assert(((ds_t *)c_item.ds)->b == 6);
    
    release(&rng_buf, &c_item);
    
    destroy_ring_buf(&rng_buf);
}

void test_3()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    ds_t item1;
    item1.a = 1;
    item1.b = 2;
    
    ds_t item2;
    item2.a = 3;
    item2.b = 4;
    
    ds_t item3;
    item3.a = 5;
    item3.b = 6;
    
    ds_t item4;
    item4.a = 6;
    item4.b = 7;
    
    assert(publish(&rng_buf, &item1) == 0);
    assert(publish(&rng_buf, &item2) == 0);
    assert(publish(&rng_buf, &item3) == 0);
    
    // state: [[1, 2], [3, 4], [5, 6]]
    
    entry_t c_item1 = consume(&rng_buf);
    entry_t c_item2 = consume(&rng_buf);
    entry_t c_item3 = consume(&rng_buf);
    
    // state: [(1, 2), (3, 4), (5, 6)]
    
    // Nothing can be consumed or published
    assert(publish(&rng_buf, &item4) == -1);
    entry_t c_item4 = consume(&rng_buf);

    assert(c_item4.ds == NULL);
    
    release(&rng_buf, &c_item2);
    release(&rng_buf, &c_item3);
    
    // state: [(1, 2), [-, -], [-, -]]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [(1, 2), [6, 7], [-, -]]

    c_item4 = consume(&rng_buf);
    assert(c_item4.ds != NULL);
    assert(((ds_t *)c_item4.ds)->a == 6);
    assert(((ds_t *)c_item4.ds)->b == 7);
    release(&rng_buf, &c_item4);
    
    // state: [(1, 2), [-, -], [-, -]]
    
    assert(publish(&rng_buf, &item2) == 0);
    
    // state: [(1, 2), [-, -], [3, 4]]
    
    assert(publish(&rng_buf, &item1) == 0);
    
    // state: [(1, 2), [1, 2], [3, 4]]
    
    assert(publish(&rng_buf, &item1) == 0);
    
    // state: [(1, 2), [1, 2], [1, 2]]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [(1, 2), [6, 7], [1, 2]]
    
    // Following the sequence above the 3rd slot (holding [1, 2]) is the oldest
    // for the available slots. The 1st slot is still being consumed and
    // therefore not eligible.
    c_item4 = consume(&rng_buf);
    assert(c_item4.ds != NULL);
    assert(((ds_t *)c_item4.ds)->a == 1);
    assert(((ds_t *)c_item4.ds)->b == 2);
    
    entry_t c_item5 = consume(&rng_buf);
    assert(c_item5.ds != NULL);
    assert(((ds_t *)c_item5.ds)->a == 6);
    assert(((ds_t *)c_item5.ds)->b == 7);
    
    entry_t c_item6 = consume(&rng_buf);
    assert(c_item6.ds == NULL);
    
    release(&rng_buf, &c_item1);
    release(&rng_buf, &c_item4);
    release(&rng_buf, &c_item5);
    
    // state: [[-, -], [-, -], [-, -]]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [[6, 7], [-, -], [-, -]]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [[6, 7], [6, 7], [-, -]]
    
    assert(publish(&rng_buf, &item1) == 0);
    
    // state: [[6, 7], [6, 7], [1, 2]]
    
    assert(publish(&rng_buf, &item2) == 0);
    
    // state: [[3, 4], [6, 7], [1, 2]]
    
    // Following the sequence above the 2nd slot (holding [6, 7]) is the oldest
    c_item4 = consume(&rng_buf);
    assert(c_item4.ds != NULL);
    assert(((ds_t *)c_item4.ds)->a == 6);
    assert(((ds_t *)c_item4.ds)->b == 7);
    
    assert(publish(&rng_buf, &item2) == 0);
    
    // state: [[3, 4], (6, 7), [3, 4]]
    
    assert(publish(&rng_buf, &item1) == 0);
    
    // state: [[1, 2], (6, 7), [3, 4]]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [[1, 2], (6, 7), [6, 7]]
    
    // Following the sequence above the 1st slot (holding [1, 2]) is the oldest
    c_item5 = consume(&rng_buf);
    assert(c_item5.ds != NULL);
    assert(((ds_t *)c_item5.ds)->a == 1);
    assert(((ds_t *)c_item5.ds)->b == 2);
    
    assert(publish(&rng_buf, &item2) == 0);
    
    // state: [(1, 2), (6, 7), [3, 4]]
    
    c_item3 = consume(&rng_buf);
    assert(c_item3.ds != NULL);
    assert(((ds_t *)c_item3.ds)->a == 3);
    assert(((ds_t *)c_item3.ds)->b == 4);
    
    // state: [(1, 2), (6, 7), (3, 4)]
    
    c_item2 = consume(&rng_buf);
    assert(c_item2.ds == NULL);
    
    release(&rng_buf, &c_item4);
    release(&rng_buf, &c_item5);
    
    c_item2 = consume(&rng_buf);
    assert(c_item2.ds == NULL);
    
    // state: [[-, -], [-, -], (3, 4)]
    
    assert(publish(&rng_buf, &item4) == 0);
    
    // state: [[-, -], [6, 7], (3, 4)]
    
    assert(publish(&rng_buf, &item3) == 0);
    
    // state: [[5, 6], [6, 7], (3, 4)]
    
    release(&rng_buf, &c_item3);
    
    // state: [[5, 6], [6, 7], [-, -]]
    c_item2 = consume(&rng_buf);
    assert(c_item2.ds != NULL);
    assert(((ds_t *)c_item2.ds)->a == 6);
    assert(((ds_t *)c_item2.ds)->b == 7);
    
    destroy_ring_buf(&rng_buf);
}

void test_overwrite_ordering()
{
    rng_buf_t rng_buf;
    init_ring_buf(&rng_buf, 3, sizeof(ds_t));
    
    ds_t item1;
    item1.a = 1;
    item1.b = 2;
    
    ds_t item2;
    item2.a = 3;
    item2.b = 4;
    
    ds_t item3;
    item3.a = 5;
    item3.b = 6;
    
    ds_t item4;
    item4.a = 6;
    item4.b = 7;
    
    assert(publish(&rng_buf, &item1) == 0);
    assert(publish(&rng_buf, &item2) == 0);
    assert(publish(&rng_buf, &item3) == 0);
    assert(publish(&rng_buf, &item4) == 0);
    
    entry_t c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 3);
    assert(((ds_t *)c_item.ds)->b == 4);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 5);
    assert(((ds_t *)c_item.ds)->b == 6);
    release(&rng_buf, &c_item);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 6);
    assert(((ds_t *)c_item.ds)->b == 7);
    release(&rng_buf, &c_item);
    
    assert(publish(&rng_buf, &item1) == 0);
    
    c_item = consume(&rng_buf);
    assert(c_item.ds != NULL);
    assert(((ds_t *)c_item.ds)->a == 1);
    assert(((ds_t *)c_item.ds)->b == 2);
    release(&rng_buf, &c_item);
    
    destroy_ring_buf(&rng_buf);
}

int main()
{
    test_basic();
    test_2();
    test_3();
    test_overwrite_ordering();
    
    return 0;
}