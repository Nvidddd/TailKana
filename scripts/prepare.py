#!/usr/bin/env python3
"""Apply reproducible local changes without changing upstream input engines."""
from pathlib import Path
import shutil, re, subprocess

def save(path,text):
    if not path.exists() or path.read_text()!=text: path.write_text(text)
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'scripts/prepare-libime.py')],check=True)
base=root/'work/fcitx5-macos'
mozc=root/'work/fcitx5-mozc'
def patch(path,old,new):
    p=base/path
    text=p.read_text()
    if new in text:return
    if old not in text:raise RuntimeError(f'Upstream changed: {path}: {old[:70]}')
    p.write_text(text.replace(old,new,1))
patch(Path('CMakeLists.txt'),'add_subdirectory(src)',f'''set(TAILKANA_ROOT "{root}")
set(TAILKANA_MOZC "{mozc}")
set(BUILD_MOZC_ADDON OFF CACHE BOOL "" FORCE)
add_subdirectory("${{TAILKANA_MOZC}}" tailkana-mozc)
add_subdirectory("${{TAILKANA_ROOT}}/src" tailkana)
add_subdirectory(src)
include("${{TAILKANA_ROOT}}/src/preview.cmake")''')
patch(Path('src/CMakeLists.txt'),'    keyboard\n    webpanel','    keyboard\n    tailkana\n    webpanel')
patch(Path('src/CMakeLists.txt'),'ADDONS beast keyboard webpanel macosfrontend notifications','ADDONS beast keyboard tailkana webpanel macosfrontend notifications')
patch(Path('appium/CMakeLists.txt'),'ADDONS keyboard webpanel macosfrontend','ADDONS beast keyboard tailkana webpanel macosfrontend notifications')
patch(Path('src/fcitx.cpp'),'#include <unistd.h>','#include <unistd.h>\n#include <mach-o/dyld.h>')
patch(Path('src/fcitx.cpp'),'    fs::path app_contents_path{APP_CONTENTS_PATH};','''    // Resolve resources relative to this bundle so packaged builds can be moved.
    uint32_t executablePathSize = 0;
    _NSGetExecutablePath(nullptr, &executablePathSize);
    std::string executablePath(executablePathSize, '\\0');
    _NSGetExecutablePath(executablePath.data(), &executablePathSize);
    fs::path app_contents_path = fs::path(executablePath.c_str()).parent_path().parent_path();''')
patch(Path('src/fcitx.cpp'),'setenv("XKB_CONFIG_ROOT", APP_CONTENTS_PATH "/share/xkeyboard-config-2", 1);','setenv("XKB_CONFIG_ROOT", (app_contents_path / "share/xkeyboard-config-2").c_str(), 1);')
patch(Path('CMakeLists.txt'),'set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")','set(CMAKE_INSTALL_RPATH "@executable_path/../lib")')
# The independent candidate panel transports small structured state through auxDown.
web=base/'fcitx5-webview/page'
for f in ('tailkana.ts','tailkana.scss'):save(web/f,(root/'panel'/f).read_text())
patch(Path('fcitx5-webview/page/index.ts'),"import { HORIZONTAL, VERTICAL } from './constant'","import { HORIZONTAL, VERTICAL } from './constant'\nimport { initTailKana } from './tailkana'")
patch(Path('fcitx5-webview/page/index.ts'),'  initSelectors(container)','  initSelectors(container)\n  initTailKana(container)')
patch(Path('fcitx5-webview/page/panel.ts'),"import emojiRegex from 'emoji-regex'","import emojiRegex from 'emoji-regex'\nimport { tailKanaCandidates, tailKanaMetadata } from './tailkana'")
patch(Path('fcitx5-webview/page/panel.ts'),'  if (cands.length) {','  if (tailKanaCandidates(cands)) return\n  if (cands.length) {')
patch(Path('fcitx5-webview/page/panel.ts'),'  const hasPreedit = formattedPreCaret.length || formattedPostCaret.length','  if (tailKanaMetadata(formattedAuxDown)) return\n  const hasPreedit = formattedPreCaret.length || formattedPostCaret.length')
print('TailKana engine and partition integrated into Fcitx5 macOS')
patch(Path('fcitx5-webview/page/style.scss'),"@use './user';","@use './user';\n@use './tailkana';")
direct=mozc/'src/unix/fcitx5/mozc_direct_client.cc'
save(direct,direct.read_text().replace('<Fcitx5/Utils/fcitx-utils/macros.h>','<fcitx-utils/macros.h>'))

mozc_cmake=mozc/'CMakeLists.txt'
save(mozc_cmake,mozc_cmake.read_text().replace('set(protobuf_BUILD_LIBUPB OFF)', 'set(protobuf_BUILD_LIBUPB ON)'))
patch(Path('macosfrontend/macosfrontend.swift'), 'nonisolated(unsafe) private var controller: IMKInputController? = nil', '''nonisolated(unsafe) private var controller: IMKInputController? = nil
// Only used in the separately packaged preview app; production has a controller.
nonisolated(unsafe) private var previewTextView: NSTextView? = nil
@MainActor public func setPreviewTextView(_ view: NSTextView) { previewTextView = view }''')
if 'applyPreviewResponse(commit, preedit, caretPos)' not in (base/'macosfrontend/macosfrontend.swift').read_text():
    patch(Path('macosfrontend/macosfrontend.swift'), '''    guard let client = controller?.client() else {
          return
        }
        commitAndSetPreeditSync''','''    guard let client = controller?.client() else {
          if !commit.isEmpty { previewTextView?.insertText(commit, replacementRange: previewTextView!.selectedRange()) }
          return
        }
        commitAndSetPreeditSync''')
if 'view.firstRect(forCharacterRange:' not in (base/'macosfrontend/macosfrontend.swift').read_text():
    patch(Path('macosfrontend/macosfrontend.swift'),'''  guard let client = controller?.client() else {
        return []
      }''','''  guard let client = controller?.client() else {
        if let view = previewTextView, let window = view.window {
          let rect = window.convertToScreen(view.convert(NSRect(x: 28,y: 70,width: 1,height: 26), to: nil))
          return [rect.minX, rect.minY, 26]
        }
        return []
      }''')
patch(Path('CMakeLists.txt'),'message(STATUS "Recent tag:', '''if(NOT RECENT_TAG)
    set(RECENT_TAG "0.4.1")
    set(RECENT_COMMITS "1")
endif()
message(STATUS "Recent tag:''')
# Register a distinct Japanese input source without changing upstream language sources.
plist=base/'cmake/MacOSXBundleInfo.plist.in'
s=plist.read_text()
if '<key>org.tailkana.inputmethod.japanese</key>' not in s:
    marker='''      <key>org.fcitx.inputmethod.zhHans</key>'''
    s=s.replace(marker,'''      <key>org.tailkana.inputmethod.japanese</key>
      <dict>
        <key>TISIntendedLanguage</key><string>ja</string>
        <key>tsInputModeLocalizedNameKey</key><string>TailKana</string>
        <key>tsInputModeMenuIconFileKey</key><string>menu_icon.pdf</string>
        <key>tsInputModePaletteIconFileKey</key><string>menu_icon.pdf</string>
      </dict>
'''+marker)
    plist.write_text(s)
patch(Path('src/fcitx.cpp'),'static fcitx::IsoCodes isoCodes;', 'static fcitx::IsoCodes isoCodes;\nstatic fs::path appContentsPath;')
patch(Path('src/fcitx.cpp'),'    isoCodes.read(ISOCODES_ISO639_JSON);', '    isoCodes.read((appContentsPath / "share/iso-codes/json/iso_639-3.json").string());')
if 'appContentsPath = app_contents_path;' not in (base/'src/fcitx.cpp').read_text():
    patch(Path('src/fcitx.cpp'),'    fs::path user_prefix = home / "Library" / "fcitx5";', '    appContentsPath = app_contents_path;\n    fs::path user_prefix = home / "Library" / "fcitx5";')
absl_time=mozc/'abseil-cpp/absl/time/CMakeLists.txt'
save(absl_time,absl_time.read_text().replace('$<$<PLATFORM_ID:Darwin,iOS,tvOS,visionOS,watchOS>:-Wl,-framework,CoreFoundation>', '$<$<PLATFORM_ID:Darwin,iOS,tvOS,visionOS,watchOS>:$<LINK_LIBRARY:FRAMEWORK,CoreFoundation>>'))
patch(Path('appium/CMakeLists.txt'),'    SwiftFrontend\n)', '    SwiftFrontend\n    SwiftNotify\n)')
patch(Path('CMakeLists.txt'),'add_link_options(-target "${CMAKE_Swift_COMPILER_TARGET}")','add_link_options(-target "${CMAKE_Swift_COMPILER_TARGET}" -Xlinker -headerpad_max_install_names)')
capture=(root/'scripts/preview_capture.mm').read_text()
patch(Path('fcitx5-webview/src/platform/macos.mm'), 'NSString *const F5mErrorDomain = @"F5mErrorDomain";', 'NSString *const F5mErrorDomain = @"F5mErrorDomain";\n\n'+capture)
patch(Path('fcitx5-webview/src/platform/macos.mm'), '    [window setIsVisible:YES];', '    [window setIsVisible:YES];\n    tailkanaCapture(window);')
save(base/'fcitx5-webview/tests/tailkana.spec.ts',(root/'tests/panel.spec.ts').read_text())
# The preview follows the same inline preedit/commit lifecycle as the IMK client.
inline=(root/'scripts/preview_inline.swift').read_text()
patch(Path('macosfrontend/macosfrontend.swift'), '@MainActor public func setPreviewTextView(_ view: NSTextView) { previewTextView = view }', '@MainActor public func setPreviewTextView(_ view: NSTextView) { previewTextView = view }\n'+inline)
patch(Path('macosfrontend/macosfrontend.swift'), 'if !commit.isEmpty { previewTextView?.insertText(commit, replacementRange: previewTextView!.selectedRange()) }', 'applyPreviewResponse(commit, preedit, caretPos)')
patch(Path('macosfrontend/macosfrontend.swift'), '      let rect = window.convertToScreen(view.convert(NSRect(x: 28,y: 70,width: 1,height: 26), to: nil))', '''      var rect = view.firstRect(forCharacterRange: view.selectedRange(), actualRange: nil)
      if rect.height == 0 { rect = window.convertToScreen(view.convert(NSRect(x: 28,y: 70,width: 1,height: 26), to: nil)) }''')
# Share the native TailKana settings UI between the input method and the preview.
save(base/'src/config/TailKana.swift',(root/'src/settings.swift').read_text()+'\n'+(root/'src/settings_navigation.swift').read_text()+'\n'+(root/'src/theme_surface.swift').read_text())
if 'func tailKanaSettings(' not in (base/'src/menu.swift').read_text():
    patch(Path('src/menu.swift'), 'extension FcitxInputController {', 'extension FcitxInputController {\n  @MainActor @objc func tailKanaSettings(_: Any? = nil) { openTailKanaSettings() }\n')
# Keep this fork's preferences separate from an existing Fcitx5 installation.
patch(Path('src/fcitx.cpp'), '    fs::path user_prefix = home / "Library" / "fcitx5";', '''    if (!getenv("XDG_CONFIG_HOME")) {
        auto configHome = home / "Library" / "Application Support" / "TailKana" / "Config";
        fs::create_directories(configHome / "fcitx5");
        setenv("XDG_CONFIG_HOME", configHome.c_str(), 1);
    }
    fs::path user_prefix = home / "Library" / "fcitx5";''')
# The installed launcher requests settings without changing the current input source.
patch(Path('src/server.swift'), '    redirectStderr()', '''    redirectStderr()
    DistributedNotificationCenter.default().addObserver(self, selector: #selector(tailKanaSettingsRequested), name: Notification.Name("org.tailkana.open-settings"), object: nil)
''')
patch(Path('src/server.swift'), '  func applicationWillTerminate(_ notification: Notification) {', '''  @MainActor @objc private func tailKanaSettingsRequested() { openTailKanaSettings() }

  func applicationWillTerminate(_ notification: Notification) {''')
# Input-method preferences can accept input as an accessory app, without a
# second Dock icon. Keep the standalone development preview a normal app.
patch(Path('src/config/ConfigWindowController.swift'), '''      // Switch to normal activation policy so that the config windows
      // can receive key events.
      if NSApp.activationPolicy() != .regular {
        NSApp.setActivationPolicy(.regular)
      }''', '''      let policy: NSApplication.ActivationPolicy =
        Bundle.main.bundleIdentifier == "org.tailkana.preview" ? .regular : .accessory
      if NSApp.activationPolicy() != policy {
        NSApp.setActivationPolicy(policy)
      }''')
patch(Path('src/server.swift'), 'static var statusItemText: String = "🐧"', 'static var statusItemText: String = "カ"')
patch(Path('src/status.swift'), 'if text.isEmpty { return "🐧" }', 'if text.isEmpty { return "カ" }')

patch(Path('appium/CMakeLists.txt'), '    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"', '    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps.noindex"\n    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"')
# Present the Fcitx menu as the Chinese TailKana product UI.
controller=base/'src/controller.swift'
text=controller.read_text()
start=re.search(r'  (?:@MainActor )?override func menu\(\) -> NSMenu! \{',text).start()
end=text.index('\n  @objc func switchGroup',start)
save(controller,text[:start]+(root/'src/menu-block.swift').read_text()+text[end:])
# Use the same complete product menu for the extra status icon.
server=base/'src/server.swift'
text=server.read_text()
start=text.index('  private func makeStatusItemMenu() -> NSMenu {')
end=text.index('  @MainActor\n  private func refreshStatusItemVisibility()',start)
save(server,text[:start]+"""  private func makeStatusItemMenu() -> NSMenu {
    TailKanaMenus.controller.makeMenu()
  }

  @MainActor @objc private func tailKanaStatusItemClicked() {
    if AppDelegate.statusItemMode == 1 && NSApp.currentEvent?.type != .rightMouseUp {
      toggle()
      return
    }
    guard let button = AppDelegate.statusItem?.button else {return}
    makeStatusItemMenu().popUp(positioning: nil, at: NSPoint(x: 0, y: button.bounds.minY - 2), in: button)
  }

"""+text[end:])
patch(Path('src/server.swift'), """      if AppDelegate.statusItemMode == 1 {
        button.action = #selector(self.toggle)
      } else {
        statusItem.menu = makeStatusItemMenu()
      }""", """      button.sendAction(on: [.leftMouseUp, .rightMouseUp])
      button.action = #selector(tailKanaStatusItemClicked)
""")
about_menu=base/'src/menu.swift'
about_text=about_menu.read_text()
if 'func tailKanaAbout(' in about_text:
    about_text=re.sub(r'  @MainActor @objc func tailKanaAbout\(_:\s*Any\? = nil\) \{[\s\S]*?\n  \}',(root/'src/about-action.swift').read_text().rstrip(),about_text,count=1)
    save(about_menu,about_text)
else:
    patch(Path('src/menu.swift'), 'extension FcitxInputController {', 'extension FcitxInputController {\n'+(root/'src/about-action.swift').read_text())
patch(Path('src/server.swift'), '  override init() {\n    super.init()', '  override init() {\n    UserDefaults.standard.set(["zh-Hans"], forKey: "AppleLanguages")\n    super.init()')
patch(Path('src/locale.swift'), 'func getLocale() -> String {', 'func getLocale() -> String {\n  if Bundle.main.bundleIdentifier?.hasPrefix("org.tailkana.inputmethod.") == true { return "zh_CN" }')
# Resolve all shipped gettext catalogues relative to the installed bundle.
patch(Path('src/fcitx.cpp'), 'fcitx::registerDomain(FCITX_GETTEXT_DOMAIN, FCITX_INSTALL_LOCALEDIR);', 'fcitx::registerDomain(FCITX_GETTEXT_DOMAIN, appContentsPath / "share/locale");')
patch(Path('src/fcitx.cpp'), 'fcitx::registerDomain(ISO_639_3_DOMAIN, FCITX_INSTALL_LOCALEDIR);', 'fcitx::registerDomain(ISO_639_3_DOMAIN, appContentsPath / "share/locale");')
patch(Path('src/fcitx.cpp'), 'void setupI18N() {', '''void setupI18N() {
    auto bundledLocale = appContentsPath / "share/locale";
    for (const auto& domain : getPluginDomains(bundledLocale)) {
        fcitx::registerDomain(domain.c_str(), bundledLocale);
    }''')
# Keep the window's real NSVisualEffectView consistent with TailKana's glass
# theme; CSS transparency alone cannot blur the desktop behind a WKWebView.
patch(Path('fcitx5-webview/include/webview_candidate_window.hpp'), '    void set_native_blur(blur_t value) const;', '    void set_native_blur(blur_t value) const;\n    void set_tailkana_appearance(int value) const;')
patch(Path('fcitx5-webview/src/webview_candidate_window.cpp'), '    bind("tabAction",', '#ifdef __APPLE__\n    bind("tailkanaAppearance", [this](int value) { set_tailkana_appearance(value); });\n#endif\n\n    bind("tabAction",')
native=base/'fcitx5-webview/src/platform/macos.mm'
native_text=native.read_text()
native_text=re.sub(r'// TailKana native candidate panel begin[\s\S]*?// TailKana native candidate panel end\n*','',native_text)
panel_code='// TailKana native candidate panel begin\n'+(root/'src/candidate_panel.h').read_text()+'\n// TailKana native candidate panel end\n'
native_text=native_text.replace('NSString *const F5mErrorDomain',panel_code+'\nNSString *const F5mErrorDomain',1)
native_text=native_text.replace('@interface HoverableWindow : NSWindow','@interface HoverableWindow : TailKanaCandidatePanel')
if '@property(nonatomic) NSInteger tailKanaAppearance;' not in native_text:
    native_text=native_text.replace('@property(nonatomic) NSRect blurViewRect;', '@property(nonatomic) NSRect blurViewRect;\n@property(nonatomic) NSInteger tailKanaAppearance;\n@property(nonatomic) NSInteger configuredBlur;')
if '@property(nonatomic) BOOL tailKanaApplyingBlur;' not in native_text:
    native_text=native_text.replace('@property(nonatomic) NSInteger configuredBlur;', '@property(nonatomic) NSInteger configuredBlur;\n@property(nonatomic) BOOL tailKanaApplyingBlur;')
if '@property(nonatomic) BOOL configuredShadow;' not in native_text:
    native_text=native_text.replace('@property(nonatomic) NSInteger configuredBlur;', '@property(nonatomic) NSInteger configuredBlur;\n@property(nonatomic) BOOL configuredShadow;')
bridge_start=native_text.find('void WebviewCandidateWindow::set_tailkana_appearance(')
blur_start=native_text.index('void WebviewCandidateWindow::set_native_blur(')
if bridge_start < 0: bridge_start=blur_start
native_text=native_text[:bridge_start]+(root/'src/native_glass.mm').read_text()+'\n'+native_text[blur_start:]
legacy='    if (window.tailKanaAppearance == 0) window.configuredBlur = static_cast<int>(value);\n'
native_text=native_text.replace(legacy,'')
policy='    if (!window.tailKanaApplyingBlur) window.configuredBlur = static_cast<int>(value);\n    if (window.tailKanaAppearance > 0) value = window.tailKanaAppearance <= 3 ? blur_t::blur : blur_t::none;\n'
blur_start=native_text.index('void WebviewCandidateWindow::set_native_blur(')
if policy not in native_text:
    before=native_text[:blur_start]
    after=native_text[blur_start:].replace('    if (window.blurView.hidden == NO) {',policy+'    if (window.blurView.hidden == NO) {',1)
    native_text=before+after
native_text=native_text.replace('visualView.material = NSVisualEffectMaterialHUDWindow;', 'visualView.material = NSVisualEffectMaterialUnderWindowBackground;')
shadow_start=native_text.index('void WebviewCandidateWindow::set_native_shadow(bool enabled) const {')
shadow_end=native_text.index('\n}',shadow_start)+2
native_text=native_text[:shadow_start]+'''void WebviewCandidateWindow::set_native_shadow(bool enabled) const {
    HoverableWindow *window = unwrap_webview_handle<HoverableWindow>(w_->window());
    if (!window.tailKanaApplyingBlur) window.configuredShadow = enabled;
    [window setHasShadow:tailKanaCandidateNativeShadow(window.tailKanaAppearance, enabled)];
}'''+native_text[shadow_end:]
save(native,native_text)
protocol=base/'fcitx5-webview/page/global.d.ts'
protocol_text=protocol.read_text()
if "(name: 'tailkanaAppearance'," not in protocol_text:
    protocol_text=protocol_text.replace("    (name: 'tabAction', id: number): void", "    (name: 'tabAction', id: number): void\n    (name: 'tailkanaAppearance', appearance: 0 | 1 | 2 | 3 | 4 | 5 | 6): void")
protocol_text=protocol_text.replace('appearance: 0 | 1 | 2 | 3)', 'appearance: 0 | 1 | 2 | 3 | 4 | 5 | 6)')
save(protocol,protocol_text)

# Suppress OS auto-repeat for the explicit mode shortcut.
patch(Path('src/controller.swift'), '    case .keyDown:\n      let unicode', '    case .keyDown:\n      if event.isARepeat && code == kVK_Tab && mods.contains(.shift) && mods.isDisjoint(with: [.command, .control, .option]) { return true }\n      let unicode')

# Apply one material/background treatment to every product settings controller.
patch(Path('src/config/ConfigWindowController.swift'), '    if let window = window {\n      let policy:', '    if let window = window {\n      installTailKanaSettingsGlass(window)\n      let policy:')
patch(Path('src/config/ConfigWindowController.swift'), '    for controller in controllers.values {\n      controller.refresh()', '    for controller in controllers.values {\n      if let window = controller.window {installTailKanaSettingsGlass(window)}\n      controller.refresh()')

# Let the shared native material show through all SwiftUI settings canvases.
for settings_source in (base/'src/config').glob('*.swift'):
    contents=settings_source.read_text()
    contents=re.sub(r'NSHostingView\(rootView: (view|[A-Za-z_][A-Za-z_0-9]*\(\))\)', r'NSHostingView(rootView: TailKanaSettingsSurface(content: \1.scrollContentBackground(.hidden)))', contents)
    contents=re.sub(r'NSHostingView\(rootView: (view|[A-Za-z_][A-Za-z_0-9]*\(\))\.scrollContentBackground\(\.hidden\)\)', r'NSHostingView(rootView: TailKanaSettingsSurface(content: \1.scrollContentBackground(.hidden)))', contents)
    save(settings_source,contents)
