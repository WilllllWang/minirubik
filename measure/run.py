#!/usr/bin/env python3
"""Stage 1 harness.
  run.py mem  N_WORDS [PROC]   -> peak host RSS (KiB), retired instructions, model exec time (ms)
  run.py rate N_WORDS PROC     -> same, for rate measurements
Each invocation is one fresh Ripes process so ru_maxrss is per run."""
import os, re, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
def run(n_words, proc):
    src = open(os.path.join(HERE, "mem.s.in")).read().replace("@N@", str(n_words))
    with tempfile.NamedTemporaryFile("w", suffix=".s", delete=False) as f:
        f.write(src); path = f.name
    cmd = [sys.executable, os.path.join(HERE, "peakrss.py"), "ripes", "--mode", "cli", "--proc", proc,
           "--src", path, "-t", "asm", "--iret", "--cycles", "--exectime"]
    t0 = time.time(); p = subprocess.run(cmd, capture_output=True, text=True); wall = time.time() - t0
    os.unlink(path)
    out = p.stdout + p.stderr
    def grab(pat):
        m = re.search(pat, out); return int(m.group(1)) if m else None
    return dict(proc=proc, n_words=n_words, guest_bytes=4*n_words,
                peak_rss_kb=grab(r"PEAK_RSS_KB=(\d+)"), iret=grab(r"[Ii]nstructions retired:?\s*(\d+)"),
                cycles=grab(r"[Cc]ycles:?\s*(\d+)"), exectime_ms=grab(r"[Ee]xec(?:ution)? time[^\d]*(\d+)"),
                wall_s=round(wall, 2), rc=p.returncode, raw=out if p.returncode or None in (grab(r"PEAK_RSS_KB=(\d+)"),) else None)
if __name__ == "__main__":
    mode, n = sys.argv[1], int(sys.argv[2]); proc = sys.argv[3] if len(sys.argv) > 3 else "RV32_ISS"
    r = run(n, proc)
    print({k: v for k, v in r.items() if k != "raw"})
    if r["raw"]: print("--- raw ---"); print(r["raw"])
