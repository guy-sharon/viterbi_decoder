#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <immintrin.h>

/* perf with: 
cc -Ofast -mavx -march=native -g -o viterbi_decoder_prof main.c && sudo perf record ./viterbi_decoder_prof -dec 1000000000000001001 10000000000101011 01011010111101011001010000001101111011010111101100100110010011010010010011100001010011100010111001000001100110011101010000101101101011110101010010110010000101011000011011001011101110000000100001100000111100001101100010101011111110001001101100011011101111011101110001110011111011011110101110111000001010000100010000100000111100110011101111001011100101 && sudo hotspot
*/

// ************************************************** //
// ********************* config ********************* //
// ************************************************** //
#define MAX_POLYNOME_LENGTH                         32
#define MAX_POLYNOMES                               16
#define MAX_METRIC                                  ( UINT_MAX >> 1 )
#define MAX_DECODE_LEN_BITS                         ( 350 )

#define STATIC_ALLOC                                ( 0 )
#if STATIC_ALLOC
    #define MAX_NUM_STATES                          ( 1 << MAX_POLYNOME_LENGTH )
#endif

// *************************************************** //
// ********************* defines ********************* //
// *************************************************** //
#define MAX(a,b)            ( a > b ? a : b )
#define MIN(a,b)            ( a < b ? a : b )

#define NUM_BITS_IN_INT                             ( 8*sizeof(unsigned int) )
//#define NODE_BITS_ARR_NUM_INTS                      ( ((MAX_DECODE_LEN_BITS / NUM_BITS_IN_INT) + 7) & ~7 )
#define NODE_BITS_ARR_NUM_INTS                      ( 1 + MAX_DECODE_LEN_BITS / NUM_BITS_IN_INT )
#define BITSTATE_PAD                                ( ((NODE_BITS_ARR_NUM_INTS + 7) & ~7) - NODE_BITS_ARR_NUM_INTS - 1)
#define NODE_SIZE                                   ( 4*NODE_BITS_ARR_NUM_INTS + 6 )
#define NODE_PAD                                    ( 32 - (NODE_SIZE % 32) )

// **************************************************** //
// ********************* typedefs ********************* //
// **************************************************** //
typedef unsigned int state_t;
typedef unsigned int encbit_t;
typedef unsigned char bit_t;

typedef struct Node
{
    //unsigned int bits[NODE_BITS_ARR_NUM_INTS];
    unsigned int metric;
    state_t state;
    int bits_ind;
    //struct Node* parent;
    unsigned int bit_state_map_ind;
    bit_t new_bit;
    unsigned int bitstate_ind;
    //int pad[NODE_PAD];
} Node;

typedef struct Bitstate
{
    unsigned int bits[NODE_BITS_ARR_NUM_INTS];
    unsigned int num_refs;
    int pad[BITSTATE_PAD];
} Bitstate;

unsigned int *bitstates_free_slots;
unsigned int bitstates_free_slots_cnt = 0;

// ************************************************** //
// ********************* static ********************* //
// ************************************************** //
static const char *program_name = "viterbi_decoder";
static unsigned int polynomes[MAX_POLYNOMES] = { 0 };
static unsigned char polynome_count = 0;
static unsigned int depth = 0;
static unsigned int depth_mask = 0;
static encbit_t encbit;
static unsigned int next_node_ind = 0;

#if STATIC_ALLOC
    static struct Node __attribute__((aligned(32))) nodes[MAX_NUM_STATES + MAX_NUM_STATES*2];
    static unsigned char trellis[MAX_NUM_STATES * 2];
#else
    static struct Node* __attribute__((aligned(32))) nodes;
    struct Bitstate *bitstates;
    static unsigned char *trellis;
#endif


// ************************************************************ //
// ********************* Helper Functions ********************* //
// ************************************************************ //
void print_help() {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help    Show this help message\n");
}


// ************************************************* //
// ********************* utils ********************* //
// ************************************************* //
static void parse_polynomes(int argc, char *argv[])
{
    polynome_count = argc - 3; // Last argument is bits
    if (polynome_count > MAX_POLYNOMES) {
        fprintf(stderr, "Error: Too many polynomes (max %d)\n", MAX_POLYNOMES);
        return;
    }
    for (int i = 0; i < polynome_count; i++) {
        polynomes[i] = 0;
        for (int j = 0; j < strlen(argv[i+2]); j++) {
            polynomes[i] = (polynomes[i] << 1) | (argv[i+2][j] == '1' ? 1 : 0);
        }
        depth = MAX(depth, 8*sizeof(polynomes[0]) - __builtin_clz(polynomes[i]));
    }

    depth_mask = 0;
    for (int i = 0; i < depth; i++)
    {
        depth_mask |= 1 << i;
    }
}

static void aligned_memcpy(unsigned int *dst, const unsigned int *src, size_t count) {
    size_t i;

    // Process 8 unsigned ints (8 × 4 = 32 bytes) at a time
    for (i = 0; i + 8 <= count; i += 8) {
        __m256i val = _mm256_load_si256((__m256i *)&src[i]);  // aligned load
        _mm256_store_si256((__m256i *)&dst[i], val);          // aligned store
    }

    // Copy any remaining elements
    for (; i < count; ++i) {
        dst[i] = src[i];
    }
}

// ************************************************** //
// ********************* Encode ********************* //
// ************************************************** //
__inline static unsigned int _encode_bit(bit_t bit, state_t state)
{
    unsigned int _out = 0;
    for (int j = 0; j < polynome_count; j++) {
        _out |= __builtin_parity(state & polynomes[j]) << j;
    }
    return _out;
}

static state_t encode_bit(bit_t bit, state_t state, encbit_t *encbit)
{
    unsigned int _out = 0;
    state_t next_state = ( (state << 1) | bit ) & depth_mask;
    *encbit = _encode_bit(bit, next_state);
    return next_state;
}

void encode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -enc <polynomes0> <polynomes1> ... <polynomesN-1> <bits>\n", program_name);
        return;
    }
    
    char *bits = argv[argc-1];
    int len = strlen(bits);
    state_t state = 0;
    for (int i = 0; i < len; i++) {
        bit_t bit = bits[i] == '1';
        state = encode_bit(bit, state, &encbit);
        for (int j = 0; j < polynome_count; j++) {
            printf("%d", (bit_t)((encbit & (1<<j)) >> j));
        }
    }
    printf("\n");
}


// ************************************************** //
// ********************* Decode ********************* //
// ************************************************** //
static int hamming_dist(encbit_t in_encbit, encbit_t encbit)
{
    return __builtin_popcount(in_encbit ^ encbit);
}

/*static void append_new_bit(struct Node *pNode)
{
    pNode->bits_ind++;
    unsigned int int_num = pNode->bits_ind / NUM_BITS_IN_INT;
    unsigned int bit_offset = pNode->bits_ind - int_num*NUM_BITS_IN_INT;
    pNode->bits[int_num] |= pNode->new_bit << bit_offset;
}*/

static void unref(unsigned int ind)
{
    if (bitstates[ind].num_refs <= 1)
    {
        bitstates[ind].num_refs = 0;
        bitstates_free_slots[bitstates_free_slots_cnt++] = ind;
    }
    else bitstates[ind].num_refs--;
}

static void copy_node(struct Node *dst, struct Node *src)
{
    dst->metric = src->metric;
    dst->state = src->state;
    dst->new_bit = src->new_bit;
    dst->bits_ind = src->bits_ind;
    dst->bit_state_map_ind = src->bit_state_map_ind;

    dst->bitstate_ind = src->bitstate_ind;
    struct Bitstate *bitstate = &bitstates[dst->bitstate_ind];
    if (bitstate->num_refs > 1)
    {
        bitstates_free_slots_cnt--;
        unsigned int available_bitstate_slot = bitstates_free_slots[bitstates_free_slots_cnt];
        memcpy(&bitstates[available_bitstate_slot].bits,
               &bitstate->bits, sizeof(bitstates[0].bits));
        dst->bitstate_ind = available_bitstate_slot;
        bitstate = &bitstates[available_bitstate_slot];
    }   
    bitstates[src->bitstate_ind].num_refs--;

    unsigned int int_num = src->bits_ind / NUM_BITS_IN_INT;
    unsigned int bit_offset = src->bits_ind - int_num*NUM_BITS_IN_INT;
    bitstate->bits[int_num] |= src->new_bit << bit_offset;
    
    //aligned_memcpy((unsigned int *)&dst->bits, (unsigned int *)&src->bits, 8 * (1 + (int_num + 1) / 8));
}

static void chain_node(struct Node *child, struct Node *parent, bit_t bit, unsigned int ham_dist, state_t state)
{
    child->metric = parent->metric + ham_dist;
    child->state = state;
    child->new_bit = bit;
    child->bitstate_ind = parent->bitstate_ind;
    
    bitstates[child->bitstate_ind].num_refs++;
    
    child->bits_ind = parent->bits_ind + 1;
    child->bit_state_map_ind = parent->bit_state_map_ind;
    //child->parent = parent;
    //unsigned int int_num = child->bits_ind / NUM_BITS_IN_INT;
    //unsigned int bit_offset = child->bits_ind - int_num*NUM_BITS_IN_INT;
    //aligned_memcpy((unsigned int*)&child->bits, (unsigned int*)&parent->bits, 8*(1+(int_num+1)/8));
    //child->bits[int_num] |= bit << bit_offset;
}

static void init_parents(struct Node *parents, int num_states)
{
    parents[0].bits_ind = -1;
    parents[0].bit_state_map_ind = 0;
    parents[0].bitstate_ind = 0;
    for (unsigned int i = 1; i < num_states; i++)
    {
        parents[i].metric = 0; // MAX_METRIC;
        parents[i].state = i;
        parents[i].bits_ind = -1;
        parents[i].bit_state_map_ind = i;
        parents[i].bitstate_ind = i;
    }
}

static void calc_trellis(int num_states)
{
#if !STATIC_ALLOC
    trellis = malloc(num_states * 2 * sizeof(unsigned char));
#endif

    for (state_t state = 0; state < num_states; state++)
    {
        unsigned int state_shift = state << 1;
        for (bit_t bit = 0; bit < 2; bit++)
        {
            (void)encode_bit(bit, state, &encbit);
            trellis[state_shift + bit] = encbit;
        }
    }
}

static void** memalign(size_t aligned, size_t nbytes)
{
    void *ptr;
    int ret = posix_memalign(&ptr, aligned, nbytes);
    if (ret)
    {
        printf("posix_memalign err (%d)... exiting...\n", ret);
        exit(1);
    }
    return ptr;
}

static void init_states_metrics_map(int num_states, unsigned int *state_min_metric, int *state_min_metric_ind)
{
    for (int i = 0; i < num_states; i++)
    {
        state_min_metric[i] = MAX_METRIC;
        state_min_metric_ind[i] = -1;
    }
} 

static void pack_bits(unsigned char *bits, unsigned int num_bits, encbit_t *out)
{
    encbit_t res = 0;
    for (int i = 0; i < num_bits; i++)
    {
        bit_t bit = bits[i] == '1';
        res |= (bit << i);
    }
    *out = res;
}

void decode(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -dec <polynome0> <polynome1> ... <polynomeN-1> <encoded_bits>\n", program_name);
        return;
    }

    // trellis
    int num_states = 1 << depth;
    calc_trellis(num_states);

    //------------------------------------- parents --- children (2 children per parent)
#if STATIC_ALLOC
        unsigned int state_min_metric[MAX_NUM_STATES];
        int state_min_metric_ind[MAX_NUM_STATES];
#else
        nodes                           = (struct Node*)memalign(32, sizeof(struct Node) * (num_states + num_states*2));
        bitstates                       = (struct Bitstate*)memalign(32, sizeof(struct Bitstate) * num_states);
        bitstates_free_slots            = malloc(sizeof(unsigned int)*num_states);
        unsigned int *state_min_metric  = malloc(sizeof(unsigned int) * num_states);
        int *state_min_metric_ind       = malloc(sizeof(int) * num_states);
        unsigned int *bits_state_map    = malloc(sizeof(unsigned int) * num_states * NODE_BITS_ARR_NUM_INTS);
        unsigned char *states_taken     = malloc(sizeof(unsigned char) * num_states);
#endif

    memset(bitstates, 0, sizeof(bitstates));
    memset(nodes, 0, sizeof(nodes));
    memset(bits_state_map, 0, sizeof(bits_state_map));   
    
    // parents and children
    struct Node *parents = &nodes[0];
    struct Node *children = &nodes[num_states];
    init_parents(parents, num_states);
    
    unsigned int bit_ind = 0;
    unsigned char *bits = argv[argc-1];
    int len = strlen(bits);
    encbit_t in_encbit = 0;
    while (bit_ind < len)
    {
        init_states_metrics_map(num_states, state_min_metric, state_min_metric_ind);

        pack_bits(&bits[bit_ind], polynome_count, &in_encbit);
        bit_ind += polynome_count;

        // calculate next column of trellis
        for (unsigned int i = 0; i < num_states; i++)
        {
            struct Node *parent = &parents[i]; 
            unsigned int state_shift = parent->state << 1;
            unsigned char has_replicated = 0;
            for (bit_t bit = 0; bit < 2; bit++)
            {
                unsigned int child_ind = (i<<1) + bit;
                struct Node *child = &children[child_ind];

                state_t next_state = ( state_shift | bit ) & depth_mask;
                encbit = trellis[state_shift + bit];

                unsigned int ham_dist = hamming_dist(in_encbit, encbit);
                if ( (ham_dist + parent->metric) < state_min_metric[next_state] )
                {
                    has_replicated = 1;
                    if (state_min_metric_ind[next_state] != -1)
                    {
                        struct Node *dethroned_child = &children[state_min_metric_ind[next_state]];
                        unref(dethroned_child->bitstate_ind);
                        int q = 3;
                    }
                    chain_node(child, parent, bit, ham_dist, next_state);
                    state_min_metric[next_state] = child->metric;
                    state_min_metric_ind[next_state] = child_ind;
                }
            }
            if (has_replicated == 0)
                unref(parent->bitstate_ind);
        }

        int sum = 0, sum0 = 0;
        for (int i =0 ; i < num_states; i++)
        {
            sum += bitstates[i].num_refs;
            sum0 += bitstates[i].num_refs == 0;
        }
        // eliminate half of the children
        //memset(states_taken, 0, sizeof(states_taken));
        for (unsigned int i = 0; i < num_states; i++)
        {
            struct Node* best_child = &children[state_min_metric_ind[i]];
            copy_node(&parents[i], best_child);
        }
        bit_t q = 0;
    }

    unsigned int min_metric = MAX_METRIC;
    unsigned int min_metric_ind = 0;
    for (unsigned int i = 0; i < num_states; i++)
    {
        if ( parents[i].metric < min_metric )
        {
            min_metric = parents[i].metric;
            min_metric_ind = i;
        }
    }

    struct Node chosen = parents[min_metric_ind];
    struct Bitstate *bitstate = &bitstates[chosen.bitstate_ind];
    //append_new_bit(&chosen);
    for (int i = 0; i < chosen.bits_ind+1; i++) {
        unsigned int int_num = i / NUM_BITS_IN_INT;
        unsigned int bit_offset = i - int_num*NUM_BITS_IN_INT;  
        unsigned int val = bitstate->bits[int_num];
        printf("%d", (bit_t)((val & (1<<bit_offset)) >> bit_offset));
    }
    
    printf("\n");
    unsigned int q = 3;

#if !STATIC_ALLOC
    free(state_min_metric);
    free(state_min_metric_ind);
    free(bits_state_map);
    free(states_taken);
#endif
}

// ************************************************ //
// ********************* Main ********************* //
// ************************************************ //
int main(int argc, char *argv[]) {
    
    assert((MAX_DECODE_LEN_BITS / NUM_BITS_IN_INT) > 0);

    if ( argc == 1 ) {
        print_help();
        return 0;
    }

    parse_polynomes(argc, argv);
    
    if ( strcmp(argv[1], "-enc") == 0 ) {
        encode(argc, argv);
    }
    else if ( strcmp(argv[1], "-dec") == 0 ) {
        decode(argc, argv);
    }
    else if ( strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 ) {
        print_help();
    }
    else {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        print_help();
        return 1;
    }

#if !STATIC_ALLOC
    if (trellis) free(trellis);
    if (nodes) free(nodes);
#endif

    return 0;
}