"""Audit compiled startup and Hall observer MMIO against the pre-Hall image.

Requires unicorn. Stops startup at clock_init, before any peripheral activity
other than the initial safe GPIO setup. Does not model electrical coupling.
"""
import argparse
import json
import struct
import sys
from pathlib import Path


def machine(binary):
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
    from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for a,size in [(0x08000000,0x10000),(0x20000000,0x20000),
                   (0x40000000,0x30000),(0xE000E000,0x1000)]:uc.mem_map(a,size)
    uc.mem_write(0x08000000,binary)
    uc.reg_write(UC_ARM_REG_SP,0x20010000);uc.reg_write(UC_ARM_REG_LR,0x0800F001)
    return uc


def startup(binary,symbols,seed):
    from unicorn import UC_HOOK_CODE
    uc=machine(binary)
    for a in [0x40020400,0x4002040c]:uc.mem_write(a,struct.pack('<I',seed))
    reached=[]
    def code(machine,address,size,user):
        if address==symbols['clock_init']:reached.append(True);machine.emu_stop()
    uc.hook_add(UC_HOOK_CODE,code)
    uc.emu_start(symbols['main']|1,0x0800F000,count=1000)
    assert reached,'Startup did not reach clock_init'
    return {a:struct.unpack('<I',uc.mem_read(a,4))[0] for a in
            [0x40020000,0x4002000c,0x40020400,0x40020404,0x40020408,
             0x4002040c,0x40020414,0x40020800,0x4002080c,0x40020814,
             0x40013808,0x40013c00,0x40013c08,0x40013c0c]}


def audit(before,before_symbols,after,after_symbols):
    mask=0x3f000
    for seed in [0,0xffffffff,0xaaaaaaaa,0x55555555,0x5a5aa5a5]:
        old=startup(before,before_symbols,seed);new=startup(after,after_symbols,seed)
        for a in old:
            if a in [0x40020400,0x4002040c]:assert (old[a]^new[a])&~mask==0,(hex(a),old,new)
            else:assert old[a]==new[a],(hex(a),old,new)
        assert new[0x40020400]&mask==0 and new[0x4002040c]&mask==0x15000
        assert old[0x40020400]&12==new[0x40020400]&12==0,'PB1 must remain input'
        assert old[0x4002040c]&12==new[0x4002040c]&12,'PB1 pull setting changed'
    print('PASS compiled startup: only PB6/7/8 modes/pulls differ; PB1 and EXTI untouched')

    from unicorn import UC_HOOK_MEM_WRITE
    from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR
    uc=machine(after)
    def peripheral_write(machine,access,address,size,value,user):
        if address>=0x40000000:raise AssertionError('Hall observer wrote peripheral '+hex(address))
    uc.hook_add(UC_HOOK_MEM_WRITE,peripheral_write)
    for state in range(8):
        for n in range(3):
            pins=((state>>2)&1)<<6|((state>>1)&1)<<7|(state&1)<<8|(n&1)<<1
            uc.mem_write(0x40020410,struct.pack('<I',pins))
            uc.reg_write(UC_ARM_REG_SP,0x20010000);uc.reg_write(UC_ARM_REG_LR,0x0800F001)
            uc.emu_start(after_symbols['repo_tune_hall_tick']|1,0x0800F000,count=1000)
        raw=struct.unpack('<I',uc.mem_read(after_symbols['g_tune_hall_raw'],4))[0]
        stable=struct.unpack('<I',uc.mem_read(after_symbols['g_tune_hall_state'],4))[0]
        assert raw==stable==state
    print('PASS compiled Hall observer: all8 codes, PB1 toggles do not alter Hall code, no peripheral writes')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['before_bin','before_symbols','after_bin','after_symbols']:p.add_argument(name,type=Path)
    p.add_argument('--deps',type=Path);a=p.parse_args()
    if a.deps:sys.path.insert(0,str(a.deps.resolve()))
    audit(a.before_bin.read_bytes(),json.loads(a.before_symbols.read_text()),
          a.after_bin.read_bytes(),json.loads(a.after_symbols.read_text()))
