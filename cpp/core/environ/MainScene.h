/**
 * @file MainScene.h
 * @brief Stub MainScene — replaces the original Scene-based MainScene.
 *
 * This is a lightweight engine loop driver that provides the same external
 * interface as the original TVPMainScene but without any external framework dependency.
 * It drives Application::Run() each frame and manages game startup.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Use plain unsigned int instead of tjs_uint to avoid TJS2 header dependency
using tjs_uint = unsigned int;

class TVPMainScene {
    TVPMainScene();

public:
    ~TVPMainScene();

    static TVPMainScene *GetInstance();
    static TVPMainScene *CreateInstance();

    /**
     * Start the engine from the given game path.
     * Replaces the original version that also set up UI nodes.
     */
    bool startupFrom(const std::string &path);

    /**
     * Enable per-frame updates (called by engine_api after game open).
     */
    void scheduleUpdate();

    /**
     * Main loop tick — drives Application::Run() + texture recycling.
     * Called by engine_tick() or the host frame callback.
     */
    void update(float delta);

    /**
     * Whether updates are currently scheduled.
     */
    bool isUpdateScheduled() const { return _updateScheduled; }

private:
    void doStartup(const std::string &path);

    bool _updateScheduled = false;
    bool _started = false;
};

// Global functions previously defined in MainScene.cpp
void TVPSetPostUpdateEvent(void (*f)());
int TVPDrawSceneOnce(int interval);
// runtime-restart：复位 TVPDrawSceneOnce 内部的进程级静态节拍，避免二次打开继承
// 上一游戏 lastTick 算出巨大 remain 而跳过合成。
void TVPResetDrawSceneOnceTimingForRestart();
bool TVPGetKeyMouseAsyncState(unsigned int keycode, bool getcurrent);
bool TVPGetJoyPadAsyncState(unsigned int keycode, bool getcurrent);