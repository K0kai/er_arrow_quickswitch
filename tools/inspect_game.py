"""Read-only PE/signature inspection. Writes nothing to the game directory."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).parent / 'python'))

class PE:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        count = struct.unpack_from('<H', d, pe + 6)[0]
        size = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<Q', d, pe + 24 + 24)[0]
        self.sections = []
        for i in range(count):
            off = pe + 24 + size + i * 40
            name = d[off:off+8].rstrip(b'\0').decode()
            vs, va, rs, raw = struct.unpack_from('<IIII', d, off+8)
            flags = struct.unpack_from('<I', d, off+36)[0]
            self.sections.append((name, va, raw, rs, flags))

    def bytes(self, rva, size):
        for _, va, raw, rs, _ in self.sections:
            if va <= rva < va+rs:
                return self.data[raw+rva-va:raw+rva-va+size]
        raise ValueError(hex(rva))

    def scan(self, pattern):
        pat = b''.join(b'.' if t == '??' else re.escape(bytes.fromhex(t)) for t in pattern.split())
        hits = []
        for _, va, raw, rs, flags in self.sections:
            if flags & 0x20000000:
                hits += [va + m.start() for m in re.finditer(pat, self.data[raw:raw+rs], re.DOTALL)]
        return hits

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('exe')
    p.add_argument('--rva', type=lambda s: int(s, 0))
    p.add_argument('--size', type=lambda s: int(s, 0), default=160)
    a = p.parse_args()
    pe = PE(a.exe)
    if a.rva is not None:
        import capstone
        cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        for i in cs.disasm(pe.bytes(a.rva, a.size), a.rva):
            print(f'{i.address:08X} {i.mnemonic:8s} {i.op_str}')
    else:
        patterns = {
            'GameDataMan': '48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3',
            'EquipGearBody': '?? 8B F1 ?? 8B D8 ?? 63 EA ?? 8B F9',
        }
        print(json.dumps({'sha256': hashlib.sha256(pe.data).hexdigest(),
                          'matches': {k: [hex(x) for x in pe.scan(v)] for k,v in patterns.items()}}, indent=2))
