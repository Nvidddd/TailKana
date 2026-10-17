import Carbon

let url = URL(fileURLWithPath: "/Library/Input Methods/TailKanaInput.app")
TISRegisterInputSource(url as CFURL)
