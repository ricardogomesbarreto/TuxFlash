#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Open this build on a private X server and capture its actual GTK window.

Never presses START or invokes the privileged helper. Xvfb is caller-supplied.
"""
import ctypes as c
import json
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time
from PIL import Image

project = Path(sys.argv[1]).resolve()
xvfb = sys.argv[2]
output = Path(sys.argv[3]).resolve()
output.mkdir(parents=True, exist_ok=True)
screen = os.environ.get('TUXFLASH_SMOKE_SCREEN', '1024x768')
try:
    screen_width, screen_height = (int(item) for item in screen.split('x'))
    if not 640 <= screen_width <= 8192 or not 480 <= screen_height <= 8192:
        raise ValueError('screen dimensions out of range')
except ValueError as exc:
    raise SystemExit(f'Invalid TUXFLASH_SMOKE_SCREEN: {screen}') from exc
server = None
app = None
display = None
x = c.CDLL('libX11.so.6')

class XImage(c.Structure):
    _fields_ = [(name, kind) for name, kind in [
        ('width', c.c_int), ('height', c.c_int), ('xoffset', c.c_int),
        ('format', c.c_int), ('data', c.c_void_p), ('byte_order', c.c_int),
        ('bitmap_unit', c.c_int), ('bitmap_bit_order', c.c_int),
        ('bitmap_pad', c.c_int), ('depth', c.c_int), ('bytes_per_line', c.c_int),
        ('bits_per_pixel', c.c_int), ('red_mask', c.c_ulong),
        ('green_mask', c.c_ulong), ('blue_mask', c.c_ulong),
    ]]

x.XOpenDisplay.argtypes = [c.c_char_p]
x.XOpenDisplay.restype = c.c_void_p
x.XDefaultRootWindow.argtypes = [c.c_void_p]
x.XDefaultRootWindow.restype = c.c_ulong
x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong),
    c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
x.XFetchName.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_void_p)]
x.XFree.argtypes = [c.c_void_p]
x.XGetGeometry.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong),
    c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_uint),
    c.POINTER(c.c_uint), c.POINTER(c.c_uint), c.POINTER(c.c_uint)]
x.XGetImage.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_int,
    c.c_uint, c.c_uint, c.c_ulong, c.c_int]
x.XGetImage.restype = c.POINTER(XImage)
x.XDestroyImage.argtypes = [c.POINTER(XImage)]
x.XCloseDisplay.argtypes = [c.c_void_p]

try:
    with (output / 'xvfb.log').open('w') as server_log, (output / 'gui.log').open('w') as app_log:
        server = subprocess.Popen([xvfb, '-displayfd', '1', '-screen', '0',
            f'{screen_width}x{screen_height}x24', '-nolisten', 'tcp', '-ac'], stdout=subprocess.PIPE,
            stderr=server_log, start_new_session=True)
        selector = selectors.DefaultSelector()
        selector.register(server.stdout, selectors.EVENT_READ)
        if not selector.select(timeout=10):
            raise RuntimeError('Xvfb did not announce a display')
        number = server.stdout.readline().decode().strip()
        selector.close()
        if not number.isdigit():
            raise RuntimeError('Invalid Xvfb display number')
        env = os.environ.copy()
        for name in ('GTK_PATH', 'GTK_EXE_PREFIX', 'GTK_IM_MODULE_FILE', 'GIO_MODULE_DIR', 'LOCPATH',
                     'GSETTINGS_SCHEMA_DIR', 'GDK_PIXBUF_MODULE_FILE', 'GDK_PIXBUF_MODULEDIR'):
            env.pop(name, None)
        env.update(DISPLAY=':' + number, GDK_BACKEND='x11', GSK_RENDERER='cairo',
                   GTK_A11Y='none', LC_ALL='pt_BR.UTF-8', LANGUAGE='pt_BR',
                   LD_LIBRARY_PATH=str(project / 'linux/runtime/lib'),
                   XDG_DATA_DIRS=str(project / 'linux/runtime/share') + ':/usr/local/share:/usr/share')
        app = subprocess.Popen(['dbus-run-session', '--', str(project / 'tuxflash.sh')],
            stdout=app_log, stderr=subprocess.STDOUT, env=env, start_new_session=True)
        display = x.XOpenDisplay((':' + number).encode())
        if not display:
            raise RuntimeError('Cannot connect to private X server')
        root = x.XDefaultRootWindow(display)
        selected = None
        for attempt in range(40):
            if app.poll() is not None:
                raise RuntimeError(f'GTK application exited: {app.returncode}')
            root_out, parent = c.c_ulong(), c.c_ulong()
            children = c.POINTER(c.c_ulong)()
            count = c.c_uint()
            x.XQueryTree(display, root, c.byref(root_out), c.byref(parent), c.byref(children), c.byref(count))
            for index in range(count.value):
                name = c.c_void_p()
                if x.XFetchName(display, children[index], c.byref(name)) and name.value:
                    title = c.string_at(name).decode(errors='replace')
                    x.XFree(name)
                    if title.startswith('TuxFlash '):
                        selected = (children[index], title)
                        break
            if children:
                x.XFree(children)
            if selected:
                break
            time.sleep(0.2)
        if not selected:
            raise RuntimeError('No TuxFlash application window appeared')
        time.sleep(1)
        window, title = selected
        window_root, wx, wy = c.c_ulong(), c.c_int(), c.c_int()
        width, height, border, depth = (c.c_uint() for _ in range(4))
        if not x.XGetGeometry(display, window, c.byref(window_root), c.byref(wx),
            c.byref(wy), c.byref(width), c.byref(height), c.byref(border), c.byref(depth)):
            raise RuntimeError('Cannot inspect GTK window geometry')
        capture = x.XGetImage(display, window, 0, 0, width.value, height.value, c.c_ulong(-1).value, 2)
        if not capture:
            raise RuntimeError('Cannot capture GTK window')
        try:
            info = capture.contents
            if info.bits_per_pixel != 32 or info.red_mask != 0xff0000 or info.blue_mask != 0xff:
                raise RuntimeError('Unexpected X image format')
            pixels = c.string_at(info.data, info.bytes_per_line * info.height)
            Image.frombytes('RGB', (info.width, info.height), pixels,
                'raw', 'BGRX', info.bytes_per_line).save(output / 'gui.png')
        finally:
            x.XDestroyImage(capture)
        if width.value > screen_width or height.value > screen_height:
            raise RuntimeError(f'TuxFlash {width.value}x{height.value} exceeds virtual screen {screen}')
        report = {'window_title': title, 'width': width.value, 'height': height.value,
                  'screen': screen,
                  'running_at_capture': app.poll() is None, 'usb_write_tested': False}
        (output / 'gui.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report))
finally:
    if display:
        x.XCloseDisplay(display)
    for process in (app, server):
        if process and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
