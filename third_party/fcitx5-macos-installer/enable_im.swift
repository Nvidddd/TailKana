import Carbon

let bundleId = "org.tailkana.inputmethod.TailKana"
let inputSourceId = bundleId

let conditions = NSMutableDictionary()
conditions.setValue(bundleId, forKey: kTISPropertyBundleID as String)
// There are 3 items with kTISPropertyBundleID.
// We enable the parent, which has kTISPropertyInputSourceID: org.tailkana.inputmethod.TailKana
conditions.setValue(inputSourceId, forKey: kTISPropertyInputSourceID as String)
if let array = TISCreateInputSourceList(conditions, true)?.takeRetainedValue() as? [TISInputSource]
{
  for inputSource in array {
    TISEnableInputSource(inputSource)
  }
}
