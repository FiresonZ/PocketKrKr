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
            _motionTracksLoaded = false;
            _motionTracks.clear();
            _motionNodes.clear();
            _captureActive = false;
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
                    f.src.clear();
                    // Append the sub-subtree rooted under this referencing node,
                    // remapping internal parent edges by the insertion offset.
                    // 把子子树以本引用节点为父追加，按插入偏移重映射内部父子边。
                    const int selfIdx = static_cast<int>(i);
                    const int offset = static_cast<int>(nodes.size());
                    for(auto &c : child) {
                        if(c.parentIndex == -1) c.parentIndex = selfIdx;
                        else c.parentIndex += offset;
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
            // M2 text-layout subtrees: a node whose ancestor chain contains a
            // "str_*" container (str_clip / str_locate, e.g. the m2logo
            // "cheeseware" letters) is positioned by the TEXT pen — each glyph
            // bitmap is drawn LEFT-ALIGNED at its advance anchor, so
            // variable-width letters tile without overlapping. Ordinary image
            // nodes (yuzu logo letters etc.) stay center-anchored. Precompute
            // once per motion; parentIndex is guaranteed parent-before-child.
            // M2 文本排版子树：祖先链含 "str_*" 容器（str_clip/str_locate，如 m2logo
            // 的 "cheeseware" 字母）的节点按**文本笔位**定位——每个字形位图在它的
            // advance 锚点处**左对齐**绘制，可变宽度字母才能依次排开不重叠；普通图像
            // 节点（yuzu logo 字母等）保持居中锚定。每 motion 预计算一次；
            // parentIndex 保证父先于子。
            _nodeInStrSubtree.assign(_motionNodes.size(), false);
            for(size_t ni = 0; ni < _motionNodes.size(); ni++) {
                const int pi = _motionNodes[ni].parentIndex;
                if(pi < 0) continue; // root: not a text leaf
                if(_nodeInStrSubtree[static_cast<size_t>(pi)] ||
                   _motionNodes[static_cast<size_t>(pi)].label.compare(0, 3, "str") == 0) {
                    _nodeInStrSubtree[ni] = true;
                }
            }
            if(auto l = _logger()) {
                l->info("loadMotionTracks: {} tracks for {}/{} motion={}",
                    _motionTracks.size(), storageStr, charaStr, motionStr);
                for(const auto &tr : _motionTracks) {
                    l->info("  track '{}': {} frames (first src='{}', last time={})",
                        tr.label, tr.frames.size(),
                        tr.frames.empty() ? std::string("") : tr.frames.front().src,
                        tr.frames.empty() ? 0 : tr.frames.back().time);
                    // Dump every frame (time, src, ox/oy/cx/cy, opacity, visible) once
                    // per loaded motion so we can see the real M2 timeline and implement
                    // the coord/opacity animation correctly rather than guessing.
                    // 每帧转储（time, src, ox/oy/cx/cy, opacity, visible），只在 motion
                    // 装载时打一次，据此拿到真实的 M2 时间线，按真实坐标实现动画而非猜测。
                    for(const auto &f : tr.frames) {
                        l->info("    t={} ty={} src='{}' ox={} oy={} cx={} cy={} op={} vis={}",
                            f.time, f.type, f.src, f.ox, f.oy, f.cx, f.cy, f.opacity,
                            f.visible ? 1 : 0);
                    }
                }
                l->info("loadMotionTracks: {} nodes for {}/{} motion={} (tree, "
                        "parent-before-child)",
                    _motionNodes.size(), storageStr, charaStr, motionStr);
                for(size_t ni = 0; ni < _motionNodes.size(); ni++) {
                    const auto &nd = _motionNodes[ni];
                    l->info("  node[{}] '{}' parent={} frames={}",
                        ni, nd.label, nd.parentIndex,
                        static_cast<int>(nd.frames.size()));
                    for(const auto &f : nd.frames) {
                        l->info("    n[{}] t={} ty={} src='{}' ox={} oy={} cx={} cy={} op={} vis={}",
                            ni, f.time, f.type, f.src, f.ox, f.oy, f.cx, f.cy, f.opacity,
                            f.visible ? 1 : 0);
                    }
                }
            }
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
            std::vector<float> wx(n, 0.0f), wy(n, 0.0f);
            std::vector<int> wo(n, 255);
            std::vector<bool> vis(n, true);
            int drawn = 0;
            for(int i = 0; i < n; i++) {
                const auto &node = _motionNodes[i];
                // Reference semantics (PlayerFrameProgress + PlayerUpdateLayerEval):
                // - A content frame marks the node visible; a "no content" frame
                //   (src empty / type-0) marks it invisible only while later content
                //   still exists in the timeline.
                // - Once the timeline is exhausted (now >= last frame time), the
                //   node HOLDS the last content frame (静止, motion finished) — it
                //   does NOT hide. Only a declared loopTime (loop motion) rewinds.
                // - Mid-timeline empty frames (e.g. a layer that appears at t=90)
                //   are hidden before their first content frame.
                // 参考语义（PlayerFrameProgress + PlayerUpdateLayerEval）：
                // - 有内容帧使节点可见；"无内容"帧（src 空/type-0）仅在时间线后面还有
                //   内容帧时代表不可见。
                // - 时间线播完（now >= 末帧时间）后节点**保持最后一帧内容**（静止），
                //   不隐藏；仅声明了 loopTime 的循环 motion 才回绕。
                // - 时间线中间的空帧（如 t=90 才出现的层）在首个内容帧之前隐藏。
                const auto &frames = node.frames;
                // Last content frame time (the "end" of this node's timeline).
                // 该节点时间线的末内容帧时间。
                tjs_int lastContentTime = -1;
                for(const auto &f : frames) {
                    if(f.visible && f.src.size() > 4 &&
                       f.src.compare(0, 4, "src/") == 0) {
                        if(f.time > lastContentTime) lastContentTime = f.time;
                    }
                }
                // Active frame = last frame with time <= now (per-frame evaluation).
                // 活跃帧 = time <= now 的最后一帧（逐帧求值）。
                const PSB::PSBMedia::PSBMotionFrame *af = nullptr;
                for(const auto &f : frames) {
                    if(f.time <= now) af = &f; else break;
                }
                if(!af) { vis[i] = false; continue; }
                if(!af->visible) {
                    // No-content frame hides the node for its time range — INCLUDING
                    // the final empty frames of the timeline. Logo scenes end by
                    // hiding their layers (yuzulogo letters/kanji disappear at the
                    // t=215f empty keyframe before the m2logo transition); holding
                    // the last content frame instead left the complete static logo
                    // visible in the background ("播放前背景有完整静止 yuzulogo").
                    // Only container nodes (layout / submotion parents with no
                    // content frame of their own) stay active so their children
                    // keep driving visibility. Steady-state motions (normal/status)
                    // simply have content frames at the end of their timeline.
                    // 无内容帧在它所覆盖的时间段内隐藏节点——**包括时间线末尾的空帧**。
                    // logo 场景以隐藏图层收尾（yuzulogo 字母/柚子汉字在 t=215f 的空
                    // 关键帧处消失，再切 m2logo）；此前"保持最后一帧内容"反而让完整的
                    // 静止 logo 一直留在背景里（"播放前背景有完整静止 yuzulogo"）。
                    // 只有无自身内容帧的容器节点（layout/子运动父节点）保持活跃，
                    // 由子层驱动可见性。稳态 motion（normal/status）时间线末尾
                    // 本来就是内容帧，不受影响。
                    if(lastContentTime < 0) {
                        // Container node (layout / submotion parent) with no content
                        // frame of its own: it never hides its subtree (mirrors the
                        // reference where a type-3/motion container stays active and
                        // the child motion drives visibility).
                        // 无自身内容帧的容器节点（layout / 子运动父节点）：永远不隐藏
                        // 子树（对应参考中 type-3/motion 容器持续 active，由子运动决定可见性）。
                        vis[i] = true;
                    } else {
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
                if(af->visible) {
                    const PSB::PSBMedia::PSBMotionFrame *next = nullptr;
                    for(const auto &f : frames) {
                        if(f.time > now) { next = &f; break; }
                    }
                    if(next && next->visible && next->time > af->time) {
                        const float t = static_cast<float>(now - af->time) /
                                        static_cast<float>(next->time - af->time);
                        interpOx = af->ox + (next->ox - af->ox) * t;
                        interpOy = af->oy + (next->oy - af->oy) * t;
                        interpCx = af->cx + (next->cx - af->cx) * t;
                        interpCy = af->cy + (next->cy - af->cy) * t;
                        interpOp = af->opacity + (next->opacity - af->opacity) * t;
                    }
                }
                const bool parentOn = (node.parentIndex >= 0) ? vis[node.parentIndex] : true;
                if(!parentOn) { vis[i] = false; continue; } // hidden parent hides subtree
                const float baseX = (node.parentIndex >= 0) ? wx[node.parentIndex] : 0.0f;
                const float baseY = (node.parentIndex >= 0) ? wy[node.parentIndex] : 0.0f;
                const int baseOp = (node.parentIndex >= 0) ? wo[node.parentIndex] : 255;
                const float px = baseX + interpOx + interpCx;
                const float py = baseY + interpOy + interpCy;
                const int lop = std::clamp(static_cast<int>(interpOp), 0, 255);
                const int wop = baseOp * lop / 255;
                wx[i] = px; wy[i] = py; wo[i] = wop;
                vis[i] = (wop > 0);
                if(!vis[i]) continue;
                // Only image lines draw; layout/motion containers only accumulate.
                // 仅图像行绘制；layout/motion 容器只累加不绘制。
                if(af->src.size() <= 4 || af->src.compare(0, 4, "src/") != 0) continue;
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
                // M2 text-layout letters (str_* subtree) are pen-positioned:
                // left-align the glyph bitmap at the advance anchor so
                // variable-width letters tile without overlapping (m2logo
                // "cheeseware"; centering a wide glyph like 'w' overlaps the
                // previous letter). Ordinary image nodes stay center-anchored.
                // M2 文本字母（str_* 子树）按笔位排布：字形位图在 advance 锚点处
                // 左对齐，可变宽度字母才不重叠（m2logo "cheeseware"；居中会让
                // 较宽的 'w' 压到前一个字母）。普通图像节点仍居中锚定。
                int left;
                if(static_cast<size_t>(i) < _nodeInStrSubtree.size() &&
                   _nodeInStrSubtree[static_cast<size_t>(i)]) {
                    left = _coordX + halfCw + static_cast<int>(px);
                } else {
                    left = _coordX + halfCw + static_cast<int>(px) - iw / 2;
                }
                const int top = _coordY + halfCh + static_cast<int>(py) - ih / 2;
                if(logger) logger->info("drawAnimatedTree: '{}' at ({},{}) op={} src='{}'",
                    node.label, left, top, wop, af->src);
                tTJSVariant opArgs[9] = {
                    tTJSVariant(static_cast<tjs_int>(left)),
                    tTJSVariant(static_cast<tjs_int>(top)),
                    tTJSVariant(temp, temp),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(0)),
                    tTJSVariant(static_cast<tjs_int>(iw)),
                    tTJSVariant(static_cast<tjs_int>(ih)),
                    tTJSVariant(static_cast<tjs_int>(2)),  // omAlpha
                    tTJSVariant(static_cast<tjs_int>(wop)),
                };
                tTJSVariant *opArgv[] = { &opArgs[0], &opArgs[1], &opArgs[2],
                                          &opArgs[3], &opArgs[4], &opArgs[5],
                                          &opArgs[6], &opArgs[7], &opArgs[8] };
                try {
                    dest->FuncCall(0, TJS_W("operateRect"), nullptr, nullptr, 9, opArgv, dest);
                    drawn++;
                } catch(const std::exception &e) {
                    if(auto l = _logger()) l->warn("drawAnimatedTree: operateRect exception: {}", e.what());
                } catch(...) {
                    if(auto l = _logger()) l->warn("drawAnimatedTree: operateRect unknown exception");
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
                return;
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
        // M2 animation: per-motion layer frame time-lines (see PSBMedia). Empty →
        // fall back to the static full-frame composite. Loaded after the archive
        // is parsed by cachePSBImages(). Reset on play() so a new motion reloads.
        // M2 动画：每个 motion 图层的帧时间线（见 PSBMedia）。为空则回退静态合成。
        // 在 cachePSBImages() 解析归档后加载；play() 时重置以便新 motion 重载。
        bool _motionTracksLoaded = false;
        std::vector<PSB::PSBMedia::PSBMotionLayerTrack> _motionTracks;
        std::vector<PSB::PSBMedia::PSBMotionNode> _motionNodes;
        // Per-node flag: node sits in an M2 text-layout subtree (str_* ancestor,
        // e.g. m2logo "cheeseware" letters) → its glyph bitmap is left-aligned at
        // the advance anchor instead of centered. Computed in loadMotionTracks().
        // 每节点标记：是否处于 M2 文本排版子树（str_* 祖先，如 m2logo "cheeseware"
        // 字母）→ 字形位图在 advance 锚点左对齐而非居中。loadMotionTracks() 计算。
        std::vector<bool> _nodeInStrSubtree;
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
    };

} // namespace motion
