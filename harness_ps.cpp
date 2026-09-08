// Standalone harness: production scalar PS blender vs candidate SIMD-algorithms.
// Uses REAL functors from blend_functor_c.h wrapped by blend_variation.h (the
// exact production path wired by TVPGL_C_Init). Candidate per-pixel functions
// mirror what the Highway SIMD file computes (u8 cores + u32-packed alpha).
#include <cstdio>
#include <cstdint>
#include "../../workspace/cpp/core/tjs2/tjsTypes.h"
extern "C" {
extern unsigned char TVPDivTable[256 * 256];
extern unsigned char TVPOpacityOnOpacityTable[256 * 256];
extern unsigned char TVPNegativeMulTable[256 * 256];
}
#define TVPPS_USE_OVERLAY_TABLE
#include "../../workspace/cpp/core/visual/gl/blend_functor_c.h"

// define the static member storage used by ps_overlay_blend_func / ps_hard_light_blend_func
unsigned char ps_overlay_table::TABLE[256][256];

static void BuildOverlayTable() {
    for (int s = 0; s < 256; s++)
        for (int d = 0; d < 256; d++)
            ps_overlay_table::TABLE[s][d] = (d < 128)
                ? ((s * d * 2) / 255)
                : (((s + d) * 2) - ((s * d * 2) / 255) - 255);
}

// ---- per-byte u8 helpers (what the Highway u8 vectors compute) ----
static inline uint8_t satadd8(uint8_t a, uint8_t b){ uint16_t t=(uint16_t)a+b; return t>255?255:(uint8_t)t; }
static inline uint8_t satsub8(uint8_t a, uint8_t b){ return (uint8_t)(a<b?0:a-b); }
static inline uint8_t mulshr8(uint8_t a, uint8_t b){ return (uint8_t)(((uint16_t)a*b)>>8); }
static inline uint8_t hb(uint32_t v){ return (uint8_t)((v>>24)&0xff); }
static inline uint8_t rb(uint32_t v){ return (uint8_t)((v>>16)&0xff); }
static inline uint8_t gb(uint32_t v){ return (uint8_t)((v>>8)&0xff); }
static inline uint8_t bb(uint32_t v){ return (uint8_t)(v&0xff); }

// ---- Candidate u32-packed PsApplyAlpha (bit-exact) ----
static inline uint32_t c_ps_alpha(uint32_t d, uint32_t s, uint32_t a) {
    uint32_t d1 = d & 0x00ff00ff, d2 = d & 0x0000ff00;
    return ((( (((s & 0x00ff00ff) - d1) * a) >> 8) + d1) & 0x00ff00ff) |
           ((( (((s & 0x0000ff00) - d2) * a) >> 8) + d2) & 0x0000ff00);
}
static inline uint32_t alpha_n(uint32_t s){ return s >> 24; }
static inline uint32_t alpha_o(uint32_t s, uint32_t opa){ return ((s>>24)*opa)>>8; }
static inline uint32_t hda(uint32_t r, uint32_t d){ return (r & 0x00ffffff) | (d & 0xff000000); }

// ---- Candidate blending cores (produce RGB, alpha ignored) ----
static inline uint32_t core_identity(uint32_t s){ return s; }
static inline uint32_t core_add(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | satadd8(rb(d),rb(s))<<16 | satadd8(gb(d),gb(s))<<8 | satadd8(bb(d),bb(s));
}
static inline uint32_t core_sub(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | satsub8(rb(d),(uint8_t)~rb(s))<<16 | satsub8(gb(d),(uint8_t)~gb(s))<<8 | satsub8(bb(d),(uint8_t)~bb(s));
}
static inline uint32_t core_mul(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | mulshr8(rb(d),rb(s))<<16 | mulshr8(gb(d),gb(s))<<8 | mulshr8(bb(d),bb(s));
}
static inline uint32_t core_lighten(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | (rb(d)>rb(s)?rb(d):rb(s))<<16 | (gb(d)>gb(s)?gb(d):gb(s))<<8 | (bb(d)>bb(s)?bb(d):bb(s));
}
static inline uint32_t core_darken(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | (rb(d)<rb(s)?rb(d):rb(s))<<16 | (gb(d)<gb(s)?gb(d):gb(s))<<8 | (bb(d)<bb(s)?bb(d):bb(s));
}
static inline uint32_t core_diff(uint32_t d, uint32_t s){
    uint8_t a=rb(d),b=rb(s); uint32_t r = a>b? (a-b) : (b-a);
    a=gb(d); b=gb(s); uint32_t g = a>b? (a-b) : (b-a);
    a=bb(d); b=bb(s); uint32_t bl = a>b? (a-b) : (b-a);
    return (uint32_t)0<<24 | (r<<16) | (g<<8) | bl;
}
static inline uint32_t core_overlay(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | ps_overlay_table::TABLE[rb(s)][rb(d)]<<16 |
          ps_overlay_table::TABLE[gb(s)][gb(d)]<<8 | ps_overlay_table::TABLE[bb(s)][bb(d)];
}
static inline uint32_t core_hardlight(uint32_t d, uint32_t s){
    return (uint32_t)0<<24 | ps_overlay_table::TABLE[rb(d)][rb(s)]<<16 |
          ps_overlay_table::TABLE[gb(d)][gb(s)]<<8 | ps_overlay_table::TABLE[bb(d)][bb(s)];
}

// ---- Candidate Screen / Exclusion (scalar-inline packed formulas) ----
static inline uint32_t c_screen(uint32_t d, uint32_t s, uint32_t a){
    uint32_t sd1 = (((((d >> 16) & 0xff) * (s & 0x00ff0000)) & 0xff000000) |
                    ((((d >> 0) & 0xff) * (s & 0x000000ff)))) >> 8;
    uint32_t sd2 = (((((d >> 8) & 0xff) * (s & 0x0000ff00)) & 0x00ff0000)) >> 8;
    return ((((((s & 0x00ff00ff) - sd1) * a) >> 8) + (d & 0x00ff00ff)) & 0x00ff00ff) |
           ((((((s & 0x0000ff00) - sd2) * a) >> 8) + (d & 0x0000ff00)) & 0x0000ff00);
}
static inline uint32_t c_exclusion(uint32_t d, uint32_t s, uint32_t a){
    uint32_t sd1 = (((((d >> 16) & 0xff) * ((s & 0x00ff0000) >> 7)) & 0x01ff0000) |
                    ((((d >> 0) & 0xff) * (s & 0x000000ff)) >> 7));
    uint32_t sd2 = (((((d >> 8) & 0xff) * (s & 0x0000ff00)) & 0x00ff8000)) >> 7;
    return ((((((s & 0x00ff00ff) - sd1) * a) >> 8) + (d & 0x00ff00ff)) & 0x00ff00ff) |
           ((((((s & 0x0000ff00) - sd2) * a) >> 8) + (d & 0x0000ff00)) & 0x0000ff00);
}

// ---- RNG ----
static uint32_t g_rng = 0x9E3779B9u;
static uint32_t NextRng(){ g_rng ^= g_rng<<13; g_rng ^= g_rng>>17; g_rng ^= g_rng<<5; return g_rng; }
static int g_fail = 0;

// Candidate color blend modes: for color-producing cores, apply u32-packed alpha
#define CMP_COLOR(NAME, CORE)                                                   \
    static uint32_t c_##NAME(uint32_t d, uint32_t s, uint32_t a){              \
        return c_ps_alpha(d, CORE(d,s), a);                                    \
    }
CMP_COLOR(c_add, core_add)
CMP_COLOR(c_sub, core_sub)
CMP_COLOR(c_mul, core_mul)
CMP_COLOR(c_overlay, core_overlay)
CMP_COLOR(c_hardlight, core_hardlight)
CMP_COLOR(c_lighten, core_lighten)
CMP_COLOR(c_darken, core_darken)
CMP_COLOR(c_diff, core_diff)

// Compare a mode using functor typedefs (N/O/H/HO) and candidate fn
template <class N, class O, class H, class HO>
static void Cmp(const char *name, uint32_t (*cand)(uint32_t,uint32_t,uint32_t)) {
    for (int it = 0; it < 2000000 && g_fail < 20; ++it) {
        uint32_t d = NextRng(), s = NextRng();
        uint32_t opa = (uint32_t[]){1,64,128,191,254,255}[NextRng()%6];
        uint32_t cn = cand(d,s,alpha_n(s));
        uint32_t co = cand(d,s,alpha_o(s,opa));
        uint32_t rn = N{}(d,s);
        uint32_t ro = O(opa)(d,s);
        uint32_t co2, rn2;
        (void)co2; (void)rn2;
        uint32_t rhd = H{}(d,s);
        uint32_t rho = HO(opa)(d,s);
        uint32_t chd = hda(cand(d,s,alpha_n(s)), d);
        uint32_t cho = hda(cand(d,s,alpha_o(s,opa)), d);
        if (rn!=cn || ro!=co || rhd!=chd || rho!=cho) {
            ++g_fail;
            if (g_fail<=5) std::printf("[MISMATCH] %-10s d=%08X s=%08X opa=%d\n"
               "  ref  n=%08X o=%08X hd=%08X ho=%08X\n"
               "  cand n=%08X o=%08X hd=%08X ho=%08X\n",
               name,d,s,(int)opa, rn,ro,rhd,rho, cn,co,chd,cho);
        }
    }
}

// For color-producing modes, candidate = c_ps_alpha(d, core(d,s), a)

#define RUN(NAME, N, O, H, HO, CAND)                                           \
    Cmp<N,O,H,HO>(NAME, CAND)

int main() {
    BuildOverlayTable();
    RUN("alpha", ps_alpha_blend_functor, ps_alpha_blend_o_functor,
        ps_alpha_blend_HDA_functor, ps_alpha_blend_HDA_o_functor, &c_ps_alpha);
    RUN("add", ps_add_blend_functor, ps_add_blend_o_functor,
        ps_add_blend_HDA_functor, ps_add_blend_HDA_o_functor, &c_c_add);
    RUN("sub", ps_sub_blend_functor, ps_sub_blend_o_functor,
        ps_sub_blend_HDA_functor, ps_sub_blend_HDA_o_functor, &c_c_sub);
    RUN("mul", ps_mul_blend_functor, ps_mul_blend_o_functor,
        ps_mul_blend_HDA_functor, ps_mul_blend_HDA_o_functor, &c_c_mul);
    RUN("screen", ps_screen_blend_functor, ps_screen_blend_o_functor,
        ps_screen_blend_HDA_functor, ps_screen_blend_HDA_o_functor, &c_screen);
    RUN("overlay", ps_overlay_blend_functor, ps_overlay_blend_o_functor,
        ps_overlay_blend_HDA_functor, ps_overlay_blend_HDA_o_functor, &c_c_overlay);
    RUN("hardlight", ps_hard_light_blend_functor, ps_hard_light_blend_o_functor,
        ps_hard_light_blend_HDA_functor, ps_hard_light_blend_HDA_o_functor, &c_c_hardlight);
    RUN("lighten", ps_lighten_blend_functor, ps_lighten_blend_o_functor,
        ps_lighten_blend_HDA_functor, ps_lighten_blend_HDA_o_functor, &c_c_lighten);
    RUN("darken", ps_darken_blend_functor, ps_darken_blend_o_functor,
        ps_darken_blend_HDA_functor, ps_darken_blend_HDA_o_functor, &c_c_darken);
    RUN("diff", ps_diff_blend_functor, ps_diff_blend_o_functor,
        ps_diff_blend_HDA_functor, ps_diff_blend_HDA_o_functor, &c_c_diff);
    RUN("exclusion", ps_exclusion_blend_functor, ps_exclusion_blend_o_functor,
        ps_exclusion_blend_HDA_functor, ps_exclusion_blend_HDA_o_functor, &c_exclusion);
    std::printf("failures=%d\n", g_fail);
    return g_fail ? 1 : 0;
}