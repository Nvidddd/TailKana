// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Carbon

@MainActor final class LauncherDelegate: NSObject, NSApplicationDelegate {
  let inputApp = URL(fileURLWithPath: "/Library/Input Methods/TailKanaInput.app")
  var window: NSWindow!
  let status = NSTextField(wrappingLabelWithString: "")
  func applicationDidFinishLaunching(_ notification: Notification) {
    let menu = NSMenu(); let item = NSMenuItem(); let appMenu = NSMenu(title: "TailKana")
    appMenu.addItem(withTitle: "关闭控制面板", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
    item.submenu = appMenu; menu.addItem(item); NSApp.mainMenu = menu
    window = NSWindow(contentRect: NSRect(x: 0,y: 0,width: 440,height: 305),styleMask: [.titled,.closable,.miniaturizable],backing: .buffered,defer: false)
    window.title = "TailKana"
    let heading = NSTextField(labelWithString: "TailKana"); heading.font = .systemFont(ofSize: 28,weight: .semibold)
    let detail = NSTextField(wrappingLabelWithString: "本地日语输入法\n三个短候选 · 假名标注 · 末尾拼写轮盘")
    detail.font = .systemFont(ofSize: 13); detail.textColor = .secondaryLabelColor
    status.font = .systemFont(ofSize: 12); status.textColor = .secondaryLabelColor
    let enable = NSButton(title: "启用并切换",target: self,action: #selector(enableInput)); enable.bezelStyle = .rounded
    let settings = NSButton(title: "打开设置",target: self,action: #selector(settings)); settings.bezelStyle = .rounded
    let keyboard = NSButton(title: "系统输入源…",target: self,action: #selector(keyboardSettings)); keyboard.bezelStyle = .rounded
    let row = NSStackView(views: [enable,settings,keyboard]); row.orientation = .horizontal; row.spacing = 10
    let tip = NSTextField(wrappingLabelWithString: "输入法由系统在后台运行。\n关闭控制面板后，仍可从系统输入菜单切换。")
    tip.font = .systemFont(ofSize: 11);tip.textColor = .secondaryLabelColor
    let stack = NSStackView(views: [heading,detail,status,row,tip]);stack.orientation = .vertical;stack.alignment = .leading;stack.spacing = 18
    stack.translatesAutoresizingMaskIntoConstraints = false;window.contentView!.addSubview(stack)
    NSLayoutConstraint.activate([stack.leadingAnchor.constraint(equalTo:window.contentView!.leadingAnchor,constant:24),stack.trailingAnchor.constraint(equalTo:window.contentView!.trailingAnchor,constant:-24),stack.topAnchor.constraint(equalTo:window.contentView!.topAnchor,constant:24)])
    let installed = FileManager.default.fileExists(atPath:inputApp.path)
    status.stringValue = installed ? "已安装。点击启用后即可开始输入。" : "请先安装 TailKana 安装包。"
    enable.isEnabled = installed;settings.isEnabled = installed
    window.center();window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
  }
  func prepareProfile() throws {
    let config = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/TailKana/Config/fcitx5")
    try FileManager.default.createDirectory(at:config,withIntermediateDirectories:true)
    let files = ["profile":"[Groups/0]\nName=TailKana\nDefault Layout=us\nDefaultIM=tailkana\n[Groups/0/Items/0]\nName=tailkana\n[Groups/0/Items/1]\nName=keyboard-us\n[GroupOrder]\n0=TailKana\n", "config":"[Behavior]\nActiveByDefault=True\n"]
    for (name,text) in files where !FileManager.default.fileExists(atPath:config.appendingPathComponent(name).path) {
      try text.write(to:config.appendingPathComponent(name),atomically:true,encoding:.utf8)
    }
  }
  @objc func enableInput() {
    do {try prepareProfile()} catch {status.stringValue = "无法保存配置：\(error.localizedDescription)";return}
    do {
      try TailKanaRegistration.registerAndSelect()
      status.stringValue = "已请求切换到 TailKana。请在输入框试打确认。"
    } catch {status.stringValue = error.localizedDescription}
  }

  @objc func settings() {
    do {try prepareProfile()} catch {status.stringValue = error.localizedDescription;return}
    NSWorkspace.shared.openApplication(at: inputApp,configuration:NSWorkspace.OpenConfiguration()) { _,error in
      DispatchQueue.main.async {
        if let error {self.status.stringValue = error.localizedDescription;return}
        DispatchQueue.main.asyncAfter(deadline:.now()+0.8) {
          DistributedNotificationCenter.default().postNotificationName(Notification.Name("org.tailkana.open-settings"),object:nil,userInfo:nil,deliverImmediately:true)
        }
      }
    }
  }
  @objc func keyboardSettings() {NSWorkspace.shared.open(URL(string:"x-apple.systempreferences:com.apple.Keyboard-Settings.extension")!)}
  func applicationShouldTerminateAfterLastWindowClosed(_ sender:NSApplication) -> Bool {true}
}
@main struct TailKanaLauncher {
  @MainActor static func main() {let app = NSApplication.shared;app.setActivationPolicy(.regular);let delegate = LauncherDelegate();app.delegate = delegate;app.run()}
}
