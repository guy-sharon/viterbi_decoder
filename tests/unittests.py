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
    for _ in tqdm(range(100)):
        bits_arr = gen_bits(n=1000, depth=depth)
        
        bl_enc, bl_dec = baseline_encdec(bits_arr)
        enc, dec = encdec(polynomes, bits_arr)
        
        assert bl_enc == enc
        assert bl_dec == dec
    
if __name__ == "__main__":
    build()
    main()