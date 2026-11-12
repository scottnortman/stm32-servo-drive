"""Exercise the compiled Thumb fault reader with modeled MMIO (requires unicorn).

This checks refusal and timeout paths, not real SPI timing or electrical safety.
Pass the BIN and symbols.json from the exact diagnostic firmware archive.
"""
import argparse
import json
import struct
import sys
from pathlib import Path


def check(binary, symbols, case):
    from unicorn import (Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS,
                         UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE)
    from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    for address, size in [(0x08000000, 0x10000), (0x20000000, 0x20000),
                          (0x40000000, 0x30000), (0xE0001000, 0x1000)]:
        uc.mem_map(address, size)
    uc.mem_write(0x08000000, binary)
    put = lambda address, value: uc.mem_write(address, struct.pack('<I', value))
    put(0x40020414, 0 if case == 'enable_low' else 1)
    put(0x40020814, 1 << 6 if case == 'phase_enabled' else 0)
    put(0x40010044, 0x8000 if case == 'pwm_enabled' else 0)
    put(0x40013000, 0 if case == 'spi_disabled' else 0x40)
    put(0xE0001000, 0 if case == 'counter_disabled' else 1)
    model = {'cycles': 0, 'rx': False, 'reg': 0, 'tx': [], 'cs': []}
    response = [0x602, 0x40, 0x20]

    def instruction(machine, address, size, user):
        model['cycles'] += 1  # monotonic virtual clock; not real CPU cycles

    def read(machine, access, address, size, value, user):
        if address == 0xE0001004:
            put(address, model['cycles'])
        elif address == 0x40013008:
            status = 0 if case == 'tx_timeout' else 2
            if model['rx'] and case != 'rx_timeout':
                status |= 1
            if case == 'busy_initially' or (case == 'busy_timeout' and model['tx']):
                status |= 0x80
            put(address, status)
        elif address == 0x4001300C:
            put(address, response[model['reg']])
            model['rx'] = False

    def write(machine, access, address, size, value, user):
        if address == 0x4001300C:
            assert value & 0x8000, 'Fault reader must never write a DRV register'
            model['reg'] = (value >> 11) & 15
            assert model['reg'] < 3
            model['tx'].append(value)
            model['rx'] = True
        elif address == 0x40020818:
            assert value in (1 << 8, 1 << 24), 'Only nCS may change'
            model['cs'].append(value)
        elif address == 0x40020418 or address == 0x40010044:
            raise AssertionError('SPI reader must never enable or alter drive outputs')

    uc.hook_add(UC_HOOK_CODE, instruction)
    uc.hook_add(UC_HOOK_MEM_READ, read)
    uc.hook_add(UC_HOOK_MEM_WRITE, write)
    uc.reg_write(UC_ARM_REG_SP, 0x20010000)
    uc.reg_write(UC_ARM_REG_LR, 0x0800F001)
    uc.reg_write(UC_ARM_REG_R0, 0x20001000)
    uc.reg_write(UC_ARM_REG_R1, 25000)
    uc.emu_start(symbols['drv8353_fault_snapshot'] | 1, 0x0800F000, count=100000)
    returned = uc.reg_read(UC_ARM_REG_R0)
    assert model['cycles'] < 26000, (case, model)
    if case == 'success':
        assert returned == 1
        assert list(struct.unpack('<4H', uc.mem_read(0x20001000, 8))) == [response[0]]+response
        assert model['tx'] == [0x8000, 0x8000, 0x8800, 0x9000]
    else:
        assert returned == 0, case
        if case.endswith('timeout'):
            assert model['cs'][-1] == 1 << 8, 'Timeout must deassert nCS'
        else:
            assert not model['tx'] and not model['cs'], 'Unsafe entry must refuse before SPI'
    print('PASS', case, 'virtual instructions', model['cycles'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('symbols', type=Path)
    parser.add_argument('--deps', type=Path, help='Optional local unicorn installation')
    args = parser.parse_args()
    if args.deps:
        sys.path.insert(0, str(args.deps.resolve()))
    for scenario in ['success', 'phase_enabled', 'pwm_enabled', 'enable_low',
                     'spi_disabled', 'counter_disabled', 'busy_initially',
                     'tx_timeout', 'rx_timeout', 'busy_timeout']:
        check(args.binary.read_bytes(), json.loads(args.symbols.read_text()), scenario)
