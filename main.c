#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POLYNOME_LENGTH             32
#define MAX_POLYNOMES                   16

static unsigned long long polynomes[MAX_POLYNOMES] = { 0 };
static unsigned char polynome_count = 0;

static unsigned long long out;

#ifdef DEBUG
    static unsigned char trellis[1 << 10];
#else
    static unsigned char *trellis;
#endif

const char *program_name = "viterbi_decoder";


// ************************************************************ //
// ********************* Helper Functions ********************* //
// ************************************************************ //
void print_help() {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help    Show this help message\n");
}


// ************************************************** //
// ********************* Common ********************* //
// ************************************************** //
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
    }
}

// ************************************************** //
// ********************* Encode ********************* //
// ************************************************** //
static void encode_bit(unsigned char bit, long long *state, unsigned long long *out)
{
    unsigned long long _out = 0;
    *state = (*state << 1) | bit;
    for (int j = 0; j < polynome_count; j++) {
        _out = __builtin_parity(*state & polynomes[j]) << j;
    }
    *out = _out;
}

void encode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -enc <polynomes0> <polynomes1> ... <polynomesN-1> <bits>\n", program_name);
        return;
    }
    
    char *bits = argv[argc-1];
    int len = strlen(bits);
    long long state = 0;
    for (int i = 0; i < len; i++) {
        unsigned char bit = bits[i] == '1';
        encode_bit(bit, &state, &out);
        for (int j = 0; j < polynome_count; j++) {
            printf("%d", (unsigned char)((out & (1<<j)) >> j));
        }
    }
    printf("\n");
}

// ************************************************** //
// ********************* Decode ********************* //
// ************************************************** //
void decode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -dec <polynomes0> <polynomes1> ... <polynomesN-1> <encoded_bits>\n", program_name);
        return;
    }

    // trellis
    int trellis_size = 1 << polynome_count;
#ifndef DEBUG
    trellis = malloc(trellis_size * 2 * sizeof(unsigned char));
#endif
    for (long long state = 0; state < trellis_size; state++)
    {
        long long _state = state;
        for (unsigned char bit = 0; bit < 2; bit++)
        {
            encode_bit(0, &_state, &out);
            trellis[state + (bit << polynome_count)] = out;
        }
    }

    char *bits = argv[argc-1];
    int len = strlen(bits);
    int state = 0;
    for (int i = 0; i < len; i++) {
        unsigned char bit = bits[i] == '1';

    }
}


// ************************************************ //
// ********************* Main ********************* //
// ************************************************ //
int main(int argc, char *argv[]) {
    
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

#ifndef DEBUG
    if (trellis) free(trellis);
#endif
    
    return 0;
}