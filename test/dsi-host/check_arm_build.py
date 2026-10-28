"""Check the actual linked ARM kernels and instruction-memory budget."""
from pathlib import Path
import json, re, subprocess, sys

elf = Path(sys.argv[1])
bin_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('C:/devkitPro/devkitARM/bin')
def run(tool, *args):
    return subprocess.check_output([str(bin_dir / (tool + '.exe')), *args, str(elf)], text=True)

headers = run('arm-none-eabi-objdump', '-h')
match = re.search(r'\s\.itcm\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', headers)
assert match, 'Missing ITCM output section'
size, address, load = (int(x, 16) for x in match.groups())
assert 0x1ff8020 <= address < address + size <= 0x2000000, 'ITCM overflow/vector overlap'
assert 0x2000000 <= load < 0x3000000, 'ITCM must have a main-RAM load image'
symbols = run('arm-none-eabi-nm', '-S')
required = ['DsiBlitMapped', 'DsiBlitBlended', 'DsiBlitBlendedMapped',
            'DsiFillBlended', 'dsi_present_index8', 'dsi_present_reading_index8', 'DsiExpandScene2x']
result = {}
for name in required:
    m = re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+T\s+'+name+r'$', symbols, re.M)
    assert m, 'Missing kernel '+name
    start, length = (int(x, 16) for x in m.groups())
    assert address <= start < start+length <= address+size, 'Kernel outside ITCM: '+name
    code = run('arm-none-eabi-objdump', '-d', '--disassemble='+name)
    assert re.search(r'^\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+', code, re.M), 'Expected ARM instructions: '+name
    if name.startswith('dsi_present_'):
        assert re.search(r'\bstr\s', code) and re.search(r'\bstrh\s', code), 'Need both word and halfword VRAM paths'
    result[name] = {'address':hex(start), 'bytes':length}
print('PASS: seven ARM kernels in loaded ITCM; word/halfword stores retained; vector/32 KiB budget respected')
print(json.dumps({'itcm_bytes':size, 'itcm_available_bytes':0x7fe0,
                  'load_address':hex(load), 'kernels':result}, indent=2))
