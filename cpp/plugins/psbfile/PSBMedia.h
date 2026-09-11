//
// Created by LiDon on 2025/9/11.
//
#pragma once

#include <list>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

#include "PSBValue.h"
#include "StorageIntf.h"
#include "resources/ImageMetadata.h"

namespace PSB {
    struct PSBMediaCacheStats {
        size_t entryCount = 0;
        size_t entryLimit = 0;
        size_t bytesInUse = 0;
        size_t byteLimit = 0;
        uint64_t hitCount = 0;
        uint64_t missCount = 0;
    };

    class PSBMedia;
    bool GetPSBMediaCacheStats(PSBMediaCacheStats &outStats);
    void SetPSBMediaCacheBudget(size_t maxEntries, size_t maxBytes);
    PSBMedia *GetGlobalPSBMedia();

    class PSBMedia : public iTVPStorageMedia {
    public:
        PSBMedia();

        ~PSBMedia() override = default;

        void AddRef() override { _ref++; }

        void Release() override {
            if(_ref == 1)
                delete this;
            else
                _ref--;
        }

        void GetName(ttstr &name) override { name = TJS_W("psb"); }

        void NormalizeDomainName(ttstr &name) override;

        void NormalizePathName(ttstr &name) override;

        bool CheckExistentStorage(const ttstr &name) override;

        tTJSBinaryStream *Open(const ttstr &name, tjs_uint32 flags) override;

        void GetListAt(const ttstr &name, iTVPStorageLister *lister) override;

        void GetLocallyAccessibleName(ttstr &name) override;

        void add(const std::string &name,
                 const std::shared_ptr<PSBResource> &resource,
                 const ImageMetadata *imageMeta = nullptr);
        void removeByPrefix(const std::string &prefix);
        void clear();
        void setCacheBudget(size_t maxEntries, size_t maxBytes);
        PSBMediaCacheStats getCacheStats() const;

    public:
        struct CachedImageInfo {
            std::string debugKey;
            int width = 0;
            int height = 0;
            int left = 0;
            int top = 0;
            // Icon hotspot (anchor/rotation pivot of the source bitmap; from the
            // icon's originX/originY). Draw anchor: org = pos - M*(originX+ox, ...).
            // 图标热点（源位图锚点/旋转枢轴；来自 icon 的 originX/originY）。
            // 绘制锚点：org = pos - M*(originX+ox, ...)。
            float originX = 0;
            float originY = 0;
            int opacity = 255;
            bool visible = true;
            int layerType = 0;
            std::string type;
            std::string paletteType;
            PSBSpec spec = PSBSpec::Other;
            PSBCompressType compress = PSBCompressType::ByName;
            std::vector<uint8_t> palette;
        };
        struct CacheEntry {
            std::shared_ptr<PSBResource> resource;
            std::shared_ptr<std::vector<uint8_t>> convertedImage;
            CachedImageInfo imageInfo;
            bool hasImageInfo = false;
            size_t sizeBytes = 0;
            std::list<std::string>::iterator lruIt;
        };

        struct ImageInfoEntry {
            std::string key;
            CachedImageInfo info;
        };
        std::vector<ImageInfoEntry> getImagesByPrefix(const std::string &prefix) const;
        bool getImageInfo(const std::string &key, CachedImageInfo &outInfo) const;

        struct LayerPosition {
            std::string sceneName;
            std::string layerName;
            std::string srcPath;
            float left = 0;
            float top = 0;
            int width = 0;
            int height = 0;
            int opacity = 255;
            bool visible = true;
        };
        void addLayerPositions(const std::string &archiveKey,
                               std::vector<LayerPosition> positions);
        std::vector<LayerPosition> getLayerPositions(const std::string &prefix) const;

        struct ButtonBoundInfo {
            std::string sceneName;
            std::string buttonName;
            std::string imageKey;
            float left = 0;
            float top = 0;
            int width = 0;
            int height = 0;
        };
        void addButtonBounds(const std::string &archiveKey,
                             std::vector<ButtonBoundInfo> bounds);
        std::vector<ButtonBoundInfo> getButtonBounds(const std::string &prefix) const;

        // ---- M2 motion timeline (frame animation) ----
        // One keyframe of a motion layer: becomes active at `time` ms. M2 PSB
        // layers carry a frameList whose frames have `time` + `content{src,ox,oy,
        // coord,op}`; an M2 player advances a clock and shows the latest frame
        // with time <= clock. This is what makes kirkiroid2's title/logo animate
        // instead of our current static full-frame composite.
        // M2 motion 时间轴：motion 图层上的一个关键帧，于 `time` 毫秒时刻生效。M2 的 PSB
        // 图层带 frameList，帧含 time + content{src,ox,oy,coord,op}；播放器推进时钟并显示
        // time <= 时钟的最新一帧。这是 K2 里标题/logo 能动的数据基础。
        struct PSBMotionFrame {
            int time = 0;            // ms, when this frame takes effect / 生效时刻(ms)
            // PSB frame "type": 0=invisible, 2=static, 3=interpolate. The
            // reference (libkrkr2 sub_6926B4 parseFrame) treats type==0 as an
            // INVISIBLE frame regardless of content — a layer's hidden initial
            // state (e.g. yuzulogo's white/logo layers at t=0 carry a src but
            // type==0, so the complete static logo must NOT appear before the
            // intro animation starts). Previously we only checked "has content",
            // which drew that static logo too early.
            // PSB 帧 "type"：0=不可见, 2=静态, 3=插值。参考实现（libkrkr2
            // sub_6926B4 parseFrame）把 type==0 一律视为**不可见帧**——图层的
            // 隐藏初始态（如 yuzulogo 的 white/logo 层 t=0 帧带 src 但 type==0，
            // 完整静止 logo 不该在片头动画开始前出现）。此前只按"有无 content"
            // 判断可见性，导致该静态 logo 过早出现。
            int type = 2;            // PSB frame type / 帧类型
            std::string src;         // content.src ("src/..." image path)
            float ox = 0, oy = 0;    // content origin offset / 原点偏移
            float cx = 0, cy = 0;    // content coord / 坐标
            // M2 per-frame scale from content keys "zx"/"zy" (libkrkr2 sub_692AB0
            // mask 0x60; also coord[2] 'z'). Logo backdrops magnify a tiny source
            // image (yuzulogo's 64x64 white_box → fullscreen) via zx/zy — without
            // this the white bg can never cover the canvas. Default 1 = no scale.
            // M2 帧内缩放来自 content 的 "zx"/"zy"（libkrkr2 sub_692AB0 mask 0x60；
            // 也有 coord[2] 'z'）。logo 背景用 zx/zy 把小图放大（yuzulogo 的 64×64
            // white_box → 全屏）；不读这个字段，白底永远铺不满画布。默认 1=不缩放。
            float scaleX = 1, scaleY = 1; // content scale "zx"/"zy" / 缩放
            // M2 per-frame rotation (content "angle", degrees). The yuzusoft logo's
            // leaf "jitter"/wobble is an angle animation; without it the leaf neither
            // swings nor faces the correct way. Units: degrees (reference
            // applyLocalTransform uses angle*2π/360).
            // M2 帧内旋转（content "angle"，单位度）。yuzusoft logo 的叶子"抖动/摆动"
            // 就是 angle 动画；缺它则叶子既不摆动、朝向也不对。单位：度（参考
            // applyLocalTransform 用 angle*2π/360）。
            float angle = 0;             // content "angle" degrees / 旋转角度（度）
            float opacity = 255;     // 0..255 (m2 `op`) / 透明度
            // M2 blend mode (content "bm") and clipping rect (round 2). bm drives
            // the operate blend op; clip limits the node to a sub-rect (probed
            // this round — applied once data confirmed present).
            // M2 混合模式(content "bm")与裁切矩形(第二轮)。bm 决定 operate 的混合
            // 算子；clip 把节点限制在子矩形内（本轮先探针，确认存在后再应用）。
            int blendMode = 0;         // content "bm" / 混合模式
            bool hasClip = false;      // content "clip" present / 是否有裁切
            int clipL = 0, clipT = 0, clipR = 0, clipB = 0; // clip rect / 裁切矩形
            // M2 flip flags from content "fx"/"fy" (libkrkr2 sub_692F6C, mask 0x4/0x8).
            // A mirrored sprite (e.g. yuzulogo's leaf) uses these — without applying
            // the flip the piece renders reversed vs K2.
            // M2 翻转标志来自 content "fx"/"fy"（libkrkr2 sub_692F6C，mask 0x4/0x8）。
            // 叶片等镜像精灵用它们；不应用则叶片朝向与原版相反。
            bool flipX = false, flipY = false; // content flipX "fx" / flipY "fy" / 翻转
            bool visible = true;     // !(type==0) && has content / 本帧是否可见
        };
        struct PSBMotionLayerTrack {
            std::string label;                        // layer label / 图层名
            int width = 0;                            // PSB layer display width / 图层显示宽
            int height = 0;                           // PSB layer display height / 图层显示高
            std::vector<PSBMotionFrame> frames;       // sorted by time / 按时间排序
        };
        // A motion-layer NODE in the motion's layer tree. Unlike the flat track
        // list, nodes keep the parent→child relationship (PSB "children" key),
        // which is what lets a `layout` container's position/opacity propagate
        // down to its child layers (generic M2 semantics — see libkrkr2.so's
        // Player_buildNodeTree). `frames` holds this node's own key-frame timeline.
        // M2 motion 的图层**节点**。与扁平轨道不同，节点保留 parent→child 父子关系
        //（PSB 的 "children" 键），这正是 `layout` 容器的位移/透明度能传给子层的
        // 通用基础（参考 libkrkr2.so 的 Player_buildNodeTree）。`frames` 是该节点
        // 自身的帧时间线。
        struct PSBMotionNode {
            std::string label;                  // node label / 节点名
            int parentIndex = -1;               // parent node index (-1 = root-level)
            int type = 0;                       // PSB layer "type" / 图层类型
            int width = 0;                      // PSB layer display width / 图层显示宽
            int height = 0;                     // PSB layer display height / 图层显示高
            // Per-node transform inheritance mask (PSB "inheritMask"). Bits gate which
            // of flip/angle/scale/slant a node inherits from its parent (default 0x1FC =
            // inherit all). libkrkr2 reads it at node build (sub_6B3C78):
            //   0x004 flipX, 0x008 flipY, 0x010 angle, 0x020 scaleX, 0x040 scaleY,
            //   0x080 slantX, 0x100 slantY
            // 节点的变换继承掩码（PSB "inheritMask"）。位门控该节点从父节点继承
            // flip/angle/scale/slant 的哪些（默认 0x1FC=全部继承）。libkrkr2 在节点构建时
            // 读取（sub_6B3C78）：0x004 flipX、0x008 flipY、0x010 angle、0x020 scaleX、
            // 0x040 scaleY、0x080 slantX、0x100 slantY。
            int inheritMask = 0x1FC;
            // Per-node local-matrix operator order (PSB "transformOrder", default
            // [0,1,2,3] = flip, angle, scale, slant). libkrkr2 sub_699940 iterates it to
            // LEFT-multiply each transform onto the local 2x2 matrix.
            // 节点局部矩阵的算子顺序（PSB "transformOrder"，默认 [0,1,2,3] =
            // flip, angle, scale, slant）。libkrkr2 sub_699940 依序左乘到局部 2×2 矩阵。
            int transformOrder[4] = {0, 1, 2, 3};
            std::vector<PSBMotionFrame> frames; // own timeline, sorted by time
        };
        void addMotionNodes(const std::string &archiveKey,
                            const std::string &sceneName,
                            const std::string &motionName,
                            std::vector<PSBMotionNode> nodes);
        std::vector<PSBMotionNode> getMotionNodes(const std::string &archiveKey,
                                                  const std::string &sceneName,
                                                  const std::string &motionName) const;
        // M2 motion-level loop metadata. loopTime > 0 means the timeline loops
        // (e.g. the yuzulogo/m2logo intros keep playing until the script advances),
        // mirroring Player_initNonEmoteMotion's read of PSB "loopTime".
        // M2 motion 级循环元数据。loopTime > 0 表示时间线循环（如 yuzulogo/m2logo
        // 片头会一直播到脚本推进），对应 Player_initNonEmoteMotion 读取 PSB "loopTime"。
        void setMotionLoopTime(const std::string &archiveKey,
                               const std::string &sceneName,
                               const std::string &motionName,
                               tjs_int loopTime);
        tjs_int getMotionLoopTime(const std::string &archiveKey,
                                  const std::string &sceneName,
                                  const std::string &motionName) const;
        void addMotionTracks(const std::string &archiveKey,
                             const std::string &sceneName,
                             const std::string &motionName,
                             std::vector<PSBMotionLayerTrack> tracks);
        std::vector<PSBMotionLayerTrack> getMotionTracks(const std::string &archiveKey,
                                                         const std::string &sceneName,
                                                         const std::string &motionName) const;
        std::vector<std::string> getMotionNames(const std::string &archiveKey,
                                                const std::string &sceneName) const;

        // Force the archive identified by `archiveKey` (e.g. "yuzulogo.mtn") to be
        // parsed and registered NOW, so layer positions and motion tracks become
        // queryable before a consumer asks for them. Idempotent: after the first
        // successful load later calls are no-ops. Needed because PSB archives load
        // lazily on first resource access, and motion data must be ready before
        // Player/captureCanvas reads it on the very first frame.
        // 立即解析并注册 `archiveKey`（如 "yuzulogo.mtn"）对应的归档，让图层坐标与
        // motion 时间线在消费者查询前就绪。幂等：首次成功后后续调用为空操作。用于
        // 绕开"归档首次访问资源才懒加载"的时序，确保首帧时 motion 数据已可用。
        bool ensureArchiveLoaded(const std::string &archiveKey);

    private:
        using ResourceMap = std::unordered_map<std::string, CacheEntry>;

        std::string canonicalizeKey(const std::string &key) const;
        ResourceMap::iterator findBySuffixLocked(const std::string &key);
        bool tryLazyLoadArchive(const std::string &key);
        void touchLocked(CacheEntry &entry);
        void adaptBudgetByMemoryPressureLocked();
        void evictIfNeededLocked();

        int _ref = 0;
        mutable std::mutex _mutex;
        ResourceMap _resources;
        std::list<std::string> _lru;
        size_t _bytesInUse = 0;
        size_t _configuredMaxEntryCount = 2048;
        size_t _configuredMaxByteSize = 192ULL * 1024ULL * 1024ULL;
        size_t _maxEntryCount = 2048;
        size_t _maxByteSize = 192ULL * 1024ULL * 1024ULL;
        uint64_t _hitCount = 0;
        uint64_t _missCount = 0;
        std::unordered_set<std::string> _loadedArchives;
        std::unordered_map<std::string, std::vector<LayerPosition>> _layerPositions;
        std::unordered_map<std::string, std::vector<ButtonBoundInfo>> _buttonBoundsMap;
        std::unordered_map<std::string, std::vector<PSBMotionLayerTrack>> _motionTracks;
        std::unordered_map<std::string, std::vector<PSBMotionNode>> _motionNodes;
        std::unordered_map<std::string, tjs_int> _motionLoopTimes;
    };
} // namespace PSB
