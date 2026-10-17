  override func menu() -> NSMenu! {
    // IMK's ObjC callback runs on main, but lacks Swift actor annotations.
    // Return an owned address across that annotation boundary, not an NSMenu
    // (AppKit explicitly marks NSMenu as non-Sendable).
    let controllerAddress = UInt(bitPattern: Unmanaged.passUnretained(self).toOpaque())
    let menuAddress = MainActor.assumeIsolated {
      let target = Unmanaged<FcitxInputController>.fromOpaque(UnsafeRawPointer(bitPattern: controllerAddress)!).takeUnretainedValue()
      return UInt(bitPattern: Unmanaged.passRetained(TailKanaMenus.controller.makeMenu(target: target)).toOpaque())
    }
    return Unmanaged<NSMenu>.fromOpaque(UnsafeRawPointer(bitPattern: menuAddress)!).takeRetainedValue()
  }
