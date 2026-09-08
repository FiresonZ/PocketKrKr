//---------------------------------------------------------------------------
// k2compat.dll (Krkr2Compat) 内置 TJS 脚本
//---------------------------------------------------------------------------
// 来源: https://github.com/krkrz/Krkr2Compat (data/k2compat/*.tjs, GPL-2.0)
// 真实实现: k2compat.dll 在 krkrz 中即把上述纯 TJS 兼容层打进 DLL, link 时执行；
// 这里等价内嵌, Plugins.link("k2compat.dll") 时按依赖序执行。
// 注: deskinfo/scrinfo 对应 K2COMPAT_SPEC_* 条件编译, 默认关闭, 不内嵌。
//---------------------------------------------------------------------------
#include "tjsCommHead.h"
#include "tjsString.h"
#include "ScriptMgnIntf.h"
#include "CharacterSet.h"
#include <spdlog/spdlog.h>

namespace {

const char *k2compat_main_tjs = R"KRKRZ(//=============================================================
// k2compat - 吉里吉里２専用クラス／関数の吉里吉里Ｚ向け互換実装

//-------------------------------------------------------------
// k2compat.tjsをロードする前に下記条件コンパイル式が定義可能です

//// 各種互換実装を無効化します
//
//	@set (K2COMPAT_PURGE_MENU = 1) // MenuItem, Window.menu (menu.dll)
//	@set (K2COMPAT_PURGE_KAGPARSER = 1) // KAGParser (KAGParrser.dll
//	@set (K2COMPAT_PURGE_FONTSELECT = 1) // Layer.font.doUserSelect
//	@set (K2COMPAT_PURGE_INPUTSTRING = 1) // System.inputString
//	@set (K2COMPAT_PURGE_WINDOWPROP = 1) // Window.innerSunken, Window.showScrollBars
//	@set (K2COMPAT_PURGE_PTDRAWDEVICE = 1) // Window.PassThroughDrawDevice
//	@set (K2COMPAT_PURGE_PAD = 1) // Pad
//	@set (K2COMPAT_PURGE_DEBUG = 1) // 下記のDebugクラス周りの実装一括
//		@set (K2COMPAT_PURGE_CONSOLE = 1) // コンソール (Debug.console)
//		@set (K2COMPAT_PURGE_CONTROLLER = 1) // コントローラ (Debug.controller)
//		@set (K2COMPAT_PURGE_SCRIPTEDITOR = 1) // スクリプトエディタ (Debug.scripted)
//		@set (K2COMPAT_PURGE_WATCH = 1) // 監視式 (Debug.watchexp)
//		@set (K2COMPAT_PURGE_HOTKEY = 1) // Shift+F1～F4のホットキー

//// System.desktop{Left,Top,Width,Height} の仕様を変更します(要windowEx.dllプラグイン)
//	@set (K2COMPAT_SPEC_DESKTOPINFO =  1) // 常にプライマリモニタのDesktop情報を返します
//	@set (K2COMPAT_SPEC_DESKTOPINFO = -1) // Window.mainWindowのあるDesktop情報を返します
//  ※未指定の場合はZ本来の仕様(全モニタ統合した座標情報)のままになります

//// System.screen{Width,Height} の仕様を変更します(要windowEx.dllプラグイン)
//	@set (K2COMPAT_SPEC_SCREENINFO = 1) // 常にプライマリモニタのDesktop情報を返します
//  ※未指定の場合はZ本来の仕様(mainWindowのあるScreenサイズを返す)のままになります


//// k2compat.tjsのデバッグログ表示を有効にします
//	@set (K2COMPAT_VERBOSE = 1)

//// ダミープロパティ(Window.innerSunken等)に書き込まれた場合のログ出力を抑制します
//	@set (K2COMPAT_PURGE_DUMMYPROP_LOG = 1)


class Krkr2CompatUtils {
	var scriptBase = "k2compat/";
	var scriptLoaded = %[];
	var messageTag = "Krkr2CompatUtils";

	function trace {
		@if (K2COMPAT_VERBOSE)
		Debug.message(...);
		@endif
	}
	function error(message) {
		throw new Exception(messageTag + ": " +message);
	}
	function include(file) {
		trace(messageTag+".include", file);
		Scripts.execStorage(file);
	}
	function require(module) {
		if (module == "" || scriptLoaded[module]) return;
		trace(messageTag+".require", module);
		scriptLoaded[module] = true;
		include(scriptBase + @"k2compat_${module}.tjs");
	}
	function requireWIN32Dialog() {
		if (typeof global.WIN32GenericDialogEX == "Object") return;
		trace(messageTag+".requireWIN32Dialog");
		if (typeof global.WIN32Dialog != "Object") {
			delete global.WIN32Dialog;
			loadPlugin("win32dialog.dll");
		}
		var file = @"win32dialog.tjs";
		if (Storages.isExistentStorage(scriptBase + file)) {
			include(scriptBase + file);
		} else if (Storages.isExistentStorage(file)) {
			include(file);
		} else {
			error(file+" not found.");
		}
	}
	function requireWindowEx() {
		if (typeof global.Window.registerExEvent == "Object") return;
		trace(messageTag+".requireWindowEx");
		onBeforeWindowExLink();
		loadPlugin("windowEx.dll");
	}
	function onBeforeWindowExLink() {
		// [XXX] typeof 参照で遅延読み込みが入る／ない場合はダミーを作成
		if (typeof global.MenuItem == "undefined")
			/**/   global.MenuItem  = %[];

		if (typeof global.Pad      == "undefined")
			/**/   global.Pad       = %[];

		if (typeof Debug.console   == "undefined")
			/**/   Debug.console    = %[];
	}

	var _inPluginLink;
	function loadPlugin(plugin, raiseerror = true) {
		trace(messageTag+".loadPlugin", plugin);
		try {
			Plugins.link(plugin);
		} catch(e) {
			Debug.notice(e.message, Scripts.getTraceString());
			if (raiseerror) throw e;
		}
	}
	function hookPluginLink(orig, dll, *) {
		var r, target;
		with (Storages) target=.chopStorageExt(.extractStorageName(((string)dll).toLowerCase()));
		trace(messageTag+".hookPluginLink", target);
		if (target == "windowex") {
			onBeforeWindowExLink();
		} else if (target.indexOf("kagparser") == 0) {
			delete global.KAGParser;
		}
		_inPluginLink = true;
		try {
			r = orig(dll, *);
		} catch (e) {
			_inPluginLink = false;
			throw e;
		}
		_inPluginLink = false;
		return r;
	}

	function delayLoadPlugin   (plugin, ref, pre) { return makeDelay(@"${pre} loadPlugin('${plugin}'); return ${ref};"); }
	function delayLoadFunction (module, ref)      { return makeDelay(@"require('${module}'); return ${ref};"); }
	function delayLoadSingleton(module, cls, ovr) { return makeDelay(@"require('${module}'); return ${ovr} = new ${cls}();"); }
	function delayLoadProperty { return delayLoadFunction(...); }

	function makeDelay(funcstr, context = this) {
		var unnamed = %[], eval = @"property _ { getter { ${funcstr} } }";
		(function (eval) { eval!; } incontextof unnamed)(eval);
		return (&unnamed._) incontextof context;
	}

	function makeDummyProperty(cls, key, value) {
		var chk;
		try { chk = typeof cls[key]; } catch {} // [XXX] クラスのプロパティを直接typeofすると実行コンテキストが違う例外が飛ぶ場合がある
		if (chk == "undefined") {
			(function (key, value, prefix) {
				(@"property ${key} { getter { return typeof this.${prefix}${key} != 'undefined' ? this.${prefix}${key} : ${value}; }" +
				 @"setter(v) { this.${prefix}${key} = v;"
				 @if (! K2COMPAT_PURGE_DUMMYPROP_LOG)
				 + @"global.Debug.message('Krkr2CompatUtils: dummy property ${key} set', v, Scripts.getTraceString());"
				 @endif
				 +  "} }"
				 )!;
			} incontextof cls)(key, value, "_k2compat_");
		}
	}

	function hookInjection(cls, func, injection, context = null) {
		var hook = @"_${func}_k2compat";
		while (typeof cls[hook] != "undefined") hook+="_";
		var orig = cls[func];
		cls[func] = @"function(*) { return (${hook}[0] incontextof this)(${hook}[1] incontextof this, *); }"! incontextof context;
		cls[hook] = [ injection, orig ];
	}

	function createDebugShortcutMenuItem(win, exp, shortcut, sysarg) {
		//trace("createDebugShortcutMenuItem", sysarg);
		var ovr = System.getArgument("-"+sysarg);
		if (ovr !== void) shortcut=ovr; // -hkXXX= 指定がある場合は上書き
		if (shortcut == "") return; // -hkXXX="" ならショートカット無効

		var item = new MenuItem(win, sysarg);
		with (item) {
			.exp = exp;
			.onClick = function {
				if (isvalid this) try { this.exp(); } catch {}
				//Debug.notice(caption, "clicked", typeof exp);
			} incontextof item;
			.shortcut = shortcut;
			.visible = false;
		}
		win.add(item);
		win.menu.add(item);
		return item;
	}

	function toString(target, level=0, sep="\t") {
		var key = @"(${(typeof target).toLowerCase()})", value;
		switch (typeof target) {
		case "Object":
			value = target ? (string)target : "null";
			/**/ if (target instanceof "Function") key = "(Function)";
			else if (target instanceof "Property") key = "(Property)";
			else if (target instanceof "Class")    key = "(Class)"; // [MEMO] Scripts.getClassNames はインスタンスにしか効かない
			else if (target instanceof "Array" || target instanceof "Dictionary") {
				var isdic = target instanceof "Dictionary", ext = [];
				ext.assign(target);
				var n = isdic ? ext.count\2 : ext.count;
				value = isdic ? @"(Dictionary:${n})%[" : @"(Array:${n})[";
				var step = isdic ? 2 : 1;
				var large =  n >= 10; // [XXX]自動改行対応の閾値
				var showkey = isdic || large;
				var cr = (sep != "" && large);
				if (cr) value += "\n";
				for (var i=0, cnt=ext.count; i < cnt; i+=step) {
					var name = isdic ? @'"${ext[i]}"' : (string)i;
					var item = toString(ext[isdic ? (i+1) : i], level+1, sep);
					if (level>0 && cr) value += ((string)sep).repeat(level);
					if (showkey) value += name + "=>";
					value += item;
					if (i+step < cnt) {
						value += ",";
						if (cr) value += "\n";
					}
				}
				value += "]";
			}
			break;
		case "Integer": key = "(int)"; value = "%d".sprintf(target); break;
		case "String":  value = '"'+target.escape()+'"'; break;
		case "Octet":
			var oct = "<% ";
			for (var i = 0, cnt = target.length; i < cnt; i++) oct+="%02X ".sprintf(target[i]);
			oct += "%>";
			return "(octet)"+oct;
		default:
			value = (string)target;
			break;
		}
		return key + value;
	}
}
//=============================================================
with (global.Krkr2CompatUtils = new Krkr2CompatUtils()) // replace singleton instance
{
	var delaylink = false;

	@if (! K2COMPAT_PURGE_MENU)
	if (typeof global.MenuItem == "undefined") {
		// [MEMO] 遅延読み込みに若干問題あり？(WindowExとの相性）
		var del = "delete global.MenuItem; delete global.Window.menu;";
		&global.MenuItem = .delayLoadPlugin("menu.dll", "MenuItem", del);
		&Window.menu = .makeDelay("var ref = global.MenuItem; return &this.menu = (global.Window.menu incontextof this);", null);
		delaylink = true;
	}
	@endif

	@if (! K2COMPAT_PURGE_KAGPARSER)
	if (typeof global.KAGParser == "undefined") {
		// [MEMO] 遅延読み込みに若干問題あり？(KAGParserExとの相性）
		&global.KAGParser = .delayLoadPlugin("KAGParser.dll", "KAGParser", "delete global.KAGParser;");
		delaylink = true;
	}
	@endif

	// Plugins.linkフック
	if (delaylink) {
		.hookInjection(Plugins, "link", function {
			return global.Krkr2CompatUtils.hookPluginLink(...);
		} incontextof null, Plugins);
	}

	@if (! K2COMPAT_PURGE_FONTSELECT)
	&System.doFontSelect = .delayLoadFunction("fontselect",  "System.doFontSelect");
	Layer.k2compat_doUserSelect = function(*) {
		var face = System.doFontSelect(this, *);
		if (face !== void) this.font.face = face;
		return face !== void;
	} incontextof null;
	.hookInjection(Layer, "Layer", function (orig, *) {
		var r = orig(*);
		this.font.doUserSelect = (this.k2compat_doUserSelect);
		return r;
	} incontextof null);
	@endif

	@if (! K2COMPAT_PURGE_INPUTSTRING)
	&System.inputString  = .delayLoadFunction("inputstring", "System.inputString");
	@endif

	@if (! K2COMPAT_PURGE_WINDOWPROP)
	.makeDummyProperty(Window, "innerSunken",    /*default value*/true);
	.makeDummyProperty(Window, "showScrollBars", /*default value*/true);
	@endif

	@if (! K2COMPAT_PURGE_PTDRAWDEVICE)
	if (typeof global.Window.PassThroughDrawDevice == "undefined") {
		/**/   global.Window.PassThroughDrawDevice =
			%[ recreate:function{}, dtNone:0, dtDrawDib:1, dtDBGDI:2, dtDBDD:3, dtDBD3D:4 ];
	}
	@endif

	@if (K2COMPAT_SPEC_DESKTOPINFO)
	{
		var sel = "primary_", props = ["Left", "Top", "Width", "Height"], amp = "&";
		@if (K2COMPAT_SPEC_DESKTOPINFO < 0)
			sel = "mainwin_";
		@endif
		for (var i = props.count-1; i >= 0; i--) {
			var name = props[i];
			&System["desktop"+name] = .delayLoadProperty("deskinfo", @"*(${amp}System.desktop${name} = ${amp}_System_${sel}desktop${name})");
		}
	}
	@endif
	@if (K2COMPAT_SPEC_SCREENINFO)
	&System.screenWidth  = .delayLoadProperty("scrinfo", "System.screenWidth");
	&System.screenHeight = .delayLoadProperty("scrinfo", "System.screenHeight");
	@endif

	@if (! K2COMPAT_PURGE_PAD)
	&global.Pad = .delayLoadProperty("pad", "Pad");
	@endif

	@if (! K2COMPAT_PURGE_DEBUG)
	if (System.getArgument("-debugwin") != "no") {
		@if (! K2COMPAT_PURGE_CONSOLE)
		&Debug.console = .delayLoadSingleton("console", "DebugConsoleCompatDialog", "&Debug.console");
		@endif
		@if (! K2COMPAT_PURGE_CONTROLLER)
		&Debug.controller = %[]; //.delayLoadSingleton("controller", "DebugControllerCompatDialog", "&Debug.controller");
		@endif
		@if (! K2COMPAT_PURGE_SCRIPTEDITOR)
		&Debug.scripted = .delayLoadSingleton("pad", "DebugScriptEditorCompatPad", "&Debug.scripted");
		@endif
		@if (! K2COMPAT_PURGE_WATCH)
		&Debug.watchexp = %[]; //.delayLoadSingleton("watch", "DebugWatchExpressionCompatDialog", "&Debug.watchexp");
		@endif
		@if (! K2COMPAT_PURGE_HOTKEY && !(K2COMPAT_PURGE_CONSOLE && K2COMPAT_PURGE_CONTROLLER && K2COMPAT_PURGE_SCRIPTEDITOR && K2COMPAT_PURGE_WATCH))
		.hookInjection(Window, "Window", function (orig, *) {
			var r = orig(*);
			if (typeof this.menu == "Object") {
				var create = global.Krkr2CompatUtils.createDebugShortcutMenuItem;
				@if (! K2COMPAT_PURGE_CONSOLE)
				create(this, function { with (console)    .visible=!.visible; } incontextof Debug, "Shift+F4", "hkconsole");
				@endif
				@if (! K2COMPAT_PURGE_CONTROLLER)
				create(this, function { with (controller) .visible=!.visible; } incontextof Debug, "Shift+F1", "hkcontroller");
				@endif
				@if (! K2COMPAT_PURGE_SCRIPTEDITOR)
				create(this, function { with (scripted)   .visible=!.visible; } incontextof Debug, "Shift+F2", "hkeditor");
				@endif
				@if (! K2COMPAT_PURGE_WATCH)
				create(this, function { with (watchexp)   .visible=!.visible; } incontextof Debug, "Shift+F3", "hkwatch");
				@endif
			}
			return r;
		} incontextof null);
		@endif
	} else {
		var dummyObject = %[];
		@if (! K2COMPAT_PURGE_CONSOLE)
		&Debug.console = dummyObject;
		@endif
		@if (! K2COMPAT_PURGE_CONTROLLER)
		&Debug.controller = dummyObject;
		@endif
	}
	@endif // !K2COMPAT_PURGE_DEBUG
}
)KRKRZ";

const char *k2compat_win32dialog_tjs = R"KRKRZ(Plugins.link("win32dialog.dll") if (typeof global.WIN32Dialog == "undefined");

class WIN32DialogEX extends WIN32Dialog {
	// コンストラクタ
	function WIN32DialogEX(owner) {
		this.owner = owner if (typeof owner == "Object");
		super.WIN32Dialog(null); // 必ず自分自身にイベントを投げる
	}
	function finalize() {
		removeAllBitmap();
		super.finalize(...);
	}
	// オーナーへのイベント投げは自分
	var owner;

	// テンプレート情報保持用クラス
	var Header = global.WIN32Dialog.Header;
	var Items  = global.WIN32Dialog.Items;

	var itemMap   = %[];
	var itemNames = [];
	var itemAlias = %[];

	// 各コントロールの結果保持用
	var itemResults;

	// 文字列ID割り当てる番号
	var namedIDnumber = 10000;

	property dm { getter { return global.Debug.message; } }

	property results { getter { return itemResults; } }

	// テンプレートを流し込む
	function store(elm) {
		if (typeof elm != 'Object') return false;

		var head = new Header();
		var items = [];
		var cnt = 0, n = (typeof elm.items == "Object") ? elm.items.count : 0;

		head.store(elm);
		for (var i = 0; i < n; i++) {
			if (typeof elm.items[i] != 'Object') continue;
			var tmp = %[];
			(global.Dictionary.assign incontextof tmp)(elm.items[i], true);

			// パラメータのエイリアス
			makeAlias(tmp, "id", "ID");
			makeAlias(tmp, "cx", "w");
			makeAlias(tmp, "cy", "h");
			makeAlias(tmp, "windowClass", "class");

			var origID;
			switch (typeof tmp.id) {
			case "String":
				if (tmp.id != "") {
					origID = tmp.id;
					tmp.id = namedIDnumber++; // 文字列のIDは数値に置き換える
					itemAlias[tmp.id] = origID;
				}
				break;
			case "Integer":
				origID = tmp.id;
				break;
			default:
				tmp.id = -1;
				break;
			}

			// itemMap / itemNames を設定
			//dm(i, origID);
			if (origID != "" && origID != -1) {
				if (typeof itemMap[origID] == "undefined") itemNames.add(origID);
				else if (!tmp.multipleid) throw new Exception(@"IDが重複しています: ${i}, ${origID}, ${tmp.id}");
				itemMap[origID] = tmp;
				itemMap[tmp.id] = tmp if (origID != tmp.id);
			}

			var item = new Items();
			items.add(item);
			item.store(tmp);
			cnt++;
		}
		head.dlgItems = cnt;

		// プラグインにテンプレ情報を渡す
		makeTemplate(head, items*);

		invalidate head;
		for (var i = 0; i < cnt; i++) invalidate items[i];
	}

	// エイリアス生成
	function makeAlias(dict, orig, alias) {
		if (typeof dict[alias] != "undefined" && typeof tmp[orig] == "undefined") {
			tmp[orig] = tmp[alias];
			delete tmp[alias];
		}
	}

	// 文字列IDを渡せるように
	function getNumberdId(id, raiseException = true) {
		var item = itemMap[id];
		var r = (typeof item == "Object" && typeof item.id != "undefined") ? item.id : id;
		if (typeof r != "Integer") {
			var text = @"コントロールID(${id})が見つかりません";
			if (raiseException) throw new Exception(text);
			else {
				Debug.notice(text);
				r = -1;
			}
		}
		return r;
	}
	function getNamedId(id) {
		return (id != "" && itemAlias[id] != "") ? itemAlias[id] : id;
	}
	function getItem(id)               { return super.getItem(        getNumberdId(id)); }
	function setItemInt(id, value)     { return super.setItemInt(     getNumberdId(id), value); }
	function getItemInt(id)            { return super.getItemInt(     getNumberdId(id)); }
	function setItemText(id, value)    { return super.setItemText(    getNumberdId(id), value); }
	function getItemText(id)           { return super.getItemText(    getNumberdId(id)); }
	function setItemEnabled(id, value) { return super.setItemEnabled( getNumberdId(id), value); }
	function getItemEnabled(id)        { return super.getItemEnabled( getNumberdId(id)); }
	function setItemFocus(id)          { return super.setItemFocus(   getNumberdId(id)); }
	function setItemPos(id, x, y)      { return super.setItemPos(     getNumberdId(id), x, y); }
	function setItemSize(id, w, h)     { return super.setItemSize(    getNumberdId(id), w, h); }
	function setItemBitmap(id, layer)  {
		var bmp;
		if (typeof   layer == "Object" && layer) {
			if (     layer instanceof "Layer" ) bmp = new global.WIN32Dialog.Bitmap(layer);
			else if (layer instanceof "Bitmap") bmp = layer;
		}
		if (bmp === void) return;
		allBitmaps.add(bmp);
		return super.setItemBitmap(getNumberdId(id), bmp);
	}
	var allBitmaps = [];
	function removeAllBitmap() {
		for (var i = allBitmaps.count-1; i >= 0; i--) invalidate allBitmaps[i];
		allBitmaps.clear();
	}
	function sendItemMessage(id, msg, *) {
		id = getNumberdId(id);
		var sim = super.sendItemMessage;
		switch (typeof msg) {
		case "Integer": return sim(id, msg, *);
		case "String":  return sim(id, this[msg], *);
		case "Object":  return sim(id, msg.message, msg.wparam, msg.lparam);
		}
	}
	function getCheckBox(id)        { return sendItemMessage(id, BM_GETCHECK, 0, 0); }
	function setCheckBox(id, value) { return sendItemMessage(id, BM_SETCHECK, +value, 0); }

	// 初期化処理用
	function setListBoxTexts( *) { return addItemStrings(LB_ADDSTRING, *); }
	function setComboBoxTexts(*) { return addItemStrings(CB_ADDSTRING, *); }
	function selectListBox( *)   { return selectItem(LB_SETCURSEL, *); }
	function selectComboBox(*)   { return selectItem(CB_SETCURSEL, *); }

	function addItemStrings(msg, id, list) {
		for (var i = 0; i < list.count; i++) sendItemMessage(id, msg, 0, list[i]);
	}
	function selectItem(msg, id, value) {
		sendItemMessage(id, msg, +value, 0) if (msg !== void);
	}

	function setTrackBarPos(id, pos) { return sendItemMessage(id, TBM_SETPOS, true, pos); }
	function getTrackBarPos(id)      { return sendItemMessage(id, TBM_GETPOS, 0, 0); }


	var parent;

	// リソース読み込み上書き
	function loadResource(dll, res) {
		if (dll != "")  with (Storages) {
			var path = .getPlacedPath(dll);
			if (path == "") throw new Exception(@"${dll} が見つかりません");
			dll = .getLocalName(path);
		} else dll = void;
		//dm("loadResource", dll, res);
		super.loadResource(dll, res);
	}

	// オープン処理上書き
	function open(win) {
		parent = win;
		var r = super.open(...);
		//結果辞書を返す
		return %[ result:r, items:itemResults ];
	}

	// クローズ処理上書き
	function close() {
		if (!modeless) makeResults();
		var r = super.close(...);
		removeAllBitmap();
		return r;
	}
	// 結果を保持する
	function makeResults() {
		itemResults = %[];
		for (var i = 0; i < itemNames.count; i++) {
			var name = itemNames[i];
			itemResults[name] = getResult(name);
		}
	}

	// イベントフック
	function onInit(*) {
		super.onInit(...);
		initItems();
		throwEvent("onInit", *);
		return true;
	}
	function onCommand(*) {
		super.onCommand(...);
		throwEvent("onCommand", *);
		return defaultCommand(...);
	}
	function onSize(*)    { super.onSize(   ...); return throwEvent("onSize",    *); }
	function onHScroll(*) { super.onHScroll(...); return throwEvent("onHScroll", *); }
	function onVScroll(*) { super.onVScroll(...); return throwEvent("onVScroll", *); }
	function onNotify( *) { super.onNotify( ...); return throwEvent("onNotify",  *); }
	function throwEvent(tag, *) {
		if (typeof owner == "Object" && typeof owner[tag] != "undefined") return owner[tag](*);
		return false;
	}

	// 指定ウィンドウの中央に配置（ただし画面外にならないこと）
	function setCenterPosition(win = parent) {
		// モニタ情報の取得
		var x1, y1, x2, y2, monitor;
		if (typeof System.getMonitorInfo != "undefined") {
			var info = System.getMonitorInfo(true, win);
			monitor = info.work if (typeof info == "Object");
		}
		monitor = %[ x:0, y:0, w:System.screenWidth, h:System.screenHeight ] if (monitor === void);
		with (monitor) x1=.x, y1=.y, x2=.w+x1, y2=.h+y1;

		// 位置決定
		var w = width, h = height;
		var x = (x1+x2 - w)\2, y = (y1+y2 - h)\2;
		if (win && typeof win == "Object" && isvalid win &&
			(win instanceof "Window" || win instanceof "WIN32Dialog")) with (win) {
			x = ((.width  - w)\2) + .left;
			y = ((.height - h)\2) + .top;
		}
		// モニタからはみ出ていたら内側へ移動
		x = x2 - width  if (x + w > x2);
		y = y2 - height if (y + h > y2);
		x = x1          if (x < x1);
		y = y1          if (y < y1);
		setPos(x, y);
	}

	// 初期パラメータを設定する
	function initItems() {
		for (var i = 0; i < itemNames.count; i++) {
			var name = itemNames[i];
			var item = itemMap[name];
			setParams(name, item.init) if (item.init != void);
		}
	}

	// デフォルトコマンドイベント
	function defaultCommand(msg, wp, lp) {
		switch (wp) {
		case IDOK:
		case IDCANCEL:
		case IDABORT:
			close(wp);
			return true;
		}
		return false;
	}

	function setParams(id, elm) {
		if (typeof elm != "Object") return;
		if (elm instanceof "Array") {
			for (var i = 0; i < elm.count; i++) setParams(id, elm[i]);
		} else {
			var ext = [];
			ext.assign(elm);
			for (var i = 0; i < ext.count; i+=2) {
				var key = ext[i], value = ext[i+1];
				//dm("setParams", key, value);
				if (typeof this[key] == "Object" && this[key] instanceof "Function") this[key](id, value);
				else throw new Exception(@"不明な initParam: ${key}");
			}
		}
	}
	function setInitParams(elm, forced = false) {
		var ext = [];
		ext.assign(elm);
		for (var i = 0; i < ext.count; i+=2) {
			var item, key = ext[i], value = ext[i+1];
			if (key == "") continue;
			if (forced && typeof itemMap[key] == "undefined") {
				itemNames.add(key);
				itemMap[key] = %[];
			}
			var item = itemMap[key];
			if (item !== void) item.init = value;
		}
	}
	function getItemClass(itemOrId) {
		var item = itemOrId;
		item = itemMap[itemOrId] if (typeof itemOrId != "Object" && itemOrId != "");
		if (typeof item == "Object") {
			var cls = item.windowClass;
			cls = cls.toLowerCase() if (typeof cls == "String");
			switch (cls) {
			case 0x80: case "button":    return BUTTON;
			case 0x81: case "edit":      return EDIT;
			case 0x82: case "static":    return STATIC;
			case 0x83: case "listbox":   return LISTBOX;
			case 0x85: case "combobox":  return COMBOBOX;
			case 0x84: case "scrollbar": return SCROLLBAR;
			case "msctls_trackbar32":    return TRACKBAR;
			}
		}
	}
	function getResult(id) {
		var item = (id != "") ? itemMap[id] : void;
		if (item === void) return;
		var style = item.style;

		switch (getItemClass(item)) {

		case BUTTON:
			// ラジオボタンの場合はその状態
			if (style & BS_AUTOCHECKBOX ||
				style & BS_CHECKBOX ||
				style & BS_AUTORADIOBUTTON ||
				style & BS_RADIOBUTTON) return getCheckBox(id);
			// それ以外はステート
			return sendItemMessage(id, BM_GETSTATE, 0, 0);

		case EDIT:
		case STATIC:
			return getItemText(id);

		case LISTBOX:
			// 複数選択できるか
			if (style & LBS_MULTIPLESEL) {
				var cnt = sendItemMessage(id, LB_GETCOUNT, 0, 0);
				var rslt = [];
				for (var i = 0; i < cnt; i++)
					rslt.add(sendItemMessage(id, LB_GETSEL, i, 0));
				return rslt;
			}
			// それ以外は選択インデックス
			return sendItemMessage(id, LB_GETCURSEL, 0, 0);

		case COMBOBOX:
			// ドロップダウンリストでは選択インデックスを返す
			if (style & CBS_DROPDOWNLIST)
				return sendItemMessage(id, CB_GETCURSEL, 0, 0);

			// それ以外はテキスト
			return getItemText(id);

		case SCROLLBAR:
			return sendItemMessage(id, SBM_GETPOS, 0, 0);

		case TRACKBAR:
			return getTrackBarPos(id);
		}
	}

	function printOCT(oct, text) {
		text += " = ";
		for (var i = 0, len = oct.length; i < len; i++) text += "%02x".sprintf(oct[i]);
		Debug.message(text);
	}
	function getPtrOCT(oct) { return               global.WIN32Dialog.getOctetAddress(oct); }
	function getPtrSTR(str) {
		with (global.WIN32Dialog) Debug.message(.getStringFromAddress(.getStringAddress(str)));

		return getHexLSB(4,  global.WIN32Dialog.getStringAddress(str));
	}
	function getHexLSB(bc, val, ret) {
		for (var i = 0; i < bc; i++, val>>=8) ret += "%02x".sprintf(val & 0xFF);
		return ret;
	}
	function makeStructOctet(list) {
		var oct;
		for (var i = 0; i < list.count; i++) {
			var item = list[i];
			switch (typeof item) {
			case "String":  oct += getPtrSTR(item); break;
			case "Integer": oct += getHexLSB(4, item); break;
			case "Object":
				if (item instanceof "Array") {
					var keta = item[0] * 2;
					oct += (@"%0${keta}x").sprintf(item[1] & 0xFFFFFFFF);
				} else with (item) {
					switch (.type) {
					case "string": oct += getPtrSTR(.value);   break;
					case "byte":   oct += getHexLSB(1, .value); break;
					case "word":   oct += getHexLSB(2, .value); break;
					case "dword":  oct += getHexLSB(4, .value); break;
					default:
						throw new Exception("unknwon type:" + .type);
					}
				}
				break;
			}
		}
		var exp = "<% "+oct+" %>";
		return exp!;
	}
	function listViewClearAllItems(id) {
		sendItemMessage(id, LVM_DELETEALLITEMS, 0, 0);
	}
	function listViewSetColumns(id, list) {
		sendItemMessage(id, LVM_DELETECOLUMN, 0, 0);
		for  (var i = 0; i < list.count; i++) {
			var info = list[i];
			if (info === void) continue;
			info = %[ text:info, width:100 ]; if (typeof info == "String");
			with (info) {
				var mask = 0x0004; // LVCF_TEXT
				var text = .text != "" ? .text : "";
				var fmt = 0, width = 0;
				if (.align != "") {
					switch(.align) {
					case "right":  fmt |= 0x0001; break; // LVCFMT_RIGHT
					case "center": fmt |= 0x0002; break; // LVCFMT_CENTER
					}
					mask |= 0x0001; // LVCF_FMT
				}
				if (.width !== void) {
					width = (int).width;
					mask |= 0x0002; // LVCF_WIDTH
				}
				var col = makeStructOctet([ mask, fmt, width, getPtrSTR(text), /*cchTextMax*/0, /*iSubItem*/0, /*iImage*/0, /*iOrder*/0 ]);
				printOCT(col);
				Debug.message(sendItemMessage(id, LVM_INSERTCOLUMNW, i, getPtrOCT(col)));
			}
		}
	}

	function Control(text, id, wndcls, style, x, y, width, height, exStyle = 0) {
		return %[ windowClass:wndcls, x:x, y:y, cx:width, cy:height, title:text, style:style|WS_VISIBLE, exStyle:exStyle, id:id ];
	}

	// 各種テンプレートを生成する関数

	// ボタン系列
	function DefPushButton(  text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_DEFPUSHBUTTON,   x, y, w, h, ex); }
	function    PushButton(  text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_PUSHBUTTON,      x, y, w, h, ex); }
	function AutoCheckBox(   text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_AUTOCHECKBOX,    x, y, w, h, ex); }
	function     CheckBox(   text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_CHECKBOX,        x, y, w, h, ex); }
	function AutoRadioButton(text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_AUTORADIOBUTTON, x, y, w, h, ex); }
	function     RadioButton(text, id,    x, y, w, h, style=WS_TABSTOP, ex=0) { return Control(text, id, BUTTON, style|BS_RADIOBUTTON,     x, y, w, h, ex); }
	function GroupBox(       text, id=-1, x, y, w, h, style=WS_GROUP,   ex=0) { return Control(text, id, BUTTON, style|BS_GROUPBOX,        x, y, w, h, ex); }

	// スタティック系列
	function LText(          text, id,    x, y, w, h, style=WS_GROUP,   ex=0) { return Control(text, id, STATIC, style|SS_LEFT,            x, y, w, h, ex); }
	function CText(          text, id,    x, y, w, h, style=WS_GROUP,   ex=0) { return Control(text, id, STATIC, style|SS_CENTER,          x, y, w, h, ex); }
	function RText(          text, id,    x, y, w, h, style=WS_GROUP,   ex=0) { return Control(text, id, STATIC, style|SS_RIGHT,           x, y, w, h, ex); }
	function Icon(           text, id,    x, y, w, h, style=0,          ex=0) { return Control(text, id, STATIC, style|SS_ICON,            x, y, 0, 0, ex); } // w,h は無視される

	var DefaultStyles = %[
	EditText: ES_LEFT     |WS_BORDER           |WS_TABSTOP,
	ListBox:  LBS_NOTIFY  |WS_BORDER|WS_VSCROLL|WS_TABSTOP,
	ComboBox: CBS_DROPDOWN          |WS_VSCROLL|WS_TABSTOP,
	ListView: LVS_REPORT  |WS_BORDER,
	TrackBar: TBS_HORZ    |TBS_AUTOTICKS       |WS_TABSTOP,
		];

	// エディット系列 EDITTEXT, BEDIT, HEDIT, or IEDIT. 
	function EditText(             id,    x, y, w, h, style=DefaultStyles.EditText, ex=0) { return Control(, id, EDIT,      style, x, y, w, h, ex); } // ES_*,  WS_TABSTOP, WS_GROUP, WS_VSCROLL, WS_HSCROLL, WS_DISABLED
	// リストボックス系列
	function ListBox(              id,    x, y, w, h, style=DefaultStyles.ListBox,  ex=0) { return Control(, id, LISTBOX,   style, x, y, w, h, ex); } // LBS_*, WS_BORDER, WS_VSCROLL
	// コンボボックス系列
	function ComboBox(             id,    x, y, w, h, style=DefaultStyles.ComboBox, ex=0) { return Control(, id, COMBOBOX,  style, x, y, w, h, ex); } // CBS_*, WS_TABSTOP, WS_GROUP, WS_VSCROLL, WS_DISABLED

	// リストビュー（要initCommonControlsEx(ICC_LISTVIEW_CLASSES);）
	function ListView(             id,    x, y, w, h, style=DefaultStyles.ListView, ex=0) { return Control(, id, LISTVIEW,  style, x, y, w, h, ex); } // LVS_*, WS_TABSTOP, WS_GROUP, WS_VSCROLL

	// スクロールバー系列
	function TrackBar(             id,    x, y, w, h, style=DefaultStyles.TrackBar, ex=0) { return Control(, id, TRACKBAR,  style, x, y, w, h, ex); } // TBS_*, ...
}

// もう少し使いやすくしたバージョン
class WIN32GenericDialogEX extends WIN32DialogEX
{
	var _templ, _padding, _ptSz, _cx, _cury, _curh, _maxy, _curband, _id2text;
	function WIN32GenericDialogEX(elm) {
		super.WIN32DialogEX();
		elm = %[] if (typeof elm != "Object");
		with (elm) {
			_padding = .padding !== void ? (int).padding : 4;

			var style = .style !== void ? .style : DS_MODALFRAME|DS_CENTER|WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_SETFONT;
			/**/style = style! if (typeof style == "String");

			var fw = .fontWeight != "" ? .fontWeight : FW_NORMAL;
			/**/fw = fw! if (typeof fw == "String");

			var cx = _cx = .width !== void ? +.width : 100;
			var pt = _ptSz = .fontSize !== void ? (int).fontSize : 9;
			_templ = %[
				/**/style:style, x:0, y:0, cx:cx, cy:0,
				/**/title:.title != "" ? .title : "GenericDialog",
				/**/pointSize:pt, typeFace:.fontFace != "" ? .fontFace : "ＭＳ Ｐゴシック",
				/**/weight:fw, items:[]
				];
			_id2text = .itemtexts !== void ? .itemtexts : %[];
		}
		_curband = %[ x:_padding, w:_cx - _padding*2 ];
		_maxy = _cury = _padding;
		_curh = 0;
	}
	function finalize() {
		super.finalize(...);
	}
	function throwError() {
		throw new Exception(...);
	}

	function addItem(item) { _templ.items.add(item); }
	function getLastItem() { with (_templ) return .items[.items.count-1]; }
	function nextLine(ofs = _padding) {
		_cury += _curh + ofs;
		_curh = 0;
		_maxy = _cury if (_maxy < _cury);
	}

	function divRectPos(rect, pos, align = 1) {
		with (rect) {
			var ofs = (!align ? .w\2 : align > 0 ? 0 : .w) + pos;
			return %[
			left:  %[ x:.x,     y:.y, w:ofs,    h:.h ],
			right: %[ x:.x+ofs, y:.y, w:.w-ofs, h:.h ] ];
		}
	}
	function divRectPer(rect, div, pad = _padding) {
		if (div <= 1) return rect;
		var ret = [], w = (rect.w - (pad * (div-1))) / div, step = w + pad;
		for (var i = 0; i < div; i++) {
			with (rect) ret[i] = %[ x:.x + (int)(step*i), w:(int)w, y:.y, h:.h ];
		}
		return ret;
	}
	function makeDiv(pos, per, pad) {
		if (per < 1 || pos < 0 || pos >= per) throwError("makeDiv: invalid divisions");
		return %[ type:"per", per:per, pos:pos, padding:pad ];
	}
	function makeSpan(pos, span, per, pad) {
		if (pos === void) throwError("makeSpan: invalid pos");
		var r = makeDiv(pos, per, pad);
		if (span <= 0 || pos+span > per) throwError("makeSpan: invalid span");
		if (span == 1) return r;
		r.type = "span";
		r.span = span;
		return r;
	}
	function makeStep(align,  index, step, width, pad) {
		return %[ type:"step", align:align, step:step, index:index, width:width, padding:pad ];
	}
	function makeStepLeft (*) { return makeStep("left",  *); }
	function makeStepRight(*) { return makeStep("right", *); }

	function makeCut(sel, pos, align) {
		return %[ type:sel, align:align, sel => pos ];
	}
	function makeCutLeft  (*) { return makeCut ("left",  *); }
	function makeCutRight (*) { return makeCut ("right", *); }

	function getCurrentRect(div, height) {
		_curh = height if (_curh < height);
		var rect = %[ x:_curband.x, y:_cury, w:_curband.w, h:height ];
		if (typeof div != "Object" || !div) return rect;
		switch (div.type) {
		case "per":
			var per = divRectPer(rect, div.per, div.padding);
			rect = per[div.pos] if (div.pos !== void && per[(int)div.pos] !== void);
			break;
		case "span":
			var per = divRectPer(rect, div.per, div.padding), x2;
			rect = per[div.pos];
			with ( per[div.pos + div.span-1]) x2 = .x+.w;
			with ( rect) .w = x2-.x;
			break;
		case "left":  rect = divRectPos(rect, div.left,  div.align).left;  break;
		case "right": rect = divRectPos(rect, div.right, div.align).right; break;
		case "step":
			var pad = div.padding !== void ? div.padding : _padding;
			switch (div.align) {
			case "right": rect.x = rect.w - (div.step + pad) * (div.index+1) + pad*2; break;
			default: case "left": rect.x += (div.step + pad) * div.index; break;
			}
			rect.w = (div.width !== void) ? div.width : div.step;
			break;
		case "fixed":
			rect.x = div.x;
			rect.w = div.width;
			break;
		}
		return rect;
	}

	function getNameInfo(name, initMethod, initValue) {
		var ret;
		switch (typeof name) {
		case "Integer": ret = %[ id:name, text:"ID"+name ]; break;
		case "String":  ret = %[ id:name, text:name ]; break;
		case "Object":  ret = name; break;
		default: throwError("getNameInfo: unkonw name type "+name);
		}
		with (ret) {
			if (.id == "" || (.text == "" && !.nolabel)) throwError("getNameInfo: invalid name value");
			if (_id2text[.id] != "") .text = _id2text[.id];
			addInit(.id, initMethod, initValue) if (initMethod != "" && initValue !== void);
		}
		return ret;
	}

	var _inits = %[];
	function addInit(id, method, value) {
		if (id == "" || method == "") throwError("addInit: invalid ID/method");
		_inits[id] = [] if (_inits[id] === void);
		_inits[id].add(%[ method => value ]);
	}

	function _addText(sel, text, div, height = _ptSz, id = -1)  {
		var rect = getCurrentRect(div, height);
		with (rect) addItem(sel(text, id, .x, .y, .w, .h));
	}
	function addLText(*) /*text, div, height=_ptSz, id=-1*/ { _addText(LText, *); }
	function addRText(*) /*text, div, height=_ptSz, id=-1*/ { _addText(RText, *); }
	function addCText(*) /*text, div, height=_ptSz, id=-1*/ { _addText(CText, *); }

	function addIcon(id, height, div, style = SS_BITMAP|SS_CENTERIMAGE|SS_SUNKEN)  {
		var rect = getCurrentRect(div, height);
		with (rect) addItem(Control("", id, STATIC, style, .x, .y, .w, .h, 0));
	}

	function _addLineEditDrop(method, info, div, style) {
		var ofs  = _ptSz <= 9 ? 2 : 0;
		var p    = 2; //_ptSz > 9 ? 2 : 2;
		var rect = getCurrentRect(div, _ptSz + ofs*2);
		if (info.nolabel) {
			with (rect) addItem(this[method](info.id, .x, .y, .w, .h, style|WS_TABSTOP));
		} else {
			var div  = divRectPos(rect, info.text.length * _ptSz);
			with (div.left)  addItem(LText(info.text, -1, .x, .y+p, .w, .h-p));
			with (div.right) addItem(this[method](info.id,    .x, .y, .w, .h, style|WS_TABSTOP));
		}
	}
	function addLineInput(name, div, initial)  {
		var info = getNameInfo(name, "setItemText", initial);
		_addLineEditDrop("EditText", info, div, ES_LEFT|ES_AUTOHSCROLL|WS_BORDER);
	}
	function addDropSelect(name, height, div, initial, list)  {
		var info = getNameInfo(name, "setComboBoxTexts", list);
		if (typeof initial == "String") initial = list.find(initial);
		addInit(info.id, "selectComboBox", initial);
		_addLineEditDrop("ComboBox", info, div, CBS_DROPDOWNLIST|WS_VSCROLL);
		getLastItem().cy = height;
	}
	function addDropInput(name, height, div, initial, list)  {
		var info = getNameInfo(name, "setComboBoxTexts", list);
		if (typeof initial == "Integer") initial = list[i];
		addInit(info.id, "setItemText", initial);
		_addLineEditDrop("ComboBox", info, div);
		getLastItem().cy = height;
	}
	function addListSelect(name, height, div, initial, list) {
		var info = getNameInfo(name, "setListBoxTexts", list);
		if (typeof initial == "String") initial = list.find(initial);
		var   rect = getCurrentRect(div, height);
		with (rect) addItem(ListBox(info.id, .x, .y, .w, .h));
	}
/*
	function addFileSelect  (name, div, initial) {}
	function addFolderSelect(name, div, initial) {}
	function addListInput (name, div, initial, list) {}
*/
	function addListView (name, height, div, columns) {
		var info = getNameInfo(name, "listViewSetColumns", columns);
		var rect = getCurrentRect(div, height);
		with (rect) addItem(ListView(info.id, .x, .y, .w, .h,, LVS_EX_GRIDLINES));
	}

	function addTextInput(name, height, div, initial) {
		var rect, info = getNameInfo(name, "setItemText", initial);
		if (!info.nolabel) {
			/**/  rect = getCurrentRect(div, _ptSz);
			with (rect) addItem(LText(info.text, -1, .x, .y, .w, .h));
			nextLine(0);
		}
		/**/  rect = getCurrentRect(div, height);
		with (rect) addItem(EditText(info.id, .x, .y, .w, .h,
									 ES_LEFT|ES_MULTILINE|ES_WANTRETURN|
									 ES_AUTOVSCROLL|ES_AUTOHSCROLL|
									 WS_VSCROLL|WS_HSCROLL|
									 WS_BORDER|WS_TABSTOP));
	}
	function _addAnyTypeButton(type, pad, name, div, pad2) {
		var info = getNameInfo(name);
		var rect = getCurrentRect(div, _ptSz + (pad2 !== void ? pad2 : pad));
		with (rect) addItem(this[type](info.text, info.id, .x, .y, .w, .h));
	}
	function addDefPush(*) { _addAnyTypeButton("DefPushButton", _padding, *); }
	function addButton(*)  { _addAnyTypeButton("PushButton", _padding, *); }
	function addToggle(*)  { _addAnyTypeButton("AutoCheckBox", 0, *); }
	function addRadio (*)  { _addAnyTypeButton("AutoRadioButton", 0, *); }

	function addTrackBar(name, div, min=0, max=100, pos=0, step, height = _ptSz * 2) {
		var info = getNameInfo(name);
		var rect = getCurrentRect(div, height);
		with (rect) addItem(TrackBar(info.id, .x, .y, .w, .h));
		if (min > max) throwError("addTrackBar:invalid min/max.");
		if (pos < min) pos = min;
		if (pos > max) pos = max;
		if (step === void) step = (max-min)\4;
		addInit(info.id, "initTrackBarParam", [min, max, pos, step]);
	}
	function initTrackBarParam(id, list) {
		var min = (int)+list[0];
		var max = (int)+list[1];
		var pos = (int)+list[2];
		var step =      list[3];
		dm("initTrackBarParam", min, max, pos, step);
		sendItemMessage(id, TBM_SETRANGEMIN, 0, min);
		sendItemMessage(id, TBM_SETRANGEMAX, 0, max);
		sendItemMessage(id, TBM_SETTICFREQ,  (int)+step, 0) if (step !== void);
		setTrackBarPos(id, pos);
	}

	var _groupStack = [];
	function beginGroup(grp, id=-1, nogrp=false) {
		nextLine(0);
		var rect = getCurrentRect(void, _ptSz), item;
		with (rect) item = GroupBox(grp, id, .x, .y, .w, .h, nogrp ? 0 : void);
		var stack = %[ band:%[], y:_cury, item:item ];
		(Dictionary.assign incontextof stack.band)(_curband, true);
		_groupStack.push(stack);
		addItem(item);
		_curband.x += _padding*2;
		_curband.w -= _padding*4;
		nextLine(0);
	}
	function endGroup() {
		nextLine(0);
		var stack = _groupStack.pop();
		if (stack === void) return;
		(Dictionary.assign incontextof _curband)(stack.band, true);
		stack.item.cy = _cury - stack.y + _padding\2;
		nextLine();
	}
	var _frameStack = [];
	function beginFrame(div) {
		nextLine(0);
		var band = %[];
		(Dictionary.assign incontextof band)(_curband, true);
		_frameStack.unshift(%[ band:band, y1:_cury, y2:_cury ]);
		setFrame(div) if (div !== void);
	}
	function _resetFrame(pop) {
		var info = pop ? _frameStack.shift() : _frameStack[0];
		(Dictionary.assign incontextof _curband)(info.band, true);
		_curh = 0;
		return info;
	}
	function setFrame(div) {
		nextLine(0);
		var info = _resetFrame(false);
		info.y2 = _cury if (info.y2 < _cury);
		_cury = info.y1;
		var rect = getCurrentRect(div, 0);
		_curband.x = rect.x;
		_curband.w = rect.w;
	}
	function endFrame() {
		nextLine(0);
		var info = _resetFrame(true);
		_cury = info.y2 if (info.y2 > _cury);
	}

	var _heightStack = [];
	function pushCurrentHeight() { _heightStack.push(_curh);  }
	function popCurrentHeight() { _curh = _heightStack.pop(); }

	var _stored;
	function open() {
		if (!_stored) {
			nextLine();
			_templ.cy = _maxy;
			_stored = true;
			store(_templ);
			setInitParams(_inits);
		}
		return super.open(...);
	}
}
)KRKRZ";

const char *k2compat_modeless_tjs = R"KRKRZ(Krkr2CompatUtils.requireWIN32Dialog();

//=============================================================

class        WIN32ModelessDialogEX      extends WIN32DialogEX {
	function WIN32ModelessDialogEX(elm) { super.WIN32DialogEX(...);
		modeless = true;
		(Dictionary.assignStruct incontextof (_initialParam = %[]))(elm) if (typeof elm == "Object" && elm);
		createAsyncTrigger("onAsyncUpdate");
	}
	function finalize {
		deleteAsyncTrigger();
		super.finalize(...);
	}

	// visibleでmodeless dialogをopen/closeする
	var _visible = false, _stored = false, _initialParam;
	property visible {
		getter { return _visible; }
		setter(v) {
			if (_visible == !!v) return;
			if (_visible) {
				onClose();
				_visible = false;
				super.close(-1);
			} else {
				var first = false;
				if(!_stored) {
					_stored = first = true;
					store(getDialogTemplate(_initialParam));
				}
				_visible = true;
				open(Window.mainWindow ? null : void); // ※ZではMainWindowがないとTVPGetApplicationWindowが失敗する
				onOpen(first);
			}
		}
	}
	function close() { visible = false; }
	/*virtusl*/function onOpen(first) { _syncPos(true); }
	/*virtusl*/function onClose()     { _resetPos(); }

	/*virtual*/function onResized(w, h) {}
	/*virtual*/function getDialogTemplate(elmov) {
		var cx, cy;
		if (_width !== void || _height !== void) {
			// フォントサイズに影響されるので実サイズとは異なるが表示時にリサイズすることで対応する
			var units = getBaseUnits();
			cx = _width  * 4 \ units.h if (_width  !== void);
			cy = _height * 8 \ units.v if (_height !== void);
		}
		var style = getWindowStyle();
		var dlgsty = getDialogStyle();
		return %[ items:[], style: style[0] | dlgsty, exStyle: style[1],
				  /**/title: _title, x:_left, y:_top, cx:cx, cy:cy ];
	}
	/*virtual*/function getDialogStyle() { return DS_SHELLFONT/*DS_SETFONT*/ | WS_VISIBLE; }

	// borderStyleからSTYLE/EXSTYLEを決定する
	function getWindowStyle(style = WS_OVERLAPPEDWINDOW, exStyle = 0) {
		var origStyle   = style;
		var origExStyle = exStyle;
		/**/style   &= ~(WS_POPUP | WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_DLGFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
		/**/exStyle &= ~(WS_EX_TOOLWINDOW | /*WS_EX_ACCEPTFILES |*/WS_EX_APPWINDOW);
		switch (_borderStyle) {
		case bsDialog:
			style   |= WS_DLGFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
			exStyle |= WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;
			break;
		case bsSingle:
			style   |= WS_CAPTION | WS_BORDER;
			style   |= WS_MINIMIZEBOX | WS_SYSMENU;
			break;
		case bsNone:
		case bsSizeable:
			style   |= (_borderStyle == bsNone) ? WS_POPUP : (WS_CAPTION | WS_THICKFRAME);
			style   |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
			break;
		case bsToolWindow:
		case bsSizeToolWin:
			style   |= WS_CAPTION;
			style   |= (_borderStyle == bsToolWindow) ? WS_BORDER : WS_THICKFRAME;
			style   |= WS_SYSMENU;
			exStyle |= WS_EX_TOOLWINDOW;
			break;
		default:
			// revert
			style   = origStyle;
			exStyle = origExStyle;
			break;
		}
		return [ style, exStyle ];
	}

	// ダイアログアイテムのコールバックを onCommand/onNotify_アイテム名 で呼び返す
	function onCommand(msg, wp, lp) {
		super.onCommand(...);
		_invokeCallback("onCommand", getNamedId(wp & 0xFFFF), wp>>16, lp);
	}
	function onNotify(wp, nm) {
		super.onNotify(...);
		var id = getNamedId(wp);
		if (id == "") id = getNamedId(nm.idFrom);
		_invokeCallback("onNotify", id, nm);
	}
	function _invokeCallback(cbname, id, *) {
		if (id != "") {
			var method = @"${cbname}_${id}";
			if (typeof this[method] == "Object") try {
				this[method](id, *);
			} catch (e) {
				Debug.notice(@"${cbname}Exception", id, e.message);
			}
		}
	}

	// 複数個のAsyncTriggerを汎用に生成する処理（callback method名をキーに保存）
	var _asyncTriggers;
	function createAsyncTrigger(cbmethod, mode = atmAtIdle, cached = true) {
		if (cbmethod == "") return;
		var trig = new AsyncTrigger(this, cbmethod);
		with (trig) .mode=mode, .cached=cached;
		if (_asyncTriggers === void) 
			_asyncTriggers = %[];
		var old = trig;
		_asyncTriggers[cbmethod] <-> old;
		if (old) invalidate old;
		return trig;
	}
	function deleteAsyncTrigger(cbmethod) {
		if (_asyncTriggers === void) return;
		if (cbmethod != "") {
			var target = _asyncTriggers[cbmethod];
			delete       _asyncTriggers[cbmethod];
			if (target) invalidate target;
		} else {
			var ext = [];
			ext.assign(_asyncTriggers);
			for (var i = 0, cnt = ext.count; i < cnt; i+=2) {
				var target = ext[i+1];
				if (target) invalidate target;
			}
			(Dictionary.clear incontextof _asyncTriggers)();
			_asyncTriggers = void;
		}
	}
	function kickAsyncTrigger(cbmethod, autogen = true, *) {
		if (cbmethod == "" || _asyncTriggers === void) return;
		var trig = _asyncTriggers[cbmethod];
		if (trig === void) {
			if (!autogen) return;
			trig = createAsyncTrigger(cbmethod, *);
		}
		trig.trigger();
		return trig;
	}

	//-------------------------------------------------------------

	var _title = System.title;
	property title {
		getter { return _title; }
		setter(v) {     _title = v; _updateTitle(); }
	}
	function _updateTitle() {
		if (_visible) setItemText(0, _title);
	}
	var _borderStyle = bsSingle;
	property borderStyle {
		getter { return _borderStyle; }
		setter(v) {     _borderStyle = v; _updateBorderStyle(); }
	}
	function _updateBorderStyle {
		if (_visible) {
			var   style = getItemLong(0, GWL_STYLE);
			var exstyle = getItemLong(0, GWL_EXSTYLE);
			var gws = getWindowStyle(style, exstyle);
			setItemLong(0, GWL_STYLE,   gws[0]) if (gws[0] !=   style);
			setItemLong(0, GWL_EXSTYLE, gws[1]) if (gws[1] != exstyle);
			invalidateAll(true);
		}
	}

	//-------------------------------------------------------------

	function onSize(msg, wp, lp) {
		var w = (lp&0xFFFF), h = (lp>>16)&0xFFFF;
		onResized(w, h) if (_visible && (wp == 0/*SIZE_RESTORED*/ || wp == 2/*SIZE_MAXIMIZED*/));
	}

	var _left, _top, _width, _height;
	function _syncPos(force) {
		if (!_visible) return;
		if (_left   === void) _left   = super.left;
		if (_top    === void) _top    = super.top;
		if (_width  === void) _width  = super.width;
		if (_height === void) _height = super.height;
		if (force) onAsyncUpdate();
	}
	function _resetPos() {
		if (_visible) {
			_left = _top = _width = _height = void;
			_syncPos(false);
		}
	}
	function _updatePos()  { _asyncUpdate(); }
	function _updateSize() { _asyncUpdate(); }
	function _asyncUpdate() { _syncedPosSize = false; kickAsyncTrigger("onAsyncUpdate"); }
	function onAsyncUpdate() {
		if (isvalid this && _visible) {
			super.setPos( _left,  _top)    if (_left  != super.left  || _top    != super.top);
			super.setSize(_width, _height) if (_width != super.width || _height != super.height);
			_syncedPosSize = true;
		}
	}
	var _syncedPosSize = true;
	property left {
		getter { return _visible && _syncedPosSize ? super.left : _left; }
		setter(v) { _left = (int)+v; _updatePos(); }
	}
	property top {
		getter { return _visible && _syncedPosSize ? super.top :_top; }
		setter(v) { _top  = (int)+v; _updatePos(); }
	}
	property width {
		getter { return _visible && _syncedPosSize ? super.width : _width; }
		setter(v) { _width  = (int)+v; _updateSize(); }
	}
	property height {
		getter { return _visible && _syncedPosSize ? super.height : _height; }
		setter(v) { _height = (int)+v; _updateSize(); }
	}
	function setPos(x, y, w, h) {
		_left = (int)+x if (x !== void);
		_top  = (int)+y if (y !== void);
		_updatePos();
		setSize(w, h) if (w !== void || h !== void);
	}
	function setSize(w, h) {
		_width  = (int)+w if (w !== void);
		_height = (int)+h if (h !== void);
		_updateSize();
	}
}

)KRKRZ";

const char *k2compat_padcommon_tjs = R"KRKRZ(Krkr2CompatUtils.require("modeless");

//=============================================================

// テキスト編集（エディットコントロール）のあるモードレスダイアログ汎用
class        TextContentModelessDialog extends WIN32ModelessDialogEX {
	function TextContentModelessDialog { super.WIN32ModelessDialogEX(...); }
	function finalize {
		invalidate _brush if (_brush);
		super.finalize(...);
	}

	function getDialogTemplate(elmov) {
		var r = super.getDialogTemplate(...);
		(Dictionary.assign incontextof r)(elmov, false) if (elmov);
		if (r) with (r) {
			.pointSize = _fontSize;
			.typeFace  = _fontFace;
			.weight    = _fontBold ? FW_BOLD : FW_NORMAL;

			.items = getItemTemplate(r);
		}
		return r;
	}
	function getDialogStyle() { return DS_SETFONT | WS_VISIBLE; }

	var _editID;
	function onInit() {
		_editID = getNumberdId("edit");
		_updateText();
		return super.onInit(...);
	}
	function onClose() {
		_syncText(true);
		return super.onClose(...);
	}

	var _bottomContentSize;
	function getItemTemplate(elm) {
		var w = elm.cx, h = elm.cy;
		var bh = _bottomContentSize = getBottomContentSize();

		var r = [], rect = %[ x:0, y:0, w:w, h:h-bh ];
		setMainContent(elm, r.push, rect);
		if (bh > 0) {
			rect.y = rect.h;
			rect.h = bh;
			setBottomContent(elm, r.push, rect);
		}
		return r;
	}
	function setMainContent(elm, add, rect) {
		with (rect) add(EditText("edit", .x, .y, .w, .h, getEditStyle(), WS_EX_CLIENTEDGE));
		if (_bottomContentSize > 0) _addAutoMapRect("edit", 0, 0, 0, _bottomContentSize);
	}

	// 最下行に何か表示させる場合の拡張用
	/*virtual*/function getBottomContentSize() { return 0; } //fontSize + 4, etc.
	/*virtual*/function setBottomContent(elm, add, rect) {}
	/*virtual*/function  onBottomContentResized(w, h, bh) {}

	// 現在の状態からエディットコントロールのSTYLEを決定
	function getEditStyle(style = ES_LEFT|ES_MULTILINE|ES_WANTRETURN|ES_NOHIDESEL|WS_BORDER|WS_TABSTOP) {
		style = getWordWrapStyle (style);
		style = getScrollBarStyle(style);
		if (_readOnly) style |=  ES_READONLY;
		else           style &= ~ES_READONLY;
		return style;
	}
	function getScrollBarStyle(style = 0) {
		style &= ~(WS_VSCROLL|WS_HSCROLL);
		switch (_showScrollBars) {
		case ssNone: break;
		case ssHorizontal: style |= WS_HSCROLL; break;
		case ssVertical:   style |= WS_VSCROLL; break;
		default:
		case ssBoth:       style |= WS_VSCROLL|WS_HSCROLL; break;
		}
		return style;
	}
	function getWordWrapStyle(style = 0) {
		if (!_wordWrap) style |=  ES_AUTOHSCROLL;
		else            style &= ~ES_AUTOHSCROLL;
		return          style |   ES_AUTOVSCROLL; // always on
	}

	//-------------------------------------------------------------

	function _updateFont() {
		if (_visible) unsupport("ondemand font change.");
	}
	function resetEditStyle(cb) {
		var now = getItemLong(_editID, GWL_STYLE);
		var set = cb(now);
		setItemLong(_editID, GWL_STYLE, set) if (set != now);
	}
	function _updateScrollBars() {
		if (_visible) resetEditStyle(getScrollBarStyle);
	}
	function _updateFontColor() {
		if (_visible) invalidateAll(true);
	}
	function _updateReadOnly() {
		if (_visible) sendItemMessage(_editID, EM_SETREADONLY, _readOnly, 0);
	}
	function _updateWordWrap() {
		if (_visible) unsupport("ondemand word wrap change.");
	}
	function _updateText() {
		if (_visible) {
			var cvtext = _convertTextToDialog(_text);
			setItemText(_editID, cvtext);
		}
	}
	var _modified;
	function _syncText(force) {
		if (_visible && (_modified || force)) {
			var cvtext = getItemText(_editID);
			_text = _convertDialogToText(cvtext);
			//Debug.message("syncText", cvtext, _text);
			_modified = false;
		}
	}

	//-------------------------------------------------------------

	var _autoMapRects, _autoMapRectTargets = [];
	function _addAutoMapRect(name, left, right, top, bottom) {
		if (name != "") _autoMapRectTargets.add(%[
			/**/name:name, left:(int)+left, right:(int)+right, top:(int)+top, bottom:(int)+bottom
			]);
	}
	function _makeAutoMapRectList(list) {
		try {
			var map = %[];
			for (var i = list.count-1; i >= 0; i--) {
				var target = list[i], name;
				if (target && (name = target.name) != "") {
					var rect = mapRect(target);
					if(!rect) with (target) {
						// fail safe
						var cv = _convertPointToPixel;
						rect = %[
							/**/left:  cv(.left),
							/**/right: cv(.right),
							/**/top:   cv(.top),
							/**/bottom:cv(.bottom),
							];
					}
					map[name] = rect;
				}
			}
			return map;
		} catch (e) {
			Debug.notice(e.message);
		}
		return null;
	}

	/*virtual*/function onResized(w, h) {
		if (_autoMapRects === void)
			_autoMapRects = _makeAutoMapRectList(_autoMapRectTargets);
		if (_autoMapRects) with (_autoMapRects) try {
			var bh = .edit ? .edit.bottom : 0;
			setItemSize("edit", w, h-bh);
			onBottomContentResized(w, h, _autoMapRects, bh);
			invalidateAll(false);
		} catch (e) {
			Debug.notice(e.message);
		}
	}

	function onCommand_edit(id, msg, lp) {
		if (msg == EN_CHANGE) _modified = true;
	}

	// テキスト／背景色変更に関する細工
	var _brush, _brushColor;
	function onCtrlColorEdit   { return onCtrlColor(...); }
	function onCtrlColorStatic { return onCtrlColor(...); }
	function onCtrlColor(id) {
		if (id == _editID) {
			if (_brush !== void && _brushColor !== _color) {
				// 古いブラシを破棄
				invalidate _brush if (typeof _brush == "Object");
				_brush = void;
			}
			if (_brush === void) {
				// 背景色ブラシを作成(白,黒はStockObject使用)
				_brush = !_color ? BLACK_BRUSH :
				/**/     (_color == 0xFFFFFF) ? WHITE_BRUSH :
				/**/     new global.WIN32Dialog.SolidBrush(_color);
			}
			return %[ fgcolor:_fontColor, bgcolor:_color, bgbrush:_brush ];
		}
	}

	//-------------------------------------------------------------

	// default props.
	var _fontFace = "ＭＳ ゴシック";
	var _fontSize = 9;
	var _fontBold = false; // ※boldのみweight値で参照される
	var _fontItalic = false; // unsupported
	var _fontUnderline = false; // unsupported
	var _fontStrikeOut = false; // unsupported
	var _fontColor = 0xFFFFFF;
	var _color = 0x000080;
	var _text = "";
	var _readOnly = false;
	var _wordWrap = false;
	var _showScrollBars = ssBoth;

	// Pad compatible font properties.
	property fontFace      { getter { return _fontFace;      } setter(v) { _fontFace      =   v; _updateFont(); } }
	property fontSize      { getter { return _fontSize;      } setter(v) { _fontSize  = (int)+v; _updateFont(); } }
	property fontHeight    {
		getter { return        _convertPointToPixel(_fontSize); }
		setter(v) { fontSize = _convertPixelToPoint((int)+v);   }
	}
	property fontBold      { getter { return _fontBold;      } setter(v) { _fontBold      = !!v; _updateFont(); } }

	// unsupported properties
	property fontItalic    { getter { return _fontItalic;    } setter(v) { _fontItalic    = !!v; _updateFont(); } }
	property fontStrikeOut { getter { return _fontStrikeOut; } setter(v) { _fontStrikeOut = !!v; _updateFont(); } }
	property fontUnderline { getter { return _fontUnderline; } setter(v) { _fontUnderline = !!v; _updateFont(); } }

	property  fontColor    { getter { return _fontColor;     } setter(v) { _fontColor = _normalizeColor(v); _updateFontColor(); } }
	property      color    { getter { return     _color;     } setter(v) {     _color = _normalizeColor(v); _updateFontColor(); } }

	property text {
		getter { _syncText(); return _text; }
		setter(v) { _text = v; _updateText(); }
	}

	property readOnly      { getter { return _readOnly;      } setter(v) { _readOnly = !!v;     _updateReadOnly(); } }
	property wordWrap      { getter { return _wordWrap;      } setter(v) { _wordWrap = !!v;     _updateWordWrap(); } }
	property showScrollBars{ getter { return _showScrollBars;} setter(v) { _showScrollBars = v; _updateScrollBars(); } }

	//-------------------------------------------------------------
	// utils.

	// [XXX] pixel <-> point method: force 96dpi calc.
	function _convertPointToPixel(pt, dispdpi=96, basedpi=72) { return (int)Math.round(dispdpi * pt / basedpi); }
	function _convertPixelToPoint(px, dispdpi=96, basedpi=72) { return (int)Math.round(basedpi * px / dispdpi); }

	function _convertTextToDialog(str) { return str.replace(/[\r]?\n/g, "\r\n"); }
	function _convertDialogToText(str) { return str.replace(/\r\n/g, "\n"); }

	function _normalizeColor(col) { return ((int)+col) & 0xFFFFFF; }

	function unsupport(keyword) {
		var trace = Scripts.getTraceString();
		Debug.notice(@"k2debugui.unsupported: ${keyword}" + (trace != "" ? "\n"+trace : ""));
	}
}

//=============================================================

class        DebugPadCompatDialog extends TextContentModelessDialog {
	function DebugPadCompatDialog { super.TextContentModelessDialog(...); }
	function finalize { super.finalize(...); }

	// [XXX] super class defaults
	var _width  = 640;
	var _height = 480;
	var _borderStyle = bsSizeable;
	var _fontFace = "ＭＳ 明朝";

	//-------------------------------------------------------------
	// 最下部ライン表示対応

	var _execMark = $9654; /* &#9654 (U+25B6): 右向き三角 */
	var _execEnabled = true;
	var _execName = "exec";
	var _lineName = "status";

	function onInit() {
		setItemEnabled("exec", _execEnabled);
		return super.onInit(...);
	}
	function getBottomContentSize() { return fontSize + 4; } // [XXX]4pt padding.
	function setBottomContent(elm, add, rect) {
		addExecButtonParts(...);
		addLineTextParts  (...);
	}
	function  onBottomContentResized(w, h, rects, bh) {
		resizeExecButtonParts(...);
		resizeLineTextParts  (...);
	}

	function addExecButtonParts(elm, add, rect) {
		with (rect) add(PushButton(_execMark, _execName, .x, .y, .h, .h,  BS_FLAT)); // ※BS_FLATはVisualStyle適用時は効かないっぽい
	}
	function resizeExecButtonParts(w, h, rects, bh) {
		setItemPos(_execName, 0, h-bh);
	}

	function addLineTextParts(elm, add, rect) {
		with (rect) {
			add(LText(getStatusText(), _lineName, .x+.h, .y, .w-.h, .h, SS_SUNKEN|SS_CENTERIMAGE));
			_addAutoMapRect(_lineName, 0, .h, 0, 0);
		}
	}
	function resizeLineTextParts(w, h, rects, bh) {
		if (rects[_lineName]) with (rects[_lineName]) {
			var ox = .right, sw = w-.right;
			setItemPos(_lineName, ox, h-bh);
			setItemSize(_lineName, sw <= 0 ? 1 : sw, bh);
		}
	}

	//-------------------------------------------------------------

	function getStatusText() { return _showStatusBar ? (string)_statusText : ""; }

	function _updateStatusText() {
		if (_visible) setItemText(_lineName, getStatusText());
	}

	//-------------------------------------------------------------
	// Callbacks

	function onCommand_exec(id, msg, lp) {
		if (msg == BN_CLICKED && _execEnabled) kickAsyncTrigger("onExecute");
	}
	function onExecute {
		if (!isvalid this) return;
		var exec = this.text;
		try {
			Scripts.exec(exec);
		} catch (e) {
			System.inform(e.message);
		}
	}

	//-------------------------------------------------------------
	// Pad互換プロパティ群

	var      _statusText = "";
	property  statusText {
		getter { return _statusText; }
		setter(v) {     _statusText = v; _updateStatusText(); }
	}
	var      _showStatusBar = true;
	property  showStatusBar {
		getter { return _showStatusBar; }
		setter(v) {     _showStatusBar = !!v; _updateStatusText(); }
	}

	var     _fileName;
	property fileName {
		getter { return _fileName; }
		setter(v) {     _fileName = v; unsupport("fileName prop not implemented."); }
	}

}
)KRKRZ";

const char *k2compat_pad_tjs = R"KRKRZ(Krkr2CompatUtils.require("padcommon");

class        Pad extends DebugPadCompatDialog{
	function Pad { super.DebugPadCompatDialog(); }
	function finalize { super.finalize(...); }
	var _title = "Pad";
	var _execEnabled = false;
}
class        DebugScriptEditorCompatPad extends DebugPadCompatDialog{
	function DebugScriptEditorCompatPad { super.DebugPadCompatDialog(); }
	function finalize { super.finalize(...); }
	var _title = "ScriptEditor";
}
)KRKRZ";

const char *k2compat_console_tjs = R"KRKRZ(Krkr2CompatUtils.require("padcommon");

class        DebugConsoleCompatDialog extends DebugPadCompatDialog {
	function DebugConsoleCompatDialog { super.DebugPadCompatDialog();
		try {
			Debug.addLoggingHandler(this.onLog);
			setupInitialLog(Debug.getLastLog());
		} catch(e) {
			var log = "LoggingHandler not found:"+e.message;
			unsupport(log);
			_textLogs.add(log);
			@if (!kirikiriz)
			_textLogs.add("吉里吉里２ではコンソール互換ウィンドウはサポートされません");
			@endif
		}
		initComboBoxEx();
	}
	function finalize {
		try { Debug.removeLoggingHandler(this.onLog); } catch {}
		super.finalize(...);
	}

	var _title = "Console";
	var _fontColor = 0xFFFFFF;
	var _color = 0x000000;
	var _wordWrap = true;
	var _showScrollBars = ssVertical;
	var _readOnly = true;

	var _lineName = "eval";

	var _maxTextLogs = 1024; // [XXX]
	var _textLogs = [];
	var _focusEval = false;

	function onInit() {
		setupEvalComboBox();
		var r = super.onInit(...);
		_focusEval = true;
		return r;
	}

	property text {
		getter { return _textLogs.join("\n"); }
//		setter(v) { /* text log is read only */ }
	}
	function _syncText {}
	function onCommand_edit(id, msg, lp) {}

	// LoggingHandler callback
	function onLog(line) {
		if (!isvalid this || ! this.isValid) return;

		// 行数上限対応
		_textLogs.add(line);
		while (_textLogs.count > _maxTextLogs) _textLogs.shift();

		_updateText() if (_visible);
	}

	// getLastLog を登録
	function setupInitialLog(text) {
		var div = ((string)text).split(/[\r]?\n/g);
		// 末尾空行を削除
		while (div.count > 0 && div[div.count-1] == "") div.pop();
		_textLogs.assign(div);
	}

	// ログテキストを更新（遅延対応）
	function _updateText() {
		if (_visible) kickAsyncTrigger("onUpdateLog");
	}
	function onUpdateLog {
		if (!isvalid this || ! this.isValid || !_visible) return;
		lockItemUpdate(0);
		try {
			setItemText    (_editID, _textLogs.join("\n").replace(/[\r]?\n/g, "\r\n")); // [TOOD]もっとスマートな方法があれば…
			sendItemMessage(_editID, EM_SETSEL, -2, -2);
			sendItemMessage(_editID, EM_SCROLLCARET, 0, 0);
		} catch (e) {
			unlockItemUpdate();
			throw e;
		}
		unlockItemUpdate();
		//invalidateAll(false);
		if (_focusEval) {
			_focusEval = false;
			setItemFocus(_lineName);
		}
	}
	// optional method
	function   lockItemUpdate { return typeof super.  lockItemUpdate == "Object" ? super.  lockItemUpdate(...) : void; }
	function unlockItemUpdate { return typeof super.unlockItemUpdate == "Object" ? super.unlockItemUpdate(...) : void; }


	//-------------------------------------------------------------

	function addLineTextParts(elm, add, rect) {
		with (rect) {
			add(ComboBoxEx(_lineName, .x+.h, .y, .w-.h, .h*8));
			_addAutoMapRect(_lineName, 0, .h, 0, 0);
		}
	}
	function initComboBoxEx() {
		// for use ComboBoxEx
		global.WIN32Dialog.initCommonControlsEx(ICC_USEREX_CLASSES);
		var set = function (key, value) {
			if (typeof this[key] == "undefined") this[key] = value & 0xFFFFFFFF;
		} incontextof this;
		// [XXX] CBEN_* が登録されていない場合自前で対応
		set("CBEN_ENDEDITA", -800 - 5);
		set("CBEN_ENDEDITW", -800 - 6);
//		set("CBEM_DELETEITEM",  CB_DELETESTRING);
		set("CBEM_INSERTITEMW", 0x0400 + 11);
	}
	function ComboBoxEx() {
		var r = ComboBox(...);
		r.windowClass = COMBOBOXEX; // WC_COMBOBOXEX
		return r;
	}
	// ComboBox の CBEN_ENDEDIT 通知から ENTERキー入力を取得
	function onNotify_eval(id, nm) {
		var sz = 0;
		switch (nm.code) {
		case CBEN_ENDEDITA: sz = 1; break;
		case CBEN_ENDEDITW: sz = 2; break;
		}
		if (sz > 0) {
			var sztext = /*CBEMAXSTRLEN*/260 * sz;
			var szHdr = (System.exeBits == 32) ? (4 * 3) : (8 + 4 * 2);
			var szPadding = (System.exeBits == 32) ? 0 : 4;
			var iWhy = nm.getDWord(/*NMHDR hdr*/szHdr + /*bool fChanged*/4+ szPadding +/*int iNewSelection*/4+ szPadding + sztext);
			if (iWhy == /*CBENF_RETURN*/2) {
				onExecute();
			}
		}
	}
	var _evalID;
	function setupEvalComboBox() {
		_evalID = getNumberdId(_lineName);
		/*
		setComboBoxTexts(_evalID, [
			"ああああああ",
			"bbbbbbbbbbbb",
			"cccccccccccc" ]);
		 */
	}
	function clearEvalComboBox() {
		while (sendItemMessage(_evalID, CB_DELETESTRING, 0, 0) > 0);
	}
	// ComboBoxExの場合はCB_ADDSTRINGが効かない（代わりにCBEM_INSERTITEMを使う）
	function setComboBoxTexts(id, array) {
		clearEvalComboBox();
		for (var i = 0; i < array.count; i++) {
			addComboBoxText(id, (string)array[i]);
		}
	}
	function addComboBoxText(id, str) {
		// CBEM_INSERTITEMW用構造体をBlobで作成orz
		var blob;
		if (System.exeBits == 32) {
			var sz = 4*9;
			blob = new global.WIN32Dialog.Blob(sz);
			blob.setDWord(/*mask*/0, /*CBEIF_TEXT*/1);
			blob.setDWord(/*iItem*/4, -1);
			blob.setText (/*pszText*/8, str);
			blob.setDWord(/*pszText*/12, str.length);
		} else {
			var sz = 72;
			blob = new global.WIN32Dialog.Blob(sz);
			blob.setDWord(/*mask*/0, /*CBEIF_TEXT*/1);
			blob.setDWordLong(/*iItem*/8, -1);
			blob.setText (/*pszText*/16, str);
			blob.setDWord(/*pszText*/24, str.length);
		}
		var idx = sendItemMessage(id, CBEM_INSERTITEMW, 0, blob.pointer);
		invalidate blob;
		return idx;
	}

	var _historyMaxCount = 100; // [XXX]履歴上限

	function _updateStatusText() {}
	function onExecute {
		if (!isvalid this || ! this.isValid) return;
		var _toString = Krkr2CompatUtils.toString;
		var result, eval = ((string)getItemText(_lineName)).trim();
		var store;
		if (eval != "") {
			try {
				result = (string)_toString(Scripts.eval(eval));
				store = true;
			} catch (e) {
				result = "(Exception)";
				if (typeof e == "Object" && typeof e.message == "String")
					result += e.message;
				else result += _toString(e);
			}
			var cr = result.indexOf("\n") >= 0 ? "\n" : "";
			Debug.message(@"${_title}: ${eval} = ${cr}"+result);
		}
		if (store) {
			// 履歴に追加
			var id = _evalID;
			var cnt = sendItemMessage(id, CB_GETCOUNT, 0, 0);
			if (cnt >= _historyMaxCount) {
				while (sendItemMessage(id, CB_DELETESTRING, 0, 0) >= _historyMaxCount);
			}
			var idx = addComboBoxText(id, eval);
			sendItemMessage(id, CB_SETCURSEL, idx, 0);
			setItemText(id, "");
		}
	}

}
)KRKRZ";

const char *k2compat_inputstring_tjs = R"KRKRZ(// System.inputStringの互換実装

Krkr2CompatUtils.requireWIN32Dialog();

class        _System_inputString_Dialog extends WIN32GenericDialogEX {
	function _System_inputString_Dialog(caption, prompt, initialString, width = 200) {
		var tagOK = getResourceString("ButtonOK",     "&OK");
		var tagNG = getResourceString("ButtonCancel", "キャンセル");
		super.WIN32GenericDialogEX(%[ title:caption, width:width, itemtexts:%[ IDOK=>tagOK, IDCANCEL=>tagNG ] ]);

		addLText(prompt);
		nextLine();
		addLineInput(%[ id:"Input", nolabel:true ], void, (string)initialString);
		nextLine();
		addDefPush(IDOK,    makeStepRight(1, 60), 8);
		addButton(IDCANCEL, makeStepRight(0, 60), 8);
	}
	function finalize() {
		super.finalize(...);
	}
	function getResourceString (name, def) { return def; } // [TODO] リソースなどでボタンの文言を差し替えられるようにする
	function getResourceInteger(name, def) { return def; } // [TODO] リソースなどでボタンの文言を差し替えられるようにする
	function open() {
		var r = super.open(...);
		if (r.result == IDOK) return (string)r.items.Input;
	}
}
&System.inputString = function { // caption, prompt, initialString
	var dialog = new _System_inputString_Dialog(...);
	var result = dialog.open(null);
	invalidate dialog;
	return result;
} incontextof global;

)KRKRZ";

const char *k2compat_fontselect_tjs = R"KRKRZ(Krkr2CompatUtils.requireWIN32Dialog();

class FontSelectDialog extends WIN32GenericDialogEX
{
	var fontList;						//< フォント名一覧配列
	var initialSelect;					//< 初期選択フォント（fontListインデックス番号）
	var itemHeight;						//< ListBoxの１行の高さ
	var itemPadding;					//< ListBoxの隙間
	var fontHeight;						//< フォントの高さ
	var fontRasterizer;					//< ラスタライザ
	var sampleText;						//< サンプルテキスト
	var useFontFace;					//< fsfUseFontFaceオプション指定の有無

	var fontLayer;						//< フォント描画レイヤ（使い回し用）
	var layerCreated;					//< フォント描画済みのフラグ配列
	var selectLayer, selectBitmap;		//< ListSelect描画用レイヤ・Bitmap
	var sampleLayer, sampleBitmap;		//< サンプル描画用レイヤ・Bitmap

	// [TODO] リソースなどでボタンの文言/数値を差し替えられるようにする
	function getResourceString (name, def) { return def; }
	function getResourceInteger(name, def) { return def; }

	function finalize() {
		invalidate selectBitmap if (selectBitmap);
		invalidate sampleBitmap if (selectBitmap);
		invalidate selectLayer  if (selectLayer);
		invalidate sampleLayer  if (sampleLayer);
		invalidate fontLayer    if (fontLayer);
		super.finalize(...);
	}
	function FontSelectDialog(lay, face, flags, caption, prompt, sample) {
		// ボタン名称を取得
		var tagOK = getResourceString("ButtonOK",     "&OK");
		var tagNG = getResourceString("ButtonCancel", "キャンセル");

		// ダイアログ生成
		super.WIN32GenericDialogEX(%[ title:caption, width:200, itemtexts:%[ IDOK=>tagOK, IDCANCEL=>tagNG ] ]);

		// 作業用レイヤ・Bitmap
		fontLayer    = new Layer(lay.window, lay);
		selectLayer  = new Layer(lay.window, lay);
		selectBitmap = new global.WIN32Dialog.Bitmap(selectLayer);

		// 変数初期化
		useFontFace = flags & fsfUseFontFace;
		fontList = getFontList(lay, flags);
		initialSelect = getInitialSelect(face);
		initialSelect = 0 if (initialSelect < 0);

		fontHeight = lay.font.height;
		fontRasterizer = lay.font.rasterizer if (typeof lay.font.rasterizer != "undefined");
		itemPadding = getResourceInteger("FontDialogListPadding", 4);
		itemHeight = (fontHeight < 0 ? -fontHeight : fontHeight) + itemPadding;

		// ListSelectの高さを決定
		var selheight = getResourceInteger("FontDialogListHeight", 150);
		if (selheight <= 0) selheight = 150;

		// ダイアログの子アイテムを追加
		addLText(prompt);
		nextLine();
		addListSelect("Select", selheight, void, void, fontList);
		if (useFontFace) {
			addInit("Select", "setItemHeight", itemHeight);
			getLastItem().style |= LBS_OWNERDRAWFIXED;
		}
		nextLine();
		if (sample != "") {
			sampleLayer  = new Layer(lay.window, lay);
			sampleBitmap = new global.WIN32Dialog.Bitmap(sampleLayer);
			sampleText = sample;
			addIcon("Sample", itemHeight);
			nextLine();
		}
		addDefPush(IDOK,    makeStepRight(1, 60), 8);
		addButton(IDCANCEL, makeStepRight(0, 60), 8);
	}

	// フォント一覧を取得
	function getFontList(lay, flags) {
		var list = [];
		list.assign(lay.font.getList(flags));
//		list.sort(); // ソート不要？
		return list;
	}

	// 現在選択中のフォントを調べる
	function getInitialSelect(face) {
		face = (string)face;
		if (face.indexOf(",") < 0) return fontList.find(face);
		var div = face.split(",",, true);
		for (var i = 0; i < div.count; i++) {
			var r = getInitialSelect(div[i]);
			if (r >= 0) return r;
		}
		return -1;
	}

	// ダイアログ表示開始時処理
	function onInit() {
		super.onInit(...);
		setCenterPosition();
		setItemFocus( "Select");
		selectListBox("Select", initialSelect);
		onSelectChanged();
	}
	// ListBox高さ指定用
	function setItemHeight(id, h) {
		sendItemMessage(id, LB_SETITEMHEIGHT, 0, h);
	}

	// ダイアログイベント処理
	function onCommand(msg, wp, lp) {
		var proc, notify = wp >>16;
		switch (getNamedId(wp & 0xFFFF)) {
		case "Select": proc = onSelectCommand(notify, lp); break;
		}
		return proc ? true : super.onCommand(...);
	}
	function onSelectCommand(notify, lp) {
		switch (notify) {
		case LBN_DBLCLK:
			close(IDOK);
			return true;
		case LBN_SELCHANGE:
			onSelectChanged();
			break;
		}
	}
	function onSelectChanged() {
		// 選択が変更された
		if (sampleText != "" && sampleLayer) with (sampleLayer) {
			// [TODO]
			var id = "Sample";
			var index = getResult("Select");
			var face = fontList[index];
			var numId = getNumberdId(id);
			var w = getItemWidth(numId);
			var h = getItemHeight(numId);
			.setSize(w, h);
			.fillRect(0, 0, w, h, getActualColor(clBtnFace) | 0xFF000000);
			setFontFace(sampleLayer, face, fontHeight);
			var sz = fontHeight < 0 ? -fontHeight : fontHeight;
			try {
				.drawText(itemPadding, (h - sz)\2, sampleText, getActualColor(clBtnText), 255, true);
			} catch (e) {
				Debug.notice("drawTextFailed:", face, e.message);
			}

			setItemBitmap(id, sampleBitmap);
			allBitmaps.clear(); // [XXX]
		}
	}
	function onDrawItem(id, info) {
		if (!useFontFace) return false;
		if (getNamedId(id) == "Select") try {
			var x, y, w, h;
			with (info.itemRect) x=.x, y=.y, w=.w, h=.h;
			var selected = info.itemState & ODS_SELECTED;

			createFontLayer(w);
			var y2 = y + sendItemMessage(id, LB_GETTOPINDEX, 0, 0) * itemHeight;
			var getcol = getActualColor;
			with (selectLayer) {
				.setImageSize(w, h);
				.fillRect(0, 0, w, h,           getcol(selected ? clHighlight : clWindow) | 0xFF000000);
				drawFontLayer(y2, h);
				fontLayer.fillRect(x, y2, w, h, getcol(selected ? clHighlightText : clWindowText));
				.operateRect(0, 0, fontLayer, x, y2, w, h);
			}
			info.draw(selectBitmap, x, y);
		} catch (e) {}
		return true;
	}
	function getActualColor(tag) { return System.toActualColor(tag); }
	function createFontLayer(width) {
		if (layerCreated) return;
		/**/layerCreated = [];
		var cnt = fontList.count;
		with (fontLayer) {
			.setImageSize(width, cnt * itemHeight);
			.fillRect(0, 0, .imageWidth, .imageHeight, 0);
		}
	}
	function drawFontLayer(y2, h) {
		for (var s = y2\itemHeight, e = (y2+h-1)\itemHeight; s <= e; s++) {
			if(!layerCreated[s]) {
				layerCreated[s] = true;
				with (fontLayer) .holdAlpha = false, .face = dfBoth;
				drawFontLayerOne(s);
			}
		}
		with (fontLayer) .holdAlpha = true, .face = dfOpaque;
	}
	function drawFontLayerOne(i) {
		var h = fontHeight < 0 ? -fontHeight : fontHeight;
		with (fontLayer) {
			var face = setFontFace(fontLayer, fontList[i], fontHeight);
			.font.height = fontHeight;
			try {
				.drawText(itemPadding, i*itemHeight + itemPadding\2, face, 0xFFFFFF, 255, true);
			} catch (e) {
				Debug.notice("drawTextFailed:", face, e.message);
			}
		}
	}
	function setFontFace(lay, face, height) {
		with (lay.font) {
			.face = face;
			.height = height if (height !== void);
			.rasterizer = fontRasterizer if (fontRasterizer !== void);
		}
		return face;
	}

	function open() {
		var r = super.open(...);
		if (r.result == IDOK) return fontList[r.items.Select];
	}
}

&System.doFontSelect = function (layer, *) { // flags, caption, prompt, sample) {
	if (typeof layer != "Object" || !layer || !(layer instanceof "Layer"))
		throw new Exception("System.doFontSelect: Specify a Layer object");
	var dialog = new FontSelectDialog(layer, layer.font.face, *);
	var result = dialog.open(layer.window);
	invalidate dialog;
	return result;
} incontextof global;
)KRKRZ";

struct K2CompatScriptEntry { const char *name; const char *text; };
const K2CompatScriptEntry k2compat_scripts[] = {
    { "k2compat/k2compat.tjs", k2compat_main_tjs },
    { "k2compat/win32dialog.tjs", k2compat_win32dialog_tjs },
    { "k2compat/k2compat_modeless.tjs", k2compat_modeless_tjs },
    { "k2compat/k2compat_padcommon.tjs", k2compat_padcommon_tjs },
    { "k2compat/k2compat_pad.tjs", k2compat_pad_tjs },
    { "k2compat/k2compat_console.tjs", k2compat_console_tjs },
    { "k2compat/k2compat_inputstring.tjs", k2compat_inputstring_tjs },
    { "k2compat/k2compat_fontselect.tjs", k2compat_fontselect_tjs },
};

} // namespace

static bool s_k2compat_installed = false;

void TVPResetK2CompatInstalledForRestart() { s_k2compat_installed = false; }

// 把 UTF-8 脚本转宽字符后经 tTJS::ExecScript 执行（name 供报错定位）。
static void ExecUtf8Script(tTJS *tjs, const char *utf8, const char *name) {
    if(!utf8 || !*utf8) return;
    tjs_int len = TVPUtf8ToWideCharString(utf8, nullptr);
    if(len < 0) { spdlog::error("K2Compat: invalid UTF-8 in {}", name); return; }
    std::vector<tjs_char> buf(static_cast<size_t>(len) + 1);
    TVPUtf8ToWideCharString(utf8, buf.data());
    buf[len] = 0;
    ttstr script(buf.data());
    ttstr scriptname(name);
    tjs->ExecScript(script, nullptr, nullptr, &scriptname);
}

void TVPInstallK2CompatScripts() {
    if(s_k2compat_installed) return;
    s_k2compat_installed = true;
    tTJS *tjs = TVPGetScriptEngine();
    if(!tjs) { spdlog::warn("K2Compat: script engine not ready, skip install"); return; }
    // 1. 主脚本: 定义 Krkr2CompatUtils 单例 + 注册 dummy 属性/延迟加载
    ExecUtf8Script(tjs, k2compat_main_tjs, "k2compat/k2compat.tjs");
    // 2. 预置 scriptLoaded: 子脚本顶层 require() 因此命中跳过, 避免依赖
    //    Scripts.execStorage 从存储读取（脚本已内嵌, 存储中不存在）
    static const char *preseed =
        "with (global.Krkr2CompatUtils) { "
        "scriptLoaded[\"modeless\"]=true; "
        "scriptLoaded[\"padcommon\"]=true; "
        "scriptLoaded[\"pad\"]=true; "
        "scriptLoaded[\"console\"]=true; "
        "scriptLoaded[\"inputstring\"]=true; "
        "scriptLoaded[\"fontselect\"]=true; }\n";
    ExecUtf8Script(tjs, preseed, "k2compat/preseed");
    // 3. 按依赖序执行子模块脚本（win32dialog→modeless→padcommon→{pad,console,inputstring,fontselect}）
    for(size_t i = 1; i < sizeof(k2compat_scripts) / sizeof(k2compat_scripts[0]); ++i) {
        const auto &e = k2compat_scripts[i];
        spdlog::info("K2Compat: installing {}", e.name);
        ExecUtf8Script(tjs, e.text, e.name);
    }
}
