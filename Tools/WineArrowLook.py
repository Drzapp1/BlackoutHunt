#!/usr/bin/env python3
import argparse
import ctypes
import ctypes.util
import math
import os
import signal
import sys
import time


KEY_NAMES = {
    "left": b"Left",
    "right": b"Right",
    "up": b"Up",
    "down": b"Down",
}


def process_exists(pid):
    if pid <= 0:
        return True
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def key_is_down(keymap, keycode):
    return bool(keymap[keycode >> 3] & (1 << (keycode & 7)))


def main():
    parser = argparse.ArgumentParser(description="Translate arrow keys to relative mouse look for Wine/XWayland games.")
    parser.add_argument("--parent-pid", type=int, default=0)
    parser.add_argument("--yaw-speed", type=float, default=float(os.environ.get("BLACKOUTHUNT_ARROW_YAW_SPEED", "900")))
    parser.add_argument("--pitch-speed", type=float, default=float(os.environ.get("BLACKOUTHUNT_ARROW_PITCH_SPEED", "700")))
    parser.add_argument("--poll-hz", type=float, default=float(os.environ.get("BLACKOUTHUNT_ARROW_POLL_HZ", "120")))
    args = parser.parse_args()

    lib_x11_name = ctypes.util.find_library("X11")
    lib_xtst_name = ctypes.util.find_library("Xtst")
    if not lib_x11_name or not lib_xtst_name:
        print("WineArrowLook: X11/Xtst libraries were not found.", file=sys.stderr)
        return 1

    x11 = ctypes.CDLL(lib_x11_name)
    xtst = ctypes.CDLL(lib_xtst_name)

    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.restype = ctypes.c_int
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
    x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    x11.XQueryKeymap.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte)]
    x11.XQueryKeymap.restype = ctypes.c_int
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XFlush.restype = ctypes.c_int
    xtst.XTestFakeRelativeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
    xtst.XTestFakeRelativeMotionEvent.restype = ctypes.c_int

    display = x11.XOpenDisplay(os.environ.get("DISPLAY", "").encode() or None)
    if not display:
        print("WineArrowLook: could not open the X display.", file=sys.stderr)
        return 1

    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    try:
        keycodes = {}
        for name, keysym_name in KEY_NAMES.items():
            keysym = x11.XStringToKeysym(keysym_name)
            keycodes[name] = int(x11.XKeysymToKeycode(display, keysym))
            if keycodes[name] == 0:
                print(f"WineArrowLook: could not resolve key {keysym_name.decode()}.", file=sys.stderr)
                return 1

        keymap = (ctypes.c_ubyte * 32)()
        interval = 1.0 / max(args.poll_hz, 1.0)
        carry_x = 0.0
        carry_y = 0.0
        last_time = time.monotonic()

        while running and process_exists(args.parent_pid):
            now = time.monotonic()
            delta_seconds = min(now - last_time, 0.05)
            last_time = now

            x11.XQueryKeymap(display, keymap)
            yaw = float(key_is_down(keymap, keycodes["right"])) - float(key_is_down(keymap, keycodes["left"]))
            pitch = float(key_is_down(keymap, keycodes["down"])) - float(key_is_down(keymap, keycodes["up"]))

            if yaw or pitch:
                carry_x += yaw * args.yaw_speed * delta_seconds
                carry_y += pitch * args.pitch_speed * delta_seconds
                move_x = int(math.trunc(carry_x))
                move_y = int(math.trunc(carry_y))
                if move_x or move_y:
                    carry_x -= move_x
                    carry_y -= move_y
                    xtst.XTestFakeRelativeMotionEvent(display, move_x, move_y, 0)
                    x11.XFlush(display)
            else:
                carry_x = 0.0
                carry_y = 0.0

            time.sleep(interval)
    finally:
        x11.XCloseDisplay(display)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
