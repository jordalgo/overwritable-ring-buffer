#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "ring_buffer_lib.h"

// TODO: move this to another file
const size_t BITS_PER_BYTE = 8;

void bs_init(bitset_t *bs, size_t n) {
    assert(n > 0);
    unsigned long alloc_num = (n / BITS_PER_BYTE) + 1;
    bs->data = calloc(alloc_num, sizeof(char));
    bs->num_bits = n;
}

bool bs_is_set(bitset_t *bs, size_t idx) {
    assert(idx < bs->num_bits);
    size_t byte_idx = idx / BITS_PER_BYTE;
    size_t bit_pos = idx % BITS_PER_BYTE;
    char num = bs->data[byte_idx];
    return num & (1 << bit_pos) ? true : false;
}

int bs_set(bitset_t *bs, size_t idx){
    assert(idx < bs->num_bits);
    size_t byte_idx = idx / BITS_PER_BYTE;
    size_t bit_pos = idx % BITS_PER_BYTE;
    char num = bs->data[byte_idx];
    bs->data[byte_idx] = num | (1 << bit_pos);
    return 0;
}

int bs_unset(bitset_t *bs, size_t idx){
    assert(idx < bs->num_bits);
    size_t byte_idx = idx / BITS_PER_BYTE;
    size_t bit_pos = idx % BITS_PER_BYTE;
    char num = bs->data[byte_idx];
    bs->data[byte_idx] = num & ~(1 << bit_pos);
    return 0;
}

void advance_consumer(struct rng_buf * rng_buf)
{
    if (rng_buf->consumer_pos == (rng_buf->entries - 1)) {
        rng_buf->consumer_pos = 0;
        rng_buf->consumer_ptr = rng_buf->buf;
    } else {
        ++rng_buf->consumer_pos;
        ++rng_buf->consumer_ptr;
    }
}

void find_next_consumer_pos(struct rng_buf * rng_buf)
{
    if (rng_buf->nr_can_consume == 0) {
        return;
    }
    
    int i = 0;
    while (i < rng_buf->entries) {
        advance_consumer(rng_buf);
        if (bs_is_set(&rng_buf->consumed, rng_buf->consumer_pos)) {
            ++i;
        } else {
            break;
        }
    }
}

void advance_producer(struct rng_buf * rng_buf)
{
    if (rng_buf->producer_pos == (rng_buf->entries - 1)) {
        rng_buf->producer_pos = 0;
        rng_buf->producer_ptr = rng_buf->buf;
    } else {
        ++rng_buf->producer_pos;
        ++rng_buf->producer_ptr;
    }
}

void find_next_producer_pos(struct rng_buf * rng_buf)
{
    if (rng_buf->nr_can_consume == 0) {
        return;
    }
    
    int i = 0;
    while (i < rng_buf->entries) {
        advance_producer(rng_buf);
        if (bs_is_set(&rng_buf->consumed, rng_buf->producer_pos)) {
            ++i;
        } else {
            break;
        }
    }
}

entry_t consume(struct rng_buf * rng_buf)
{
    entry_t ret;
    
    if (!bs_is_set(&rng_buf->populated, rng_buf->consumer_pos)
        || bs_is_set(&rng_buf->consumed, rng_buf->consumer_pos))
    {
        ret.consumed_idx = -1;
        ret.ds = NULL;
        return ret;
    }
    
    bs_set(&rng_buf->consumed, rng_buf->consumer_pos);
    if (rng_buf->nr_can_consume > 0) {
        --rng_buf->nr_can_consume;
    }
    ret.consumed_idx = rng_buf->consumer_pos;
    ret.ds = rng_buf->consumer_ptr;
    find_next_consumer_pos(rng_buf);
    if (bs_is_set(&rng_buf->consumed, rng_buf->producer_pos)) {
        find_next_producer_pos(rng_buf);
    }
    
    return ret;
}

void release(struct rng_buf * rng_buf, entry_t *entry)
{
    bs_unset(&rng_buf->consumed, entry->consumed_idx);
    bs_unset(&rng_buf->populated, entry->consumed_idx);
    ++rng_buf->nr_can_consume;
    ++rng_buf->nr_available;
    
    if (rng_buf->nr_can_consume == 1) {
        find_next_consumer_pos(rng_buf);
        find_next_producer_pos(rng_buf);
    }
}

int publish(struct rng_buf * rng_buf, struct ds * ds)
{
    if (bs_is_set(&rng_buf->consumed, rng_buf->producer_pos)) {
        return -1;
    }
    
    bs_set(&rng_buf->populated, rng_buf->producer_pos);
    memcpy(rng_buf->producer_ptr, ds, sizeof(*ds));
    
    if (rng_buf->nr_available == 0) {
        find_next_consumer_pos(rng_buf);
    }
    find_next_producer_pos(rng_buf);
    
    if (rng_buf->nr_available > 0) {
        --rng_buf->nr_available;
    }
    
    return 0;
}

void init_ring_buf(struct rng_buf * rng_buf, int num_entries)
{
    rng_buf->buf = (struct ds*)malloc(num_entries * sizeof(struct ds));
    rng_buf->entries = num_entries;
    rng_buf->nr_can_consume = num_entries;
    rng_buf->nr_available = num_entries;
    rng_buf->consumer_pos = 0;
    bs_init(&rng_buf->consumed, num_entries);
    bs_init(&rng_buf->populated, num_entries);
    
    rng_buf->consumer_ptr = rng_buf->buf;
    rng_buf->producer_pos = 0;
    rng_buf->producer_ptr = rng_buf->buf;
}

void debug_rb(struct rng_buf * rng_buf) {
    for (int i = 0; i < rng_buf->entries; ++i) {
        printf("entry idx %d. A: %d B: %lu\n", i, rng_buf->buf[i].a, rng_buf->buf[i].b);
    }
    printf("consumer_pos: %d, producer_pos: %d\n", rng_buf->consumer_pos, rng_buf->producer_pos);
    printf("nr_can_consume: %d\n", rng_buf->nr_can_consume);
}