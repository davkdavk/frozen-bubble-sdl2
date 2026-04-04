#!/usr/bin/env python3
"""Convert a PowerPC ELF to a Wii DOL file using program headers."""

import struct
import sys

def elf_to_dol(elf_path, dol_path):
    with open(elf_path, 'rb') as f:
        data = f.read()

    if data[:4] != b'\x7fELF':
        raise ValueError("Not a valid ELF file")

    endian = '>'

    e_entry = struct.unpack('>I', data[0x18:0x1C])[0]
    e_phoff = struct.unpack('>I', data[0x1C:0x20])[0]
    e_phentsize = struct.unpack('>H', data[0x2A:0x2C])[0]
    e_phnum = struct.unpack('>H', data[0x2C:0x2E])[0]

    print(f"ELF Entry: 0x{e_entry:08X}")
    print(f"Program headers: {e_phnum} at offset 0x{e_phoff:X}")

    PT_LOAD = 1
    PF_X = 0x1

    text_sections = []
    data_sections = []
    bss_addr = 0
    bss_size = 0

    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type = struct.unpack('>I', data[off:off+4])[0]
        p_offset = struct.unpack('>I', data[off+4:off+8])[0]
        p_vaddr = struct.unpack('>I', data[off+8:off+12])[0]
        p_filesz = struct.unpack('>I', data[off+16:off+20])[0]
        p_memsz = struct.unpack('>I', data[off+20:off+24])[0]
        p_flags = struct.unpack('>I', data[off+24:off+28])[0]

        if p_type != PT_LOAD:
            continue

        print(f"LOAD: vaddr=0x{p_vaddr:08X} off=0x{p_offset:08X} filesz=0x{p_filesz:X} memsz=0x{p_memsz:X} flags={'R' if p_flags&4 else ''}{'W' if p_flags&2 else ''}{'X' if p_flags&1 else ''}")

        if p_filesz == 0:
            if bss_addr == 0:
                bss_addr = p_vaddr
                bss_size = p_memsz
            else:
                end = max(bss_addr + bss_size, p_vaddr + p_memsz)
                bss_size = end - bss_addr
        elif p_flags & PF_X:
            text_sections.append((p_vaddr, p_offset, p_filesz))
        else:
            data_sections.append((p_vaddr, p_offset, p_filesz))
            if p_memsz > p_filesz:
                bss_end = p_vaddr + p_memsz
                if bss_addr == 0:
                    bss_addr = p_vaddr + p_filesz
                    bss_size = bss_end - bss_addr
                else:
                    bss_size = max(bss_size, bss_end - bss_addr)

    text_sections.sort(key=lambda x: x[0])
    data_sections.sort(key=lambda x: x[0])

    if len(text_sections) > 7:
        print(f"WARNING: {len(text_sections)} text sections, merging")
        while len(text_sections) > 7:
            last = text_sections.pop()
            text_sections[-1] = (text_sections[-1][0], text_sections[-1][1],
                                text_sections[-1][2] + last[2])

    if len(data_sections) > 11:
        print(f"WARNING: {len(data_sections)} data sections, merging")
        while len(data_sections) > 11:
            last = data_sections.pop()
            data_sections[-1] = (data_sections[-1][0], data_sections[-1][1],
                                data_sections[-1][2] + last[2])

    # Build DOL header (256 bytes)
    dol_header = bytearray(0x100)

    # All sections in order: Text0-6, Data0-10 (18 total)
    all_sections = text_sections + [(0, 0, 0)] * (7 - len(text_sections)) + \
                   data_sections + [(0, 0, 0)] * (11 - len(data_sections))

    # Text file offsets: 0x00-0x1B (7 entries)
    for i in range(7):
        struct.pack_into('>I', dol_header, 0x00 + i*4, all_sections[i][1])

    # Data file offsets: 0x1C-0x47 (11 entries)
    for i in range(11):
        struct.pack_into('>I', dol_header, 0x1C + i*4, all_sections[7 + i][1])

    # Addresses: 0x48-0x8F (18 entries: Text0-6, Data0-10)
    for i in range(18):
        struct.pack_into('>I', dol_header, 0x48 + i*4, all_sections[i][0])

    # Sizes: 0x90-0xD7 (18 entries: Text0-6, Data0-10)
    for i in range(18):
        struct.pack_into('>I', dol_header, 0x90 + i*4, all_sections[i][2])

    # BSS
    struct.pack_into('>I', dol_header, 0xD8, bss_addr)
    struct.pack_into('>I', dol_header, 0xDC, bss_size)

    # Entry point
    struct.pack_into('>I', dol_header, 0xE0, e_entry)

    # Print summary
    for i, (addr, offset, size) in enumerate(text_sections):
        print(f"TEXT{i}: addr=0x{addr:08X} off=0x{offset:08X} size=0x{size:08X}")
    for i, (addr, offset, size) in enumerate(data_sections):
        print(f"DATA{i}: addr=0x{addr:08X} off=0x{offset:08X} size=0x{size:08X}")
    print(f"BSS:    addr=0x{bss_addr:08X} size=0x{bss_size:08X}")
    print(f"Entry:  0x{e_entry:08X}")

    # Calculate file size
    max_offset = 0x100
    for sec in text_sections + data_sections:
        end = sec[1] + sec[2]
        if end > max_offset:
            max_offset = end

    dol_data = bytearray(max_offset)
    dol_data[:0x100] = dol_header

    for addr, offset, size in text_sections + data_sections:
        dol_data[offset:offset+size] = data[offset:offset+size]

    with open(dol_path, 'wb') as f:
        f.write(dol_data)

    print(f"\nDOL written: {dol_path} ({len(dol_data)} bytes)")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.elf> <output.dol>")
        sys.exit(1)

    elf_to_dol(sys.argv[1], sys.argv[2])
