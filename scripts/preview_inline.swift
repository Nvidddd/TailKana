@MainActor
public func applyPreviewResponse(_ commit: String, _ preedit: String, _ caretPos: Int) {
  guard let view = previewTextView else { return }
  if !commit.isEmpty {
    let range = view.hasMarkedText() ? view.markedRange() : view.selectedRange()
    view.insertText(commit, replacementRange: range)
  }
  if !preedit.isEmpty {
    let count = max(0, min(caretPos, preedit.utf8.count))
    let prefix = String(decoding: Array(preedit.utf8.prefix(count)), as: UTF8.self)
    view.setMarkedText(preedit, selectedRange: NSRange(location: prefix.utf16.count, length: 0), replacementRange: NSRange(location: NSNotFound, length: 0))
  } else if view.hasMarkedText() {
    view.setMarkedText("", selectedRange: NSRange(location: 0, length: 0), replacementRange: NSRange(location: NSNotFound, length: 0))
    view.unmarkText()
  }
}
