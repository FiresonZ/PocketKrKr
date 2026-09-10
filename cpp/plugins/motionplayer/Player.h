//
// Created by LiDon on 2025/9/15.
//
#pragma once

#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <spdlog/spdlog.h>
#include "ResourceManager.h"
#include "tjs.h"
#include "tjsArray.h"
#include "tjsDictionary.h"
#include "../core/base/StorageIntf.h"
#include "../core/base/ScriptMgnIntf.h"
#include "../core/base/SysInitIntf.h"
#include "../core/visual/WindowIntf.h"
#include "../psbfile/PSBMedia.h"
#include "SeparateLayerAdaptor.h"
#include "ncbind.hpp"

namespace motion {

    class ShapeContainsFunc : public tTJSDispatch {
        int _l, _t, _w, _h;
    public:
        ShapeContainsFunc(int l, int t, int w, int h) : _l(l), _t(t), _w(w), _h(h) {}
        tjs_error FuncCall(
            tjs_uint32 flag, const tjs_char *membername, tjs_uint32 *hint,
            tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
            iTJSDispatch2 *objthis) override {
            if(membername) return TJS_E_MEMBERNOTFOUND;
            if(numparams < 2) return TJS_E_BADPARAMCOUNT;
            int x = static_cast<int>(param[0]->AsInteger());
            int y = static_cast<int>(param[1]->AsInteger());
            if(result) {
                *result = (x >= _l && x < _l + _w && y >= _t && y < _t + _h);
            }
            return TJS_S_OK;
        }
    };

    class Player {
        static std::shared_ptr<spdlog::logger> _logger() {
            return spdlog::get("plugin");
        }

    public:
        Player() = default;
        ~Player() { cleanupTempLayer(); }

        Player(const Player &) = delete;
        Player &operator=(const Player &) = delete;

        static bool getUseD3D() { return _useD3D; }
        static void setUseD3D(bool v) { _useD3D = v; }
        static bool getEnableD3D() { return _enableD3D; }
        static void setEnableD3D(bool v) { _enableD3D = v; }

        bool getPlaying() const { return _playing; }
        bool getAllplaying() const { return _allplaying; }
        ttstr getMotion() const { return _motion; }
        void setMotion(const ttstr &v) { _motion = v; }
        ttstr getChara() const { return _chara; }
        void setChara(const ttstr &v) { _chara = v; }
        tjs_int getTickCount() const { return _tickCount; }
        void setTickCount(tjs_int v) { _tickCount = v; }
        tjs_int getLastTime() const { return _lastTime; }
        void setLastTime(tjs_int v) { _lastTime = v; }
        tjs_real getSpeed() const { return _speed; }
        void setSpeed(tjs_real v) { _speed = v; }
        tjs_int getCompletionType() const { return _completionType; }

        // Yuzusoft affinesourcemotion.tjs canSync(): non-"emote" storage reads
        // `_player.loopTime`, "emote" reads `_player.animating`. Without these
        // members the KAG backup/env-transition chain throws "Member loopTime
        // does not exist" and the scene freezes (white screen) right after the
        // yuzulogo intro voice. loopTime = remaining/total loop time (ms, 0 is
        // a safe "not looping" value), animating = whether a motion is playing.
        // Yuzusoft 的 affinesourcemotion.tjs canSync()：非 emote 读 _player.loopTime，
        // emote 读 _player.animating。缺这两个成员会让 KAG 备份/环境转换链路抛
        // "Member loopTime does not exist" 并卡白屏。loopTime=循环时长(ms, 0=不在循环)，
        // animating=是否在播放。
        tjs_int getLoopTime() const { return _loopTime; }
        void setLoopTime(tjs_int v) { _loopTime = v; }
        bool getAnimating() const { return _animating; }
        void setAnimating(bool v) { _animating = v; }

        // Yuzusoft affinesourcemotion.tjs getOptions() enumerates a fixed list of
        // Player members (motion/chara/tickcount/speed/outline/...) to build the
        // option dictionary. `outline` (stroke width) is in that list; without it
        // the KAG storeFlags -> onStore -> getOptions chain throws "Member outline
        // does not exist" and the scene freezes white. Default 0 = no stroke.
        // Yuzusoft 的 getOptions() 会遍历 Player 固定成员表（motion/chara/tickcount/
        // speed/outline/...）生成选项字典；outline（描边宽度）在其中，缺失会让
        // storeFlags -> onStore -> getOptions 链路抛 "Member outline does not exist"
        // 并卡白屏。默认 0=无描边。
        tjs_int getOutline() const { return _outline; }
        void setOutline(tjs_int v) { _outline = v; }

        // Same getOptions() enumeration also reads `zpos` (Z-order/depth). Default 0.
        // getOptions() 的枚举也读 zpos（Z 序/深度）。默认 0。
        tjs_int getZpos() const { return _zpos; }
        void setZpos(tjs_int v) { _zpos = v; }

        // getOptions() also copies `_player.variableKeys` (array of dynamic-variable
        // key names) into the option dictionary; missing it throws a fatal
        // "Member variableKeys does not exist" (Senren Banka engine(2) verified).
        // Return an empty array — we keep no per-player dynamic variables.
        // getOptions() 还会把 _player.variableKeys（动态变量键名数组）拷进选项字典；
        // 缺失会抛 "Member variableKeys does not exist"（千恋万花 engine(2) 实证）。
        // 返回空数组——我们不维护 per-player 动态变量。
        iTJSDispatch2 *getVariableKeys() const {
            return TJSCreateArrayObject();
        }
        void setCompletionType(tjs_int v) { _completionType = v; }

        void play(const ttstr &motion, tjs_int all = 0) {
            // Record this as the most recent motion source so the D3DAdaptor's
            // captureCanvas(destLayer) callback can composite our frame onto the
            // game-supplied destination layer every frame.
            // 记录这是最近一次 motion 源，供 D3DAdaptor.captureCanvas(destLayer)
            // 回调把当前帧合成到游戏传入的目标层（每帧）。
            sLastDrawSource = this;
            if(auto l = _logger()) l->info("Player::play motion={} all={}", motion.AsStdString(), all);
            _motion = motion;
            _allplaying = (all != 0);
            _playWasCalled = true;
            _stopCommandSent = false;
            _tickCount = 0;
            _lastTime = 0;
            auto newStorage = ResourceManager::getLastLoadedPath();
            if(!newStorage.IsEmpty()) {
                _loadedStorage = newStorage;
            }
            _psbImagesCached = false;
            _composited = false;
            _psbCacheRetries = 0;
            cleanupTempLayer();
            buildButtonBounds(_loadedStorage);

            auto lower = motion.AsLowerCase();
            auto lowerStd = lower.AsStdString();
            _isTransition = (lowerStd.compare(0, 4, "show") == 0 ||
                             lowerStd.compare(0, 4, "hide") == 0);
            // Steady-state motions don't need to "play" or send STOP.
            // All other motions (transitions, effects, etc.) need to play
            // and eventually send a STOP command so the KAG conductor can advance.
            bool isSteadyState = (lower == TJS_W("normal") || lower == TJS_W("status"));
            _playing = !isSteadyState;
            _stopCommandSent = isSteadyState;
        }
        void stop() {
            _playing = false;
            _allplaying = false;
        }
        void progress(tjs_int delta) {
            _tickCount += delta;
            if(_tickCount > _lastTime) _lastTime = _tickCount;
            if(_playing && _tickCount >= 100) {
                _playing = false;
                _allplaying = false;
            }
        }
        void clear(iTJSDispatch2 *target, tjs_int color) {
            if(!target) return;
            tTJSVariant width, height;
            if(TJS_SUCCEEDED(target->PropGet(0, TJS_W("width"), nullptr, &width, target)) &&
               TJS_SUCCEEDED(target->PropGet(0, TJS_W("height"), nullptr, &height, target))) {
                tTJSVariant args[5] = {
                    tTJSVariant((tjs_int)0),
                    tTJSVariant((tjs_int)0),
                    width,
                    height,
                    tTJSVariant(color),
                };
                tTJSVariant *argv[] = { &args[0], &args[1], &args[2], &args[3], &args[4] };
                target->FuncCall(0, TJS_W("fillRect"), nullptr, nullptr, 5, argv, target);
            }
        }

        void draw(iTJSDispatch2 *target) {
            if(!target) return;
            // Same as play(): mark this player as the latest motion source so the
            // D3DAdaptor.captureCanvas callback can reach it. See play().
            // 与 play() 相同：标记本 player 为最新 motion 源，供 D3DAdaptor.captureCanvas
            // 回调取用。见 play() 注释。
            sLastDrawSource = this;
            const ttstr storage = _loadedStorage.IsEmpty() ? ResourceManager::getLastLoadedPath()
                                                           : _loadedStorage;
            if(storage.IsEmpty()) return;

            auto logger = _logger();
            try {
                if(!_psbImagesCached) {
                    cachePSBImages(storage, logger);
                }

                if(!_psbImages.empty()) {
                    drawPSBImages(target, storage, logger);
                } else {
                    drawFallback(target, storage, logger);
                }
            } catch(const std::exception &e) {
                if(logger) logger->error("draw: exception: {}", e.what());
            } catch(...) {
                if(logger) logger->error("draw: unknown exception");
            }
        }

    private:
        struct PSBImageEntry {
            std::string key;
            ttstr path;
            int left = 0;
            int top = 0;
            int width = 0;
            int height = 0;
            int opacity = 255;
            bool isBackground = false;
        };

        void cachePSBImages(const ttstr &storage, const std::shared_ptr<spdlog::logger> &logger) {
            _psbImages.clear();

            auto *media = PSB::GetGlobalPSBMedia();
            if(!media) {
                if(logger) logger->warn("cachePSBImages: PSBMedia is null");
                return;
            }

            auto allLayerPositions = media->getLayerPositions(storage.AsStdString());
            const std::string charaStr = _chara.AsStdString();
            const std::string storageStr = storage.AsStdString();

            size_t matchCount = 0;
            for(const auto &lp : allLayerPositions) {
                if(lp.sceneName == charaStr) ++matchCount;
            }

            if(logger) logger->info("cachePSBImages: {} layer positions for {} (chara={}, total={})",
                matchCount, storageStr, charaStr, allLayerPositions.size());

            if(matchCount > 0) {
                std::set<std::string> seenKeys;
                for(const auto &lp : allLayerPositions) {
                    if(lp.sceneName != charaStr) continue;
                    if(lp.srcPath.empty()) continue;
                    if(!lp.visible) continue;
                    if(lp.srcPath.find("_over") != std::string::npos) continue;
                    if(lp.srcPath.find("_unselect") != std::string::npos) continue;
                    if(lp.srcPath.find("_press") != std::string::npos) continue;

                    std::string pngKey = storageStr + "/" + lp.srcPath + "/pixel.png";

                    // Dedup by image path + position so same image at different
                    // positions (e.g. repeated ON/OFF buttons) is kept
                    std::string dedupKey = pngKey + "@" +
                        std::to_string(static_cast<int>(lp.left)) + "," +
                        std::to_string(static_cast<int>(lp.top));
                    if(seenKeys.count(dedupKey)) continue;
                    seenKeys.insert(dedupKey);

                    ttstr path = ttstr(TJS_W("psb://")) + ttstr(pngKey.c_str());

                    PSB::PSBMedia::CachedImageInfo info;
                    bool hasInfo = media->getImageInfo(pngKey, info);
                    int w = hasInfo ? info.width : lp.width;
                    int h = hasInfo ? info.height : lp.height;
                    if(w <= 0 || h <= 0) continue;

                    if(!TVPIsExistentStorage(path)) continue;

                    float cw, ch;
                    resolveCanvasSize(cw, ch);
                    const tjs_int origin = resolveCoordOrigin();

                    // Heuristic: a layer whose size matches the canvas is a
                    // full-screen background. Whatever the origin convention, it
                    // must fill the canvas, so pin its top-left to (0,0) instead
                    // of guessing center(top-left(-half)) or raw coords.
                    // 启发式：尺寸等于画布的图层是全屏背景。无论原点约定如何它都必须
                    // 铺满画布，因此直接固定其左上角为 (0,0)，不再猜中心/左上角。
                    const bool fullscreenBg =
                        cw > 0 && ch > 0 &&
                        std::abs(w - static_cast<int>(cw)) <= 1 &&
                        std::abs(h - static_cast<int>(ch)) <= 1;

                    PSBImageEntry img;
                    img.key = pngKey;
                    img.path = path;
                    if(fullscreenBg) {
                        img.left = _coordX;
                        img.top = _coordY;
                    } else if(origin == 1) {
                        // Top-left origin: the PSB position is already the layer's
                        // top-left corner, so no half-size / half-canvas offset.
                        // 左上角原点：PSB 坐标即图层左上角，无需半尺寸/半画布偏移。
                        img.left = _coordX + static_cast<int>(lp.left);
                        img.top = _coordY + static_cast<int>(lp.top);
                    } else {
                        // Center origin: PSB (0,0) is mid-canvas; shift by half the
                        // canvas, then center the image on that point for the top-left
                        // origin expected by Layer.operateRect.
                        // 中心原点：PSB (0,0) 即画布正中；先平移半画布，再以该点对中
                        // 图像，换算成 operateRect 需要的左上角坐标。
                        img.left = _coordX + static_cast<int>(cw / 2.0f) +
                            static_cast<int>(lp.left) - w / 2;
                        img.top = _coordY + static_cast<int>(ch / 2.0f) +
                            static_cast<int>(lp.top) - h / 2;
                    }
                    img.width = w;
                    img.height = h;
                    img.opacity = lp.opacity;
                    img.isBackground = (lp.layerName.find("/bg") != std::string::npos ||
                                        lp.srcPath.find("/bg") != std::string::npos);
                    _psbImages.push_back(std::move(img));
                }
            }

            if(_psbImages.empty() && allLayerPositions.empty()) {
                // Raw fallback: only when PSB hasn't been parsed yet (no layer
                // position data at all). Once parsed, scenes that don't match
                // the current chara simply have no images to render — their
                // content is managed by the game script's Layer system.
                auto entries = media->getImagesByPrefix(storage.AsStdString());
                if(logger) logger->info("cachePSBImages: fallback rawEntries={} retry={}",
                    entries.size(), _psbCacheRetries);
                if(entries.empty()) {
                    _psbCacheRetries++;
                    if(_psbCacheRetries >= 5) {
                        _psbImagesCached = true;
                        if(logger) logger->warn("cachePSBImages: giving up after {} retries", _psbCacheRetries);
                    }
                    return;
                }

                for(auto &e : entries) {
                    if(e.info.width <= 0 || e.info.height <= 0) continue;
                    const auto &k = e.key;
                    if(k.find("/pixel") == std::string::npos) continue;
                    if(k.size() < 4 || k.substr(k.size() - 4) != ".png") continue;
                    if(k.find("_over/") != std::string::npos) continue;
                    if(k.find("_unselect/") != std::string::npos) continue;

                    PSBImageEntry img;
                    img.key = k;
                    img.path = ttstr(TJS_W("psb://")) + ttstr(k.c_str());
                    img.left = e.info.left;
                    img.top = e.info.top;
                    img.width = e.info.width;
                    img.height = e.info.height;
                    img.opacity = e.info.opacity;
                    img.isBackground = (k.find("/bg/") != std::string::npos);
                    if(TVPIsExistentStorage(img.path)) {
                        _psbImages.push_back(std::move(img));
                    }
                }
            } else if(_psbImages.empty()) {
                // Layer positions exist but none match current chara — this is
                // normal (e.g. MSGWIN in main.psb has no static images; its
                // content is composited by the game script at runtime)
                _psbImagesCached = true;
                return;
            }

            _psbImagesCached = true;
            _psbCacheRetries = 0;

            std::stable_sort(_psbImages.begin(), _psbImages.end(),
                [](const PSBImageEntry &a, const PSBImageEntry &b) {
                    auto bgPriority = [](const PSBImageEntry &e) -> int {
                        if(!e.isBackground) return 2;
                        // "title/icon/bg" (main bg) comes first
                        if(e.key.find("/title/") != std::string::npos) return 0;
                        return 1;
                    };
                    return bgPriority(a) < bgPriority(b);
                });

            if(logger) {
                logger->info("PSB cache: {} images for {}", _psbImages.size(), storage.AsStdString());
                for(auto &img : _psbImages) {
                    logger->info("  {} @ ({},{}) {}x{} opacity={}",
                                 img.key, img.left, img.top, img.width, img.height, img.opacity);
                }
            }

            if(_buttonBounds.empty()) {
                buildButtonBounds(storage);
            }
        }

        void drawPSBImages(iTJSDispatch2 *target, const ttstr &storage,
                           const std::shared_ptr<spdlog::logger> &logger) {
            if(_psbImages.empty()) return;

            // Follow the game's OWN logic: render the motion into the layer the game
            // handed to Player::draw (the resolved real game layer, e.g. motionWorkLayer)
            // whose z-order the game script controls. We create NO synthetic overlay
            // layer. Re-composite every frame so the pixels survive any per-frame clear
            // the game may do on that layer.
            // 按游戏自身逻辑：把 motion 画进游戏传给 Player::draw 的目标层（resolveRealLayer
            // 解析出的真实游戏层，如 motionWorkLayer，其 z-order 由游戏脚本控制）。不创建任何
            // 合成 overlay。每帧整组重绘，避免被游戏对该层做的逐帧清空抹掉。
            iTJSDispatch2 *realLayer = resolveRealLayer(target);
            if(!realLayer) {
                if(logger) logger->warn("drawPSBImages: no real game layer to draw onto");
                return;
            }
            iTJSDispatch2 *tempParent = realLayer;

            if(logger) {
                logger->info("drawPSBImages: {} images, target={} realLayer={}",
                             _psbImages.size(),
                             static_cast<void*>(target),
                             static_cast<void*>(realLayer));
            }

            if(logger) logger->info("drawPSBImages: drew {} of {} images",
                                    compositeTo(realLayer, tempParent, logger),
                                    _psbImages.size());
        }

        // Composite every cached PSB image onto an explicit destination layer. It carries
        // NO _composited single-shot guard: both Player::draw and captureCanvas run it
        // every frame, and their destination layers are game-managed and may be cleared
        // between frames, so we always re-composite the whole image set. Returns how many
        // images were drawn.
        // 把所有缓存 PSB 图层绘制到指定的目标层。它不带 _composited 单次守卫：Player::draw
        // 与 captureCanvas 每帧都会调用它，而目标层由游戏管理、帧间可能被清空，因此每次都
        // 整组重绘。返回实际绘制的张数。
        int compositeTo(iTJSDispatch2 *dest, iTJSDispatch2 *tempParent,
                        const std::shared_ptr<spdlog::logger> &logger) {
            if(!dest || _psbImages.empty()) return 0;

            tTJSVariant faceVal(static_cast<tjs_int>(0)); // dfAlpha
            dest->PropSet(0, TJS_W("face"), nullptr, &faceVal, dest);

            int drawn = 0;
            for(size_t i = 0; i < _psbImages.size(); i++) {
                const auto &img = _psbImages[i];

                iTJSDispatch2 *temp = getOrCreateTempLayer(tempParent);
                if(!temp) {
                    if(logger) logger->warn("compositeTo: getOrCreateTempLayer failed for {}",
                                            img.key);
                    continue;
                }

                if(!tryLoadImage(temp, img.path)) {
                    if(logger) logger->warn("compositeTo: tryLoadImage failed for {}",
                                            img.path.AsStdString());
                    continue;
                }

                tTJSVariant wVar, hVar;
                temp->PropGet(0, TJS_W("imageWidth"), nullptr, &wVar, temp);
                temp->PropGet(0, TJS_W("imageHeight"), nullptr, &hVar, temp);
                int iw = static_cast<int>(wVar.AsInteger());
                int ih = static_cast<int>(hVar.AsInteger());
                if(iw <= 0 || ih <= 0) {
                    if(logger) logger->warn("compositeTo: bad image size {}/{} for {}",
                                            iw, ih, img.key);
                    continue;
                }

                int opacity = std::min(img.opacity, 255);
                if(opacity <= 0) continue;

                tTJSVariant opArgs[9] = {
                    tTJSVariant(static_cast<tjs_int>(img.left)),
                    tTJSVariant(static_cast<tjs_int>(img.top)),
                    tTJSVariant(temp, temp),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(iw)),
                    tTJSVariant(static_cast<tjs_int>(ih)),
                    tTJSVariant(static_cast<tjs_int>(2)),  // omAlpha
                    tTJSVariant(static_cast<tjs_int>(opacity)),
                };
                tTJSVariant *opArgv[] = { &opArgs[0], &opArgs[1], &opArgs[2],
                                          &opArgs[3], &opArgs[4], &opArgs[5],
                                          &opArgs[6], &opArgs[7], &opArgs[8] };
                try {
                    dest->FuncCall(0, TJS_W("operateRect"), nullptr, nullptr, 9, opArgv, dest);
                    drawn++;
                } catch(const std::exception &e) {
                    if(auto l = _logger()) l->warn("compositeTo: operateRect exception: {}", e.what());
                } catch(...) {
                    if(auto l = _logger()) l->warn("compositeTo: operateRect unknown exception");
                }
            }
            return drawn;
        }

        // Draw the current motion frame onto an arbitrary game-supplied destination
        // layer. Used by D3DAdaptor.captureCanvas(destLayer): the game passes the
        // target background/display layer so the motion lands exactly where the
        // game's own layering expects it (e.g. rendered UNDER the title menu instead
        // of a free-floating child layer above it). Re-composites on every call.
        // 把当前 motion 帧绘制到游戏传入的任意目标层。供 D3DAdaptor.captureCanvas(dest)
        // 使用——游戏传入目标背景/显示层，让 motion 恰好落在游戏自身层级期望的位置（例如
        // 渲染在标题菜单**之下**，而不是压在菜单之上的自由子层）。每次调用都整组重绘。
        void drawOnto(iTJSDispatch2 *target) {
            if(!target) return;
            sLastDrawSource = this;
            const ttstr storage = _loadedStorage.IsEmpty() ? ResourceManager::getLastLoadedPath()
                                                           : _loadedStorage;
            if(storage.IsEmpty()) return;
            auto logger = _logger();
            try {
                if(!_psbImagesCached) {
                    cachePSBImages(storage, logger);
                }
                if(_psbImages.empty()) return;
                iTJSDispatch2 *realLayer = resolveRealLayer(target);
                iTJSDispatch2 *tempParent = realLayer ? realLayer : target;
                int drawn = compositeTo(target, tempParent, logger);
                if(logger) logger->info("drawOnto: drew {} images onto capture target={}",
                                        drawn, static_cast<void*>(target));
            } catch(const std::exception &e) {
                if(logger) logger->error("drawOnto: exception: {}", e.what());
            } catch(...) {
                if(logger) logger->error("drawOnto: unknown exception");
            }
        }

        void drawFallback(iTJSDispatch2 *target, const ttstr &storage,
                          const std::shared_ptr<spdlog::logger> &logger) {
            if(logger) logger->info("drawFallback: storage={} chara={} motion={}",
                storage.AsStdString(), _chara.AsStdString(), _motion.AsStdString());

            std::vector<ttstr> candidates;
            if(!_chara.IsEmpty() && !_motion.IsEmpty())
                candidates.emplace_back(TJS_W("motion/") + _chara + TJS_W("/") + _motion);
            if(!_chara.IsEmpty()) {
                candidates.emplace_back(TJS_W("motion/") + _chara + TJS_W("/normal"));
                candidates.emplace_back(TJS_W("motion/") + _chara + TJS_W("/show"));
            }
            candidates.emplace_back(TJS_W("source/title/motion/show"));
            candidates.emplace_back(TJS_W("source/title/motion/normal"));
            candidates.emplace_back(TJS_W("source/title/icon/bg/pixel"));

            for(const auto &c : candidates) {
                if(c.IsEmpty()) continue;
                const ttstr path = TJS_W("psb://") + storage + TJS_W("/") + c;
                if(logger) logger->debug("drawFallback: trying {}", path.AsStdString());
                if(tryLoadImage(target, path)) {
                    if(logger) logger->info("drawFallback: loaded {}", path.AsStdString());
                    return;
                }
                if(tryLoadImage(target, path + TJS_W(".png"))) {
                    if(logger) logger->info("drawFallback: loaded {}.png", path.AsStdString());
                    return;
                }
            }
            if(logger) logger->warn("drawFallback: no image loaded for {}", storage.AsStdString());
        }

        iTJSDispatch2 *resolveRealLayer(iTJSDispatch2 *target) {
            if(!target) return nullptr;
            auto *adaptor = ncbInstanceAdaptor<SeparateLayerAdaptor>::GetNativeInstance(target);
            if(adaptor) {
                auto *rt = adaptor->getTarget();
                if(rt) return rt;
                auto *owner = adaptor->getOwner();
                return owner;
            }
            // A draw target that is not a real Layer (e.g. the Yuzusoft
            // D3DAdaptor shell has no window member) cannot host temp layers or
            // operateRect; route to the main window's primaryLayer instead.
            // 绘制目标若不是真实 Layer（如 Yuzusoft D3DAdaptor 空壳无 window 成员），
            // 无法承载临时层/operateRect；改路由到主窗口的 primaryLayer。
            tTJSVariant probe;
            if(TJS_FAILED(target->PropGet(0, TJS_W("window"), nullptr, &probe, target)) ||
               probe.Type() != tvtObject || !probe.AsObjectNoAddRef()) {
                if(TVPMainWindow) {
                    iTJSDispatch2 *winDsp = TVPMainWindow->GetOwnerNoAddRef();
                    if(winDsp) {
                        tTJSVariant plVar;
                        if(TJS_SUCCEEDED(winDsp->PropGet(0, TJS_W("primaryLayer"),
                                                         nullptr, &plVar, winDsp)) &&
                           plVar.Type() == tvtObject && plVar.AsObjectNoAddRef()) {
                            return plVar.AsObjectNoAddRef();
                        }
                    }
                }
            }
            return target;
        }

        // Resolve (window, parent) for a temp/display layer, independent of the
        // draw target's type. When the target is a D3DAdaptor shell (no window
        // member), fall back to the main window + its primaryLayer.
        // 解析创建临时/显示层所需的 (window, parent)，与绘制目标类型无关；当目标是
        // D3DAdaptor 空壳（无 window 成员）时回退到主窗口 + primaryLayer。
        // Resolve the animated scene's canvas width/height (from the primary layer).
        // Yuzusoft PSB files place layer coordinates relative to the canvas
        // center, and a canvas-sized layer is a full-screen background, so these
        // dimensions drive both coordinate mapping and the fullscreen heuristic.
        // 解析动画画布的宽/高（取自 primaryLayer）。Yuzusoft 的 PSB 以画布中心为坐标
        // 原点，尺寸等于画布的图层即为全屏背景，宽高同时用于坐标映射与全屏启发式。
        void resolveCanvasSize(float &cw, float &ch) {
            cw = 0;
            ch = 0;
            iTJSDispatch2 *probe =
                TVPMainWindow ? TVPMainWindow->GetOwnerNoAddRef() : nullptr;
            if(!probe) return;
            tTJSVariant plVar;
            if(TJS_SUCCEEDED(probe->PropGet(0, TJS_W("primaryLayer"), nullptr,
                                             &plVar, probe)) &&
               plVar.Type() == tvtObject) {
                iTJSDispatch2 *pl = plVar.AsObjectNoAddRef();
                if(pl) {
                    tTJSVariant wVar, hVar;
                    if(TJS_SUCCEEDED(
                           pl->PropGet(0, TJS_W("width"), nullptr, &wVar, pl)))
                        cw = static_cast<float>(wVar.AsReal());
                    if(TJS_SUCCEEDED(
                           pl->PropGet(0, TJS_W("height"), nullptr, &hVar, pl)))
                        ch = static_cast<float>(hVar.AsReal());
                }
            }
        }

        // Coordinate-origin convention for PSB layer positions.
        // -1 = unset (auto), 0 = center (0,0 = mid-canvas), 1 = top-left.
        // Read once from the "-psb_coord_origin=topleft|center|auto" command line
        // option; default is center (the Yuzusoft/kag-affine convention). Options
        // that are not honored on device are overridden here without touching git.
        // PSB 图层坐标的原点约定：-1=未设(auto)，0=中心(0,0=画布正中)，1=左上角。
        // 从命令行 "-psb_coord_origin=topleft|center|auto" 读取一次，默认中心原点
        //（Yuzusoft / kag-affine 的惯例）。
        tjs_int resolveCoordOrigin() {
            if(_coordOrigin >= 0) return _coordOrigin;
            _coordOrigin = 0; // default center (safe fallback)
            tTJSVariant v;
            if(TVPGetCommandLine(TJS_W("psb_coord_origin"), &v)) {
                ttstr s = ttstr(v).AsLowerCase();
                if(s == TJS_W("topleft") || s == TJS_W("top-left") ||
                   s == TJS_W("left")) {
                    _coordOrigin = 1;
                }
            }
            return _coordOrigin;
        }

        bool resolveWindowAndParent(iTJSDispatch2 *realLayer,
                                    tTJSVariant &windowVar,
                                    tTJSVariant &parentVar) {
            if(!realLayer) return false;

            bool haveWindow = TJS_SUCCEEDED(
                realLayer->PropGet(0, TJS_W("window"), nullptr, &windowVar, realLayer)) &&
                windowVar.Type() == tvtObject && windowVar.AsObjectNoAddRef();

            bool haveParent = false;
            // The real layer may itself be the primaryLayer, which has no
            // `primaryLayer` member; fall back to the main window in that case.
            // realLayer 可能是 primaryLayer 本身（无 primaryLayer 成员），此时回退主窗口。
            if(TJS_SUCCEEDED(realLayer->PropGet(0, TJS_W("primaryLayer"), nullptr,
                                                &parentVar, realLayer)) &&
               parentVar.Type() == tvtObject && parentVar.AsObjectNoAddRef()) {
                haveParent = true;
            }

            if((!haveParent || !haveWindow) && TVPMainWindow) {
                iTJSDispatch2 *winDsp = TVPMainWindow->GetOwnerNoAddRef();
                if(winDsp) {
                    if(!haveParent) {
                        if(TJS_SUCCEEDED(winDsp->PropGet(0, TJS_W("primaryLayer"),
                                                         nullptr, &parentVar, winDsp))) {
                            haveParent = parentVar.Type() == tvtObject &&
                                         parentVar.AsObjectNoAddRef();
                        }
                    }
                    if(!haveWindow) {
                        tTJSVariant wVar(winDsp, winDsp);
                        windowVar = wVar;
                        haveWindow = true;
                    }
                }
            }
            return haveWindow && haveParent;
        }

        iTJSDispatch2 *createChildLayer(const tTJSVariant &windowVar,
                                        const tTJSVariant &parentVar) {
            iTJSDispatch2 *global = TVPGetScriptDispatch();
            if(!global) return nullptr;

            tTJSVariant layerClassVar;
            global->PropGet(0, TJS_W("Layer"), nullptr, &layerClassVar, global);

            tTJSVariant ctorArgs[2] = { windowVar, parentVar };
            tTJSVariant *ctorArgv[] = { &ctorArgs[0], &ctorArgs[1] };
            iTJSDispatch2 *newLayer = nullptr;
            auto hr = layerClassVar.AsObjectNoAddRef()->CreateNew(
                0, nullptr, nullptr, &newLayer, 2, ctorArgv, layerClassVar.AsObjectNoAddRef());
            global->Release();
            if(TJS_FAILED(hr) || !newLayer) return nullptr;
            return newLayer;
        }

        iTJSDispatch2 *getOrCreateTempLayer(iTJSDispatch2 *realLayer) {
            if(_tempLayer) return _tempLayer;
            if(!realLayer) return nullptr;

            try {
                tTJSVariant windowVar, parentVar;
                if(!resolveWindowAndParent(realLayer, windowVar, parentVar)) {
                    return nullptr;
                }

                iTJSDispatch2 *newLayer = createChildLayer(windowVar, parentVar);
                if(!newLayer) return nullptr;

                tTJSVariant falseVar(false);
                newLayer->PropSet(TJS_MEMBERENSURE, TJS_W("visible"), nullptr, &falseVar, newLayer);

                _tempLayer = newLayer;
                return _tempLayer;
            } catch(...) {
                return nullptr;
            }
        }

        void cleanupTempLayer() {
            if(_tempLayer) {
                try {
                    _tempLayer->FuncCall(0, TJS_W("invalidate"), nullptr, nullptr, 0, nullptr, _tempLayer);
                } catch(...) {}
                _tempLayer->Release();
                _tempLayer = nullptr;
            }
        }

    public:
        void skipToSync() {}

        // Public accessor for the most recent motion source player, so the
        // D3DAdaptor/SeparateLayerAdaptor captureCanvas callbacks (defined in a
        // separate native class in main.cpp) can reach the active Player.
        // sLastDrawSource 的公开访问器：main.cpp 中独立的 D3DAdaptor/
        // SeparateLayerAdaptor captureCanvas 回调借此找到活动 Player。
        static Player *getLastDrawSource() { return sLastDrawSource; }

        // Public bridge used by D3DAdaptor/SeparateLayerAdaptor.captureCanvas in
        // main.cpp: composite the current motion frame onto a game-supplied target
        // layer (drawOnto stays private, this exposes a safe entry point).
        // captureCanvas 回调（main.cpp）使用的公开入口：把当前 motion 帧合成到游戏传入的
        // 目标层（drawOnto 保持私有，此处暴露安全入口）。
        void captureDrawTo(iTJSDispatch2 *target) { drawOnto(target); }

        void setDrawAffineTranslateMatrix(tjs_real, tjs_real, tjs_real, tjs_real, tjs_real, tjs_real) {}
        void setCoord(tjs_real x, tjs_real y) {
            _coordX = x;
            _coordY = y;
        }

        bool contains(tjs_int x, tjs_int y) const {
            // When called via getLayerGetter → motion.contains, check specific button
            if(!_pendingButtonName.empty()) {
                std::string btn = _pendingButtonName;
                _pendingButtonName.clear();
                auto it = _buttonBounds.find(btn);
                if(it == _buttonBounds.end()) return false;
                const auto &b = it->second;
                return x >= b.left && x < b.left + b.width &&
                       y >= b.top && y < b.top + b.height;
            }
            // Layer-level hit test (from onHitTest): true if within layer bounds
            return true;
        }

        bool containsForButton(const ttstr &buttonName, tjs_int x, tjs_int y) const {
            auto it = _buttonBounds.find(buttonName.AsStdString());
            if(it == _buttonBounds.end()) return false;
            const auto &b = it->second;
            return x >= b.left && x < b.left + b.width &&
                   y >= b.top && y < b.top + b.height;
        }

        iTJSDispatch2 *getCommandList() {
            iTJSDispatch2 *arr = TJSCreateArrayObject();
            if(_playWasCalled && !_playing && !_stopCommandSent) {
                _stopCommandSent = true;
                iTJSDispatch2 *cmd = TJSCreateDictionaryObject();
                if(cmd) {
                    tTJSVariant typeVal(TJS_W("stop"));
                    cmd->PropSet(TJS_MEMBERENSURE, TJS_W("type"), nullptr, &typeVal, cmd);
                    tTJSVariant nameVal(_motion);
                    cmd->PropSet(TJS_MEMBERENSURE, TJS_W("name"), nullptr, &nameVal, cmd);
                    tTJSVariant cmdVar(cmd, cmd);
                    arr->PropSetByNum(TJS_MEMBERENSURE, 0, &cmdVar, arr);
                    cmd->Release();
                    if(auto l = _logger()) l->info("Player::getCommandList → STOP motion={}", _motion.AsStdString());
                }
                _isTransition = false;
            }
            return arr;
        }

        iTJSDispatch2 *createLayerGetter(iTJSDispatch2 *self, const ttstr &name) const {
            // Track the button being queried so contains() can do per-button hit testing
            _pendingButtonName = name.AsStdString();

            iTJSDispatch2 *obj = TJSCreateDictionaryObject();
            if(!obj) return nullptr;

            auto set = [&](const tjs_char *n, const tTJSVariant &value) {
                obj->PropSet(TJS_MEMBERENSURE, n, nullptr,
                             const_cast<tTJSVariant *>(&value), obj);
            };

            int left = 0, top = 0, width = 1, height = 1;
            auto it = _buttonBounds.find(name.AsStdString());
            if(it != _buttonBounds.end()) {
                left = it->second.left;
                top = it->second.top;
                width = it->second.width;
                height = it->second.height;
            }

            set(TJS_W("visible"), tTJSVariant(true));
            set(TJS_W("originX"), tTJSVariant(static_cast<tjs_real>(0)));
            set(TJS_W("originY"), tTJSVariant(static_cast<tjs_real>(0)));
            set(TJS_W("left"), tTJSVariant(static_cast<tjs_real>(left)));
            set(TJS_W("top"), tTJSVariant(static_cast<tjs_real>(top)));
            set(TJS_W("x"), tTJSVariant(static_cast<tjs_real>(left)));
            set(TJS_W("y"), tTJSVariant(static_cast<tjs_real>(top)));
            set(TJS_W("flipX"), tTJSVariant(false));
            set(TJS_W("flipY"), tTJSVariant(false));
            set(TJS_W("zoomX"), tTJSVariant(static_cast<tjs_real>(1)));
            set(TJS_W("zoomY"), tTJSVariant(static_cast<tjs_real>(1)));
            set(TJS_W("slantX"), tTJSVariant(static_cast<tjs_real>(0)));
            set(TJS_W("slantY"), tTJSVariant(static_cast<tjs_real>(0)));
            set(TJS_W("angleDeg"), tTJSVariant(static_cast<tjs_real>(0)));
            set(TJS_W("opacity"), tTJSVariant(static_cast<tjs_int>(255)));

            if(self) {
                tTJSVariant selfValue(self);
                set(TJS_W("motion"), selfValue);
            } else {
                set(TJS_W("motion"), tTJSVariant());
            }

            iTJSDispatch2 *shape = TJSCreateDictionaryObject();
            if(shape) {
                auto setShape = [&](const tjs_char *n, const tTJSVariant &value) {
                    shape->PropSet(TJS_MEMBERENSURE, n, nullptr,
                                   const_cast<tTJSVariant *>(&value), shape);
                };
                setShape(TJS_W("type"), tTJSVariant(static_cast<tjs_int>(2)));
                setShape(TJS_W("x"), tTJSVariant(static_cast<tjs_real>(left)));
                setShape(TJS_W("y"), tTJSVariant(static_cast<tjs_real>(top)));
                setShape(TJS_W("l"), tTJSVariant(static_cast<tjs_real>(left)));
                setShape(TJS_W("t"), tTJSVariant(static_cast<tjs_real>(top)));
                setShape(TJS_W("w"), tTJSVariant(static_cast<tjs_real>(width)));
                setShape(TJS_W("h"), tTJSVariant(static_cast<tjs_real>(height)));

                auto *containsFunc = new ShapeContainsFunc(left, top, width, height);
                tTJSVariant containsVar(containsFunc, containsFunc);
                shape->PropSet(TJS_MEMBERENSURE, TJS_W("contains"), nullptr,
                               &containsVar, shape);
                containsFunc->Release();

                tTJSVariant shapeValue(shape);
                set(TJS_W("shape"), shapeValue);
                shape->Release();
            } else {
                set(TJS_W("shape"), tTJSVariant());
            }

            return obj;
        }

        void setVariable(const ttstr &name, const tTJSVariant &value) {
            _variables[name.AsStdString()] = value;
        }
        tTJSVariant getVariable(const ttstr &name) const {
            auto it = _variables.find(name.AsStdString());
            if(it != _variables.end()) return it->second;
            return tTJSVariant();
        }

        void buildButtonBounds(const ttstr &storage) {
            _buttonBounds.clear();
            if(_psbImages.empty()) return;
            auto logger = _logger();

            // Method 1: title.psb style — match _nomal/_normal in image key
            for(auto &img : _psbImages) {
                if(img.width <= 0 || img.height <= 0) continue;
                if(img.key.find("_nomal/") == std::string::npos &&
                   img.key.find("_normal/") == std::string::npos) continue;

                auto iconStart = img.key.rfind("/icon/");
                if(iconStart == std::string::npos) continue;
                auto nameStart = iconStart + 6;
                auto nameSuffix = img.key.find("_no", nameStart);
                if(nameSuffix == std::string::npos) continue;
                auto iconName = img.key.substr(nameStart, nameSuffix - nameStart);
                auto btnName = "bt_" + iconName;

                ButtonBounds bounds;
                bounds.left = img.left;
                bounds.top = img.top;
                bounds.width = img.width;
                bounds.height = img.height;
                _buttonBounds[btnName] = bounds;

                if(logger) logger->info("Button bounds: {} → ({},{}) {}x{}",
                                        btnName, bounds.left, bounds.top, bounds.width, bounds.height);
            }

            if(!_buttonBounds.empty()) return;

            // Method 2: config.psb style — use PSBMedia button info with
            // stored image keys and position proximity for matching
            auto *media = PSB::GetGlobalPSBMedia();
            if(!media) return;

            std::string charaStr = _chara.AsStdString();
            auto allBtnBounds = media->getButtonBounds(storage.AsStdString());

            for(auto &btn : allBtnBounds) {
                if(!charaStr.empty() && btn.sceneName != charaStr) continue;
                if(_buttonBounds.count(btn.buttonName)) continue;

                const PSBImageEntry *bestImg = nullptr;
                float bestDist = 1e9f;
                for(auto &img : _psbImages) {
                    if(img.width <= 0 || img.height <= 0) continue;

                    bool keyMatch = false;
                    // Primary: match by stored imageKey (srcPath from PSB tree)
                    if(!btn.imageKey.empty() &&
                       img.key.find(btn.imageKey) != std::string::npos) {
                        keyMatch = true;
                    }

                    if(!keyMatch) {
                        // Fallback: match by button name substring in image key
                        auto iconPos = img.key.rfind("/icon/");
                        if(iconPos == std::string::npos) continue;
                        std::string iconPart = img.key.substr(iconPos + 6);
                        std::string btnLower = btn.buttonName;
                        for(auto &c : btnLower) { if(c >= 'A' && c <= 'Z') c += 32; }
                        std::string iconLower = iconPart;
                        for(auto &c : iconLower) { if(c >= 'A' && c <= 'Z') c += 32; }
                        if(iconLower.find(btnLower) == std::string::npos) continue;
                        keyMatch = true;
                    }

                    // When same image appears at multiple positions, pick the
                    // one closest to the button's PSB position
                    float imgPsbX = static_cast<float>(img.left + img.width / 2) - _coordX;
                    float imgPsbY = static_cast<float>(img.top + img.height / 2) - _coordY;
                    float dist = std::abs(imgPsbX - btn.left) + std::abs(imgPsbY - btn.top);
                    if(!bestImg || dist < bestDist) {
                        bestImg = &img;
                        bestDist = dist;
                    }
                }

                if(bestImg) {
                    ButtonBounds bounds;
                    bounds.left = bestImg->left;
                    bounds.top = bestImg->top;
                    bounds.width = bestImg->width;
                    bounds.height = bestImg->height;
                    _buttonBounds[btn.buttonName] = bounds;

                    if(logger) logger->info("Button bounds: {} (img={}) → ({},{}) {}x{}",
                                            btn.buttonName, btn.imageKey,
                                            bounds.left, bounds.top,
                                            bounds.width, bounds.height);
                }
            }
        }

    private:
        static bool tryLoadImage(iTJSDispatch2 *target, const ttstr &path) {
            if(!TVPIsExistentStorage(path)) return false;
            tTJSVariant arg(path);
            tTJSVariant *argv[] = { &arg };
            return TJS_SUCCEEDED(
                target->FuncCall(0, TJS_W("loadImages"), nullptr, nullptr, 1, argv, target));
        }

        inline static bool _useD3D = false;
        inline static bool _enableD3D = false;

        // The most recent motion source player. The D3DAdaptor/SeparateLayerAdaptor
        // native class is decoupled from Player instances, so captureCanvas on it
        // uses this pointer to find the active Player and composite its frame onto
        // the game-supplied destination layer. Set in play()/draw()/drawOnto().
        // 最近的 motion 源 player。D3DAdaptor/SeparateLayerAdaptor 原生类与 Player 实例
        // 解耦，其上的 captureCanvas 借该指针找到活动 Player，把当前帧合成到游戏传入的
        // 目标层。在 play()/draw()/drawOnto() 中设置。
        inline static Player *sLastDrawSource = nullptr;

        bool _playing = false;
        bool _allplaying = false;
        bool _playWasCalled = false;
        bool _isTransition = false;
        bool _stopCommandSent = false;

        tjs_int _loopTime = 0;
        bool _animating = false;
        tjs_int _outline = 0;
        tjs_int _zpos = 0;
        mutable std::string _pendingButtonName;
        tjs_real _coordX = 0;
        tjs_real _coordY = 0;
        // -1 = origin convention not resolved yet (auto). See resolveCoordOrigin().
        // -1 = 尚未解析的原点约定（auto）。见 resolveCoordOrigin()。
        tjs_int _coordOrigin = -1;
        ttstr _motion;
        ttstr _chara;
        tjs_int _tickCount = 0;
        tjs_int _lastTime = 0;
        tjs_real _speed = 1.0;
        tjs_int _completionType = 0;
        ttstr _loadedStorage;

        bool _psbImagesCached = false;
        bool _composited = false;
        int _psbCacheRetries = 0;
        std::vector<PSBImageEntry> _psbImages;
        iTJSDispatch2 *_tempLayer = nullptr;

        struct ButtonBounds {
            int left = 0, top = 0, width = 0, height = 0;
        };
        std::unordered_map<std::string, ButtonBounds> _buttonBounds;

        std::unordered_map<std::string, tTJSVariant> _variables;
    };

} // namespace motion
