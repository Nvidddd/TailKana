import AppKit
import InputMethodKit

@main struct ProductMenuTest {
  @MainActor static func main() {
    NSApplication.shared.setActivationPolicy(.prohibited)
    var mode="tailkana"
    var opened:[String]=[]
    let controller=TailKanaProductMenu(.init(currentMode:{mode},selectMode:{mode=$0},open:{opened.append($0)}))
    let menu=controller.makeMenu()
    func item(_ title:String)->NSMenuItem {menu.items.first(where:{$0.title==title})!}
    func click(_ title:String) {
      let entry=item(title)
      precondition(NSApp.sendAction(entry.action!,to:entry.target,from:entry))
      controller.menuNeedsUpdate(menu)
    }
    precondition(menu.items.filter{!$0.isSeparatorItem}.map(\.title)==["日语输入","英文直输","TailKana 设置…","重新启动输入法"])
    precondition(item("日语输入").state == .on)
    click("英文直输");precondition(mode=="keyboard-us" && item("英文直输").state == .on)
    click("日语输入");precondition(mode=="tailkana")
    click("TailKana 设置…");click("重新启动输入法")
    precondition(opened==["settings","restart"])
    // The system input menu sends the same owned item inside a dictionary.
    controller.tailKanaOpenMenuPage([kIMKCommandMenuItemName as String:item("TailKana 设置…")] as NSDictionary)
    precondition(opened.last=="settings")
    let target=NSObject();let imk=controller.makeMenu(target:target)
    precondition(imk.items.allSatisfy{$0.submenu==nil})
    precondition(imk.items.filter{!$0.isSeparatorItem}.allSatisfy{$0.target === target && $0.action != nil})
    print("PASS: exactly four menu entries, mode checkmarks, settings/restart routing, IMK dictionary commands")
  }
}
