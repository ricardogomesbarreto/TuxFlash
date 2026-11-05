#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prevent destructive-input and false-success regressions in the VM harness."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('real_media', Path(__file__).parent / 'vm/real_media.py')
qa = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qa)

class Guards(unittest.TestCase):
    def test_existing_job_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory); saved = path / 'target.img'; saved.write_bytes(b'preserve')
            with self.assertRaises(ValueError): qa.prepare_work(path)
            self.assertEqual(saved.read_bytes(), b'preserve')

    def test_symlink_work_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory); link = path / 'link'; link.symlink_to(path, target_is_directory=True)
            with self.assertRaises(ValueError): qa.prepare_work(link)
            self.assertFalse((path / '.tuxflash-real-media-qa').exists())

    def test_nonregular_input_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory); source = path / 'source'; source.write_bytes(b'input')
            link = path / 'link'; link.symlink_to(source)
            for invalid in (link, path, Path('/dev/null')):
                with self.subTest(path=str(invalid)), self.assertRaises(ValueError): qa.regular(invalid)

    def test_qemu_option_injection_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            for name in ('bad,readonly=off', 'bad\npath', 'bad\rpath'):
                with self.subTest(name=name), self.assertRaises(ValueError): qa.prepare_work(Path(directory) / name)

    def test_short_independent_read_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'image'; path.write_bytes(b'ab')
            with self.assertRaises(ValueError): qa.digest(path, 3)

    def log(self, complete='1024', status=0):
        return f'MEDIA LOG BEGIN\r\nCOMPLETE {complete}\r\nMEDIA LOG END\r\nMEDIA RESULT status={status}\r\n'

    def test_raw_and_windows_completion(self):
        for message in ('1024', 'Windows installer media created'):
            self.assertEqual(qa.completion(self.log(message), 0), (message, True))

    def test_false_success_rejected(self):
        valid = self.log()
        cases = [(valid, 1), (self.log(status=1), 0), ('COMPLETE 1024\n', 0),
                 (valid + valid, 0), (valid.replace('COMPLETE 1024', 'COMPLETE 1024\nCOMPLETE 1024'), 0),
                 (valid + 'MEDIA FATAL unsafe-target\n', 0),
                 ('MEDIA PROGRESS COMPLETE 1024\nMEDIA RESULT status=0\n', 0)]
        for text, code in cases:
            with self.subTest(text=text, code=code): self.assertFalse(qa.completion(text, code)[1])

    def test_unattended_requires_explicit_opt_in(self):
        normal = qa.init_script('windows.iso', 1024, 'windows-installer', 1)
        opted_in = qa.init_script('windows.iso', 1024, 'windows-installer', 1, True)
        self.assertIn('--windows-flags 30 ', normal)
        self.assertNotIn('--windows-flags 94 ', normal)
        self.assertIn('--windows-flags 94 ', opted_in)

    def test_ntfs_layout_matches_requested_option(self):
        normal = qa.init_script('windows.iso', 1024, 'windows-installer', 4, True)
        ntfs = qa.init_script('windows.iso', 1024, 'windows-installer', 4, True, True)
        self.assertNotIn('--uefi-ntfs', normal)
        self.assertEqual(ntfs.count('--uefi-ntfs'), 1)
        self.assertIn('--windows-flags 94 ', ntfs)

    def test_image_name_shell_quoted(self):
        import shlex
        image = "danger'$(touch /tmp/qa-should-not-exist).iso"
        script = qa.init_script(image, 1024, 'raw', 1)
        self.assertIn('--source ' + shlex.quote('/inputs/' + image) + ' --target', script)

if __name__ == '__main__': unittest.main()
