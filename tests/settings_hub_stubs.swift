import AppKit
import SwiftUI

@MainActor enum SettingsFixture {
  static var options:[String:String]=[
    "Appearance":"Dark","Glass":"True","TintOpacity":"20","CustomColors":"False",
    "BackgroundColor":"#202020","TextColor":"#edf3ef","HighlightColor":"#355b47",
    "DefaultInputMode":"Japanese","PanelWidth":"520","ShowCandidate":"True","Katakana":"False","Corrections":"True","SmartChinese":"True",
    "Furigana":"True","PanelScale":"100","Punctuation":"Auto","SwapKeys":"False",
    "LeftShift":"True","RightShift":"True","PinyinCorrections":"True"]
}
@MainActor func getConfig(_ uri:String)->[String:Any] {
  ["Children":SettingsFixture.options.map{["Option":$0.key,"Value":$0.value]}]
}
@MainActor class ConfigManager:ObservableObject {
  @Published var value:Any=SettingsFixture.options
  @Published var error:String?=nil
  var uri="" {didSet {reload()}}
  func set(_ settings:Any) {value=settings;if let settings=settings as? [String:String] {SettingsFixture.options=settings}}
  func reload() {value=SettingsFixture.options}
  func reset() {reload()}
}
@MainActor public class ConfigWindowController:NSWindowController {
  static var opened:[String]=[]
  public static func openWindow(_ key:String,_ type:ConfigWindowController.Type) {opened.append(key)}
  public static func closeWindow(_ key:String) {}
  public static func refreshAll() {for window in NSApp.windows {applyTailKanaWindowTheme(window,options:SettingsFixture.options)}}
  public func windowShouldClose(_ sender:NSWindow)->Bool {true}
}
public final class InputMethodConfigController:ConfigWindowController {}
public final class GlobalConfigController:ConfigWindowController {}
public final class PluginManager:ConfigWindowController {}
public final class AdvancedController:ConfigWindowController {}
