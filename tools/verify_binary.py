"""Check release PE protections and version metadata without loading the DLL."""
from pathlib import Path
import ctypes
import struct
import sys

path = Path(sys.argv[1]).resolve()
blob = path.read_bytes()
assert blob[:2] == b"MZ", "Missing DOS header"
pe = struct.unpack_from("<I", blob, 0x3C)[0]
assert blob[pe:pe+4] == b"PE\0\0", "Missing PE header"
machine, sections = struct.unpack_from("<HH", blob, pe+4)
optional_size = struct.unpack_from("<H", blob, pe+20)[0]
optional = pe+24
assert machine == 0x8664 and struct.unpack_from("<H", blob, optional)[0] == 0x20B, "Expected x64 DLL"
characteristics = struct.unpack_from("<H", blob, optional+70)[0]
for flag, name in [(0x20,"High entropy ASLR"),(0x40,"ASLR"),(0x100,"DEP"),(0x4000,"Control Flow Guard")]:
    assert characteristics & flag, f"Missing {name}"
for index in range(sections):
    header = optional+optional_size+index*40
    flags = struct.unpack_from("<I", blob, header+36)[0]
    assert flags & 0xA0000000 != 0xA0000000, "Writable executable PE section"
# Check the load-config table as well as the PE header's CFG flag.
load_rva, load_size = struct.unpack_from("<II", blob, optional+112+10*8)
load_offset = None
for index in range(sections):
    header = optional+optional_size+index*40
    virtual_size, address, raw_size, raw = struct.unpack_from("<IIII", blob, header+8)
    if address <= load_rva < address+max(virtual_size, raw_size):
        load_offset = raw+load_rva-address
assert load_offset is not None and load_size >= 148, "Missing CFG load configuration"
guard_flags = struct.unpack_from("<I", blob, load_offset+144)[0]
assert guard_flags & 0x500 == 0x500, "Missing CFG instrumentation or function table"
version = ctypes.WinDLL("version", use_last_error=True)
version.GetFileVersionInfoSizeW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint)]
version.GetFileVersionInfoSizeW.restype = ctypes.c_uint
ignored = ctypes.c_uint()
size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(ignored))
assert size, "Missing Windows version resource"
print("DLL checks passed: x64, ASLR, DEP, CFG, no writable executable sections, Windows version metadata.")
