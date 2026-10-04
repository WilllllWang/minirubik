#!/usr/bin/env python3
"""Run a command, print its stdout, then print PEAK_RSS_KB=<max resident set of the child> on stderr."""
import resource, subprocess, sys
p = subprocess.run(sys.argv[1:], capture_output=True, text=True)
sys.stdout.write(p.stdout)
sys.stderr.write(p.stderr)
print(f"PEAK_RSS_KB={resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss}", file=sys.stderr)
sys.exit(p.returncode)
