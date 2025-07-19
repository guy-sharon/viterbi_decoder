import os
import subprocess
import numpy as np
from viterbi import Viterbi
from tqdm import tqdm

def binlen(x):
    return len(bin(x))-2

np.random.seed(0)
polynomes = [0o133, 0o171] 
#polynomes = [0b101, 0b110]

depth = max(map(binlen, polynomes))
baseline_viterbi = Viterbi(depth, polynomes)
exe = os.path.join(os.getcwd(), "build", "viterbi_decoder")

def encdec(bits_arr):
    polys = [bin(p)[2:] for p in polynomes]
    bits = "".join(map(str, bits_arr))
    
    enc = run(["-enc"] + polys + [bits])
    dec = run(["-dec"] + polys + ["".join(map(str, enc))])
    return list(map(int,enc)), list(map(int,dec))

def baseline_encdec(bits_arr):
    enc = baseline_viterbi.encode(bits_arr)
    dec = baseline_viterbi.decode(enc)
    return enc, dec

def build():
    cmd = "mkdir -p build && cd build && cmake .. && make"
    subprocess.run(cmd, shell=True)

def run(argv):
    cmd = f"{exe} {' '.join(argv)}"
    ret = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return ret.stdout.strip()

def gen_bits(n):
    bits_arr = np.random.randint(0,2,n)
    bits_arr[-depth:] = 0
    return bits_arr

def main():
    for _ in tqdm(range(100)):
        bits_arr = gen_bits(n=3000)
        
        bl_enc, bl_dec = baseline_encdec(bits_arr)
        enc, dec = encdec(bits_arr)
        
        assert bl_enc == enc
        assert bl_dec == dec
    
if __name__ == "__main__":
    build()
    main()