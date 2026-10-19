#!/usr/bin/env python3
"""Decode a bootstrap command string (commanddata_lgx2.cpp) into numbered, labelled I2C
transactions, one per line: index, W(rite)/R(ead)/X (other bridge command), device or ADV7604
map, register, data. See PROTOCOL.md, "Bootstrap command stream".

Usage: decode_bootstrap.py src/liblgx/liblgx-usb/src/bootstrap/commanddata_lgx2.cpp
"""
import re
import sys

src = open(sys.argv[1]).read()
body = re.search(r'\{"(.*)"\s*\}', src, re.S).group(1).replace('\\012', ' ').replace('\\"', '"')
tokens = body.split()

MAPS = {
    0x20: "ADV.IO", 0x21: "ADV.AVLINK", 0x40: "ADV.CEC", 0x3E: "ADV.INFOFRAME", 0x38: "ADV.ESDP",
    0x3C: "ADV.DPP", 0x26: "ADV.AFE", 0x32: "ADV.REP", 0x36: "ADV.EDID", 0x34: "ADV.HDMI",
    0x29: "ADV.TEST", 0x22: "ADV.CP", 0x24: "ADV.VDP", 0x42: "ADV.AVLINK",
    0x74: "PCA9539A", 0x10: "FPGA?", 0x65: "dev65", 0x2C: "FE.master", 0x58: "FE.RX",
    0x4B: "FE.router", 0x44: "FE.TX0", 0x45: "FE.TX1", 0x46: "FE.TX2", 0x47: "FE.TX3", 0x6C: "FE.EDID",
}

out = []
i = 0
n = 0
while i < len(tokens):
    t = tokens[i]
    if t.startswith('>'):
        b = bytes.fromhex(t[1:])
        reply = None
        if i + 1 < len(tokens) and tokens[i + 1].startswith('<'):
            reply = tokens[i + 1][1:]
            i += 1
        op = b[0]
        if op == 0x01:
            addr, ll = b[1], b[2]
            reg = int.from_bytes(b[3:3 + ll], 'big')
            data = b[3 + ll:]
            out.append((n, 'W', addr, reg, data.hex(' ').upper()))
        elif op == 0x02:
            addr, ll, cnt = b[1], b[2], b[3]
            reg = int.from_bytes(b[4:4 + ll], 'big')
            out.append((n, 'R', addr, reg, f"n={cnt}" + (f" reply={reply}" if reply and ',' in reply else "")))
        else:
            out.append((n, 'X', None, None, b.hex(' ').upper() + (f" <{reply}" if reply else "")))
        n += 1
    i += 1

for n, kind, addr, reg, rest in out:
    name = MAPS.get(addr, f"0x{addr:02X}") if addr is not None else "BRIDGE"
    regs = f"{reg:02X}" if reg is not None else "--"
    print(f"{n:5d} {kind} {name:14s} {regs}  {rest}")
