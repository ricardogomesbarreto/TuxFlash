#!/usr/bin/env python3
"""Exercise real GTK allocations on isolated Xvfb displays; no USB access."""
from pathlib import Path
import argparse
import ctypes as C
import json
import os
import select
import subprocess
import time


def screenshot(display, xid, path):
    from PIL import Image
    lib = C.CDLL('libX11.so.6')
    lib.XOpenDisplay.argtypes = [C.c_char_p]
    lib.XOpenDisplay.restype = C.c_void_p
    connection = lib.XOpenDisplay(display.encode())
    assert connection

    class XImage(C.Structure):
        _fields_ = [('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int),
                    ('format', C.c_int), ('data', C.c_void_p), ('byte_order', C.c_int),
                    ('bitmap_unit', C.c_int), ('bitmap_bit_order', C.c_int),
                    ('bitmap_pad', C.c_int), ('depth', C.c_int),
                    ('bytes_per_line', C.c_int), ('bits_per_pixel', C.c_int),
                    ('red_mask', C.c_ulong), ('green_mask', C.c_ulong), ('blue_mask', C.c_ulong)]

    root = C.c_ulong()
    x, y = C.c_int(), C.c_int()
    width, height, border, depth = C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
    lib.XGetGeometry.argtypes = [C.c_void_p, C.c_ulong] + [C.c_void_p] * 7
    assert lib.XGetGeometry(connection, xid, C.byref(root), C.byref(x), C.byref(y),
                            C.byref(width), C.byref(height), C.byref(border), C.byref(depth))
    lib.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                             C.c_uint, C.c_uint, C.c_ulong, C.c_int]
    lib.XGetImage.restype = C.POINTER(XImage)
    frame = lib.XGetImage(connection, xid, 0, 0, width.value, height.value, 0xffffffff, 2)
    assert frame
    value = frame.contents
    assert value.bits_per_pixel == 32
    Image.frombytes('RGB', (value.width, value.height),
                    C.string_at(value.data, value.bytes_per_line * value.height),
                    'raw', 'BGRX', value.bytes_per_line).save(path)
    lib.XDestroyImage.argtypes = [C.POINTER(XImage)]
    lib.XDestroyImage(frame)
    lib.XCloseDisplay.argtypes = [C.c_void_p]
    lib.XCloseDisplay(connection)


def wait_file(path, process, log):
    deadline = time.monotonic() + 30
    while not path.exists() and process.poll() is None and time.monotonic() < deadline:
        time.sleep(.05)
    assert path.exists(), log.read_text(errors='replace')
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--catalogs', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--xvfb', default='Xvfb')
    parser.add_argument('--xvfb-lib-dir')
    parser.add_argument('--quick', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    sizes = ('1024x768',) if args.quick else ('800x600', '1024x768', '1920x1080')
    languages = ('pt_BR',) if args.quick else ('pt_BR', 'en', 'zh_CN', 'es')
    for size in sizes:
        for language in languages:
            prefix = (args.output / f'{language}-{size}').resolve()
            for suffix in ('-basic.json', '-expanded.json', '-advance', '-done'):
                Path(str(prefix) + suffix).unlink(missing_ok=True)
            read_fd, write_fd = os.pipe()
            server = process = None
            log_path = Path(str(prefix) + '.log')
            try:
                server_env = dict(os.environ)
                if args.xvfb_lib_dir:
                    server_env['LD_LIBRARY_PATH'] = args.xvfb_lib_dir
                with Path(str(prefix) + '-xvfb.log').open('w') as server_log:
                    server = subprocess.Popen([args.xvfb, '-displayfd', str(write_fd),
                        '-screen', '0', size + 'x24', '-nolisten', 'tcp', '-ac'],
                        pass_fds=(write_fd,), env=server_env, stdout=server_log, stderr=subprocess.STDOUT)
                os.close(write_fd)
                write_fd = -1
                assert select.select([read_fd], [], [], 20)[0]
                display = ':' + os.read(read_fd, 32).decode().strip()
                environment = dict(os.environ, DISPLAY=display, GDK_BACKEND='x11', GTK_A11Y='none',
                    GSETTINGS_BACKEND='memory', LANGUAGE=language, LANG='C.UTF-8', LC_ALL='C.UTF-8',
                    XDG_CONFIG_HOME=str(prefix) + '-config', XDG_CACHE_HOME=str(prefix) + '-cache')
                if args.runtime:
                    command = [str(args.runtime / 'run-elf'), 'tuxflash-layout-test', str(args.binary.resolve())]
                    for key in ('LD_LIBRARY_PATH', 'GTK_PATH', 'GTK_EXE_PREFIX', 'GIO_MODULE_DIR',
                                'LOCPATH', 'GSETTINGS_SCHEMA_DIR', 'GDK_PIXBUF_MODULE_FILE', 'GDK_PIXBUF_MODULEDIR'):
                        environment.pop(key, None)
                else:
                    command = [str(args.binary.resolve())]
                command += [str(args.catalogs.resolve()), str(prefix)]
                with log_path.open('w') as log:
                    process = subprocess.Popen(command, env=environment, stdout=log, stderr=subprocess.STDOUT)
                stages = []
                for stage, marker in (('basic', 'advance'), ('expanded', 'done')):
                    data = wait_file(Path(str(prefix) + '-' + stage + '.json'), process, log_path)
                    screenshot(display, data.pop('xid'), Path(str(prefix) + '-' + stage + '.png'))
                    Path(str(prefix) + '-' + marker).touch()
                    stages.append(data)
                assert process.wait(timeout=10) == 0, log_path.read_text(errors='replace')
                results.append({'language': language, 'screen': size, 'stages': stages, 'passed': True})
                print('LAYOUT PASS', language, size, flush=True)
            finally:
                os.close(read_fd)
                if write_fd >= 0:
                    os.close(write_fd)
                for item in (process, server):
                    if item is not None and item.poll() is None:
                        item.terminate()
                        try:
                            item.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            item.kill()
                            item.wait()
    (args.output / 'layout-results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
