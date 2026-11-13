#!/usr/bin/env python3
"""Quet cac binary Symbian (ROM dump EKA2L1, thu muc rm-530/Sys/Bin) tim hang so thanh ghi 0xC6xxxxxx trong literal pool
va (voi --refs) tim lenh 'ldr rX,[pc,#imm]' tham chieu toi chung -> biet ham nao dung thanh ghi nao.
Dung: python3 tools/scan_regs.py <file.dll> [--refs 0xc6032604,0xc6033004]
Can: pip install capstone --break-system-packages. Xem ham: python3 tools/armdis.py <file> <dau_hex> <cuoi_hex>."""
import re, struct, sys
d = open(sys.argv[1], 'rb').read()
lits = {}
for i in range(0, len(d) - 3, 4):
    w = struct.unpack_from('<I', d, i)[0]
    if 0xC6000000 <= w < 0xC6100000:
        lits.setdefault(w, []).append(i)
print("%s: %d hang so 0xC6xxxxxx khac nhau" % (sys.argv[1], len(lits)))
for w in sorted(lits):
    print("  %08x x%d tai %s" % (w, len(lits[w]), [hex(x) for x in lits[w][:4]]))
if '--refs' in sys.argv:
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    want = set(int(x, 16) for x in sys.argv[sys.argv.index('--refs') + 1].split(','))
    for i in range(0, len(d) - 3, 4):
        ins = list(md.disasm(d[i:i + 4], i))
        if ins and ins[0].mnemonic.startswith('ldr') and 'pc' in ins[0].op_str:
            m = re.search(r'\[pc, #(-?)(0x[0-9a-f]+|\d+)\]', ins[0].op_str)
            if m:
                t = i + 8 + int(m.group(2), 0) * (-1 if m.group(1) else 1)
                if 0 <= t < len(d) - 3 and struct.unpack_from('<I', d, t)[0] in want:
                    print("  ref 0x%08x tai offset %s" % (struct.unpack_from('<I', d, t)[0], hex(i)))
