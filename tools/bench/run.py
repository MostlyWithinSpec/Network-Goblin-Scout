#!/usr/bin/env python3
"""Count instructions per UI frame on the ESP32-C5's CPU type, in an emulator.

  sh tools/bench/build.sh && python3 tools/bench/run.py tools/bench/bench.elf

Counts instructions only: real time also depends on PSRAM latency and caches, so treat the
numbers as relative (before/after a change), not as milliseconds.
"""
import sys
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32, UC_HOOK_BLOCK, UC_HOOK_CODE
from unicorn.riscv_const import UC_RISCV_REG_SP, UC_RISCV_REG_RA, UC_RISCV_REG_PC

elf = ELFFile(open(sys.argv[1], "rb"))
mu = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
for seg in elf.iter_segments():
    if seg["p_type"] != "PT_LOAD":
        continue
    base = seg["p_vaddr"] & ~0xFFF
    size = ((seg["p_vaddr"] + seg["p_memsz"] + 0xFFF) & ~0xFFF) - base
    try:
        mu.mem_map(base, size)
    except Exception:
        pass
    mu.mem_write(seg["p_vaddr"], seg.data())
syms = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
heap_top = (max(seg["p_vaddr"] + seg["p_memsz"] for seg in elf.iter_segments() if seg["p_type"] == "PT_LOAD") + 0xFFFF) & ~0xFFFF
mu.mem_map(heap_top, 16 << 20)              # heap + stack
mu.reg_write(UC_RISCV_REG_SP, heap_top + (16 << 20) - 16)

counts = {}
total = [0]
blocks = {}

import bisect
funcs = sorted((s["st_value"] & ~1, s.name) for s in elf.get_section_by_name(".symtab").iter_symbols()
               if s["st_info"]["type"] == "STT_FUNC" and s["st_value"])
faddr = [f[0] for f in funcs]
fcount = {}
fof = {}

def on_block(uc, addr, size, _):
    n = blocks.get(addr)
    if n is None:
        code = uc.mem_read(addr, size)
        n, i = 0, 0
        while i < size:
            i += 4 if (code[i] & 3) == 3 else 2
            n += 1
        blocks[addr] = n
    total[0] += n
    if len(marks) == 1:  # profile the first section (home idle)
        f = fof.get(addr)
        if f is None:
            i = bisect.bisect_right(faddr, addr) - 1
            f = fof[addr] = funcs[i][1] if i >= 0 else "?"
        fcount[f] = fcount.get(f, 0) + n

marks = []
def on_mark(uc, addr, size, _):
    marks.append((uc.reg_read(10), total[0]))  # a0 = mark id

def on_done(uc, addr, size, _):
    uc.emu_stop()

callers = {}
def on_float(uc, addr, size, _):
    if len(marks) != 1:
        return
    ra = uc.reg_read(UC_RISCV_REG_RA)
    i = bisect.bisect_right(faddr, ra) - 1
    f = funcs[i][1] if i >= 0 else "?"
    callers[f] = callers.get(f, 0) + 1
for fn in ("__mulsf3", "__addsf3", "__subsf3", "__divsf3", "__gtsf2", "__ltsf2", "__fixsfsi", "__floatsisf"):
    if fn in syms:
        mu.hook_add(UC_HOOK_CODE, on_float, begin=syms[fn], end=syms[fn])
mu.hook_add(UC_HOOK_BLOCK, on_block)
mu.hook_add(UC_HOOK_CODE, on_mark, begin=syms["bench_mark"], end=syms["bench_mark"])
mu.hook_add(UC_HOOK_CODE, on_done, begin=syms["bench_done"], end=syms["bench_done"])
mu.emu_start(syms["_start"], 0)
names = ["home idle", "home scanning", "level-up overlay", "radar"]  # in bench.cpp order
for i, ((_, ta), (_, tb)) in enumerate(zip(marks, marks[1:])):
    per = (tb - ta) / 3
    print(f"{names[i]:18s} {per / 1e6:7.2f} M instructions/frame  (~{per / 240e3:5.1f} ms at 1 instr/cycle, 240 MHz)")

print("\nhome idle, top functions (instructions/frame):")
import subprocess
for name, n in sorted(fcount.items(), key=lambda kv: -kv[1])[:14]:
    try:
        nice = subprocess.run(["c++filt", name], capture_output=True, text=True).stdout.strip()
    except Exception:
        nice = name
    print(f"  {n / 3 / 1e3:8.0f} k  {nice[:90]}")

print("\nhome idle, soft-float calls per frame by caller:")
for name, n in sorted(callers.items(), key=lambda kv: -kv[1])[:10]:
    nice = subprocess.run(["c++filt", name], capture_output=True, text=True).stdout.strip()
    print(f"  {n / 3:8.0f}  {nice[:90]}")
