typedef struct {
  char *data;
  size_t num_bits;
} bitset_t;

typedef struct {
    int nr_entries;
    size_t entry_size;
    // consider storing this as metadata on the entry itself
    bitset_t committed;
    char * buf;
    int * queue;
    int consumer_pos;
    int producer_pos;
} rng_buf_t;

typedef struct {
    int memory_idx;
    void * ds;
} entry_t;

entry_t consume(rng_buf_t * rng_buf);
void release(rng_buf_t * rng_buf, entry_t * entry);

entry_t reserve(rng_buf_t * rng_buf);
void commit(rng_buf_t * rng_buf, entry_t * entry);

void init_ring_buf(rng_buf_t * rng_buf, int nr_entries, size_t entry_size);
void destroy_ring_buf(rng_buf_t * rng_buf);
// for debugging
void debug_rb(rng_buf_t * rng_buf);