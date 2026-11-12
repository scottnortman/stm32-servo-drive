"""Regression-test the compiled nFAULT IRQ's decision to call stop_drive.

Requires unicorn and a symbols.json produced by arm-none-eabi-nm WITHOUT -g,
so the local stop_drive and boot_fault_test symbols are included. Stops at
the real stop_drive entry for active faults; never executes a motor action.
"""
import argparse
import json
import struct
import sys
from pathlib import Path


def check(binary,symbols,case):
    from unicorn import (Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,
                         UC_HOOK_CODE,UC_HOOK_MEM_WRITE)
    from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_SP,UC_ARM_REG_LR
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    for address,size in [(0x08000000,0x10000),(0x20000000,0x20000),(0x40000000,0x30000)]:uc.mem_map(address,size)
    uc.mem_write(0x08000000,binary)
    put=lambda address,value:uc.mem_write(address,struct.pack('<I',value))
    get=lambda name:struct.unpack('<I',uc.mem_read(symbols[name],4))[0]
    high=case in ['inactive_edge','boot_test']
    put(0x40020410,2 if high else 0)
    put(0x40013C14,0 if case=='no_pending' else 2)
    if 'boot_fault_test' in symbols:uc.mem_write(symbols['boot_fault_test'],bytes([case=='boot_test']))
    called=[];ack=[]

    def code(machine,address,size,user):
        if address==symbols['stop_drive']:
            called.append(uc.reg_read(UC_ARM_REG_R0));uc.emu_stop()

    def write(machine,access,address,size,value,user):
        if address==0x40013C14:ack.append(value)
        if address in [0x40020418,0x40020818,0x40010044]:
            raise AssertionError('IRQ must only call the established stop, not manipulate drive directly')

    uc.hook_add(UC_HOOK_CODE,code);uc.hook_add(UC_HOOK_MEM_WRITE,write)
    uc.reg_write(UC_ARM_REG_SP,0x20010000);uc.reg_write(UC_ARM_REG_LR,0x0800F001)
    uc.emu_start(symbols['EXTI1_IRQHandler']|1,0x0800F000,count=10000)
    expected=[] if case in ['inactive_edge','no_pending'] else [2]
    assert called==expected,(case,'stop_drive calls',called,'expected',expected)
    if case=='inactive_edge':
        assert ack==[2] and get('g_repo_fault_irq_seen')==1
        assert get('g_repo_fault_irq_rejected')==1
    elif case=='no_pending':assert not ack and get('g_repo_fault_irq_seen')==0
    print('PASS',case,'stop_drive calls',called)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);parser.add_argument('symbols',type=Path)
    parser.add_argument('--deps',type=Path)
    args=parser.parse_args()
    if args.deps:sys.path.insert(0,str(args.deps.resolve()))
    for scenario in ['inactive_edge','active_low','boot_test','no_pending']:
        check(args.binary.read_bytes(),json.loads(args.symbols.read_text()),scenario)
