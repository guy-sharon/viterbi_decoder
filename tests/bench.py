from common import *
import galois
import time
import viterbi


def main():
    num_iters = 100
    num_polys = 2
    poly_len = 15
    n = 350
    
    #
    polys = []
    for i in range(num_polys):
        polys.append(galois.irreducible_poly(2, poly_len-2*i))
    

    depth = max(map(binlen, polys))
    baseline_viterbi = Viterbi(depth, list(map(int,polys)))

    bits_arr = gen_bits(n=n, depth=poly_len)
    enc, dec = encdec(polys, bits_arr)
    _enccmd = enccmd(polys, bits_arr)
    _deccmd = deccmd(polys, enc)
    
    z = baseline_viterbi.encode(bits_arr)

    t0 = time.time()
    baseline_viterbi.decode(z)
    print(time.time() - t0)

    t0 = time.time()
    for _ in tqdm(range(num_iters)):
        run(_enccmd)
    dt = time.time() - t0
    print(f"n={n}, num_polys={num_polys}, poly_len={poly_len}: {round(num_iters/dt,2)} encodes per second")
        
    t0 = time.time()
    for _ in tqdm(range(num_iters)):
        run(_deccmd)
    dt = time.time() - t0
    print(f"n={n}, num_polys={num_polys}, poly_len={poly_len}: {round(num_iters/dt,2)} decodes per second")
    
if __name__ == "__main__":
    build()
    main()