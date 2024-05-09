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

void find_next_consumer_pos(rng_buf_t * rng_buf)
{
    int i = 0;
    int memory_idx;
    while (i < rng_buf->nr_entries) {
        if (++rng_buf->consumer_pos >= rng_buf->nr_entries) {
            rng_buf->consumer_pos = 0;
        }
        memory_idx = rng_buf->queue[rng_buf->consumer_pos];
        if (memory_idx != -1
            && bs_is_set(&rng_buf->committed, memory_idx)) {
            return;
        }
        ++i;
    }
    
    // Couldn't find a slot to consume (reset to producer pos)
    rng_buf->consumer_pos = rng_buf->producer_pos;
}

void find_next_producer_pos(rng_buf_t * rng_buf)
{
    int i = 0;
    int memory_idx;
    while (i < rng_buf->nr_entries) {
        if (++rng_buf->producer_pos >= rng_buf->nr_entries) {
            rng_buf->producer_pos = 0;
        }
        if (rng_buf->queue[rng_buf->producer_pos] > -1) {
            break;
        }
        ++i;
    }
}

entry_t consume(rng_buf_t * rng_buf)
{
    entry_t ret = {.memory_idx = -1, .slot = NULL};
    
    int memory_idx = rng_buf->queue[rng_buf->consumer_pos];
    
    if (memory_idx == -1 || !bs_is_set(&rng_buf->committed, memory_idx)) {
        return ret;
    }
    
    ret.memory_idx = memory_idx;
    
    rng_buf->queue[rng_buf->consumer_pos] = -1;
    bs_unset(&rng_buf->committed, memory_idx);
    ret.slot = &rng_buf->buf[ret.memory_idx * rng_buf->entry_size];
    
    find_next_consumer_pos(rng_buf);
        
    return ret;   
}

void release(rng_buf_t * rng_buf, entry_t * entry)
{
    if (entry->memory_idx == -1) {
        printf("Error: entry is already released\n");
        return;
    }
    
    int i = 0;
    for ( ; i < rng_buf->nr_entries; i++) {
        if (rng_buf->queue[i] == -1) {
            rng_buf->queue[i] = entry->memory_idx;
            break;
        }
    }
    // should have found an empty slot
    assert(i < rng_buf->nr_entries);
    entry->memory_idx = -1;
    entry->slot = NULL;
}

entry_t reserve(rng_buf_t * rng_buf)
{
    entry_t ret = {.memory_idx = -1, .slot = NULL};
    
    int i = 0;
    int current_pos = rng_buf->producer_pos;
    int memory_idx;
    
    while (i < rng_buf->nr_entries) {
        memory_idx = rng_buf->queue[current_pos];
        if (memory_idx > -1) {
            rng_buf->queue[current_pos] = -1;
            ret.memory_idx = memory_idx;
            ret.slot = &rng_buf->buf[memory_idx * rng_buf->entry_size];
            if (current_pos == rng_buf->consumer_pos) {
                find_next_consumer_pos(rng_buf);
            }
            return ret;
        }
        if (++current_pos >= rng_buf->nr_entries){
            current_pos = 0;
        }
        
        ++i;
    }
    
    return ret;
}

void commit(rng_buf_t * rng_buf, entry_t * entry)
{
    int current_memory_idx = rng_buf->queue[rng_buf->producer_pos];
    bool overwriting = (current_memory_idx > -1 && bs_is_set(&rng_buf->committed, current_memory_idx));
    rng_buf->queue[rng_buf->producer_pos] = entry->memory_idx;
    
    bs_set(&rng_buf->committed, entry->memory_idx);
    
    if (overwriting && rng_buf->producer_pos == rng_buf->consumer_pos) {
        find_next_consumer_pos(rng_buf);
    }
    
    if (++rng_buf->producer_pos >= rng_buf->nr_entries) {
        rng_buf->producer_pos = 0;
    }

    entry->memory_idx = -1;
    entry->slot = NULL;
}


void init_ring_buf(rng_buf_t * rng_buf, int nr_entries, size_t entry_size)
{
    rng_buf->buf = (void *)malloc(nr_entries * entry_size);
    rng_buf->nr_entries = nr_entries;
    rng_buf->entry_size = entry_size;
    rng_buf->queue = (int *)malloc(nr_entries * sizeof(int));
    rng_buf->consumer_pos = 0;
    rng_buf->producer_pos = 0;
    bs_init(&rng_buf->committed, nr_entries);
    
    int i = 0;
    for (i = 0; i < nr_entries; i++) {
        rng_buf->queue[i] = i;
    }
}

void destroy_ring_buf(rng_buf_t * rng_buf)
{
    free(rng_buf->buf);
    free(rng_buf->queue);
}