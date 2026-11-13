#!/usr/bin/env python3
"""Tom tat e72oc_log.txt (nhieu lan chay noi tiep nhau): moi lan = 1 doan bat dau bang 'Container: da chon muc'.
Dung: python3 tools/loganalyze.py e72oc_log.txt
In ra cho moi lan chay: muc chon, ket thuc binh thuong hay DUT GIUA CHUNG (nghi treo/reboot), buoc ghi PLL cuoi cung,
cac dong ket qua (RAM, bai do giong game, STAB, AUTO, BUS, CLK), va strike/treo do app phat hien o dau lan ke tiep.
Luu y: 'log dut giua chung' = may treo/reboot THAT (hoac nguoi dung rut pin) - xem cac dong cuoi de biet dang lam gi."""
import re, sys

def main(path):
    lines = open(path, encoding='latin-1').read().replace('\r', '').split('\n')
    runs, cur = [], None
    for l in lines:
        if l.startswith('Container: da chon muc'):
            cur = {'choice': l, 'lines': []}
            runs.append(cur)
        elif cur is not None:
            cur['lines'].append(l)
    print("Tong so lan chay: %d (file %d dong)" % (len(runs), len(lines)))
    keys = ('S3: RAM goc', 'S3: BAI DO', 'S3: CHE DO', 'S3: GIU', 'S3: KET QUA', 'S3: RAM bay gio', 'S3: TU CHOI', 'S3: lan ramp',
            'S3: ramp dung', 'S3: HUY', 'S3: do goc', 'S3: lan truoc', 'STAB: KET QUA', 'AUTO:', 'BUS:', 'CLK[', 'OBS ', 'SAMPLE: KET',
            'S3: cho OS ha', 'S3: dang o M', 'P2: ROLLBACK', 'DVFS: chay ngay')
    for i, r in enumerate(runs, 1):
        L = r['lines']
        done = any(x.startswith('Container: probe xong') for x in L)
        last_pll = [x for x in L if x.startswith('S3: ghi PLL=')]
        print("\n=== Lan %d: %s" % (i, r['choice'].replace('Container: da chon muc ', '')))
        print("    ket thuc: %s" % ("binh thuong" if done else "DUT GIUA CHUNG (nghi treo/reboot)"))
        if last_pll:
            print("    ghi PLL cuoi: %s" % last_pll[-1])
        if not done and L:
            print("    dong cuoi log: %s" % [x for x in L if x.strip()][-1][:150])
        for x in L:
            if any(x.startswith(k) for k in keys):
                print("    " + x[:230])

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'e72oc_log.txt')
