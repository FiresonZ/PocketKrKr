
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

    // 原始字体 ascent 的惰性推导缓存 (derived lazily from the .tft glyph metrics)
    mutable bool AscentValid;
    mutable tjs_int CachedAscent;

public:
    tTVPPrerenderedFont(const ttstr &storage);
    ~tTVPPrerenderedFont();
    void AddRef();
    void Release();

    const tTVPPrerenderedCharacterItem *Find(tjs_char ch); // serch character
    void Retrieve(const tTVPPrerenderedCharacterItem *item, tjs_uint8 *buffer,
                  tjs_int bufferpitch);

    // 返回该 .tft 生成时所依据的**原始字体** ascent（即垂直排版偏移依据）。
    // 移动端回退字体(Noto CJK)的 ascent≈1.16×字号，与 .tft 预烘焙的原始
    // 字体(如 MS ゴシック ≈0.86×字号)不符，会导致字形下移/裁切；此处直接
    // 从 .tft 字形度量推导真实 ascent。
    // Returns the ascent of the ORIGIN font this .tft was baked with, used as the
    // vertical placement reference. Recovered from the glyph metrics themselves so
    // the mobile fallback font (larger ascent) cannot mis-position the glyphs.
    tjs_int GetAscent() const;
};

#endif // __TVP_PRERENDERED_FONT_H__
