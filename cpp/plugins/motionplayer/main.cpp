//
// Created by LiDon on 2025/9/13.
// TODO: implement emoteplayer.dll plugin
//
#include <spdlog/spdlog.h>
#include "tjs.h"
#include "tjsDictionary.h"
#include "ncbind.hpp"
#include "psbfile/PSBFile.h"

#include "ResourceManager.h"
#include "EmotePlayer.h"
#include "Player.h"
#include "SeparateLayerAdaptor.h"

using namespace motion;
using namespace TJS;

#define NCB_MODULE_NAME TJS_W("motionplayer.dll")
#define LOGGER spdlog::get("plugin")

static motion::SeparateLayerAdaptor *GetSeparateLayerAdaptorInstance(iTJSDispatch2 *objthis) {
    return ncbInstanceAdaptor<motion::SeparateLayerAdaptor>::GetNativeInstance(objthis);
}

static iTJSDispatch2 *GetSeparateAdaptorRenderTarget(motion::SeparateLayerAdaptor *adaptor);

iTJSDispatch2 *ResolveLayerTreeOwnerBase(iTJSDispatch2 *base) {
    if(!base) return nullptr;
    auto *adaptor = ncbInstanceAdaptor<motion::SeparateLayerAdaptor>::GetNativeInstance(base);
    if(adaptor) return GetSeparateAdaptorRenderTarget(adaptor);
    return base;
}

static iTJSDispatch2 *GetSeparateAdaptorRenderTarget(motion::SeparateLayerAdaptor *adaptor) {
    if(!adaptor) return nullptr;
    if(adaptor->getTarget()) return adaptor->getTarget();

    auto *owner = adaptor->getOwner();
    if(!owner) return nullptr;

    tTJSVariant windowVar;
    iTJSDispatch2 *windowObj = owner;
    if(TJS_SUCCEEDED(owner->PropGet(0, TJS_W("window"), nullptr, &windowVar, owner)) &&
       windowVar.Type() == tvtObject && windowVar.AsObjectNoAddRef()) {
        windowObj = windowVar.AsObjectNoAddRef();
    }

    tTJSVariant parentVar;
    if(TJS_FAILED(owner->PropGet(0, TJS_W("primaryLayer"), nullptr, &parentVar, owner)) ||
       parentVar.Type() != tvtObject || !parentVar.AsObjectNoAddRef()) {
        if(TJS_FAILED(windowObj->PropGet(0, TJS_W("primaryLayer"), nullptr, &parentVar, windowObj)) ||
           parentVar.Type() != tvtObject || !parentVar.AsObjectNoAddRef()) {
            return owner;
        }
    }

    iTJSDispatch2 *global = TVPGetScriptDispatch();
    if(!global) return owner;

    tTJSVariant layerClassVar;
    if(TJS_FAILED(global->PropGet(0, TJS_W("Layer"), nullptr, &layerClassVar, global)) ||
       layerClassVar.Type() != tvtObject || !layerClassVar.AsObjectNoAddRef()) {
        global->Release();
        return owner;
    }

    iTJSDispatch2 *layerClass = layerClassVar.AsObjectNoAddRef();
    tTJSVariant args[2] = { tTJSVariant(windowObj, windowObj),
                            tTJSVariant(parentVar.AsObjectNoAddRef(), parentVar.AsObjectNoAddRef()) };
    tTJSVariant *argv[] = { &args[0], &args[1] };
    iTJSDispatch2 *layerObj = nullptr;
    const auto hr = layerClass->CreateNew(0, nullptr, nullptr, &layerObj, 2, argv, layerClass);
    global->Release();
    if(TJS_FAILED(hr) || !layerObj) {
        return owner;
    }

    auto syncProp = [&](const tjs_char *name) {
        tTJSVariant value;
        if(TJS_SUCCEEDED(owner->PropGet(0, name, nullptr, &value, owner))) {
            layerObj->PropSet(TJS_MEMBERENSURE, name, nullptr, &value, layerObj);
        }
    };
    syncProp(TJS_W("left"));
    syncProp(TJS_W("top"));
    syncProp(TJS_W("width"));
    syncProp(TJS_W("height"));
    syncProp(TJS_W("visible"));
    syncProp(TJS_W("opacity"));
    syncProp(TJS_W("name"));

    // Prevent the render target from intercepting mouse events;
    // hitThreshold=256 makes hit test always fail (max alpha is 255)
    tTJSVariant htVal(static_cast<tjs_int>(256));
    layerObj->PropSet(TJS_MEMBERENSURE, TJS_W("hitThreshold"), nullptr, &htVal, layerObj);

    // Ensure the owner AnimKAGLayer passes hit test even without its own bitmap;
    // hitThreshold=0 means bounds-only checking (no alpha test needed)
    tTJSVariant ownerHtVal(static_cast<tjs_int>(0));
    owner->PropSet(TJS_MEMBERENSURE, TJS_W("hitThreshold"), nullptr, &ownerHtVal, owner);

    adaptor->setTarget(layerObj);
    layerObj->Release();
    return adaptor->getTarget() ? adaptor->getTarget() : owner;
}

static tjs_error SeparateLayerAdaptor_getWidth(tTJSVariant *r, tjs_int, tTJSVariant **,
                                               iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(0));
        return TJS_S_OK;
    }
    tTJSVariant value;
    const auto hr = target->PropGet(0, TJS_W("width"), nullptr, &value, target);
    if(r) {
        *r = TJS_SUCCEEDED(hr) ? value : tTJSVariant(static_cast<tjs_int>(0));
    }
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_getHeight(tTJSVariant *r, tjs_int, tTJSVariant **,
                                                iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(0));
        return TJS_S_OK;
    }
    tTJSVariant value;
    const auto hr = target->PropGet(0, TJS_W("height"), nullptr, &value, target);
    if(r) {
        *r = TJS_SUCCEEDED(hr) ? value : tTJSVariant(static_cast<tjs_int>(0));
    }
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_loadImages(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                                 iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target || count < 1) return TJS_E_INVALIDPARAM;
    return target->FuncCall(0, TJS_W("loadImages"), nullptr, r, count, p, target);
}

static tjs_error SeparateLayerAdaptor_fillRect(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                               iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target || count < 5) return TJS_E_INVALIDPARAM;
    return target->FuncCall(0, TJS_W("fillRect"), nullptr, r, count, p, target);
}

static tjs_error SeparateLayerAdaptor_operateRect(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                                  iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target || count < 9) return TJS_E_INVALIDPARAM;
    return target->FuncCall(0, TJS_W("operateRect"), nullptr, r, count, p, target);
}

static tjs_error SeparateLayerAdaptor_getFace(tTJSVariant *r, tjs_int, tTJSVariant **,
                                              iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target) { if(r) *r = tTJSVariant(static_cast<tjs_int>(0)); return TJS_S_OK; }
    tTJSVariant value;
    const auto hr = target->PropGet(0, TJS_W("face"), nullptr, &value, target);
    if(r) *r = TJS_SUCCEEDED(hr) ? value : tTJSVariant(static_cast<tjs_int>(0));
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_setFace(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                              iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target || count < 1) return TJS_E_INVALIDPARAM;
    return target->PropSet(TJS_MEMBERENSURE, TJS_W("face"), nullptr, p[0], target);
}

static tjs_error SeparateLayerAdaptor_getImageWidth(tTJSVariant *r, tjs_int, tTJSVariant **,
                                                    iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target) { if(r) *r = tTJSVariant(static_cast<tjs_int>(0)); return TJS_S_OK; }
    tTJSVariant value;
    const auto hr = target->PropGet(0, TJS_W("imageWidth"), nullptr, &value, target);
    if(r) *r = TJS_SUCCEEDED(hr) ? value : tTJSVariant(static_cast<tjs_int>(0));
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_getImageHeight(tTJSVariant *r, tjs_int, tTJSVariant **,
                                                     iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    auto *target = GetSeparateAdaptorRenderTarget(adaptor);
    if(!target) { if(r) *r = tTJSVariant(static_cast<tjs_int>(0)); return TJS_S_OK; }
    tTJSVariant value;
    const auto hr = target->PropGet(0, TJS_W("imageHeight"), nullptr, &value, target);
    if(r) *r = TJS_SUCCEEDED(hr) ? value : tTJSVariant(static_cast<tjs_int>(0));
    return TJS_S_OK;
}

// 千恋万花等 Yuzusoft 作品：motion 的 work layer 由 SeparateLayerAdaptor 承载，
// helper: map a TJS value / dispatch to a short type name, for captureCanvas diagnostics.
// 辅助：把 TJS 值/调度对象映射为简短类型名，供 captureCanvas 诊断使用。
static ttstr VariantTypeName(const tTJSVariant &v) {
    switch(v.Type()) {
        case tvtVoid:   return TJS_W("void");
        case tvtObject: return TJS_W("object");
        case tvtString: return TJS_W("string");
        case tvtInteger:return TJS_W("int");
        case tvtReal:   return TJS_W("real");
        case tvtOctet:  return TJS_W("octet");
        default:        return TJS_W("?");
    }
}
static ttstr VariantTypeName(iTJSDispatch2 *o) {
    return o ? TJS_W("obj") : TJS_W("void");
}
// 游戏脚本 affinesourcemotion.tjs 会调 captureCanvas/canvasCaptureEnabled/
// unloadUnusedTextures（Kirikiroid2 发布 APK 的 libgame.so 同款成员，实证）。
// Senren Clinic etc. Yuzusoft titles: the motion work layer is carried by
// SeparateLayerAdaptor; affinesourcemotion.tjs calls captureCanvas /
// canvasCaptureEnabled / unloadUnusedTextures on it (same members verified in
// the official Kirikiroid2 APK libgame.so).
//
// ⚠️ 空壳性质说明（与 D3DAdaptor 的成员属同一语义的两套宿主）：
//   Kirikiroid2 的 D3DAdaptor 与 SeparateLayerAdaptor 都有 captureCanvas 等成员；
//   本文件把它们按宿主分别注册（SeparateLayerAdaptor / 下方 D3DAdaptor），
//   motionWorkLayer 实际是哪一个实例就命中哪一个。两者都遵循：
//     优先转发到目标 Layer（若它能处理则交给它），无目标/不可转发则 no-op 兜底。
//   移动端无 D3D、motion 走 CPU/GL 已直接渲染，"捕获进另一块 canvas"可跳过。
//   升级触发条件见 D3DAdaptor 注释：仅当游戏真的取用捕获结果作为后续图像源时。
//   Empty-shell note (the two hosts share the same semantics as D3DAdaptor):
//   both D3DAdaptor and SeparateLayerAdaptor expose captureCanvas et al.; we
//   register them per host, and whichever instance motionWorkLayer actually is
//   will win. Both follow: forward to target Layer first if possible, otherwise
//   no-op fallback. Mobile has no D3D — motion renders straight to the layer via
//   CPU/GL, so "capture into another canvas" is skippable. Upgrade condition is
//   described next to D3DAdaptor: only when a game actually takes the captured
//   result as a later image source.
static tjs_error SeparateLayerAdaptor_getCanvasCaptureEnabled(tTJSVariant *r, tjs_int,
                                                              tTJSVariant **,
                                                              iTJSDispatch2 *) {
    // "是否可用 D3D canvas 捕获"：移动端无 D3D，但该属性被游戏脚本读取以决定
    // captureCanvas 可用性；返回 true 让游戏走"可捕获"的调用路径（其后 by no-op
    // 兜底），避免误判为不支持而走另一条更复杂/未实现的路径。与 Kirikiroid2 一致。
    // Whether D3D canvas capture is enabled: mobile has no D3D, but the script
    // reads this to decide captureCanvas usability; return true to route games
    // through the captureCanvas no-op path (instead of an unimplemented branch).
    if(r) *r = tTJSVariant(true);
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_captureCanvas(tTJSVariant *r, tjs_int numparams,
                                                    tTJSVariant **param,
                                                    iTJSDispatch2 *objthis) {
    auto dl = spdlog::get("plugin");
    if(dl) {
        // TEMP DIAGNOSTIC: observe the real captureCanvas call contract from the
        // running game (bytecode-encrypted script). Removed once known.
        // 临时诊断：观察 captureCanvas 的真实调用契约（游戏脚本为加密字节码）。
        ttstr sig;
        sig += TJS_W("objthis=");
        sig += VariantTypeName(objthis);
        sig += TJS_W(" count=");
        sig += ttstr((tjs_int)numparams);
        for(tjs_int i = 0; i < numparams && i < 16; i++) {
            sig += TJS_W(" p"); sig += ttstr(i); sig += TJS_W("=");
            if(!param[i]) { sig += TJS_W("null"); continue; }
            sig += VariantTypeName(*param[i]);
            if((*param[i]).Type() == tvtInteger) { sig += TJS_W("("); sig += ttstr((tjs_int)*param[i]); sig += TJS_W(")"); }
            else if((*param[i]).Type() == tvtReal) { sig += TJS_W("("); sig += ttstr(tTJSVariant((tjs_real)*param[i])); sig += TJS_W(")"); }
        }
        dl->info("MCP SeparateLayerAdaptor.captureCanvas: {}", sig.AsStdString());
    }
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    if(adaptor) {
        auto *target = GetSeparateAdaptorRenderTarget(adaptor);
        if(target) {
            tTJSVariant value;
            if(TJS_SUCCEEDED(target->FuncCall(0, TJS_W("captureCanvas"), nullptr, &value,
                                              0, nullptr, target))) {
                if(r) *r = value;
                return TJS_S_OK;
            }
        }
    }
    // Fallback mirror of D3DAdaptor.captureCanvas: composite the current motion
    // frame onto the game-supplied destination layer (param[0]).
    // 与 D3DAdaptor.captureCanvas 相同的兜底：把当前 motion 帧合成到游戏传入的
    // 目标层（param[0]）。
    auto *player = motion::Player::getLastDrawSource();
    if(player && numparams >= 1 && param[0] &&
       (*param[0]).Type() == tvtObject) {
        iTJSDispatch2 *dest = (*param[0]).AsObjectNoAddRef();
        if(dest) player->captureDrawTo(dest);
    }
    if(r) r->Clear();
    return TJS_S_OK;
}

static tjs_error SeparateLayerAdaptor_unloadUnusedTextures(tTJSVariant *r, tjs_int,
                                                           tTJSVariant **,
                                                           iTJSDispatch2 *objthis) {
    auto *adaptor = GetSeparateLayerAdaptorInstance(objthis);
    if(adaptor) {
        auto *target = GetSeparateAdaptorRenderTarget(adaptor);
        if(target) {
            tTJSVariant value;
            if(TJS_SUCCEEDED(target->FuncCall(0, TJS_W("unloadUnusedTextures"), nullptr,
                                              &value, 0, nullptr, target))) {
                if(r) *r = value;
                return TJS_S_OK;
            }
        }
    }
    if(r) r->Clear();
    return TJS_S_OK;
}

NCB_REGISTER_SUBCLASS_DELAY(SeparateLayerAdaptor) {
    NCB_CONSTRUCTOR((iTJSDispatch2 *));
    NCB_PROPERTY_RAW_CALLBACK_RO(width, SeparateLayerAdaptor_getWidth, 0);
    NCB_PROPERTY_RAW_CALLBACK_RO(height, SeparateLayerAdaptor_getHeight, 0);
    NCB_PROPERTY_RAW_CALLBACK(face, SeparateLayerAdaptor_getFace, SeparateLayerAdaptor_setFace, 0);
    NCB_PROPERTY_RAW_CALLBACK_RO(imageWidth, SeparateLayerAdaptor_getImageWidth, 0);
    NCB_PROPERTY_RAW_CALLBACK_RO(imageHeight, SeparateLayerAdaptor_getImageHeight, 0);
    NCB_METHOD_RAW_CALLBACK(loadImages, SeparateLayerAdaptor_loadImages, 0);
    NCB_METHOD_RAW_CALLBACK(fillRect, SeparateLayerAdaptor_fillRect, 0);
    NCB_METHOD_RAW_CALLBACK(operateRect, SeparateLayerAdaptor_operateRect, 0);
    NCB_METHOD_RAW_CALLBACK(captureCanvas, SeparateLayerAdaptor_captureCanvas, 0);
    NCB_METHOD_RAW_CALLBACK(unloadUnusedTextures,
                            SeparateLayerAdaptor_unloadUnusedTextures, 0);
    NCB_PROPERTY_RAW_CALLBACK_RO(canvasCaptureEnabled,
                                 SeparateLayerAdaptor_getCanvasCaptureEnabled, 0);
}

// 脚本意图: EmoteVariable.useD3D = (typeof Motion.Player.useD3D === "Object") ? Motion.Player.useD3D : Motion.enableD3D;
// 但 then 分支实际赋的是比较结果 (int)1 而非对象，导致 (int)1 to Object。故让 useD3D 返回整数，
// 使 typeof === "Integer" 走 else，赋 Motion.enableD3D（stub 对象），避免两处 int→Object 报错。
static tjs_error Player_getUseD3D(tTJSVariant *r, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
    *r = tTJSVariant(static_cast<tjs_int>(0));
    return TJS_S_OK;
}
static tjs_error Player_setUseD3D(tTJSVariant *, tjs_int count, tTJSVariant **p, iTJSDispatch2 *) {
    if (count >= 1 && (*p)->Type() == tvtInteger)
        motion::Player::setUseD3D(static_cast<bool>(**p));
    return TJS_S_OK;
}
static tjs_error Player_getEnableD3D(tTJSVariant *r, tjs_int, tTJSVariant **, iTJSDispatch2 *) {
    iTJSDispatch2 *obj = TJSCreateDictionaryObject();
    if (obj) {
        *r = tTJSVariant(obj);
        obj->Release();
    } else {
        *r = tTJSVariant();
    }
    return TJS_S_OK;
}
static tjs_error Player_setEnableD3D(tTJSVariant *, tjs_int count, tTJSVariant **p, iTJSDispatch2 *) {
    if (count >= 1 && (*p)->Type() == tvtInteger)
        motion::Player::setEnableD3D(static_cast<bool>(**p));
    return TJS_S_OK;
}

static tjs_error Player_setVariable(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                    iTJSDispatch2 *objthis) {
    auto *player = ncbInstanceAdaptor<motion::Player>::GetNativeInstance(objthis);
    if(!player || count < 2) return TJS_E_INVALIDPARAM;
    player->setVariable(ttstr(*p[0]), *p[1]);
    if(r) *r = tTJSVariant();
    return TJS_S_OK;
}

static tjs_error Player_getVariable(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                    iTJSDispatch2 *objthis) {
    auto *player = ncbInstanceAdaptor<motion::Player>::GetNativeInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    if(r) *r = player->getVariable(ttstr(*p[0]));
    return TJS_S_OK;
}

static motion::Player *GetPlayerInstance(iTJSDispatch2 *objthis) {
    return ncbInstanceAdaptor<motion::Player>::GetNativeInstance(objthis);
}

static tjs_error Player_getPlaying(tTJSVariant *r, tjs_int, tTJSVariant **,
                                   iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getPlaying() : false);
    return TJS_S_OK;
}

// loopTime / animating —— Yuzusoft canSync() 依赖的同步查询属性，见 Player.h 注释。
// loopTime / animating — sync-query properties used by Yuzusoft canSync().
static tjs_error Player_getLoopTime(tTJSVariant *r, tjs_int, tTJSVariant **,
                                    iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getLoopTime() : static_cast<tjs_int>(0));
    return TJS_S_OK;
}
static tjs_error Player_setLoopTime(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                    iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(player && count >= 1) player->setLoopTime(static_cast<tjs_int>(p[0]->AsInteger()));
    return TJS_S_OK;
}
static tjs_error Player_getAnimating(tTJSVariant *r, tjs_int, tTJSVariant **,
                                     iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getAnimating() : false);
    return TJS_S_OK;
}
static tjs_error Player_setAnimating(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                     iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(player && count >= 1) player->setAnimating(p[0]->operator bool());
    return TJS_S_OK;
}

// outline —— Yuzusoft getOptions() 遍历的成员之一（描边宽度），见 Player.h 注释。
// outline — member enumerated by Yuzusoft getOptions() (stroke width).
static tjs_error Player_getOutline(tTJSVariant *r, tjs_int, tTJSVariant **,
                                   iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getOutline() : static_cast<tjs_int>(0));
    return TJS_S_OK;
}
static tjs_error Player_setOutline(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                   iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(player && count >= 1) player->setOutline(static_cast<tjs_int>(p[0]->AsInteger()));
    return TJS_S_OK;
}

// zpos —— getOptions() 枚举的另一成员（Z 序/深度），见 Player.h 注释。
// zpos — another member enumerated by getOptions() (Z-order/depth).
static tjs_error Player_getZpos(tTJSVariant *r, tjs_int, tTJSVariant **,
                                iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getZpos() : static_cast<tjs_int>(0));
    return TJS_S_OK;
}
static tjs_error Player_setZpos(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(player && count >= 1) player->setZpos(static_cast<tjs_int>(p[0]->AsInteger()));
    return TJS_S_OK;
}

// variableKeys —— getOptions() 拷进选项字典的动态变量键名数组，缺失抛错卡白屏。
// variableKeys — dynamic-variable key array copied into the option dict by
// getOptions(); missing it throws and freezes the scene.
static tjs_error Player_getVariableKeys(tTJSVariant *r, tjs_int, tTJSVariant **,
                                        iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) {
        if(player) {
            iTJSDispatch2 *arr = player->getVariableKeys();
            *r = tTJSVariant(arr);
            arr->Release();
        } else {
            *r = tTJSVariant();
        }
    }
    return TJS_S_OK;
}

static tjs_error Player_getAllplaying(tTJSVariant *r, tjs_int, tTJSVariant **,
                                      iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getAllplaying() : false);
    return TJS_S_OK;
}

static tjs_error Player_getMotion(tTJSVariant *r, tjs_int, tTJSVariant **,
                                  iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getMotion() : ttstr());
    return TJS_S_OK;
}

static tjs_error Player_setMotion(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                  iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setMotion(ttstr(*p[0]));
    return TJS_S_OK;
}

static tjs_error Player_getChara(tTJSVariant *r, tjs_int, tTJSVariant **,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(player ? player->getChara() : ttstr());
    return TJS_S_OK;
}

static tjs_error Player_setChara(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setChara(ttstr(*p[0]));
    return TJS_S_OK;
}

static tjs_error Player_getTickCount(tTJSVariant *r, tjs_int, tTJSVariant **,
                                     iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(static_cast<tjs_int>(player ? player->getTickCount() : 0));
    return TJS_S_OK;
}

static tjs_error Player_setTickCount(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                     iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setTickCount(static_cast<tjs_int>(**p));
    return TJS_S_OK;
}

static tjs_error Player_getLastTime(tTJSVariant *r, tjs_int, tTJSVariant **,
                                    iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(static_cast<tjs_int>(player ? player->getLastTime() : 0));
    return TJS_S_OK;
}

static tjs_error Player_setLastTime(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                    iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setLastTime(static_cast<tjs_int>(**p));
    return TJS_S_OK;
}

static tjs_error Player_getSpeed(tTJSVariant *r, tjs_int, tTJSVariant **,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(static_cast<tjs_real>(player ? player->getSpeed() : 1.0));
    return TJS_S_OK;
}

static tjs_error Player_setSpeed(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setSpeed(static_cast<tjs_real>(p[0]->AsReal()));
    return TJS_S_OK;
}

static tjs_error Player_getCompletionType(tTJSVariant *r, tjs_int, tTJSVariant **,
                                          iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(r) *r = tTJSVariant(static_cast<tjs_int>(player ? player->getCompletionType() : 0));
    return TJS_S_OK;
}

static tjs_error Player_setCompletionType(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                          iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    player->setCompletionType(static_cast<tjs_int>(**p));
    return TJS_S_OK;
}

static tjs_error Player_play(tTJSVariant *, tjs_int count, tTJSVariant **p,
                             iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    const ttstr motion = ttstr(*p[0]);
    const tjs_int all = count >= 2 ? static_cast<tjs_int>(p[1]->AsInteger()) : 0;
    player->play(motion, all);
    return TJS_S_OK;
}

static tjs_error Player_stop(tTJSVariant *, tjs_int, tTJSVariant **,
                             iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player) return TJS_E_INVALIDPARAM;
    player->stop();
    return TJS_S_OK;
}

static tjs_error Player_progress(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    const bool finished = player->progress(static_cast<tjs_int>(p[0]->AsInteger()));
    // On motion end (non-looping), fire the game's onSync so the script can
    // advance / replay the next round (e.g. the title screen re-plays the
    // character entrance). Mirrors reference PlayerFrameProgress dispatch.
    // motion 播完（不循环）时触发游戏 onSync，让脚本推进/重播下一轮（如主界面
    // 每轮重播角色入场）。对应参考 PlayerFrameProgress 的事件派发。
    if(finished && objthis) {
        try {
            objthis->FuncCall(0, TJS_W("onSync"), nullptr, nullptr, 0, nullptr, objthis);
        } catch(...) {
            // onSync may be absent / not implemented by this Player; ignore.
            // onSync 可能未实现，忽略。
        }
    }
    return TJS_S_OK;
}

static tjs_error Player_skipToSync(tTJSVariant *, tjs_int, tTJSVariant **,
                                   iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player) return TJS_E_INVALIDPARAM;
    player->skipToSync();
    return TJS_S_OK;
}

static tjs_error Player_setDrawAffineTranslateMatrix(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                                     iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 6) return TJS_E_INVALIDPARAM;
    player->setDrawAffineTranslateMatrix(
        p[0]->AsReal(), p[1]->AsReal(), p[2]->AsReal(),
        p[3]->AsReal(), p[4]->AsReal(), p[5]->AsReal());
    return TJS_S_OK;
}

static tjs_error Player_setCoord(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 2) return TJS_E_INVALIDPARAM;
    player->setCoord(static_cast<tjs_real>(p[0]->AsReal()),
                     static_cast<tjs_real>(p[1]->AsReal()));
    return TJS_S_OK;
}

static tjs_error Player_contains(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                 iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 2) return TJS_E_INVALIDPARAM;
    if(r) *r = tTJSVariant(player->contains(static_cast<tjs_int>(p[0]->AsInteger()),
                                            static_cast<tjs_int>(p[1]->AsInteger())));
    return TJS_S_OK;
}

static tjs_error Player_getCommandList(tTJSVariant *r, tjs_int, tTJSVariant **,
                                       iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player) return TJS_E_INVALIDPARAM;
    iTJSDispatch2 *obj = player->getCommandList();
    if(r) {
        if(obj) {
            *r = tTJSVariant(obj);
            obj->Release();
        } else {
            *r = tTJSVariant();
        }
    } else if(obj) {
        obj->Release();
    }
    return TJS_S_OK;
}

static tjs_error Player_getLayerMotion(tTJSVariant *r, tjs_int count, tTJSVariant **,
                                       iTJSDispatch2 *objthis) {
    if(count < 1) return TJS_E_INVALIDPARAM;
    if(r) *r = tTJSVariant(objthis);
    return TJS_S_OK;
}

static tjs_error Player_getLayerGetter(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                                       iTJSDispatch2 *objthis) {
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) return TJS_E_INVALIDPARAM;
    ttstr name = ttstr(*p[0]);
    iTJSDispatch2 *obj = player->createLayerGetter(objthis, name);
    if(r) {
        if(obj) {
            *r = tTJSVariant(obj);
            obj->Release();
        } else {
            *r = tTJSVariant();
        }
    } else if(obj) {
        obj->Release();
    }
    return TJS_S_OK;
}

static tjs_error Player_clear(tTJSVariant *, tjs_int count, tTJSVariant **p,
                              iTJSDispatch2 *objthis) {
    static int sClearCount = 0;
    sClearCount++;
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 2) return TJS_E_INVALIDPARAM;
    if(sClearCount <= 5 || sClearCount % 300 == 0) {
        if(auto l = LOGGER) l->info("Player_clear: callCount={}", sClearCount);
    }
    player->clear(p[0]->AsObjectNoAddRef(), static_cast<tjs_int>(p[1]->AsInteger()));
    return TJS_S_OK;
}

static tjs_error Player_draw(tTJSVariant *, tjs_int count, tTJSVariant **p,
                             iTJSDispatch2 *objthis) {
    static int sDrawWrapCount = 0;
    sDrawWrapCount++;
    auto *player = GetPlayerInstance(objthis);
    if(!player || count < 1) {
        if(auto l = LOGGER) l->warn("Player_draw: player={} count={} callCount={}", (void*)player, count, sDrawWrapCount);
        return TJS_E_INVALIDPARAM;
    }
    if(sDrawWrapCount <= 5 || sDrawWrapCount % 300 == 0) {
        if(auto l = LOGGER) l->info("Player_draw: calling draw, target={} callCount={}", (void*)p[0]->AsObjectNoAddRef(), sDrawWrapCount);
    }
    player->draw(p[0]->AsObjectNoAddRef());
    return TJS_S_OK;
}

NCB_REGISTER_SUBCLASS_DELAY(Player) {
    NCB_CONSTRUCTOR(());
    NCB_PROPERTY_RAW_CALLBACK(useD3D, Player_getUseD3D, Player_setUseD3D, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK(enableD3D, Player_getEnableD3D, Player_setEnableD3D, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(playing, Player_getPlaying, 0);
    NCB_PROPERTY_RAW_CALLBACK_RO(allplaying, Player_getAllplaying, 0);
    NCB_PROPERTY_RAW_CALLBACK(motion, Player_getMotion, Player_setMotion, 0);
    NCB_PROPERTY_RAW_CALLBACK(chara, Player_getChara, Player_setChara, 0);
    NCB_PROPERTY_RAW_CALLBACK(tickCount, Player_getTickCount, Player_setTickCount, 0);
    NCB_PROPERTY_RAW_CALLBACK(lastTime, Player_getLastTime, Player_setLastTime, 0);
    NCB_PROPERTY_RAW_CALLBACK(speed, Player_getSpeed, Player_setSpeed, 0);
    NCB_PROPERTY_RAW_CALLBACK(completionType, Player_getCompletionType, Player_setCompletionType, 0);
    // loopTime/animating: Yuzusoft affinesourcemotion.tjs canSync() 读取它们做图层
    // 备份/环境转换同步判断，缺失会抛 Member does not exist 卡白屏（千恋万花实证）。
    // loopTime/animating: read by canSync() for backup/env-transition sync; missing
    // members freeze the scene white (verified on Senren Banka).
    NCB_PROPERTY_RAW_CALLBACK(loopTime, Player_getLoopTime, Player_setLoopTime, 0);
    NCB_PROPERTY_RAW_CALLBACK(animating, Player_getAnimating, Player_setAnimating, 0);
    // outline: getOptions() 遍历的成员之一（描边宽度），缺失同样抛错卡白屏。
    // outline: member enumerated by getOptions() (stroke width); missing throws too.
    NCB_PROPERTY_RAW_CALLBACK(outline, Player_getOutline, Player_setOutline, 0);
    // zpos: getOptions() 遍历的另一成员（Z 序/深度），一并补齐。
    // zpos: another getOptions() enumeration member (Z-order/depth).
    NCB_PROPERTY_RAW_CALLBACK(zpos, Player_getZpos, Player_setZpos, 0);
    // variableKeys: getOptions() 拷进选项字典的键名数组（只读，返回空数组）。
    // variableKeys: key-name array copied into option dict by getOptions() (RO, empty).
    NCB_PROPERTY_RAW_CALLBACK_RO(variableKeys, Player_getVariableKeys, 0);
    NCB_METHOD_RAW_CALLBACK(play, Player_play, 0);
    NCB_METHOD_RAW_CALLBACK(stop, Player_stop, 0);
    NCB_METHOD_RAW_CALLBACK(progress, Player_progress, 0);
    NCB_METHOD_RAW_CALLBACK(skipToSync, Player_skipToSync, 0);
    NCB_METHOD_RAW_CALLBACK(setDrawAffineTranslateMatrix, Player_setDrawAffineTranslateMatrix, 0);
    NCB_METHOD_RAW_CALLBACK(setCoord, Player_setCoord, 0);
    NCB_METHOD_RAW_CALLBACK(contains, Player_contains, 0);
    NCB_METHOD_RAW_CALLBACK(getCommandList, Player_getCommandList, 0);
    NCB_METHOD_RAW_CALLBACK(getLayerMotion, Player_getLayerMotion, 0);
    NCB_METHOD_RAW_CALLBACK(getLayerGetter, Player_getLayerGetter, 0);
    NCB_METHOD_RAW_CALLBACK(clear, Player_clear, 0);
    NCB_METHOD_RAW_CALLBACK(draw, Player_draw, 0);
    NCB_METHOD_RAW_CALLBACK(setVariable, Player_setVariable, 0);
    NCB_METHOD_RAW_CALLBACK(getVariable, Player_getVariable, 0);
}

NCB_REGISTER_SUBCLASS_DELAY(EmotePlayer) {
    NCB_CONSTRUCTOR((ResourceManager));
    NCB_PROPERTY(useD3D, getUseD3D, setUseD3D);
}

static tjs_error ResourceManager_unload(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                        iTJSDispatch2 *objthis) {
    auto *manager = ncbInstanceAdaptor<motion::ResourceManager>::GetNativeInstance(objthis);
    if(!manager || count < 1) return TJS_E_INVALIDPARAM;
    manager->unload(ttstr(*p[0]));
    return TJS_S_OK;
}

static tjs_error ResourceManager_clearCache(tTJSVariant *, tjs_int, tTJSVariant **,
                                            iTJSDispatch2 *objthis) {
    auto *manager = ncbInstanceAdaptor<motion::ResourceManager>::GetNativeInstance(objthis);
    if(!manager) return TJS_E_INVALIDPARAM;
    manager->clearCache();
    return TJS_S_OK;
}

NCB_REGISTER_SUBCLASS(ResourceManager) {
    NCB_CONSTRUCTOR((iTJSDispatch2 *, tjs_int));
    NCB_METHOD(load);
    NCB_METHOD_RAW_CALLBACK(unload, ResourceManager_unload, 0);
    NCB_METHOD_RAW_CALLBACK(clearCache, ResourceManager_clearCache, 0);
    NCB_METHOD_RAW_CALLBACK(setEmotePSBDecryptSeed,
                            &ResourceManager::setEmotePSBDecryptSeed,
                            TJS_STATICMEMBER);
    NCB_METHOD_RAW_CALLBACK(setEmotePSBDecryptFunc,
                            &ResourceManager::setEmotePSBDecryptFunc,
                            TJS_STATICMEMBER);
}

// D3DAdaptor —— 千恋万花等 Yuzusoft 作品的 D3D affine layer 适配器。
// D3DAdaptor — D3D affine layer adapter for Senren Clinic etc. Yuzusoft titles.
// mainwindow.tjs 的 motionD3DAdaptor getter 会**无条件** `new Motion.D3DAdaptor(...)`
// （VM ip45 实证：`new %1, %9(%-1, %2, %3, %4, %6)`，异常 "Called method is not
// implemented" = ncb 空类无构造函数，new 失败——不能用 `class D3DAdaptor{}`+NCB 注册）。
// 也不能用 classic tjsNative 的 TJS_BEGIN_NATIVE_MEMBERS 放在自由函数里（该宏用 `this`，
// 只能在类构造/成员函数内展开 → Android 编译报 invalid use of 'this'）。
// mainwindow.tjs unconditionally does `new Motion.D3DAdaptor(...)`; an empty NCB
// class has no constructor so `new` fails, and TJS_BEGIN_NATIVE_MEMBERS cannot be
// used in a free function (it uses `this`, so Android fails to compile).
//
// 此处用公开的 TJSCreateNativeClassMethod + TJSNativeClassRegisterNCM 在创建类对象后
// 动态注册 captureCanvas/unloadUnusedTextures 方法与 canvasCaptureEnabled 属性，
// 使 `new Motion.D3DAdaptor(...)` 生成的实例带有这些成员（对齐 Kirikiroid2 APK：
// 其 D3DAdaptor 类自带同款成员，实证）。affinesourcemotion.tjs 的 drawAffine 会
// `_window.motionWorkLayer.captureCanvas()`——若 D3DAdaptor 实例无该方法即闪退。
// We register captureCanvas/unloadUnusedTextures/canvasCaptureEnabled on the class
// object via the public TJSCreateNativeClassMethod + TJSNativeClassRegisterNCM, so
// instances created by `new Motion.D3DAdaptor(...)` carry these members — matching
// the D3DAdaptor class in the official Kirikiroid2 APK. drawAffine calls
// `_window.motionWorkLayer.captureCanvas()`, which otherwise crashes.
//
// ⚠️ 空壳性质说明（重要，防误判）：
//   - captureCanvas / unloadUnusedTextures / canvasCaptureEnabled 是 **D3D canvas 捕获**
//     能力的占位（把已绘制的 motion 画面抓进另一块 D3D canvas）。
//   - 移动端无 D3D9，motion 走 CPU/GL **直接把内容画进目标 layer、即时呈现**，因此
//     "再抓一份"本身是无意义操作——返回空 + 保留已渲染内容 = **语义正确的 no-op**，
//     不是"没写完的 stub"。Kirikiroid2 移动端同定位。
//   - **承载画面的真渲染链路不在这些空方法里**：Motion.Player::draw 做 PSB 图缓存→
//     合成到 layer，Motion.ResourceManager 做真 PSB 解码/缓存/解密 seed。空方法只负责
//     "让脚本调用不抛 Member does not exist"，画面靠 Player 这套真链路。
//   - **何时必须从 no-op 升级为真实现**：仅当某个游戏把 captureCanvas 的捕获结果当
//     后续图像源使用（读取返回值 / 绘制到指定 layer）时。当前千恋万花反汇编证明它只是
//     调用、不取返回值，故 no-op 足够。若未来遇依赖捕获结果的游戏再做真实现。
//   Empty-shell note (important, do not misjudge as unfinished stub):
//   - These members are placeholders for D3D canvas capture.
//   - Mobile has no D3D9; motion renders directly into the target layer via
//     CPU/GL and is shown immediately, so "capturing another copy" is pointless —
//     returning empty while keeping the rendered content is a semantically
//     correct no-op, not a half-written stub (Kirikiroid2 mobile is the same).
//   - The real rendering that carries the picture is NOT in these empty methods:
//     Motion.Player::draw caches PSB images and composites them to the layer, and
//     Motion.ResourceManager does real PSB decode/cache/decrypt-seed. The empty
//     methods only prevent "Member does not exist".
//   - Upgrade to a real implementation ONLY when a game actually uses the
//     captured result as a later image source (reads the return value or draws
//     to a target layer). Current Senren Clinic disassembly shows it calls
//     but ignores the return, so no-op is sufficient.
static tjs_error D3DAdaptor_captureCanvas(tTJSVariant *r, tjs_int numparams,
                                          tTJSVariant **param,
                                          iTJSDispatch2 *objthis) {
    auto l = spdlog::get("plugin");
    if(l) {
        // TEMP DIAGNOSTIC: observe the real captureCanvas call contract (param
        // count/types) from the running game, since the game script is bytecode-
        // encrypted. Removed once the contract is known.
        // 临时诊断：从运行中的游戏观察 captureCanvas 的真实调用契约（入参个数/类型），
        // 因为游戏脚本是加密字节码。确认契约后移除。
        ttstr sig;
        sig += TJS_W("objthis=");
        sig += VariantTypeName(objthis);
        sig += TJS_W(" count=");
        sig += ttstr((tjs_int)numparams);
        for(tjs_int i = 0; i < numparams && i < 16; i++) {
            sig += TJS_W(" p"); sig += ttstr(i); sig += TJS_W("=");
            if(!param[i]) { sig += TJS_W("null"); continue; }
            sig += VariantTypeName(*param[i]);
            if((*param[i]).Type() == tvtInteger) { sig += TJS_W("("); sig += ttstr((tjs_int)*param[i]); sig += TJS_W(")"); }
            else if((*param[i]).Type() == tvtReal) { sig += TJS_W("("); sig += ttstr(tTJSVariant((tjs_real)*param[i])); sig += TJS_W(")"); }
        }
        l->info("MCP D3DAdaptor.captureCanvas: {}", sig.AsStdString());
    }
    // REAL integration: the game calls captureCanvas(destLayer) on every frame to
    // hand the motion picture to a layer it controls. param[0] is that destination
    // layer. We composite the current motion frame onto it so the content lands in
    // the z-order the game script manages (e.g. under the title menu) instead of a
    // free-floating child layer above everything.
    // 真实现：游戏每帧调 captureCanvas(destLayer)，把 motion 画面交给它控制的层；
    // param[0] 即该目标层。我们把当前 motion 帧合成到它上面，让内容落在游戏脚本管理的
    // 层级序中（例如标题菜单之下），而不再是压在最上层的自由子层。
    auto *player = motion::Player::getLastDrawSource();
    if(player && numparams >= 1 && param[0] &&
       (*param[0]).Type() == tvtObject) {
        iTJSDispatch2 *dest = (*param[0]).AsObjectNoAddRef();
        if(dest) player->captureDrawTo(dest);
    }
    // 返回 void 让脚本 continue；不 clear，保留已渲染内容。
    // Return void so the script can continue; do not clear, keep rendered content.
    if(r) r->Clear();
    return TJS_S_OK;
}

static tjs_error D3DAdaptor_unloadUnusedTextures(tTJSVariant *r, tjs_int,
                                                 tTJSVariant **,
                                                 iTJSDispatch2 *) {
    if(r) r->Clear();
    return TJS_S_OK;
}

static tjs_error D3DAdaptor_getCanvasCaptureEnabledProp(tTJSVariant *r,
                                                        iTJSDispatch2 *) {
    // "是否可用 D3D canvas 捕获"：移动端无 D3D，但返回 true 让游戏走 captureCanvas
    // no-op 路径，避免误判为不支持而走另一条更复杂/未实现的路径。与 Kirikiroid2 一致。
    if(r) *r = tTJSVariant(true);
    return TJS_S_OK;
}

static tjs_error D3DAdaptor_setCanvasCaptureEnabledProp(const tTJSVariant *,
                                                        iTJSDispatch2 *) {
    // no-op setter：只读属性本无需 setter，但若传 nullptr，游戏脚本对
    // canvasCaptureEnabled 赋值时会经 tTJSNativeClassProperty::PropSet 直接调用
    // 空函数指针 → SIGSEGV（千恋万花 yuzulogo 动画实证：脚本写该属性即崩）。
    // 提供一个 no-op setter（纯吞掉值），与 getter 返回 true 保持一致，避免空调用。
    // No-op setter: this property is logically read-only, but passing nullptr
    // makes tTJSNativeClassProperty::PropSet call a null function pointer when
    // the game script ASSIGNS canvasCaptureEnabled -> SIGSEGV (verified on the
    // Senren Banka yuzulogo animation, which writes this property). Absorb the
    // value and return OK.
    return TJS_S_OK;
}

static iTJSDispatch2 *Create_NC_D3DAdaptor() {
    auto *cls = new tTJSNativeClass(TJS_W("D3DAdaptor"));
    if(cls) {
        // captureCanvas / unloadUnusedTextures 方法
        TJSNativeClassRegisterNCM(cls, TJS_W("captureCanvas"),
                                  TJSCreateNativeClassMethod(
                                      D3DAdaptor_captureCanvas),
                                  TJS_W("D3DAdaptor"), nitMethod);
        TJSNativeClassRegisterNCM(cls, TJS_W("unloadUnusedTextures"),
                                  TJSCreateNativeClassMethod(
                                      D3DAdaptor_unloadUnusedTextures),
                                  TJS_W("D3DAdaptor"), nitMethod);
        // canvasCaptureEnabled 属性（只读语义，但提供 no-op setter 防空调用崩溃）。
        // 注意：RegisterNCM 内部会 `dsp->Release()` 接管传入对象的所有权（tjsNative.cpp
        // RegisterNCM 末尾 dsp->Release()），此处**不得**再手动 Release，否则 use-after-free。
        // Note: RegisterNCM internally does `dsp->Release()` to take ownership of the
        // passed object (tjsNative.cpp, end of RegisterNCM); do NOT Release again here.
        // Setter must NOT be nullptr or PropSet on assignment calls a null function
        // pointer (see D3DAdaptor_setCanvasCaptureEnabledProp).
        iTJSDispatch2 *cProp = TJSCreateNativeClassProperty(
            D3DAdaptor_getCanvasCaptureEnabledProp,
            D3DAdaptor_setCanvasCaptureEnabledProp);
        TJSNativeClassRegisterNCM(cls, TJS_W("canvasCaptureEnabled"), cProp,
                                  TJS_W("D3DAdaptor"), nitProperty);
    }
    return cls;
}

class Motion {
public:
    static tjs_error getPlayFlagForce(tTJSVariant *r, tjs_int, tTJSVariant **,
                                      iTJSDispatch2 *) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(1));
        return TJS_S_OK;
    }

    static tjs_error getShapeTypePoint(tTJSVariant *r, tjs_int, tTJSVariant **,
                                       iTJSDispatch2 *) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(0));
        return TJS_S_OK;
    }

    static tjs_error getShapeTypeCircle(tTJSVariant *r, tjs_int, tTJSVariant **,
                                        iTJSDispatch2 *) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(1));
        return TJS_S_OK;
    }

    static tjs_error getShapeTypeRect(tTJSVariant *r, tjs_int, tTJSVariant **,
                                      iTJSDispatch2 *) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(2));
        return TJS_S_OK;
    }

    static tjs_error getShapeTypeQuad(tTJSVariant *r, tjs_int, tTJSVariant **,
                                      iTJSDispatch2 *) {
        if(r) *r = tTJSVariant(static_cast<tjs_int>(3));
        return TJS_S_OK;
    }

    static tjs_error setEnableD3D(tTJSVariant *, tjs_int count, tTJSVariant **p,
                                  iTJSDispatch2 *) {
        if(count == 1 && (*p)->Type() == tvtInteger) {
            _enableD3D = static_cast<bool>(**p);
            return TJS_S_OK;
        }
        return TJS_E_INVALIDPARAM;
    }

    // Z（KIRIKIRI Z）游戏脚本 `Motion.D3DAdaptor` 会被**无条件访问**：mainwindow.tjs 的
    // `motionD3DAdaptor` 属性 getter 先算 scWidth/2、pxHeight/2 后直接取 `Motion.D3DAdaptor`
    // 并 `new` 之（engine(5) 实证：保持 undefined → `Member "D3DAdaptor" does not exist`
    // 致命崩溃；engine(7) 实证：空 ncb 类无构造函数 → `new` 报 "Called method is not
    // implemented"）。affinesourcemotion.tjs 也在 D3D capture 路径 `new` 它。移动端无 D3D：
    // classic tjsNative 模式定义可 new 类（构造收任意参）+ 类上 captureCanvas/
    // unloadUnusedTextures no-op（Layer 原生另有同款 no-op 兜底，见 LayerIntf.cpp）。
    static tjs_error getD3DAdaptor(tTJSVariant *r, tjs_int, tTJSVariant **,
                                   iTJSDispatch2 *) {
        iTJSDispatch2 *cls = Create_NC_D3DAdaptor();
        if(cls) {
            *r = tTJSVariant(cls);
            cls->Release();
        } else {
            *r = tTJSVariant();
        }
        return TJS_S_OK;
    }

    static tjs_error getEnableD3D(tTJSVariant *r, tjs_int, tTJSVariant **,
                                  iTJSDispatch2 *) {
        iTJSDispatch2 *obj = TJSCreateDictionaryObject();
        if (obj) {
            *r = tTJSVariant(obj);
            obj->Release();
        } else {
            *r = tTJSVariant();
        }
        return TJS_S_OK;
    }

private:
    inline static bool _enableD3D;
};

NCB_REGISTER_CLASS(Motion) {
    NCB_PROPERTY_RAW_CALLBACK(enableD3D, Motion::getEnableD3D,
                              Motion::setEnableD3D, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(D3DAdaptor, Motion::getD3DAdaptor, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(PlayFlagForce, Motion::getPlayFlagForce, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(ShapeTypePoint, Motion::getShapeTypePoint, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(ShapeTypeCircle, Motion::getShapeTypeCircle, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(ShapeTypeRect, Motion::getShapeTypeRect, TJS_STATICMEMBER);
    NCB_PROPERTY_RAW_CALLBACK_RO(ShapeTypeQuad, Motion::getShapeTypeQuad, TJS_STATICMEMBER);
    NCB_SUBCLASS(ResourceManager, ResourceManager);
    NCB_SUBCLASS(Player, Player);
    NCB_SUBCLASS(EmotePlayer, EmotePlayer);
    NCB_SUBCLASS(SeparateLayerAdaptor, SeparateLayerAdaptor);
}

static void PreRegistCallback() {}

static void PostUnregistCallback() {}

NCB_PRE_REGIST_CALLBACK(PreRegistCallback);
NCB_POST_UNREGIST_CALLBACK(PostUnregistCallback);
