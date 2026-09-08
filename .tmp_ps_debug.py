#!/usr/bin/env python3
# Reproduce tvpgl_simd_compare RNG stream exactly, then compute scalar-functor
# (packed) vs per-channel ideal for each CI-reported mismatch.

MASK = 0xFFFFFFFF
kLen = 2048; kRounds = 8; kOpas = [1, 64, 128, 191, 254, 255]

class Rng:
    def __init__(self):
        self.g = 0x9E3779B9
    def nxt(self):
        g = self.g & MASK
        g ^= (g << 13) & MASK
        g ^= g >> 17
        g ^= (g << 5) & MASK
        self.g = g & MASK
        return g

def ps_alpha_packed(d, s, a):
    d1 = d & 0x00ff00ff; d2 = d & 0x0000ff00
    s1 = s & 0x00ff00ff; s2 = s & 0x0000ff00
    x = ((((s1 - d1) & MASK) * a) & MASK)
    y = ((((s2 - d2) & MASK) * a) & MASK)
    return (((x >> 8) + d1) & 0x00ff00ff) | ((((y >> 8) + d2) & 0x0000ff00))

def per_ch(d, s, a):
    out = 0
    for ch in range(3):
        dc = (d >> (ch*8)) & 0xFF; sc = (s >> (ch*8)) & 0xFF
        r = (((sc - dc) * a) >> 8) + dc
        out |= (r & 0xFF) << (ch*8)
    return out

def f_alpha(d, s, a):
    return ps_alpha_packed(d, s, a)

def f_add(d, s, a):
    n = (((d & s) << 1) + ((d ^ s) & 0x00fefefe)) & 0x01010100
    n = ((n >> 8) + 0x007f7f7f) ^ 0x007f7f7f
    s2 = (d + s - n) | n
    return ps_alpha_packed(d, s2, a)

def f_sub(d, s, a):
    s = (~s) & MASK
    n = (((~d & s) << 1) + ((~d ^ s) & 0x00fefefe)) & 0x01010100
    n = ((n >> 8) + 0x007f7f7f) ^ 0x007f7f7f
    s2 = (d | n) - (s | n)
    return ps_alpha_packed(d, s2 & MASK, a)

def f_mul(d, s, a):
    sm = (((((d >> 16) & 0xff) * (s & 0x00ff0000)) & 0xff000000) |
          ((((d >> 8) & 0xff) * (s & 0x0000ff00)) & 0x00ff0000) |
          ((((d >> 0) & 0xff) * (s & 0x000000ff)))) >> 8
    return ps_alpha_packed(d, sm, a)

def f_darken(d, s, a):
    n = (((~d & s) << 1) + ((~d ^ s) & 0x00fefefe)) & 0x01010100
    n = ((n >> 8) + 0x007f7f7f) ^ 0x007f7f7f
    s2 = (d & n) | (s & ~n)
    return ps_alpha_packed(d, s2, a)

def f_lighten(d, s, a):
    n = (((~d & s) << 1) + ((~d ^ s) & 0x00fefefe)) & 0x01010100
    n = ((n >> 8) + 0x007f7f7f) ^ 0x007f7f7f
    s2 = (s & n) | (d & ~n)
    return ps_alpha_packed(d, s2, a)

def f_diff(d, s, a):
    n = (((~d & s) << 1) + ((~d ^ s) & 0x00fefefe)) & 0x01010100
    n = ((n >> 8) + 0x007f7f7f) ^ 0x007f7f7f
    s2 = ((s & n) - (d & n)) | ((d & ~n) - (s & ~n))
    return ps_alpha_packed(d, s2 & MASK, a)

def f_overlay(d, s, a):
    global OVERLAY
    bl = (OVERLAY[(s >> 16) & 0xff][(d >> 16) & 0xff] << 16 |
          OVERLAY[(s >> 8) & 0xff][(d >> 8) & 0xff] << 8 |
          OVERLAY[s & 0xff][d & 0xff])
    return ps_alpha_packed(d, bl, a)

def f_hardlight(d, s, a):
    global OVERLAY
    bl = (OVERLAY[(d >> 16) & 0xff][(s >> 16) & 0xff] << 16 |
          OVERLAY[(d >> 8) & 0xff][(s >> 8) & 0xff] << 8 |
          OVERLAY[d & 0xff][s & 0xff])
    return ps_alpha_packed(d, bl, a)

def f_exclusion(d, s, a):
    sd1 = (((((d >> 16) & 0xff) * ((s & 0x00ff0000) >> 7)) & 0x01ff0000) |
           ((((d >> 0) & 0xff) * (s & 0x000000ff)) >> 7))
    sd2 = (((((d >> 8) & 0xff) * (s & 0x0000ff00)) & 0x00ff8000)) >> 7
    return ((((((s & 0x00ff00ff) - sd1) * a) >> 8) + (d & 0x00ff00ff)) & 0x00ff00ff) | \
           ((((((s & 0x0000ff00) - sd2) * a) >> 8) + (d & 0x0000ff00)) & 0x0000ff00)

OVERLAY = [[0]*256 for _ in range(256)]
for s in range(256):
    for d in range(256):
        sd = (2*s*d + 128) // 256
        if d < 128:
            v = sd
        else:
            v = 2*(s+d) - sd - 255
        OVERLAY[s][d] = v & 0xFF

def run_family(r, is_opa):
    if is_opa:
        for oi in range(len(kOpas)):
            for rr in range(kRounds):
                for i in range(kLen):
                    r.nxt(); r.nxt()
    else:
        for rr in range(kRounds):
            for i in range(kLen):
                r.nxt(); r.nxt()

def get_vectors(r, n=2):
    out = []
    for i in range(n):
        s = r.nxt(); d = r.nxt()
        out.append((s, d))
    return out

fails = [
    ('Alpha', 'plain', None, 0, 1),
    ('Alpha', 'opa', 64, 0, 0),
    ('Sub', 'plain', None, 0, 0),
    ('Sub', 'opa', 64, 0, 0),
    ('Mul', 'plain', None, 0, 0),
    ('Mul', 'opa', 64, 0, 0),
    ('Overlay', 'plain', None, 0, 0),
    ('Overlay', 'opa', 64, 0, 0),
    ('HardLight', 'plain', None, 0, 3),
    ('HardLight', 'opa', 64, 0, 1),
    ('Darken', 'plain', None, 0, 1),
    ('Darken', 'opa', 64, 0, 0),
    ('Diff', 'plain', None, 0, 0),
    ('Diff', 'opa', 64, 0, 0),
    ('Exclusion', 'plain', None, 0, 1),
    ('Exclusion', 'opa', 64, 0, 0),
]

FUNCS = {
    'Alpha': f_alpha, 'Add': f_add, 'Sub': f_sub, 'Mul': f_mul,
    'Overlay': f_overlay, 'HardLight': f_hardlight, 'Darken': f_darken,
    'Lighten': f_lighten, 'Diff': f_diff, 'Exclusion': f_exclusion,
}

ps_fams = ['Alpha','Add','Sub','Mul','Screen','Overlay','HardLight','Lighten','Darken','Diff','Exclusion']

pairs_before_ps = 3 * (2 + 2*6) * kRounds * kLen
r = Rng()
for _ in range(pairs_before_ps):
    r.nxt(); r.nxt()

for fam in ps_fams:
    for (nm, variant, opa, rd, idx) in fails:
        if nm != fam:
            continue
        rr = Rng.__new__(Rng); rr.g = r.g
        if variant == 'plain':
            for rr2 in range(rd):
                for i in range(kLen):
                    rr.nxt(); rr.nxt()
            vec = get_vectors(rr, idx+1)[idx]
        else:
            oi = kOpas.index(opa)
            for oi2 in range(oi):
                for rr2 in range(kRounds):
                    for i in range(kLen):
                        rr.nxt(); rr.nxt()
            for rr2 in range(rd):
                for i in range(kLen):
                    rr.nxt(); rr.nxt()
            vec = get_vectors(rr, idx+1)[idx]
        s, d = vec
        a = s >> 24
        f = FUNCS[nm]
        if variant == 'opa':
            a2 = ((s >> 24) * opa) >> 8
            scalar = f(d, s, a2)
            ideal = per_ch(d, s, a2)
        else:
            a2 = a
            scalar = f(d, s, a2)
            ideal = per_ch(d, s, a2)
        print(f'{nm} {variant} opa={opa} rd={rd} idx={idx}: src={s:08X} dest={d:08X} a={a2}')
        print(f'   scalar={scalar:08X} per-channel-ideal={ideal:08X}  {"MATCH" if scalar==ideal else "<<< DIFFER"}')
    run_family(r, False)
    run_family(r, True)
