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

# Stage 4 worst case

List all 2,644 distance-11 states, then build and run `asm/main.S`
once per state on `RV32_ISS` with `TESTS` off, 8 at a time (from `asm/`):

```sh
../search --hardest > ../measure/hardest.txt
mkdir -p /tmp/worst
xargs -P 8 -I STATE sh -c 'sed -e "s/^input: .*/input: .string \"STATE\"/" -e "s/^.equ TESTS, 1/.equ TESTS, 0/" main.S > /tmp/worst/STATE.S && riscv64-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -Wa,-I. /tmp/worst/STATE.S -o /tmp/worst/STATE.elf && echo STATE $(ripes --mode cli --proc RV32_ISS --src /tmp/worst/STATE.elf -t elf --iret 2>/dev/null | tr -d "\000" | tr "\n" " ")' < ../measure/hardest.txt > ../measure/results-worst-iss.txt
```

- `results-worst-iss.txt`: one line per state with its length, moves,
  exit code and instructions retired (last number).

```sh
grep -c "exited with code: 0" ../measure/results-worst-iss.txt
awk '$2 != 11' ../measure/results-worst-iss.txt | wc -l
awk '{print $NF, $1}' ../measure/results-worst-iss.txt | sort -n | tail -1
```
