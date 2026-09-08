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
    // runtime-restart 语义：tTVPAtExit 文件级 static 对象只在进程启动注册一次，
    // 因此 TVPAtExitInfos 是一份**持久**的框架级 teardown 清单，须在每次
    // engine_destroy（每次引擎生命周期结束）都重放一遍，而【不能 delete】——
    // 否则二次及以后 engine_destroy 时静态 handler 注册永久丢失（“注册失效”），
    // 第 2+ 个游戏的定时器/连续事件/tick/事件队列等框架单例将永不被 teardown。
    // 这些 handler 均为“置空静态单例指针、由下次 Init 重建”的可重入 teardown，
    // 可安全重放；TVPResetRuntimeForRestart 会把 TVPAtExitShutdown 复位为 false。
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

    // NOTE: 不再 delete TVPAtExitInfos / 置空。列表持久保留，供下次
    // engine_destroy 重放；TVPAddAtExitHandler 仍受 TVPAtExitShutdown 守卫。
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// TVPResetRuntimeForRestart : Reset state to allow re-initialization
//---------------------------------------------------------------------------
void TVPResetRuntimeForRestart() {
    TVPSystemUninitCalled = false;
    TVPAtExitShutdown = false;
    // 保留 TVPAtExitInfos 列表不置空：静态框架 teardown handler 须在每次
    // engine_destroy 重放（见 TVPCauseAtExit 注释）。若这里置空，首次退出后
    // 静态注册永久丢失，第 2+ 个游戏退出将不再清理框架单例（“注册失效”）。
    TVPProjectDir.Clear();
    TVPDataPath.Clear();
}
//---------------------------------------------------------------------------
