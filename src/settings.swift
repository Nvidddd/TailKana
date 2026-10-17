// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

public let tailKanaSettingsChangedNotification = Notification.Name("org.tailkana.settings-changed")
@MainActor public func notifyTailKanaSettingsChanged() {
  NotificationCenter.default.post(name: tailKanaSettingsChangedNotification,object: nil)
  ConfigWindowController.refreshAll()
}

public final class TailKanaSettingsController: ConfigWindowController {
  convenience init() {
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 760, height: 620), styleMask: [.titled, .closable, .resizable], backing: .buffered, defer: false)
    window.minSize=NSSize(width:660,height:520)
    window.title = "TailKana 设置"
    window.center()
    self.init(window: window)
    window.contentView = NSHostingView(rootView: TailKanaSettingsSurface(content: TailKanaSettingsView()))
  }
  public override func windowShouldClose(_ sender: NSWindow) -> Bool {
    let result = super.windowShouldClose(sender)
    if Bundle.main.bundleIdentifier == "org.tailkana.preview" { NSApp.setActivationPolicy(.regular) }
    return result
  }
}
@MainActor public func openTailKanaSettings() {
  ConfigWindowController.openWindow("tailkana", TailKanaSettingsController.self)
}
@MainActor public func openTailKanaSettingsSection(_ section:String) {
  TailKanaSettingsNavigation.shared.page=TailKanaSettingsPage(rawValue:section) ?? .general
  openTailKanaSettings()
}
@MainActor public func openTailKanaManagementPage(_ page:String) {
  switch page {
  case "inputMethods": ConfigWindowController.openWindow("im",InputMethodConfigController.self)
  case "global": ConfigWindowController.openWindow("global",GlobalConfigController.self)
  case "plugins": ConfigWindowController.openWindow("plugin",PluginManager.self)
  case "advanced": ConfigWindowController.openWindow("advanced",AdvancedController.self)
  default: break
  }
}
@_cdecl("tailkana_open_settings")
public func tailkanaOpenSettingsBridge() {
  DispatchQueue.main.async { openTailKanaSettings() }
}
struct TailKanaSettingsView: View {
  @ObservedObject private var navigation=TailKanaSettingsNavigation.shared
  static let pinyinRuleGroups=[
    ("PinyinXi","xi → し"),("PinyinQi","qi → ち"),("PinyinCi","ci → つ"),
    ("PinyinSi","si → す"),("PinyinZi","zi → ず"),
    ("PinyinXGlides","x 系拗音 · xia / xiu / xiao / xue"),
    ("PinyinQGlides","q 系拗音 · qia / qiu / qiao / qie"),
    ("PinyinJGlides","j 系拗音 · jia / jiu / jiao / jie")]

  @StateObject private var manager = ConfigManager()
  private func value(_ key: String) -> String { (manager.value as? [String: Any])?[key] as? String ?? "" }
  private func update(_ key: String, _ value: String) {
    var settings = manager.value as? [String: Any] ?? [:]
    settings[key] = value
    manager.set(settings)
    notifyTailKanaSettingsChanged()
  }
  private func toggle(_ key: String) -> Binding<Bool> {
    Binding(get: { value(key) == "True" }, set: { update(key, $0 ? "True" : "False") })
  }
  var body: some View {
    VStack(spacing:0) {
      HStack(spacing:0) {
        VStack(alignment:.leading,spacing:5) {
          Text("TailKana").font(.headline).padding(.horizontal,10).padding(.bottom,12)
          ForEach(TailKanaSettingsPage.allCases) {page in
            Button {navigation.page=page} label: {
              HStack(spacing:9) {
                Image(systemName:page.symbol).frame(width:18)
                Text(page.title)
                Spacer(minLength:0)
              }.padding(.horizontal,10).padding(.vertical,9)
                .background {RoundedRectangle(cornerRadius:8).fill(.tint).opacity(navigation.page==page ? 0.16:0)}
                .contentShape(Rectangle())
            }.buttonStyle(.plain).accessibilityLabel("设置分组："+page.title)
          }
          Spacer()
        }.padding(12).frame(width:160)
        Divider()
        VStack(alignment:.leading,spacing:0) {
          Text(navigation.page.title).font(.title2.weight(.semibold)).padding(.horizontal,22).padding(.top,20).padding(.bottom,8)
          pageContent.frame(maxWidth:.infinity,maxHeight:.infinity)
        }
      }
      Divider()
      HStack {
        if navigation.page == .general || navigation.page == .fuzzy {
          Button("恢复默认") {manager.reset();notifyTailKanaSettingsChanged()}
        }
        Spacer()
        Text(manager.error ?? "更改自动保存").font(.caption).foregroundStyle(.secondary).lineLimit(2)
        Button("完成") {ConfigWindowController.closeWindow("tailkana")}.keyboardShortcut(.defaultAction)
      }.padding(14)
    }.onAppear {manager.uri="fcitx://config/addon/tailkana"}
      .onReceive(NotificationCenter.default.publisher(for:tailKanaSettingsChangedNotification)) {_ in manager.reload()}
  }
  @ViewBuilder private var pageContent:some View {
    switch navigation.page {
    case .general: generalForm
    case .fuzzy: fuzzyForm
    case .appearance: ScrollView {TailKanaThemeView()}.background(.clear)
    case .management: managementForm
    case .about: aboutPage
    }
  }
  private var generalForm:some View {
    Form {
        Section("输入") {
          Picker("默认输入方式",selection:Binding(get:{value("DefaultInputMode").isEmpty ? "Japanese":value("DefaultInputMode")},set:{update("DefaultInputMode",$0)})) {
            Text("日文输入").tag("Japanese")
            Text("英文直输").tag("English")
          }
          Text("下次进入输入框或切回 TailKana 时使用；当前输入框仍可用 Shift＋Tab 临时切换。应用专用输入规则优先。")
            .font(.caption).foregroundStyle(.secondary)
          Picker("输入处显示", selection: toggle("ShowCandidate")) {
            Text("罗马字").tag(false)
            Text("实时假名").tag(true)
          }
          Picker("默认假名", selection: toggle("Katakana")) {
            Text("平假名 · あ").tag(false)
            Text("片假名 · ア").tag(true)
          }
        }
        Section("AI 上下文推荐") {
          Toggle("启用实时 AI 推荐",isOn:Binding(get:{value("AiEnabled") != "False"},set:{update("AiEnabled",$0 ? "True":"False")}))
          Text("完全离线。开启后预热并常驻模型，约占 250 MB；关闭后释放。AI 单独显示，不改变 1／2／3 的候选顺序。")
            .font(.caption).foregroundStyle(.secondary)
          SwiftUI.Group {
            Stepper(value:Binding(get:{Int(value("AiMaxLength")) ?? 32},set:{update("AiMaxLength",String($0))}),in:1...128) {
              HStack {
                Text("最长预测长度");Spacer()
                TextField("最长预测字符数",value:Binding(get:{Int(value("AiMaxLength")) ?? 32},set:{update("AiMaxLength",String(min(128,max(1,$0))))}),format:.number).labelsHidden().frame(width:52)
                Text("字符")
              }
            }
            Text("只推荐长度不超过此值的完整候选，不截断词句。")
              .font(.caption).foregroundStyle(.secondary)
            Stepper(value:Binding(get:{Int(value("AiContextLength")) ?? 40},set:{update("AiContextLength",String($0))}),in:0...128) {
              HStack {
                Text("参考上下文：光标前");Spacer()
                TextField("参考上下文字符数",value:Binding(get:{Int(value("AiContextLength")) ?? 40},set:{update("AiContextLength",String(min(128,max(0,$0))))}),format:.number).labelsHidden().frame(width:52)
                Text("字符")
              }
            }
            Text("设为 0 时仅参考当前读音。点击 AI 推荐，或按 Option＋Enter 确认。")
              .font(.caption).foregroundStyle(.secondary)
          }.disabled(value("AiEnabled") == "False")
        }
        Section("候选面板") {
          Toggle("键盘误按纠错", isOn: Binding(get: {value("Corrections") != "False"},set: {update("Corrections",$0 ? "True":"False")}))
          Text("常见误拼补充带标记的纠错候选，选中后确认；保留原输入。")
            .font(.caption).foregroundStyle(.secondary)
          Toggle("右侧显示中文释义", isOn: toggle("ShowMeaning"))
          Toggle("智能中文第三候选", isOn: toggle("SmartChinese"))
          Text("同一串字母同时查询拼音；第三格保留中文候选，按 3 提交。")
            .font(.caption).foregroundStyle(.secondary)
          Toggle("给汉字标注假名", isOn: toggle("Furigana"))
          HStack {
            Text("候选框宽度")
            Slider(value:Binding(get:{Double(value("PanelWidth")) ?? 520},set:{update("PanelWidth",String(Int($0.rounded())))}),in:360...1000,step:20)
            TextField("候选框宽度",value:Binding(get:{Int(value("PanelWidth")) ?? 520},set:{update("PanelWidth",String(min(1000,max(360,$0))))}),format:.number).labelsHidden().frame(width:56)
            Text("px")
          }
          Text("宽度按 100% 缩放计算；右侧轮盘保持原大小，候选区随宽度调整。")
            .font(.caption).foregroundStyle(.secondary)
          HStack {
            Text("大小")
            Slider(value: Binding(get: { Double(value("PanelScale")) ?? 100 }, set: { update("PanelScale", String(Int($0.rounded()))) }), in: 80...120, step: 5)
            Text("\(value("PanelScale"))%").monospacedDigit().frame(width: 42)
          }
        }
        Section("键盘") {
          Picker("标点",selection:Binding(get:{value("Punctuation").isEmpty ? "Auto":value("Punctuation")},set:{update("Punctuation",$0)})) {
            Text("自动").tag("Auto");Text("全角").tag("Full");Text("半角").tag("Half")
          }
          Text("自动：英文半角，日文全角。Shift＋Tab 切换英文／日文。")
            .font(.caption).foregroundStyle(.secondary)
          Toggle("空格确定，Enter 变换", isOn: toggle("SwapKeys"))
          Text(value("SwapKeys") == "True" ? "连续按 Enter 轮换候选；空格提交。" : "连续按空格轮换候选；Enter 提交。")
            .font(.caption).foregroundStyle(.secondary)
          Toggle("按住左 Shift 临时切换假名", isOn: toggle("LeftShift"))
          Toggle("右 Shift ＋方向键修改末尾", isOn: toggle("RightShift"))
          Text("Tab 展开候选 · 1 / 2 / 3 选择 · Esc 取消")
            .font(.caption).foregroundStyle(.secondary)
        }
    }.formStyle(.grouped).scrollContentBackground(.hidden).background(.clear)
  }
  private var fuzzyForm:some View {
    Form {
        Section("模糊拼音 · B") {
          Toggle("启用模糊拼音", isOn: Binding(get: {value("PinyinCorrections") != "False"},set: {update("PinyinCorrections",$0 ? "True":"False")}))
          Text("勾选适合自己的拼写。原拼写保留，新增候选在数字下面标 B。")
            .font(.caption).foregroundStyle(.secondary)
          SwiftUI.Group {
            ForEach(Array(Self.pinyinRuleGroups.enumerated()),id: \.offset) {_,rule in
              Toggle(rule.1,isOn:Binding(get:{value(rule.0) != "False"},set:{update(rule.0,$0 ? "True":"False")}))
            }
          }.disabled(value("PinyinCorrections") == "False")
        }
    }.formStyle(.grouped).scrollContentBackground(.hidden).background(.clear)
  }
  private var managementForm:some View {
    Form {
      Section("管理") {
        ForEach(TailKanaManagementDestination.all) {destination in
          Button {openTailKanaManagementPage(destination.id)} label: {
            HStack {
              VStack(alignment:.leading,spacing:4) {
                Text(destination.title)
                Text(destination.detail).font(.caption).foregroundStyle(.secondary)
              }
              Spacer()
              Image(systemName:"arrow.up.right").font(.caption).foregroundStyle(.secondary)
            }.padding(.vertical,4).contentShape(Rectangle())
          }.buttonStyle(.plain)
        }
      }
    }.formStyle(.grouped).scrollContentBackground(.hidden).background(.clear)
  }
  private var aboutPage:some View {
    VStack(alignment:.leading,spacing:16) {
      HStack(spacing:16) {
        Image(nsImage:NSApp.applicationIconImage).resizable().frame(width:72,height:72)
        VStack(alignment:.leading,spacing:4) {
          Text("TailKana").font(.title.weight(.semibold))
          Text("版本 \(Bundle.main.object(forInfoDictionaryKey:"CFBundleShortVersionString") as? String ?? "开发版")").foregroundStyle(.secondary)
        }
      }
      Text("本地日语输入 · 中文释义 · 模糊拼音 · 末尾拼写轮盘")
      Text("基于 Fcitx5、Mozc 与开源纠错规则。")
        .font(.callout).foregroundStyle(.secondary)
      Text("GPL-3.0-or-later · 包含其他开源许可组件")
        .font(.caption).foregroundStyle(.secondary)
      Button("查看开源许可…") {
        if let resources=Bundle.main.resourceURL {NSWorkspace.shared.open(resources.appendingPathComponent("TailKana-Licenses"))}
      }
      Spacer()
    }.padding(22).frame(maxWidth:.infinity,alignment:.leading)
  }
}

@MainActor public func openTailKanaThemeSettings() {
  openTailKanaSettingsSection("appearance")
}
private struct TailKanaThemeView: View {
  @StateObject private var manager = ConfigManager()
  @Environment(\.colorScheme) private var systemScheme
  private var appearance: String {(manager.value as? [String: Any])?["Appearance"] as? String ?? "System"}
  private func update(_ key: String, _ value: String) {
    var options = manager.value as? [String: Any] ?? [:]
    options[key] = value;manager.set(options);notifyTailKanaSettingsChanged()
  }
  private var glass: Bool {(manager.value as? [String: Any])?["Glass"] as? String != "False"}
  private var custom: Bool {(manager.value as? [String: Any])?["CustomColors"] as? String == "True"}
  private func colorBinding(_ key: String, _ fallback: UInt32) -> Binding<Color> {
    Binding(get: {
      let text=(manager.value as? [String: Any])?[key] as? String ?? ""
      let rgb=UInt32(text.trimmingCharacters(in: CharacterSet(charactersIn: "#")),radix:16) ?? fallback
      return Color(red:Double((rgb>>16)&255)/255,green:Double((rgb>>8)&255)/255,blue:Double(rgb&255)/255)
    },set: {color in
      guard let c=NSColor(color).usingColorSpace(.deviceRGB) else {return}
      update(key,String(format:"#%02x%02x%02x",Int((c.redComponent*255).rounded()),Int((c.greenComponent*255).rounded()),Int((c.blueComponent*255).rounded())))
    })
  }
  private var dark: Bool {appearance == "Dark" || (appearance == "System" && systemScheme == .dark)}
  var body: some View {
    VStack(alignment: .leading, spacing: 14) {
      HStack {
        Text("颜色与透明度").font(.title2.weight(.semibold))
        Spacer()
        Text("紧凑候选与末尾轮盘").font(.caption).foregroundStyle(.secondary)
      }
      Picker("明暗", selection: Binding(get: {appearance}, set: {newValue in
        var options = manager.value as? [String: Any] ?? [:]
        options["Appearance"] = newValue
        manager.set(options)
        notifyTailKanaSettingsChanged()
      })) {
        Text("跟随系统").tag("System")
        Text("浅色").tag("Light")
        Text("深色").tag("Dark")
      }.pickerStyle(.segmented)
      TailKanaThemePreview(dark: dark, glass: glass, custom: custom, background: colorBinding("BackgroundColor",0x202020).wrappedValue, text: colorBinding("TextColor",0xedf3ef).wrappedValue, highlight: colorBinding("HighlightColor",0x355b47).wrappedValue, opacity: (Double((manager.value as? [String: Any])?["TintOpacity"] as? String ?? "20") ?? 20) / 100).environment(\.colorScheme,dark ? .dark : .light)
      Toggle("透明毛玻璃", isOn: Binding(get: {glass},set: {update("Glass",$0 ? "True":"False")}))
      Text("候选面板与设置窗口同时生效；关闭后使用不透明底色。")
        .font(.caption).foregroundStyle(.secondary)
      Toggle("自定义颜色", isOn: Binding(get: {custom},set: {update("CustomColors",$0 ? "True":"False")}))
      if custom {
        HStack {
          ColorPicker("背景",selection:colorBinding("BackgroundColor",0x202020),supportsOpacity:false)
          ColorPicker("文字",selection:colorBinding("TextColor",0xedf3ef),supportsOpacity:false)
          ColorPicker("高亮",selection:colorBinding("HighlightColor",0x355b47),supportsOpacity:false)
        }
      }
      HStack {
        Text("玻璃底色浓度").font(.caption)
        Slider(value:Binding(get: {Double((manager.value as? [String: Any])?["TintOpacity"] as? String ?? "20") ?? 20},set: {update("TintOpacity",String(Int($0.rounded())))}),in:0...100,step:1)
        Text("\(Int((Double((manager.value as? [String: Any])?["TintOpacity"] as? String ?? "20") ?? 20).rounded()))%")
          .monospacedDigit().frame(width: 42)
      }
      .disabled(!glass)
      Text("浓度越低，越能透出后方窗口；0% 仅保留原生模糊。配色立即保存。")
        .font(.caption).foregroundStyle(.secondary)
      HStack {
        Button("恢复默认配色") {
          var options=manager.value as? [String: Any] ?? [:]
          options["CustomColors"]="False";options["Glass"]="True";options["TintOpacity"]="20"
          options["BackgroundColor"]="#202020";options["TextColor"]="#edf3ef";options["HighlightColor"]="#355b47"
          manager.set(options);notifyTailKanaSettingsChanged()
        }
        Text(manager.error ?? "预览示例 · 不影响正在输入的内容").font(.caption).foregroundStyle(.secondary)
        Spacer()

      }
    }.padding(22).onAppear {manager.uri = "fcitx://config/addon/tailkana"}
      .onReceive(NotificationCenter.default.publisher(for: tailKanaSettingsChangedNotification)) {_ in manager.reload()}
  }
}
private struct TailKanaThemePreview: View {
  let dark: Bool
  let glass: Bool
  let custom: Bool
  let background: Color
  let text: Color
  let highlight: Color
  let opacity: Double
  private func color(_ light: UInt32, _ darkValue: UInt32) -> Color {
    let rgb = dark ? darkValue : light
    return Color(red: Double((rgb >> 16) & 255)/255, green: Double((rgb >> 8) & 255)/255, blue: Double(rgb & 255)/255)
  }
  private var ink: Color {custom ? text : color(0x1d3d2f,0xe6f2e9)}
  private var muted: Color {custom ? text.opacity(0.8) : color(0x526b61,0xb4cebf)}
  private var selected: Color {custom ? highlight : color(0xdceee2,0x355b47)}
  var body: some View {
    HStack(spacing: 10) {
      VStack(spacing: 6) {
        HStack(spacing: 4) {
          ForEach(0..<3) {index in
            HStack(spacing: 5) {
              Text("\(index + 1)").font(.system(size: 10)).foregroundStyle(muted)
              VStack(spacing: 0) {
                Text(index == 1 ? " " : "ち").font(.system(size: 10)).foregroundStyle(muted)
                Text(["血","ち","地"][index]).font(.system(size: 22))
              }
            }.frame(maxWidth: .infinity).padding(.vertical, 5)
              .background(index == 0 ? selected : .clear, in: RoundedRectangle(cornerRadius: 7))
          }
          Text("＋").foregroundStyle(muted)
        }
        Rectangle().fill(custom ? text.opacity(0.22) : color(0xc5d6c9,0x405a4b)).frame(height: 1)
        HStack {
          Text("Tab 展开");Spacer();Text("空格 变换");Spacer();Text("Enter 确定")
        }.font(.system(size: 9)).foregroundStyle(muted)
      }
      Rectangle().fill(custom ? text.opacity(0.22) : color(0xc5d6c9,0x405a4b)).frame(width: 1)
      VStack(spacing: 1) {
        HStack {Text("02").font(.system(size: 9));Spacer();kana("き","ki");Spacer();Text("⇧ →").font(.system(size: 8))}
        HStack {kana("くぃ","qi");Spacer();kana("ち","ti").frame(width: 33).padding(.vertical, 3).background(selected,in: RoundedRectangle(cornerRadius: 6));Spacer();kana("ちょ","tyo")}
        HStack {Text("罗马字").font(.system(size: 7));Spacer();kana("ちゅ","tyu");Spacer();Text("あ").font(.system(size: 12))}
      }.frame(width: 112).foregroundStyle(muted)
    }.foregroundStyle(ink).padding(10).frame(height: 100)
      .background((custom ? background : color(0xf7f8f7,0x1c1c1c)).opacity(glass ? opacity : 1),in: RoundedRectangle(cornerRadius: 12))
      .background {
        if glass { RoundedRectangle(cornerRadius: 12).fill(.ultraThinMaterial) }
      }
      .overlay(RoundedRectangle(cornerRadius: 12).stroke(custom ? text.opacity(0.3) : color(0xb7cbbd,0x4c6859),lineWidth: 1))
  }
  private func kana(_ text: String, _ roman: String) -> some View {
    VStack(spacing: 0) {Text(text).font(.system(size: 12)).foregroundStyle(ink);Text(roman).font(.system(size: 7))}
  }
}

@_cdecl("tailkana_open_dictionary")
public func tailkanaOpenDictionary(_ pointer: UnsafePointer<CChar>) {
  let word=String(cString:pointer)
  DispatchQueue.main.async {
    var allowed=CharacterSet.urlPathAllowed;allowed.remove(charactersIn:"/#?%")
    if let escaped=word.addingPercentEncoding(withAllowedCharacters:allowed), let url=URL(string:"https://zh.wiktionary.org/wiki/\(escaped)#日語") {NSWorkspace.shared.open(url)}
  }
}

@_cdecl("tailkana_preferences_changed")
public func tailkanaPreferencesChanged() {
  DispatchQueue.main.async {notifyTailKanaSettingsChanged()}
}

// Shared by all product configuration windows, including advanced settings.
@MainActor func tailKanaThemeOptions() -> [String:String] {
  let children=getConfig("fcitx://config/addon/tailkana")["Children"] as? [[String:Any]] ?? []
  return Dictionary(children.compactMap {child in
    guard let key=child["Option"] as? String, let value=child["Value"] as? String else {return nil}
    return (key,value)
  },uniquingKeysWith: {_,new in new})
}
@MainActor public func installTailKanaSettingsGlass(_ window: NSWindow) {
  guard Bundle.main.bundleIdentifier?.hasPrefix("org.tailkana.") == true else {return}
  applyTailKanaWindowTheme(window,options:tailKanaThemeOptions())
}
