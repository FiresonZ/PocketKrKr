//
// Created by LiDon on 2025/9/11.
//

#include <algorithm>
#include <spdlog/spdlog.h>

#include "PSBMedia.h"

#include "PSBFile.h"
#include "resources/ImageMetadata.h"
#include "MsgIntf.h"
#include "Platform.h"
#include "SysInitIntf.h"
#include "UtilStreams.h"
#include "GraphicsLoaderIntf.h"
#include "../motionplayer/ResourceManager.h"

namespace PSB {
#define LOGGER spdlog::get("plugin")

    namespace {
        size_t CalcEntryFootprint(const PSBMedia::CacheEntry &entry) {
            size_t total = entry.resource ? entry.resource->data.size() : 0;
            if(entry.convertedImage) {
                total += entry.convertedImage->size();
            }
            return total;
        }

        bool IsSupportedImageHeader(const std::vector<uint8_t> &data) {
            if(data.size() >= 8 && data[0] == 0x89 && data[1] == 0x50 &&
               data[2] == 0x4e && data[3] == 0x47) {
                return true;
            }
            if(data.size() >= 2 && data[0] == 'B' && data[1] == 'M') {
                return true;
            }
            if(data.size() >= 3 && data[0] == 0xff && data[1] == 0xd8 &&
               data[2] == 0xff) {
                return true;
            }
            if(data.size() >= 3 && data[0] == 'T' && data[1] == 'L' &&
               data[2] == 'G') {
                return true;
            }
            return false;
        }

        uint16_t ReadLE16(const uint8_t *src) {
            return static_cast<uint16_t>(src[0]) |
                static_cast<uint16_t>(src[1] << 8);
        }

        uint16_t ReadBE16(const uint8_t *src) {
            return static_cast<uint16_t>(src[1]) |
                static_cast<uint16_t>(src[0] << 8);
        }

        bool DecompressRLPixel(const std::vector<uint8_t> &input,
                               std::vector<uint8_t> &output,
                               size_t align) {
            if(align == 0 || input.empty() || output.empty()) {
                return false;
            }

            constexpr uint8_t kLookAhead = 1u << 7;
            size_t inPos = 0;
            size_t outPos = 0;

            while(inPos < input.size()) {
                const uint8_t cmdByte = input[inPos++];
                if(cmdByte & kLookAhead) {
                    const size_t count =
                        static_cast<size_t>((cmdByte ^ kLookAhead) + 3);
                    if(inPos + align > input.size()) {
                        return false;
                    }
                    if(outPos + count * align > output.size()) {
                        return false;
                    }
                    for(size_t i = 0; i < count; ++i) {
                        memcpy(output.data() + outPos, input.data() + inPos, align);
                        outPos += align;
                    }
                    inPos += align;
                } else {
                    const size_t count =
                        static_cast<size_t>(cmdByte + 1) * align;
                    if(inPos + count > input.size()) {
                        return false;
                    }
                    if(outPos + count > output.size()) {
                        return false;
                    }
                    memcpy(output.data() + outPos, input.data() + inPos, count);
                    inPos += count;
                    outPos += count;
                }
            }

            return outPos == output.size();
        }

        std::shared_ptr<std::vector<uint8_t>> BuildBmpFromRaw(
            const PSBMedia::CachedImageInfo &info,
            const std::shared_ptr<PSBResource> &resource) {
            if(!resource || info.width <= 0 || info.height <= 0) {
                return nullptr;
            }

            const auto &rawSrc = resource->data;
            const size_t pixelCount =
                static_cast<size_t>(info.width) * static_cast<size_t>(info.height);
            if(pixelCount == 0) {
                return nullptr;
            }

            std::vector<uint8_t> rlDecoded;
            size_t decodedAlign = 0;
            const std::vector<uint8_t> *src = &rawSrc;

            auto inferAlign = [&]() -> size_t {
                const auto typedFormat = Extension::toPSBPixelFormat(info.type, info.spec);
                switch(typedFormat) {
                    case PSBPixelFormat::A8:
                    case PSBPixelFormat::L8:
                    case PSBPixelFormat::A8_SW:
                    case PSBPixelFormat::L8_SW:
                    case PSBPixelFormat::TileA8_SW:
                    case PSBPixelFormat::TileL8_SW:
                    case PSBPixelFormat::CI8:
                    case PSBPixelFormat::CI8_SW:
                    case PSBPixelFormat::CI8_SW_PSP:
                    case PSBPixelFormat::TileCI8:
                        return 1;
                    case PSBPixelFormat::A8L8:
                    case PSBPixelFormat::A8L8_SW:
                    case PSBPixelFormat::TileA8L8_SW:
                    case PSBPixelFormat::LeRGBA4444:
                    case PSBPixelFormat::BeRGBA4444:
                    case PSBPixelFormat::LeRGBA4444_SW:
                    case PSBPixelFormat::TileLeRGBA4444_SW:
                    case PSBPixelFormat::RGBA5650:
                    case PSBPixelFormat::RGBA5650_SW:
                    case PSBPixelFormat::TileRGBA5650_SW:
                    case PSBPixelFormat::CI4:
                    case PSBPixelFormat::CI4_SW:
                    case PSBPixelFormat::CI4_SW_PSP:
                    case PSBPixelFormat::TileCI4:
                    case PSBPixelFormat::RGB5A3:
                        return 2;
                    case PSBPixelFormat::LeRGBA8:
                    case PSBPixelFormat::BeRGBA8:
                    case PSBPixelFormat::LeRGBA8_SW:
                    case PSBPixelFormat::BeRGBA8_SW:
                    case PSBPixelFormat::FlipLeRGBA8_SW:
                    case PSBPixelFormat::FlipBeRGBA8_SW:
                    case PSBPixelFormat::TileLeRGBA8_SW:
                    case PSBPixelFormat::TileBeRGBA8_SW:
                    case PSBPixelFormat::TileBeRGBA8_Rvl:
                        return 4;
                    default:
                        if(!info.palette.empty()) {
                            return 1;
                        }
                        return 0;
                }
            };

            if(info.compress == PSBCompressType::RL) {
                std::vector<size_t> candidateAligns;
                if(const size_t inferred = inferAlign(); inferred != 0) {
                    candidateAligns.push_back(inferred);
                }
                for(size_t align : { static_cast<size_t>(4), static_cast<size_t>(3),
                                     static_cast<size_t>(2), static_cast<size_t>(1) }) {
                    if(std::find(candidateAligns.begin(), candidateAligns.end(),
                                 align) == candidateAligns.end()) {
                        candidateAligns.push_back(align);
                    }
                }
                for(const size_t align : candidateAligns) {
                    std::vector<uint8_t> trial(pixelCount * align);
                    if(!DecompressRLPixel(rawSrc, trial, align)) {
                        continue;
                    }
                    rlDecoded = std::move(trial);
                    src = &rlDecoded;
                    decodedAlign = align;
                    break;
                }
                if(src == &rawSrc) {
                    return nullptr;
                }
            }

            if(LOGGER && (info.debugKey.rfind("main.psb/", 0) == 0 ||
                          info.debugKey.rfind("title.psb/", 0) == 0 ||
                          info.debugKey.rfind("chapter.psb/", 0) == 0 ||
                          info.debugKey.rfind("autoskip.psb/", 0) == 0)) {
                LOGGER->info(
                    "psb build: key={} decodedAlign={} decodedSize={} rawSize={}",
                    info.debugKey, decodedAlign, src->size(), rawSrc.size());
            }

            PSBPixelFormat format =
                Extension::toPSBPixelFormat(info.type.empty() ? "RGBA8" : info.type,
                                            info.spec);
            const bool assumeBGRA =
                info.type.empty() && decodedAlign == 4 && info.palette.empty();
            if(info.type.empty()) {
                if(!info.palette.empty()) {
                    if(src->size() == pixelCount) {
                        format = PSBPixelFormat::CI8;
                    } else if(src->size() * 2 == pixelCount) {
                        format = PSBPixelFormat::CI4;
                    }
                } else if(decodedAlign == 4 || src->size() == pixelCount * 4) {
                    format = PSBPixelFormat::LeRGBA8;
                } else if(decodedAlign == 3 || src->size() == pixelCount * 3) {
                    format = PSBPixelFormat::None;
                } else if(decodedAlign == 2 || src->size() == pixelCount * 2) {
                    format = PSBPixelFormat::RGBA5650;
                } else if(decodedAlign == 1 || src->size() == pixelCount) {
                    format = PSBPixelFormat::L8;
                }
            }

            const auto paletteFormat = Extension::toPSBPixelFormat(
                info.paletteType, info.spec);

            const auto build32Bmp = [&](auto &&pixelWriter) {
                const size_t pitch = static_cast<size_t>(info.width) * 4;
                const size_t headerSize = sizeof(TVP_WIN_BITMAPFILEHEADER) +
                    sizeof(TVP_WIN_BITMAPINFOHEADER);
                const size_t fileSize =
                    headerSize + pitch * static_cast<size_t>(info.height);

                auto out = std::make_shared<std::vector<uint8_t>>(fileSize);
                auto *dst = out->data();

                TVP_WIN_BITMAPFILEHEADER bfh{};
                bfh.bfType = 'B' + ('M' << 8);
                bfh.bfSize = static_cast<tjs_uint32>(fileSize);
                bfh.bfOffBits = static_cast<tjs_uint32>(headerSize);
                memcpy(dst, &bfh, sizeof bfh);

                TVP_WIN_BITMAPINFOHEADER bih{};
                bih.biSize = sizeof(bih);
                bih.biWidth = info.width;
                bih.biHeight = info.height;
                bih.biPlanes = 1;
                bih.biBitCount = 32;
                bih.biCompression = 0;
                memcpy(dst + sizeof(bfh), &bih, sizeof bih);

                auto *pixelDst = dst + headerSize;
                for(int y = 0; y < info.height; ++y) {
                    auto *rowDst = pixelDst +
                        static_cast<size_t>(info.height - 1 - y) * pitch;
                    for(int x = 0; x < info.width; ++x) {
                        pixelWriter(x, y, rowDst + static_cast<size_t>(x) * 4);
                    }
                }
                return out;
            };

            const auto writePalettePixel = [&](uint32_t paletteIndex,
                                               uint8_t *dstPx) {
                if(info.palette.empty()) {
                    dstPx[0] = 0;
                    dstPx[1] = 0;
                    dstPx[2] = 0;
                    dstPx[3] = 0xff;
                    return;
                }

                switch(paletteFormat == PSBPixelFormat::None
                           ? Extension::defaultPalettePixelFormat(info.spec)
                           : paletteFormat) {
                    case PSBPixelFormat::LeRGBA8:
                    case PSBPixelFormat::BeRGBA8:
                    default: {
                        const size_t index = static_cast<size_t>(paletteIndex) * 4;
                        if(index + 3 >= info.palette.size()) {
                            dstPx[0] = 0;
                            dstPx[1] = 0;
                            dstPx[2] = 0;
                            dstPx[3] = 0xff;
                            return;
                        }
                        dstPx[0] = info.palette[index + 2];
                        dstPx[1] = info.palette[index + 1];
                        dstPx[2] = info.palette[index + 0];
                        dstPx[3] = info.palette[index + 3];
                        return;
                    }
                    case PSBPixelFormat::RGB5A3: {
                        const size_t index = static_cast<size_t>(paletteIndex) * 2;
                        if(index + 1 >= info.palette.size()) {
                            dstPx[0] = 0;
                            dstPx[1] = 0;
                            dstPx[2] = 0;
                            dstPx[3] = 0xff;
                            return;
                        }
                        const uint16_t v = ReadBE16(&info.palette[index]);
                        if(v & 0x8000) {
                            const uint8_t r =
                                static_cast<uint8_t>(((v >> 10) & 0x1f) * 255 / 31);
                            const uint8_t g =
                                static_cast<uint8_t>(((v >> 5) & 0x1f) * 255 / 31);
                            const uint8_t b =
                                static_cast<uint8_t>((v & 0x1f) * 255 / 31);
                            dstPx[0] = b;
                            dstPx[1] = g;
                            dstPx[2] = r;
                            dstPx[3] = 0xff;
                        } else {
                            const uint8_t a =
                                static_cast<uint8_t>(((v >> 12) & 0x7) * 255 / 7);
                            const uint8_t r =
                                static_cast<uint8_t>(((v >> 8) & 0xf) * 17);
                            const uint8_t g =
                                static_cast<uint8_t>(((v >> 4) & 0xf) * 17);
                            const uint8_t b =
                                static_cast<uint8_t>((v & 0xf) * 17);
                            dstPx[0] = b;
                            dstPx[1] = g;
                            dstPx[2] = r;
                            dstPx[3] = a;
                        }
                        return;
                    }
                }
            };

            switch(format) {
                case PSBPixelFormat::LeRGBA8:
                case PSBPixelFormat::BeRGBA8:
                case PSBPixelFormat::LeRGBA8_SW:
                case PSBPixelFormat::BeRGBA8_SW:
                case PSBPixelFormat::FlipLeRGBA8_SW:
                case PSBPixelFormat::FlipBeRGBA8_SW:
                case PSBPixelFormat::TileLeRGBA8_SW:
                case PSBPixelFormat::TileBeRGBA8_SW:
                case PSBPixelFormat::TileBeRGBA8_Rvl:
                case PSBPixelFormat::None: {
                    if(src->size() >= pixelCount * 3 && src->size() < pixelCount * 4) {
                        return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                            const size_t index =
                                (static_cast<size_t>(y) * info.width + x) * 3;
                            dstPx[0] = (*src)[index + 2];
                            dstPx[1] = (*src)[index + 1];
                            dstPx[2] = (*src)[index + 0];
                            dstPx[3] = 0xff;
                        });
                    }
                    if(src->size() < pixelCount * 4) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index =
                            (static_cast<size_t>(y) * info.width + x) * 4;
                        if(assumeBGRA) {
                            dstPx[0] = (*src)[index + 0];
                            dstPx[1] = (*src)[index + 1];
                            dstPx[2] = (*src)[index + 2];
                        } else {
                            dstPx[0] = (*src)[index + 2];
                            dstPx[1] = (*src)[index + 1];
                            dstPx[2] = (*src)[index + 0];
                        }
                        dstPx[3] = (*src)[index + 3];
                    });
                }
                case PSBPixelFormat::A8: {
                    if(src->size() < pixelCount) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index = static_cast<size_t>(y) * info.width + x;
                        dstPx[0] = 0xff;
                        dstPx[1] = 0xff;
                        dstPx[2] = 0xff;
                        dstPx[3] = (*src)[index];
                    });
                }
                case PSBPixelFormat::L8:
                case PSBPixelFormat::L8_SW:
                case PSBPixelFormat::TileL8_SW: {
                    if(src->size() < pixelCount) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index = static_cast<size_t>(y) * info.width + x;
                        dstPx[0] = (*src)[index];
                        dstPx[1] = (*src)[index];
                        dstPx[2] = (*src)[index];
                        dstPx[3] = 0xff;
                    });
                }
                case PSBPixelFormat::A8L8:
                case PSBPixelFormat::A8L8_SW:
                case PSBPixelFormat::TileA8L8_SW: {
                    if(src->size() < pixelCount * 2) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index =
                            (static_cast<size_t>(y) * info.width + x) * 2;
                        const uint8_t l = (*src)[index + 0];
                        const uint8_t a = (*src)[index + 1];
                        dstPx[0] = l;
                        dstPx[1] = l;
                        dstPx[2] = l;
                        dstPx[3] = a;
                    });
                }
                case PSBPixelFormat::LeRGBA4444:
                case PSBPixelFormat::BeRGBA4444:
                case PSBPixelFormat::LeRGBA4444_SW:
                case PSBPixelFormat::TileLeRGBA4444_SW: {
                    if(src->size() < pixelCount * 2) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index =
                            (static_cast<size_t>(y) * info.width + x) * 2;
                        const uint16_t v = ReadLE16(&(*src)[index]);
                        const uint8_t r =
                            static_cast<uint8_t>(((v >> 12) & 0x0f) * 17);
                        const uint8_t g =
                            static_cast<uint8_t>(((v >> 8) & 0x0f) * 17);
                        const uint8_t b =
                            static_cast<uint8_t>(((v >> 4) & 0x0f) * 17);
                        const uint8_t a = static_cast<uint8_t>((v & 0x0f) * 17);
                        dstPx[0] = b;
                        dstPx[1] = g;
                        dstPx[2] = r;
                        dstPx[3] = a;
                    });
                }
                case PSBPixelFormat::RGBA5650:
                case PSBPixelFormat::RGBA5650_SW:
                case PSBPixelFormat::TileRGBA5650_SW: {
                    if(src->size() < pixelCount * 2) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index =
                            (static_cast<size_t>(y) * info.width + x) * 2;
                        const uint16_t v = ReadLE16(&(*src)[index]);
                        const uint8_t r =
                            static_cast<uint8_t>(((v >> 11) & 0x1f) * 255 / 31);
                        const uint8_t g =
                            static_cast<uint8_t>(((v >> 5) & 0x3f) * 255 / 63);
                        const uint8_t b =
                            static_cast<uint8_t>((v & 0x1f) * 255 / 31);
                        dstPx[0] = b;
                        dstPx[1] = g;
                        dstPx[2] = r;
                        dstPx[3] = 0xff;
                    });
                }
                case PSBPixelFormat::CI8:
                case PSBPixelFormat::CI8_SW:
                case PSBPixelFormat::CI8_SW_PSP:
                case PSBPixelFormat::TileCI8: {
                    if(src->size() < pixelCount || info.palette.empty()) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t index = static_cast<size_t>(y) * info.width + x;
                        writePalettePixel((*src)[index], dstPx);
                    });
                }
                case PSBPixelFormat::CI4:
                case PSBPixelFormat::CI4_SW:
                case PSBPixelFormat::CI4_SW_PSP:
                case PSBPixelFormat::TileCI4: {
                    if(src->size() * 2 < pixelCount || info.palette.empty()) {
                        return nullptr;
                    }
                    return build32Bmp([&](int x, int y, uint8_t *dstPx) {
                        const size_t pixelIndex =
                            static_cast<size_t>(y) * info.width + x;
                        const uint8_t packed = (*src)[pixelIndex / 2];
                        const uint8_t palIndex =
                            (pixelIndex & 1) == 0 ? static_cast<uint8_t>((packed >> 4) & 0x0f)
                                                  : static_cast<uint8_t>(packed & 0x0f);
                        writePalettePixel(palIndex, dstPx);
                    });
                }
                default:
                    return nullptr;
            }
        }

        size_t ClampSizeT(size_t value, size_t min_value, size_t max_value) {
            return std::max(min_value, std::min(value, max_value));
        }

        float GetPSBFloat(const std::shared_ptr<IPSBValue> &value, float fallback = 0.0f) {
            if(!value) return fallback;
            if(const auto num = std::dynamic_pointer_cast<PSBNumber>(value)) {
                if(num->numberType == PSBNumberType::Float)
                    return num->getFloatValue();
                if(num->numberType == PSBNumberType::Double)
                    return static_cast<float>(BitConverter::fromByteArray<double>(num->data));
                return static_cast<float>(num->getLongValue());
            }
            return fallback;
        }

        int GetPSBInt(const std::shared_ptr<IPSBValue> &value, int fallback = 0) {
            if(!value) return fallback;
            if(const auto num = std::dynamic_pointer_cast<PSBNumber>(value)) {
                if(num->numberType == PSBNumberType::Float)
                    return static_cast<int>(num->getFloatValue());
                if(num->numberType == PSBNumberType::Double)
                    return static_cast<int>(
                        BitConverter::fromByteArray<double>(num->data));
                return static_cast<int>(num->getLongValue());
            }
            return fallback;
        }

        void DumpPSBValue(const std::shared_ptr<spdlog::logger> &logger,
                          const std::string &name,
                          const std::shared_ptr<IPSBValue> &value,
                          int depth, int maxDepth) {
            if(!logger || !value || depth > maxDepth) return;
            std::string indent(depth * 2, ' ');
            if(const auto subDict = std::dynamic_pointer_cast<PSBDictionary>(value)) {
                logger->info("{}{}/ (dict, {} keys)", indent, name, subDict->size());
                if(depth < maxDepth) {
                    for(const auto &[key, child] : *subDict) {
                        DumpPSBValue(logger, key, child, depth + 1, maxDepth);
                    }
                }
            } else if(const auto num = std::dynamic_pointer_cast<PSBNumber>(value)) {
                if(num->numberType == PSBNumberType::Float)
                    logger->info("{}{}= {} (float)", indent, name, num->getFloatValue());
                else
                    logger->info("{}{}= {} (int/long)", indent, name, num->getLongValue());
            } else if(const auto str = std::dynamic_pointer_cast<PSBString>(value)) {
                logger->info("{}{}= \"{}\" (string)", indent, name, str->value);
            } else if(const auto list = std::dynamic_pointer_cast<PSBList>(value)) {
                logger->info("{}{}= [list, {} items]", indent, name, list->size());
                if(depth < maxDepth) {
                    for(int i = 0; i < static_cast<int>(list->size()) && i < 3; i++) {
                        DumpPSBValue(logger, "[" + std::to_string(i) + "]",
                                     (*list)[i], depth + 1, maxDepth);
                    }
                }
            } else {
                logger->info("{}{}= <other>", indent, name);
            }
        }

        std::string MapSrcToResourcePath(const std::string &src) {
            // "src/title/bg" → "source/title/icon/bg"
            if(src.size() > 4 && src.substr(0, 4) == "src/") {
                std::string rest = src.substr(4); // "title/bg"
                auto slashPos = rest.find('/');
                if(slashPos != std::string::npos) {
                    std::string group = rest.substr(0, slashPos);  // "title"
                    std::string name = rest.substr(slashPos + 1);  // "bg"
                    return "source/" + group + "/icon/" + name;
                }
            }
            return src;
        }

        struct LayerFrameInfo {
            std::string src;
            float ox = 0, oy = 0, cx = 0, cy = 0;
        };

        bool ExtractFrameInfo(const std::shared_ptr<PSBDictionary> &layerDict,
                              LayerFrameInfo &out,
                              const std::shared_ptr<spdlog::logger> &logger = nullptr,
                              const std::string &debugLabel = "") {
            auto frameList = std::dynamic_pointer_cast<PSBList>((*layerDict)["frameList"]);
            if(!frameList || frameList->size() == 0) {
                if(logger) logger->info("  ExtractFrameInfo[{}]: no frameList", debugLabel);
                return false;
            }
            auto frame0 = std::dynamic_pointer_cast<PSBDictionary>((*frameList)[0]);
            if(!frame0) {
                if(logger) logger->info("  ExtractFrameInfo[{}]: frame0 not dict", debugLabel);
                return false;
            }
            auto content = std::dynamic_pointer_cast<PSBDictionary>((*frame0)["content"]);
            if(!content) {
                if(logger) {
                    std::string keys;
                    for(const auto &[k, v] : *frame0) {
                        if(!keys.empty()) keys += ", ";
                        keys += k;
                    }
                    logger->info("  ExtractFrameInfo[{}]: no content in frame0 (keys: {})",
                        debugLabel, keys);
                }
                return false;
            }
            auto srcStr = std::dynamic_pointer_cast<PSBString>((*content)["src"]);
            if(!srcStr || srcStr->value.empty()) {
                if(logger) logger->info("  ExtractFrameInfo[{}]: no src string in content", debugLabel);
                return false;
            }

            out.src = srcStr->value;
            out.ox = GetPSBFloat((*content)["ox"], 0);
            out.oy = GetPSBFloat((*content)["oy"], 0);
            auto coordList = std::dynamic_pointer_cast<PSBList>((*content)["coord"]);
            if(coordList && coordList->size() >= 2) {
                out.cx = GetPSBFloat((*coordList)[0], 0);
                out.cy = GetPSBFloat((*coordList)[1], 0);
            }
            return true;
        }

        void CollectLayersFromMotion(
            const std::shared_ptr<PSBDictionary> &motionDict,
            const std::string &motionName,
            const std::string &sceneName,
            float parentX, float parentY,
            const std::shared_ptr<PSBDictionary> &objectTree,
            std::vector<PSBMedia::LayerPosition> &positions,
            std::vector<PSBMedia::ButtonBoundInfo> *buttons,
            const std::shared_ptr<spdlog::logger> &logger,
            int depth = 0) {
            if(depth > 8) return;

            auto targetMotion = std::dynamic_pointer_cast<PSBDictionary>((*motionDict)[motionName]);
            if(!targetMotion) {
                if(logger && depth > 0) logger->info("CollectLayers: {}/{} not found (depth={})",
                    sceneName, motionName, depth);
                return;
            }

            auto layerList = std::dynamic_pointer_cast<PSBList>((*targetMotion)["layer"]);
            if(!layerList) {
                if(logger && depth > 0) logger->info("CollectLayers: {}/{} has no layer list (depth={})",
                    sceneName, motionName, depth);
                return;
            }

            if(logger) {
                logger->info("CollectLayers: {}/{} has {} layers (depth={})",
                    sceneName, motionName, layerList->size(), depth);
            }

            for(int i = 0; i < static_cast<int>(layerList->size()); i++) {
                auto layerDict = std::dynamic_pointer_cast<PSBDictionary>((*layerList)[i]);
                if(!layerDict) continue;

                auto labelVal = std::dynamic_pointer_cast<PSBString>((*layerDict)["label"]);
                std::string label = labelVal ? labelVal->value : ("layer_" + std::to_string(i));

                LayerFrameInfo fi;
                if(!ExtractFrameInfo(layerDict, fi, logger, sceneName + "/" + motionName + "/" + label)) continue;

                float finalX = parentX + fi.ox + fi.cx;
                float finalY = parentY + fi.oy + fi.cy;

                if(fi.src.substr(0, 7) == "motion/") {
                    std::string ref = fi.src.substr(7);
                    auto slash = ref.find('/');
                    if(slash != std::string::npos && objectTree) {
                        std::string objName = ref.substr(0, slash);
                        std::string subMotion = ref.substr(slash + 1);
                        auto objDict = std::dynamic_pointer_cast<PSBDictionary>((*objectTree)[objName]);
                        if(objDict) {
                            auto objMotionDict = std::dynamic_pointer_cast<PSBDictionary>((*objDict)["motion"]);
                            if(objMotionDict) {
                                if(logger) {
                                    logger->info("follow motion ref: {} → {}/{} offset=({},{})",
                                        fi.src, objName, subMotion, finalX, finalY);
                                }
                                size_t posBefore = positions.size();
                                CollectLayersFromMotion(objMotionDict, subMotion, sceneName,
                                    finalX, finalY, objectTree, positions, buttons, logger, depth + 1);

                                if(buttons) {
                                    std::string newImageKey;
                                    if(positions.size() > posBefore) {
                                        newImageKey = positions[posBefore].srcPath;
                                    }
                                    bool found = false;
                                    for(auto &existing : *buttons) {
                                        if(existing.buttonName == objName && existing.sceneName == sceneName) {
                                            if(existing.imageKey.empty() && !newImageKey.empty()) {
                                                existing.imageKey = newImageKey;
                                            }
                                            found = true;
                                            break;
                                        }
                                    }
                                    if(!found) {
                                        PSBMedia::ButtonBoundInfo btn;
                                        btn.sceneName = sceneName;
                                        btn.buttonName = objName;
                                        btn.imageKey = newImageKey;
                                        btn.left = finalX;
                                        btn.top = finalY;
                                        buttons->push_back(std::move(btn));
                                    }
                                }
                            }
                        }
                    }
                } else if(fi.src.substr(0, 4) == "src/") {
                    std::string resourcePath = MapSrcToResourcePath(fi.src);
                    PSBMedia::LayerPosition pos;
                    pos.sceneName = sceneName;
                    pos.layerName = sceneName + "/" + label;
                    pos.srcPath = resourcePath;
                    pos.left = finalX;
                    pos.top = finalY;
                    pos.opacity = 255;
                    pos.visible = true;

                    if(logger) {
                        logger->info("layer: [{}] src={} → res={} final=({},{}) depth={}",
                            pos.layerName, fi.src, resourcePath, finalX, finalY, depth);
                    }

                    positions.push_back(std::move(pos));
                } else {
                    if(logger) {
                        logger->info("  unhandled src type: [{}] src={} pos=({},{}) depth={}",
                            label, fi.src, finalX, finalY, depth);
                    }
                }
            }
        }

        // Extract the full frame time-line for one motion: each layer's frameList
        // becomes a PSBMotionLayerTrack whose frames (sorted by time) drive the
        // animation. Mirrors CollectLayersFromMotion but keeps EVERY frame, not
        // just frame0, so the player can pick the frame active at the current
        // clock — this is what makes logo/title actually animate.
        // 提取单个 motion 的完整帧时间线：每个图层的 frameList 变成一条
        // PSBMotionLayerTrack（帧按 time 排序）。与 CollectLayersFromMotion 同构，
        // 但保留所有帧（而非只取 frame0），播放器可按当前时钟选取生效帧——让
        // logo/标题真正动起来的基础。
        void CollectMotionTracksFromMotion(
            const std::shared_ptr<PSBDictionary> &motionDict,
            const std::string &motionName,
            const std::string &sceneName,
            std::vector<PSBMedia::PSBMotionLayerTrack> &tracks,
            const std::shared_ptr<spdlog::logger> &logger,
            int depth = 0) {
            if(depth > 8) return;
            auto targetMotion = std::dynamic_pointer_cast<PSBDictionary>((*motionDict)[motionName]);
            if(!targetMotion) return;
            auto layerList = std::dynamic_pointer_cast<PSBList>((*targetMotion)["layer"]);
            if(!layerList) return;
            if(logger) logger->debug("CollectTracks: {}/{} {} layers", sceneName, motionName, layerList->size());

            for(int i = 0; i < static_cast<int>(layerList->size()); i++) {
                auto layerDict = std::dynamic_pointer_cast<PSBDictionary>((*layerList)[i]);
                if(!layerDict) continue;
                auto labelVal = std::dynamic_pointer_cast<PSBString>((*layerDict)["label"]);
                std::string label = labelVal ? labelVal->value : ("layer_" + std::to_string(i));

                auto frameList = std::dynamic_pointer_cast<PSBList>((*layerDict)["frameList"]);
                if(!frameList || frameList->size() == 0) continue;

                PSBMedia::PSBMotionLayerTrack track;
                track.label = sceneName + "/" + motionName + "/" + label;
                track.frames.reserve(frameList->size());
                for(int j = 0; j < static_cast<int>(frameList->size()); j++) {
                    auto frame = std::dynamic_pointer_cast<PSBDictionary>((*frameList)[j]);
                    if(!frame) continue;
                    PSBMedia::PSBMotionFrame f;
                    // M2 frameList "time" is a 60fps FRAME count (PSB motion
                    // format); the Player clock (progress delta fed by the game
                    // script) is in MILLISECONDS. Convert once at parse time so
                    // every consumer compares the same unit — without this a
                    // 4-second logo timeline (t=241) finishes in ~241 ms and all
                    // characters pop in at once.
                    // M2 frameList 的 "time" 是 60fps 帧数（PSB motion 格式）；
                    // Player 时钟（脚本传给 progress 的增量）是毫秒。在解析处统一
                    // 换算成毫秒，否则 4 秒的 logo 时间线（t=241）约 241ms 就播完，
                    // 所有角色瞬间同时出现。
                    f.time = static_cast<int>(GetPSBFloat((*frame)["time"], 0) *
                                               1000.0 / 60.0);
                    // PSB frame "type" (0=invisible, 2=static, 3=interpolate) gates
                    // visibility — reference sub_6926B4 parseFrame. A type==0 frame
                    // is the layer's HIDDEN initial state even if it carries a
                    // content/src (yuzulogo white/logo at t=0); drawing it made the
                    // complete static logo appear before the intro started.
                    // PSB 帧 "type"（0=不可见, 2=静态, 3=插值）决定可见性——参考
                    // sub_6926B4 parseFrame。type==0 帧是图层的**隐藏初始态**，即使
                    // 带 content/src（yuzulogo 的 white/logo t=0 帧）；画出它就会在
                    // 片头开始前显示完整静态 logo。
                    f.type = static_cast<int>(GetPSBFloat((*frame)["type"], 0));
                    auto content = std::dynamic_pointer_cast<PSBDictionary>((*frame)["content"]);
                    if(content && f.type != 0) {
                        auto srcVal = std::dynamic_pointer_cast<PSBString>((*content)["src"]);
                        if(srcVal) f.src = srcVal->value;
                        f.ox = GetPSBFloat((*content)["ox"], 0);
                        f.oy = GetPSBFloat((*content)["oy"], 0);
                        auto coord = std::dynamic_pointer_cast<PSBList>((*content)["coord"]);
                        if(coord && coord->size() >= 2) {
                            f.cx = GetPSBFloat((*coord)[0], 0);
                            f.cy = GetPSBFloat((*coord)[1], 0);
                        }
                        // M2 per-frame scale: content "zx"/"zy" (libkrkr2 sub_692AB0
                        // mask 0x60) drive logo backdrops that magnify a tiny source
                        // (yuzulogo's 64x64 white_box → fullscreen). coord[2] 'z' is a
                        // fallback scale (reference also stores coord[2] into slot z).
                        // M2 帧内缩放：content 的 "zx"/"zy"（libkrkr2 sub_692AB0 mask 0x60）
                        // 驱动 logo 背景把 64×64 white_box 放大到全屏；coord[2] 'z' 作为
                        // 兜底缩放（参考也会把 coord[2] 存入 slot z）。
                        {
                            const float zx = GetPSBFloat((*content)["zx"], 0.0f);
                            const float zy = GetPSBFloat((*content)["zy"], 0.0f);
                            if(zx != 0.0f) f.scaleX = zx;
                            if(zy != 0.0f) f.scaleY = zy;
                            if(zx == 0.0f || zy == 0.0f) {
                                if(coord && coord->size() >= 3) {
                                    const float z = GetPSBFloat((*coord)[2], 0.0f);
                                    if(z != 0.0f) {
                                        if(zx == 0.0f) f.scaleX = z;
                                        if(zy == 0.0f) f.scaleY = z;
                                    }
                                }
                            }
                        }
                        // Round 2: blend mode (content "bm") and clipping rect (content
                        // "clip" → [l,t,r,b]). bm maps to the operate blend op; clip is
                        // probed this round (applied once confirmed present on device).
                        // 第二轮：混合模式(content "bm")与裁切矩形(content "clip"→
                        // [l,t,r,b])。bm 映射到 operate 混合算子；clip 本轮先探针，
                        // 真机确认存在后再应用。
                        f.blendMode = static_cast<int>(GetPSBFloat((*content)["bm"], 0));
                        // M2 per-frame rotation (content "angle", degrees). Drives the
                        // yuzusoft logo leaf's wobble/orientation. Reference reads the
                        // same key (sub_692AB0 mask 0x10) into the frame angle.
                        // M2 帧内旋转（content "angle"，单位度）。驱动 yuzusoft logo 叶子
                        // 的摆动/朝向。参考读取同一键（sub_692AB0 mask 0x10）到帧 angle。
                        f.angle = GetPSBFloat((*content)["angle"], 0.0f);
                        // M2 skew (content "sx"/"sy"), transformOrder case-3 operator
                        // [1,sx;sy,1] (libkrkr2 sub_699940). Aligns with AetherKiri
                        // applyLocalTransform — we previously skipped case 3 entirely.
                        // M2 斜切(content "sx"/"sy")，transformOrder case-3 算子
                        // [1,sx;sy,1]（libkrkr2 sub_699940），对齐 AetherKiri
                        // applyLocalTransform——此前我们整段跳过 case 3。
                        f.slantX = GetPSBFloat((*content)["sx"], 0.0f);
                        f.slantY = GetPSBFloat((*content)["sy"], 0.0f);
                        if(auto clipList =
                               std::dynamic_pointer_cast<PSBList>((*content)["clip"])) {
                            if(clipList->size() >= 4) {
                                f.hasClip = true;
                                f.clipL = static_cast<int>(GetPSBFloat((*clipList)[0], 0));
                                f.clipT = static_cast<int>(GetPSBFloat((*clipList)[1], 0));
                                f.clipR = static_cast<int>(GetPSBFloat((*clipList)[2], 0));
                                f.clipB = static_cast<int>(GetPSBFloat((*clipList)[3], 0));
                            }
                        }
                        // M2 flip: content "fx"/"fy" nonzero means mirror the sprite
                        // (e.g. yuzulogo's leaf jitter piece). Same as the flat path.
                        // M2 翻转：content "fx"/"fy" 非零表示镜像该精灵（如 yuzulogo
                        // 叶片摆动件）。
                        f.flipX = GetPSBFloat((*content)["fx"], 0.0f) != 0.0f;
                        f.flipY = GetPSBFloat((*content)["fy"], 0.0f) != 0.0f;
                        // M2 frame opacity is stored under "opa" (0..255), not "op". Reading the
                        // wrong key always yielded the fallback 255, so any layer whose entrance is
                        // driven by opacity (e.g. yuzulogo's `white`/`logo` t=0 frame has `opa:0`
                        // → the complete cyan logo must stay transparent at first) was rendered
                        // fully opaque. Read "opa" first, fall back to "op".
                        // M2 帧透明度字段是 "opa"（0..255），不是 "op"。读错 key 会恒返回兜底 255，
                        // 导致以透明度驱动入场/出场的层被画成完全不透明（如 yuzulogo 的 white/logo
                        // 层 t=0 帧 `opa:0`，完整青色 logo 开首应保持透明）。优先读 "opa"，无则回退 "op"。
                        f.opacity = GetPSBFloat((*content)["opa"], -1.0f);
                        if(f.opacity < 0.0f)
                            f.opacity = GetPSBFloat((*content)["op"], 255.0f);
                    } else {
                        // type==0 (invisible) or a frame without content only marks
                        // a time change: the layer is invisible during this range.
                        // type==0（不可见）或无 content 的帧只是时间标记：该时段内
                        // 图层不可见。
                        f.visible = false;
                    }
                    track.frames.push_back(std::move(f));
                }
                if(track.frames.empty()) continue;
                std::stable_sort(track.frames.begin(), track.frames.end(),
                    [](const PSBMedia::PSBMotionFrame &a, const PSBMedia::PSBMotionFrame &b) {
                        return a.time < b.time;
                    });
                tracks.push_back(std::move(track));
            }
        }

        // Extract the time-line for every scene/motion in the object tree.
        // 对对象树里每个场景/每个 motion 都提取时间线。
        void CollectAllMotionTracks(
            PSBMedia &media,
            const std::string &archiveKey,
            const std::shared_ptr<PSBDictionary> &objectTree,
            const std::shared_ptr<spdlog::logger> &logger) {
            if(!objectTree) return;
            for(const auto &[sceneName, sceneVal] : *objectTree) {
                auto sceneDict = std::dynamic_pointer_cast<PSBDictionary>(sceneVal);
                if(!sceneDict) continue;
                auto motionDict = std::dynamic_pointer_cast<PSBDictionary>((*sceneDict)["motion"]);
                if(!motionDict) continue;
                for(const auto &[motionName, motionVal] : *motionDict) {
                    auto motionDictObj = std::dynamic_pointer_cast<PSBDictionary>(motionVal);
                    if(!motionDictObj) continue;
                    // M2 motion-level metadata: "loopTime" (>0 means the timeline
                    // loops, e.g. logo intros play until the script advances).
                    // Same frames→ms conversion as frameList "time" — both are
                    // 60fps frame counts in the PSB, the Player clock is in ms.
                    // M2 motion 级元数据："loopTime"（>0 表示时间线循环，如 logo 片头
                    // 会一直播到脚本推进）。与 frameList "time" 同样的 帧→毫秒 换算——
                    // PSB 里两者都是 60fps 帧数，Player 时钟是毫秒。
                    const tjs_int loopTime = static_cast<tjs_int>(
                        GetPSBFloat((*motionDictObj)["loopTime"], 0) * 1000.0 / 60.0);
                    if(loopTime > 0) {
                        media.setMotionLoopTime(archiveKey, sceneName, motionName,
                                                loopTime);
                        if(logger) logger->info("motion {}/{} loopTime={}",
                            sceneName, motionName, loopTime);
                    }
                    std::vector<PSBMedia::PSBMotionLayerTrack> tracks;
                    CollectMotionTracksFromMotion(motionDict, motionName, sceneName,
                        tracks, logger);
                    size_t trackCount = tracks.size();
                    if(trackCount > 0) {
                        media.addMotionTracks(archiveKey, sceneName, motionName,
                            std::move(tracks));
                        if(logger) logger->info("Stored {} tracks for {}/{}",
                            trackCount, sceneName, motionName);
                    }
                }
            }
        }

        // Recursively build a node's frame timeline from its "frameList" (same
        // content fields as the flat track: time + content{src,ox,oy,coord,op}).
        // 递归解析节点自身的 "frameList" 帧时间线（字段与扁平轨道一致）。
        void CollectMotionNodeFrames(const std::shared_ptr<PSBDictionary> &layerDict,
                                     PSBMedia::PSBMotionNode &node) {
            auto frameList = std::dynamic_pointer_cast<PSBList>((*layerDict)["frameList"]);
            if(!frameList) return;
            node.frames.reserve(frameList->size());
            for(int j = 0; j < static_cast<int>(frameList->size()); j++) {
                auto frame = std::dynamic_pointer_cast<PSBDictionary>((*frameList)[j]);
                if(!frame) continue;
                PSBMedia::PSBMotionFrame f;
                // Same frames→ms conversion as the flat tracks (see
                // CollectMotionTracksFromMotion): keep the Player clock (ms) and
                // the node timeline in one unit.
                // 与扁平轨道同样的 帧→毫秒 换算（见 CollectMotionTracksFromMotion）：
                // 让 Player 时钟（毫秒）与节点时间线保持同一单位。
                f.time = static_cast<int>(GetPSBFloat((*frame)["time"], 0) *
                                           1000.0 / 60.0);
                // PSB frame "type" (0=invisible) — see CollectMotionTracksFromMotion.
                // A type==0 frame hides the node even when it carries a content/src
                // (yuzulogo white/logo t=0: the static logo must not show before
                // the intro animation starts).
                // PSB 帧 "type"（0=不可见）——见 CollectMotionTracksFromMotion。
                // type==0 帧即使带 content/src 也隐藏节点（yuzulogo white/logo 的
                // t=0 帧：完整静态 logo 不该在片头动画开始前出现）。
                f.type = static_cast<int>(GetPSBFloat((*frame)["type"], 0));
                auto content = std::dynamic_pointer_cast<PSBDictionary>((*frame)["content"]);
                if(content && f.type != 0) {
                    auto srcVal = std::dynamic_pointer_cast<PSBString>((*content)["src"]);
                    if(srcVal) f.src = srcVal->value;
                    f.ox = GetPSBFloat((*content)["ox"], 0);
                    f.oy = GetPSBFloat((*content)["oy"], 0);
                    auto coord = std::dynamic_pointer_cast<PSBList>((*content)["coord"]);
                    if(coord && coord->size() >= 2) {
                        f.cx = GetPSBFloat((*coord)[0], 0);
                        f.cy = GetPSBFloat((*coord)[1], 0);
                    }
                    // M2 per-frame scale from content "zx"/"zy" (and coord[2] 'z'
                    // fallback) — same as the flat track path. Logo backdrops
                    // magnify a tiny source image to fullscreen via zx/zy.
                    // M2 帧内缩放来自 content "zx"/"zy"（及 coord[2] 'z' 兜底）——
                    // 与扁平轨道一致。logo 背景以小图 + zx/zy 放大到全屏。
                    {
                        const float zx = GetPSBFloat((*content)["zx"], 0.0f);
                        const float zy = GetPSBFloat((*content)["zy"], 0.0f);
                        if(zx != 0.0f) f.scaleX = zx;
                        if(zy != 0.0f) f.scaleY = zy;
                        if(zx == 0.0f || zy == 0.0f) {
                            if(coord && coord->size() >= 3) {
                                const float z = GetPSBFloat((*coord)[2], 0.0f);
                                if(z != 0.0f) {
                                    if(zx == 0.0f) f.scaleX = z;
                                    if(zy == 0.0f) f.scaleY = z;
                                }
                            }
                        }
                    }
                    // Round 2 same as flat path: blend (bm) + clip probe.
                    // 第二轮同扁平路径：混合(bm)+裁切探针。
                    f.blendMode = static_cast<int>(GetPSBFloat((*content)["bm"], 0));
                    // M2 per-frame rotation (content "angle", degrees), same as flat.
                    // M2 帧内旋转（content "angle"，单位度），同扁平路径。
                    f.angle = GetPSBFloat((*content)["angle"], 0.0f);
                    // M2 skew (content "sx"/"sy"), same as flat path.
                    // M2 斜切(content "sx"/"sy")，同扁平路径。
                    f.slantX = GetPSBFloat((*content)["sx"], 0.0f);
                    f.slantY = GetPSBFloat((*content)["sy"], 0.0f);
                    if(auto clipList =
                           std::dynamic_pointer_cast<PSBList>((*content)["clip"])) {
                        if(clipList->size() >= 4) {
                            f.hasClip = true;
                            f.clipL = static_cast<int>(GetPSBFloat((*clipList)[0], 0));
                            f.clipT = static_cast<int>(GetPSBFloat((*clipList)[1], 0));
                            f.clipR = static_cast<int>(GetPSBFloat((*clipList)[2], 0));
                            f.clipB = static_cast<int>(GetPSBFloat((*clipList)[3], 0));
                        }
                    }
                    // M2 flip (content "fx"/"fy") same as flat path.
                    // M2 翻转（content "fx"/"fy"）同扁平路径。
                    f.flipX = GetPSBFloat((*content)["fx"], 0.0f) != 0.0f;
                    f.flipY = GetPSBFloat((*content)["fy"], 0.0f) != 0.0f;
                    // M2 frame opacity is stored under "opa" (0..255), not "op". Reading the
                    // wrong key always yielded the fallback 255, so any layer whose entrance is
                    // driven by opacity (e.g. yuzulogo's `white`/`logo` t=0 frame has `opa:0`
                    // → the complete cyan logo must stay transparent at first) was rendered
                    // fully opaque. Read "opa" first, fall back to "op".
                    // M2 帧透明度字段是 "opa"（0..255），不是 "op"。读错 key 会恒返回兜底 255，
                    // 导致以透明度驱动入场/出场的层被画成完全不透明（如 yuzulogo 的 white/logo
                    // 层 t=0 帧 `opa:0`，完整青色 logo 开首应保持透明）。优先读 "opa"，无则回退 "op"。
                    f.opacity = GetPSBFloat((*content)["opa"], -1.0f);
                    if(f.opacity < 0.0f)
                        f.opacity = GetPSBFloat((*content)["op"], 255.0f);
                    // M2 cubic-bezier easing (content "ccc") for this frame's
                    // interpolation toward the NEXT keyframe, if present.
                    // M2 三次贝塞尔缓动（content "ccc"）：本帧向下一帧插值用，若存在。
                    // Per-property bezier easing: the reference (libkrkr2/AetherKiri)
                    // reads a SEPARATE curve for EACH attribute ("ccc"=color,
                    // "acc"=angle, "zcc"=scale, "occ"=opacity, "scc"=slant). We read all
                    // of them so the interpolator can ease each attribute with its own
                    // curve instead of reusing "ccc" for everything (which mis-times the
                    // m2logo M-fold rotation and the position/scale tweens).
                    // 逐属性贝塞尔缓动：参考（libkrkr2/AetherKiri）为**每个属性**读取各自
                    // 曲线（"ccc"=颜色、"acc"=角度、"zcc"=缩放、"occ"=透明度、scc=斜切）。
                    // 全部读取后，插值器才能让每个属性用各自曲线，而不是把 "ccc" 复用于
                    // 所有属性（否则 m2logo 的 M 折叠角与位置/缩放补间会相对参考错位）。
                    if(auto cccObj = std::dynamic_pointer_cast<PSBDictionary>((*content)["ccc"]))
                        if(auto xl = std::dynamic_pointer_cast<PSBList>((*cccObj)["x"]),
                                   yl = std::dynamic_pointer_cast<PSBList>((*cccObj)["y"]);
                           xl && yl && xl->size() >= 3 && yl->size() >= 3) {
                            f.hasEasing = true;
                            f.easeX1 = GetPSBFloat((*xl)[1], 0.0f);
                            f.easeY1 = GetPSBFloat((*yl)[1], 0.0f);
                            f.easeX2 = GetPSBFloat((*xl)[2], 1.0f);
                            f.easeY2 = GetPSBFloat((*yl)[2], 1.0f);
                        }
                    // "acc": angle curve.
                    if(auto accObj = std::dynamic_pointer_cast<PSBDictionary>((*content)["acc"]))
                        if(auto xl = std::dynamic_pointer_cast<PSBList>((*accObj)["x"]),
                                   yl = std::dynamic_pointer_cast<PSBList>((*accObj)["y"]);
                           xl && yl && xl->size() >= 3 && yl->size() >= 3) {
                            f.hasAngleEasing = true;
                            f.acX1 = GetPSBFloat((*xl)[1], 0.0f);
                            f.acY1 = GetPSBFloat((*yl)[1], 0.0f);
                            f.acX2 = GetPSBFloat((*xl)[2], 1.0f);
                            f.acY2 = GetPSBFloat((*yl)[2], 1.0f);
                        }
                    // "zcc": scale curve.
                    if(auto zccObj = std::dynamic_pointer_cast<PSBDictionary>((*content)["zcc"]))
                        if(auto xl = std::dynamic_pointer_cast<PSBList>((*zccObj)["x"]),
                                   yl = std::dynamic_pointer_cast<PSBList>((*zccObj)["y"]);
                           xl && yl && xl->size() >= 3 && yl->size() >= 3) {
                            f.hasScaleEasing = true;
                            f.zcX1 = GetPSBFloat((*xl)[1], 0.0f);
                            f.zcY1 = GetPSBFloat((*yl)[1], 0.0f);
                            f.zcX2 = GetPSBFloat((*xl)[2], 1.0f);
                            f.zcY2 = GetPSBFloat((*yl)[2], 1.0f);
                        }
                    // "occ": opacity curve.
                    if(auto occObj = std::dynamic_pointer_cast<PSBDictionary>((*content)["occ"]))
                        if(auto xl = std::dynamic_pointer_cast<PSBList>((*occObj)["x"]),
                                   yl = std::dynamic_pointer_cast<PSBList>((*occObj)["y"]);
                           xl && yl && xl->size() >= 3 && yl->size() >= 3) {
                            f.hasOpacityEasing = true;
                            f.ocX1 = GetPSBFloat((*xl)[1], 0.0f);
                            f.ocY1 = GetPSBFloat((*yl)[1], 0.0f);
                            f.ocX2 = GetPSBFloat((*xl)[2], 1.0f);
                            f.ocY2 = GetPSBFloat((*yl)[2], 1.0f);
                        }
                    // "scc": slant curve.
                    if(auto sccObj = std::dynamic_pointer_cast<PSBDictionary>((*content)["scc"]))
                        if(auto xl = std::dynamic_pointer_cast<PSBList>((*sccObj)["x"]),
                                   yl = std::dynamic_pointer_cast<PSBList>((*sccObj)["y"]);
                           xl && yl && xl->size() >= 3 && yl->size() >= 3) {
                            f.hasSlantEasing = true;
                            f.sccX1 = GetPSBFloat((*xl)[1], 0.0f);
                            f.sccY1 = GetPSBFloat((*yl)[1], 0.0f);
                            f.sccX2 = GetPSBFloat((*xl)[2], 1.0f);
                            f.sccY2 = GetPSBFloat((*yl)[2], 1.0f);
                        }
                    // M2 per-sprite vertex color (content "color"): either a
                    // dictionary of "0".."3" (four packed ARGB DWORDs, one per corner)
                    // or a scalar number broadcast to all four corners. Default stays
                    // opaque white (identity — Player::applyFlatTint skips it).
                    // M2 精灵顶点色（content "color"）：要么是 "0".."3" 的字典（四角各一
                    // 个打包 ARGB DWORD），要么是广播到四角的标量数字。默认保持不透明白
                    //（恒等——Player::applyFlatTint 会跳过）。
                    if(auto colorDict =
                           std::dynamic_pointer_cast<PSBDictionary>((*content)["color"])) {
                        for(int ci = 0; ci < 4; ci++) {
                            const std::string key = std::to_string(ci);
                            const auto val = std::dynamic_pointer_cast<PSBNumber>(
                                (*colorDict)[key]);
                            if(val) {
                                f.packedColors[ci] =
                                    static_cast<std::uint32_t>(GetPSBInt(val, 0xFFFFFFFF));
                            }
                        }
                    } else if(auto colorVal =
                                  std::dynamic_pointer_cast<PSBNumber>((*content)["color"])) {
                        const std::uint32_t packed =
                            static_cast<std::uint32_t>(GetPSBInt(colorVal, 0xFFFFFFFF));
                        f.packedColors = { packed, packed, packed, packed };
                    } else if(auto colorList =
                                  std::dynamic_pointer_cast<PSBList>((*content)["color"])) {
                        // M2 text glyphs sometimes store the color as a flat list of
                        // four packed ARGB DWORDs (per corner) instead of a { "0".."3" }
                        // dict or a scalar. Fall back to the list so C/W and the cross
                        // vertical line actually get their red/black tint.
                        // M2 文本字形有时把颜色存成**扁平四值 list**（每个打包 ARGB DWORD，
                        // 四角各一），而不是 { "0".."3" } 字典或标量。回退到 list，让 C/W
                        // 和十字竖线真正拿到红/黑着色。
                        for(int ci = 0; ci < 4 && ci < static_cast<int>(colorList->size()); ci++) {
                            const auto val = std::dynamic_pointer_cast<PSBNumber>(
                                (*colorList)[ci]);
                            if(val) {
                                f.packedColors[ci] =
                                    static_cast<std::uint32_t>(GetPSBInt(val, 0xFFFFFFFF));
                            }
                        }
                    }
                } else {
                    // type==0 (invisible) or a frame without content only marks
                    // a time change: the node is invisible during this range.
                    // type==0（不可见）或无 content 的帧只是时间标记：该时段内
                    // 节点不可见。
                    f.visible = false;
                }
                node.frames.push_back(std::move(f));
            }
            std::stable_sort(node.frames.begin(), node.frames.end(),
                [](const PSBMedia::PSBMotionFrame &a, const PSBMedia::PSBMotionFrame &b) {
                    return a.time < b.time;
                });
        }

        // Recursively build the M2 layer NODE TREE from the motion's "layer" array,
        // descending into the PSB "children" key. Pre-order insertion guarantees
        // parents come before their children in `nodes`, which is what top-down
        // position/opacity accumulation requires.
        // 递归从 motion 的 "layer" 数组构建 M2 图层**节点树**，沿 PSB "children" 下钻。
        // 先序插入保证 `nodes` 中父节点总在子节点之前——这正是自顶向下累加坐标/透明度所需。
        void CollectMotionNodesFromLayerList(const std::shared_ptr<PSBList> &layerList,
                                             int parentIndex,
                                             std::vector<PSBMedia::PSBMotionNode> &nodes,
                                             const std::shared_ptr<spdlog::logger> &logger) {
            if(!layerList) return;
            for(int i = 0; i < static_cast<int>(layerList->size()); i++) {
                auto layerDict = std::dynamic_pointer_cast<PSBDictionary>((*layerList)[i]);
                if(!layerDict) continue;
                PSBMedia::PSBMotionNode node;
                auto labelVal = std::dynamic_pointer_cast<PSBString>((*layerDict)["label"]);
                node.label = labelVal ? labelVal->value : ("layer_" + std::to_string(i));
                node.parentIndex = parentIndex;
                node.type = static_cast<int>(GetPSBFloat((*layerDict)["type"], 0));
                // Per-node transform inheritance mask / local-matrix operator order.
                // libkrkr2 sub_6B3C78 reads "inheritMask" (default 0x1FC = inherit all)
                // and "transformOrder" (default [0,1,2,3]). Without these we can't gate
                // scale/angle/flip inheritance per node (e.g. m2logo letters must NOT
                // inherit str_clip's clip-region scale).
                // 节点的变换继承掩码 / 局部矩阵算子顺序。libkrkr2 sub_6B3C78 读取
                // "inheritMask"（默认 0x1FC=全部继承）与 "transformOrder"（默认
                // [0,1,2,3]）。缺它们就无法按节点门控 scale/angle/flip 继承（如 m2logo
                // 字母不能继承 str_clip 的裁剪窗口缩放）。
                node.inheritMask = static_cast<int>(GetPSBFloat((*layerDict)["inheritMask"], 0x1FC));
                if(auto toList = std::dynamic_pointer_cast<PSBList>((*layerDict)["transformOrder"])) {
                    for(int k = 0; k < 4 && k < static_cast<int>(toList->size()); k++) {
                        node.transformOrder[k] = static_cast<int>(GetPSBFloat((*toList)[k], k));
                    }
                }
                // Layer display size: the texture may be smaller than the layer
                // (e.g. yuzulogo's 64x64 white_box stretched to fill the canvas).
                // 图层显示尺寸：纹理可能小于图层（如 yuzulogo 的 64x64 white_box
                // 需拉伸铺满画布）。
                node.width = static_cast<int>(GetPSBFloat((*layerDict)["width"], 0));
                node.height = static_cast<int>(GetPSBFloat((*layerDict)["height"], 0));
                CollectMotionNodeFrames(layerDict, node);
                if(logger) logger->info("  node[{}] '{}' parent={} type={} box={}x{} inh=0x{:x} to={},{},{},{} frames={} firstsrc='{}'",
                    static_cast<int>(nodes.size()), node.label, parentIndex, node.type,
                    node.width, node.height, node.inheritMask,
                    node.transformOrder[0], node.transformOrder[1],
                    node.transformOrder[2], node.transformOrder[3],
                    static_cast<int>(node.frames.size()),
                    node.frames.empty() ? std::string("") : node.frames.front().src);
                const int myIndex = static_cast<int>(nodes.size());
                nodes.push_back(std::move(node));
                // Descend into children (the actual layer hierarchy).
                // 下钻到 children（真正的图层层级）。
                auto children = std::dynamic_pointer_cast<PSBList>((*layerDict)["children"]);
                if(children && children->size() > 0) {
                    CollectMotionNodesFromLayerList(children, myIndex, nodes, logger);
                }
            }
        }

        // Extract the layered node tree for every scene/motion in the object tree.
        // 对对象树里每个场景/每个 motion 提取分层节点树。
        void CollectAllMotionNodeTrees(PSBMedia &media,
                                       const std::string &archiveKey,
                                       const std::shared_ptr<PSBDictionary> &objectTree,
                                       const std::shared_ptr<spdlog::logger> &logger) {
            if(!objectTree) return;
            for(const auto &[sceneName, sceneVal] : *objectTree) {
                auto sceneDict = std::dynamic_pointer_cast<PSBDictionary>(sceneVal);
                if(!sceneDict) continue;
                auto motionDict = std::dynamic_pointer_cast<PSBDictionary>((*sceneDict)["motion"]);
                if(!motionDict) continue;
                for(const auto &[motionName, motionVal] : *motionDict) {
                    auto targetMotion = std::dynamic_pointer_cast<PSBDictionary>(motionVal);
                    if(!targetMotion) continue;
                    auto layerList = std::dynamic_pointer_cast<PSBList>((*targetMotion)["layer"]);
                    if(!layerList) continue;
                    std::vector<PSBMedia::PSBMotionNode> nodes;
                    CollectMotionNodesFromLayerList(layerList, -1, nodes, logger);
                    if(!nodes.empty()) {
                        const size_t storedNodeCount = nodes.size();
                        media.addMotionNodes(archiveKey, sceneName, motionName,
                                             std::move(nodes));
                        if(logger) logger->info("Stored {} nodes for {}/{}",
                            storedNodeCount, sceneName, motionName);
                    }
                }
            }
        }

        void CollectLayerPositionsFromMotion(
            std::vector<PSBMedia::LayerPosition> &positions,
            std::vector<PSBMedia::ButtonBoundInfo> &buttons,
            const std::shared_ptr<PSBDictionary> &objectTree,
            const std::shared_ptr<spdlog::logger> &logger) {
            if(!objectTree) return;

            for(const auto &[sceneName, sceneVal] : *objectTree) {
                auto sceneDict = std::dynamic_pointer_cast<PSBDictionary>(sceneVal);
                if(!sceneDict) continue;

                auto motionDict = std::dynamic_pointer_cast<PSBDictionary>((*sceneDict)["motion"]);
                if(!motionDict) continue;

                std::string motionName = "normal";
                if(!std::dynamic_pointer_cast<PSBDictionary>((*motionDict)[motionName])) {
                    motionName = "show";
                }
                if(!std::dynamic_pointer_cast<PSBDictionary>((*motionDict)[motionName])) {
                    motionName = "bt";
                }

                CollectLayersFromMotion(motionDict, motionName, sceneName,
                    0, 0, objectTree, positions, &buttons, logger);

                // Some Yuzusoft scenes (e.g. yuzulogo.mtn "LOGO") name their
                // animation arbitrarily instead of normal/show/bt, so the fixed
                // list above yields nothing. Enumerate the whole motion dict so
                // their source layers still get correct coordinates.
                // 部分 Yuzusoft 场景（如 yuzulogo.mtn 的 "LOGO"）的动画命名并非
                // normal/show/bt 固定名，上面的枚举取不到任何东西。遍历整个 motion
                // 字典，让它们的源图层仍能获得正确坐标。
                if(!std::dynamic_pointer_cast<PSBDictionary>((*motionDict)[motionName])) {
                    for(const auto &[altName, altVal] : *motionDict) {
                        if(!std::dynamic_pointer_cast<PSBDictionary>(altVal)) continue;
                        CollectLayersFromMotion(motionDict, altName, sceneName,
                            0, 0, objectTree, positions, &buttons, logger);
                    }
                }
            }
        }

        void RegisterPSBResourcesIntoMedia(
            PSBMedia &media, PSBFile &psb, const std::string &archiveKey) {
            auto logger = LOGGER;
            size_t logged = 0;
            const auto objs = psb.getObjects();
            if(objs) {
                for(const auto &[name, value] : *objs) {
                    const auto resource =
                        std::dynamic_pointer_cast<PSBResource>(value);
                    if(!resource)
                        continue;
                    media.add(archiveKey + "/" + name, resource);
                    if(logger && logged < 40 &&
                       (archiveKey == "main.psb" || archiveKey == "title.psb" ||
                        archiveKey == "chapter.psb" || archiveKey == "autoskip.psb")) {
                        logger->info("psb register: {}/{}", archiveKey, name);
                        ++logged;
                    }
                }
            }

            auto *handler = psb.getTypeHandler();
            if(!handler)
                return;
            auto resources = handler->collectResources(psb, false);
            for(auto &metadata : resources) {
                auto *image =
                    dynamic_cast<ImageMetadata *>(metadata.get());
                if(!image)
                    continue;
                auto resource = image->getResource();
                if(!resource)
                    continue;
                const std::string name = image->getName();
                if(name.empty())
                    continue;
                media.add(archiveKey + "/" + name, resource, image);
                if(logger && logged < 120 &&
                   (archiveKey == "main.psb" || archiveKey == "title.psb" ||
                    archiveKey == "chapter.psb" || archiveKey == "autoskip.psb")) {
                    logger->info("psb register: {}/{}", archiveKey, name);
                    ++logged;
                }
            }

            if(objs) {
                auto objectTree = std::dynamic_pointer_cast<PSBDictionary>((*objs)["object"]);
                if(objectTree && logger) {
                    std::string objNames;
                    for(const auto &[k, v] : *objectTree) {
                        if(!objNames.empty()) objNames += ", ";
                        objNames += k;
                        auto d = std::dynamic_pointer_cast<PSBDictionary>(v);
                        if(d) {
                            auto m = std::dynamic_pointer_cast<PSBDictionary>((*d)["motion"]);
                            if(m) {
                                objNames += "[";
                                bool first = true;
                                for(const auto &[mk, mv] : *m) {
                                    if(!first) objNames += ",";
                                    objNames += mk;
                                    first = false;
                                }
                                objNames += "]";
                            }
                        }
                    }
                    logger->info("PSB objectTree for {}: {}", archiveKey, objNames);
                }

                if(objectTree) {
                    std::vector<PSBMedia::LayerPosition> positions;
                    std::vector<PSBMedia::ButtonBoundInfo> buttons;
                    CollectLayerPositionsFromMotion(positions, buttons, objectTree, logger);
                    if(!positions.empty()) {
                        size_t count = positions.size();
                        media.addLayerPositions(archiveKey, std::move(positions));
                        if(logger) logger->info("Stored {} layer positions for {}",
                            count, archiveKey);
                    }
                    if(!buttons.empty()) {
                        size_t count = buttons.size();
                        media.addButtonBounds(archiveKey, std::move(buttons));
                        if(logger) logger->info("Stored {} button bounds for {}",
                            count, archiveKey);
                    }
                    // Frame time-lines for every scene/motion (M2 animation).
                    // 提取全部场景/motion 的帧时间线（M2 动画）。
                    CollectAllMotionTracks(media, archiveKey, objectTree, logger);
                    // Layered node trees (parent→child) for generic M2 accumulation.
                    // 分层节点树（父子关系），供通用 M2 坐标/透明度累加。
                    CollectAllMotionNodeTrees(media, archiveKey, objectTree, logger);
                }
            }
        }
    } // namespace

    PSBMedia::PSBMedia() {
        _ref = 1;

        tTJSVariant val;
        if(TVPGetCommandLine(TJS_W("memory_profile"), &val)) {
            ttstr profile = ttstr(val).AsLowerCase();
            if(profile == TJS_W("aggressive") || profile == TJS_W("lowmem")) {
                _configuredMaxEntryCount = 1024;
                _configuredMaxByteSize = 128ULL * 1024ULL * 1024ULL;
            }
        }

        if(TVPGetCommandLine(TJS_W("psb_cache_entries"), &val)) {
            const tjs_int configured = static_cast<tjs_int>(val.AsInteger());
            if(configured > 0) {
                _configuredMaxEntryCount =
                    static_cast<size_t>(configured);
            }
        }
        if(TVPGetCommandLine(TJS_W("psb_cache_mb"), &val)) {
            const tjs_int configured = static_cast<tjs_int>(val.AsInteger());
            if(configured > 0) {
                _configuredMaxByteSize = static_cast<size_t>(configured) *
                    1024ULL * 1024ULL;
            }
        }

        _configuredMaxEntryCount =
            ClampSizeT(_configuredMaxEntryCount, 128, 8192);
        _configuredMaxByteSize = ClampSizeT(
            _configuredMaxByteSize, 16ULL * 1024ULL * 1024ULL,
            512ULL * 1024ULL * 1024ULL);
        _maxEntryCount = _configuredMaxEntryCount;
        _maxByteSize = _configuredMaxByteSize;
    }

    void PSBMedia::NormalizeDomainName(ttstr &name) {
        name = name.AsLowerCase();
    }

    void PSBMedia::NormalizePathName(ttstr &name) {
        auto *p = name.Independ();
        while(*p) {
            if(*p == TJS_W('\\')) {
                *p = TJS_W('/');
            }
            ++p;
        }
        name = name.AsLowerCase();
    }

    std::string PSBMedia::canonicalizeKey(const std::string &key) const {
        std::string out;
        out.reserve(key.size());
        for(const char ch : key) {
            if(ch == '\\') {
                out.push_back('/');
            } else if(ch >= 'A' && ch <= 'Z') {
                out.push_back(static_cast<char>(ch - 'A' + 'a'));
            } else {
                out.push_back(ch);
            }
        }

        constexpr const char *kFileScheme = "file://";
        if(out.rfind(kFileScheme, 0) == 0) {
            out.erase(0, 7);
            if(out.rfind("./", 0) == 0) {
                out.erase(0, 1);
            }
        }
        if(out.rfind("./", 0) == 0) {
            out.erase(0, 1);
        }

        std::string compact;
        compact.reserve(out.size());
        bool prevSlash = false;
        for(const char ch : out) {
            if(ch == '/') {
                if(prevSlash) {
                    continue;
                }
                prevSlash = true;
            } else {
                prevSlash = false;
            }
            compact.push_back(ch);
        }
        return compact;
    }

    void PSBMedia::touchLocked(CacheEntry &entry) {
        if(entry.lruIt != _lru.begin()) {
            _lru.splice(_lru.begin(), _lru, entry.lruIt);
            entry.lruIt = _lru.begin();
        }
    }

    void PSBMedia::adaptBudgetByMemoryPressureLocked() {
        const tjs_int self_used_mb = TVPGetSelfUsedMemory();
        const tjs_int free_mb = TVPGetSystemFreeMemory();

        size_t max_entry_count = _configuredMaxEntryCount;
        size_t max_byte_size = _configuredMaxByteSize;

        if((self_used_mb >= 1500) || (free_mb >= 0 && free_mb < 512)) {
            max_entry_count = std::min(max_entry_count, static_cast<size_t>(512));
            max_byte_size = std::min(
                max_byte_size, static_cast<size_t>(96ULL * 1024ULL * 1024ULL));
        } else if((self_used_mb >= 1100) || (free_mb >= 0 && free_mb < 800)) {
            max_entry_count = std::min(max_entry_count, static_cast<size_t>(768));
            max_byte_size = std::min(
                max_byte_size, static_cast<size_t>(144ULL * 1024ULL * 1024ULL));
        } else if((self_used_mb >= 850) || (free_mb >= 0 && free_mb < 1200)) {
            max_entry_count = std::min(max_entry_count, static_cast<size_t>(1024));
            max_byte_size = std::min(
                max_byte_size, static_cast<size_t>(192ULL * 1024ULL * 1024ULL));
        }

        _maxEntryCount = max_entry_count;
        _maxByteSize = max_byte_size;
    }

    PSBMedia::ResourceMap::iterator
    PSBMedia::findBySuffixLocked(const std::string &key) {
        for(auto it = _resources.begin(); it != _resources.end(); ++it) {
            const auto &stored = it->first;
            if(stored.size() < key.size()) {
                continue;
            }
            if(stored.compare(stored.size() - key.size(), key.size(), key) != 0) {
                continue;
            }
            if(stored.size() == key.size()) {
                return it;
            }

            const char boundary = stored[stored.size() - key.size() - 1];
            if(boundary == '/' || boundary == '>') {
                return it;
            }
        }
        return _resources.end();
    }

    void PSBMedia::evictIfNeededLocked() {
        adaptBudgetByMemoryPressureLocked();

        size_t evictedCount = 0;
        size_t evictedBytes = 0;
        while((_resources.size() > _maxEntryCount || _bytesInUse > _maxByteSize) &&
              !_lru.empty()) {
            const std::string victimKey = _lru.back();
            _lru.pop_back();

            auto it = _resources.find(victimKey);
            if(it == _resources.end()) {
                continue;
            }

            evictedCount++;
            evictedBytes += it->second.sizeBytes;
            _bytesInUse -= it->second.sizeBytes;
            _resources.erase(it);
        }

        if(evictedCount > 0) {
            LOGGER->debug(
                "PSB media cache evicted: count={} bytes={} remain_count={} "
                "remain_bytes={}",
                evictedCount, evictedBytes, _resources.size(), _bytesInUse);
        }
    }

    bool PSBMedia::CheckExistentStorage(const ttstr &name) {
        const auto key = canonicalizeKey(name.AsStdString());
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(_resources.find(key) != _resources.end()) {
                _hitCount++;
                return true;
            }
            const bool found = findBySuffixLocked(key) != _resources.end();
            if(found)
                _hitCount++;
            else
                _missCount++;
            if(found)
                return true;
        }
        if(!tryLazyLoadArchive(key))
            return false;
        std::lock_guard<std::mutex> lock(_mutex);
        if(_resources.find(key) != _resources.end()) {
            _hitCount++;
            return true;
        }
        const bool found = findBySuffixLocked(key) != _resources.end();
        if(found)
            _hitCount++;
        else
            _missCount++;
        return found;
    }

    tTJSBinaryStream *PSBMedia::Open(const ttstr &name, tjs_uint32 flags) {
        (void)flags;
        const auto key = canonicalizeKey(name.AsStdString());

        std::shared_ptr<PSBResource> res;
        std::shared_ptr<std::vector<uint8_t>> convertedImage;
        CachedImageInfo imageInfo;
        bool hasImageInfo = false;
        std::string resolvedKey;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _resources.find(key);
            if(it == _resources.end()) {
                it = findBySuffixLocked(key);
                if(it != _resources.end()) {
                    LOGGER->debug("PSB media cache suffix-hit: {} -> {}", key,
                                  it->first);
                }
            }
            if(it != _resources.end() && it->second.resource != nullptr) {
                _hitCount++;
                touchLocked(it->second);
                res = it->second.resource;
                convertedImage = it->second.convertedImage;
                imageInfo = it->second.imageInfo;
                hasImageInfo = it->second.hasImageInfo;
                resolvedKey = it->first;
            }
        }

        if(!res && tryLazyLoadArchive(key)) {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _resources.find(key);
            if(it == _resources.end()) {
                it = findBySuffixLocked(key);
                if(it != _resources.end()) {
                    LOGGER->debug("PSB media cache suffix-hit(after-load): {} -> {}",
                                  key, it->first);
                }
            }
            if(it != _resources.end() && it->second.resource != nullptr) {
                _hitCount++;
                touchLocked(it->second);
                res = it->second.resource;
                convertedImage = it->second.convertedImage;
                imageInfo = it->second.imageInfo;
                hasImageInfo = it->second.hasImageInfo;
                resolvedKey = it->first;
            }
        }

        if(!res) {
            std::lock_guard<std::mutex> lock(_mutex);
            _missCount++;
            LOGGER->warn("PSB media cache miss: {}", key);
            TVPThrowExceptionMessage(TJS_W("%1:cannot open psb resource"),
                                     name);
            return nullptr;
        }
        if(LOGGER && (resolvedKey.rfind("main.psb/", 0) == 0 ||
                      resolvedKey.rfind("title.psb/", 0) == 0 ||
                      resolvedKey.rfind("chapter.psb/", 0) == 0 ||
                      resolvedKey.rfind("autoskip.psb/", 0) == 0)) {
            const uint32_t header =
                res->data.size() >= 4
                ? static_cast<uint32_t>(res->data[0]) |
                    (static_cast<uint32_t>(res->data[1]) << 8) |
                    (static_cast<uint32_t>(res->data[2]) << 16) |
                    (static_cast<uint32_t>(res->data[3]) << 24)
                : 0;
            LOGGER->info(
                "psb open: key={} hasMeta={} w={} h={} type={} palType={} pal={} compress={} raw={} header=0x{:08x}",
                resolvedKey, hasImageInfo ? 1 : 0, imageInfo.width,
                imageInfo.height, imageInfo.type, imageInfo.paletteType,
                imageInfo.palette.size(), static_cast<int>(imageInfo.compress),
                res->data.size(), header);
        }
        if(!convertedImage && hasImageInfo && !IsSupportedImageHeader(res->data)) {
            convertedImage = BuildBmpFromRaw(imageInfo, res);
            if(LOGGER && (resolvedKey.rfind("main.psb/", 0) == 0 ||
                          resolvedKey.rfind("title.psb/", 0) == 0 ||
                          resolvedKey.rfind("chapter.psb/", 0) == 0 ||
                          resolvedKey.rfind("autoskip.psb/", 0) == 0)) {
                LOGGER->info(
                    "psb open: convert key={} ok={} converted={} type={}",
                    resolvedKey, convertedImage ? 1 : 0,
                    convertedImage ? convertedImage->size() : 0,
                    imageInfo.type);
            }
            if(convertedImage) {
                std::lock_guard<std::mutex> lock(_mutex);
                auto it = _resources.find(resolvedKey);
                if(it != _resources.end()) {
                    _bytesInUse -= it->second.sizeBytes;
                    it->second.convertedImage = convertedImage;
                    it->second.sizeBytes = CalcEntryFootprint(it->second);
                    _bytesInUse += it->second.sizeBytes;
                    touchLocked(it->second);
                    evictIfNeededLocked();
                }
            }
        }

        const auto &streamBytes =
            convertedImage ? *convertedImage : res->data;
        if(LOGGER && (resolvedKey.rfind("main.psb/", 0) == 0 ||
                      resolvedKey.rfind("title.psb/", 0) == 0 ||
                      resolvedKey.rfind("chapter.psb/", 0) == 0 ||
                      resolvedKey.rfind("autoskip.psb/", 0) == 0)) {
            const uint32_t outHeader =
                streamBytes.size() >= 4
                ? static_cast<uint32_t>(streamBytes[0]) |
                    (static_cast<uint32_t>(streamBytes[1]) << 8) |
                    (static_cast<uint32_t>(streamBytes[2]) << 16) |
                    (static_cast<uint32_t>(streamBytes[3]) << 24)
                : 0;
            LOGGER->info("psb open: return key={} size={} header=0x{:08x}",
                         resolvedKey, streamBytes.size(), outHeader);
        }
        auto *memoryStream = new tTVPMemoryStream();
        memoryStream->WriteBuffer(streamBytes.data(), streamBytes.size());
        memoryStream->Seek(0, TJS_BS_SEEK_SET);
        return memoryStream;
    }

    bool PSBMedia::tryLazyLoadArchive(const std::string &key) {
        const auto slashPos = key.find('/');
        if(slashPos == std::string::npos || slashPos == 0)
            return false;

        const std::string archiveKey = key.substr(0, slashPos);
        bool shouldAttemptLoad = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            shouldAttemptLoad =
                _loadedArchives.insert(archiveKey).second;
        }
        if(!shouldAttemptLoad)
            return false;

        try {
            ttstr archivePath(archiveKey.c_str());
            if(auto cached = motion::ResourceManager::getLoadedFile(archivePath)) {
                LOGGER->info("PSB lazy-load archive(from cache): {}", archiveKey);
                RegisterPSBResourcesIntoMedia(*this, *cached, archiveKey);
                return true;
            }

            PSBFile psb;
            psb.setSeed(motion::ResourceManager::getDecryptSeed());
            if(!psb.loadPSBFile(archivePath)) {
                std::lock_guard<std::mutex> lock(_mutex);
                _loadedArchives.erase(archiveKey);
                LOGGER->debug("PSB lazy-load failed: {}", archiveKey);
                return false;
            }
            LOGGER->info("PSB lazy-load archive: {}", archiveKey);
            RegisterPSBResourcesIntoMedia(*this, psb, archiveKey);
            return true;
        } catch(const std::exception &e) {
            std::lock_guard<std::mutex> lock(_mutex);
            _loadedArchives.erase(archiveKey);
            LOGGER->warn("PSB lazy-load error: {} ({})", e.what(), archiveKey);
            return false;
        } catch(...) {
            std::lock_guard<std::mutex> lock(_mutex);
            _loadedArchives.erase(archiveKey);
            LOGGER->warn("PSB lazy-load unknown error: {}", archiveKey);
            return false;
        }
    }

    // Force-parse the archive so its layer positions and motion tracks are ready.
    // tryLazyLoadArchive keys on a path containing '/', so append a sentinel path.
    // Idempotent thanks to the _loadedArchives set inside tryLazyLoadArchive.
    // 立即解析归档，使其图层坐标与 motion 时间线就绪。tryLazyLoadArchive 按含 '/'
    // 的路径取归档名，故追加一个哨兵路径；内部 _loadedArchives 集合保证幂等。
    bool PSBMedia::ensureArchiveLoaded(const std::string &archiveKey) {
        return tryLazyLoadArchive(archiveKey + "/_probe");
    }

    void PSBMedia::GetListAt(const ttstr &name, iTVPStorageLister *lister) {
        LOGGER->error("TODO: PSBMedia GetListAt");
    }

    void PSBMedia::GetLocallyAccessibleName(ttstr &name) {
        LOGGER->error("can't get GetLocallyAccessibleName from {}!",
                      name.AsStdString());
    }

    void PSBMedia::add(const std::string &name,
                       const std::shared_ptr<PSBResource> &resource,
                       const ImageMetadata *imageMeta) {
        if(resource == nullptr) {
            return;
        }

        const auto key = canonicalizeKey(name);
        const size_t incomingSize = resource->data.size();

        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _resources.find(key);
        if(it != _resources.end()) {
            _bytesInUse -= it->second.sizeBytes;
            it->second.resource = resource;
            it->second.convertedImage.reset();
            it->second.hasImageInfo = imageMeta != nullptr;
            if(imageMeta) {
                it->second.imageInfo.debugKey = key;
                it->second.imageInfo.width = imageMeta->getWidth();
                it->second.imageInfo.height = imageMeta->getHeight();
                it->second.imageInfo.left = imageMeta->getLeft();
                it->second.imageInfo.top = imageMeta->getTop();
                it->second.imageInfo.originX = imageMeta->getOriginX();
                it->second.imageInfo.originY = imageMeta->getOriginY();
                it->second.imageInfo.opacity = imageMeta->getOpacity();
                it->second.imageInfo.visible = imageMeta->getVisible();
                it->second.imageInfo.layerType = imageMeta->getLayerType();
                it->second.imageInfo.type = imageMeta->getType();
                it->second.imageInfo.paletteType = imageMeta->getPalType();
                it->second.imageInfo.spec = imageMeta->getSpec();
                it->second.imageInfo.compress = imageMeta->getCompress();
                it->second.imageInfo.palette = imageMeta->getPalette().data;
            }
            it->second.sizeBytes = CalcEntryFootprint(it->second);
            _bytesInUse += it->second.sizeBytes;
            touchLocked(it->second);
        } else {
            _lru.push_front(key);
            CacheEntry entry{};
            entry.resource = resource;
            entry.hasImageInfo = imageMeta != nullptr;
            if(imageMeta) {
                entry.imageInfo.debugKey = key;
                entry.imageInfo.width = imageMeta->getWidth();
                entry.imageInfo.height = imageMeta->getHeight();
                entry.imageInfo.left = imageMeta->getLeft();
                entry.imageInfo.top = imageMeta->getTop();
                entry.imageInfo.originX = imageMeta->getOriginX();
                entry.imageInfo.originY = imageMeta->getOriginY();
                entry.imageInfo.opacity = imageMeta->getOpacity();
                entry.imageInfo.visible = imageMeta->getVisible();
                entry.imageInfo.layerType = imageMeta->getLayerType();
                entry.imageInfo.type = imageMeta->getType();
                entry.imageInfo.paletteType = imageMeta->getPalType();
                entry.imageInfo.spec = imageMeta->getSpec();
                entry.imageInfo.compress = imageMeta->getCompress();
                entry.imageInfo.palette = imageMeta->getPalette().data;
            }
            entry.sizeBytes = CalcEntryFootprint(entry);
            entry.lruIt = _lru.begin();
            size_t entrySize = entry.sizeBytes;
            _resources.emplace(key, std::move(entry));
            _bytesInUse += entrySize;
        }

        evictIfNeededLocked();
    }

    void PSBMedia::setCacheBudget(size_t maxEntries, size_t maxBytes) {
        std::lock_guard<std::mutex> lock(_mutex);
        _configuredMaxEntryCount = ClampSizeT(maxEntries, 128, 8192);
        _configuredMaxByteSize = ClampSizeT(
            maxBytes, 16ULL * 1024ULL * 1024ULL, 512ULL * 1024ULL * 1024ULL);
        _maxEntryCount = _configuredMaxEntryCount;
        _maxByteSize = _configuredMaxByteSize;
        evictIfNeededLocked();
    }

    PSBMediaCacheStats PSBMedia::getCacheStats() const {
        std::lock_guard<std::mutex> lock(_mutex);
        PSBMediaCacheStats stats;
        stats.entryCount = _resources.size();
        stats.entryLimit = _maxEntryCount;
        stats.bytesInUse = _bytesInUse;
        stats.byteLimit = _maxByteSize;
        stats.hitCount = _hitCount;
        stats.missCount = _missCount;
        return stats;
    }

    void PSBMedia::removeByPrefix(const std::string &prefix) {
        std::string normalizedPrefix = canonicalizeKey(prefix);
        if(!normalizedPrefix.empty() && normalizedPrefix.back() != '/') {
            normalizedPrefix.push_back('/');
        }

        std::lock_guard<std::mutex> lock(_mutex);
        for(auto it = _resources.begin(); it != _resources.end();) {
            if(it->first.rfind(normalizedPrefix, 0) == 0) {
                _bytesInUse -= it->second.sizeBytes;
                _lru.erase(it->second.lruIt);
                it = _resources.erase(it);
                continue;
            }
            ++it;
        }
    }

    void PSBMedia::clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _resources.clear();
        _lru.clear();
        _bytesInUse = 0;
        _hitCount = 0;
        _missCount = 0;
    }

    std::vector<PSBMedia::ImageInfoEntry> PSBMedia::getImagesByPrefix(
        const std::string &prefix) const {
        std::string norm = canonicalizeKey(prefix);
        if(!norm.empty() && norm.back() != '/')
            norm.push_back('/');
        std::vector<ImageInfoEntry> result;
        std::lock_guard<std::mutex> lock(_mutex);
        for(const auto &[key, entry] : _resources) {
            if(key.rfind(norm, 0) != 0)
                continue;
            if(!entry.hasImageInfo)
                continue;
            result.push_back({key, entry.imageInfo});
        }
        return result;
    }

    bool PSBMedia::getImageInfo(const std::string &key,
                                CachedImageInfo &outInfo) const {
        std::string norm = canonicalizeKey(key);
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _resources.find(norm);
        if(it == _resources.end() || !it->second.hasImageInfo)
            return false;
        outInfo = it->second.imageInfo;
        return true;
    }

    void PSBMedia::addLayerPositions(const std::string &archiveKey,
                                     std::vector<LayerPosition> positions) {
        std::lock_guard<std::mutex> lock(_mutex);
        _layerPositions[archiveKey] = std::move(positions);
    }

    std::vector<PSBMedia::LayerPosition>
    PSBMedia::getLayerPositions(const std::string &prefix) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _layerPositions.find(prefix);
        if(it != _layerPositions.end())
            return it->second;
        return {};
    }

    void PSBMedia::addButtonBounds(const std::string &archiveKey,
                                   std::vector<ButtonBoundInfo> bounds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _buttonBoundsMap[archiveKey] = std::move(bounds);
    }

    std::vector<PSBMedia::ButtonBoundInfo>
    PSBMedia::getButtonBounds(const std::string &prefix) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _buttonBoundsMap.find(prefix);
        if(it != _buttonBoundsMap.end())
            return it->second;
        return {};
    }

    void PSBMedia::addMotionTracks(const std::string &archiveKey,
                                   const std::string &sceneName,
                                   const std::string &motionName,
                                   std::vector<PSBMotionLayerTrack> tracks) {
        std::lock_guard<std::mutex> lock(_mutex);
        _motionTracks[archiveKey + "|" + sceneName + "|" + motionName] =
            std::move(tracks);
    }

    void PSBMedia::addMotionNodes(const std::string &archiveKey,
                                  const std::string &sceneName,
                                  const std::string &motionName,
                                  std::vector<PSBMotionNode> nodes) {
        std::lock_guard<std::mutex> lock(_mutex);
        _motionNodes[archiveKey + "|" + sceneName + "|" + motionName] =
            std::move(nodes);
    }

    std::vector<PSBMedia::PSBMotionNode>
    PSBMedia::getMotionNodes(const std::string &archiveKey,
                             const std::string &sceneName,
                             const std::string &motionName) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _motionNodes.find(archiveKey + "|" + sceneName + "|" + motionName);
        if(it != _motionNodes.end())
            return it->second;
        return {};
    }

    void PSBMedia::setMotionLoopTime(const std::string &archiveKey,
                                     const std::string &sceneName,
                                     const std::string &motionName,
                                     tjs_int loopTime) {
        std::lock_guard<std::mutex> lock(_mutex);
        _motionLoopTimes[archiveKey + "|" + sceneName + "|" + motionName] = loopTime;
    }

    tjs_int PSBMedia::getMotionLoopTime(const std::string &archiveKey,
                                        const std::string &sceneName,
                                        const std::string &motionName) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _motionLoopTimes.find(archiveKey + "|" + sceneName + "|" + motionName);
        if(it != _motionLoopTimes.end())
            return it->second;
        return 0;
    }

    std::vector<PSBMedia::PSBMotionLayerTrack>
    PSBMedia::getMotionTracks(const std::string &archiveKey,
                              const std::string &sceneName,
                              const std::string &motionName) const {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _motionTracks.find(archiveKey + "|" + sceneName + "|" + motionName);
        if(it != _motionTracks.end())
            return it->second;
        return {};
    }

    std::vector<std::string>
    PSBMedia::getMotionNames(const std::string &archiveKey,
                             const std::string &sceneName) const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> names;
        const std::string prefix = archiveKey + "|" + sceneName + "|";
        for(const auto &[key, tracks] : _motionTracks) {
            if(key.size() > prefix.size() && key.compare(0, prefix.size(), prefix) == 0) {
                names.push_back(key.substr(prefix.size()));
            }
        }
        return names;
    }
} // namespace PSB
