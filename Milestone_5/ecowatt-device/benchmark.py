import random, time, base64, json, os
from pathlib import Path

# A Python implementation of the same delta+RLE (simple) compressor for benchmarking
def compress_delta_rle_py(samples):
    # samples: list of (voltage_raw, current_raw)
    out = bytearray()
    out += b"DRL1"
    n = len(samples)
    out += n.to_bytes(2, "big")
    # voltage
    out += samples[0][0].to_bytes(2, "big", signed=True)
    prev = samples[0][0]
    for v, _ in samples[1:]:
        d = v - prev
        prev = v
        if -127 <= d <= 127:
            out.append((d+256) % 256)
        else:
            out.append(0x7F)
            out += (d & 0xFFFF).to_bytes(2, "big", signed=False)
    # current
    out += samples[0][1].to_bytes(2,"big", signed=True)
    prev = samples[0][1]
    for _, i in samples[1:]:
        d = i - prev
        prev = i
        if -127 <= d <= 127:
            out.append((d+256) % 256)
        else:
            out.append(0x7F)
            out += (d & 0xFFFF).to_bytes(2,"big", signed=False)
    return bytes(out)

def decompress_delta_rle_py(data):
    if data[:4]!=b"DRL1": raise RuntimeError("bad magic")
    pos=4
    n = int.from_bytes(data[pos:pos+2],"big"); pos+=2
    voltage0 = int.from_bytes(data[pos:pos+2],"big", signed=True); pos+=2
    volt=[voltage0]
    for _ in range(n-1):
        b=data[pos]; pos+=1
        if b==0x7F:
            val = int.from_bytes(data[pos:pos+2],"big", signed=True); pos+=2
            volt.append(volt[-1]+val)
        else:
            # signed byte
            val = (b if b<128 else b-256)
            volt.append(volt[-1]+val)
    current0 = int.from_bytes(data[pos:pos+2],"big", signed=True); pos+=2
    cur=[current0]
    for _ in range(n-1):
        b=data[pos]; pos+=1
        if b==0x7F:
            val = int.from_bytes(data[pos:pos+2],"big", signed=True); pos+=2
            cur.append(cur[-1]+val)
        else:
            val = (b if b<128 else b-256)
            cur.append(cur[-1]+val)
    samples = list(zip(volt,cur))
    return samples

def gen_samples(N):
    samples=[]
    v=2300; i=50
    for _ in range(N):
        # add small random walk
        v += random.randint(-2,2)
        i += random.randint(-1,1)
        samples.append((v,i))
    return samples

def benchmark(N=180):
    s = gen_samples(N)
    raw_bytes = bytearray()
    for v,i in s:
        raw_bytes += int(v).to_bytes(2,"big", signed=True)
        raw_bytes += int(i).to_bytes(2,"big", signed=True)
    original_size = len(raw_bytes)
    t0 = time.perf_counter()
    comp = compress_delta_rle_py(s)
    t1 = time.perf_counter()
    decomp = decompress_delta_rle_py(comp)
    t2 = time.perf_counter()
    # verify
    ok = decomp == s
    print("Samples:",N)
    print("Original payload size (bytes):", original_size)
    print("Compressed size (bytes):", len(comp))
    print("Compression ratio:", original_size/len(comp) if len(comp)>0 else None)
    print("CPU time compress (ms):", (t1-t0)*1000.0)
    print("CPU time decompress (ms):", (t2-t1)*1000.0)
    print("Lossless ok?", ok)

if __name__=="__main__":
    for n in [16, 32, 64, 180]:
        print("=== N=",n,"===")
        benchmark(n)
        print()
