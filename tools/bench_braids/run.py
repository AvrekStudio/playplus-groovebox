"""Mesure des instructions par échantillon de chaque modèle Braids (Unicorn, Cortex-M7, Thumb-2)."""
import struct, sys
from unicorn import *
from unicorn.arm_const import *
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS

elf = open('bench.elf', 'rb').read()   # lancé depuis tools/bench_braids/build/
mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
mu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
mu.mem_map(0x60200000, 0x400000); mu.mem_map(0x20200000, 0x80000); mu.mem_map(0x10000000, 0x1000)
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
    mu.reg_write(UC_ARM_REG_SP, 0x20280000 - 0x100); mu.reg_write(UC_ARM_REG_LR, RET | 1)
    mu.emu_start(sym[name] | 1, RET, count=500_000_000)
    return mu.reg_read(UC_ARM_REG_R0)

names = """CSAW MORPH SAW_SQUARE SINE_TRIANGLE BUZZ SQUARE_SUB SAW_SUB SQUARE_SYNC SAW_SYNC TRIPLE_SAW TRIPLE_SQUARE
TRIPLE_TRIANGLE TRIPLE_SINE TRIPLE_RING_MOD SAW_SWARM SAW_COMB TOY DIGITAL_FILTER_LP DIGITAL_FILTER_PK DIGITAL_FILTER_BP
DIGITAL_FILTER_HP VOSIM VOWEL VOWEL_FOF HARMONICS FM FEEDBACK_FM CHAOTIC_FEEDBACK_FM PLUCKED BOWED BLOWN FLUTED
STRUCK_BELL STRUCK_DRUM KICK CYMBAL SNARE WAVETABLES WAVE_MAP WAVE_LINE WAVE_PARAPHONIC FILTERED_NOISE TWIN_PEAKS_NOISE
CLOCKED_NOISE GRANULAR_CLOUD PARTICLE_NOISE DIGITAL_MODULATION QUESTION_MARK""".split()
call('bench_init')
N = 2048
rows = []
for i, nm in enumerate(names):
    worst = 0; peak = 0
    for pitch, p1, p2 in ((60 << 7, 16384, 16384), (72 << 7, 32000, 32000), (36 << 7, 0, 0)):
        call('bench_shape', i, pitch, p1, p2)
        call('bench_render', 256)                       # mise en route
        c0 = count[0]; call('bench_render', N); per = (count[0] - c0) / N
        worst = max(worst, per); peak = max(peak, call('bench_peak'))
    rows.append((nm, worst, peak))
    print(f'{nm:22s} {worst:7.1f} instr/échantillon   crête {peak}', flush=True)
import json; json.dump(rows, open('results.json', 'w'))
