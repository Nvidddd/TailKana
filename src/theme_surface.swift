// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import SwiftUI

struct TailKanaSurfaceStyle {
  let options: [String:String]
  var glass: Bool {options["Glass"] != "False"}
  var custom: Bool {options["CustomColors"] == "True"}
  var opacity: Double {glass ? min(100,max(0,Double(options["TintOpacity"] ?? "20") ?? 20))/100 : 1}
  var appearance: NSAppearance? {
    switch options["Appearance"] {
    case "Dark": NSAppearance(named:.darkAqua)
    case "Light": NSAppearance(named:.aqua)
    default: nil
    }
  }
  func color(_ key: String, fallback: NSColor) -> NSColor {
    guard custom, let hex=options[key], hex.count==7, hex.first=="#",
          let rgb=UInt32(hex.dropFirst(),radix:16) else {return fallback}
    return NSColor(srgbRed:Double((rgb>>16)&255)/255,green:Double((rgb>>8)&255)/255,blue:Double(rgb&255)/255,alpha:1)
  }
  private func adaptive(_ light:UInt32,_ dark:UInt32)->NSColor {
    NSColor(name:nil) {appearance in
      let rgb=appearance.bestMatch(from:[.darkAqua,.aqua]) == .darkAqua ? dark:light
      return NSColor(srgbRed:Double((rgb>>16)&255)/255,green:Double((rgb>>8)&255)/255,blue:Double(rgb&255)/255,alpha:1)
    }
  }
  // Same default palette as the candidate panel; dynamic colors also follow
  // system appearance when the user has not pinned Light or Dark.
  var background:NSColor {color("BackgroundColor",fallback:adaptive(0xf7f8f7,0x1c1c1c))}
  var ink:NSColor {color("TextColor",fallback:adaptive(0x1d3d2f,0xe6f2e9))}
  var accent:NSColor {color("HighlightColor",fallback:adaptive(0x387954,0x9ecfb3))}
}

// A color layer above the native effect, below every hosting view. This view
// resolves dynamic colors again when macOS changes appearance.
private final class TailKanaTintView: NSView {
  var style=TailKanaSurfaceStyle(options:[:]) {didSet {needsDisplay=true}}
  override var isOpaque: Bool {!style.glass}
  override func draw(_ dirtyRect: NSRect) {
    style.background.withAlphaComponent(style.opacity).setFill()
    dirtyRect.fill()
  }
  override func viewDidChangeEffectiveAppearance() {
    super.viewDidChangeEffectiveAppearance();needsDisplay=true
  }
}

@MainActor func applyTailKanaWindowTheme(_ window: NSWindow,options:[String:String]) {
  let style=TailKanaSurfaceStyle(options:options)
  window.isOpaque = !style.glass
  window.appearance=style.appearance
  window.backgroundColor = style.glass ? .clear : style.background
  window.titlebarAppearsTransparent=true
  window.styleMask.insert(.fullSizeContentView)
  let effect: NSVisualEffectView
  if let existing=window.contentView as? NSVisualEffectView,
     existing.identifier?.rawValue == "tailkana-settings-glass" {
    effect=existing
  } else {
    guard let content=window.contentView else {return}
    effect=NSVisualEffectView(frame:content.bounds)
    effect.identifier=NSUserInterfaceItemIdentifier("tailkana-settings-glass")
    effect.material = .underWindowBackground
    effect.blendingMode = .behindWindow
    window.contentView=effect
    let tint=TailKanaTintView(frame:effect.bounds)
    tint.identifier=NSUserInterfaceItemIdentifier("tailkana-settings-tint")
    tint.autoresizingMask=[.width,.height]
    effect.addSubview(tint)
    content.translatesAutoresizingMaskIntoConstraints=false
    content.wantsLayer=true
    content.layer?.backgroundColor=NSColor.clear.cgColor
    effect.addSubview(content)
    NSLayoutConstraint.activate([
      content.leadingAnchor.constraint(equalTo:effect.leadingAnchor),
      content.trailingAnchor.constraint(equalTo:effect.trailingAnchor),
      content.topAnchor.constraint(equalTo:effect.safeAreaLayoutGuide.topAnchor),
      content.bottomAnchor.constraint(equalTo:effect.bottomAnchor)
    ])
  }
  effect.state=style.glass ? .active:.inactive
  // Hiding the effect view would also hide its content; disable only the material.
  effect.material=style.glass ? .underWindowBackground:.windowBackground
  (effect.subviews.first(where:{$0 is TailKanaTintView}) as? TailKanaTintView)?.style=style
}

struct TailKanaSettingsSurface<Content:View>: View {
  let content: Content
  @State private var options:[String:String]=[:]
  var body: some View {
    let style=TailKanaSurfaceStyle(options:options)
    content
      .foregroundStyle(Color(nsColor:style.ink))
      .tint(Color(nsColor:style.accent))
      .background(.clear)
      .onAppear {options=tailKanaThemeOptions()}
      .onReceive(NotificationCenter.default.publisher(for:tailKanaSettingsChangedNotification)) {_ in options=tailKanaThemeOptions()}
  }
}
