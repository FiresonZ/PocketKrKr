/*
 * KrKr2 Engine - Highway SIMD Initialization
 *
 * Overrides the scalar function pointers with Highway SIMD-optimized versions.
 * Called from TVPInitTVPGL() after TVPGL_C_Init().
 */

#include "tvpgl_simd_init.h"
#include "tjsTypes.h"
#include "tvpgl.h"

// Forward declarations of Highway SIMD implementations
extern "C" {

// Phase 1: Copy/Fill
void TVPCopyOpaqueImage_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPFillARGB_hwy(tjs_uint32 *dest, tjs_int len, tjs_uint32 value);

// Phase 2: Alpha Blend (4 variants)
void TVPAlphaBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAlphaBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAlphaBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPAlphaBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Add Blend (4 variants)
void TVPAddBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAddBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAddBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPAddBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Sub Blend (4 variants)
void TVPSubBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPSubBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPSubBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPSubBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Mul Blend (4 variants)
void TVPMulBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPMulBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPMulBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPMulBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Screen Blend (4 variants)
void TVPScreenBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPScreenBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPScreenBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPScreenBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Const Alpha Blend (4 variants)
void TVPConstAlphaBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPConstAlphaBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPConstAlphaBlend_d_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPConstAlphaBlend_a_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: Additive Alpha Blend (6 variants)
void TVPAdditiveAlphaBlend_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAdditiveAlphaBlend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAdditiveAlphaBlend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPAdditiveAlphaBlend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);
void TVPAdditiveAlphaBlend_a_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPAdditiveAlphaBlend_ao_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len, tjs_int opa);

// Phase 2: AlphaColorMat
void TVPAlphaColorMat_hwy(tjs_uint32 *dest, const tjs_uint32 color, tjs_int len);

// Phase 3: Photoshop blend modes (16 types × 4 variants = 64 functions)
// Part 1: Fully SIMD-able (8 modes × 4 variants = 32)
#define DECLARE_PS_BLEND_4V(Name)                                             \
    void TVPPs##Name##Blend_hwy(tjs_uint32 *dest, const tjs_uint32 *src,      \
                                tjs_int len);                                  \
    void TVPPs##Name##Blend_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src,    \
                                  tjs_int len, tjs_int opa);                   \
    void TVPPs##Name##Blend_HDA_hwy(tjs_uint32 *dest, const tjs_uint32 *src,  \
                                    tjs_int len);                              \
    void TVPPs##Name##Blend_HDA_o_hwy(tjs_uint32 *dest, const tjs_uint32 *src,\
                                      tjs_int len, tjs_int opa);

DECLARE_PS_BLEND_4V(Alpha)
DECLARE_PS_BLEND_4V(Add)
DECLARE_PS_BLEND_4V(Sub)
DECLARE_PS_BLEND_4V(Mul)
DECLARE_PS_BLEND_4V(Screen)
DECLARE_PS_BLEND_4V(Lighten)
DECLARE_PS_BLEND_4V(Darken)
DECLARE_PS_BLEND_4V(Diff)

// Part 2: Special handling (8 modes × 4 variants = 32)
DECLARE_PS_BLEND_4V(Overlay)
DECLARE_PS_BLEND_4V(HardLight)
DECLARE_PS_BLEND_4V(Exclusion)
DECLARE_PS_BLEND_4V(SoftLight)
DECLARE_PS_BLEND_4V(ColorDodge)
DECLARE_PS_BLEND_4V(ColorBurn)
DECLARE_PS_BLEND_4V(ColorDodge5)
DECLARE_PS_BLEND_4V(Diff5)

#undef DECLARE_PS_BLEND_4V

// Phase 4: Convert functions
void TVPConvertAdditiveAlphaToAlpha_hwy(tjs_uint32 *buf, tjs_int len);
void TVPConvertAlphaToAdditiveAlpha_hwy(tjs_uint32 *buf, tjs_int len);
void TVPConvert24BitTo32Bit_hwy(tjs_uint32 *dest, const tjs_uint8 *buf, tjs_int len);
void TVPConvert32BitTo24Bit_hwy(tjs_uint8 *dest, const tjs_uint8 *buf, tjs_int len);
void TVPReverseRGB_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);

// Phase 4: Misc functions
void TVPDoGrayScale_hwy(tjs_uint32 *dest, tjs_int len);
void TVPSwapLine32_hwy(tjs_uint32 *line1, tjs_uint32 *line2, tjs_int len);
void TVPSwapLine8_hwy(tjs_uint8 *line1, tjs_uint8 *line2, tjs_int len);
void TVPReverse32_hwy(tjs_uint32 *pixels, tjs_int len);
void TVPReverse8_hwy(tjs_uint8 *pixels, tjs_int len);
void TVPMakeAlphaFromKey_hwy(tjs_uint32 *dest, tjs_int len, tjs_uint32 key);
void TVPCopyMask_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPCopyColor_hwy(tjs_uint32 *dest, const tjs_uint32 *src, tjs_int len);
void TVPFillColor_hwy(tjs_uint32 *dest, tjs_int len, tjs_uint32 color);
void TVPFillMask_hwy(tjs_uint32 *dest, tjs_int len, tjs_uint32 mask);
void TVPBindMaskToMain_hwy(tjs_uint32 *main, const tjs_uint8 *mask, tjs_int len);
void TVPConstColorAlphaBlend_hwy(tjs_uint32 *dest, tjs_int len, tjs_uint32 color, tjs_int opa);
void TVPRemoveConstOpacity_hwy(tjs_uint32 *dest, tjs_int len, tjs_int strength);

// Phase 4: Blur functions
void TVPAddSubVertSum16_hwy(tjs_uint16 *dest, const tjs_uint32 *addline, const tjs_uint32 *subline, tjs_int len);
void TVPAddSubVertSum16_d_hwy(tjs_uint16 *dest, const tjs_uint32 *addline, const tjs_uint32 *subline, tjs_int len);
void TVPAddSubVertSum32_hwy(tjs_uint32 *dest, const tjs_uint32 *addline, const tjs_uint32 *subline, tjs_int len);
void TVPAddSubVertSum32_d_hwy(tjs_uint32 *dest, const tjs_uint32 *addline, const tjs_uint32 *subline, tjs_int len);
void TVPDoBoxBlurAvg16_hwy(tjs_uint32 *dest, tjs_uint16 *sum, const tjs_uint16 *add, const tjs_uint16 *sub, tjs_int n, tjs_int len);
void TVPDoBoxBlurAvg16_d_hwy(tjs_uint32 *dest, tjs_uint16 *sum, const tjs_uint16 *add, const tjs_uint16 *sub, tjs_int n, tjs_int len);
void TVPDoBoxBlurAvg32_hwy(tjs_uint32 *dest, tjs_uint32 *sum, const tjs_uint32 *add, const tjs_uint32 *sub, tjs_int n, tjs_int len);
void TVPDoBoxBlurAvg32_d_hwy(tjs_uint32 *dest, tjs_uint32 *sum, const tjs_uint32 *add, const tjs_uint32 *sub, tjs_int n, tjs_int len);
void TVPChBlurMulCopy65_hwy(tjs_uint8 *dest, const tjs_uint8 *src, tjs_int len, tjs_int level);
void TVPChBlurAddMulCopy65_hwy(tjs_uint8 *dest, const tjs_uint8 *src, tjs_int len, tjs_int level);
void TVPChBlurMulCopy_hwy(tjs_uint8 *dest, const tjs_uint8 *src, tjs_int len, tjs_int level);
void TVPChBlurAddMulCopy_hwy(tjs_uint8 *dest, const tjs_uint8 *src, tjs_int len, tjs_int level);

}  // extern "C"

void TVPGL_SIMD_Init() {
    // =====================================================================
    // Phase 1: Copy/Fill operations
    // =====================================================================
    TVPCopyOpaqueImage = TVPCopyOpaqueImage_hwy;
    TVPFillARGB        = TVPFillARGB_hwy;
    TVPFillARGB_NC     = TVPFillARGB_hwy;

    // =====================================================================
    // Phase 2: Alpha Blend (fixed: unsigned u16 arithmetic)
    // =====================================================================
    TVPAlphaBlend       = TVPAlphaBlend_hwy;
    TVPAlphaBlend_HDA   = TVPAlphaBlend_HDA_hwy;
    TVPAlphaBlend_o     = TVPAlphaBlend_o_hwy;
    TVPAlphaBlend_HDA_o = TVPAlphaBlend_HDA_o_hwy;

    // Phase 2: Add/Sub/Mul/Screen Blend (safe - uses SaturatedAdd/Sub/Mul)
    TVPAddBlend       = TVPAddBlend_hwy;
    TVPAddBlend_HDA   = TVPAddBlend_HDA_hwy;
    TVPAddBlend_o     = TVPAddBlend_o_hwy;
    TVPAddBlend_HDA_o = TVPAddBlend_HDA_o_hwy;

    TVPSubBlend       = TVPSubBlend_hwy;
    TVPSubBlend_HDA   = TVPSubBlend_HDA_hwy;
    // P3 已修：SIMD SubBlend_o 强制 src alpha=0xFF（与标量一致），饱和减后 alpha
    // 保留 dst 原值。out/p34_check/check.c 对拍 30M 全一致。
    TVPSubBlend_o     = TVPSubBlend_o_hwy;
    TVPSubBlend_HDA_o = TVPSubBlend_HDA_o_hwy;

    TVPMulBlend       = TVPMulBlend_hwy;
    TVPMulBlend_HDA   = TVPMulBlend_HDA_hwy;
    TVPMulBlend_o     = TVPMulBlend_o_hwy;
    TVPMulBlend_HDA_o = TVPMulBlend_HDA_o_hwy;

    // P4 已修：SIMD ScreenBlend base 强制 alpha=0xFF（与标量一致）。对拍 30M 全一致。
    TVPScreenBlend       = TVPScreenBlend_hwy;
    TVPScreenBlend_HDA   = TVPScreenBlend_HDA_hwy;
    TVPScreenBlend_o     = TVPScreenBlend_o_hwy;
    TVPScreenBlend_HDA_o = TVPScreenBlend_HDA_o_hwy;

    // Phase 2: Const Alpha Blend (only truly SIMD variants)
    TVPConstAlphaBlend     = TVPConstAlphaBlend_hwy;
    TVPConstAlphaBlend_HDA = TVPConstAlphaBlend_HDA_hwy;
    // NOTE: _d and _a variants are pure scalar with table lookups -
    // keep original C (Duff's device optimized) for better performance
    // TVPConstAlphaBlend_d   = TVPConstAlphaBlend_d_hwy;
    // TVPConstAlphaBlend_a   = TVPConstAlphaBlend_a_hwy;

    // Phase 2: Additive Alpha Blend (truly SIMD variants only)
    TVPAdditiveAlphaBlend       = TVPAdditiveAlphaBlend_hwy;
    TVPAdditiveAlphaBlend_HDA   = TVPAdditiveAlphaBlend_HDA_hwy;
    TVPAdditiveAlphaBlend_o     = TVPAdditiveAlphaBlend_o_hwy;
    TVPAdditiveAlphaBlend_HDA_o = TVPAdditiveAlphaBlend_HDA_o_hwy;
    // NOTE: _a and _ao are pure scalar - keep original C
    // TVPAdditiveAlphaBlend_a     = TVPAdditiveAlphaBlend_a_hwy;
    // TVPAdditiveAlphaBlend_ao    = TVPAdditiveAlphaBlend_ao_hwy;

    // Phase 2: Alpha Color Mat (truly SIMD)
    TVPAlphaColorMat = TVPAlphaColorMat_hwy;

    // =====================================================================
    // Phase 3: Photoshop blend modes
    // Alpha/Add/Sub/Mul/Screen/Lighten/Darken/Diff/Overlay/HardLight/Exclusion
    // 回退到标量，不注册 SIMD（原因见下）。
    // Table-based modes (SoftLight, ColorDodge, ColorBurn, ColorDodge5, Diff5)
    // 也保持纯标量。
    // =====================================================================

    // ---------------------------------------------------------------
    // PS 混合：11 个模式（Alpha/Add/Sub/Mul/Screen/Lighten/Darken/Diff/Overlay/
    // HardLight/Exclusion）回退到标量，不注册 SIMD。
    //
    // 原因（2026-09 诊断）：这些模式的最终步骤 ps_alpha_blend 标量用“32 位打包
    // R/B 通道 + 跨字节借位”的 %2^32 运算（见 blend_functor_c.h 的
    // ps_alpha_blend_func），而本 SIMD 用逐字节 u16 通道 + OrderedDemote2To：
    //   1) OrderedDemote2To 对 16 位中间值“饱和”到 0xFF，标量是 &0xFF 截断；
    //   2) 标量打包算术里 B 通道下溢会借位影响 R 通道，逐字节通道无法复现。
    // 二者结构性不等，tvpgl_simd_compare 逐位比对必然失败（scalar=005E8946
    // vs simd=00FF8946 等 16 处）。
    // 保持与 conventions §9 一致：SIMD 与标量不等 → 回退标量保正确。
    // 待办：若能以 u32 lane 逐像素复现打包算术（harness 已验证该算法位级一致，
    // 见 harness_ps.cpp），再重新注册放回。

    // =====================================================================
    // Phase 4: Convert functions (only truly SIMD ones)
    // =====================================================================
    // NOTE: ConvertAdditiveAlphaToAlpha is pure scalar (table lookup) - keep C
    // TVPConvertAdditiveAlphaToAlpha = TVPConvertAdditiveAlphaToAlpha_hwy;
    TVPConvertAlphaToAdditiveAlpha = TVPConvertAlphaToAdditiveAlpha_hwy;
    // NOTE: 24/32 bit convert are pure scalar - keep C
    // TVPConvert24BitTo32Bit         = TVPConvert24BitTo32Bit_hwy;
    // TVPConvert32BitTo24Bit         = TVPConvert32BitTo24Bit_hwy;
    TVPReverseRGB                  = TVPReverseRGB_hwy;

    // =====================================================================
    // Phase 4: Misc functions (only truly SIMD ones)
    // =====================================================================
    // NOTE: DoGrayScale, Reverse32/8, BindMaskToMain, RemoveConstOpacity
    // are pure scalar loops - keep original C (Duff's device optimized)
    // TVPDoGrayScale         = TVPDoGrayScale_hwy;
    TVPSwapLine32          = TVPSwapLine32_hwy;
    TVPSwapLine8           = TVPSwapLine8_hwy;
    // TVPReverse32           = TVPReverse32_hwy;
    // TVPReverse8            = TVPReverse8_hwy;
    TVPMakeAlphaFromKey    = TVPMakeAlphaFromKey_hwy;
    TVPCopyMask            = TVPCopyMask_hwy;
    TVPCopyColor           = TVPCopyColor_hwy;
    TVPFillColor           = TVPFillColor_hwy;
    TVPFillMask            = TVPFillMask_hwy;
    // TVPBindMaskToMain      = TVPBindMaskToMain_hwy;
    TVPConstColorAlphaBlend = TVPConstColorAlphaBlend_hwy;
    // TVPRemoveConstOpacity  = TVPRemoveConstOpacity_hwy;

    // =====================================================================
    // Phase 4: Blur functions
    // Only AddSubVertSum has true SIMD. DoBoxBlurAvg and ChBlur* are
    // pure scalar - keep original C for better performance.
    // =====================================================================
    TVPAddSubVertSum16     = TVPAddSubVertSum16_hwy;
    TVPAddSubVertSum16_d   = TVPAddSubVertSum16_d_hwy;
    TVPAddSubVertSum32     = TVPAddSubVertSum32_hwy;
    TVPAddSubVertSum32_d   = TVPAddSubVertSum32_d_hwy;
    // NOTE: DoBoxBlurAvg* are sequential scalar - keep original C
    // TVPDoBoxBlurAvg16      = TVPDoBoxBlurAvg16_hwy;
    // TVPDoBoxBlurAvg16_d    = TVPDoBoxBlurAvg16_d_hwy;
    // TVPDoBoxBlurAvg32      = TVPDoBoxBlurAvg32_hwy;
    // TVPDoBoxBlurAvg32_d    = TVPDoBoxBlurAvg32_d_hwy;
    // NOTE: ChBlur* are pure scalar - keep original C
    // TVPChBlurMulCopy65     = TVPChBlurMulCopy65_hwy;
    // TVPChBlurAddMulCopy65  = TVPChBlurAddMulCopy65_hwy;
    // TVPChBlurMulCopy       = TVPChBlurMulCopy_hwy;
    // TVPChBlurAddMulCopy    = TVPChBlurAddMulCopy_hwy;
}
