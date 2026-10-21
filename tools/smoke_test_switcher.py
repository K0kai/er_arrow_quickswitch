"""Load the final DLL in an isolated non-game process; it must not install hooks."""
import ctypes
from pathlib import Path
import shutil
import time
import sys

root=Path(__file__).resolve().parents[1]
folder=root/'build'/'smoke-switcher'
folder.mkdir(parents=True,exist_ok=True)
log=folder/'ArrowQuickSwitch-0.4.log'
if log.exists(): log.unlink()
source=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'dist'/'ArrowQuickSwitch.dll'
shutil.copy2(source,folder/'ArrowQuickSwitch.dll')
dll=ctypes.CDLL(str(folder/'ArrowQuickSwitch.dll'))
initialize=dll.QuickSwitchInitialize
initialize.argtypes=[]; initialize.restype=ctypes.c_bool
assert initialize() is True
assert initialize() is True
for _ in range(100):
    if log.exists() and 'unsupported executable version' in log.read_text(): break
    time.sleep(.05)
else: raise AssertionError('Expected unsupported-host shutdown was not logged')
text=log.read_text()
assert text.count('ArrowQuickSwitch 0.6.2:')==1,text
assert 'Ready:' not in text and 'hook is executing' not in text,text
print('Final DLL loads, initializes once, logs, and refuses an unsupported host without installing hooks.')
