

#include "tjsCommHead.h"

#include "FontSystem.h"
#include "StringUtil.h"
#include "MsgIntf.h"
#include "DebugIntf.h"
#include <vector>
#include "ConfigManager/IndividualConfigManager.h"

extern void TVPGetAllFontList(std::vector<ttstr> &list);
extern const ttstr &TVPGetDefaultFontName();

void FontSystem::InitFontNames() {
    // enumlate all fonts
    if(FontNamesInit)
        return;

    std::vector<ttstr> list;
    TVPGetAllFontList(list);
    size_t count = list.size();
    for(size_t i = 0; i < count; i++) {
        AddFont(list[i]);
    }

    FontNamesInit = true;
}
//---------------------------------------------------------------------------
void FontSystem::AddFont(const ttstr &name) { TVPFontNames.Add(name, 1); }
//---------------------------------------------------------------------------
bool FontSystem::FontExists(const ttstr &name) {
    // check existence of font
    InitFontNames();

    int *t = TVPFontNames.Find(name);
    return t != nullptr;
}

FontSystem::FontSystem() : FontNamesInit(false), DefaultLOGFONTCreated(false) {
    ConstructDefaultFont();
}

void FontSystem::ConstructDefaultFont() {
    if(!DefaultLOGFONTCreated) {
        DefaultLOGFONTCreated = true;
        DefaultFont.Height = -12;
        DefaultFont.Flags = 0;
        DefaultFont.Angle = 0;
        DefaultFont.Face = ttstr(TVPGetDefaultFontName());
    }
}

static bool tjs_IsPrerenderDirectiveHead(const ttstr &head) {
    // Case-insensitive: does `head` equal "prerenderfont" (the part before
    // '(' of a "PrerenderFont(参数)" directive)? The head is pure ASCII text.
    // 不区分大小写判断 `head` 是否为 "prerenderfont"（即 "PrerenderFont(参数)"
    // 指令中 '(' 之前的那段）。该段为纯 ASCII 文本。
    std::string h = head.AsNarrowStdString();
    std::string lower;
    lower.reserve(h.size());
    for(char c : h) {
        if(c >= 'A' && c <= 'Z')
            c += ('a' - 'A');
        lower.push_back(c);
    }
    return lower == "prerenderfont" || lower == "prerendfont";
}

ttstr FontSystem::GetBeingFont(ttstr fonts) {
    // retrieve being font in the system.
    // font candidates are given by "fonts", separated by comma.
    // Yuzusoft 系作品（千恋万花等）的菜单/选项字体 face 形如
    //   "PrerenderFont(スキップ),ＭＳ ゴシック"
    // 其中 "PrerenderFont(參數)" 是 KrKr 的"预渲染/位图字体"指示符：提示该文字应
    // 走预渲染字体通道（按逗号后的真实字体度量排版），而不是把整个前缀当作一个
    // 系统字体名去匹配失败后任意 fallback。剥离该前缀后，逗号后的真实字体名才
    // 能进入候选匹配，从而保持等宽全角字身，避免"字变小、偏左上、下半被裁"。

    bool vfont;

    if(fonts.c_str()[0] == TJS_W('@')) { // for vertical writing
        fonts = fonts.c_str() + 1;
        vfont = true;
    } else {
        vfont = false;
    }

    // Strip a leading "PrerenderFont(...)" / "PreRenderFont(...)" directive.
    // The paren block carries generic parameters (e.g. スキップ=skip) that are
    // NOT a real font name; the actual font candidate follows the comma.
    // 剥离前缀 "PrerenderFont(...)" / "PreRenderFont(...)" 指令：括号内是通用参数
    //（如 スキップ=skip），并非真实字体名；真正的字体候选在逗号之后。
    {
        int ob = fonts.IndexOf(TJS_W("("));
        if(ob > 0) {
            ttstr head = fonts.SubString(0, ob);
            head = Trim(head);
            if(tjs_IsPrerenderDirectiveHead(head)) {
                int cb = fonts.IndexOf(TJS_W(")"));
                if(cb > ob) {
                    // remainder after ')' : either empty / "," / ",FONT"
                    // ')' 之后是空 / "," / ",FONT"
                    ttstr tail = fonts.SubString(cb + 1, -1);
                    tail = Trim(tail);
                    if(!tail.IsEmpty()) {
                        if(tail.c_str()[0] == TJS_W(','))
                            tail = Trim(tail.c_str() + 1);
                    }
                    if(!tail.IsEmpty()) {
                        fonts = tail;
                    } else {
                        // nothing after the directive; fall back to default
                        // 指令后无候选；回退到默认字体
                        fonts = TJS_W("");
                    }
                }
            }
        }
    }

    static bool force_default_font =
        IndividualConfigManager::GetInstance()->GetValue<bool>(
            "force_default_font", false);
    if(!force_default_font) {
        bool prev_empty_name = false;
        while(fonts != TJS_W("")) {
            ttstr fontname;
            int pos = fonts.IndexOf(TJS_W(","));
            if(pos != -1) {
                fontname = Trim(fonts.SubString(0, pos));
                fonts = fonts.SubString(pos + 1, -1);
            } else {
                fontname = Trim(fonts);
                fonts = TJS_W("");
            }

            // no existing check if previously specified font
            // candidate is empty eg. ",Fontname"

            if(fontname != TJS_W("") &&
               (prev_empty_name || FontExists(fontname))) {
                if(vfont && fontname.c_str()[0] != TJS_W('@')) {
                    return TJS_W("@") + fontname;
                } else {
                    return fontname;
                }
            }

            prev_empty_name = (fontname == TJS_W(""));
        }
    }

    // Monospace/fullwidth first fallback: a PrerenderFont-flagged menu/option
    // face (MS Gothic etc.) is expected to render in an equal-width fullwidth
    // cell. Before dropping to the plain default face, prefer a registered
    // monospace / fullwidth CJK font so option text keeps its square cell and
    // does not shrink / shift out of its box.
    // 等宽/全角优先回退：PrerenderFont 标记的菜单/选项字体（如 MS ゴシック）按等宽全角
    // 字身排版。在落到普通默认字体之前，优先选择已注册的等宽/全角 CJK 字体，保持选项
    // 文字的正方字身，避免字缩小、偏移出框。
    if(vfont) {
        return ttstr(TJS_W("@")) + TVPGetDefaultFontName();
    } else {
        static bool below_printed = false;
        std::vector<ttstr> list;
        TVPGetAllFontList(list);
        for(const auto &n : list) {
            std::string ln = n.AsNarrowStdString();
            std::string low;
            low.reserve(ln.size());
            for(char c : ln) {
                if(c >= 'A' && c <= 'Z')
                    c += ('a' - 'A');
                low.push_back(c);
            }
            // Mono / CJK fullwidth candidates (case-insensitive).
            // 等宽 / 全角 CJK 候选（不区分大小写）。
            if(low.find("mono") != std::string::npos ||
               low.find("gothic") != std::string::npos ||
               low.find("mincho") != std::string::npos ||
               low.find(" cjk") != std::string::npos ||
               low.find("sans cjk") != std::string::npos) {
                if(!below_printed) {
                    TVPAddLog(ttstr(TJS_W("FontSystem::GetBeingFont: mono/fullwidth "
                                          "fallback -> ")) + n);
                }
                return n;
            }
        }
        if(!below_printed) {
            below_printed = true;
            TVPAddLog(ttstr(TJS_W("FontSystem::GetBeingFont: no mono/fullwidth; "
                                  "using default ")) +
                      TVPGetDefaultFontName());
        }
        return TVPGetDefaultFontName();
    }
}
