#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare the actual src/hash.c algorithms against Python's independent hashes."""
import concurrent.futures
import ctypes as c
import hashlib
from pathlib import Path
import random
import sys
import unittest

LIBRARY = Path(sys.argv.pop(1)).resolve()
ALGORITHMS = ("md5", "sha1", "sha256", "sha512")


class HashCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib = c.CDLL(str(LIBRARY))
        cls.lib.HashBuffer.argtypes = [c.c_uint, c.c_void_p, c.c_size_t, c.c_void_p]
        cls.lib.HashBuffer.restype = c.c_int
        cls.lib.RufusHashInitialize()
        cls.sha1 = c.c_int.in_dll(cls.lib, "cpu_has_sha1_accel")
        cls.sha256 = c.c_int.in_dll(cls.lib, "cpu_has_sha256_accel")
        cls.detected = (cls.lib.DetectSHA1Acceleration(), cls.lib.DetectSHA256Acceleration())
        cls.init = [c.CFUNCTYPE(None, c.c_void_p)(address)
                    for address in (c.c_void_p * 4).in_dll(cls.lib, "hash_init")]
        cls.write = [c.CFUNCTYPE(None, c.c_void_p, c.c_void_p, c.c_size_t)(address)
                     for address in (c.c_void_p * 4).in_dll(cls.lib, "hash_write")]
        cls.final = [c.CFUNCTYPE(None, c.c_void_p)(address)
                     for address in (c.c_void_p * 4).in_dll(cls.lib, "hash_final")]
        print(f"CPU SHA acceleration detected: SHA1={cls.detected[0]}, SHA256={cls.detected[1]}")

    def tearDown(self):
        self.sha1.value, self.sha256.value = self.detected

    def modes(self):
        # Never force an ISA unsupported by the actual CPU.
        for label, flags in (("scalar", (0, 0)), ("detected", self.detected)):
            self.sha1.value, self.sha256.value = flags
            yield label

    def digest(self, kind, data, offset=0):
        source = c.create_string_buffer(len(data) + offset + 1)
        if data:
            c.memmove(c.addressof(source) + offset, data, len(data))
        output = (c.c_ubyte * 64)()
        result = self.lib.HashBuffer(kind, c.addressof(source) + offset, len(data), output)
        self.assertEqual(result, 1)
        size = hashlib.new(ALGORITHMS[kind]).digest_size
        return bytes(output[:size]).hex()

    def test_padding_boundaries_and_unaligned_inputs(self):
        lengths = (0, 1, 2, 55, 56, 57, 63, 64, 65, 111, 112, 113, 127, 128,
                   129, 255, 256, 257, 4095, 4096, 4097, 1048576 + 31)
        for mode in self.modes():
            for length in lengths:
                data = (bytes(range(256)) * ((length + 255) // 256))[:length]
                for kind, name in enumerate(ALGORITHMS):
                    expected = hashlib.new(name, data).hexdigest()
                    for offset in (0, 1, 3):
                        with self.subTest(mode=mode, algorithm=name, length=length, offset=offset):
                            self.assertEqual(self.digest(kind, data, offset), expected)

    def test_known_vectors(self):
        for mode in self.modes():
            for data in (b"abc", b"The quick brown fox jumps over the lazy dog", b"a" * 1000000):
                for kind, name in enumerate(ALGORITHMS):
                    with self.subTest(mode=mode, algorithm=name, length=len(data)):
                        self.assertEqual(self.digest(kind, data), hashlib.new(name, data).hexdigest())

    def test_streaming_tiny_and_random_chunks(self):
        rng = random.Random(8162412)
        for mode in self.modes():
            for length in (2, 63, 64, 65, 127, 128, 129, 8193):
                data = rng.randbytes(length)
                for kind, name in enumerate(ALGORITHMS):
                    for chunk_limit in (1, 17, 300):
                        # HASH_CONTEXT is aligned to 64 bytes in the shared C header.
                        storage = c.create_string_buffer(320)
                        context = (c.addressof(storage) + 63) & ~63
                        self.init[kind](context)
                        position = 0
                        while position < length:
                            size = min(rng.randint(1, chunk_limit), length - position)
                            segment = c.create_string_buffer(data[position:position + size], size)
                            self.write[kind](context, segment, size)
                            position += size
                        self.final[kind](context)
                        actual = c.string_at(context, hashlib.new(name).digest_size).hex()
                        with self.subTest(mode=mode, algorithm=name, length=length, limit=chunk_limit):
                            self.assertEqual(actual, hashlib.new(name, data).hexdigest())

    def test_invalid_arguments_do_not_write_output(self):
        output = (c.c_ubyte * 64)(*([0x5A] * 64))
        for kind, source, length, destination in (
            (4, None, 0, output), (0xFFFFFFFF, None, 0, output),
            (0, None, 1, output), (0, None, 0, None),
        ):
            self.assertEqual(self.lib.HashBuffer(kind, source, length, destination), 0)
            self.assertEqual(bytes(output), b"\x5a" * 64)
        for kind, name in enumerate(ALGORITHMS):
            self.assertEqual(self.lib.HashBuffer(kind, None, 0, output), 1)
            size = hashlib.new(name).digest_size
            self.assertEqual(bytes(output[:size]), hashlib.new(name, b"").digest())

    def test_concurrent_independent_contexts(self):
        self.sha1.value, self.sha256.value = self.detected
        def task(index):
            data = bytes([index]) * (4096 + index)
            return all(self.digest(kind, data) == hashlib.new(name, data).hexdigest()
                       for kind, name in enumerate(ALGORITHMS))
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            self.assertTrue(all(pool.map(task, range(32))))


if __name__ == "__main__":
    unittest.main(verbosity=2)
