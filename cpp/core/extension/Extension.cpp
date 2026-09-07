
#include "tjsCommHead.h"
#include "Extension.h"

//---------------------------------------------------------------------------
// TVPAddClassHandler related
//---------------------------------------------------------------------------
struct tTVPAtClassInstallInfo {
    tTVPAtClassInstallInfo(const tjs_char *name,
                           iTJSDispatch2 *(*handler)(iTJSDispatch2 *),
                           const tjs_char *dependences) {
        Name = name, Handler = handler;
        if(dependences) {
            ttstr dep(dependences);
            const tjs_char *start = dependences;
            const tjs_char *cur = dependences;
            while(*cur) {
                if((*cur) == TJS_W(',')) {
                    if(start != cur) {
                        Dependences.emplace_back(start, cur - start);
                    }
                    start = cur + 1;
                }
                cur++;
            }
            if(start != cur) {
                Dependences.emplace_back(start, cur - start);
            }
        }
    }
    const tjs_char *Name;
    iTJSDispatch2 *(*Handler)(iTJSDispatch2 *);
    std::vector<ttstr> Dependences; // 依存クラスリスト
};
static std::vector<tTVPAtClassInstallInfo> *TVPAtClassInstallInfos = nullptr;
static bool TVPAtInstallClass = false;
//---------------------------------------------------------------------------
void TVPAddClassHandler(const tjs_char *name,
                        iTJSDispatch2 *(*handler)(iTJSDispatch2 *),
                        const tjs_char *dependences) {
    if(TVPAtInstallClass)
        return;

    if(!TVPAtClassInstallInfos)
        TVPAtClassInstallInfos = new std::vector<tTVPAtClassInstallInfo>();
    TVPAtClassInstallInfos->emplace_back(name, handler, dependences);
}
//---------------------------------------------------------------------------
void TVPCauseAtInstallExtensionClass(iTJSDispatch2 *global) {
    if(TVPAtInstallClass)
        return;
    TVPAtInstallClass = true;

    if(TVPAtClassInstallInfos) {
        iTJSDispatch2 *dsp;
        tTJSVariant val;
        std::vector<tTVPAtClassInstallInfo>::iterator i;
        for(i = TVPAtClassInstallInfos->begin();
            i != TVPAtClassInstallInfos->end(); i++) {
            dsp = i->Handler(global);
            val = tTJSVariant(dsp /*, dsp*/);
            dsp->Release();
            global->PropSet(TJS_MEMBERENSURE | TJS_IGNOREPROP, i->Name, nullptr,
                            &val, global);
        }
        delete TVPAtClassInstallInfos;
        TVPAtClassInstallInfos = nullptr;
    }
}
//---------------------------------------------------------------------------
void TVPResetExtensionClassInstallStateForRestart() {
    // 二次初始化前复位「已触发安装 / 待装类列表」状态。
    // TVPAtClassInstallInfos 在 TVPCauseAtInstallExtensionClass 里已被 delete 置空，
    // 这里只需复位标志，使下次 TVPAddClassHandler / TVPCauseAtInstallExtensionClass
    // 可再次登记并安装。
    TVPAtInstallClass = false;
    TVPAtClassInstallInfos = nullptr;
}
//---------------------------------------------------------------------------
