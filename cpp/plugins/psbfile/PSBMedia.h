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
            std::string src;         // content.src ("src/..." image path)
            float ox = 0, oy = 0;    // content origin offset / 原点偏移
            float cx = 0, cy = 0;    // content coord / 坐标
            float opacity = 255;     // 0..255 (m2 `op`) / 透明度
            bool visible = true;     // frame has content / 本帧是否有内容
        };
        struct PSBMotionLayerTrack {
            std::string label;                        // layer label / 图层名
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
            std::vector<PSBMotionFrame> frames; // own timeline, sorted by time
        };
        void addMotionNodes(const std::string &archiveKey,
                            const std::string &sceneName,
                            const std::string &motionName,
                            std::vector<PSBMotionNode> nodes);
        std::vector<PSBMotionNode> getMotionNodes(const std::string &archiveKey,
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
    };
} // namespace PSB
