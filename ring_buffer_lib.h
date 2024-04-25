typedef struct {
  char *data;
  size_t num_bits;
} bitset_t;

typedef struct {
    int entries;
    size_t entry_size;
    // todo: remove these in favor of counting the bits in consumed/populated
    int nr_can_consume;
    int nr_available;
    bitset_t consumed;
    bitset_t populated;
    int consumer_pos;
    int producer_pos;
    char * consumer_ptr;
    char * producer_ptr;
    char * buf;
} rng_buf_t;

typedef struct {
    int consumed_idx;
    void * ds;
} entry_t;

entry_t consume(rng_buf_t * rng_buf);

void release(rng_buf_t * rng_buf, entry_t * entry);

int publish(rng_buf_t * rng_buf, void * ds);
void init_ring_buf(rng_buf_t * rng_buf, int num_entries, size_t entry_size);
void destroy_ring_buf(rng_buf_t * rng_buf);
// for debugging
void debug_rb(rng_buf_t * rng_buf);