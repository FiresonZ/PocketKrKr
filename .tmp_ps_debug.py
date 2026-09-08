#!/usr/bin/env python3
# Reproduce tvpgl_simd_compare RNG and find pixels where scalar != SIMD-wrap.

kLen = 2048
kRounds = 8
kOpas = [1, 64, 128, 191, 254, 255]
kNumOpas = len(kOpas)

def rng():
    g = 0x9E3779B9
    while True:
        g ^= (g << 13) & 0xFFFFFFFF
        g ^= (g >> 17)
        g ^= (g << 5) & 0xFFFFFFFF
        g &= 0xFFFFFFFF
        yield g
gen = rng()

def fill3(src,ref,got):
    for i in range(kLen):
        src[i]=next(gen); ref[i]=next(gen); got[i]=next(gen)

def consume_plain():
    src=[0]*kLen;ref=[0]*kLen;got=[0]*kLen
    for r in range(kRounds): fill3(src,ref,got)
def consume_opa():
    src=[0]*kLen;ref=[0]*kLen;got=[0]*kLen
    for oi in range(kNumOpas):
        for r in range(kRounds): fill3(src,ref,got)

# Non-PS families (12 compares), all pass fully -> consume all RNG
for _ in range(12):
    # order within family: plain, plain, opa, opa, x3 families
    pass
# do them in exact order:
consume_plain(); consume_plain(); consume_opa(); consume_opa()   # Sub
consume_plain(); consume_plain(); consume_opa(); consume_opa()   # Screen
consume_plain(); consume_plain(); consume_opa(); consume_opa()   # AddAlpha

def scalar_ps_alpha(d, s, a):
    d1 = d & 0x00ff00ff; d2 = d & 0x0000ff00
    return (((((s & 0x00ff00ff) - d1) * a) >> 8) + d1) & 0x00ff00ff | \
           ((((((s & 0x0000ff00) - d2) * a) >> 8) + d2) & 0x0000ff00)

def simd_wrap(d, s, a):
    # replicate current SIMD PsApplyAlpha: per-byte u16 promote, Sub(s,d) wraps 16-bit,
    # Mul(diff,a) wraps 16-bit, >>8, +d, truncate low byte; then mask rgb.
    out=0
    for ch in range(4):
        sb=(s>>(8*ch))&0xFF; db=(d>>(8*ch))&0xFF
        diff=(sb-db)&0xFFFF
        r=((diff*a)&0xFFFF)>>8
        r=(r+db)&0xFF
        out|=r<<(8*ch)
    return out & 0x00ffffff

print("scanning PsAlphaBlend all rounds for scalar!=simd_wrap ...")
found=0
src=[0]*kLen;ref=[0]*kLen;got=[0]*kLen
for r in range(kRounds):
    fill3(src,ref,got)
    for i in range(kLen):
        s,d=src[i],ref[i]; a=s>>24
        sc=scalar_ps_alpha(d,s,a); sw=simd_wrap(d,s,a)
        if sc!=sw:
            found+=1
            if found<=5:
                diffable=[(ch, (s>>(8*ch))&0xFF, (d>>(8*ch))&0xFF) for ch in range(3) if ((s>>(8*ch))&0xFF)<((d>>(8*ch))&0xFF)]
                print(f"  round={r} idx={i}: src={s:08X} dest={d:08X} a={a} scalar={sc:08X} simdw={sw:08X} negdiff_chans={diffable}")
print(f"total mismatching pixels: {found}")