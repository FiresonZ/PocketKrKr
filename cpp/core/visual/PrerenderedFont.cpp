
#include "PrerenderedFont.h"
#include "BinaryStream.h"
#include "MsgIntf.h"
#include "tvpgl.h"

extern tTJSHashTable<ttstr, tTVPPrerenderedFont *> TVPPrerenderedFonts;

tTVPPrerenderedFont::tTVPPrerenderedFont(const ttstr &storage)
//	: LocalStorage(storage)
{
    RefCount = 1;
    Storage = storage;
    AscentValid = false;
    CachedAscent = 0;

    tTJSBinaryStream *stream = TVPCreateBinaryStreamForRead(storage, TJS_W(""));
    if(stream == nullptr) {
        TVPThrowExceptionMessage(TVPCannotOpenStorage, storage);
    }

    Image = nullptr;
    try {
        FileLength = stream->GetSize();
        if(FileLength == 0) {
            TVPThrowExceptionMessage(TVPPrerenderedFontMappingFailed,
                                     (const tjs_char *)TVPFileSizeIsZero);
        }
        Image = new tjs_uint8[(tjs_uint)FileLength];
        if(Image == nullptr) {
            TVPThrowExceptionMessage(
                TVPPrerenderedFontMappingFailed,
                (const tjs_char *)TVPMemoryAllocationError);
        }
        tjs_uint readsize =
            stream->Read(const_cast<tjs_uint8 *>(Image), (tjs_uint)FileLength);
        if(readsize != FileLength) {
            TVPThrowExceptionMessage(TVPPrerenderedFontMappingFailed,
                                     (const tjs_char *)TVPFileReadError);
        }
        delete stream;
        stream = nullptr;

        // check header
        if(memcmp("TVP pre-rendered font\x1a", Image, 22)) {
            TVPThrowExceptionMessage(
                TVPPrerenderedFontMappingFailed,
                (const tjs_char *)TVPInvalidPrerenderedFontFile);
        }

        if(Image[23] != 2) {
            TVPThrowExceptionMessage(
                TVPPrerenderedFontMappingFailed,
                (const tjs_char *)TVPNot16BitUnicodeFontFile);
        }

        Version = Image[22];
        if(Version != 0 && Version != 1) {
            TVPThrowExceptionMessage(TVPPrerenderedFontMappingFailed,
                                     (const tjs_char *)TVPInvalidHeaderVersion);
        }

        // read index offset
        IndexCount = *(const tjs_uint32 *)(Image + 24);
        ChIndex = (const tjs_char *)(Image + *(const tjs_uint32 *)(Image + 28));
        Index =
            (const tTVPPrerenderedCharacterItem *)(Image +
                                                   *(const tjs_uint32 *)(Image +
                                                                         32));
    } catch(...) {

        delete stream;

        delete[] Image;
        throw;
    }
    TVPPrerenderedFonts.Add(storage, this);
}
//---------------------------------------------------------------------------
tTVPPrerenderedFont::~tTVPPrerenderedFont() {

    delete[] Image;

    TVPPrerenderedFonts.Delete(Storage);
}
//---------------------------------------------------------------------------
void tTVPPrerenderedFont::AddRef() { RefCount++; }
//---------------------------------------------------------------------------
void tTVPPrerenderedFont::Release() {
    if(RefCount == 1)
        delete this;
    else
        RefCount--;
}
//---------------------------------------------------------------------------
const tTVPPrerenderedCharacterItem *tTVPPrerenderedFont::Find(tjs_char ch) {
    // search through ChIndex
    tjs_uint s = 0;
    tjs_uint e = IndexCount;
    const tjs_char *chindex = ChIndex;
    while(true) {
        tjs_int d = e - s;
        if(d <= 1) {
            if(chindex[s] == ch)
                return Index + s;
            else
                return nullptr;
        }
        tjs_uint m = s + (d >> 1);
        if(chindex[m] > ch)
            e = m;
        else
            s = m;
    }
}
//---------------------------------------------------------------------------
tjs_int tTVPPrerenderedFont::GetAscent() const {
    // 惰性推导该 .tft 生成时的原始字体 ascent。
    // tTVPCharacterData::OriginY 定义为"与 ascent 位置的垂直(向下为正)偏移"；
    // 显示公式 data->OriginY = -pitem->OriginY + aofsy。当 aofsy 取原始字体
    // ascent 时，一个顶到 ascent 线的满高字形应得到 data->OriginY = 0，因此其
    // 存储的 OriginY 恰等于原始字体 ascent。取全部字形 OriginY 的最大值即为
    // 真实 ascent（满高 CJK 字形通常存在，故 max≈ascent）。
    // Lazy derive the origin-font ascent of this .tft. Because data->OriginY is
    // defined as the offset (positive = down) from the ascent line, a full-height
    // glyph reaching the ascent line evaluates to data->OriginY = 0 only when
    // aofsy equals the origin ascent, hence its stored OriginY equals that ascent.
    // The maximum stored OriginY over all glyphs is therefore the true ascent.
    if(!AscentValid) {
        tjs_int maxAscent = 0;
        for(tjs_uint i = 0; i < IndexCount; ++i) {
            tjs_int y = Index[i].OriginY;
            if(y > maxAscent) maxAscent = y;
        }
        CachedAscent = maxAscent;
        AscentValid = true;
    }
    return CachedAscent;
}
//---------------------------------------------------------------------------
void tTVPPrerenderedFont::Retrieve(const tTVPPrerenderedCharacterItem *item,
                                   tjs_uint8 *buffer, tjs_int bufferpitch) {
    // retrieve font data and store to buffer
    // bufferpitch must be larger then or equal to item->Width
    if(item->Width == 0 || item->Height == 0)
        return;

    const tjs_uint8 *ptr = item->Offset + Image;
    tjs_uint8 *dest = buffer;
    tjs_uint8 *destlim = dest + item->Width * item->Height;

    // expand compressed character bitmap data
    if(Version == 0) {
        // version 0 decompressor
        while(dest < destlim) {
            if(*ptr == 0x41) // running
            {
                ptr++;
                tjs_uint8 last = dest[-1];
                tjs_int len = *ptr;
                ptr++;
                while(len--)
                    *(dest++) = (tjs_uint8)last;
            } else {
                *(dest++) = *(ptr++);
            }
        }
    } else if(Version >= 1) {
        // version 1+ decompressor
        while(dest < destlim) {
            if(*ptr >= 0x41) // running
            {
                tjs_int len = *ptr - 0x40;
                ptr++;
                tjs_uint8 last = dest[-1];
                while(len--)
                    *(dest++) = (tjs_uint8)last;
            } else {
                *(dest++) = *(ptr++);
            }
        }
    }

    TVPUpscale65_255(buffer, item->Width * item->Height);

    // expand to each pitch
    ptr = destlim - item->Width;
    dest = buffer + bufferpitch * item->Height - bufferpitch;
    while(buffer <= dest) {
        if(dest != ptr)
            memmove(dest, ptr, item->Width);
        dest -= bufferpitch;
        ptr -= item->Width;
    }
}
//---------------------------------------------------------------------------
