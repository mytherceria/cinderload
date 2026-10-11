"""dll/switches.lua as a C string, dll/switches_lua.h (kSwitchesLua), for CinderLoad.dll to run inside the game; the
tests check the two agree.  python3 -I tools/lua_to_c.py"""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, '..', 'dll', 'switches.lua'), encoding='ascii').read()
out = ['/* dll/switches.lua, made into a C string by tools/lua_to_c.py: edit the .lua, then run it. */',
       'static const char kSwitchesLua[] =']
for line in src.split('\n')[:-1] if src.endswith('\n') else src.split('\n'):
    out.append('    "' + line.replace('\\', '\\\\').replace('"', '\\"') + '\\n"')
out[-1] += ';'
open(os.path.join(HERE, '..', 'dll', 'switches_lua.h'), 'w', newline='\n').write('\n'.join(out) + '\n')
