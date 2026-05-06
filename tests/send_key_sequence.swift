import ApplicationServices
import Foundation

guard CommandLine.arguments.count >= 3,
      let pidValue = Int32(CommandLine.arguments[1]) else {
    fputs("usage: send_key_sequence <pid> <keycode> [<keycode> ...]\n", stderr)
    exit(1)
}

let pid = pid_t(pidValue)
let keycodes = CommandLine.arguments.dropFirst(2).compactMap { Int($0) }

for keycode in keycodes {
    let virtualKey = CGKeyCode(keycode)
    guard let keyDown = CGEvent(keyboardEventSource: nil, virtualKey: virtualKey, keyDown: true),
          let keyUp = CGEvent(keyboardEventSource: nil, virtualKey: virtualKey, keyDown: false) else {
        continue
    }

    keyDown.postToPid(pid)
    usleep(40_000)
    keyUp.postToPid(pid)
    usleep(40_000)
}
