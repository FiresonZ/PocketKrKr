//
// Created by LiDon on 2025/9/15.
//
#pragma once

#include <vector>
#include <string>
#include <iterator>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include "ResourceManager.h"
#include "tjs.h"
#include "tjsArray.h"
#include "tjsDictionary.h"
#include "../core/base/StorageIntf.h"
#include "../core/base/ScriptMgnIntf.h"
#include "../core/base/SysInitIntf.h"
#include "../core/visual/WindowIntf.h"
#include "../core/visual/LayerIntf.h"
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
            _strClipActiveParent = -1;
            _tickCount = 0;
            _lastTime = 0;
            auto newStorage = ResourceManager::getLastLoadedPath();
            if(!newStorage.IsEmpty()) {
                _loadedStorage = newStorage;
            }
            _psbImagesCached = false;
            _composited = false;
            _psbCacheRetries = 0;
            _motionTracksLoaded = false;
            _motionTracks.clear();
            _motionNodes.clear();
            _captureActive = false;
            _lastFramePosX.clear();
            _lastFramePosY.clear();
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
        // M2 motion 级循环时长（ms），0=不循环。片头（yuzulogo/m2logo）在 K2 里是
        // 循环播到语音/脚本推进，不是到 lastTime 就停。读取自 PSB "loopTime"。
        tjs_int _motionLoopTime = 0;
        // Advance the motion clock. Returns true when the motion just finished
        // this frame (timeline exhausted and not looping) — the caller should
        // fire the game's onSync callback so the script can advance / replay the
        // next motion round (reference PlayerFrameProgress: on timeline end it
        // queues an onSync event; the title screen re-plays the entrance each
        // round via script, which is why "characters keep cycling" in K2).
        // 推进 motion 时钟。返回 true 表示本帧 motion 刚播完（时间线耗尽且不循环）
        // ——调用方应触发游戏的 onSync 回调，让脚本推进/重播下一轮 motion
        //（参考 PlayerFrameProgress：时间线结束时排队 onSync 事件；主界面靠脚本
        // 每轮重播入场，K2 里"角色持续切换"即由此而来）。
        bool progress(tjs_int delta) {
            // The PSB parser (PSBMedia.cpp ExtractFrameInfo) already converts the
            // raw 60fps FRAME counts in the mtn file into MILLISECONDS once at
            // parse time (yuzulogo last frame raw 241 ticks -> 4016 ms ≈ 4s,
            // m2logo back_white raw 91 ticks -> 1516 ms ≈ 1.5s), and the caller
            // feeds real elapsed ms. So advance the clock directly in ms — the
            // earlier "tick at 60fps" double-conversion slowed every animation by
            // ~17x (4016 ms timeline took 67s of real time).
            // PSB 解析器（PSBMedia.cpp ExtractFrameInfo）在解析时已把 mtn 里原始的
            // 60fps **帧数**统一换算成**毫秒**（yuzulogo 末帧原始 241 tick->4016 ms≈4s，
            // m2logo back_white 原始 91 tick->1516 ms≈1.5s），调用方传入的也是真实流逝
            // 毫秒。因此时钟直接用毫秒推进——此前"按 60fps 换算 tick"的二次换算让所有
            // 动画慢了约 17 倍（4016ms 的时间线要 67s 真实时间才播完）。
            _tickCount += delta;
            if(_tickCount > _lastTime) _lastTime = _tickCount;
            if(!_playing) return false;
            // Natural end of the motion: the last keyframe time across every
            // loaded track, NOT a hard-coded 100 ms. The old hard-coded cap made
            // logo animations stop after 100 ms no matter how long the timeline
            // was, so players felt "no animation" for assets whose keyframes lie
            // later (e.g. m2logo back_white reaching ~125 ms+).
            // 运动自然结束：取所有已加载 track 的最后一个关键帧时间，而不是写死的
            // 100 ms。旧的硬编码上限会让 logo 动画无论时间线多长都在 100 ms 后停，
            // 因此时间线靠后的资产（如 m2logo back_white 到 ~125 ms 后）看起来"没有动画"。
            tjs_int end = 0;
            if(_motionTracksLoaded) {
                for(const auto &tr : _motionTracks) {
                    if(!tr.frames.empty()) {
                        const tjs_int t = tr.frames.back().time;
                        if(t > end) end = t;
                    }
                }
            }
            if(end <= 0) end = 100; // timeline-less fallback / 无时间线兜底
            if(_tickCount >= end) {
                // Loop only when the motion declares loopTime > 0 (M2 logo intros):
                // wrap the clock so the timeline keeps replaying until the script
                // advances. Non-looping motions stop at end and return true so the
                // caller fires onSync (script advances / replays next round).
                // 仅当 motion 声明 loopTime > 0（M2 logo 片头）时循环：回绕时钟让时间线
                // 持续重播直到脚本推进。非循环 motion 在 end 处停止并返回 true，由调用方
                // 触发 onSync（脚本推进/重播下一轮）。
                if(_motionLoopTime > 0 && end > 0) {
                    _tickCount %= _motionLoopTime;
                    if(_tickCount < 0) _tickCount += _motionLoopTime;
                    return false;
                }
                _playing = false;
                _allplaying = false;
                return true; // finished this frame / 本帧播完
            }
            return false;
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
                // PSB archives load lazily on first resource access; force the
                // archive to be parsed BEFORE we query layer positions / motion
                // tracks, otherwise the very first frame sees no motion data and
                // loadMotionTracks() latches an empty result forever. Idempotent.
                // PSB 归档在首次访问资源时才懒加载；在查询图层坐标/motion 时间线
                // 之前强制其解析完成，否则首帧取不到 motion 数据，
                // loadMotionTracks() 会把空结果永久锁存。幂等。
                if(auto *media = PSB::GetGlobalPSBMedia()) {
                    media->ensureArchiveLoaded(storage.AsStdString());
                }
                if(!_psbImagesCached) {
                    cachePSBImages(storage, logger);
                }
                // Load motion frame time-lines once per play. Requires the archive
                // to be parsed (done above by cachePSBImages).
                // 每次 play 加载一次帧时间线（需要归档已解析，上面 cachePSBImages 已完成）。
                if(!_motionTracksLoaded) {
                    loadMotionTracks(storage);
                }

                if(!_motionTracks.empty() || !_psbImages.empty()) {
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

        // Same src→resource mapping as PSBMedia::MapSrcToResourcePath
        // ("src/title/bg" → "source/title/icon/bg") so we can build the psb:// key.
        // 与 PSBMedia::MapSrcToResourcePath 相同的 src→resource 映射，用于构造 psb:// key。
        static std::string MotionSrcToResource(const std::string &src) {
            if(src.size() > 4 && src.substr(0, 4) == "src/") {
                std::string rest = src.substr(4);
                auto slashPos = rest.find('/');
                if(slashPos != std::string::npos) {
                    return "source/" + rest.substr(0, slashPos) +
                           "/icon/" + rest.substr(slashPos + 1);
                }
            }
            return src;
        }

        // Recursively expand "motion/obj/submotion" src references inside a set
        // of tracks into the referenced submotion's own layer tracks. M2 scenes
        // (e.g. title_bg's "main" layer) reference a child motion (char_move)
        // whose layers carry the actual animation; without expansion the player
        // only sees one reference track whose src is "motion/..." and draws 0
        // images (title static / char never animated).
        // Flatten "motion/<obj>/<submotion>" layer references into real tracks by
        // pulling in the referenced submotion's own tracks (title char animation etc.).
        //
        // The old implementation re-scanned the whole list on every recursion, so the
        // SAME parent ref (whose src='motion/...' frame stays in the list) was merged
        // AGAIN at each depth — observed as "motion/title_bg/char_move merged 5 tracks"
        // × 9 and 46 tracks instead of 6. The fix keeps a persistent `expanded` set of
        // already-consumed refs and walks `tracks` with grow-safe indexing, so each
        // sub-motion is pulled in exactly once, while nested refs found inside freshly
        // appended tracks are still processed.
        //
        // 把 "motion/<对象>/<子motion>" 图层引用拍平成真实轨道：拉入被引用子 motion 的轨道
        //（title 立绘动画等）。
        // 旧实现每次递归都会重扫整个列表，导致同一条父引用（其 src='motion/...' 帧仍在列表里）
        // 在每个深度再次被合并——日志表现为 "motion/title_bg/char_move merged 5 tracks" ×9、
        // 轨道数 46 而非 6。修复：用持久的 `expanded` 集合记录已消费的引用，并用可增长索引遍历
        // `tracks`，保证每个子 motion 只展开一次，同时仍能处理新追加轨道内部嵌套的引用。
        void expandSubMotionRefs(PSB::PSBMedia *media,
                                 const std::string &storageStr,
                                 std::vector<PSB::PSBMedia::PSBMotionLayerTrack> &tracks,
                                 const std::shared_ptr<spdlog::logger> &logger) {
            if(!media) return;
            std::set<std::string> expanded; // refs already merged / 已合并的引用
            size_t i = 0;
            while(i < tracks.size()) {
                auto &tr = tracks[i];
                for(auto &f : tr.frames) {
                    if(f.src.size() < 7 || f.src.compare(0, 7, "motion/") != 0) continue;
                    if(expanded.count(f.src)) {
                        // Already consumed: neutralize so it won't be processed again.
                        // 已消费：清空 src，避免重复处理。
                        f.src.clear();
                        continue;
                    }
                    // f.src = "motion/<obj>/<submotion>"
                    std::string ref = f.src.substr(7);
                    auto slash = ref.find('/');
                    if(slash == std::string::npos) continue;
                    const std::string obj = ref.substr(0, slash);
                    const std::string submotion = ref.substr(slash + 1);
                    std::vector<PSB::PSBMedia::PSBMotionLayerTrack> sub =
                        media->getMotionTracks(storageStr, obj, submotion);
                    if(sub.empty() && submotion != "normal")
                        sub = media->getMotionTracks(storageStr, obj, "normal");
                    if(sub.empty()) {
                        if(logger) logger->warn(
                            "expandSubMotionRefs: no tracks for '{}' (obj='{}' sub='{}')",
                            f.src, obj, submotion);
                        f.src.clear();
                        continue;
                    }
                    if(logger) logger->info(
                        "expandSubMotionRefs: '{}' -> {}/{} merged {} tracks",
                        f.src, obj, submotion, sub.size());
                    expanded.insert(f.src);
                    f.src.clear(); // consumed / 已消费
                    tracks.insert(tracks.end(),
                                  std::make_move_iterator(sub.begin()),
                                  std::make_move_iterator(sub.end()));
                }
                ++i; // grow-safe: appended tracks are scanned by the same while loop
            }
        }

        // Expand "motion/<obj>/<sub>" sub-motion references inside the layered node tree.
        // The referencing node becomes a passive container; the target motion's node
        // subtree is appended with its roots re-parented to the referencing node, so
        // the referencing node's accumulated transform still wraps the sub- motion
        // (generic M2 child-motion). Deduped like the flat version; grow-safe walk.
        // 在分层节点树里展开 "motion/<对象>/<子motion>" 子运动引用。引用节点退化为
        // 被动容器，被引用 motion 的子树以引用节点为父追加进来，使引用节点的累加变换
        // 仍包住子运动（通用 M2 子运动）。与扁平版同样去重、增长安全遍历。
        void expandSubMotionNodes(PSB::PSBMedia *media,
                                  const std::string &storageStr,
                                  std::vector<PSB::PSBMedia::PSBMotionNode> &nodes,
                                  const std::shared_ptr<spdlog::logger> &logger) {
            if(!media) return;
            std::set<std::string> expanded;
            size_t i = 0;
            while(i < nodes.size()) {
                auto &node = nodes[i];
                for(auto &f : node.frames) {
                    if(f.src.size() < 7 || f.src.compare(0, 7, "motion/") != 0) continue;
                    if(expanded.count(f.src)) { f.src.clear(); continue; }
                    std::string ref = f.src.substr(7);
                    auto slash = ref.find('/');
                    if(slash == std::string::npos) { f.src.clear(); continue; }
                    const std::string obj = ref.substr(0, slash);
                    const std::string sub = ref.substr(slash + 1);
                    std::vector<PSB::PSBMedia::PSBMotionNode> child =
                        media->getMotionNodes(storageStr, obj, sub);
                    if(child.empty() && sub != "normal")
                        child = media->getMotionNodes(storageStr, obj, "normal");
                    if(child.empty()) {
                        if(logger) logger->warn(
                            "expandSubMotionNodes: no nodes for '{}' (obj='{}' sub='{}')",
                            f.src, obj, sub);
                        f.src.clear();
                        continue;
                    }
                    if(logger) logger->info(
                    "expandSubMotionNodes: '{}' -> {}/{} appended {} nodes",
                    f.src, obj, sub, static_cast<int>(child.size()));
                expanded.insert(f.src);
                const int refLaunchTime = f.time; // when the ref frame fires the sub-motion / 参考帧发起子运动的时刻
                const int subLoop = media->getMotionLoopTime(storageStr, obj, sub);
                const std::string subRef = f.src;
                f.src.clear();
                // Append the sub-subtree rooted under this referencing node,
                // remapping internal parent edges by the insertion offset.
                // 把子子树以本引用节点为父追加，按插入偏移重映射内部父子边。
                const int selfIdx = static_cast<int>(i);
                const int offset = static_cast<int>(nodes.size());
                for(auto &c : child) {
                    if(c.parentIndex == -1) c.parentIndex = selfIdx;
                    else c.parentIndex += offset;
                    // Reference child-player: the expanded sub-motion's content
                    // is driven by the PARENT motion node's activity, not the
                    // child's own trailing type-0 frame.
                    // 参考子播放器：展开的子运动内容由**父 motion 节点**的活动驱动，
                    // 而非子节点自己的末尾 type-0 帧。
                    c.submotionContent = true;
                    // Sub-motion child-clock: content nodes carry their own real
                    // clock (launch time + loop) instead of the parent's global
                    // tick, mirroring AetherKiri's child player time sync.
                    // 子运动子时钟：内容节点携带自己的真实时钟（发起时刻+循环），
                    // 而不是父的全局 tick，对应 AetherKiri 子播放器的时间同步。
                    c.subRefSrc = subRef;
                    c.subLaunchTime = refLaunchTime;
                    c.subLoopTime = subLoop;
                }
                    nodes.insert(nodes.end(),
                                 std::make_move_iterator(child.begin()),
                                 std::make_move_iterator(child.end()));
                }
                ++i;
            }
        }

        // Fetch the current motion's per-layer frame time-lines from PSBMedia.
        // 从 PSBMedia 取当前 motion 的每层帧时间线。
        void loadMotionTracks(const ttstr &storage) {
            _motionTracksLoaded = true;
            _motionTracks.clear();
            _motionNodes.clear();
            _motionLoopTime = 0;
            auto *media = PSB::GetGlobalPSBMedia();
            if(!media) return;
            const std::string storageStr = storage.AsStdString();
            const std::string charaStr = _chara.AsStdString();
            const std::string motionStr = _motion.AsStdString();
            _motionLoopTime = media->getMotionLoopTime(storageStr, charaStr, motionStr);
            _motionTracks = media->getMotionTracks(storageStr, charaStr, motionStr);
            if(_motionTracks.empty() && motionStr != "normal") {
                _motionTracks = media->getMotionTracks(storageStr, charaStr, "normal");
            }
            if(_motionTracks.empty() && motionStr != "show") {
                _motionTracks = media->getMotionTracks(storageStr, charaStr, "show");
            }
            // Layered node tree (parent→child) — the primary source for generic M2
            // accumulation. Falls back to the flat track list when no tree exists.
            // 分层节点树（父子关系）——通用 M2 累加的主数据源；无树时回退扁平轨道。
            _motionNodes = media->getMotionNodes(storageStr, charaStr, motionStr);
            if(_motionNodes.empty() && motionStr != "normal")
                _motionNodes = media->getMotionNodes(storageStr, charaStr, "normal");
            if(_motionNodes.empty() && motionStr != "show")
                _motionNodes = media->getMotionNodes(storageStr, charaStr, "show");
            // Expand "motion/<obj>/<sub>" references so the player can actually
            // draw the submotion's layers (title char animation etc.).
            // 展开 "motion/<对象>/<子motion>" 引用，让 Player 能真实画出子 motion 图层
            //（title 立绘动画等）。
            if(auto *m = PSB::GetGlobalPSBMedia()) {
                expandSubMotionRefs(m, storageStr, _motionTracks, _logger());
                if(!_motionNodes.empty()) {
                    expandSubMotionNodes(m, storageStr, _motionNodes, _logger());
                }
            }
            // M2 text-layout subtrees (str_* containers like m2logo's
            // "cheeseware"): every node — including these letters — is drawn
            // ORIGIN-ANCHORED via org = pos - M*(iconOrigin+ox, ...) (reference
            // updateLayersPhase3_VertexComputation). There is NO separate pen /
            // left-align path: each letter node animates its own coord (cx/cy)
            // keyframes, and the icon origin + content ox carry the anchor. The
            // str_clip container merely clips the subtree (type-7 shapeAABB).
            // M2 文本排版子树（str_* 容器，如 m2logo 的 "cheeseware"）：所有节点
            // 包括这些字母都按**原点锚定**绘制 org = pos - M*(iconOrigin+ox, ...)
            //（参考 updateLayersPhase3_VertexComputation）。不存在独立的笔位/左对齐
            // 路径：每个字母节点各自动画自己的 coord(cx/cy) 关键帧，图标原点与 content
            // 的 ox 承载锚点；str_clip 容器只负责裁剪子树（type-7 shapeAABB）。
        }

        // Evaluate the motion at the current clock and draw the active frame of every
        // layer (M2 timeline). Prefers the layered node tree (generic parent→child
        // accumulation); falls back to the flat per-track path when no tree exists.
        // 按当前时钟求值 motion，绘制每层生效帧（M2 时间轴）。优先用分层节点树（通用
        // 父子累加）；无节点树时回退扁平按轨道路径。
        int drawAnimated(iTJSDispatch2 *dest, iTJSDispatch2 *tempParent,
                         const std::shared_ptr<spdlog::logger> &logger) {
            if(!dest) return 0;
            if(!_motionNodes.empty()) return drawAnimatedTree(dest, tempParent, logger);
            if(_motionTracks.empty()) return 0;
            return drawAnimatedFlat(dest, tempParent, logger);
        }

        // ---------------------------------------------------------------------------
        // E-mote / spline / parameter helpers (ported from AetherKiri)
        // ---------------------------------------------------------------------------
        // ---------------------------------------------------------------------------
        // E-mote / 样条 / 参数工具（自 AetherKiri 移植）
        // ---------------------------------------------------------------------------

        // Bicubic Bernstein patch evaluation (reference AetherKiri
        // evaluateMotionBezierPatch / libkrkr2 sub_6990A0). `mesh` = 32 floats laid
        // out as 4 rows × 4 control points × (x,y); u,v ∈ [0,1].
        // 双三次 Bernstein 面片求值（参考 AetherKiri evaluateMotionBezierPatch /
        // libkrkr2 sub_6990A0）。`mesh` = 32 float，按 4 行 × 4 控制点 ×(x,y) 排布；
        // u,v ∈ [0,1]。
        static void evaluateMotionBezierPatch(const float *mesh, float u, float v,
                                              float &outX, float &outY) {
            if(!mesh) { outX = u; outY = v; return; }
            const float su = 1.0f - u;
            const float sv = 1.0f - v;
            const float bu[4] = {
                su * su * su,
                3.0f * su * su * u,
                3.0f * su * u * u,
                u * u * u,
            };
            const float bv[4] = {
                sv * sv * sv,
                3.0f * sv * sv * v,
                3.0f * sv * v * v,
                v * v * v,
            };
            float rowX[4], rowY[4];
            for(int row = 0; row < 4; ++row) {
                const float *p = mesh + row * 8;
                rowX[row] = p[0] * bu[0] + p[2] * bu[1] + p[4] * bu[2] + p[6] * bu[3];
                rowY[row] = p[1] * bu[0] + p[3] * bu[1] + p[5] * bu[2] + p[7] * bu[3];
            }
            outX = rowX[0] * bv[0] + rowX[1] * bv[1] + rowX[2] * bv[2] + rowX[3] * bv[3];
            outY = rowY[0] * bv[0] + rowY[1] * bv[1] + rowY[2] * bv[2] + rowY[3] * bv[3];
        }

        // Control-point rotation spline (reference AetherKiri sub_698454 /
        // evaluateControlPointCurve): samples the cp curve at `inputT` and returns
        // the point (outXY[0], outXY[1]) which callers use directly as (cosA, sinA).
        // 控制点旋转样条（参考 AetherKiri sub_698454 / evaluateControlPointCurve）：
        // 在 inputT 处采样 cp 曲线，返回点 (outXY[0], outXY[1])，调用方直接用作
        // (cosA, sinA)。
        static void evaluateCpCurve(double outXY[2],
                                    const PSB::PSBMotionCpCurve &cp,
                                    double inputT) {
            outXY[0] = 1.0; outXY[1] = 0.0;
            if(cp.t.size() < 2 || cp.x.size() < 4 || cp.y.size() < 4) return;
            int segIdx = 0, mainIdx = 0;
            for(size_t i = 1; i < cp.t.size(); ++i) {
                mainIdx += 3;
                if(cp.t[i] >= inputT) { segIdx = static_cast<int>(i) - 1; break; }
                segIdx = static_cast<int>(i) - 1;
            }
            if(segIdx < 0 || segIdx >= static_cast<int>(cp.s.size())) return;
            const double tStart = cp.t[segIdx];
            const double tEnd = (segIdx + 1 < static_cast<int>(cp.t.size()))
                ? cp.t[segIdx + 1] : tStart;
            const double localT = (tEnd != tStart) ? (inputT - tStart) / (tEnd - tStart) : 0.0;
            double param = localT;
            const auto &seg = cp.s[segIdx];
            if(!seg.x.empty() && seg.x.size() == seg.y.size()) {
                const double sx0 = seg.x[0];
                if(sx0 >= localT) {
                    param = seg.y[0];
                } else if(seg.x.back() <= localT) {
                    param = seg.y.back();
                } else {
                    int subIdx = 0;
                    for(size_t i = 1; i < seg.x.size(); ++i) {
                        if(seg.x[i] >= localT) { subIdx = static_cast<int>(i) - 1; break; }
                        subIdx = static_cast<int>(i) - 1;
                    }
                    if(subIdx >= 0 && subIdx + 1 < static_cast<int>(seg.x.size()) &&
                       subIdx + 1 < static_cast<int>(seg.y.size())) {
                        const double x0 = seg.x[subIdx], x1 = seg.x[subIdx + 1];
                        const double y0 = seg.y[subIdx], y1 = seg.y[subIdx + 1];
                        const double dx = x1 - x0;
                        if(dx != 0.0) {
                            const double u = (localT - x0) / dx;
                            const double p0 = (subIdx < static_cast<int>(seg.p.size())) ? seg.p[subIdx] : 0.0;
                            const double p1 = (subIdx + 1 < static_cast<int>(seg.p.size())) ? seg.p[subIdx + 1] : 0.0;
                            param = dx * dx * ((u * u * u - u) * p1 +
                                               ((1 - u) * (1 - u) * (1 - u) - (1 - u)) * p0) / 6.0
                                  + u * y1 + (1 - u) * y0;
                        }
                    }
                }
            }
            if(mainIdx >= 3 && mainIdx < static_cast<int>(cp.x.size()) &&
               mainIdx < static_cast<int>(cp.y.size())) {
                const double px0 = cp.x[mainIdx - 3], py0 = cp.y[mainIdx - 3];
                const double px1 = cp.x[mainIdx - 2], py1 = cp.y[mainIdx - 2];
                const double px2 = cp.x[mainIdx - 1], py2 = cp.y[mainIdx - 1];
                const double px3 = cp.x[mainIdx],   py3 = cp.y[mainIdx];
                const double u = 1.0 - param;
                outXY[0] = u * u * u * px0 + 3 * u * u * param * px1 +
                           3 * u * param * param * px2 + param * param * param * px3;
                outXY[1] = u * u * u * py0 + 3 * u * u * param * py1 +
                           3 * u * param * param * py2 + param * param * param * py3;
            }
        }

        // Parameterized clip time (reference detail::parameterizedClipTime): maps a
        // UI selector variable `value` in [rangeBegin, rangeEnd] onto the motion
        // timeline axis ([0, timelineEnd]) instead of the global clock.
        // 参数化 clip 时间（参考 detail::parameterizedClipTime）：把 UI 选择器变量
        // value（范围 [rangeBegin, rangeEnd]）映射到 motion 时间轴（[0, timelineEnd]），
        // 而非全局时钟。
        static double parameterizedClipTime(
            const PSB::PSBMotionParameter &parameter,
            double value) {
            const double range = parameter.rangeEnd - parameter.rangeBegin;
            if(std::abs(range) <= 0.0000001) return 0.0;
            const double minimum = std::min(parameter.rangeBegin, parameter.rangeEnd);
            const double maximum = std::max(parameter.rangeBegin, parameter.rangeEnd);
            double normalized = (std::clamp(value, minimum, maximum) - parameter.rangeBegin) / range;
            if(parameter.discretization) {
                const double rangeMagnitude = std::abs(range);
                const double integerSteps = std::round(rangeMagnitude);
                const double selectorSteps =
                    (integerSteps >= 1.0 && std::abs(rangeMagnitude - integerSteps) <= 0.0000001)
                        ? integerSteps : parameter.division;
                if(selectorSteps > 0.0)
                    normalized = std::round(normalized * selectorSteps) / selectorSteps;
            }
            normalized = std::clamp(normalized, 0.0, 1.0);
            return normalized * parameter.division;
        }

        // Generic M2 node-tree path: for each node in pre-order (parent before child),
        // evaluate its active frame's LOCAL pos (ox+cx, oy+cy) and opacity, then
        // accumulate top-down:
        //   worldPos      = parent.worldPos + localPos        (axis-aligned)
        //   worldOpacity  = parent.worldOpacity * localOpacity / 255
        // A `layout` / sub-motion container node (no "src/..." image) contributes its
        // transform to children but draws nothing itself — this is how a container's
        // slide/fade propagates to child layers (generic M2, mirrors libkrkr2.so's
        // Player_updateLayers).
        // 通用 M2 节点树路径：按先序（父先于子）求每个节点 active 帧的**局部**坐标
        //（ox+cx, oy+cy）与透明度，再自顶向下累加：
        //   世界坐标   = 父世界坐标 + 局部坐标（axis-aligned）
        //   世界透明度 = 父世界透明度 * 局部透明度 / 255
        // `layout`/子运动容器节点（无 "src/..." 图像）把自身变换传给子层但自身不画
        //——容器的滑入/淡入由此传给子层（通用 M2，对应 libkrkr2.so 的 Player_updateLayers）。
        int drawAnimatedTree(iTJSDispatch2 *dest, iTJSDispatch2 *tempParent,
                             const std::shared_ptr<spdlog::logger> &logger) {
            const tjs_int now = _tickCount; // ms clock, advanced by progress()
            float cw = 0, ch = 0;
            resolveCanvasSize(cw, ch);
            const tjs_int halfCw = static_cast<tjs_int>(cw / 2.0f);
            const tjs_int halfCh = static_cast<tjs_int>(ch / 2.0f);
            const std::string storageStr = _loadedStorage.IsEmpty()
                ? ResourceManager::getLastLoadedPath().AsStdString()
                : _loadedStorage.AsStdString();
            const int n = static_cast<int>(_motionNodes.size());
            // Motion timeline end (max keyframe time across every node). Used by the
            // end-of-motion hold for submotion content (title entrance persists).
            // 时间线结束（所有节点关键帧的最大时间）。用于 motion 播完时对子运动内容
            // 的静止保持（主界面入场保持）。
            tjs_int motionEnd = 0;
            for(const auto &nd : _motionNodes)
                for(const auto &f : nd.frames)
                    if(f.time > motionEnd) motionEnd = f.time;
            if(motionEnd <= 0) motionEnd = 100;
            // Per-node content-span END: the time of the FIRST frame after the node's
            // last VISIBLE frame (i.e. when its animation is done). Used to hold
            // sub-motion content after its parent finished folding (m2logo M stays
            // assembled) and after the title entrance's `main` ends. -1 = no content.
            // 每节点内容段**结束**：最后一个可见帧之后首帧的时刻（即动画完成的时刻）。
            // 用于父折叠完成后保持子运动内容（m2logo 的 M 保持成型）以及主界面 `main`
            // 结束后保持入场。为 -1 表示无可见内容。
            std::vector<tjs_int> nodeContentEnd(n, -1);
            for(int i = 0; i < n; i++) {
                const auto &frames = _motionNodes[i].frames;
                int lastVis = -1;
                for(int j = 0; j < static_cast<int>(frames.size()); j++) {
                    if(frames[j].visible) lastVis = j;
                }
                if(lastVis >= 0) {
                    if(lastVis + 1 < static_cast<int>(frames.size()))
                        nodeContentEnd[i] = frames[lastVis + 1].time;
                    else
                        nodeContentEnd[i] = frames[lastVis].time; // no trailing frame: ends at its own time
                }
            }
            std::vector<float> wx(n, 0.0f), wy(n, 0.0f);
            std::vector<float> wsx(n, 1.0f), wsy(n, 1.0f); // accumulated scale / 累加缩放
            std::vector<float> wa(n, 0.0f);          // accumulated angle (deg) / 累加角度
            std::vector<float> wslx(n, 0.0f), wsly(n, 0.0f); // accumulated skew / 累加斜切
            std::vector<bool> wfx(n, false), wfy(n, false); // accumulated flip (XOR) / 累加翻转
            std::vector<int> wo(n, 255);
            // Accumulated nearest non-white packedColors tint (4 corners) down the
            // parent chain (M2 text/group colors can live on a str_clip/comp container
            // and tint its descendants). White corners = inherit nothing.
            // 沿父链累计最近的"非白"packedColors 平涂（四角；M2 文本/组颜色可落在
            // str_clip/comp 容器上给整棵子树着色）。白=不继承。
            std::vector<std::array<std::uint32_t, 4>> wtint(
                n, std::array<std::uint32_t, 4>{
                    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu});
            std::vector<bool> vis(n, true);
            // Per-node accumulated WORLD linear 2x2 matrix (rotation×scale×flip), used to
            // transform each child's LOCAL offset into world space (gap-1 alignment to
            // libkrkr2: `pos = parentM·local + parentPos`). Roots and nodes under an
            // identity parent keep px=ox+cx (no change: background & yuzu letters intact).
            // 每个节点的累加**世界线性 2×2 矩阵**（旋转×缩放×翻转），用来把子节点的**局部
            // 偏移**变换到世界坐标（缺口①，对齐 libkrkr2：`pos = parentM·local + parentPos`）。
            // 根节点及恒等父矩阵下的节点保持 px=ox+cx 不变（背景与 yuzu 字母不受影响）。
            std::vector<double> wm11(n, 1.0), wm12(n, 0.0), wm21(n, 0.0), wm22(n, 1.0);
            int drawn = 0;
            // ---- ② Mesh/E-mote / ③ stencil / ④ child-clock / ⑤ param support state ----
            // ② Mesh/E-mote：per-node active frame pointer + source icon dims (the mesh
            // parent's clipW/clipH/originX/originY normalize each child into [0,1]^2
            // before evaluating the patch, mirroring AetherKiri updateLayers). Resolved
            // lazily via getImageInfo and cached for the frame.
            // ② Mesh/E-mote：每节点当前生效帧指针 + 源 icon 尺寸（mesh 父的
            // clipW/clipH/originX/originY 把每个子节点归一化到 [0,1]² 再求值面片，
            // 对齐 AetherKiri updateLayers）。通过 getImageInfo 惰性解析、帧内缓存。
            std::vector<const PSB::PSBMedia::PSBMotionFrame *> nodeActiveFrame(n, nullptr);
            std::vector<float> imgW(n, 0.0f), imgH(n, 0.0f);
            std::vector<float> imgOrX(n, 0.0f), imgOrY(n, 0.0f);
            std::vector<std::string> imgKey(n); // icon key the cached dims belong to
            std::unordered_map<std::string, std::array<float, 4>> iconMetaCache;
            bool anyMesh = false, anyStencil = false, anyParam = false, anyCp = false,
                 anySubClock = false, anyGround = false, anyMotionDt = false;
            // ③ stencil composite (type-12) bookkeeping: subtle set handled per node.
            // ③ stencil 合成（type-12）登记：具体集归属在节点循环里处理。
            std::vector<int> stencilGroupOf(n, -1);   // nearest stencil ancestor / 最近 stencil 祖先
            std::vector<bool> stencilMaskOf(n, false); // node referenced as a mask / 被引用为蒙版
            bool drewGroup = false, drewMask = false;
            // ⑤ parameterized clip TIME for parameterize-indexed nodes: mirror the
            // reference by reading the selector variable from the Player's own
            // _variables table (setVariable/getVariable are already TJS-bound).
            // ⑤ 参数化 clip 时间：参考模型从 Player 自身的 _variables 表读选择器变量
            //（setVariable/getVariable 已由 TJS 绑定）。
            std::vector<double> nodeTimeOverride(n, -1.0); // <0 = use global clock
            std::vector<PSB::PSBMotionParameter> motionParams;
            if(auto *media = PSB::GetGlobalPSBMedia()) {
                motionParams = media->getMotionParameters(
                    storageStr, _chara.AsStdString(), _motion.AsStdString());
            }
            if(!motionParams.empty()) anyParam = true;
            // Pre-pass for stencil: mark each node's nearest stencil-composite ancestor
            // and which nodes are referenced masks (label-match, like NodeTree).
            // 预扫描 stencil：标记每个节点的最近 stencil 合成祖先，以及被引用为蒙版的
            // 节点（按标签匹配，同 NodeTree）。
            {
                int activeStencil = -1;
                for(int i = 0; i < n; i++) {
                    const auto &nd = _motionNodes[i];
                    if(nd.hasStencil) {
                        anyStencil = true;
                        activeStencil = i;
                    }
                    stencilGroupOf[i] = activeStencil;
                    for(const int mIdx : nd.stencilMaskNodeIndices) {
                        if(mIdx >= 0 && mIdx < n) stencilMaskOf[mIdx] = true;
                    }
                }
            }
            // Save previous clip state for str_clip containers.
            // 保存 str_clip 容器之前的裁剪状态，绘制后恢复。
            ClipState prevClip;
            const bool clipSupported = readClip(dest, prevClip);
            // ③ stencil composite provisioning: when any type-12 composite with
            // authored masks exists, prepare + clear the two canvas-sized scratch
            // layers once per pass (content → group, masks → mask).
            // ③ stencil 合成供给：确有带蒙版的 type-12 合成组时，每轮预备并清空
            // 两块画布大小的离屏层（内容→组层、蒙版→蒙版层）。
            bool stencilActive = false;
            if(anyStencil) {
                for(int gi = 0; gi < n && !stencilActive; gi++) {
                    if(_motionNodes[gi].hasStencil &&
                       !_motionNodes[gi].stencilMaskNodeIndices.empty())
                        stencilActive = true;
                }
                if(stencilActive) {
                    iTJSDispatch2 *gl = getOrCreateStencilLayer(dest, false, cw, ch);
                    iTJSDispatch2 *ml = getOrCreateStencilLayer(dest, true, cw, ch);
                    if(gl && ml) { clear(gl, 0); clear(ml, 0); }
                    if(logger) logger->info(
                        "drawAnimatedTree stencil: composite active, scratch layers ready");
                }
            }
            for(int i = 0; i < n; i++) {
                const auto &node = _motionNodes[i];
                // Visibility semantics aligned to libkrkr2 / AetherKiri:
                // - A type-0 (invisible) frame hides the node for its time range.
                // - Node type 2 (structural transform group) stays ACTIVE even when
                //   its own frame is type-0, so its transform still reaches children.
                // - Sub-motion CONTENT nodes follow the PARENT motion node's activity
                //   (reference child player); after a non-looping motion finishes
                //   they HOLD their last content frame (title entrance persists).
                // 可见性语义对齐 libkrkr2 / AetherKiri：
                // - type-0（隐藏）帧在该时间段内隐藏节点。
                // - 节点 type 2（结构变换组）即使自身帧为 type-0 也保持 active，变换
                //   仍传给子层。
                // - **子运动内容**节点跟随父 motion 节点的活动（参考子播放器）；
                //   非循环 motion 播完后保持末内容帧（主界面入场不黑）。
                const auto &frames = node.frames;
                // Last VISIBLE frame of this node's timeline (any src, incl. layout
                // containers) — used to HOLD after the node's content ends so the
                // assembled m2logo M/2 persist and the title entrance stays on screen.
                // 节点时间线的末**可见**帧（任意 src，含 layout 容器帧）——内容结束后
                // 用它**保持**，让成型的 m2logo M/2 持续、主界面入场保持在屏。
                const PSB::PSBMedia::PSBMotionFrame *lastVisibleFrame = nullptr;
                for(const auto &f : frames) {
                    if(f.visible) {
                        if(!lastVisibleFrame || f.time > lastVisibleFrame->time) {
                            lastVisibleFrame = &f;
                        }
                    }
                }
                // Effective evaluation time for this node:
                //   (a) parameterize-indexed node → parameterized clip TIME (⑤);
                //   (b) expanded sub-motion CONTENT → its own child clock (④):
                //       runs from its launch time, loops by its own loopTime, and
                //       hides before launch (AetherKiri child-player time sync);
                //   (c) otherwise the global clock.
                // 本节点生效求值时间：
                //   (a) 带 parameterize 索引的节点 → 参数化 clip 时间（⑤）；
                //   (b) 展开的子运动内容 → 自身子时钟（④）：自发起时刻起播，按自身
                //       loopTime 循环，发起前隐藏（AetherKiri 子播放器时间同步）；
                //   (c) 否则全局时钟。
                tjs_int effNow = now;
                if(node.parameterizeIndex >= 0 && anyParam) {
                    if(node.parameterizeIndex < static_cast<int>(motionParams.size())) {
                        const auto &parameter = motionParams[node.parameterizeIndex];
                        double rawValue = 0.0;
                        const auto vit = _variables.find(parameter.id);
                        if(vit != _variables.end()) {
                            const tTJSVariant &mv = vit->second;
                            if(mv.Type() == tvtInteger) rawValue = static_cast<double>(mv.AsInteger());
                            else if(mv.Type() == tvtReal) rawValue = mv.AsReal();
                        }
                        if(parameter.division > 0.0) {
                            // parameterizedClipTime returns frames on the [0, division]
                            // axis; convert frames→ms (60fps) for the workspace clock.
                            // parameterizedClipTime 返回以 [0, division] 为轴的帧数；
                            // 换算 帧→ms（60fps）给本工作区时钟。
                            effNow = static_cast<tjs_int>(
                                parameterizedClipTime(parameter, rawValue) * 1000.0 / 60.0);
                            nodeTimeOverride[i] = effNow;
                        }
                    }
                } else if(node.submotionContent && !node.subRefSrc.empty()) {
                    // (b) child clock: submotion starts at its launch keyframe and
                    // loops with its own loopTime (a “real” sub-player clock instead
                    // of sampling the parent's global tick).
                    // (b) 子时钟：子运动自发起关键帧起播，用自身 loopTime 循环
                    //（"真"子播放器时钟，而非采样父的全局 tick）。
                    int subT = now - node.subLaunchTime;
                    if(subT < 0) { vis[i] = false; continue; } // pre-launch hidden / 未发起
                    if(node.subLoopTime > 0) subT %= node.subLoopTime;
                    effNow = subT;
                    anySubClock = true;
                }
                // Active frame = last frame with time <= effNow (per-frame evaluation).
                // 活跃帧 = time <= effNow 的最后一帧（逐帧求值）。
                const PSB::PSBMedia::PSBMotionFrame *afBase = nullptr;
                for(const auto &f : frames) {
                    if(f.time <= effNow) afBase = &f; else break;
                }
                if(!afBase) { vis[i] = false; continue; }
                nodeActiveFrame[i] = afBase; // for mesh parents / 供 mesh 父节点使用
                const PSB::PSBMedia::PSBMotionFrame *af = afBase;
                if(!af->visible) {
                    // Reference semantics (libkrkr2 updateLayers 0x6BB8F4 /
                    // AetherKiri): a type-0 (invisible) frame HIDES the node for its
                    // time range — EXCEPT:
                    //   * node type 2 (structural transform group): stays ACTIVE so
                    //     its transform still reaches children;
                    //   * sub-motion CONTENT nodes: driven by the PARENT motion
                    //     node's activity (reference child player), so while the
                    //     parent is active they hold their last visible frame —
                    //     otherwise the m2logo fold pieces would vanish mid-fold
                    //     (their own trailing type-0 frame is the submotion's
                    //     timeline end, not a hide); after the motion ends
                    //     (non-looping) they HOLD so the title entrance persists.
                    // 参考语义（libkrkr2 updateLayers 0x6BB8F4 / AetherKiri）：
                    // type-0（隐藏）帧在该时间段内**隐藏**节点——例外：
                    //   * 节点 type 2（结构变换组）：保持 active，变换仍传给子层；
                    //   * **子运动内容**节点：由父 motion 节点的活动驱动（参考子
                    //     播放器），父 active 期间保持末可见帧——否则 m2logo 折叠件
                    //     会在折叠中途消失（它们自己的末尾 type-0 帧是子运动时间线的
                    //     结束，不是隐藏）；motion 播完（非循环）后保持，主界面入场不黑。
                    // parentEnded/pastMotionEnd compare against PARENT-timeline times, so a
                // sub-motion CONTENT node with its own child clock must use the
                // GLOBAL clock here (the two timelines diverge once the sub loops).
                // parentEnded/pastMotionEnd 对比的是**父时间线**时刻，因此带子时钟的
                // 子运动内容节点在这里必须用**全局时钟**（子运动一循环，两条时间线就分叉）。
                const tjs_int holdNow =
                    (node.submotionContent && !node.subRefSrc.empty()) ? now : effNow;
                const bool parentActive =
                    (node.parentIndex < 0) || vis[node.parentIndex];
                const bool parentEnded =
                    (node.parentIndex >= 0) &&
                    (nodeContentEnd[node.parentIndex] >= 0) &&
                    (holdNow >= nodeContentEnd[node.parentIndex]);
                const bool pastMotionEnd =
                    (_motionLoopTime <= 0) && (holdNow >= motionEnd);
                    if(node.submotionContent) {
                        // Parent active: the sub-motion content is live; hold its last
                        // content once its OWN content ended (e.g. m2logo fold pieces
                        // whose only content frame is t=0). Before its own content
                        // starts (title_charall at t=0) it stays hidden.
                        // 父 active：子运动内容播放中；一旦**自身**内容结束就保持末内容帧
                        //（如 m2logo 折叠件只有 t=0 一个内容帧）。自身内容开始前（主界面
                        // title_charall 在 t=0）保持隐藏。
                        if(parentActive) {
                            if(lastVisibleFrame && af->time >= lastVisibleFrame->time) {
                                af = lastVisibleFrame;
                            } else {
                                vis[i] = false;
                                continue;
                            }
                        } else if((parentEnded || pastMotionEnd) && lastVisibleFrame) {
                            // Parent finished folding / motion finished: hold the content
                            // (m2logo M stays assembled; title entrance persists).
                            // 父折叠完成 / motion 播完：保持内容（m2logo 的 M 保持成型；
                            // 主界面入场保持）。
                            af = lastVisibleFrame;
                        } else {
                            vis[i] = false;
                            continue;
                        }
                    } else if(node.type == 2) {
                        // Structural group: keep active, draw nothing itself.
                        // 结构组：保持 active，自身不绘制。
                    } else {
                        // A regular (non-sub-motion) image node: honor its type-0 frame
                        // as a REAL hide. The reference (libkrkr2/AetherKiri) hides the
                        // node for the type-0 range instead of holding the previous
                        // visible frame. Holding was too broad: it kept `dummy_bar` (the
                        // m2logo thick line) visible past its t=200 hide, so the bar no
                        // longer "folds into" the M — it stayed as a separate bar while
                        // the fold fragments rotated. Sub-motion content and structural
                        // groups are handled above and keep their own hold semantics.
                        // 常规（非子运动）图像节点：把它的 type-0 帧当作**真正的隐藏**。
                        // 参考（libkrkr2/AetherKiri）在 type-0 区段隐藏节点，而不是保持前
                        // 一可见帧。此前保持太宽：把 m2logo 的粗横线 dummy_bar 在其 t=200
                        // 的隐藏帧后仍保持显示，导致它不再"折叠进 M"，而是作为一条单独的
                        // 横杠与折叠碎片同时出现。子运动内容与结构组在上方单独处理。
                        vis[i] = false;
                        continue;
                    }
                }
                // Frame interpolation between the active frame and the next frame.
                // M2 animates position/opacity smoothly between keyframes; taking
                // only the active frame makes characters pop in instantly and logos
                // look broken (reference has full bezier interpolation; linear is
                // our v1). Interpolate between ANY two content frames — including
                // `layout` / sub-motion CONTAINER frames (src not starting with
                // "src/") so container slides/fades propagate smoothly to children
                // instead of hopping keyframe to keyframe. An empty frame (vis=0)
                // stops the tween (holds the active values). The src (image) is
                // taken from the active frame; the image doesn't change mid-tween,
                // only position/opacity do.
                // 帧间插值：M2 在关键帧之间平滑过渡位置/透明度；只取 active 帧会让角色
                // 瞬间出现、logo 看起来破碎（参考有完整贝塞尔插值，v1 用线性）。对任意
                // 两个"有内容"帧之间插值——包括 src 不是 "src/" 的 layout/子运动容器帧，
                // 让容器的滑入/淡入平滑传给子层而不是在关键帧间跳变；空帧（vis=0）终止
                // 补间（保持当前值）。src（图像）取 active 帧，过渡期间只变位置/透明度。
                float interpOx = af->ox, interpOy = af->oy;
                float interpCx = af->cx, interpCy = af->cy;
                float interpOp = af->opacity;
                float interpSx = af->scaleX, interpSy = af->scaleY;
                float interpSlx = af->slantX, interpSly = af->slantY;
                float interpAngle = af->angle;
                // Interpolated 4-corner tint (M2 packedColors). Defaults to the
                // active frame's color; animates toward the next keyframe (the m2logo
                // thin line goes red→black) under the interpolation block below.
                // 插值后的四角 tint（M2 packedColors）。默认取活跃帧四角；在下
                // 方插值块内向下一关键帧渐变（m2logo 细线由红转黑）。
                std::array<std::uint32_t, 4> interpPacked = af->packedColors;
                if(af->visible) {
                    const PSB::PSBMedia::PSBMotionFrame *next = nullptr;
                    for(const auto &f : frames) {
                        if(f.time > effNow) { next = &f; break; }
                    }
                    // K2 semantics (PlayerUpdateLayerEval: crossfading = !invisible
                    // && interpolate): ONLY an ACTIVE frame of type 3 (interpolate)
                    // tweens toward the next keyframe. A type 2 (static) frame HOLDS
                    // its value until the next keyframe — it does NOT fade. This is
                    // what keeps yuzulogo's full-logo sheet invisible during the
                    // letter-by-letter intro (t=0 frame is type2 opa0 → hold 0),
                    // previously we interpolated it 0→255 and a faint "ghost" logo
                    // smeared over the whole intro.
                    // K2 语义（PlayerUpdateLayerEval: crossfading = !invisible &&
                    // interpolate）：只有 active 帧为 type=3（interpolate）才会向
                    // 下一关键帧插值；type=2（static）帧**保持当前值**直到下一关键帧，
                    // 不做淡入淡出。这就是 yuzulogo 整张 logo 在逐字母阶段应保持
                    // 不可见的关键（t=0 帧 type2 opa0 → 保持 0）；此前我们对它做
                    // 0→255 插值，导致整段 intro 叠上一层"印痕"幻影。
                    if(af->type == 3 && next && next->visible && next->time > af->time) {
                        // Raw linear progress over the frame; if the DEPARTURE frame
                        // carries M2 cubic-bezier easing (ccc), remap via the bezier so
                        // the leaf swing / letter slide decelerates smoothly like the
                        // reference instead of subjecting velocity through big keyframes.
                        // 帧内线性进度；若**出发帧**带 M2 三次贝塞尔缓动(ccc)，用贝塞尔重映射，
                        // 让叶子摆动/字母滑入像参考一样平滑减速，而不是在大关键帧间生硬直连。
                        float t = static_cast<float>(effNow - af->time) /
                                  static_cast<float>(next->time - af->time);
                        if(t < 0.0f) t = 0.0f; else if(t > 1.0f) t = 1.0f;
                        // Per-property easing aligned to the reference (AetherKiri
                        // interpolateSlots): EACH attribute eases with ITS OWN curve
                        // instead of reusing one "ccc" for everything.
                        //   position → RAW t (linear)
                        //   angle    → acc, else raw t
                        //   opacity  → occ, else fall back to ccc
                        //   scale    → zcc, else raw t
                        //   slant    → scc, else raw t
                        //   color    → ccc
                        // Previously we remapped position/angle/scale through "ccc",
                        // which mis-times the m2logo M-fold rotation ("M 折叠角乱/碎片
                        // 扭"下来) and the other tweens vs the reference.
                        // 逐属性缓动对齐参考（AetherKiri interpolateSlots）：每个属性用
                        // **各自曲线**，而不是把一个 "ccc" 复用于所有属性。
                        //   位置→原始 t（线性）、角度→acc（无则原始 t）、透明度→occ
                        //   （无则回退 ccc）、缩放→zcc（无则原始 t）、斜切→scc（无则原始 t）。
                        // 此前我们把 position/angle/scale 都套 "ccc"，导致 m2logo 的
                        // M 折叠角相对参考时序/形状错误（叠成"M 碎片乱扭"）。
                        float tAngle = af->hasAngleEasing
                            ? static_cast<float>(BezierEase(t, af->acX1, af->acY1, af->acX2, af->acY2))
                            : t;
                        float tOpa = af->hasOpacityEasing
                            ? static_cast<float>(BezierEase(t, af->ocX1, af->ocY1, af->ocX2, af->ocY2))
                            : (af->hasEasing
                               ? static_cast<float>(BezierEase(t, af->easeX1, af->easeY1, af->easeX2, af->easeY2))
                               : t);
                        float tScale = af->hasScaleEasing
                            ? static_cast<float>(BezierEase(t, af->zcX1, af->zcY1, af->zcX2, af->zcY2))
                            : t;
                        float tSlant = af->hasSlantEasing
                            ? static_cast<float>(BezierEase(t, af->sccX1, af->sccY1, af->sccX2, af->sccY2))
                            : t;
                        interpOx = af->ox + (next->ox - af->ox) * t;
                        interpOy = af->oy + (next->oy - af->oy) * t;
                        interpCx = af->cx + (next->cx - af->cx) * t;
                        interpCy = af->cy + (next->cy - af->cy) * t;
                        // ⑤ cp rotation spline: when the DEPARTURE frame carries a
                        // control-point curve (content "cp"), the position path is
                        // ROTATED by the sampled (cosA,sinA) at the eased t instead of
                        // a straight lerp (reference interpolatePosition69A4D4 /
                        // sub_698454). Applied to the node's coord (position) — cp only
                        // fires when the data authors it, otherwise pure lerp.
                        // ⑤ cp 旋转样条：出发帧带控制点曲线（content "cp"）时，位置路径
                        // 按采样点 (cosA,sinA) 在缓动 t 处**旋转**而非直线插值（参考
                        // interpolatePosition69A4D4 / sub_698454）。作用在节点坐标
                        //（位置）上——数据没有 cp 时退化为纯线性插值。
                        if(!af->cp.empty()) {
                            double rot[2] = {1.0, 0.0};
                            evaluateCpCurve(rot, af->cp, static_cast<double>(tAngle));
                            const double cosA = rot[0], sinA = rot[1];
                            const double dCx = static_cast<double>(next->cx - af->cx);
                            const double dCy = static_cast<double>(next->cy - af->cy);
                            interpCx = static_cast<float>(
                                af->cx + dCx * cosA - dCy * sinA);
                            interpCy = static_cast<float>(
                                af->cy + dCx * sinA + dCy * cosA);
                            anyCp = true;
                        }
                        interpOp = af->opacity + (next->opacity - af->opacity) * tOpa;
                        interpSx = af->scaleX + (next->scaleX - af->scaleX) * tScale;
                        interpSy = af->scaleY + (next->scaleY - af->scaleY) * tScale;
                        interpSlx = af->slantX + (next->slantX - af->slantX) * tSlant;
                        interpSly = af->slantY + (next->slantY - af->slantY) * tSlant;
                        // Angle interpolates along the 360° SHORTEST PATH
                        // (AetherKiri interpolateSlots / libkrkr2 sub_699AE4 at
                        // 0x699DEC): if the span exceeds 180°, wrap the target so
                        // the piece sweeps the short way. Without this the m2logo
                        // fold chain (node6 286°→0, node9 270°→0) spins ~286°/270°
                        // (almost a full turn) while the reference folds only
                        // 74°/90° — the "M 不是横线平滑弯折、乱转" symptom.
                        // 角度沿 360°**最短路径**插值（AetherKiri interpolateSlots /
                        // libkrkr2 sub_699AE4@0x699DEC）：跨度超过 180° 时回绕目标，
                        // 让部件只扫过短弧。否则 m2logo 折叠链（node6 286°→0、
                        // node9 270°→0）会转 ~286°/270°（几乎一整圈），而参考只折
                        // 74°/90°——即"M 不是横线平滑弯折、像乱转"的现象。
                        {
                            float curA = af->angle;
                            float nxtA = next->angle;
                            if(curA >= nxtA) {
                                if(curA - nxtA > 180.0f) nxtA += 360.0f;
                            } else {
                                if(nxtA - curA > 180.0f) nxtA -= 360.0f;
                            }
                            // Fold DIRECTION — REVERTED. Real-device comparison after the
                            // previous "flat→steep" reversal made the m2logo fold WORSEN
                            // ("很多竖线变换成 m"): the tall bars icon25/27/28 at fold start
                            // (parent angles 90/286/73/270) are rotated ~flat = a horizontal
                            // thick bar, and they steepen to vertical strokes forming the M.
                            // Reverting to the data's own direction (steep→flat for the
                            // parents, i.e. horizontal-bar→M) restores the correct sweep.
                            // Math for the anchor (sub_6BC4F0: org = pos − M·(icon.originX+
                            // ox, icon.originY+oy)) already matches AetherKiri, so no change
                            // is made to the pivot here.
                            // 折叠**方向**——已撤销。上次"平→斜"反转在真机让 m2logo 折叠更
                            // 糟（"很多竖线变换成 m"）：竖条 icon25/27/28 在折叠开始（父角
                            // 90/286/73/270）被转到接近水平＝一条**水平粗线**，随折叠变竖直
                            // 成 M。恢复数据自身方向（父角斜→平，即"横线→M"）才是正确扫描。
                            // 锚点数学（sub_6BC4F0：org = pos − M·(icon.originX+ox, oy)）已与
                            // AetherKiri 一致，此处不改枢轴。
                            interpAngle = curA + (nxtA - curA) * tAngle;
                            if(interpAngle < 0.0f) interpAngle += 360.0f;
                            else if(interpAngle >= 360.0f) interpAngle -= 360.0f;
                        }
                        // Interpolate the flat tint color toward the next keyframe. The
                        // color eases with the "ccc" curve (AetherKiri: color→ccc), e.g.
                        // the m2logo thin line animates red→black over its keyframes.
                        // 把纯色 tint 向下一关键帧插值。颜色用 "ccc" 曲线缓动（AetherKiri：
                        // color→ccc），如 m2logo 细线在其关键帧间由红转黑。
                        {
                            float tc = af->hasEasing
                                ? static_cast<float>(BezierEase(t, af->easeX1, af->easeY1,
                                                                af->easeX2, af->easeY2))
                                : t;
                            auto lerpc = [](tjs_uint32 a, tjs_uint32 b, float r) {
                                return static_cast<tjs_uint32>(a + (b - a) * r);
                            };
                            // interpolate all 4 corners, not just corner 0 / 四角全部插值
                            for(int ci = 0; ci < 4; ++ci) {
                                const std::uint32_t a0 = af->packedColors[ci];
                                const std::uint32_t n0 = next->packedColors[ci];
                                interpPacked[ci] =
                                    (lerpc((a0 >> 24) & 0xffu, (n0 >> 24) & 0xffu, tc) << 24) |
                                    (lerpc((a0 >> 16) & 0xffu, (n0 >> 16) & 0xffu, tc) << 16) |
                                    (lerpc((a0 >> 8) & 0xffu, (n0 >> 8) & 0xffu, tc) << 8) |
                                    (lerpc((a0 >> 0) & 0xffu, (n0 >> 0) & 0xffu, tc) << 0);
                            }
                        }
                    }
                }
                // Sub-motion content may keep rendering after the parent motion
                // node's own content span ended (fold done / entrance `main` done) —
                // the m2logo M stays assembled and the title entrance holds this way.
                // 子运动内容可在父 motion 节点自身内容段结束后继续渲染（折叠完成 /
                // 入场 `main` 结束）——m2logo 的 M 保持成型、主界面入场保持都靠它。
                const bool parentOn = (node.parentIndex >= 0)
                    ? (vis[node.parentIndex] ||
                       (node.submotionContent &&
                        nodeContentEnd[node.parentIndex] >= 0 &&
                        effNow >= nodeContentEnd[node.parentIndex]))
                    : true;
                if(!parentOn) { vis[i] = false; continue; } // hidden parent hides subtree
                const float baseX = (node.parentIndex >= 0) ? wx[node.parentIndex] : 0.0f;
                const float baseY = (node.parentIndex >= 0) ? wy[node.parentIndex] : 0.0f;
                const int baseOp = (node.parentIndex >= 0) ? wo[node.parentIndex] : 255;
                const float baseSx = (node.parentIndex >= 0) ? wsx[node.parentIndex] : 1.0f;
                const float baseSy = (node.parentIndex >= 0) ? wsy[node.parentIndex] : 1.0f;
                const float baseAngle = (node.parentIndex >= 0) ? wa[node.parentIndex] : 0.0f;
                // Accumulate flip as XOR through the parent chain (reference
                // Player_Rendering_Architecture: node.flipX ^= parent.flipX). A
                // parent container's flip must mirror the whole subtree, and two
                // flips on the same axis cancel — ignoring it made the yuzusoft
                // leaf render with the wrong handedness ("叶子方向反了").
                // 沿父链 XOR 累加翻转（参考 Player_Rendering_Architecture：
                // node.flipX ^= parent.flipX）。父容器的翻转要镜像整棵子树，同一轴
                // 两次翻转相消——此前忽略它导致 yuzusoft 绿叶朝向相反（"叶子方向反了"）。
                const int inh = node.inheritMask;
                const bool pOn = (node.parentIndex >= 0);
                // libkrkr2 gates each transform attribute's INHERITANCE per-node via
                // `inheritMask` (bit 0x004 flipX, 0x008 flipY, 0x010 angle, 0x020 scaleX,
                // 0x040 scaleY). Bit SET = accumulate the parent contribution (XOR flips,
                // add angle, multiply scale); bit CLEAR = use the node's own value only.
                // This is how a child can deliberately NOT inherit its ancestor's
                // transform (e.g. m2logo letters excluding str_clip's clip-region scale).
                // 参考 libkrkr2 用 `inheritMask` 逐节点门控各变换属性的**继承**（bit
                // 0x004 flipX、0x008 flipY、0x010 angle、0x020 scaleX、0x040 scaleY）。
                // 位置=1 则累加父贡献（flip XOR、angle 相加、scale 相乘）；=0 只用自己的值。
                // 这正是子节点可刻意不继承祖先变换的机制（如 m2logo 字母排除 str_clip 的
                // 裁剪窗口缩放）。
                const bool effFx = (inh & 0x004) ? (af->flipX ^ (pOn ? wfx[node.parentIndex] : false))
                                                 : af->flipX;
                const bool effFy = (inh & 0x008) ? (af->flipY ^ (pOn ? wfy[node.parentIndex] : false))
                                                 : af->flipY;
                // Position: the node's local **coord** (cx, cy) is transformed by the
                // PARENT's world matrix and added to the parent's world pos, per libkrkr2
                // `pos = parentM·local + parentPos`. This is gap-1 alignment so a
                // rotated/scaled parent correctly carries its children. Roots and nodes
                // under an identity parent reduce to px=cx (no change to bg/letters).
                // NOTE (anchoring): content "ox"/"oy" is NOT added to the position —
                // it is the texture's anchor/pivot offset applied at draw time via
                // org = pos - M*(iconOrigin+ox, iconOriginY+oy) (reference
                // updateLayersPhase3_VertexComputation). Adding ox to the position
                // double-counted it and moved the whole sprite (e.g. yuzusoft's leaf
                // ox=91 drifted / looked mirrored while swaying).
                // 位置：节点的局部**坐标**(cx, cy) 经**父节点世界矩阵**变换后加到父世界坐标
                //（libkrkr2：`pos = parentM·local + parentPos`）。这是缺口①对齐，让旋转/
                // 缩放的父节点正确带动子节点。根节点及恒等父矩阵下退化为 px=cx。
                // 注意（锚点）：content 的 "ox"/"oy" **不进位置**——它是纹理的旋转**枢轴热区**
                //（下面 Round-3 绕 (ax+ox·scale) 旋转，如 yuzusoft 叶子 ox=91）。把 ox
                // 加进位置等于**双计**，会让整个精灵漂移（如叶子 ox=91 摆动时偏移/呈镜像）。
                const float loX = interpCx;
                const float loY = interpCy;
                // ② E-mote mesh position deformation. Ported from AetherKiri
                // updateLayers sub_69AE74 (0x6BB714): when the PARENT is a mesh
                // (meshType==1) and its meshSyncChildMask bit 1 is set, each child's
                // LOCAL position is normalized into the parent's source-icon box
                // (u,v ∈ [0,1]), evaluated on the parent's bicubic patch, then mapped
                // back to local space — BEFORE the parent world-matrix transform below.
                // Angle (gradient) and scale (jacobian) deformation follow the same
                // gate/flags, then fold into the accumulated angle/scale.
                // ② E-mote 网格位置变形。移植自 AetherKiri updateLayers sub_69AE74
                // (0x6BB714)：父节点是网格（meshType==1）且 meshSyncChildMask 位 1
                // 置位时，把每个子节点的**局部位置**归一化到父的源 icon 盒（u,v ∈
                // [0,1]）、在父的双三次面片上求值、再映回局部空间——发生在下方的父
                // 世界矩阵变换**之前**。角度（梯度）与缩放（Jacobian）变形走同一
                // 门控/标志，随后折进累加角度/缩放。
                float defCx = loX, defCy = loY;
                float meshAngleDelta = 0.0f, meshScaleFactor = 1.0f;
                if(pOn) {
                    const auto &pnode = _motionNodes[node.parentIndex];
                    const PSB::PSBMedia::PSBMotionFrame *paf = nodeActiveFrame[node.parentIndex];
                    const bool hasSrc = (af->src.size() > 4 && af->src.compare(0, 4, "src/") == 0);
                    if(paf && hasSrc && pnode.meshType == 1 &&
                       (pnode.meshSyncChildMask & 0x1) &&
                       paf->meshControlPoints.size() >= 32) {
                        // Parent icon dims normalize the child into [0,1]^2
                        // (reference: normX=(posX+originX)/clipW). Lazily resolved
                        // (getImageInfo) and frame-cached.
                        // 用父 icon 尺寸把子节点归一化到 [0,1]²（参考：
                        // normX=(posX+originX)/clipW）。getImageInfo 惰性解析并帧内缓存。
                        float pw = imgW[node.parentIndex], ph = imgH[node.parentIndex];
                        float porX = imgOrX[node.parentIndex], porY = imgOrY[node.parentIndex];
                        if(pw <= 0.0f || ph <= 0.0f) {
                            std::string psrc = paf->src;
                            if(psrc.size() > 4 && psrc.compare(0, 4, "src/") == 0) {
                                const std::string pres = MotionSrcToResource(psrc);
                                const std::string pkey = storageStr + "/" + pres + "/pixel.png";
                                // Re-resolve when the parent's icon changed (cache is
                                // keyed by parent index + its icon key).
                                // 父 icon 变化时重新解析（缓存按父索引+其 icon 键）。
                                if(imgKey[node.parentIndex] != pkey) {
                                    auto cit = iconMetaCache.find(pkey);
                                    if(cit == iconMetaCache.end() && PSB::GetGlobalPSBMedia()) {
                                        PSB::PSBMedia::CachedImageInfo gi;
                                        if(PSB::GetGlobalPSBMedia()->getImageInfo(pkey, gi)) {
                                            cit = iconMetaCache.emplace(
                                                pkey, std::array<float, 4>{
                                                    static_cast<float>(gi.width), static_cast<float>(gi.height),
                                                    gi.originX, gi.originY}).first;
                                        }
                                    }
                                    if(cit != iconMetaCache.end()) {
                                        // map value is array<float,4> → access via ->second.
                                        // map 的值是 array<float,4>，需经 ->second 取角标。
                                        pw = cit->second[0]; ph = cit->second[1];
                                        porX = cit->second[2]; porY = cit->second[3];
                                        imgW[node.parentIndex] = pw; imgH[node.parentIndex] = ph;
                                        imgOrX[node.parentIndex] = porX; imgOrY[node.parentIndex] = porY;
                                        imgKey[node.parentIndex] = pkey;
                                    }
                                }
                            }
                            if(pw <= 0.0f) pw = pnode.width > 0 ? static_cast<float>(pnode.width) : 1.0f;
                            if(ph <= 0.0f) ph = pnode.height > 0 ? static_cast<float>(pnode.height) : 1.0f;
                        }
                        const float u = (loX + porX) / pw;
                        const float v = (loY + porY) / ph;
                        float ex = u, ey = v;
                        evaluateMotionBezierPatch(paf->meshControlPoints.data(), u, v, ex, ey);
                        defCx = ex * pw - porX;
                        defCy = ey * ph - porY;
                        anyMesh = true;
                        // Angle deformation from the mesh gradient (meshSyncChildMask
                        // bit 2, inheritMask bit 0x010) — 4-eps sampling, averaged.
                        // 由网格梯度变形角度（meshSyncChildMask 位 2、inheritMask 位
                        // 0x010）——4 个 eps 邻域采样后取平均。
                        if((pnode.meshSyncChildMask & 0x2) && (inh & 0x010)) {
                            const float eps = 0.0001f;
                            const float *mp = paf->meshControlPoints.data();
                            float x1, y1, x2, y2, x3, y3, x4, y4;
                            evaluateMotionBezierPatch(mp, u - eps, v, x1, y1);
                            evaluateMotionBezierPatch(mp, u + eps, v, x2, y2);
                            evaluateMotionBezierPatch(mp, u, v - eps, x3, y3);
                            evaluateMotionBezierPatch(mp, u, v + eps, x4, y4);
                            const double a1 = std::atan2(static_cast<double>(y3 - y4),
                                                         static_cast<double>(x4 - x3));
                            const double a2 = std::atan2(static_cast<double>(x2 - x1),
                                                         static_cast<double>(y2 - y1));
                            meshAngleDelta = static_cast<float>((a1 + a2) * 0.5 * 360.0 / 6.28318531);
                        }
                        // Scale deformation from the mesh jacobian (meshSyncChildMask
                        // bit 4, inheritMask bits 0x020/0x040).
                        // 由网格 Jacobian 变形缩放（meshSyncChildMask 位 4、inheritMask
                        // 位 0x020/0x040）。
                        if((pnode.meshSyncChildMask & 0x4) && (inh & 0x060)) {
                            const float eps = 0.0001f;
                            const float *mp = paf->meshControlPoints.data();
                            float x1, y1, x2, y2, x3, y3, x4, y4;
                            evaluateMotionBezierPatch(mp, u - eps, v, x1, y1);
                            evaluateMotionBezierPatch(mp, u + eps, v, x2, y2);
                            evaluateMotionBezierPatch(mp, u, v - eps, x3, y3);
                            evaluateMotionBezierPatch(mp, u, v + eps, x4, y4);
                            const double dx1 = static_cast<double>(x2 - x1);
                            const double dy1 = static_cast<double>(y2 - y1);
                            const double dx2 = static_cast<double>(x3 - x4);
                            const double dy2 = static_cast<double>(y3 - y4);
                            const double area1 = std::fabs(dx1 * (y4 - y1) - dy1 * (x4 - x1)) * 0.5;
                            const double area2 = std::fabs(dx1 * (y3 - y1) - dy1 * (x3 - x1)) * 0.5;
                            meshScaleFactor = static_cast<float>(
                                std::sqrt(area1 + area2 + area2 + area1) / 0.0002);
                        }
                    }
                }
                float px = pOn
                    ? static_cast<float>(wm11[node.parentIndex] * defCx +
                                         wm12[node.parentIndex] * defCy) + baseX
                    : loX;
                float py = pOn
                    ? static_cast<float>(wm21[node.parentIndex] * defCx +
                                         wm22[node.parentIndex] * defCy) + baseY
                    : loY;
                // ⑤ groundCorrection TJS callback (reference sub_6BAA10): when the
                // node carries PSB "groundCorrection", invoke onGroundCorrection on
                // the destination layer with [parentPos] and [childPos] arrays. If
                // the callback returns an array the child position is replaced.
                // ⑤ groundCorrection TJS 回调（参考 sub_6BAA10）：节点带 PSB
                // "groundCorrection" 时，在目标层上调用 onGroundCorrection，传入
                // [父位置] 与 [子位置] 数组；回调返回数组则替换子位置。
                if(node.groundCorrection) {
                    anyGround = true;
                    try {
                        tTJSVariant hasFn;
                        if(TJS_SUCCEEDED(dest->PropGet(0, TJS_W("onGroundCorrection"),
                                                       nullptr, &hasFn, dest)) &&
                           hasFn.Type() == tvtObject) {
                            iTJSDispatch2 *parentArr = TJSCreateArrayObject();
                            iTJSDispatch2 *childArr = TJSCreateArrayObject();
                            if(parentArr && childArr) {
                                const tTJSVariant p0(static_cast<tjs_real>(node.parentIndex >= 0 ? wx[node.parentIndex] : 0.0f));
                                const tTJSVariant p1(static_cast<tjs_real>(node.parentIndex >= 0 ? wy[node.parentIndex] : 0.0f));
                                const tTJSVariant c0(static_cast<tjs_real>(px));
                                const tTJSVariant c1(static_cast<tjs_real>(py));
                                parentArr->PropSet(TJS_MEMBERENSURE, TJS_W("0"), nullptr, const_cast<tTJSVariant *>(&p0), parentArr);
                                parentArr->PropSet(TJS_MEMBERENSURE, TJS_W("1"), nullptr, const_cast<tTJSVariant *>(&p1), parentArr);
                                childArr->PropSet(TJS_MEMBERENSURE, TJS_W("0"), nullptr, const_cast<tTJSVariant *>(&c0), childArr);
                                childArr->PropSet(TJS_MEMBERENSURE, TJS_W("1"), nullptr, const_cast<tTJSVariant *>(&c1), childArr);
                                tTJSVariant args[2] = { tTJSVariant(parentArr, parentArr),
                                                        tTJSVariant(childArr, childArr) };
                                tTJSVariant *argv[] = { &args[0], &args[1] };
                                tTJSVariant result;
                                dest->FuncCall(0, TJS_W("onGroundCorrection"), nullptr, &result, 2, argv, dest);
                                iTJSDispatch2 *resObj = result.AsObjectNoAddRef();
                                if(resObj) {
                                    tTJSVariant rx, ry;
                                    if(TJS_SUCCEEDED(resObj->PropGet(0, TJS_W("0"), nullptr, &rx, resObj)))
                                        if(rx.Type() == tvtInteger || rx.Type() == tvtReal)
                                            px = static_cast<float>(rx.AsReal());
                                    if(TJS_SUCCEEDED(resObj->PropGet(0, TJS_W("1"), nullptr, &ry, resObj)))
                                        if(ry.Type() == tvtInteger || ry.Type() == tvtReal)
                                            py = static_cast<float>(ry.AsReal());
                                }
                                if(logger)
                                    logger->info("drawAnimatedTree groundCorrection: '{}' -> ({:.1f},{:.1f})",
                                                 node.label, px, py);
                                parentArr->Release();
                                childArr->Release();
                            }
                        }
                    } catch(...) {
                        if(logger) logger->warn("drawAnimatedTree groundCorrection: exception in '{}'", node.label);
                    }
                }
                // Accumulate scale through the parent chain (B round 3 partial: a
                // container's scale now propagates to its children multiplicatively).
                // 沿父链累加缩放（B 第 3 轮的一部分：容器的缩放以乘法传给子层）。
                //
                // libkrkr2 gates scale/angle/flip inheritance per-node via `inheritMask`
                // (bit 0x20 scaleX, 0x40 scaleY). We don't parse inheritMask yet, but the
                // M2 `str_clip` text container carries a CLIP-REGION scale (zx/zy, e.g.
                // m2logo "cheeseware" str_clip s=9,1) that must scale ONLY the reveal
                // window, NOT the letter glyphs underneath — otherwise the letters inherit
                // 9× and smear into an unreadable blob. So a str_clip container's own scale
                // is dropped from what it passes to its children (children still get the
                // logo's real scale from main/layout ancestors).
                // libkrkr2 用 inheritMask 逐节点门控 scale/angle/flip 的继承（bit 0x20
                // scaleX、0x40 scaleY）。我们暂未解析 inheritMask，但 M2 的 str_clip 文本
                // 容器携带的是**裁剪区域**的缩放（zx/zy，如 m2logo "cheeseware" 的
                // str_clip s=9,1），它只该缩放显现窗口，不能传给下面那些文字字形——否则
                // 字母继承 9× 糊成一片。因此 str_clip 容器自身的缩放不下传给子层（子层仍
                // 从 main/layout 祖先得到 logo 真正的缩放）。
                const bool isStrClipNode =
                    node.label.compare(0, 8, "str_clip") == 0;
                // Scale inheritance gated by inheritMask bit 0x020 (X) / 0x040 (Y);
                // when CLEAR, the node uses only its own scale (doesn't multiply parent).
                // 缩放继承由 inheritMask bit 0x020(X)/0x040(Y) 门控；为 0 时只用自身缩放
                //（不乘父）。
                const float ownSx = std::max(interpSx, 0.0f);
                const float ownSy = std::max(interpSy, 0.0f);
                // ② E-mote jacobian scale deformation folds into the OWN scale before
                // inheritance (the reference multiplies the accumulated scale, which for
                // us is own×parent — equivalent). Gate: mesh angle bit 2 uses inheritMask
                // 0x10, scale bit 4 uses 0x020/0x040; only apply when that bit is set.
                // ② E-mote Jacobian 缩放变形折进**自身**缩放后再继承（参考乘到累加缩放，
                // 等价于 自身×父）。门控：mesh 角度位 2 用 inheritMask 0x10、缩放位 4 用
                // 0x020/0x040；仅在该位置位时生效。
                const float ownSxM = (meshScaleFactor != 1.0f && (inh & 0x020))
                    ? ownSx * meshScaleFactor : ownSx;
                const float ownSyM = (meshScaleFactor != 1.0f && (inh & 0x040))
                    ? ownSy * meshScaleFactor : ownSy;
                const float scx = (inh & 0x020) ? baseSx * ownSxM : ownSxM;
                const float scy = (inh & 0x040) ? baseSy * ownSyM : ownSyM;
                // Additionally, the M2 `str_clip` text container carries a CLIP-REGION
                // scale (zx/zy, e.g. m2logo str_clip s=9,1) that must scale ONLY the
                // reveal window, NOT the letter glyphs underneath — otherwise the letters
                // inherit 9x and smear into an unreadable blob. If the letters' own
                // inheritMask excludes scale this gate would cover it, but as a safety net
                // we also stop a str_clip's scale from reaching its children.
                // 另外，M2 的 str_clip 文本容器携带**裁剪窗口**缩放（zx/zy，如 m2logo
                // str_clip s=9,1），只该缩放显现窗口，不应传给下面的字母字形——否则字母
                // 继承 9× 糊成团。若字母自身 inheritMask 排除缩放，该门控即可覆盖，但这里
                // 仍加一道保险：str_clip 自身的缩放不下传给子层。
                const float scxChild = isStrClipNode ? baseSx : scx;
                const float scyChild = isStrClipNode ? baseSy : scy;
                // Accumulate rotation through the parent chain (deg, additive) — gated by
                // inheritMask bit 0x010; CLEAR uses own angle only.
                // 沿父链累加旋转角（度，相加）——由 inheritMask bit 0x010 门控；为 0 只用自己的角度。
                //
                // ④ motionDt 5 模式 (reference sub_6BE534, mn.activeSlot().motionDt):
                //   mode 1 = direct dofst (replaces keyframe angle contribution);
                //   mode 2 = dofst + atan2(prevPos - currentPos) (direction of motion);
                //   mode 4 = dofst + atan2 toward node `motionDtgt`.
                //   (mode 3 is the dual-slot crossfade path — the workspace has no
                //   crossfade slots, so it degrades to mode 2's per-frame delta.)
                // ④ motionDt 5 模式（参考 sub_6BE534，mn.activeSlot().motionDt）：
                //   模式 1=直接 dofst（取代关键帧角贡献）、模式 2=dofst+atan2(上一位置-
                //   当前位置)（运动朝向）、模式 4=dofst+朝节点 motionDtgt 的 atan2。
                //   （模式 3 是双槽交叉淡入路径——本工作区无交叉淡入槽，退化为模式 2 的
                //   逐帧位移差。）
                float motionDtExtra = 0.0f;
                if(af->motionDt != 0) {
                    float mdtBase = af->motionDofst;
                    if(af->motionDt == 2) {
                        // delta = current own pos - PREVIOUS own pos (per-frame delta,
                        // reference deltaPosX/Y). Reuse the LAST FRAME state if we have
                        // one (persistent, not cleared per draw) else the keyframe delta.
                        // 位移差 = 当前自身位置 - 上一帧自身位置（参考 deltaPosX/Y）。
                        // 有上一帧状态用上一帧（跨绘制保留），否则用关键帧间差。
                        const float prevCx = _lastFramePosX.count(node.label) ? _lastFramePosX[node.label] : af->cx;
                        const float prevCy = _lastFramePosY.count(node.label) ? _lastFramePosY[node.label] : af->cy;
                        const float dx = interpCx - prevCx;
                        const float dy = interpCy - prevCy;
                        mdtBase += static_cast<float>(std::atan2(static_cast<double>(dy),
                                                                 static_cast<double>(dx)) * 360.0 / 6.28318531);
                    } else if(af->motionDt == 4 && !af->motionDtgt.empty()) {
                        // aim at another NODE's world position (reference sub_6BE7B4).
                        // 瞄准另一节点的世界位置（参考 sub_6BE7B4）。
                        for(int ti = 0; ti < n; ti++) {
                            if(_motionNodes[ti].label == af->motionDtgt) {
                                const float dx = wx[ti] - px;
                                const float dy = wy[ti] - py;
                                mdtBase += static_cast<float>(std::atan2(static_cast<double>(dy),
                                                                         static_cast<double>(dx)) * 360.0 / 6.28318531);
                                break;
                            }
                        }
                    }
                    motionDtExtra = mdtBase;
                    anyMotionDt = true;
                    // Persist this node's own pos for the NEXT frame's mode-2 delta.
                    // 保存本节点自身位置，供下一帧模式 2 的位移差使用。
                    _lastFramePosX[node.label] = interpCx;
                    _lastFramePosY[node.label] = interpCy;
                }
                const float effAngle = (inh & 0x010)
                    ? baseAngle + interpAngle + meshAngleDelta
                    : interpAngle + meshAngleDelta;
                // ④ motionDt mode 1 REPLACES the keyframe angle with dofst (still added
                // to the parent chain); modes 2/4 ADD the aiming angle. Mode 1's value
                // carries the authored dofst itself (reference case 1: computedAngle =
                // dofst + accumulated.angle — our accumulated angle is base+interp).
                // ④ motionDt 模式 1 **取代**关键帧角为 dofst（仍叠加父链）；模式 2/4
                // **叠加**瞄准角。模式 1 的值即作者写的 dofst 本身（参考 case 1：
                // computedAngle = dofst + accumulated.angle——我们的累加角即 base+interp）。
                const float finalAngle = (af->motionDt == 1)
                    ? baseAngle + af->motionDofst + meshAngleDelta
                    : (af->motionDt != 0 ? effAngle + motionDtExtra : effAngle);
                // Accumulate skew through the parent chain (additive) — gated by
                // inheritMask bit 0x080 (X) / 0x100 (Y) (AetherKiri updateLayers
                // 0x6BB8F4: slantX += parent.slantX). We previously never read skew.
                // 沿父链累加斜切（相加）——由 inheritMask bit 0x080(X)/0x100(Y) 门控
                //（AetherKiri updateLayers 0x6BB8F4：slantX += parent.slantX）。此前从未读斜切。
                const float baseSlx = (node.parentIndex >= 0) ? wslx[node.parentIndex] : 0.0f;
                const float baseSly = (node.parentIndex >= 0) ? wsly[node.parentIndex] : 0.0f;
                const float effSlx = (inh & 0x080) ? baseSlx + interpSlx : interpSlx;
                const float effSly = (inh & 0x100) ? baseSly + interpSly : interpSly;
                const int lop = std::clamp(static_cast<int>(interpOp), 0, 255);
                const int wop = baseOp * lop / 255;
                wx[i] = px; wy[i] = py; wsx[i] = scxChild; wsy[i] = scyChild;
                // Effective tint: the node's own non-white frame color, else the parent
                // chain's nearest non-white container color (text/group tint). White =
                // no tint. Stored so descendants inherit the container color too.
                // 生效 tint：节点自身非白的帧色，否则沿父链取最近的非白容器色（文本/组
                // 着色）。白=不着色。存起来供子节点继续继承该容器色。
                // Effective 4-corner tint: the node's own non-white frame corners, else the
                // parent chain's nearest non-white container corners (text/group tint).
                // White corners = no tint. Stored so descendants inherit them too.
                // 生效四角 tint：节点自身非白的帧四角，否则沿父链取最近的非白容器四角
                //（文本/组着色）。白=不着色。存起来供子节点继续继承。
                const bool ownColored = (interpPacked[0] != 0xFFFFFFFFu);
                std::array<std::uint32_t, 4> effPacked =
                    ownColored ? interpPacked
                               : ((node.parentIndex >= 0)
                                      ? wtint[node.parentIndex]
                                      : std::array<std::uint32_t, 4>{
                                            0xFFFFFFFFu, 0xFFFFFFFFu,
                                            0xFFFFFFFFu, 0xFFFFFFFFu});
                wtint[i] = effPacked;
                // Store this node's WORLD linear matrix (from accumulated flip/angle/scale)
                // so its children can be position-transformed by it next (pre-order).
                // 存储本节点的**世界线性矩阵**（由累加 flip/angle/scale 构建），供下一轮
                //（子节点）用它做位置变换（先序）。
                buildLocalMatrix(effFx, effFy, finalAngle, scxChild, scyChild,
                                 effSlx, effSly,
                                 node.transformOrder,
                                 wm11[i], wm12[i], wm21[i], wm22[i]);
                wa[i] = finalAngle;
                wslx[i] = effSlx; wsly[i] = effSly;
                wfx[i] = effFx; wfy[i] = effFy;
                wo[i] = wop;
                vis[i] = (wop > 0);
                if(!vis[i]) continue;
                // M2 text-layout (str_clip) container: clamp the dest layer's clip
                // rect to this node's display box (size = PSB "clip" region) so its
                // letter subtree is progressively revealed/cropped, then RESTORE the
                // clip when a later sibling node asks us to reset. libkrkr2 does the
                // same: each render item computes a clip (= paintBox clamped to the
                // viewport) and calls renderLayer->SetClip(...) before operating the
                // child, ResetClip() otherwise. Without this, m2logo's "cheeseware"
                // letters overlap / never get the typewriter-erase crop.
                // M2 文本排版(str_clip)容器：把目标层的裁剪矩形收紧到本节点显示盒
                //（尺寸即 PSB 的 "clip" 区域），让它的字母子树逐字显现/裁切；当后续
                // 兄弟节点请求 ResetClip 时再恢复。libkrkr2 做法相同：每个渲染项算出
                // 一个 clip（=paintBox 与 viewport 求交），operate 前对子层 SetClip，
                // 否则 ResetClip。缺它则 m2logo 的 "cheeseware" 字母互相重叠、永远
                // 没有打字机擦除效果。
                // The clip window is a fixed type-7 text-clip region (node.width/height
                // may be 0). Its size is derived from the letters it reveals: the
                // horizontal extent of the descendants' active-frame pen positions
                // (cx+ox) plus a per-letter width estimate. The letters' local cx are
                // relative to the str_locate container (which is offset from the
                // str_clip itself, e.g. m2logo cx=-114), so the window is anchored at
                // the TEXT's world position (computed below), not the str_clip's own.
                // 裁剪窗口是固定的 type-7 文本裁剪区（node.width/height 可能为 0）。
                // 尺寸按它揭示的字母派生：各后代节点当前帧笔位 (cx+ox) 的水平范围 +
                // 每字母宽度估计。字母的局部 cx 相对 str_locate 容器（str_locate 又相对
                // str_clip 有偏移，如 m2logo 的 cx=-114），因此窗口锚定在**文字**的世界
                // 位置（见下），而非 str_clip 自身。
                float winMin = 1e9f, winMax = -1e9f;
                // The text-layout container (str_locate) is the direct child of the
                // str_clip that carries the letter glyphs. The clip window must be
                // anchored at the TEXT's world position, not the str_clip's own
                // position: the letters' local cx+ox are relative to str_locate
                // (which itself sits offset from str_clip, e.g. m2logo cx=-114), so
                // anchoring at str_clip misplaced the window ~114px to the right and
                // cut off the leading letters ("CheeseWare" showed as "sewa"-like
                // garbage). The window covers the letters' world x-extent.
                // 文本布局容器（str_locate）是 str_clip 的直接子节点、承载字母字形。
                // 裁剪窗口必须锚定在**文字的世界位置**而非 str_clip 自身位置：字母的
                // 局部 cx+ox 是相对 str_locate 的（str_locate 又相对 str_clip 有偏移，
                // 如 m2logo 的 cx=-114），锚定在 str_clip 会让窗口右移约 114px、截掉
                // 前面的字母（"CheeseWare" 显示成 "sewa" 之类的乱码）。窗口应覆盖字母的
                // 世界 x 范围。
                float txtWorldX = wx[i], txtWorldY = wy[i]; // str_clip world pos (fallback)
                float txtScaleX = 1.0f; // world X scale applied to the letters / 字母的世界 X 缩放
                // Canonical signal: PSB type 7 = M2 text-clip container (str_clip /
                // str_locate family). Label and src are fallbacks; type==7 is the generic
                // discriminator libkrkr2 uses, so this is not asset-specific naming.
                // 规范判据：PSB type 7 = M2 文本裁剪容器（str_clip/str_locate 族）。label 与
                // src 只是兜底；type==7 才是 libkrkr2 用的通用判据，不与具体资产命名绑定。
                const bool strClipCandidate =
                    node.type == 7 || node.label.compare(0, 8, "str_clip") == 0;
                bool isStrClipContainer = false;
                if(clipSupported && strClipCandidate) {
                    // Descendants are laid out contiguously after this node in pre-order;
                    // gather the active-frame pen x-extent of its leaf letters.
                    // 后代先序连续排在本节点之后；收集其字母叶节点当前帧笔位 x 范围。
                    for(int j = i + 1; j < n; j++) {
                        int p = _motionNodes[j].parentIndex;
                        bool desc = false;
                        while(p >= 0) { if(p == i) { desc = true; break; } p = _motionNodes[p].parentIndex; }
                        if(!desc) break; // pre-order: children immediately follow; stop at first non-descendant
                        const auto &fr = _motionNodes[j].frames;
                        const PSB::PSBMedia::PSBMotionFrame *cf = nullptr;
                        for(const auto &f : fr) { if(f.time <= now) cf = &f; else break; }
                        if(!cf || !cf->visible) continue;
                        if(cf->src.size() <= 4 || cf->src.compare(0, 4, "src/") != 0) continue; // letters only / 只统计字母
                        const float lx = cf->cx + cf->ox;
                        if(lx < winMin) winMin = lx;
                        const float rx = lx + 60.0f; // per-letter width estimate / 字母宽度估计
                        if(rx > winMax) winMax = rx;
                        // Anchor the window at the letters' layout container (str_locate):
                        // its world pos = str_clip's world matrix applied to the container's
                        // own active-frame local pos + str_clip's world pos. The container
                        // follows the str_clip in pre-order so its own wm/wx are not
                        // computed yet — replicate the parent transform here. The world X
                        // scale the letters get is the str_clip's own passed-down scale
                        // (its clip-region 9x is excluded by the isStrClipNode gate).
                        // 把窗口锚定在字母的布局容器（str_locate）：其世界位置 =
                        // str_clip 的世界矩阵 × 容器自身活跃帧局部坐标 + str_clip 世界位置。
                        // 容器先序排在 str_clip 之后、自身的 wm/wx 尚未计算——这里复算父变换。
                        // 字母得到的世界 X 缩放即 str_clip 下传的缩放（其裁剪窗口 9x 已被
                        // isStrClipNode 门控排除）。
                        const int lparent = _motionNodes[j].parentIndex;
                        if(lparent >= 0) {
                            const auto &pframes = _motionNodes[lparent].frames;
                            const PSB::PSBMedia::PSBMotionFrame *pf = nullptr;
                            for(const auto &f : pframes) { if(f.time <= now) pf = &f; else break; }
                            if(pf) {
                                txtWorldX = static_cast<float>(wm11[i] * pf->cx +
                                                               wm12[i] * pf->cy) + wx[i];
                                txtWorldY = static_cast<float>(wm21[i] * pf->cx +
                                                               wm22[i] * pf->cy) + wy[i];
                                txtScaleX = static_cast<float>(wm11[i]);
                            }
                        }
                    }
                    isStrClipContainer = winMax > winMin;
                }
                float wcL = 0, wcT = 0, wcR = 0, wcB = 0;
                if(isStrClipContainer) {
                    // Window box in layer coords: anchored at the TEXT's world position
                    // (str_locate), size = the letters' local extent scaled to world.
                    // 窗口盒转到层坐标：锚定在**文字**的世界位置（str_locate），尺寸 =
                    // 字母局部范围换算到世界。
                    wcL = static_cast<float>(_coordX + halfCw) + txtWorldX + winMin * txtScaleX
                    // Leave one half-glyph of headroom on the LEFT of the first letter:
                    // the window is anchored at the letters' pen positions (cx+ox), and
                    // the leftmost glyph ("c" of cheeseware, ox=-4) extends to the left
                    // of its pen — starting the crop exactly at the pen nicks off the
                    // glyph's leading edge ("Cheese 最左端 C 被挡一小块").
                    // 在首字母左侧留出半个字形的余量：窗口锚定在字母笔位(cx+ox)，而最左
                    // 字形（cheeseware 的 "c"，ox=-4）实际向左超出笔位——裁剪正好从笔位
                    // 开始会切掉该字形前缘（"Cheese 最左端 C 被挡一小块"）。
                    - 15.0f * txtScaleX;
                    wcT = static_cast<float>(_coordY + halfCh) + txtWorldY - 30.0f;
                    const float winW = std::max(1.0f, (winMax - winMin) * txtScaleX);
                    wcR = wcL + winW;
                    wcB = wcT + 120.0f; // enough height for the glyph / 足够容纳字形
                }
                // ⑤ 父 viewport 裁剪（通用）：非 type-7 容器若本帧自带 clip 矩形
                //（frame clip），把该矩形按其世界盒换算成层坐标，作为其后代的裁剪窗。
                // 与 str_clip 的"字母范围"启发式不同，这里直接用作者给出的矩形。
                // ⑤ Parent viewport clip (generic): a NON-type-7 container whose active
                // frame carries its own clip rect maps that rect into layer space via
                // its world box and clips its descendants — unlike str_clip's letter
                // heuristic, this uses the authored rect directly.
                bool isViewportClip = false;
                if(!isStrClipContainer && af->hasClip && !isStrClipNode) {
                    const float anX = node.width > 0 ? static_cast<float>(node.width) * 0.5f : 0.0f;
                    const float anY = node.height > 0 ? static_cast<float>(node.height) * 0.5f : 0.0f;
                    const float left = static_cast<float>(_coordX + halfCw) + px
                        - static_cast<float>(wm11[i] * anX + wm12[i] * anY);
                    const float top = static_cast<float>(_coordY + halfCh) + py
                        - static_cast<float>(wm21[i] * anX + wm22[i] * anY);
                    wcL = left + af->clipL * scxChild;
                    wcT = top + af->clipT * scyChild;
                    wcR = left + af->clipR * scxChild;
                    wcB = top + af->clipB * scyChild;
                    isViewportClip = (wcR > wcL && wcB > wcT);
                }
                // Only image lines draw; layout/motion containers only accumulate.
                // 仅图像行绘制；layout/motion 容器只累加不绘制。
                // --- str_clip clip lifecycle (runs for containers AND images) ---
                // --- str_clip 裁剪生命周期（容器与图像都执行）---
                if(_strClipActiveParent >= 0 && i != _strClipActiveParent) {
                    // Is this node still inside the active str_clip's subtree? If not,
                    // the crop ended — restore the previous layer clip.
                    // 本节点是否仍在生效 str_clip 的子树内？若否，裁切结束，恢复之前裁剪。
                    bool under = false;
                    for(int p = node.parentIndex; p >= 0; p = _motionNodes[p].parentIndex) {
                        if(p == _strClipActiveParent) { under = true; break; }
                    }
                    if(!under) { writeClip(dest, prevClip); _strClipActiveParent = -1; }
                }
                if(isStrClipContainer || isViewportClip) {
                    writeClip(dest, ClipState{static_cast<int>(wcL), static_cast<int>(wcT),
                                              static_cast<int>(wcR - wcL), static_cast<int>(wcB - wcT)});
                    _strClipActiveParent = i;
                }
                if(af->src.size() <= 4 || af->src.compare(0, 4, "src/") != 0) {
                    // Container node: only accumulate transform (clip already handled above).
                    // 容器节点只累加变换（裁剪已在上方处理）。
                    continue;
                }
                // An image node: if a str_clip subtree previously set a clip, an
                // unrelated image arriving without a new str_clip keeps the crop —
                // harmless; we only reset when the container itself ends. Most
                // importantly, never leave the layer clipped across frames.
                // 图像节点：若此前 str_clip 子树设置了裁剪，无关图像没有新 str_clip
                // 时保持裁剪——无害；只在容器结束时重置。关键是绝不跨帧残留裁剪。
                const std::string res = MotionSrcToResource(af->src);
                const ttstr path = TJS_W("psb://") +
                    ttstr((storageStr + "/" + res + "/pixel.png").c_str());
                if(!TVPIsExistentStorage(path)) {
                    if(auto l = logger) {
                        l->warn("drawAnimatedTree: skip node '{}' src='{}' -> missing '{}'",
                                node.label, af->src, path.AsStdString());
                    }
                    continue;
                }
                iTJSDispatch2 *temp = getOrCreateTempLayer(tempParent);
                if(!temp) continue;
                if(!tryLoadImage(temp, path)) continue;
                tTJSVariant wVar, hVar;
                temp->PropGet(0, TJS_W("imageWidth"), nullptr, &wVar, temp);
                temp->PropGet(0, TJS_W("imageHeight"), nullptr, &hVar, temp);
                const int iw = static_cast<int>(wVar.AsInteger());
                const int ih = static_cast<int>(hVar.AsInteger());
                if(iw <= 0 || ih <= 0) continue;
                // Load probe (P-load): for the m2logo line icons the authored
                // texture is a 3x16 paletted bar (icon32 dark / icon18 gray), yet
                // the engine often ends up with a 1x1 texture (invisible vertical
                // line). Log the resolved path, the actually-loaded size, and the
                // cached image metadata so one run shows whether the width/height
                // meta is wrong or the Open/convert path fails for these sprites.
                // 加载探针(P-load)：m2logo 线框 icon 的原创纹理是 3x16 调色条
                //（icon32 深色/icon18 浅灰），但引擎常得到 1x1（竖线不可见）。
                // 打印解析路径、实际加载尺寸与缓存的图像元数据，一次看清是元数据
                // 宽高错了还是 Open/convert 对这些精灵失败。
#if defined(KRKR_RENDER_PROBE)
                if(logger &&
                   (af->src == "src/logo/icon17" ||
                    af->src == "src/logo/icon18" ||
                    af->src == "src/logo/icon32" ||
                    af->src == "src/logo/icon26" ||
                    node.label == "line" || node.label == "line2")) {
                    PSB::PSBMedia::CachedImageInfo gi;
                    const bool hasGi = PSB::GetGlobalPSBMedia() &&
                        PSB::GetGlobalPSBMedia()->getImageInfo(
                            storageStr + "/" + res + "/pixel.png", gi);
                    logger->info(
                        "loadProbe: node='{}' src='{}' res='{}' loaded={}x{} "
                        "hasMeta={} meta={}x{} compress={} type='{}' palBytes={}",
                        node.label, af->src, res, iw, ih, hasGi ? 1 : 0,
                        hasGi ? gi.width : 0, hasGi ? gi.height : 0,
                        static_cast<int>(gi.compress), gi.type, gi.palette.size());
                }
#endif
                // Draw an M2 content sprite with the REFERENCE single-world-matrix + origin
                // anchor model (`org = pos - M*(originX+ox, originY+oy)`, quad
                // `org + M*[0..iw,0..ih]`). A sprite's position px,py is its world
                // position (loX=interpCx, ox/oy NOT in position — adding ox double-counts
                // and drifts the sprite); the transform (flip×angle×scale, wm11..wm22)
                // and the anchored translation are ONE matrix, so rotation pivots about
                // the icon/ox hotspot rather than the box center. Anchor resolution
                // (resolveCoordOrigin + per-icon origin) is below.
                // 用参考的**单一世界矩阵+原点锚定**模型绘制 M2 内容精灵（`org = pos -
                // M*(originX+ox, oy)`，四边形 `org + M*[0..iw,0..ih]`）。精灵位置
                // px,py 即世界位置（loX=interpCx，ox/oy **不进位置**——加 ox 会双计并使
                // 精灵漂移）；变换(flip×angle×scale，wm11..wm22)与锚定平移合成一个矩阵，
                // 旋转天然绕 icon/ox 热区而非盒子中心。锚点解析见下（resolveCoordOrigin
                // + 逐 icon origin）。
                const int coordOrigin = resolveCoordOrigin();
                // Reference model (libkrkr2 updateLayersPhase3_VertexComputation /
                // AetherKiri: `org = pos - M*(originX+ox, originY+oy)`, quad
                // `org + M*[0..iw,0..ih]`): ONE world matrix M (= the accumulated
                // flip×angle×scale, wm11..wm22) drives BOTH the quad affine and the
                // anchored translation. Image anchor = source icon hotspot (originX,
                // originY) + frame (ox,oy); it lands exactly on the node's world
                // position (px,py), and rotation pivots about it naturally — no
                // separate fold / center / pivot hacks. Icons WITHOUT a baked origin
                // fall back to center (default) so full-canvas icons stay centered.
                // 参考模型（libkrkr2 / AetherKiri：`org = pos - M*(originX+ox, oy)`、
                // 四边形 `org + M*[0..iw,0..ih]`）：**单一世界矩阵** M（= buildLocalMatrix
                // 累加的 flip×angle×scale，即 wm11..wm22）同时驱动四边形仿射与锚定平移。
                // 图像锚点 = 源 icon 热点(originX,originY) + 帧(ox,oy)，正好落在节点世界
                // 位置(px,py)；旋转天然绕该锚点，不再需要单独的折叠/居中/枢轴 hack。
                // icon 无烘焙 origin 时回退中心（默认），保证整图 icon 居中。
                float anchorX = interpOx, anchorY = interpOy;
                float iconOrX = 0.0f, iconOrY = 0.0f;
                bool hasIconOrigin = false;
                if(auto *med = PSB::GetGlobalPSBMedia()) {
                    PSB::PSBMedia::CachedImageInfo gi;
                    if(med->getImageInfo(storageStr + "/" + res + "/pixel.png", gi)) {
                        iconOrX = gi.originX;
                        iconOrY = gi.originY;
                        hasIconOrigin = (gi.originX != 0.0f || gi.originY != 0.0f);
                        anchorX += iconOrX;
                        anchorY += iconOrY;
                    }
                }
                if(!hasIconOrigin && coordOrigin != 1) {
                    // No baked origin: default anchor to the image CENTER so centered /
                    // full-canvas icons align with the static composite (and we don't
                    // regress the prior "整体向右下偏移" from anchoring everything at
                    // top-left = origin 0).
                    // 无烘焙 origin：锚点默认取图像**中心**，让整图/居中 icon 与静态合成
                    // 对齐（也避免重犯此前"整体向右下偏移"——那时全部钉左上角=0）。
                    anchorX += iw * 0.5f;
                    anchorY += ih * 0.5f;
                } // else top-left: anchor stays = ox,oy (reference for non-anchored icons)
                // Display-box scale (layer larger than its texture) + per-frame
                // scale; stretch the box and center it on the same anchor.
                // 显示盒缩放(图层大于纹理)+帧内缩放；把盒拉伸并居中到同一锚点。
                const float boxScX = (node.width > 0 && node.width > iw) ? static_cast<float>(node.width) / iw : 1.0f;
                const float boxScY = (node.height > 0 && node.height > ih) ? static_cast<float>(node.height) / ih : 1.0f;
                // Single world matrix: x' = a*x + b*y + tx, y' = c*x + d*y + ty.
                // Display-box scale (layer box > texture) folds into the diagonal.
                // 单一世界矩阵：x'=a*x+b*y+tx, y'=c*x+d*y+ty；显示盒缩放卷进对角。
                const double mA0 = wm11[i] * boxScX;
                const double mB0 = wm12[i] * boxScX;
                const double mC0 = wm21[i] * boxScY;
                const double mD0 = wm22[i] * boxScY;
                // Origin-anchored translation: place the anchor at the world position.
                // 原点锚定平移：把锚点放到世界位置 (px,py)。
                const tjs_real outputTx = static_cast<tjs_real>(_coordX + halfCw + px - (mA0 * anchorX + mB0 * anchorY));
                const tjs_real outputTy = static_cast<tjs_real>(_coordY + halfCh + py - (mC0 * anchorX + mD0 * anchorY));
                const tjs_real mA = static_cast<tjs_real>(mA0);
                const tjs_real mB = static_cast<tjs_real>(mB0);
                const tjs_real mC = static_cast<tjs_real>(mC0);
                const tjs_real mD = static_cast<tjs_real>(mD0);

#if defined(KRKR_RENDER_PROBE)
                if(logger && now >= 190.0 &&
                   (af->src == "src/logo/icon25" ||
                    af->src == "src/logo/icon26" ||
                    af->src == "src/logo/icon27" ||
                    af->src == "src/logo/icon28" ||
                    af->src == "src/logo/icon29" ||
                    af->src == "src/logo/icon48")) {
                    auto screenX = [&](float pixX, float pixY) -> float {
                        return static_cast<float>(outputTx) + mA * (pixX - anchorX) +
                            mB * (pixY - anchorY);
                    };
                    auto screenY = [&](float pixX, float pixY) -> float {
                        return static_cast<float>(outputTy) + mC * (pixX - anchorX) +
                            mD * (pixY - anchorY);
                    };
                    float minX = screenX(0, 0), maxX = minX;
                    float minY = screenY(0, 0), maxY = minY;
                    const float corners[4][2] = {{0, 0}, {static_cast<float>(iw), 0},
                                                 {0, static_cast<float>(ih)},
                                                 {static_cast<float>(iw), static_cast<float>(ih)}};
                    for(const auto &co : corners) {
                        const float sx = screenX(co[0], co[1]);
                        const float sy = screenY(co[0], co[1]);
                        minX = std::min(minX, sx); maxX = std::max(maxX, sx);
                        minY = std::min(minY, sy); maxY = std::max(maxY, sy);
                    }
                    logger->info(
                        "m2foldProbe: '{}' now={:.1f}ms t={} src='{}' ang={:.1f} "
                        "aabbX=[{:.1f},{:.1f}] aabbY=[{:.1f},{:.1f}] w={:.0f} h={:.0f} "
                        "anchor=({:.1f},{:.1f}) ic={}",
                        node.label, static_cast<float>(now), static_cast<tjs_int>(af->time), af->src,
                        finalAngle, minX, maxX, minY, maxY, (maxX - minX), (maxY - minY),
                        anchorX, anchorY,
                        (hasIconOrigin ? (std::to_string(static_cast<int>(iconOrX)) + "," +
                                          std::to_string(static_cast<int>(iconOrY)))
                                       : std::string("-")));
                }
#endif
                tjs_int opaClamp = std::clamp(wop, 0, 255);
                // Apply the M2 PER-CORNER vertex-color tint to the glyph texture before
                // drawing (identity when all corners white). This is what colors the
                // m2logo C/W red, the thin line red→black, and the black cross — plus
                // 4-corner gradients when authored — operateAffine has no color channel,
                // so tint the source pixels here.
                // 绘制前把 M2 **四角**顶点色平涂到字形纹理（四角全白为恒等跳过）。给
                // m2logo 的 C/W 上红、细线红转黑、黑色十字，以及作者创作的四角渐变上
                // 色——operateAffine 没有颜色通道，因此在此乘源像素。
                if(effPacked[0] != 0xFFFFFFFFu || effPacked[1] != 0xFFFFFFFFu ||
                   effPacked[2] != 0xFFFFFFFFu || effPacked[3] != 0xFFFFFFFFu) {
                    applyCornerTint(temp, effPacked);
                }
                // Round 2 blend mode: map M2 content "bm" to an operate blend op.
                // 0=normal(alpha),1=additive,2=subtractive,3=multiplicative,4=addalpha
                // (om ints from drawable.h: alpha=2,add=3,sub=4,mul=5,addalpha=12).
                // 第二轮混合模式：把 M2 content "bm" 映射为 operate 混合算子。
                // 0=正常(alpha),1=加,2=减,3=乘,4=加alpha（int 见 drawable.h）。
                int blendOm = 2;
                switch(af->blendMode) {
                    case 1: blendOm = 3; break;  // additive / 加
                    case 2: blendOm = 4; break;  // subtractive / 减
                    case 3: blendOm = 5; break;  // multiplicative / 乘
                    case 4: blendOm = 12; break; // addalpha / 加 alpha
                    default: blendOm = 2; break; // alpha (normal) / 正常
                }
                // 参数依 Layer.operateAffine(src, x, y, w, h, affine, a,b,c,d,
                // tx,ty, mode, opa, ...)。dst 对象绑定到 dest（调用对象），src 是
                // 临时层上的纹理。
                // Note: argument order follows Layer.operateAffine(src, x, y, w, h,
                // affine, a,b,c,d, tx,ty, mode, opa, ...); the destination is the
                // object `dest` this method is called on, src is the temp texture.
                tTJSVariant opArgs[14] = {
                    tTJSVariant(temp, temp),                      // 0 src
                    tTJSVariant(static_cast<tjs_int>(0)),         // 1 x
                    tTJSVariant(static_cast<tjs_int>(0)),         // 2 y
                    tTJSVariant(static_cast<tjs_int>(iw)),        // 3 src width
                    tTJSVariant(static_cast<tjs_int>(ih)),        // 4 src height
                    tTJSVariant(true),                            // 5 affine (matrix mode)
                    tTJSVariant(mA),                                      // 6 a (rot+scale, may flip)
                    tTJSVariant(mB),                                      // 7 b
                    tTJSVariant(mC),                                      // 8 c
                    tTJSVariant(mD),                                      // 9 d (rot+scale, may flip)
                    tTJSVariant(outputTx),                               // 10 tx
                    tTJSVariant(outputTy),                               // 11 ty
                    tTJSVariant(blendOm),                                // 12 blend mode / 混合模式
                    tTJSVariant(opaClamp),                        // 13 opacity
                };
                tTJSVariant *opArgv[] = { &opArgs[0], &opArgs[1], &opArgs[2],
                                          &opArgs[3], &opArgs[4], &opArgs[5],
                                          &opArgs[6], &opArgs[7], &opArgs[8],
                                          &opArgs[9], &opArgs[10], &opArgs[11],
                                          &opArgs[12], &opArgs[13] };
                // ③ stencil divert: nodes inside a type-12 composite draw into the
                // offscreen GROUP layer; referenced mask nodes draw into the MASK
                // layer (even when authored outside the group subtree — masks are
                // dedicated sprites); both fold together at the end of the pass.
                // ③ stencil 分流：type-12 合成组内的节点画进离屏**组层**；被引用的
                // 蒙版节点画进**蒙版层**（即使作者把蒙版放在组子树外——蒙版是专用
                // 精灵）；两者在本轮末尾合成。
                iTJSDispatch2 *drawTarget = dest;
                if(anyStencil && (stencilMaskOf[i] || stencilGroupOf[i] >= 0)) {
                    if(stencilMaskOf[i]) {
                        drawTarget = getOrCreateStencilLayer(dest, true, cw, ch);
                        if(drawTarget) drewMask = true;
                    } else {
                        drawTarget = getOrCreateStencilLayer(dest, false, cw, ch);
                        if(drawTarget) drewGroup = true;
                    }
                }
                if(drawTarget) {
                    try {
                        drawTarget->FuncCall(0, TJS_W("operateAffine"), nullptr, nullptr, 14, opArgv, drawTarget);
                        drawn++;
                    } catch(const std::exception &e) {
                        if(auto l = _logger()) l->warn("drawAnimatedTree: operateAffine exception: {}", e.what());
                    } catch(...) {
                        if(auto l = _logger()) l->warn("drawAnimatedTree: operateAffine unknown exception");
                    }
                }
            }
            // Restore the layer clip we saved, so a str_clip crop that was the last
            // active region cannot leak into the next frame/motion. Idempotent.
            // 恢复此前保存的层裁剪，确保 str_clip 若为最后一个活跃区域也不会残留到
            // 下一帧/motion。幂等。
            if(_strClipActiveParent >= 0) {
                writeClip(dest, prevClip);
                _strClipActiveParent = -1;
            }
            // ③ stencil composite fold: alpha-multiply the mask into the group and
            // blit the masked group onto the destination as one normal-alpha pass.
            // Only the first active composite type-12 group is folded this pass (the
            // scratch layers are single-buffered); a probe notes every composite.
            // ③ stencil 合成收尾：把蒙版 alpha 乘进组层，再把蒙版后的组层以普通 alpha
            // 一道合成到目标。本轮只折叠第一个活跃的 type-12 合成组（离屏层为单缓冲）；
            // 离屏层为单缓冲，超出一个合成组时只处理第一个。
            if(stencilActive && drewGroup) {
                iTJSDispatch2 *gl = getOrCreateStencilLayer(dest, false, cw, ch);
                iTJSDispatch2 *ml = getOrCreateStencilLayer(dest, true, cw, ch);
                int compositeCount = 0;
                for(int gi = 0; gi < n; gi++) {
                    const auto &gnd = _motionNodes[gi];
                    if(!gnd.hasStencil || gnd.stencilMaskNodeIndices.empty()) continue;
                    compositeCount++;
                    if(compositeCount > 1) {
                        if(logger) logger->warn(
                            "drawAnimatedTree stencil: multiple composites ({}), only first folded", compositeCount);
                        continue;
                    }
                    const bool applied = applyStencilComposite(gl, ml, gnd.stencilType);
                    if(applied) {
                        // Blit the masked group at canvas origin (identity rect).
                        // 把蒙版后的组层按画布原点合成（恒等矩形）。
                        tTJSVariant srArgs[9] = {
                            tTJSVariant(static_cast<tjs_int>(0)),
                            tTJSVariant(static_cast<tjs_int>(0)),
                            tTJSVariant(gl, gl),
                            tTJSVariant(static_cast<tjs_int>(0)),
                            tTJSVariant(static_cast<tjs_int>(0)),
                            tTJSVariant(static_cast<tjs_int>(cw)),
                            tTJSVariant(static_cast<tjs_int>(ch)),
                            tTJSVariant(static_cast<tjs_int>(2)), // omAlpha / 正常 alpha
                            tTJSVariant(static_cast<tjs_int>(255)),
                        };
                        tTJSVariant *srArgv[] = { &srArgs[0], &srArgs[1], &srArgs[2],
                                                  &srArgs[3], &srArgs[4], &srArgs[5],
                                                  &srArgs[6], &srArgs[7], &srArgs[8] };
                        try {
                            dest->FuncCall(0, TJS_W("operateRect"), nullptr, nullptr, 9, srArgv, dest);
                            drawn++;
                        } catch(...) {
                            if(auto l = _logger()) l->warn("drawAnimatedTree stencil: composite blit exception");
                        }
#if defined(KRKR_RENDER_PROBE)
                        if(logger)
                            logger->info("drawAnimatedTree stencil: '{}' stencilType={} group={} mask={} applied={}",
                                         gnd.label, gnd.stencilType, drewGroup ? 1 : 0, drewMask ? 1 : 0, applied ? 1 : 0);
#endif
                    }
                    if(gl) clear(gl, 0);
                    if(ml) clear(ml, 0);
                }
            }
            return drawn;
        }

        // Flat per-track fallback (archives without a node tree). Keeps the previous
        // active-frame selection and center-origin mapping.
        // 扁平按轨道回退（无节点树的归档）。沿用原有 active 帧选择与中心原点映射。
        int drawAnimatedFlat(iTJSDispatch2 *dest, iTJSDispatch2 *tempParent,
                             const std::shared_ptr<spdlog::logger> &logger) {
            if(!dest || _motionTracks.empty()) return 0;
            const tjs_int now = _tickCount; // ms clock
            float cw = 0, ch = 0;
            resolveCanvasSize(cw, ch);
            const tjs_int halfCw = static_cast<tjs_int>(cw / 2.0f);
            const tjs_int halfCh = static_cast<tjs_int>(ch / 2.0f);
            const std::string storageStr = _loadedStorage.IsEmpty()
                ? ResourceManager::getLastLoadedPath().AsStdString()
                : _loadedStorage.AsStdString();
            int drawn = 0;
            for(const auto &track : _motionTracks) {
                const PSB::PSBMedia::PSBMotionFrame *active = nullptr;
                for(const auto &f : track.frames) {
                    if(f.time <= now) active = &f; else break;
                }
                if(!active) continue;
                // Same semantics as the node-tree path: an empty frame hides the
                // layer for its time range (incl. the timeline's final empty
                // frames), instead of holding the last content frame — otherwise
                // logo layers stay visible after they should have disappeared.
                // 与节点树路径一致：空帧在覆盖时段内隐藏该层（含时间线末尾空帧），
                // 而不是保持末内容帧——否则 logo 图层在应该消失后仍可见。
                if(!active->visible) continue;
                if(active->src.size() <= 4 || active->src.compare(0, 4, "src/") != 0) continue;
                const std::string res = MotionSrcToResource(active->src);
                const ttstr path = TJS_W("psb://") +
                    ttstr((storageStr + "/" + res + "/pixel.png").c_str());
                if(!TVPIsExistentStorage(path)) {
                    if(auto l = logger) {
                        l->warn("drawAnimatedFlat: skip track '{}' src='{}' -> missing '{}'",
                                track.label, active->src, path.AsStdString());
                    }
                    continue;
                }
                iTJSDispatch2 *temp = getOrCreateTempLayer(tempParent);
                if(!temp) continue;
                if(!tryLoadImage(temp, path)) continue;
                tTJSVariant wVar, hVar;
                temp->PropGet(0, TJS_W("imageWidth"), nullptr, &wVar, temp);
                temp->PropGet(0, TJS_W("imageHeight"), nullptr, &hVar, temp);
                const int iw = static_cast<int>(wVar.AsInteger());
                const int ih = static_cast<int>(hVar.AsInteger());
                if(iw <= 0 || ih <= 0) continue;
                const float px = active->ox + active->cx;
                const float py = active->oy + active->cy;
                const int left = _coordX + halfCw + static_cast<int>(px) - iw / 2;
                const int top  = _coordY + halfCh + static_cast<int>(py) - ih / 2;
                const int opacity = std::clamp(static_cast<int>(active->opacity), 0, 255);
                if(opacity <= 0) continue;
                tTJSVariant opArgs[9] = {
                    tTJSVariant(static_cast<tjs_int>(left)),
                    tTJSVariant(static_cast<tjs_int>(top)),
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
                    if(auto l = _logger()) l->warn("drawAnimatedFlat: operateRect exception: {}", e.what());
                } catch(...) {
                    if(auto l = _logger()) l->warn("drawAnimatedFlat: operateRect unknown exception");
                }
            }
            return drawn;
        }

        void drawPSBImages(iTJSDispatch2 *target, const ttstr &storage,
                           const std::shared_ptr<spdlog::logger> &logger) {
            if(_psbImages.empty() && _motionTracks.empty()) return;
            // When captureCanvas is active it is the single source that draws the
            // animation onto the on-screen layer each frame; Player::draw would
            // double-draw the same frames onto the game layer and cause overlap
            // artifacts. Only cache/load here; the actual draw happens in drawOnto.
            // 当 captureCanvas 活跃时，它是唯一把动画画上屏层的画源；Player::draw 若再画
            // 会双画同一份帧到游戏层导致叠影。此处只缓存/加载，真正绘制由 drawOnto 完成。
            if(_captureActive) {
                if(logger) logger->info("drawPSBImages: captureCanvas active, skip draw (cache only)");
                return;
            }

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

            // M2 animation: when the per-motion frame time-lines are available,
            // evaluate the active frame at the current clock instead of compositing
            // every cached frame statically. Falls back to the static composite
            // when no tracks were extracted.
            // M2 动画：有该 motion 的帧时间线时，按当前时钟画活跃帧，而不是把所有缓存帧
            // 一次静态合成；无时间线时回退静态合成。
            if(!_motionTracks.empty()) {
                int d = drawAnimated(realLayer, tempParent, logger);
                if(logger) logger->info("drawAnimated: drew {} images at tick={}",
                                        d, static_cast<tjs_int>(_tickCount));
                // M2 multi-layer: the motion node tree animates SOME layers (backdrop,
                // text, confetti) while OTHER layers are STATIC elements that must still
                // composite each frame (e.g. the m2logo "M"/cross pieces in
                // _psbImages — without this they never appear during back_white). Port:
                // composite the cached static images whose src the motion does NOT draw.
                // M2 多层：motion 节点树只动画部分图层（底布/文字/纸屑），其它图层是
                // **静态元素**（如 m2logo 的 M 字/十字碎片在 _psbImages 里）——动画时若
                // 不补合成，它们全程不显示（back_white 缺 M 字即此因）。移植：把 motion
                // 没画到的静态缓存图也合成。
                compositeStaticLayersNotAnimated(realLayer, tempParent, logger);
                return;
            }

            if(logger) logger->info("drawPSBImages: drew {} of {} images",
                                    compositeTo(realLayer, tempParent, logger),
                                    _psbImages.size());
        }

        // Composite the cached STATIC layer images that the current motion does NOT
        // animate. In M2 a logo is a multi-layer composition: the motion node tree
        // drives SOME layers (backdrop/text/confetti) while OTHER layers are static
        // elements that must still render every frame (e.g. the m2logo "M"/cross pieces).
        // drawAnimated only renders the motion nodes, so without this the static pieces
        // never appear. Skip images whose src the motion already drew (avoid double-draw).
        // 合成当前 motion **没有动画**的静态缓存图层。M2 的 logo 是多图层合成：motion 节点树
        // 只驱动部分图层（底布/文字/纸屑），其它是**静态元素**，每帧都要渲染（如 m2logo 的
        // M 字/十字碎片）。drawAnimated 只渲染 motion 节点，缺此则静态碎片从不显示。跳过
        // motion 已画到的图（避免双画）。
        int compositeStaticLayersNotAnimated(iTJSDispatch2 *dest, iTJSDispatch2 *tempParent,
                                             const std::shared_ptr<spdlog::logger> &logger) {
            if(!dest || _psbImages.empty()) return 0;
            std::set<std::string> animRes;
            for(const auto &nd : _motionNodes)
                for(const auto &f : nd.frames)
                    if(f.src.size() > 4 && f.src.compare(0, 4, "src/") == 0)
                        animRes.insert(MotionSrcToResource(f.src));
            for(const auto &tr : _motionTracks)
                for(const auto &f : tr.frames)
                    if(f.src.size() > 4 && f.src.compare(0, 4, "src/") == 0)
                        animRes.insert(MotionSrcToResource(f.src));
            int drawn = 0;
            for(const auto &img : _psbImages) {
                bool animated = false;
                for(const auto &r : animRes)
                    if(img.key.find(r) != std::string::npos) { animated = true; break; }
                if(animated) continue; // already drawn by the motion / 已由 motion 画出
                iTJSDispatch2 *temp = getOrCreateTempLayer(tempParent);
                if(!temp) continue;
                if(!tryLoadImage(temp, img.path)) continue;
                tTJSVariant wVar, hVar;
                temp->PropGet(0, TJS_W("imageWidth"), nullptr, &wVar, temp);
                temp->PropGet(0, TJS_W("imageHeight"), nullptr, &hVar, temp);
                int iw = static_cast<int>(wVar.AsInteger());
                int ih = static_cast<int>(hVar.AsInteger());
                if(iw <= 0 || ih <= 0) continue;
                int op = std::min(img.opacity, 255);
                if(op <= 0) continue;
                if(logger) logger->info("compositeStatic: add '{}' @({},{}) {}x{}", img.key,
                    img.left, img.top, iw, ih);
                tTJSVariant opArgs[9] = {
                    tTJSVariant(static_cast<tjs_int>(img.left)),
                    tTJSVariant(static_cast<tjs_int>(img.top)),
                    tTJSVariant(temp, temp),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(iw)),
                    tTJSVariant(static_cast<tjs_int>(ih)),
                    tTJSVariant(static_cast<tjs_int>(2)),  // omAlpha
                    tTJSVariant(static_cast<tjs_int>(op)),
                };
                tTJSVariant *opArgv[] = { &opArgs[0], &opArgs[1], &opArgs[2],
                                          &opArgs[3], &opArgs[4], &opArgs[5],
                                          &opArgs[6], &opArgs[7], &opArgs[8] };
                try {
                    dest->FuncCall(0, TJS_W("operateRect"), nullptr, nullptr, 9, opArgv, dest);
                    drawn++;
                } catch(const std::exception &e) {
                    if(auto l = _logger()) l->warn("compositeStatic: operateRect exception: {}", e.what());
                } catch(...) {}
            }
            if(logger) logger->info("compositeStatic: drew {} static pieces", drawn);
            return drawn;
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
            _captureActive = true;
            const ttstr storage = _loadedStorage.IsEmpty() ? ResourceManager::getLastLoadedPath()
                                                           : _loadedStorage;
            if(storage.IsEmpty()) return;
            auto logger = _logger();
            try {
                if(!_psbImagesCached) {
                    cachePSBImages(storage, logger);
                }
                if(_psbImages.empty() && _motionTracks.empty()) return;
                iTJSDispatch2 *realLayer = resolveRealLayer(target);
                iTJSDispatch2 *tempParent = realLayer ? realLayer : target;
                // Ensure the per-motion timeline is loaded BEFORE drawing. Without
                // this the capture path could hit its first frame with an empty
                // _motionTracks and emit the STATIC full image set (background +
                // full yuzu_logo at full opacity) onto the white screen — the
                // "播放前背景有完整静止 logo" artifact.
                // 绘制前务必已加载该 motion 的帧时间线。否则 capture 路径第一帧可能
                // _motionTracks 仍为空，退回去画**静态全量图集**（背景 + 完整静止
                // yuzu_logo、不透明）——这正是"播放前背景有完整静止 logo"的来源。
                if(!_motionTracksLoaded) {
                    loadMotionTracks(storage);
                }
                // IMPORTANT: captureCanvas's destination layer IS the layer that
                // reaches the screen, so it must receive the ANIMATION frame, not
                // the static full composite. When a motion timeline exists it is
                // AUTHORITATIVE: an empty (all-invisible) frame at the current tick
                // means "draw nothing" for that motion — NOT "fall back to the
                // static composite" (which would pop in a full logo that the
                // timeline keeps transparent). Static composite is used only when
                // this motion has NO timeline at all (a plain image scene).
                // 重要：captureCanvas 的目标层就是真正上屏的层，必须画**动画帧**而非静态
                // 全量合成。一旦存在 motion 时间线它就是**权威**：当前 tick 为空
                //（全部不可见）时表示该 motion 此刻"不画任何东西"——绝不能回退成静态
                // 全量合成（那会把时间线始终保持透明的完整 logo 闪回屏上）。仅当该
                // motion 完全没有时间线（纯图像场景）时才用静态合成。
                // Clear the capture layer FIRST so per-frame animation replaces the
                // previous frame instead of stacking (which produced color blocks).
                // 先清空 capture 层，让每帧动画**替换**上一帧而非叠加（叠加曾产生色块）。
                clear(target, 0);
                int drawn = 0;
                if(!_motionTracks.empty() || !_motionNodes.empty()) {
                    drawn = drawAnimated(target, tempParent, logger);
                } else {
                    drawn = compositeTo(target, tempParent, logger);
                }
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

        // ③ stencil offscreen layer provisioning: a type-12 composite buffers its
        // content (group) and its mask (mask) into two canvas-sized scratch layers
        // which are alpha-multiplied at the end of drawAnimatedTree. Both are inert
        // (invisible, same parent chain as the shared temp layer) and reused across
        // frames until cleanupTempLayer().
        // ③ stencil 离屏层供给：type-12 合成组把内容（group）与蒙版（mask）分别缓冲到
        // 两块画布大小的临时层，在 drawAnimatedTree 末尾做 alpha 相乘合成。两层保持不可见
        //（与共享 temp 层同父链），跨帧复用直到 cleanupTempLayer()。
        iTJSDispatch2 *getOrCreateStencilLayer(iTJSDispatch2 *realLayer, bool isMask,
                                          float canvasW, float canvasH) {
            iTJSDispatch2 *&slot = isMask ? _stencilMaskLayer : _stencilGroupLayer;
            if(slot) return slot;
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
                // Size the scratch to the canvas so group/mask draws land at raw
                // canvas coords (K2 Layer supports width/height resize; ignored if
                // the runtime rejects it — the composite probe will tell).
                // 把离屏层设成画布大小，让组/蒙版绘制落在原始画布坐标（K2 Layer 支持
                // width/height 调整；若运行时拒绝则忽略——合成探针会反映）。
                if(canvasW > 0.0f || canvasH > 0.0f) {
                    tTJSVariant wVarV(static_cast<tjs_real>(canvasW));
                    tTJSVariant hVarV(static_cast<tjs_real>(canvasH));
                    if(canvasW > 0.0f)
                        newLayer->PropSet(TJS_MEMBERENSURE, TJS_W("width"), nullptr, &wVarV, newLayer);
                    if(canvasH > 0.0f)
                        newLayer->PropSet(TJS_MEMBERENSURE, TJS_W("height"), nullptr, &hVarV, newLayer);
                }
                slot = newLayer;
                return slot;
            } catch(...) {
                return nullptr;
            }
        }

        // ③ Alpha-multiply the mask layer into the group layer (in place), then blit
        // the group to `dest`. stencilType 1 = normal (groupA * maskA), 2 = reverse
        // (groupA * (255-maskA)). The mask's RGB is ignored — the stencil composites
        // alpha topology only (K2 item+264 semantics).
        // ③ 把蒙版层 alpha 乘进组层（就地），再把组层合成到 dest。stencilType 1=正常
        //（组A×蒙版A）、2=反向（组A×(255-蒙版A)）。只消费蒙版 alpha（RGB 忽略），
        // 对应 K2 item+264 语义。
        bool applyStencilComposite(iTJSDispatch2 *groupLayer, iTJSDispatch2 *maskLayer,
                                   int stencilType) {
            if(!groupLayer || !maskLayer) return false;
            bool recoveredRgbAlpha = false;
            try {
                tTJSNI_Layer *groupNI = nullptr, *maskNI = nullptr;
                if(TJS_FAILED(groupLayer->NativeInstanceSupport(
                       TJS_NIS_GETINSTANCE, tTJSNC_Layer::ClassID,
                       (iTJSNativeInstance **)&groupNI)) || !groupNI) return false;
                if(TJS_FAILED(maskLayer->NativeInstanceSupport(
                       TJS_NIS_GETINSTANCE, tTJSNC_Layer::ClassID,
                       (iTJSNativeInstance **)&maskNI)) || !maskNI) return false;
                const tjs_int w = groupNI->GetWidth(), h = groupNI->GetHeight();
                if(w <= 0 || h <= 0) return false;
                const bool reverse = (stencilType & 0x2) != 0;
                unsigned char *gbuf = (unsigned char *)groupNI->GetMainImagePixelBufferForWrite();
                unsigned char *mbuf = (unsigned char *)maskNI->GetMainImagePixelBufferForWrite();
                const tjs_int gpitch = groupNI->GetMainImagePixelBufferPitch();
                const tjs_int mpitch = maskNI->GetMainImagePixelBufferPitch();
                if(!gbuf || !mbuf || gpitch < w * 4 || mpitch < w * 4) return false;
                for(tjs_int y = 0; y < h; ++y) {
                    unsigned char *mrow = mbuf + (tjs_int)y * mpitch;
                    unsigned char *grow = gbuf + (tjs_int)y * gpitch;
                    for(tjs_int x = 0; x < w; ++x) {
                        const size_t off = static_cast<size_t>(x) * 4u;
                        unsigned char maskA = mrow[off + 3];
                        // K2 stencil masks may carry their alpha as RGB rotation
                        // (RGB-only textures): recover alpha = max(R,G,B) when the
                        // alpha channel is empty but the color isn't (reference
                        // recoverRgbEncodedDifferenceAlphaMask).
                        // K2 stencil 蒙版可能把 alpha 存在 RGB（纯 RGB 纹理）：
                        // alpha 通道为空但 RGB 非空时，取 alpha = max(R,G,B)（参考
                        // recoverRgbEncodedDifferenceAlphaMask）。
                        if(maskA == 0 &&
                           (mrow[off] != 0 || mrow[off + 1] != 0 || mrow[off + 2] != 0)) {
                            const int m = std::max(static_cast<int>(mrow[off]),
                                                   std::max(static_cast<int>(mrow[off + 1]),
                                                            static_cast<int>(mrow[off + 2])));
                            maskA = static_cast<unsigned char>(m);
                            recoveredRgbAlpha = true;
                        }
                        const unsigned char prevA = grow[off + 3];
                        if(prevA == 0) continue;
                        const unsigned char newA = reverse
                            ? static_cast<unsigned char>((static_cast<int>(prevA) * (255 - maskA)) / 255)
                            : static_cast<unsigned char>((static_cast<int>(prevA) * maskA) / 255);
                        grow[off + 3] = newA;
                    }
                }
                groupNI->Update(tTVPRect(0, 0, w, h));
#if defined(KRKR_RENDER_PROBE)
                auto logger = _logger();
                if(logger && recoveredRgbAlpha)
                    logger->info("applyStencilComposite: RGB-rotation alpha recovered in mask");
#endif
                return true;
            } catch(...) {
                return false;
            }
        }

        // Apply an M2 PER-CORNER packedColors tint (bilinear across the 4 corners,
        // ported from AetherKiri applyPackedCornerTintLike_0x6A7518) to the just-loaded
        // glyph texture before Layer.operateAffine (which has no color channel):
        //   out.rgb = lerp4corner(tint) * src.rgb / 255,  out.a = lerp4corner(tint.a)*src.a/255.
        // All-opaque-white corners are the identity and are SKIPPED, keeping the pixel
        // work limited to the colored sprites (C/W red, thin red→black line, black cross).
        // 对刚加载的字形纹理应用 M2 **四角** packedColors 平涂（按 AetherKiri
        // applyPackedCornerTintLike 移植的四角双线性）：out.rgb = 四角插值tint × src.rgb/255，
        // out.a = 四角插值tint.a × src.a/255。四角全不透明白＝恒等、直接跳过，逐像素开销
        // 只落在着色精灵上（C/W 红、红转黑细线、黑色十字）。
        static bool applyCornerTint(
            iTJSDispatch2 *temp,
            const std::array<std::uint32_t, 4> &tints) {
            bool neutral = true;
            for(const auto t : tints) {
                if(t != 0xFFFFFFFFu) { neutral = false; break; }
            }
            if(neutral) return true; // identity / 恒等
            try {
                tTJSNI_Layer *ni = nullptr;
                if(TJS_FAILED(temp->NativeInstanceSupport(
                       TJS_NIS_GETINSTANCE, tTJSNC_Layer::ClassID,
                       (iTJSNativeInstance **)&ni)) || !ni)
                    return false;
                const tjs_int w = ni->GetWidth(), h = ni->GetHeight();
                if(w <= 0 || h <= 0) return false;
                // corner order: [0]=topLeft [1]=topRight [2]=bottomRight [3]=bottomLeft
                // 角序：[0]=左上 [1]=右上 [2]=右下 [3]=左下
                auto unpack = [](std::uint32_t v, int out[4]) {
                    out[0] = static_cast<int>((v >> 16) & 0xffu); // R
                    out[1] = static_cast<int>((v >> 8) & 0xffu);  // G
                    out[2] = static_cast<int>((v >> 0) & 0xffu);  // B
                    out[3] = static_cast<int>((v >> 24) & 0xffu); // A
                };
                int tl[4], tr[4], br[4], bl[4];
                unpack(tints[0], tl); unpack(tints[1], tr);
                unpack(tints[2], br); unpack(tints[3], bl);
                // corners frequently animate as one uniform value; lerp(x,x)=x so the
                // uniform fast path is bit-equivalent to the general bilinear loop.
                // 四角常为同一值动画；均匀色下 lerp(x,x)≡x，快速路径与通用双线性逐位等价。
                const bool uniform =
                    tl[0] == tr[0] && tl[0] == br[0] && tl[0] == bl[0] &&
                    tl[1] == tr[1] && tl[1] == br[1] && tl[1] == bl[1] &&
                    tl[2] == tr[2] && tl[2] == br[2] && tl[2] == bl[2] &&
                    tl[3] == tr[3] && tl[3] == br[3] && tl[3] == bl[3];
                const int spanX = std::max(w - 1, 1);
                const int spanY = std::max(h - 1, 1);
                auto lerpCh = [](int a, int b, int pos, int span) -> int {
                    return a + (pos * (b - a)) / span;
                };
                unsigned char *buf = (unsigned char *)ni->GetMainImagePixelBufferForWrite();
                const tjs_int pitch = ni->GetMainImagePixelBufferPitch();
                if(!buf || pitch < w * 4) return false;
                // Apply the bilinear corner tint to each non-transparent source pixel.
                // 对每个非透明源像素应用四角双线性颜色。
                for(tjs_int y = 0; y < h; ++y) {
                    tjs_uint32 *row = (tjs_uint32 *)(buf + (tjs_int)y * pitch);
                    // left column lerps topLeft↔bottomLeft, right column topRight↔bottomRight
                    // 左列 左上↔左下，右列 右上↔右下
                    const int rowLR = lerpCh(tl[0], bl[0], y, spanY);
                    const int rowLG = lerpCh(tl[1], bl[1], y, spanY);
                    const int rowLB = lerpCh(tl[2], bl[2], y, spanY);
                    const int rowLA = lerpCh(tl[3], bl[3], y, spanY);
                    const int rowRR = lerpCh(tr[0], br[0], y, spanY);
                    const int rowRG = lerpCh(tr[1], br[1], y, spanY);
                    const int rowRB = lerpCh(tr[2], br[2], y, spanY);
                    const int rowRA = lerpCh(tr[3], br[3], y, spanY);
                    for(tjs_int x = 0; x < w; ++x) {
                        const tjs_uint32 px = row[x];
                        const tjs_uint32 srcA = (px >> 24) & 0xffu;
                        if(srcA == 0) continue; // keep transparent / 保持透明
                        const int tintR = uniform
                            ? rowLR : lerpCh(rowLR, rowRR, x, spanX);
                        const int tintG = uniform
                            ? rowLG : lerpCh(rowLG, rowRG, x, spanX);
                        const int tintB = uniform
                            ? rowLB : lerpCh(rowLB, rowRB, x, spanX);
                        const int tintA = uniform
                            ? rowLA : lerpCh(rowLA, rowRA, x, spanX);
                        const tjs_uint32 srcR = (px >> 16) & 0xffu;
                        const tjs_uint32 srcG = (px >> 8) & 0xffu;
                        const tjs_uint32 srcB = (px >> 0) & 0xffu;
                        // premultiplied-ARGB multiply by the tint / 预乘 ARGB 乘上 tint
                        const tjs_uint32 nr = std::min(255, tintR * static_cast<int>(srcR) / 255);
                        const tjs_uint32 ng = std::min(255, tintG * static_cast<int>(srcG) / 255);
                        const tjs_uint32 nb = std::min(255, tintB * static_cast<int>(srcB) / 255);
                        const tjs_uint32 na = std::min(255, tintA * static_cast<int>(srcA) / 255);
                        row[x] = (nb & 0xffu) | ((ng & 0xffu) << 8) |
                                 ((nr & 0xffu) << 16) | (na << 24);
                    }
                }
                ni->Update(tTVPRect(0, 0, w, h)); // notify the layer its pixels changed / 通知层像素已变
                return true;
            } catch(...) {
                return false;
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
            // ③ stencil offscreen layers follow the same lifecycle.
            // ③ stencil 离屏层走同一生命周期。
            for(iTJSDispatch2 **pp : { &_stencilGroupLayer, &_stencilMaskLayer }) {
                if(*pp) {
                    try {
                        (*pp)->FuncCall(0, TJS_W("invalidate"), nullptr, nullptr, 0, nullptr, *pp);
                    } catch(...) {}
                    (*pp)->Release();
                    *pp = nullptr;
                }
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
        // Build the local 2x2 linear matrix from a node's accumulated flip/angle/scale,
        // LEFT-multiplying each transform in `order` (default [0,1,2,3] = flip, angle,
        // scale, slant). Faithful port of libkrkr2 sub_699940 / applyLocalTransform.
        // Used to compute each node's WORLD matrix so a rotated/scaled parent transforms
        // its children's positions (gap-1 alignment).
        // 依 `order`（默认 [0,1,2,3]=flip,angle,scale,s slant）把 flip/angle/scale 左乘到
        // 局部 2×2 线性矩阵。忠实移植 libkrkr2 sub_699940 / applyLocalTransform。用于算
        // 每个节点的**世界矩阵**，使旋转/缩放的父节点能变换子节点位置（缺口①对齐）。
        // Cubic-bezier easing solver for M2 `ccc` curves (a bezier from (0,0) to (1,1),
        // control points (x1,y1),(x2,y2)); given linear progress u in [0,1] it returns the
        // eased value. Mirrors the reference's per-frame easing so the leaf swing and the
        // m2logo letter slides decelerate smoothly instead of hitting big keyframes
        // linearly. Solution via bisection on x(t)=u (x(t) is monotonic for valid easing).
        // 三次贝塞尔缓动求解器（M2 `ccc` 曲线：从 (0,0) 到 (1,1)，控制点 (x1,y1),(x2,y2)）；
        // 给定线性进度 u∈[0,1]，返回缓动后的值。对准参考的逐帧缓动，让叶子摆动与 m2logo
        // 字母滑入平滑减速，而不是在大关键帧间线性生硬直连。用二分求解 x(t)=u（对合法
        // 缓动 x(t) 单调）。
        static double BezierEase(double u, double x1, double y1, double x2, double y2) {
    double lo = 0.0, hi = 1.0;
    for(int it = 0; it < 40; ++it) {
        const double t = (lo + hi) * 0.5;
        const double inv = 1.0 - t;
        const double xt = 3.0 * inv * inv * t * x1 +
                          3.0 * inv * t * t * x2 + t * t * t;
        if(xt < u) {
            lo = t;
        } else {
            hi = t;
        }
    }
    const double t = (lo + hi) * 0.5;
    const double inv = 1.0 - t;
    return 3.0 * inv * inv * t * y1 + 3.0 * inv * t * t * y2 + t * t * t;
}

static void buildLocalMatrix(bool fx, bool fy, double ang, double sx, double sy,
                                     double slx, double sly,
                                     const int (&order)[4],
                                     double &l11, double &l12, double &l21, double &l22) {
            l11 = 1.0; l12 = 0.0; l21 = 0.0; l22 = 1.0;
            for(int k = 0; k < 4; k++) {
                switch(order[k]) {
                    case 0: // flip: negate row1 (X) / row2 (Y)
                        if(fx) { l11 = -l11; l12 = -l12; }
                        if(fy) { l21 = -l21; l22 = -l22; }
                        break;
                    case 1: // angle: left-multiply [[c,-s],[s,c]]
                        if(ang != 0.0) {
                            const double rad = ang * 2.0 * 3.14159265358979323846 / 360.0;
                            const double c = std::cos(rad), s = std::sin(rad);
                            const double t11 = c * l11 - s * l21;
                            const double t12 = c * l12 - s * l22;
                            const double t21 = s * l11 + c * l21;
                            const double t22 = s * l12 + c * l22;
                            l11 = t11; l12 = t12; l21 = t21; l22 = t22;
                        }
                        break;
                    case 2: // scale: left-multiply [[sx,0],[0,sy]]
                        l11 *= sx; l12 *= sx; l21 *= sy; l22 *= sy;
                        break;
                    case 3: // slant: left-multiply [[1,slx],[sly,1]]
                        // libkrkr2 sub_699940 / AetherKiri applyLocalTransform case 3
                        // — previously skipped, so slanted nodes lost their skew.
                        // libkrkr2 sub_699940 / AetherKiri applyLocalTransform 的
                        // case 3——此前跳过，带斜切的节点丢失斜切。
                        if(slx != 0.0 || sly != 0.0) {
                            const double t11 = l11 + slx * l21;
                            const double t12 = l12 + slx * l22;
                            const double t21 = sly * l11 + l21;
                            const double t22 = sly * l12 + l22;
                            l11 = t11; l12 = t12; l21 = t21; l22 = t22;
                        }
                        break;
                }
            }
        }

        // Restore a layer's clip rect to a previously saved state (used to undo a
        // str_clip crop once its letter subtree is done). The Layer's clip is a
        // native member, not an iTJSDispatch2 method, so we reach it through the
        // script-side properties/method: read clipLeft/clipTop/clipWidth/clipHeight
        // and write back via setClip(l,t,w,h). setClip with 0 args resets to the
        // full layer.
        // 把层的裁剪矩形恢复为之前保存的状态（str_clip 字母子树绘制完后撤除裁切）。
        // Layer 的裁剪是原生成员，不在 iTJSDispatch2 接口上，故经脚本侧属性/方法访问：
        // 读 clipLeft/clipTop/clipWidth/clipHeight，再用 setClip(l,t,w,h) 写回；
        // setClip 无参调用则重置为整层。
        struct ClipState { int l = 0, t = 0, w = 0, h = 0; };

        static bool readClip(iTJSDispatch2 *layer, ClipState &out) {
            if(!layer) return false;
            tTJSVariant v;
            if(TJS_FAILED(layer->PropGet(0, TJS_W("clipLeft"), nullptr, &v, layer)))
                return false;
            out.l = static_cast<int>(v.AsInteger());
            if(TJS_FAILED(layer->PropGet(0, TJS_W("clipTop"), nullptr, &v, layer)))
                return false;
            out.t = static_cast<int>(v.AsInteger());
            if(TJS_FAILED(layer->PropGet(0, TJS_W("clipWidth"), nullptr, &v, layer)))
                return false;
            out.w = static_cast<int>(v.AsInteger());
            if(TJS_FAILED(layer->PropGet(0, TJS_W("clipHeight"), nullptr, &v, layer)))
                return false;
            out.h = static_cast<int>(v.AsInteger());
            return true;
        }

        static void writeClip(iTJSDispatch2 *layer, const ClipState &c) {
            if(!layer) return;
            tTJSVariant args[4] = {
                tTJSVariant(static_cast<tjs_int>(c.l)),
                tTJSVariant(static_cast<tjs_int>(c.t)),
                tTJSVariant(static_cast<tjs_int>(c.w)),
                tTJSVariant(static_cast<tjs_int>(c.h)),
            };
            tTJSVariant *argv[4] = { &args[0], &args[1], &args[2], &args[3] };
            try {
                layer->FuncCall(0, TJS_W("setClip"), nullptr, nullptr, 4, argv, layer);
            } catch(...) {}
        }

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
        // Node index of the currently active str_clip text crop, or -1 when none. Reset on
        // play() and cleared as soon as a node outside that str_clip's subtree is drawn,
        // so the crop can't leak across motions/frames.
        // 当前生效的 str_clip 文本裁切的节点索引，-1=无。play() 重置；一旦画到该
        // str_clip 子树之外的节点就清除，防止裁剪跨 motion/帧残留。
        int _strClipActiveParent = -1;

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
        // M2 animation: per-motion layer frame time-lines (see PSBMedia). Empty →
        // fall back to the static full-frame composite. Loaded after the archive
        // is parsed by cachePSBImages(). Reset on play() so a new motion reloads.
        // M2 动画：每个 motion 图层的帧时间线（见 PSBMedia）。为空则回退静态合成。
        // 在 cachePSBImages() 解析归档后加载；play() 时重置以便新 motion 重载。
        bool _motionTracksLoaded = false;
        std::vector<PSB::PSBMedia::PSBMotionLayerTrack> _motionTracks;
        std::vector<PSB::PSBMedia::PSBMotionNode> _motionNodes;
        // Set while D3DAdaptor/SeparateLayerAdaptor.captureCanvas is driving the
        // on-screen draw (single draw source); Player::draw then only caches/loads.
        // captureCanvas 驱动上屏绘制（单一画源）时置位；Player::draw 此时只缓存/加载。
        bool _captureActive = false;
        int _psbCacheRetries = 0;
        std::vector<PSBImageEntry> _psbImages;
        iTJSDispatch2 *_tempLayer = nullptr;

        struct ButtonBounds {
            int left = 0, top = 0, width = 0, height = 0;
        };
        std::unordered_map<std::string, ButtonBounds> _buttonBounds;

        std::unordered_map<std::string, tTJSVariant> _variables;

        // ④ motionDt mode-2 delta pos: previous pose of each node label (persists
        // across drawAnimatedTree calls so the atan2(delta) uses real frame motion).
        // ④ motionDt 模式 2 的位移差：每个节点标签的上一姿态（跨 drawAnimatedTree
        // 调用保留，让 atan2(位移差) 用真实帧间运动）。
        std::unordered_map<std::string, float> _lastFramePosX;
        std::unordered_map<std::string, float> _lastFramePosY;
        // ③ stencil: offscreen group/mask temp layers (created only when a stencil
        // composite is actually present). Reused across frames; invalidated in
        // cleanupTempLayer().
        // ③ stencil：离屏组/蒙版临时层（仅在确有 stencil 合成时创建）。跨帧复用，
        // cleanupTempLayer() 里失效。
        iTJSDispatch2 *_stencilGroupLayer = nullptr;
        iTJSDispatch2 *_stencilMaskLayer = nullptr;
    };

} // namespace motion
