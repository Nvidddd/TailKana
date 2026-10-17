// SPDX-License-Identifier: GPL-3.0-or-later
import AppKit
import InputMethodKit

@MainActor final class TailKanaProductMenu: NSObject, NSMenuDelegate {
  struct Services {
    let currentMode: () -> String
    let selectMode: (String) -> Void
    let open: (String) -> Void
  }
  let services: Services
  init(_ services: Services) {self.services = services}

  func makeMenu(target: AnyObject? = nil) -> NSMenu {
    let menu = NSMenu()
    menu.autoenablesItems = false
    // IMK routes commands through its input controller. Native status menus
    // instead retain this object as their delegate and send plain NSMenuItems.
    if target == nil {menu.delegate = self}
    populate(menu, target: target ?? self)
    return menu
  }
  func menuNeedsUpdate(_ menu: NSMenu) {
    menu.removeAllItems()
    populate(menu, target: self)
  }
  private func populate(_ menu: NSMenu, target: AnyObject) {
    func item(_ title: String, _ action: Selector, _ value: String, checked: Bool? = nil) {
      let entry = NSMenuItem(title: title, action: action, keyEquivalent: "")
      entry.target = target
      entry.representedObject = value
      if let checked {entry.state = checked ? .on : .off}
      menu.addItem(entry)
    }
    let mode = services.currentMode()
    item("日语输入", #selector(tailKanaSelectMode(_:)), "tailkana", checked: mode == "tailkana")
    item("英文直输", #selector(tailKanaSelectMode(_:)), "keyboard-us", checked: mode == "keyboard-us")
    menu.addItem(.separator())
    item("TailKana 设置…", #selector(tailKanaOpenMenuPage(_:)), "settings")
    item("重新启动输入法", #selector(tailKanaOpenMenuPage(_:)), "restart")
  }
  private func value(_ sender: Any?) -> String? {
    if let item = sender as? NSMenuItem {return item.representedObject as? String}
    if let command = sender as? NSDictionary,
       let item = command[kIMKCommandMenuItemName] as? NSMenuItem {
      return item.representedObject as? String
    }
    return nil
  }
  @objc func tailKanaSelectMode(_ sender: Any?) {
    guard let mode = value(sender), ["tailkana", "keyboard-us"].contains(mode) else {return}
    services.selectMode(mode)
  }
  @objc func tailKanaOpenMenuPage(_ sender: Any?) {
    if let page = value(sender), ["settings","restart"].contains(page) {services.open(page)}
  }
}
