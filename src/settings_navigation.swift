// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

enum TailKanaSettingsPage: String, CaseIterable, Identifiable {
  case general, fuzzy, appearance, management, about
  var id:String {rawValue}
  var title:String {
    switch self {
    case .general: "输入与候选"
    case .fuzzy: "模糊拼音"
    case .appearance: "外观"
    case .management: "管理与高级"
    case .about: "关于"
    }
  }
  var symbol:String {
    switch self {
    case .general: "keyboard"
    case .fuzzy: "textformat.abc"
    case .appearance: "paintpalette"
    case .management: "slider.horizontal.3"
    case .about: "info.circle"
    }
  }
}
@MainActor final class TailKanaSettingsNavigation: ObservableObject {
  static let shared=TailKanaSettingsNavigation()
  @Published var page:TailKanaSettingsPage = .general
}
struct TailKanaManagementDestination: Identifiable {
  let id:String
  let title:String
  let detail:String
  static let all:[Self]=[
    .init(id:"inputMethods",title:"输入法管理",detail:"调整可用输入法与排列顺序"),
    .init(id:"global",title:"全局配置",detail:"调整全局快捷键与输入行为"),
    .init(id:"plugins",title:"插件管理",detail:"查看和管理输入法插件"),
    .init(id:"advanced",title:"其他高级配置",detail:"打开扩展选项与底层配置")]
}
