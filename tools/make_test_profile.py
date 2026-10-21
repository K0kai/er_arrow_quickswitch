"""Clone an ME3 profile and replace its one wheel entry with a test DLL."""
import argparse
import json
from pathlib import Path
import re
import tomllib

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--dll", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
source, dll, output = (p.resolve() for p in (args.source, args.dll, args.output))
if output in (source, dll):
    raise SystemExit("The test profile must have its own output path")
if not source.is_file() or not dll.is_file():
    raise SystemExit("The source profile and test DLL must exist")
text = source.read_text(encoding="utf-8-sig")
original = tomllib.loads(text)
entries = [entry for entry in original.get("natives", [])
           if entry.get("initializer", {}).get("function") == "QuickSwitchInitialize"]
if len(entries) != 1:
    raise SystemExit("Expected exactly one wheel entry in the source profile")
def absolute(value):
    p = Path(value)
    return (p if p.is_absolute() else source.parent/p).resolve()
old_dll = absolute(entries[0]["path"])
replaced = 0
def rewrite(match):
    global replaced
    value = tomllib.loads(match.group(0))["path"]
    path = absolute(value)
    if path == old_dll:
        path = dll
        replaced += 1
    if not path.exists():
        raise SystemExit(f"Profile path is missing: {path}")
    return match.group(1) + json.dumps(path.as_posix(), ensure_ascii=False)
result = re.sub(r"^(\s*path\s*=\s*)([^\r\n]+)$", rewrite, text, flags=re.MULTILINE)
if replaced != 1:
    raise SystemExit("Expected to replace exactly one wheel path")
check = tomllib.loads(result)
if len(check.get("natives", [])) != len(original.get("natives", [])):
    raise SystemExit("Native entry count changed")
wheel = [entry for entry in check["natives"]
         if entry.get("initializer", {}).get("function") == "QuickSwitchInitialize"]
assert len(wheel) == 1 and Path(wheel[0]["path"]) == dll
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(result, encoding="utf-8")
print(f"Test profile: {output}")
print(f"Wheel DLL: {dll}")
