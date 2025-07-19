#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

// *************************************************** //
// ********************* defines ********************* //
// *************************************************** //
#define MAX(a,b)            ( a > b ? a : b )
#define MIN(a,b)            ( a < b ? a : b )

#define MAX_POLYNOME_LENGTH             32
#define MAX_POLYNOMES                   16
#define MAX_METRIC                      ( UINT_MAX >> 1 )
#define MAX_DECODE_LEN_BITS             ( 1 << 14 ) // enfore power of 2!
#define NUM_BITS_IN_INT                 ( 8*sizeof(unsigned int) )

// **************************************************** //
// ********************* typedefs ********************* //
// **************************************************** //
typedef unsigned int state_t;
typedef unsigned int encbit_t;
typedef unsigned char bit_t;

typedef struct Node
{
    unsigned int metric;
    state_t state;
    int bits_ind;
    unsigned int bits[MAX_DECODE_LEN_BITS / NUM_BITS_IN_INT];
} Node;


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
static struct Node *nodes;
static unsigned char *trellis;


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

static void chain_node(struct Node *child, struct Node *parent, bit_t bit, unsigned int ham_dist, state_t state)
{
    child->metric = parent->metric + ham_dist;
    child->state = state;
    
    child->bits_ind = parent->bits_ind + 1;
    unsigned int int_num = child->bits_ind / NUM_BITS_IN_INT;
    unsigned int bit_offset = child->bits_ind - int_num*NUM_BITS_IN_INT;
    memcpy(&child->bits, parent->bits, sizeof(child->bits));
    child->bits[int_num] |= bit << bit_offset;
}

void decode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -dec <polynome0> <polynome1> ... <polynomeN-1> <encoded_bits>\n", program_name);
        return;
    }

    // trellis
    int num_states = 1 << depth;

    //------------------------------------- parents --- children (2 children per parent)
    nodes = malloc(sizeof(struct Node) * (num_states + num_states*2));

    trellis = malloc(num_states * 2 * sizeof(unsigned char));
    for (state_t state = 0; state < num_states; state++)
    {
        unsigned int state_shift = state << 1;
        for (bit_t bit = 0; bit < 2; bit++)
        {
            (void)encode_bit(bit, state, &encbit);
            trellis[state_shift + bit] = encbit;
        }
    }
    
    char *bits = argv[argc-1];
    int len = strlen(bits);
    memset(nodes, 0, sizeof(nodes));
    state_t state = 0;


    struct Node *parents = &nodes[0];
    parents[0].bits_ind = -1;
    for (unsigned int i = 1; i < num_states; i++)
    {
        parents[i].metric = 0; // MAX_METRIC;
        parents[i].state = i;
        parents[i].bits_ind = -1;
    }
    struct Node *children = &nodes[num_states];

    unsigned int *state_min_metric = malloc(num_states * sizeof(unsigned int));
    int *state_min_metric_ind      = malloc(num_states * sizeof(int));

    unsigned int bit_ind = 0;
    while (bit_ind < len)
    {
        for (int i = 0 ; i < num_states; i++)
        {
            state_min_metric[i] = MAX_METRIC;
            state_min_metric_ind[i] = -1;
        }

        // pack in-encoded-bits
        encbit_t in_encbit = 0;
        for (int i = 0; i < polynome_count; i++)
        {
            bit_t bit = bits[bit_ind++] == '1';
            in_encbit |= (bit << i);
        }

        // calculate next column of trellis
        for (unsigned int i = 0; i < num_states; i++)
        {
            struct Node *parent = &parents[i]; 
            unsigned int state_shift = parent->state << 1;
            for (bit_t bit = 0; bit < 2; bit++)
            {
                unsigned int child_ind = (i<<1) + bit;
                struct Node *child = &children[child_ind];

                //state_t next_state = encode_bit(bit, parent->state, &encbit);
                state_t next_state = ( (state_shift) | bit ) & depth_mask;
                encbit = trellis[state_shift + bit];

                unsigned int ham_dist = hamming_dist(in_encbit, encbit);
                chain_node(child, parent, bit, ham_dist, next_state);

                if ( child->metric < state_min_metric[next_state] )
                {
                     state_min_metric[next_state] = child->metric;
                     state_min_metric_ind[next_state] = child_ind;
                }
            }
        }

        // eliminate half of the children
        for (unsigned int i = 0; i < num_states; i++)
        {
            parents[i] = children[state_min_metric_ind[i]];
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
    for (int i = 0; i < chosen.bits_ind+1; i++) {
        unsigned int int_num = i / NUM_BITS_IN_INT;
        unsigned int bit_offset = i - int_num*NUM_BITS_IN_INT;  
        unsigned int val = chosen.bits[int_num];
        printf("%d", (bit_t)((val & (1<<bit_offset)) >> bit_offset));
    }
    
    printf("\n");
    unsigned int q = 3;

    free(state_min_metric);
    free(state_min_metric_ind);
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

    if (trellis) free(trellis);
    if (nodes) free(nodes);

    return 0;
}