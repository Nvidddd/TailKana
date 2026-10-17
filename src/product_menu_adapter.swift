// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import Fcitx
import FcitxConfigUI

@MainActor enum TailKanaMenus {
  static let controller = TailKanaProductMenu(.init(
    currentMode: {String(Fcitx.imGetCurrentIMName())},
    selectMode: {Fcitx.imSetCurrentIM($0, false)},
    open: { page in
      switch page {
      case "settings": openTailKanaSettings()
      case "restart": restartProcess()
      default: break
      }
    }))
}

extension FcitxInputController {
  @MainActor @objc func tailKanaSelectMode(_ sender: Any?) {TailKanaMenus.controller.tailKanaSelectMode(sender)}
  @MainActor @objc func tailKanaOpenMenuPage(_ sender: Any?) {TailKanaMenus.controller.tailKanaOpenMenuPage(sender)}
}
