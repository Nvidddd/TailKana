import AppKit
import SwiftUI

@main struct SettingsHubTest {
  @MainActor static func main() throws {
    let interactive=CommandLine.arguments.contains("--interactive")
    NSApplication.shared.setActivationPolicy(interactive ? .accessory:.prohibited)
    let root=URL(fileURLWithPath:FileManager.default.currentDirectoryPath)
    NSApp.applicationIconImage=NSImage(contentsOf:root.appendingPathComponent("assets/tailkana.png"))
    let folder=root.appendingPathComponent("work/settings-previews-0.3.8")
    try FileManager.default.createDirectory(at:folder,withIntermediateDirectories:true)
    precondition(TailKanaSettingsPage.allCases.count==5)
    precondition(Set(TailKanaManagementDestination.all.map(\.id))==Set(["inputMethods","global","plugins","advanced"]))
    let window=NSWindow(contentRect:NSRect(x:0,y:0,width:760,height:620),styleMask:[.titled,.closable],backing:.buffered,defer:false)
    let hosting=NSHostingView(rootView:TailKanaSettingsSurface(content:TailKanaSettingsView()))
    window.contentView=hosting
    if interactive {
      window.title="TailKana 设置预览"
      applyTailKanaWindowTheme(window,options:SettingsFixture.options)
      window.center();window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
      NSApp.run();return
    }
    for scheme in ["Dark","Light"] {
      SettingsFixture.options["Appearance"]=scheme
      applyTailKanaWindowTheme(window,options:SettingsFixture.options)
      for page in TailKanaSettingsPage.allCases {
        TailKanaSettingsNavigation.shared.page=page
        notifyTailKanaSettingsChanged()
        window.contentView?.layoutSubtreeIfNeeded()
        RunLoop.current.run(until:Date().addingTimeInterval(0.1))
        hosting.layoutSubtreeIfNeeded()
        guard let bitmap=hosting.bitmapImageRepForCachingDisplay(in:hosting.bounds) else {fatalError("missing bitmap")}
        hosting.cacheDisplay(in:hosting.bounds,to:bitmap)
        let image=bitmap.representation(using:.png,properties:[:])!
        try image.write(to:folder.appendingPathComponent("\(page.rawValue)-\(scheme.lowercased()).png"))
        precondition(!window.isOpaque)
        precondition((window.contentView as! NSVisualEffectView).blendingMode == .behindWindow)
      }
    }
    for destination in TailKanaManagementDestination.all {openTailKanaManagementPage(destination.id)}
    precondition(ConfigWindowController.opened==["im","global","plugin","advanced"])
    openTailKanaThemeSettings()
    precondition(TailKanaSettingsNavigation.shared.page == .appearance && ConfigWindowController.opened.last=="tailkana")
    openTailKanaSettingsSection("about")
    precondition(TailKanaSettingsNavigation.shared.page == .about && ConfigWindowController.opened.last=="tailkana")
    print("PASS: five settings sections, all relocated destinations, theme/about reuse one settings window, native glass in both schemes; previews saved")
  }
}
