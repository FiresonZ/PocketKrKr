
#ifndef __TVP_PRERENDERED_FONT_H__
#define __TVP_PRERENDERED_FONT_H__

#include "tjsCommHead.h"
#include "UtilStreams.h"

#pragma pack(push, 1)
struct tTVPPrerenderedCharacterItem {
    tjs_uint32 Offset;
    tjs_uint16 Width;
    tjs_uint16 Height;
    tjs_int16 OriginX;
    tjs_int16 OriginY;
    tjs_int16 IncX;
    tjs_int16 IncY;
    tjs_int16 Inc;
    tjs_uint16 Reserved;
};
#pragma pack(pop)

//---------------------------------------------------------------------------
// tTVPPrerenderedFont
//---------------------------------------------------------------------------
class tTVPPrerenderedFont {
private:
    ttstr Storage;
    // HANDLE FileHandle; // tft file handle
    // HANDLE MappingHandle; // file mapping handle
    // ファイルマッピングではなく、全て最初にデータを読み込んでしまう形にする。
    // BinaryStream で読み込む
    const tjs_uint8 *Image; // tft mapped memory
    tjs_uint64 FileLength;
    tjs_uint RefCount;
    //	tTVPLocalTempStorageHolder LocalStorage;

    tjs_int Version; // data version
    const tjs_char *ChIndex;
    const tTVPPrerenderedCharacterItem *Index;
    tjs_uint IndexCount;
    // Maximum OriginY across this .tpf's glyphs. For a KiriKiri prerendered
    // (bitmap) font this equals the ascent of the source font the .tpf was baked
    // from (e.g. font1_39 -> ~34, font1_32 -> ~28 = MSゴシック ascent). Placing
    // glyphs at this baseline keeps them aligned with the fixed source crop the
    // PreRenderFontEx script applies, instead of the runtime fallback font's
    // ascent (Noto CJK ~1.16x height) which pushes glyphs down and clips them.
    // 该 .tpf 全部字形的 OriginY 最大值，等于生成此 TPF 的源字号字体的 ascent
    //（如 font1_39≈34、font1_32≈28 = MSゴシック ascent）。
    tjs_int MaxOriginY;

public:
    tTVPPrerenderedFont(const ttstr &storage);
    ~tTVPPrerenderedFont();
    void AddRef();
    void Release();

    const tTVPPrerenderedCharacterItem *Find(tjs_char ch); // serch character
    void Retrieve(const tTVPPrerenderedCharacterItem *item, tjs_uint8 *buffer,
                  tjs_int bufferpitch);
    // TPF source-font baseline (max OriginY). Public so glyph placement can use
    // it instead of the runtime fallback font's ascent. See MaxOriginY above.
    // TPF 源字体基线(max OriginY)，供字形摆放使用。见上文 MaxOriginY。
    [[nodiscard]] tjs_int GetMaxOriginY() const { return MaxOriginY; }
};

#endif // __TVP_PRERENDERED_FONT_H__
