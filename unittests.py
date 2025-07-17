import os
import subprocess
import numpy as np
from viterbi import Viterbi

polynomes = [0o133, 0o171] 
baseline_viterbi = Viterbi(7, polynomes)
exe = os.path.join(os.getcwd(), "build", "viterbi_decoder")

def build():
    cmd = "mkdir -p build && cd build && cmake .. && make"
    subprocess.run(cmd, shell=True)

def run(argv):
    cmd = f"{exe} {' '.join(argv)}"
    ret = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return ret.stdout.strip()
    
def main():
    bits_arr = np.random.randint(0,2, 50)
    bits = "".join(map(str, bits_arr))
    
    polys = [bin(p)[2:] for p in polynomes]
    
    z = baseline_viterbi.encode(bits_arr)
    x = baseline_viterbi.decode(z)
    
    print("".join(map(str,z)))
    print(run(["-enc"] + polys + [bits]))

    print(" ")
    
    print("".join(map(str,x)))
    print(run(["-dec"] + polys + [bits]))
    
if __name__ == "__main__":
    build()
    main()