// Exercises the real macOS Fcitx frontend and NSTextView marked-text lifecycle.
import AppKit
import CxxFrontend
import Fcitx
import SwiftFrontend

@MainActor
extension TailKanaTextView {
  func selfTest(_ outputPath: String) {
    var checks: [String] = []
    func verify(_ ok: Bool, _ name: String) {
      if !ok {
        try? Data("FAILED: \(name)\n".utf8).write(to: URL(fileURLWithPath: outputPath))
        fputs("FAILED: \(name)\n", stderr)
        stop_fcitx_thread()
        exit(1)
      }
      checks.append(name)
    }
    func key(_ unicode: UInt32, _ code: UInt16, _ mods: UInt32 = 0, release: Bool = false) {
      let data = Data(String(process_key(uuid, unicode, mods, code, release, false, string, UInt32(selectedRange().location), UInt32(NSMaxRange(selectedRange())))).utf8)
      guard let response = try? JSONDecoder().decode(PreviewResponse.self, from: data) else { verify(false, "native response decode"); return }
      applyPreviewResponse(response.commit, response.preedit, response.caretPos)
    }
    func word(_ letters: String) {
      let codes: [Character: UInt16] = ["n":45,"i":34,"h":4,"o":31,"g":5,"t":17,"y":16,"u":32]
      for letter in letters { key(letter.unicodeScalars.first!.value, codes[letter]!) }
    }
    func resetDocument() { key(27,53); string="";setSelectedRange(NSRange(location:0,length:0)) }
    Fcitx.setConfig("fcitx://config/addon/tailkana", "{\"ShowCandidate\":\"False\"}")
    insertText("前缀 ",replacementRange:selectedRange())
    word("nihongo")
    verify(string == "前缀 nihongo" && hasMarkedText(), "roman preedit is inline and uncommitted")
    key(32,49)
    verify(string == "前缀 日本語" && hasMarkedText(), "Space visibly converts in the input location")
    let first=string
    key(32,49)
    verify(string != first && hasMarkedText(), "repeated Space changes the displayed candidate")
    let chosen=string
    key(13,36)
    verify(string == chosen && !hasMarkedText(), "confirm replaces marked text without duplication or loss")
    word("ti");key(27,53)
    verify(string == chosen && !hasMarkedText(), "cancel removes only the active preedit")
    resetDocument();word("tyo")
    key(0,56,0x20000)
    verify(String(Fcitx.imGetCurrentIMName()) == "tailkana", "left Shift does not activate the default English toggle")
    key(13,36,0x20000)
    verify(string == "チョ" && !hasMarkedText(), "held left Shift produces katakana through the native frontend")
    key(0,56,0,release:true)
    resetDocument();word("ti")
    key(0,60,0x20000);key(0xF703,124,0x20000);key(0,60,0,release:true);key(13,36)
    verify(string == "ちょ", "right Shift still corrects the trailing syllable")
    resetDocument()
    Fcitx.setConfig("fcitx://config/addon/tailkana", "{\"LeftShift\":\"False\"}")
    word("tyo");key(0,56,0x20000);key(0,56,0,release:true)
    verify(String(Fcitx.imGetCurrentIMName()) == "tailkana", "disabled left Shift does not fall back to English")
    key(13,36);verify(string == "ちょ", "disabled left Shift preserves hiragana")
    Fcitx.setConfig("fcitx://config/addon/tailkana", "{\"LeftShift\":\"True\"}")
    let data = try! JSONSerialization.data(withJSONObject: ["passed": true, "checks":checks], options:.prettyPrinted)
    try! data.write(to:URL(fileURLWithPath:outputPath))
    print("Native inline preview: \(checks.count) checks passed")
  }
}
