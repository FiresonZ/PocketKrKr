//---------------------------------------------------------------------------
/*
        TVP2 ( T Visual Presenter 2 )  A script authoring tool
        Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

        See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Utilities for Debugging
//---------------------------------------------------------------------------
#ifndef DebugIntfH
#define DebugIntfH

#include "tjsNative.h"
#include "tjs.h"

//---------------------------------------------------------------------------
// global definitions
//---------------------------------------------------------------------------
extern bool TVPAutoLogToFileOnError;
extern bool TVPAutoClearLogOnError;
extern bool TVPLoggingToFile;

extern void TVPSetOnLog(void (*func)(const ttstr &line));

// 释放脚本注册的全部日志闭包（tTJSVariantClosure）。
// 必须在脚本引擎销毁（TVPUninitScriptEngine）之前调用，否则闭包 finalizer
// 会命中已失效的 TJS 全局态导致退出卡死（runtime-restart）。由
// tTVPApplication::OnExit 在销毁引擎前调用；TVPDestroyLoggingHandlerVector
// at-exit handler 仍保留作兜底（届时引擎已死，向量应为空/无闭包）。
extern void TVPClearLoggingHandlers();

TJS_EXP_FUNC_DEF(void, TVPAddLog, (const ttstr &line));

TJS_EXP_FUNC_DEF(void, TVPAddImportantLog, (const ttstr &line));

extern ttstr TVPGetLastLog(tjs_uint n);

extern iTJSConsoleOutput *TVPGetTJS2ConsoleOutputGateway();

extern iTJSConsoleOutput *TVPGetTJS2DumpOutputGateway();

extern void TVPTJS2StartDump();

extern void TVPTJS2EndDump();

extern void TVPOnError();

extern ttstr TVPGetImportantLog();

extern void TVPSetLogLocation(const ttstr &loc);

extern ttstr TVPNativeLogLocation;

extern void TVPStartLogToFile(bool clear);
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// implement in each platform
//---------------------------------------------------------------------------
// extern void TVPOnErrorHook();
// called from TVPOnError, on system error.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// tTJSNC_Debug : TJS Debug Class
//---------------------------------------------------------------------------
class tTJSNC_Debug : public tTJSNativeClass {
public:
    tTJSNC_Debug();

    static tjs_uint32 ClassID;

protected:
    tTJSNativeInstance *CreateNativeInstance() override;
};

//---------------------------------------------------------------------------
extern tTJSNativeClass *TVPCreateNativeClass_Debug();
//---------------------------------------------------------------------------

#endif
