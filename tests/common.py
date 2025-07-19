import os
import subprocess
import numpy as np
from viterbi import Viterbi
from tqdm import tqdm

exe = os.path.join(os.getcwd(), "build", "viterbi_decoder")

def gen_bits(n, depth):
    bits_arr = np.random.randint(0,2,n)
    bits_arr[-depth:] = 0
    return bits_arr

def build():
    cmd = "mkdir -p build && cd build && cmake .. && make"
    subprocess.run(cmd, shell=True)

def run(argv):
    cmd = f"{exe} {' '.join(argv)}"
    ret = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return ret.stdout.strip()

def _cmd(cmd, polynomes, bits_arr):
    polys = [bin(p)[2:] for p in polynomes]
    bits = "".join(map(str, bits_arr))
    return [f"-{cmd}"] + polys + [bits]

def enccmd(polynomes, bits_arr):
    return _cmd("enc", polynomes, bits_arr)

def deccmd(polynomes, bits_arr):
    return _cmd("dec", polynomes, bits_arr)

def encdec(polynomes, bits_arr):    
    enc = run(enccmd(polynomes, bits_arr))
    dec = run(deccmd(polynomes, "".join(map(str, enc))))
    return list(map(int,enc)), list(map(int,dec))

def binlen(x):
    return len(bin(x))-2