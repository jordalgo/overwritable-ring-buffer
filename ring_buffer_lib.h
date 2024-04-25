typedef struct {
  char *data;
  size_t num_bits;
} bitset_t;

struct rng_buf {
    int entries;
    int nr_can_consume;
    int nr_available;
    int consumer_pos;
    bitset_t consumed;
    bitset_t populated;
    struct ds * consumer_ptr;
    int producer_pos;
    struct ds * producer_ptr;
    struct ds * buf;
};

struct ds {
  int a;
  unsigned long b;
};

typedef struct {
    int consumed_idx;
    struct ds * ds;
} entry_t;

entry_t consume(struct rng_buf * rng_buf);

void release(struct rng_buf * rng_buf, entry_t *entry);

int publish(struct rng_buf * rng_buf, struct ds * ds);
void init_ring_buf(struct rng_buf * rng_buf, int num_entries);
// for debugging
void debug_rb(struct rng_buf * rng_buf);