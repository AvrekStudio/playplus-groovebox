"""Charge de LOOM-8 (synth_render + effets) par bloc de 128 trames, Cortex-M7, dans Unicorn."""
import struct, sys
from unicorn import *
from unicorn.arm_const import *
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS

elf = open('loom.elf', 'rb').read()
mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
mu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
mu.mem_map(0x60200000, 0x800000); mu.mem_map(0x80000000, 0x400000); mu.mem_map(0x10000000, 0x1000)
# segments PT_LOAD
e_phoff, = struct.unpack_from('<I', elf, 0x1C); e_phentsize, e_phnum = struct.unpack_from('<HH', elf, 0x2A)
for i in range(e_phnum):
    p_type, p_off, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from('<6I', elf, e_phoff + i*e_phentsize)
    if p_type == 1 and p_filesz:
        mu.mem_write(p_vaddr, elf[p_off:p_off+p_filesz])
# symboles
e_shoff, = struct.unpack_from('<I', elf, 0x20); e_shentsize, e_shnum = struct.unpack_from('<HH', elf, 0x2E)
secs = [struct.unpack_from('<10I', elf, e_shoff + i*e_shentsize) for i in range(e_shnum)]
sym = {}
for s in secs:
    if s[1] == 2:  # SHT_SYMTAB
        strtab = secs[s[6]]
        for k in range(s[5] // 16):
            n, v, sz, info, oth, shn = struct.unpack_from('<IIIBBH', elf, s[4] + 16*k)
            name = elf[strtab[4]+n:].split(b'\0')[0].decode()
            sym[name] = v
# FPU on
mu.reg_write(UC_ARM_REG_C1_C0_2, mu.reg_read(UC_ARM_REG_C1_C0_2) | (0xf << 20)) if hasattr(sys,'x') else None
try: mu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
except Exception: pass

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS); cache = {}; count = [0]
def blk(uc, addr, size, _):
    n = cache.get((addr, size))
    if n is None:
        n = sum(1 for _ in md.disasm_lite(bytes(uc.mem_read(addr, size)), addr)) or 1; cache[(addr, size)] = n
    count[0] += n
mu.hook_add(UC_HOOK_BLOCK, blk)
RET = 0x10000000
mu.mem_write(RET, b'\x00\xbf\x00\xbf')
def call(name, *args):
    for r, a in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args): mu.reg_write(r, a & 0xffffffff)
    mu.reg_write(UC_ARM_REG_SP, 0x80400000 - 0x100); mu.reg_write(UC_ARM_REG_LR, RET | 1)
    mu.emu_start(sym[name] | 1, RET, count=500_000_000)
    return mu.reg_read(UC_ARM_REG_R0)


import struct as _s
BUDGET = 1741497
ENG = "knit rayon jacquard bellows cane voile sample slice kit midi acid".split()
def fret():
    return _s.unpack('<f', _s.pack('<I', mu.reg_read(UC_ARM_REG_S0)))[0]
def measure(label, cas, k=0, n=0, fx=0, warm=20, nb=200):
    call('bench_setup', cas, k, n, fx)
    for _ in range(warm): call('bench_block')
    per = []; peak = 0.0
    for _ in range(nb):
        c0 = count[0]; call('bench_block'); per.append(count[0] - c0)
        call('bench_peak'); peak = max(peak, fret())
    avg = sum(per) / len(per); mx = max(per)
    print(f"{label:34s} moy {avg/1000:7.1f} k instr/bloc = {100*avg/BUDGET:5.1f} %   pic {100*mx/BUDGET:5.1f} %   niveau {peak:.3f}", flush=True)
    return label, avg, mx, peak
rows = []
rows.append(measure("silence, sans effets", 0))
rows.append(measure("silence + delay + reverb", 0, fx=1))
npre = call('bench_npresets')
first = {}
for k in range(npre):
    e = call('bench_preset_engine', k)
    first.setdefault(e, k)
for e, k in sorted(first.items()):
    name = ENG[e] if e < len(ENG) else str(e)
    if name in ("midi", "kit"):
        continue
    for n in (1, 4, 6):
        rows.append(measure(f"{name} ({n} voix)", 1, k, n))
rows.append(measure("chanson : kit+acid+knit x4+voile x4, effets", 2, fx=1))
import json; json.dump(rows, open('results_loom.json', 'w'))
