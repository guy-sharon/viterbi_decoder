from common import *

np.random.seed(0)
polynomes = [0o133, 0o171] 
#polynomes = [0b101, 0b110]

depth = max(map(binlen, polynomes))
baseline_viterbi = Viterbi(depth, polynomes)

def baseline_encdec(bits_arr):
    enc = baseline_viterbi.encode(bits_arr)
    dec = baseline_viterbi.decode(enc)
    return enc, dec

def main():
    bits_arr = gen_bits(n=10, depth=depth)
    bl_enc, bl_dec = baseline_encdec(bits_arr)
    print("".join(map(str,bl_enc)))
    print("".join(map(str,bl_dec)))
    
    for _ in tqdm(range(100)):
        bits_arr = gen_bits(n=350, depth=depth)
        
        bl_enc, bl_dec = baseline_encdec(bits_arr)
        enc, dec = encdec(polynomes, bits_arr)
        

        if bl_enc != enc or bl_dec != dec:
            print("failed")
            exit(0)
    
if __name__ == "__main__":
    build()
    main()