"""(AI-assisted) Play a timeline of key presses into the running Jak 3 (gk.exe) window.

usage: python -I keys.py <timeline>
Each timeline entry is (start seconds, key name, hold seconds). Keys are sent as scan codes
with SendInput, so they arrive through the game's normal keyboard handling.
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

user32 = ctypes.windll.user32

SCAN = {'p': 0x19, 'space': 0x39, 'w': 0x11, 'a': 0x1E, 's': 0x1F, 'd': 0x20,
        'i': 0x17, 'j': 0x24, 'k': 0x25, 'l': 0x26, 'r': 0x13, 'e': 0x12, 'f': 0x21,
        'comma': 0x33, 'return': 0x1C, 'period': 0x34, '1': 0x02, 'q': 0x10, 'o': 0x18,
        'up': 0x48, 'down': 0x50, 'left': 0x4B, 'right': 0x4D}

TIMELINES = {
    # mount, push, pause for four seconds, unpause, push, dismount
    'pause': [
        (0.0, 'p', 0.2),
        (15.0, 'space', 2.5),
        (18.0, 'return', 0.15),
        (22.0, 'return', 0.15),
        (23.0, 'space', 2.0),
        (27.0, 'r', 0.2),
    ],
    # mount, push, steer, ollie, kickflip, switch to jetboard and back, dismount, remount
    'play': [
        (0.0, 'p', 0.2),
        (15.0, 'space', 4.0),
        (19.5, 'd', 1.5),
        (21.5, 'k', 0.18), (21.68, 'i', 0.10),
        (25.0, 'k', 0.18), (25.18, 'i', 0.10), (25.18, 'j', 0.10),
        (29.0, 'period', 0.6), (29.25, 'comma', 0.15),
        (32.0, 'period', 0.6), (32.25, 'comma', 0.15),
        (35.0, 'r', 0.2),
        (39.0, 'p', 0.2),
        (43.0, 'space', 2.0),
        (46.0, 'r', 0.2),
    ],
}


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', wt.WORD), ('wScan', wt.WORD), ('dwFlags', wt.DWORD),
                ('time', wt.DWORD), ('dwExtraInfo', ctypes.POINTER(ctypes.c_ulong))]


class INPUTUNION(ctypes.Union):
    _fields_ = [('ki', KEYBDINPUT), ('pad', ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _fields_ = [('type', wt.DWORD), ('u', INPUTUNION)]


def send(scan, up):
    flags = 0x0008 | (0x0002 if up else 0)  # SCANCODE | KEYUP
    if scan in (0x48, 0x50, 0x4B, 0x4D):
        flags |= 0x0001  # EXTENDEDKEY for the arrow keys
    inp = INPUT(type=1)
    inp.u.ki = KEYBDINPUT(0, scan, flags, 0, None)
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def gk_pid():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq gk.exe', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split(',')]
        if len(parts) > 1 and parts[0].lower() == 'gk.exe':
            return int(parts[1])
    return None


def focus_game():
    pid = gk_pid()
    if pid is None:
        return False
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def each(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd) and user32.GetWindowTextLengthW(hwnd) > 0:
            found.append(hwnd)
        return True

    user32.EnumWindows(each, 0)
    if not found:
        return False
    hwnd = found[0]
    # Windows only lets a process take the foreground after it has had input: tap Alt first.
    user32.keybd_event(0x12, 0, 0, 0)
    user32.keybd_event(0x12, 0, 2, 0)
    user32.ShowWindow(hwnd, 9)
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)
    return user32.GetForegroundWindow() == hwnd


def main():
    timeline = TIMELINES[sys.argv[1]]
    events = []
    for start, key, hold in timeline:
        events.append((start, SCAN[key], False))
        events.append((start + hold, SCAN[key], True))
    events.sort(key=lambda e: e[0])
    print('game window focused:', focus_game(), flush=True)
    began = time.perf_counter()
    for at, scan, up in events:
        delay = at - (time.perf_counter() - began)
        if delay > 0:
            time.sleep(delay)
        send(scan, up)
    print('keys sent:', len(events), flush=True)


if __name__ == '__main__':
    main()
