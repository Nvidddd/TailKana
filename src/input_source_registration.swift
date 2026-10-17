// SPDX-License-Identifier: GPL-3.0-or-later
import Carbon
import Foundation

/// Registration and parent enabling use the vendored Fcitx5 installer helpers.
/// Never writes HIToolbox preferences, disables sources, or resets system agents.
enum TailKanaRegistration {
  static let bundleID = "org.tailkana.inputmethod.TailKana"
  static let parentID = bundleID
  static let modeID = bundleID + ".japanese"

  private static func failure(_ text: String, _ code: Int = 1) -> NSError {
    NSError(domain: "TailKanaRegistration", code: code,
            userInfo: [NSLocalizedDescriptionKey: text])
  }
  private static func runHelper(_ name: String) throws {
    let helper = Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/" + name)
    guard FileManager.default.isExecutableFile(atPath: helper.path) else {
      throw failure("安装不完整，请重新安装 TailKana。")
    }
    let process = Process()
    process.executableURL = helper
    try process.run()
    process.waitUntilExit()
    guard process.terminationStatus == 0 else {
      throw failure("系统未完成输入源登记，请注销并重新登录后添加 TailKana。")
    }
  }
  private static func property(_ source: TISInputSource, _ key: CFString) -> String? {
    guard let pointer = TISGetInputSourceProperty(source,key) else {return nil}
    return Unmanaged<CFString>.fromOpaque(pointer).takeUnretainedValue() as String
  }
  static func registerAndSelect() throws {
    try runHelper("register_im")
    try runHelper("enable_im")
    let conditions = [kTISPropertyBundleID as String: bundleID] as CFDictionary
    let sources = TISCreateInputSourceList(conditions, true)?.takeRetainedValue() as? [TISInputSource] ?? []
    guard let source = sources.first(where: {property($0,kTISPropertyInputSourceID) == modeID}) else {
      throw failure("系统尚未载入 TailKana。请注销并重新登录，再在系统输入源中添加。")
    }
    let status = TISSelectInputSource(source)
    guard status == noErr else {
      throw failure("系统未切换输入源（\(status)）。请在系统输入源中添加 TailKana；必要时注销并重新登录。",Int(status))
    }
    // Selecting is only a request; it is not evidence of successful text input.
    guard let current = TISCopyCurrentKeyboardInputSource()?.takeRetainedValue(),
          property(current,kTISPropertyInputSourceID) == modeID else {
      throw failure("系统没有保持 TailKana 选择，请从系统输入菜单切换。")
    }
  }
}
