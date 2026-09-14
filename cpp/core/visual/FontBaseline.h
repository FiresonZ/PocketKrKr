//
// FontBaseline.h
//
// Generic font layout helpers.
//
// These functions are pure pixel geometry: they never look at font names,
// layer names, or game-specific resources. Replacement fonts (e.g. Noto CJK
// used when MS Gothic is missing) can push a glyph's ink above the logical
// line box; a draw that would be clipped at the top is shifted as a whole so
// the shared baseline stays intact and only the one draw moves into its clip.
//
// 通用字体排版辅助。纯像素几何，不依赖字体名/图层名/游戏资源。回退字体（如
// MS ゴシック缺失时用 Noto CJK）可能把字形墨迹推到逻辑行盒之上；此处把整段
// 绘制整体下移进裁剪区，保持共享基线不变，只移动这一次绘制。
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

// Keep the shared line baseline inside the line box, reserving the face's
// descent so glyphs never run below the box.  Passed values are in pixels.
// 把共享行基线钳制进行盒，保留字体的 descent，避免字形落到行盒之下。
inline int TVPComputeLineBaseline(int lineHeight, int ascent, int descent) {
    if(lineHeight <= 0)
        return 0;
    return std::clamp(ascent, 0, std::max(0, lineHeight - std::max(0, descent)));
}

// Glyph origin for a prerendered (.tft) glyph: place the baked bearing on the
// caller's line baseline.  Equivalent to baseline - bearingY.
// 预渲染(.tft)字形的原点：把烘焙的 bearing 放到调用方的行基线上，等于
// baseline - bearingY。
inline int TVPComputeGlyphOriginY(int lineBaseline, int glyphBearingY) {
    return lineBaseline - glyphBearingY;
}

// Fallback faces (missing CJK glyphs, different design metrics) keep their
// bitmaps on the requested face's baseline so adjacent characters do not jump
// vertically.  Positive shifts move the fallback glyph down onto the line.
// 回退字体(缺 CJK 字形、设计度量不同)要把位图放到请求字体的基线上，避免相邻
// 字符上下跳动；正值表示把回退字形下移到行基线。
inline int TVPComputeFallbackBaselineAdjustment(int requestedBaseline,
                                                int fallbackBaseline) {
    return requestedBaseline - fallbackBaseline;
}

// Some scripts draw into a small transparent layer whose clip starts at 0.
// A face with a design ascender taller than the logical line box legitimately
// yields a negative glyph top.  Keep the shared baseline unchanged, but move
// the whole draw down so both the ink and its outline stay inside the clip.
// 部分脚本在小透明层里绘制（clip 顶为 0）。设计 ascender 高于逻辑行盒的
// 字体会产生合法负字形顶；保持共享基线不变，把整段绘制下移，使墨迹与描边
// 都留在裁剪区内。
inline int TVPClampTextOriginToClipTop(int originY, int glyphTop,
                                       int outlineWidth, int clipTop) {
    const int inkTop = originY + glyphTop - std::max(0, outlineWidth);
    return inkTop < clipTop ? originY + (clipTop - inkTop) : originY;
}

// A blurred shadow grows by shadowWidth in every direction and is then moved
// by the requested shadow offset.  Return only the extra space needed above
// the unshadowed glyph; shadows moved far enough down need no extra padding.
// 模糊阴影各方向扩大 shadowWidth，再按偏移移动。只返回未加阴影字形上方需要
// 的额外空间；下移足够的阴影无需额外上边距。
inline int TVPComputeTextShadowTopPadding(int shadowLevel, int shadowWidth,
                                          int shadowOffsetY) {
    if(shadowLevel == 0)
        return 0;
    const std::int64_t width = std::max<std::int64_t>(
        -static_cast<std::int64_t>(shadowWidth),
        static_cast<std::int64_t>(shadowWidth));
    const std::int64_t padding =
        std::max<std::int64_t>(0, width - shadowOffsetY);
    return static_cast<int>(std::min<std::int64_t>(
        padding, std::numeric_limits<int>::max()));
}