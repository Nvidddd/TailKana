#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the Windows port to a separate, hash-pinned Mozc checkout."""
from pathlib import Path
import argparse, html, json, re, shutil, struct, subprocess, uuid
ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads((ROOT/'windows/upstream.json').read_text())

def prepare(source: Path):
    source = source.resolve()
    actual = subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    if actual != LOCK['commit']:
        raise SystemExit('Unexpected Mozc revision; refusing to patch: '+actual)
    base = source/'src'
    pending = {}
    def read(relative):
        if relative not in pending: pending[relative] = (base/relative).read_text(encoding='utf-8')
        return pending[relative]
    def patch(relative, old, new, count=1):
        text = read(relative)
        if new in text: return
        if text.count(old) != count: raise RuntimeError(f'Upstream changed: {relative}: {old[:100]!r}')
        pending[relative] = text.replace(old,new,count)
    def transform(relative, operation): pending[relative] = operation(read(relative))
    h = 'win32/tip/tip_private_context.h'
    patch(h,'#include "client/client_interface.h"','#include "client/client_interface.h"\n#include "win32/tip/tailkana_windows.h"')
    patch(h,'  client::ClientInterface* GetClient();','  client::ClientInterface* GetClient();\n  TailKanaWindows* GetTailKana();')
    c = 'win32/tip/tip_private_context.cc'
    patch(c,'  std::unique_ptr<client::ClientInterface> client_;','  std::unique_ptr<client::ClientInterface> client_;\n  std::unique_ptr<TailKanaWindows> tailkana_;')
    patch(c,'TipPrivateContext::~TipPrivateContext() = default;','''TipPrivateContext::~TipPrivateContext() = default;
TailKanaWindows* TipPrivateContext::GetTailKana() {
  if (!state_->tailkana_) state_->tailkana_ = std::make_unique<TailKanaWindows>(state_->client_.get());
  return state_->tailkana_.get();
}''')
    patch('win32/tip/tip_input_mode_manager.h','  bool GetEffectiveOpenClose() const;','''  bool TailKanaSensitiveInput() const {
    for (const auto scope : input_scope_) if (scope == IS_PASSWORD || scope == IS_NUMERIC_PASSWORD || scope == IS_NUMERIC_PIN || scope == IS_ALPHANUMERIC_PIN || scope == IS_ALPHANUMERIC_PIN_SET) return true;
    return false;
  }
  bool GetEffectiveOpenClose() const;''')
    patch('win32/tip/tip_input_mode_manager.cc','      case IS_PASSWORD:','''      case IS_PASSWORD:
      case IS_NUMERIC_PASSWORD:
      case IS_NUMERIC_PIN:
      case IS_ALPHANUMERIC_PIN:
      case IS_ALPHANUMERIC_PIN_SET:''')
    k = 'win32/tip/tip_keyevent_handler.cc'
    anchor = '  const KeyboardStatus keyboard_status(key_state);'
    # This exact line appears twice: Test first, Key second. Keep test callbacks
    # free of conversion/edit side effects and bypass disabled/password contexts.
    text = read(k)
    if 'GetTailKana()->ProcessKey' not in text:
        if text.count(anchor) != 2: raise RuntimeError('Key handler anchors changed')
        before, middle, after = text.split(anchor)
        test = '''  if (!TipStatus::IsDisabledContext(context) &&
      private_context->GetTailKana()->ProcessKey(text_service,context,true,open,text_service->GetThreadContext()->GetInputModeManager()->GetEffectiveConversionMode(),is_key_down,wparam,lparam,key_state,eaten)) return S_OK;
'''
        key = '''  if (!TipStatus::IsDisabledContext(context) &&
      private_context->GetTailKana()->ProcessKey(text_service,context,false,open,text_service->GetThreadContext()->GetInputModeManager()->GetEffectiveConversionMode(),is_key_down,wparam,lparam,key_state,eaten)) return S_OK;
'''
        pending[k] = before+test+anchor+middle+key+anchor+after
    else:
        pending[k] = text.replace('true,open,is_key_down,wparam','true,open,text_service->GetThreadContext()->GetInputModeManager()->GetEffectiveConversionMode(),is_key_down,wparam').replace('false,open,is_key_down,wparam','false,open,text_service->GetThreadContext()->GetInputModeManager()->GetEffectiveConversionMode(),is_key_down,wparam')
    ui = 'win32/tip/tip_ui_handler_conventional.cc'
    patch(ui,'void UpdateCommand(TipTextService* text_service, ITfContext* context,','''void DeliverTailKana(TipTextService* service, ITfContext* context, RendererCommand command) {
  if (auto* state = service->GetPrivateContext(context)) {
    if (state->GetTailKana()->Render(service,context,command)) command.set_visible(false);
  }
  Win32RendererClient::OnUpdated(command);
}

void UpdateCommand(TipTextService* text_service, ITfContext* context,''')
    patch(ui,'      Win32RendererClient::OnUpdated(command);','      DeliverTailKana(text_service_.get(),context_.get(),command);')
    # Two-space indentation identifies the synchronous updater, without changing
    # the empty-document hide path (which must never dereference a null context).
    patch(ui,'    Win32RendererClient::OnUpdated(command);\n  }\n  return true;','    DeliverTailKana(text_service,context,command);\n  }\n  return true;')
    patch(ui,'  OnFocusChange(text_service, document.get());\n}', '  OnFocusChange(text_service, document.get());\n  TailKanaWindows::ApplyDefaultMode(text_service);\n}')
    patch(ui,'void TipUiHandlerConventional::OnDeactivate() {','void TipUiHandlerConventional::OnDeactivate() {\n  TailKanaWindows::HideAll();')
    patch(ui,'    TipTextService* text_service, ITfDocumentMgr* focused_document_manager) {','    TipTextService* text_service, ITfDocumentMgr* focused_document_manager) {\n  TailKanaWindows::HideAll();')
    es = 'win32/tip/tip_edit_session.cc'
    for name,submit in [('HilightCandidateAsync','false'),('SelectCandidateAsync','true')]:
        old = f'''bool TipEditSession::{name}(TipTextService* text_service,
{' '* (43 if name=='HilightCandidateAsync' else 42)}ITfContext* context,
{' '* (43 if name=='HilightCandidateAsync' else 42)}int candidate_id) {{'''
        if old not in read(es): raise RuntimeError('Candidate edit-session signature changed: '+name)
        patch(es,old,old+f'''\n  if (auto* state = text_service->GetPrivateContext(context)) {{
    if (state->GetTailKana()->CandidateAction(text_service,context,candidate_id,{submit})) return true;
  }}''')
    patch(es, '  SessionCommand session_command;\n  session_command.set_type(SessionCommand::SUBMIT);\n  return OnSessionCommandAsync(text_service, context, session_command);', '  if (private_context->GetTailKana()->Finish(text_service,context,false)) return true;\n  SessionCommand session_command;\n  session_command.set_type(SessionCommand::SUBMIT);\n  return OnSessionCommandAsync(text_service, context, session_command);')
    patch(es, '  SessionCommand command;\n  command.set_type(SessionCommand::REVERT);', '  if (auto* state = text_service->GetPrivateContext(context)) { if (state->GetTailKana()->Finish(text_service,context,true)) return true; }\n  SessionCommand command;\n  command.set_type(SessionCommand::REVERT);')
    callback = '''      if ((type == SessionCommand::HIGHLIGHT_CANDIDATE) &&'''
    patch(es,callback,'''      if (private_context->GetTailKana()->CandidateAction(text_service,context,candidate_id,type == SessionCommand::SELECT_CANDIDATE)) return true;
'''+callback)
    # Every consumer sees only a small header target; the implementation is
    # linked by tip_private_context, avoiding a cyclic TSF dependency graph.
    build = 'win32/tip/BUILD.bazel'
    text = read(build)
    additions = {
        'tip_private_context_h': [':tailkana_windows_h'],
        'tip_private_context': [':tailkana_windows_impl'],
    }
    for name,deps in additions.items():
        pattern = r'(name = "'+name+r'",.*?deps = \[)'
        match = re.search(pattern,text,re.S)
        if not match: raise RuntimeError('Missing Bazel target: '+name)
        if all('"'+d+'"' in text[match.end():text.find('    ],',match.end())] for d in deps): continue
        text = text[:match.end()]+''.join('\n        "'+d+'",' for d in deps)+text[match.end():]
    if 'name = "tailkana_windows_impl"' not in text:
        text += '''
mozc_cc_library(
    name = "tailkana_windows_h",
    hdrs = ["tailkana_windows.h"],
    target_compatible_with = ["@platforms//os:windows"],
    deps = ["//client:client_interface", "//protocol:renderer_cc_proto"],
)
mozc_cc_library(
    name = "tailkana_windows_impl",
    srcs = ["tailkana_windows.cc"],
    features = features_gdi(),
    target_compatible_with = ["@platforms//os:windows"],
    linkopts = ["advapi32.lib"],
    deps = [
        ":tailkana_windows_h", ":tip_private_context_h", ":tip_text_service", ":tip_dll_module", ":tip_range_util",
        ":tip_edit_session", ":tip_status", "//client", "//client:client_interface",
        "//protocol:commands_cc_proto", "//protocol:candidate_window_cc_proto",
        "//protocol:renderer_cc_proto", "//win32/tailkana:core", "//win32/tailkana:panel",
        "@com_microsoft_wil//:wil",
    ],
)
'''
    impl_start=text.index('name = "tailkana_windows_impl",')
    deps_start=text.index('deps = [',impl_start)+len('deps = [')
    deps_end=text.index('    ],',deps_start)
    for dependency in (':tip_dll_module',':tip_range_util',':tip_thread_context',':tip_input_mode_manager'):
        if '"'+dependency+'"' not in text[deps_start:deps_end]:
            text=text[:deps_start]+'\n        "'+dependency+'",'+text[deps_start:]
            deps_end=text.index('    ],',deps_start)
    if 'name = "tailkana_converter"' not in text:
        text = text.replace('"mozc_cc_library",','"mozc_cc_library",\n    "mozc_cc_binary",',1)
        text = text.replace('name = "tailkana_windows_impl",','name = "tailkana_windows_impl",',1)
        start = text.index('name = "tailkana_windows_impl",')
        position = text.index('deps = [',start)+len('deps = [')
        text = text[:position]+'\n        ":tailkana_converter",'+text[position:]
        text += '''
mozc_cc_library(
    name = "tailkana_converter",
    srcs = ["mozc_converter.cc"], hdrs = ["mozc_converter.h"],
    target_compatible_with = ["@platforms//os:windows"],
    deps = ["//client", "//client:client_interface", "//protocol:commands_cc_proto", "//protocol:candidate_window_cc_proto", "//win32/tailkana:core"],
)
mozc_cc_binary(
    name = "tailkana_backend_test", srcs = ["tailkana_backend_test.cc"],
    target_compatible_with = ["@platforms//os:windows"],
    deps = [":tailkana_converter", ":tip_input_mode_manager", "//base:init_mozc", "//win32/tailkana:core"],
)
'''
    test_start=text.index('name = "tailkana_backend_test",')
    deps_start=text.index('deps = [',test_start)+len('deps = [')
    deps_end=text.index(']',deps_start)
    if '":tip_input_mode_manager"' not in text[deps_start:deps_end]: text=text[:deps_start]+'":tip_input_mode_manager", '+text[deps_start:]
    pending[build] = text
    # Fixed, distinct identities are applied only to the Windows working copy.
    def cpp_guid(value):
        data = uuid.UUID(value)
        return f'0x{data.time_low:08x},\n    0x{data.time_mid:04x},\n    0x{data.time_hi_version:04x},\n    {{'+', '.join(f'0x{b:02x}' for b in data.bytes[8:])+'}'
    profile = 'win32/base/tsf_profile.cc'
    for symbol,key in [('kMozcTextService','textService'),('kMozcProfile','languageProfile')]:
        text = read(profile)
        text,n = re.subn(r'(static const GUID '+symbol+r' = \{).*?(\};)',lambda m:m[1]+'\n    '+cpp_guid(LOCK['identity'][key])+m[2],text,flags=re.S)
        if n != 1: raise RuntimeError('Missing TSF GUID '+symbol)
        pending[profile] = text
    transform('base/system_util.cc',lambda s:s.replace('{10A67BC8-22FA-4A59-90DC-2546652C56BF}','{'+LOCK['identity']['textService']+'}'))
    transform('base/const.h',lambda s:s.replace('"Mozc Project"','"TailKana Project"').replace('"Mozc"','"TailKana"').replace('"MozcCacheService"','"TailKanaCacheService"').replace('Mozc Project\\\\Mozc','TailKana\\\\TailKana').replace('Local\\\\Mozc.','Local\\\\TailKana.').replace('\\\\pipe\\\\mozc.','\\\\pipe\\\\tailkana.').replace('L"mozc.renderer.','L"tailkana.renderer.'))
    transform('win32/tip/tip_text_service.cc',lambda s:s.replace('https://github.com/google/mozc','https://github.com/Nvidddd/TailKana').replace('Mozc Immersive Task Message Window','TailKana TSF Task Message Window'))
    transform('win32/tip/tip_resource.rc',lambda s:s.replace('"Mozc"','"TailKana"').replace('About Mozc','关于 TailKana').replace('Mozc について','关于 TailKana').replace('"Input Mode"','"输入方式"').replace('"Direct Input"','"英文直输"').replace('"Hiragana"','"平假名"').replace('"Full-width Katakana"','"全角片假名"'))
    wix = 'win32/installer/installer_oss_64bit.wxs'
    transform(wix,lambda s:s.replace('Name="Mozc"','Name="TailKana"').replace('Language="1041" Codepage="932"','Language="2052" Codepage="936"').replace('Codepage="932"','Codepage="936"').replace('Manufacturer="Google LLC"','Manufacturer="TailKana Project"').replace('Mozc インストーラー','TailKana 安装程序').replace('Title="Mozc"','Title="TailKana"').replace('Name="MozcCacheService"','Name="TailKanaCacheService"').replace('Name="Mozc Prelauncher"','Name="TailKana Prelauncher"').replace('Mozc をインストールするには','安装 TailKana 需要：').replace('管理者権限が必要です。','管理员权限。').replace('Windows 10 1809 以降にアップグレードする必要があります。','Windows 10 1809 或更高版本。'))
    patch('win32/installer/build_installer.py',"upgrade_code = 'DD94B570-B5E2-4100-9D42-61930C611D8A'","upgrade_code = '"+LOCK['identity']['upgradeCode']+"'")
    patch('win32/installer/build_installer.py',"f'MozcVersion={version.GetVersionString()}'","'MozcVersion="+LOCK['installerVersion']+"'")
    credit = 'data/installer/credits_en.html'
    if 'TailKana Windows source' not in read(credit):
        text = '<section><h2>TailKana Windows source</h2><p>GPL-3.0-or-later. Source: https://github.com/Nvidddd/TailKana/tree/codex/windows-port</p><h3>TailKana</h3><pre>'+html.escape((ROOT/'LICENSE').read_text())+'</pre><h3>azooKey typo rules (MIT)</h3><pre>'+html.escape((ROOT/'third_party/azookey-typo/LICENSE').read_text())+'</pre></section>'
        pending[credit] = read(credit)+text
    # Commit all validated replacements together. A stale upstream fails before
    # any source file is changed.
    for relative,content in pending.items():
        target = base/relative
        if target.read_text(encoding='utf-8') != content: target.write_text(content,encoding='utf-8')
    copied = base/'win32/tailkana';copied.mkdir(exist_ok=True)
    for name in ('roman_buffer.h','roman_buffer.cpp','roman_table.inc','roman_correction.h','roman_correction.cpp','pinyin_rules.inc','typo_rules.inc','typing_policy.h','typing_policy.cpp'):
        shutil.copy2(ROOT/'src'/name,copied/name.replace('.cpp','.cc'))
    for name in ('session.h','session.cpp'): shutil.copy2(ROOT/'windows/core'/name,copied/name.replace('.cpp','.cc'))
    for name in ('panel.h','panel.cpp'): shutil.copy2(ROOT/'windows/native'/name,copied/name.replace('.cpp','.cc'))
    for name in ('tailkana_windows.h','tailkana_windows.cpp','mozc_converter.h','mozc_converter.cpp'): shutil.copy2(ROOT/'windows/native'/name,base/'win32/tip'/name.replace('.cpp','.cc'))
    (copied/'BUILD.bazel').write_text('''load("//:build_defs.bzl", "mozc_cc_library")
load("//bazel/win32:build_defs.bzl", "features_gdi")
package(default_visibility = ["//win32/tip:__pkg__"])
mozc_cc_library(
    name = "core",
    srcs = ["session.cc", "roman_buffer.cc", "roman_correction.cc", "typing_policy.cc"],
    hdrs = ["session.h", "roman_buffer.h", "roman_correction.h", "typing_policy.h", "roman_table.inc", "pinyin_rules.inc", "typo_rules.inc"],
)
mozc_cc_library(
    name = "panel", srcs = ["panel.cc"], hdrs = ["panel.h"],
    features = features_gdi(), target_compatible_with = ["@platforms//os:windows"],
    deps = [":core", "//bazel/win32:user32", "//bazel/win32:gdi32"],
)
''',encoding='utf-8')
    shutil.copy2(ROOT/'windows/tests/backend_test.cpp',base/'win32/tip/tailkana_backend_test.cc')
    png = (ROOT/'assets/tailkana-preview.png').read_bytes()
    icon = struct.pack('<HHH',0,1,1)+struct.pack('<BBBBHHII',0,0,0,0,1,32,len(png),22)+png
    for name in ('product_icon.ico','product_icon_langbar.ico'): (base/'data/images/win'/name).write_bytes(icon)
    report = {'upstreamCommit':actual,'version':LOCK['version'],'identity':LOCK['identity'],'patchedFiles':sorted(pending),'source':'https://github.com/Nvidddd/TailKana'}
    (source/'tailkana-patch-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Prepared TailKana Windows:',LOCK['version'],len(pending),'validated upstream modifications')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=ROOT/'work/windows/mozc');args=parser.parse_args();prepare(args.source)
