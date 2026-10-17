import Carbon
import Foundation
func property(_ source: TISInputSource, _ key: CFString) -> String {
  guard let pointer = TISGetInputSourceProperty(source,key) else { return "" }
  return Unmanaged<CFString>.fromOpaque(pointer).takeUnretainedValue() as String
}
func sources() -> [TISInputSource] { TISCreateInputSourceList(nil,true)?.takeRetainedValue() as? [TISInputSource] ?? [] }
func enableParent() {
  if let parent=sources().first(where:{property($0,kTISPropertyInputSourceID)=="org.tailkana.inputmethod.TailKana"}) {
    print("enable-parent=\(TISEnableInputSource(parent))")
  }
}
let args=CommandLine.arguments
if args.count>2 && args[1]=="--register" {
  let url=URL(fileURLWithPath:args[2]) as CFURL
  let status=TISRegisterInputSource(url)
  print("register=\(status)")
  if status != noErr { exit(1) }
}
if args.count>2 && args[1]=="--select" {
  guard let source=sources().first(where:{property($0,kTISPropertyInputSourceID)==args[2]}) else {print("source unavailable");exit(2)}
  enableParent()
  let enabled=TISEnableInputSource(source)
  let selected=TISSelectInputSource(source)
  print("enable=\(enabled) select=\(selected)")
  exit(selected == noErr ? 0 : 3)
}
if args.count>1 && args[1]=="--current" {
  print(property(TISCopyCurrentKeyboardInputSource().takeRetainedValue(),kTISPropertyInputSourceID))
  exit(0)
}
if args.count>1 && args[1]=="--enable" {enableParent()}
for source in sources() {
  let id=property(source,kTISPropertyInputSourceID)
  if id.contains("fcitx") || id.contains("tailkana") {
    print("\(id) | \(property(source,kTISPropertyLocalizedName))")
    if args.count>1 && args[1]=="--enable" && id=="org.tailkana.inputmethod.TailKana.japanese" { print("enable=\(TISEnableInputSource(source))") }
  }
}
