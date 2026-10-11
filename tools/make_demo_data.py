#!/usr/bin/env python3
"""make_demo_data.py - game/demo_data.h from the recorded demos (game/demorec.c).

    python3 tools/make_demo_data.py <out demo_data.h> d1.txt d1.chk [d2.txt d2.chk ...]

dN.txt: "stage warp_x warp_y seed steps", then one pad state per 60 Hz step (demorec HB_DEMO_OUT).
dN.chk: the player's place after every 60 steps of the replay (demorec HB_PLAY's output).
The pads are run-length coded (pad, count); the places let p1.c end a demo that ever went off
its path (it should not: the replay is the same on the PC and the 32X)."""
import sys


def main():
    out, args = sys.argv[1], sys.argv[2:]
    if len(args) % 2:
        sys.exit(__doc__)
    lines = ["/* game/demo_data.h - the recorded demos (tools/make_demo_data.py from",
             " * game/demorec.c's recordings; the attract mode plays one at random) */"]
    recs = []
    for i in range(0, len(args), 2):
        k = i // 2
        txt = open(args[i]).read().split()
        st, wx, wy, seed, n = (int(v) for v in txt[:5])
        pads = [int(v) for v in txt[5:5 + n]]
        rle = []
        for p in pads:
            if rle and rle[-2] == p and rle[-1] < 65535:
                rle[-1] += 1
            else:
                rle += [p, 1]
        chk = [int(v) for v in open(args[i + 1]).read().split()]
        lines.append("static const uint16_t demo_rle%d[%d] = {" % (k, len(rle)))
        for j in range(0, len(rle), 16):
            lines.append("\t" + ", ".join(str(v) for v in rle[j:j + 16]) + ",")
        lines.append("};")
        lines.append("static const int16_t demo_chk%d[%d] = {" % (k, len(chk)))
        for j in range(0, len(chk), 16):
            lines.append("\t" + ", ".join(str(v) for v in chk[j:j + 16]) + ",")
        lines.append("};")
        recs.append("\t{ %d, %d, %d, %du, %d, demo_rle%d, %d, demo_chk%d, %d }," % (st, wx, wy, seed, n, k, len(rle), k, len(chk) // 2))
        print("demo %d (stage %d): %d steps (%.1f s), %d runs, %d places" % (k + 1, st, n, n / 60, len(rle) // 2, len(chk) // 2))
    lines.append("#define DEMO_RECS %d" % len(recs))
    lines.append("static const DemoRec demo_recs[DEMO_RECS] = {")
    lines += recs
    lines.append("};")
    open(out, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
