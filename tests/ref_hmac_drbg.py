#!/usr/bin/env python3
"""Independent HMAC_DRBG (SHA-256) reference using Python's hmac module.
Prints the expected output used by tests/test_core.c so the C code is
cross-checked against a second implementation. Run: python3 ref_hmac_drbg.py"""
import hashlib, hmac

class HmacDrbg:
    def __init__(self, seed, pers=b""):
        self.K, self.V = b"\x00" * 32, b"\x01" * 32
        self._update(seed + pers)
    def _h(self, k, m): return hmac.new(k, m, hashlib.sha256).digest()
    def _update(self, data=b""):
        self.K = self._h(self.K, self.V + b"\x00" + data); self.V = self._h(self.K, self.V)
        if data:
            self.K = self._h(self.K, self.V + b"\x01" + data); self.V = self._h(self.K, self.V)
    def reseed(self, seed): self._update(seed)
    def generate(self, n):
        out = b""
        while len(out) < n:
            self.V = self._h(self.K, self.V); out += self.V
        self._update()
        return out[:n]

d = HmacDrbg(bytes(range(32)), b"Jitterbug")
a = d.generate(40)
d.reseed(bytes(range(32, 64)))
b = d.generate(32)
print("first ", a.hex())
print("second", b.hex())
