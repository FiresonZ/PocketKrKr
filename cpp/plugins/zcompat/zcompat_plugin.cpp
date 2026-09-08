//---------------------------------------------------------------------------
// Z (krkrz / KIRIKIRI Z) 插件挂名模块
//---------------------------------------------------------------------------
// 游戏 plugin/ 目录下的 Z 系插件 DLL 在移动端不存在独立二进制（ncb 插件编进
// libengine_api.so），由内部注册表直接命中，先做到「Plugins.link 不再 Failed」。
// 功能实现按 krkrz-compat.md 清单逐个补齐；仅挂名的条目（Regist 为空转）游戏
// 若实际调用对应类/函数会抛「类不存在」，属「先可 link，再实现」的中间态。
//---------------------------------------------------------------------------
#include "ncbind.hpp"

// 挂名空回调：仅让 ncbAutoRegister::LoadModule 命中内部注册表返回成功。
static void ZCompatStub() {}

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

// kztouch.dll —— Z 触摸/触摸控件（来源待确认），挂名。
static ncbCallbackAutoRegister g_z_kztouch(
    TJS_W("kztouch.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// k2compat.dll —— krkr2→Z 兼容层；主要功能是纯 TJS（Krkr2Compat data/k2compat/*.tjs，
// 见 krkrz-compat.md），此处挂名让 dll 侧 link 通过，TJS 侧内置另行处理。
static ncbCallbackAutoRegister g_z_k2compat(
    TJS_W("k2compat.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// kagexopt.dll —— KAG 系统扩展（KAGEX 优化），挂名。
static ncbCallbackAutoRegister g_z_kagexopt(
    TJS_W("kagexopt.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// multiimage.dll —— 多图/多图层纹理（PSD 相关），挂名。
static ncbCallbackAutoRegister g_z_multiimage(
    TJS_W("multiimage.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// squirrel.dll —— 内嵌 Squirrel 脚本语言（部分 Z 游戏系统/存档逻辑）；属 P1
// 真缺插件，挂名后需按 krkrz krkr2/src/plugins/win32/squirrel 实现宿主绑定。
static ncbCallbackAutoRegister g_z_squirrel(
    TJS_W("squirrel.dll"), ncbAutoRegister::PreRegist, &ZCompatStub, nullptr);

// menu.dll —— MenuItem / window.menu(dll)；移动端以 win32dialog 近似，挂名。
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

// motionplayer_nod3d.dll —— 无 D3D 版 motionplayer（千恋万花）；与 motionplayer.dll
// 同源，挂名后如需功能应映射到现有 motionplayer 实现。
static ncbCallbackAutoRegister g_z_motionplayer_nod3d(
    TJS_W("motionplayer_nod3d.dll"), ncbAutoRegister::PreRegist, &ZCompatStub,
    nullptr);

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
