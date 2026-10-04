# Stage 1 measurements

`mem.s.in` writes N 32-bit words from the start of `.data` in a
4-instruction loop. N = 0 is the control: the same program, writing
nothing. Ripes build: `v2.2.6-106-g5b8a616`.

Make the program for one N:

```sh
sed 's/@N@/262144/' mem.s.in > /tmp/mem.s
```

Run it under GNU `time` and append the output to a results file:

```sh
/usr/bin/time -v ripes --mode cli --proc RV32_ISS --src /tmp/mem.s -t asm --iret --cycles --exectime 2>&1 | tee -a results-mem-iss.txt
```

- `results-mem-iss.txt`: N = 0, 262144, 1048576, 4194304 on `RV32_ISS`,
  3 runs each. Read "Maximum resident set size (kbytes)".
- `results-rate.txt`: N = 262144 on `RV32_ISS`, `RV32_SS`, `RV32_5S`,
  3 runs each. Rate = instructions retired / execution time (ms).

Each block in a results file is one run; instructions retired is
4N + 6, which identifies N.
