#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POLYNOME_MAX_LENGTH             32
#define MAX_POLYNOMES                   16

static unsigned long long polynomes[MAX_POLYNOMES] = { 0 };

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
// ********************* Encode ********************* //
// ************************************************** //
void encode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -enc <polynomes0> <polynomes1> ... <polynomesN-1> <bits>\n", program_name);
        return;
    }

    // Collect polynomes
    int polynome_count = argc - 3; // Last argument is bits
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
    
    char *bits = argv[argc-1];
    int len = strlen(bits);
    long long state = 0;
    for (int i = 0; i < len; i++) {
        unsigned char bit = bits[i] == '1';
        state = (state << 1) | bit;
        for (int j = 0; j < polynome_count; j++) {
            unsigned char out = __builtin_parity(state & polynomes[j]);
            printf("%d", out);
        }
    }
    printf("\n");
}


// ************************************************** //
// ********************* Decode ********************* //
// ************************************************** //
void decode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -dec <input_file>\n", program_name);
        return;
    }
    
    const char *input_file = argv[2];
    printf("Decoding file: %s\n", input_file);
    // Add decoding logic here
}


// ************************************************ //
// ********************* Main ********************* //
// ************************************************ //
int main(int argc, char *argv[]) {
    
    if ( argc == 1 ) {
        print_help();
        return 0;
    }
    
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
    
    return 0;
}