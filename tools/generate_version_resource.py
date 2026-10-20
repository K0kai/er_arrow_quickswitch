"""Generate Windows version metadata from the build script's release version."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("--filename", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
versions = set(re.findall(r"ArrowQuickSwitch-(\d+\.\d+\.\d+)\.dll", (root/"build-switcher.cmd").read_text()))
if len(versions) != 1:
    raise SystemExit("Expected one release version")
version = versions.pop()
if not re.fullmatch(r"[A-Za-z0-9_.-]+\.dll", args.filename):
    raise SystemExit("Invalid DLL filename")
numeric = version.replace(".", ",") + ",0"
Path("version.rc").write_text(f'''#include <windows.h>
1 VERSIONINFO
FILEVERSION {numeric}
PRODUCTVERSION {numeric}
FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
FILEFLAGS 0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_DLL
FILESUBTYPE 0
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "FileDescription", "Arrow Wheel - Elden Ring ammo wheel mod\\0"
            VALUE "FileVersion", "{version}.0\\0"
            VALUE "InternalName", "ArrowQuickSwitch\\0"
            VALUE "OriginalFilename", "{args.filename}\\0"
            VALUE "ProductName", "Arrow Wheel\\0"
            VALUE "ProductVersion", "{version}.0\\0"
            VALUE "Comments", "Source: https://github.com/K0kai/er_arrow_quickswitch\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
''', encoding="utf-8")
