//---------------------------------------------------------------------------
// Z (krkrz / KIRIKIRI Z) 插件兼容模块
//---------------------------------------------------------------------------
// 游戏 plugin/ 目录下的 Z 系插件 DLL 在移动端不存在独立二进制（ncb 插件编进
// libengine_api.so），由内部注册表直接命中。与 Kirikiroid2 一致：TVPLoadPlugin
// 密封所有 DLL 加载，功能内建在引擎核心 + 插件层，link 只查内部注册表。
//
// 各条目状态（详见 krkrz-compat.md）：
//   - 功能已内建（核心类/渲染管线已有，link 即可用）：drawdeviceD3DZ /
//     drawdeviceD3D（主 DrawBuffer 由 core/visual 合成）、kztouch（Window
//     getTouchPoint 等触摸类已内建）、menu（MenuItem 核心类全局注册）。
//   - 真实现（内嵌参考源码）：k2compat（Krkr2Compat 纯 TJS 兼容层内嵌执行）。
//   - 名字映射（已有实现复用）：motionplayer_nod3d → motionplayer.dll（在
//     PluginImpl.cpp 的 TVPLoadPlugin 映射）。
//   - 暂不可实现（源码不可得/桌面概念）：squirrel（VM 源码可得但待移植）、
//     multiimage（闭源）、win32ole/lzfs/PackinOne/extNagano/pkutil/xpzdec/
//     yuzuex（桌面/工具/私有），挂名消除 Failed 噪音，属"先可 link，再实现"
//     的中间态，游戏若实际调用对应类会抛「类不存在」。
//---------------------------------------------------------------------------
#include "ncbind.hpp"

// 挂名空回调：仅让 ncbAutoRegister::LoadModule 命中内部注册表返回成功。
static void ZCompatStub() {}

// k2compat.dll —— krkr2→Z 兼容层真实实现：krkrz 中该 DLL 即内嵌
// Krkr2Compat 的纯 TJS（data/k2compat/*.tjs），link/预加载时执行。
// 见 k2compat_scripts.cpp（TVPInstallK2CompatScripts）。
void TVPInstallK2CompatScripts();
void TVPResetK2CompatInstalledForRestart();
static void ZCompatK2CompatInit() { TVPInstallK2CompatScripts(); }
static void ZCompatK2CompatTerm() { TVPResetK2CompatInstalledForRestart(); }

// drawdeviceD3DZ.dll —— Z 主画面 D3D drawdevice。移动端真形态是 core/visual 渲染
// 管线（Kirikiroid2 RenderManager_ogl 直接合成 Z 主 DrawBuffer），非独立插件；
// 挂名仅为让 Z 游戏启动时的 Plugins.link 不报 Failed。
static ncbCallbackAutoRegister g_z_drawdeviceD3DZ(
    TJS_W("drawdeviceD3DZ.dll"), ncbAutoRegister::PreRegist, &ZCompatStub,
    nullptr);

// drawdeviceD3D.dll —— krkr2 常规 D3D draw device（Z 之前的路径），同上挂名。
static ncbCallbackAutoRegister g_z_drawdeviceD3D(
    TJS_W("drawdeviceD3D.dll"), ncbAutoRegister::PreRegist, &ZCompatStub,
    nullptr);

// kztouch.dll —— Z 触摸/触摸控件；移动端触摸已内建（Window.getTouchPoint 等），
// link 通过即满足 Z 游戏启动链路，挂名。
static ncbCallbackAutoRegister g_z_kztouch(
    TJS_W("kztouch.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// k2compat.dll —— krkr2→Z 兼容层：内嵌 Krkr2Compat 纯 TJS 脚本，注册时执行
// （真实实现，非挂名）。Term 回调供 runtime-restart 重置安装标记。
static ncbCallbackAutoRegister g_z_k2compat(
    TJS_W("k2compat.dll"), ncbAutoRegister::PreRegist, &ZCompatK2CompatInit,
    &ZCompatK2CompatTerm);

// kagexopt.dll —— KAG 系统扩展（KAGEX 优化），挂名。
static ncbCallbackAutoRegister g_z_kagexopt(
    TJS_W("kagexopt.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// multiimage.dll —— 多图/多图层纹理（PSD 相关），源码未开源（krkrz 无源码），挂名。
static ncbCallbackAutoRegister g_z_multiimage(
    TJS_W("multiimage.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// squirrel.dll —— 内嵌 Squirrel 脚本语言（部分 Z 游戏系统/存档逻辑）；krkr2 源码
// 可得（trunk/src/plugins/win32/squirrel + Squirrel VM），待移植，当前挂名。
static ncbCallbackAutoRegister g_z_squirrel(
    TJS_W("squirrel.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// menu.dll —— MenuItem 类/Window.menu；核心已全局注册 tTJSNC_MenuItem
// （ScriptMgnIntf.cpp registerObject("MenuItem")），link 即用，挂名。
static ncbCallbackAutoRegister g_z_menu(
    TJS_W("menu.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// yuzuex.dll —— 千恋万花等 Yuzusoft 作品专用（疑似私有），挂名。
static ncbCallbackAutoRegister g_z_yuzuex(
    TJS_W("yuzuex.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// lzfs.dll —— LZ 文件系统归档支持，挂名。
static ncbCallbackAutoRegister g_z_lzfs(
    TJS_W("lzfs.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// win32ole.dll —— OLE 自动化（桌面概念，移动端无意义），挂名避免启动报错。
static ncbCallbackAutoRegister g_z_win32ole(
    TJS_W("win32ole.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// motionplayer_nod3d.dll —— 无 D3D 版 motionplayer（千恋万花）；已在
// PluginImpl.cpp TVPLoadPlugin 映射到 motionplayer.dll 复用现有实现，无需挂名。

// PackinOne.dll —— 打包/资源插件，疑似闭源（krkrz-compat.md 判定 C 级跳过）；
// 挂名消除 Failed 噪音，功能不实现。
static ncbCallbackAutoRegister g_z_packinone(
    TJS_W("packinone.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// extNagano.dll —— 独立冷门扩展（千恋万花实载 Failed；krkrz-compat.md C 级跳过），挂名。
static ncbCallbackAutoRegister g_z_extnagano(
    TJS_W("extnagano.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// pkutil.dll —— 打包工具（Kemomusu 等实载 Failed），挂名。
static ncbCallbackAutoRegister g_z_pkutil(
    TJS_W("pkutil.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// xpzdec.dll —— xpz 加密归档解码（.tpm 在 TVPLoadInternalPlugin 中归一为 .dll），挂名。
static ncbCallbackAutoRegister g_z_xpzdec(
    TJS_W("xpzdec.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);
