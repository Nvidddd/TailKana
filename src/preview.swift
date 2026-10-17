import AppKit
import CxxFrontend
import Fcitx
import FcitxConfigUI
import SwiftFrontend

struct PreviewResponse: Decodable { let commit: String; let preedit: String; let caretPos: Int; let accepted: Bool }
@MainActor
final class TailKanaTextView: NSTextView {
  let uuid = create_input_context("org.tailkana.preview", "#4f795b")
  var lastModifiers = NSEvent.ModifierFlags()
  func send(_ event: NSEvent, release: Bool) -> Bool {
    let unicode = event.type == .keyDown ? UInt32(event.charactersIgnoringModifiers?.unicodeScalars.first?.value ?? 0) : 0
    let json = String(process_key(uuid, unicode, UInt32(event.modifierFlags.rawValue), event.keyCode, release, false, string, UInt32(selectedRange().location), UInt32(NSMaxRange(selectedRange()))))
    guard let response = try? JSONDecoder().decode(PreviewResponse.self, from: Data(json.utf8)) else { return false }
    applyPreviewResponse(response.commit, response.preedit, response.caretPos)
    return response.accepted
  }
  override func keyDown(with event: NSEvent) { if !send(event, release: false) { super.keyDown(with: event) } }
  override func flagsChanged(with event: NSEvent) {
    let change = event.modifierFlags.rawValue ^ lastModifiers.rawValue
    let release = (lastModifiers.rawValue & change) != 0
    _ = send(event, release: release)
    lastModifiers = event.modifierFlags
  }
  override func becomeFirstResponder() -> Bool {
    focus_in(uuid, false)
    imSetCurrentIM("tailkana")
    return super.becomeFirstResponder()
  }
}
@MainActor
final class PreviewDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
  var window: NSWindow!
  func applicationDidFinishLaunching(_ notification: Notification) {
    let menu = NSMenu()
    let appItem = NSMenuItem()
    let appMenu = NSMenu(title: "TailKana")
    let settings = NSMenuItem(title: "TailKana 设置…", action: #selector(showSettings), keyEquivalent: ",")
    settings.target = self
    appMenu.addItem(settings)
    appMenu.addItem(.separator())
    let quit = NSMenuItem(title: "退出试打", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
    quit.target = NSApp
    appMenu.addItem(quit)
    appItem.submenu = appMenu
    menu.addItem(appItem)
    NSApp.mainMenu = menu
    // Isolate the preview profile from installed input methods and user preferences.
    let config = FileManager.default.temporaryDirectory.appendingPathComponent("tailkana-preview-\(ProcessInfo.processInfo.processIdentifier)")
    try? FileManager.default.createDirectory(at: config.appendingPathComponent("fcitx5"), withIntermediateDirectories: true)
    let profile = "[Groups/0]\nName=TailKana\nDefault Layout=us\nDefaultIM=tailkana\n[Groups/0/Items/0]\nName=tailkana\n[Groups/0/Items/1]\nName=keyboard-us\n[GroupOrder]\n0=TailKana\n"
    try? profile.write(to: config.appendingPathComponent("fcitx5/profile"), atomically: true, encoding: .utf8)
    setenv("TAILKANA_PREVIEW_CAPTURE", config.path, 1)
    setenv("XDG_CONFIG_HOME", config.path, 1)
    setenv("TAILKANA_MOZC_PROFILE", config.appendingPathComponent("mozc").path, 1)
    try? "[Behavior]\nActiveByDefault=True\n".write(to: config.appendingPathComponent("fcitx5/config"), atomically: true, encoding: .utf8)
    start_fcitx_thread("zh_CN")
    _ = get_current_group_layout()
    window = NSWindow(contentRect: NSRect(x: 0,y: 0,width: 820,height: 460), styleMask: [.titled,.closable,.miniaturizable,.resizable], backing: .buffered, defer: false)
    window.title = "TailKana 试打"
    window.delegate = self
    let scroll = NSScrollView(frame: window.contentView!.bounds)
    scroll.autoresizingMask = [.width,.height]
    scroll.hasVerticalScroller = true
    let text = TailKanaTextView(frame: scroll.bounds)
    text.font = .systemFont(ofSize: 24)
    text.textContainerInset = NSSize(width: 28,height: 28)
    text.isRichText = false
    text.isAutomaticQuoteSubstitutionEnabled = false
    text.isAutomaticSpellingCorrectionEnabled = false
    text.isAutomaticTextReplacementEnabled = false
    text.autoresizingMask = [.width,.height]
    scroll.documentView = text
    window.contentView!.addSubview(scroll)
    setPreviewTextView(text)
    if let option = CommandLine.arguments.firstIndex(of: "--self-test"), option + 1 < CommandLine.arguments.count {
      window.makeFirstResponder(text)
      text.selfTest(CommandLine.arguments[option + 1])
      NSApp.terminate(nil)
      return
    }
    window.center()
    window.makeKeyAndOrderFront(nil)
    window.makeFirstResponder(text)
    NSApp.activate(ignoringOtherApps: true)
  }
  @objc func showSettings() { openTailKanaSettings() }
  func windowWillClose(_ notification: Notification) { NSApp.terminate(nil) }
  func applicationWillTerminate(_ notification: Notification) { stop_fcitx_thread() }
  func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}
@main
struct TailKanaPreview {
  @MainActor static func main() {
    let app = NSApplication.shared
    app.setActivationPolicy(.regular)
    let delegate = PreviewDelegate()
    app.delegate = delegate
    app.run()
  }
}
