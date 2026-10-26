import CoreGraphics
// Prints "<window id> <owner pid> <height>" for every Azahar window taller than 200 px.
let list = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
for w in list where (w[kCGWindowOwnerName as String] as? String) == "Azahar" {
  let b = w[kCGWindowBounds as String] as! [String: Any]
  if let h = b["Height"] as? Double, h > 200 {
    print(w[kCGWindowNumber as String]!, w[kCGWindowOwnerPID as String]!, h)
  }
}
