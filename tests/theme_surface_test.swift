import AppKit
import SwiftUI

let tailKanaSettingsChangedNotification=Notification.Name("tailkana-test-settings")
@MainActor func tailKanaThemeOptions() -> [String:String] {[:]}

@main struct ThemeSurfaceTest {
  @MainActor static func main() {
    NSApplication.shared.setActivationPolicy(.prohibited)
    let window=NSWindow(contentRect:NSRect(x:0,y:0,width:530,height:600),styleMask:[.titled,.closable],backing:.buffered,defer:false)
    let content=NSHostingView(rootView:TailKanaSettingsSurface(content:Text("设置内容")))
    window.contentView=content
    var options=["Appearance":"Dark","Glass":"True","CustomColors":"True","BackgroundColor":"#112244","TextColor":"#fff4de","HighlightColor":"#334499","TintOpacity":"0"]
    applyTailKanaWindowTheme(window,options:options)
    let effect=window.contentView as! NSVisualEffectView
    precondition(!window.isOpaque && window.backgroundColor == .clear)
    precondition(effect.blendingMode == .behindWindow && effect.state == .active)
    precondition(effect.material == .underWindowBackground)
    precondition(window.effectiveAppearance.bestMatch(from:[.aqua,.darkAqua]) == .darkAqua)
    precondition(effect.subviews.count==2 && effect.subviews.last === content)
    precondition(TailKanaSurfaceStyle(options:options).opacity==0)
    options["Glass"]="False"
    applyTailKanaWindowTheme(window,options:options)
    precondition(window.isOpaque && window.contentView === effect && !effect.isHidden)
    precondition(effect.subviews.last === content && !content.isHidden)
    precondition(TailKanaSurfaceStyle(options:options).opacity==1)
    precondition(window.backgroundColor.usingColorSpace(.sRGB)!.hexString == "112244")
    options["Glass"]="True";options["TintOpacity"]="35";options["Appearance"]="Light"
    applyTailKanaWindowTheme(window,options:options)
    precondition(!window.isOpaque && effect.state == .active && effect.subviews.count==2)
    precondition(TailKanaSurfaceStyle(options:options).opacity==0.35)
    precondition(window.effectiveAppearance.bestMatch(from:[.aqua,.darkAqua]) == .aqua)
    options["Appearance"]="System"
    applyTailKanaWindowTheme(window,options:options)
    precondition(TailKanaSurfaceStyle(options:options).appearance == nil)
    options["BackgroundColor"]="invalid"
    precondition(TailKanaSurfaceStyle(options:options).color("BackgroundColor",fallback:.red) == .red)
    let preset=TailKanaSurfaceStyle(options:[:])
    NSAppearance(named:.darkAqua)!.performAsCurrentDrawingAppearance {
      precondition(preset.ink.usingColorSpace(.sRGB)!.hexString=="e6f2e9")
      precondition(preset.accent.usingColorSpace(.sRGB)!.hexString=="9ecfb3")
    }
    NSAppearance(named:.aqua)!.performAsCurrentDrawingAppearance {
      precondition(preset.ink.usingColorSpace(.sRGB)!.hexString=="1d3d2f")
      precondition(preset.accent.usingColorSpace(.sRGB)!.hexString=="387954")
    }
    print("PASS: native behind-window glass, zero tint, opaque off, live toggles, one wrapper, custom color, appearance, retained content")
  }
}
private extension NSColor {
  var hexString:String {String(format:"%02x%02x%02x",Int((redComponent*255).rounded()),Int((greenComponent*255).rounded()),Int((blueComponent*255).rounded()))}
}
