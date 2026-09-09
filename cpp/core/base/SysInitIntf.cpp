//---------------------------------------------------------------------------
/*
        TVP2 ( T Visual Presenter 2 )  A script authoring tool
        Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

        See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// System Initialization and Uninitialization
//---------------------------------------------------------------------------
#include "tjsCommHead.h"

#include <vector>
#include <algorithm>
#include <functional>

#include <dlfcn.h> // dladdr：把 at-exit handler 函数指针解析为符号名，便于真机定位

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "tjsUtils.h"
#include "SysInitIntf.h"
#include "ScriptMgnIntf.h"
#include "KAGParser.h"
#include "tvpgl.h"
#include <spdlog/spdlog.h>

//---------------------------------------------------------------------------
// global data
//---------------------------------------------------------------------------
ttstr TVPProjectDir; // project directory (in unified storage name)
ttstr TVPDataPath; // data directory (in unified storage name)
//---------------------------------------------------------------------------

extern void TVPGL_C_Init();

//---------------------------------------------------------------------------
// TVPSystemInit : Entire System Initialization
//---------------------------------------------------------------------------
void TVPSystemInit() {
#ifdef _WIN32
#ifdef USING_PROTECT
    while(!TVPProtectInit()) {
        TVPUpdateLicense();
    }
#endif
#endif

    TVPBeforeSystemInit();

    TVPInitScriptEngine();

    TVPInitTVPGL();
    //	TVPGL_C_Init();

    TVPAfterSystemInit();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// TVPSystemUninit : System shutdown, cleanup, etc...
//---------------------------------------------------------------------------
static void TVPCauseAtExit();

bool TVPSystemUninitCalled = false;

void TVPSystemUninit() {
    if(TVPSystemUninitCalled)
        return;
    TVPSystemUninitCalled = true;

    TVPBeforeSystemUninit();

    TVPUninitTVPGL();

    try {
        TVPUninitScriptEngine();
    } catch(...) {
        // ignore errors
    }

    TVPAfterSystemUninit();

    TVPCauseAtExit();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// TVPAddAtExitHandler related
//---------------------------------------------------------------------------
struct tTVPAtExitInfo {
    tTVPAtExitInfo(tjs_int pri, void (*handler)()) {
        Priority = pri, Handler = handler;
    }

    tjs_int Priority;

    void (*Handler)();

    bool operator<(const tTVPAtExitInfo &r) const {
        return this->Priority < r.Priority;
    }

    bool operator>(const tTVPAtExitInfo &r) const {
        return this->Priority > r.Priority;
    }

    bool operator==(const tTVPAtExitInfo &r) const {
        return this->Priority == r.Priority;
    }
};

static std::vector<tTVPAtExitInfo> *TVPAtExitInfos = nullptr;
static bool TVPAtExitShutdown = false;

//---------------------------------------------------------------------------
void TVPAddAtExitHandler(tjs_int pri, void (*handler)()) {
    if(TVPAtExitShutdown)
        return;

    if(!TVPAtExitInfos)
        TVPAtExitInfos = new std::vector<tTVPAtExitInfo>();
    TVPAtExitInfos->emplace_back(pri, handler);
}

//---------------------------------------------------------------------------
static void TVPCauseAtExit() {
    // 修正 runtime-restart：tTVPAtExit 文件级 static 对象只在进程启动注册一次，
    // 首次 engine_destroy 的 TVPCauseAtExit 已把 TVPAtExitInfos delete 置空；二次
    // engine_destroy（TVPResetRuntimeForRestart 复位后会再次进入）时为空，不能解引用。
    if(TVPAtExitShutdown || !TVPAtExitInfos)
        return;
    TVPAtExitShutdown = true;

    std::sort(TVPAtExitInfos->begin(),
              TVPAtExitInfos->end()); // descending sort

    // 逐 handler 打点：真机退出卡死时据最后一条日志定位卡在哪个 at-exit handler。
    tjs_uint idx = 0;
    for(auto i = TVPAtExitInfos->begin(); i != TVPAtExitInfos->end();
        ++i, ++idx) {
        // dladdr 把 handler 函数指针解析为符号名（release/strip 后可能为 "?"）
        const char *sym = "?";
        Dl_info inf;
        if(dladdr(reinterpret_cast<void *>(i->Handler), &inf) &&
           inf.dli_sname && inf.dli_sname[0])
            sym = inf.dli_sname;
        spdlog::info("TVPCauseAtExit: handler[{}] pri={} sym={} begin", idx,
                     i->Priority, sym);
        spdlog::default_logger()->flush();
        i->Handler();
        spdlog::info("TVPCauseAtExit: handler[{}] pri={} sym={} end", idx,
                     i->Priority, sym);
        spdlog::default_logger()->flush();
    }

    delete TVPAtExitInfos;
    TVPAtExitInfos = nullptr;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// TVPResetRuntimeForRestart : Reset state to allow re-initialization
//---------------------------------------------------------------------------
void TVPResetRuntimeForRestart() {
    TVPSystemUninitCalled = false;
    TVPAtExitShutdown = false;
    // TVPAtExitInfos was deleted by TVPCauseAtExit(); leave it null so
    // TVPAddAtExitHandler will re-create it on next startup.
    TVPAtExitInfos = nullptr;
    TVPProjectDir.Clear();
    TVPDataPath.Clear();
    // TVPScenarioCache 以场景短名（如 "first.ks"）为 key 的进程级缓存，restart 不随 VM 销毁；
    // 换游戏后下一个游戏 kag.loadScenario 会命中前一个游戏的内容（跨游戏同名场景碰撞），
    // 此处清空。同游戏复开重读缓存为空后正常重建，仅损失一次解压。
    TVPClearScnearioCache();

    // tTVPAtExit 一次性、首次已消费 → 重启不再执行 ShutdownWaveSoundBuffers，上一游戏的
    // BGM/音效由 TVPWaveSoundBufferThread 继续混音输出、叠进下一游戏。这里显式停掉。
    // （实现见 cpp/core/sound/win32/WaveImpl.cpp TVPStopAllWaveSoundsForRestart）
    extern void TVPStopAllWaveSoundsForRestart();
    TVPStopAllWaveSoundsForRestart();
}
//---------------------------------------------------------------------------
